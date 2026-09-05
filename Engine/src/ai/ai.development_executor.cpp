/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module ai.development_executor;

import ai.development_guard;
import core.sha256;
import platform.filesystem;

namespace epochengine::ai::development_executor
{
    namespace fs = std::filesystem;
    namespace guard = development_guard;

    namespace
    {
        struct PreparedChange final
        {
            SourceChange change{};
            fs::path destination{};
            fs::path temporary{};
            fs::path backup{};
            std::string original_bytes{};
            std::size_t evidence_index{};
            bool original_moved{};
            bool replacement_installed{};
        };

        [[nodiscard]] bool same_path_identity(
            std::string_view left,
            std::string_view right) noexcept
        {
            if (left.size() != right.size())
                return false;
#if defined(_WIN32)
            for (std::size_t index = 0u; index < left.size(); ++index)
            {
                const auto fold = [](char value) noexcept
                {
                    return value >= 'A' && value <= 'Z'
                        ? static_cast<char>(value + ('a' - 'A'))
                        : value;
                };
                if (fold(left[index]) != fold(right[index]))
                    return false;
            }
            return true;
#else
            return left == right;
#endif
        }

        [[nodiscard]] bool owned_runtime_workspace_path(const fs::path& relative)
        {
            // core.path owns this checkout-local runtime workspace. It can
            // contain conversation/tool traces and staged research, not source
            // inputs. Apply the exclusion here to initial copies, repairs and
            // chosen-parent succession alike, even without caller exclusions.
            // An unrelated source component named "workspace" remains valid.
            constexpr std::string_view components[]{
                "Engine", "examples", "EpochEditor", "workspace"};
            auto component = relative.begin();
            for (const auto expected : components)
            {
                if (component == relative.end()
                    || !same_path_identity(component->generic_string(), expected))
                    return false;
                ++component;
            }
            return true;
        }

        [[nodiscard]] bool valid_limits(
            const TransactionLimits& limits) noexcept
        {
            constexpr std::size_t hard_maximum_changes = 4096u;
            constexpr std::uint64_t hard_maximum_file =
                64u * 1024u * 1024u;
            constexpr std::uint64_t hard_maximum_total =
                256u * 1024u * 1024u;
            return limits.maximum_changes > 0u
                && limits.maximum_changes <= hard_maximum_changes
                && limits.maximum_file_bytes > 0u
                && limits.maximum_file_bytes <= hard_maximum_file
                && limits.maximum_total_bytes > 0u
                && limits.maximum_total_bytes <= hard_maximum_total
                && limits.maximum_file_bytes <= limits.maximum_total_bytes
                && limits.maximum_path_bytes > 0u
                && limits.maximum_path_bytes <= 16u * 1024u;
        }

        [[nodiscard]] bool valid_state(
            const ContentState& state) noexcept
        {
            return state.exists
                ? state.digest.valid()
                : !state.digest.valid() && state.byte_count == 0u;
        }

        [[nodiscard]] ContentState state_for(
            std::string_view bytes) noexcept
        {
            return {
                .exists = true,
                .digest = digest(bytes),
                .byte_count = static_cast<std::uint64_t>(bytes.size())};
        }

        [[nodiscard]] bool checked_add(
            std::uint64_t& value,
            std::uint64_t addition,
            std::uint64_t maximum) noexcept
        {
            if (addition > maximum || value > maximum - addition)
                return false;
            value += addition;
            return true;
        }

        [[nodiscard]] bool missing_error(
            const std::error_code& error) noexcept
        {
            return error == std::errc::no_such_file_or_directory;
        }

        [[nodiscard]] std::optional<std::string> read_regular_file(
            const fs::path& path,
            std::uint64_t maximum_bytes,
            std::error_code& error)
        {
            error.clear();
            const std::uintmax_t size = fs::file_size(path, error);
            if (error || size > maximum_bytes
                || size > static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::size_t>::max)()))
            {
                return std::nullopt;
            }

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                error = std::make_error_code(std::errc::io_error);
                return std::nullopt;
            }

            std::string bytes(static_cast<std::size_t>(size), '\0');
            if (!bytes.empty())
            {
                input.read(bytes.data(),
                    static_cast<std::streamsize>(bytes.size()));
                if (input.gcount()
                        != static_cast<std::streamsize>(bytes.size())
                    || input.bad())
                {
                    error = std::make_error_code(std::errc::io_error);
                    return std::nullopt;
                }
            }
            return bytes;
        }

        [[nodiscard]] TransactionCode validate_parent_chain(
            const fs::path& root,
            const fs::path& relative_parent,
            std::string& status)
        {
            fs::path current = root;
            for (const fs::path& component : relative_parent)
            {
                current /= component;
                std::error_code error;
                const fs::file_status entry =
                    fs::symlink_status(current, error);
                if (error || !fs::exists(entry))
                {
                    status = "Approved destination parent is missing: "                        + current.generic_string();
                    return TransactionCode::parent_missing;
                }
                if (fs::is_symlink(entry))
                {
                    status = "Symbolic links and junction-style redirects "
                        "are rejected below the approved workspace root.";
                    return TransactionCode::link_rejected;
                }
                if (!fs::is_directory(entry))
                {
                    status = "Approved destination parent is not a directory: "
                        + current.generic_string();
                    return TransactionCode::parent_missing;
                }
            }
            return TransactionCode::none;
        }

        [[nodiscard]] std::optional<fs::path> unused_sibling(
            const fs::path& destination,
            std::string_view role,
            std::uint64_t nonce,
            std::size_t change_index)
        {
            for (std::uint32_t attempt = 0u; attempt < 64u; ++attempt)
            {
                fs::path candidate = destination;
                candidate += ".epoch_ai_";
                candidate += role;
                candidate += "_";
                candidate += std::to_string(nonce);
                candidate += "_";
                candidate += std::to_string(change_index);
                candidate += "_";
                candidate += std::to_string(attempt);

                std::error_code error;
                const fs::file_status entry =
                    fs::symlink_status(candidate, error);
                if ((error && missing_error(error))
                    || (!error && !fs::exists(entry)))
                {
                    return candidate;
                }
                if (error)
                    return std::nullopt;
            }
            return std::nullopt;
        }

        void remove_if_present(const fs::path& path) noexcept
        {
            if (path.empty())
                return;
            std::error_code ignored;
            (void)fs::remove(path, ignored);
        }

        [[nodiscard]] bool verify_destination(
            const PreparedChange& prepared,
            bool expected_after,
            const TransactionLimits& limits)
        {
            const ContentState& expected = expected_after
                ? prepared.change.after
                : prepared.change.before;
            std::error_code error;
            const fs::file_status entry =
                fs::symlink_status(prepared.destination, error);
            const bool exists = !error && fs::exists(entry);
            if (error && !missing_error(error))
                return false;
            if (exists != expected.exists)
                return false;
            if (!exists)
                return true;
            if (fs::is_symlink(entry) || !fs::is_regular_file(entry))
                return false;
            const auto bytes = read_regular_file(
                prepared.destination,
                limits.maximum_file_bytes,
                error);
            return bytes && state_for(*bytes) == expected;
        }

        [[nodiscard]] bool rollback_prepared(
            std::vector<PreparedChange>& prepared,
            TransactionResult& result,
            const TransactionLimits& limits) noexcept
        {
            result.rollback_attempted = true;
            bool complete = true;
            for (std::size_t offset = prepared.size();
                 offset > 0u;
                 --offset)
            {
                PreparedChange& item = prepared[offset - 1u];
                bool item_complete = true;
                const bool touched = item.replacement_installed
                    || item.original_moved;
                if (item.replacement_installed)
                {
                    std::error_code remove_error;
                    if (!fs::remove(item.destination, remove_error)
                        && remove_error
                        && !missing_error(remove_error))
                    {
                        item_complete = false;
                    }
                    else
                    {
                        item.replacement_installed = false;
                    }
                }
                if (item.original_moved)
                {
                    std::error_code restore_error;
                    if (!platform::filesystem::
                            atomic_replace_same_filesystem(
                                item.backup,
                                item.destination,
                                restore_error))
                    {
                        item_complete = false;
                    }
                    else
                    {
                        item.original_moved = false;
                    }
                }
                if (touched && item_complete)
                {
                    try
                    {
                        item_complete = verify_destination(
                            item, false, limits);
                    }
                    catch (...)
                    {
                        item_complete = false;
                    }
                }
                if (item.replacement_installed || item.original_moved
                    || !item_complete)
                {
                    complete = false;
                }
                if (touched && item.evidence_index < result.files.size())
                {
                    result.files[item.evidence_index].rolled_back =
                        item_complete;
                }
            }
            result.rollback_complete = complete;
            return complete;
        }

        void cleanup_transaction_files(
            const std::vector<PreparedChange>& prepared) noexcept
        {
            for (const PreparedChange& item : prepared)
            {                remove_if_present(item.temporary);
                if (!item.original_moved)
                    remove_if_present(item.backup);
            }
        }

        [[nodiscard]] std::string manifest_for(
            const ContentDigest& authorization,
            const TransactionResult& result)
        {
            std::string manifest{"epoch_ai_source_transaction "};
            manifest += std::to_string(transaction_schema_version);
            manifest += "\nauthorization_sha256 ";
            manifest += digest_hex(authorization);
            manifest += "\nresult ";
            manifest += to_string(result.code);
            manifest += "\ncommitted ";
            manifest += result.committed ? "1" : "0";
            manifest += "\nrollback_attempted ";
            manifest += result.rollback_attempted ? "1" : "0";
            manifest += "\nrollback_complete ";
            manifest += result.rollback_complete ? "1" : "0";
            manifest += "\nfiles ";
            manifest += std::to_string(result.files.size());
            manifest += "\n";

            for (const FileEvidence& file : result.files)
            {
                manifest += "file ";
                manifest += std::to_string(file.stable_operation_id);
                manifest += "\t";
                manifest += file.relative_path;
                manifest += "\tbefore=";
                manifest += file.before.exists ? "sha256:" : "absent:";
                manifest += digest_hex(file.before.digest);
                manifest += ":";
                manifest += std::to_string(file.before.byte_count);
                manifest += "\tafter=";
                manifest += file.after.exists ? "sha256:" : "absent:";
                manifest += digest_hex(file.after.digest);
                manifest += ":";
                manifest += std::to_string(file.after.byte_count);
                manifest += "\tpreimage=";
                manifest += file.preimage_verified ? "1" : "0";
                manifest += "\ttemporary=";
                manifest += file.replacement_verified ? "1" : "0";
                manifest += "\tcommitted=";
                manifest += file.committed ? "1" : "0";
                manifest += "\trolled_back=";
                manifest += file.rolled_back ? "1" : "0";
                manifest += "\n";
            }
            return manifest;
        }

        void finalize(
            TransactionResult& result,
            const ContentDigest& authorization)
        {
            result.evidence_manifest = manifest_for(authorization, result);
            result.evidence_digest = digest(result.evidence_manifest);
        }

        [[nodiscard]] TransactionResult rejected(
            TransactionCode code,
            std::string status,
            const ContentDigest& authorization,
            std::vector<FileEvidence> files = {})
        {
            TransactionResult result{
                .code = code,
                .status = std::move(status),
                .files = std::move(files)};
            finalize(result, authorization);
            return result;
        }

        [[nodiscard]] TransactionResult exceptional_failure(
            TransactionCode code,
            std::string status,
            const ContentDigest& authorization,
            std::vector<PreparedChange>& prepared,
            const TransactionLimits& limits) noexcept
        {
            TransactionResult result{
                .code = code,
                .status = std::move(status)};
            bool touched{};
            for (const PreparedChange& item : prepared)
            {
                touched = touched || item.original_moved
                    || item.replacement_installed;
            }
            if (touched)
                (void)rollback_prepared(prepared, result, limits);
            cleanup_transaction_files(prepared);

            try
            {
                result.files.reserve(prepared.size());
                for (const PreparedChange& item : prepared)
                {
                    bool restored = !touched;
                    if (touched)
                    {
                        restored = verify_destination(
                            item, false, limits);
                    }
                    result.files.push_back({
                        .stable_operation_id =
                            item.change.stable_operation_id,
                        .relative_path = item.change.relative_path,
                        .before = item.change.before,
                        .after = item.change.after,
                        .preimage_verified = true,
                        .replacement_verified =
                            !item.change.after.exists
                            || !item.temporary.empty(),
                        .committed = false,
                        .rolled_back = touched && restored});
                }
                finalize(result, authorization);
            }
            catch (...)
            {
                result.files.clear();
                result.evidence_manifest =
                    "epoch_ai_source_transaction exception_cleanup";
                result.evidence_digest = digest(result.evidence_manifest);
            }
            return result;
        }
    }

    ContentDigest digest(std::string_view bytes) noexcept
    {
        ContentDigest result{};
        result.bytes = core::sha256::hash(bytes).bytes;
        return result;
    }

    std::string digest_hex(const ContentDigest& value)
    {
        core::sha256::Digest source{};
        source.bytes = value.bytes;
        return core::sha256::hex(source);
    }

    SourceTransactionExecutor::SourceTransactionExecutor(
        TransactionLimits limits)
        : limits_(limits)
    {
    }

    const TransactionLimits&
    SourceTransactionExecutor::limits() const noexcept
    {
        return limits_;
    }

    TransactionResult SourceTransactionExecutor::execute(
        const TransactionRequest& request) const
    {
        ContentDigest authorization{};
        authorization.bytes = request.permit.digest().bytes;
        if (!valid_limits(limits_))
        {
            return rejected(
                TransactionCode::invalid_limits,
                "Source transaction limits are invalid.",
                authorization);
        }
        if (!request.permit.valid()
            || !authorization.valid()
            || request.changes.empty()
            || request.changes.size() > limits_.maximum_changes)
        {
            return rejected(
                TransactionCode::invalid_authorization,
                "A content-addressed authorization and bounded change set "
                "are required.",
                authorization);
        }

        std::vector<PreparedChange> prepared{};
        try
        {
            const fs::path supplied_root{request.workspace_root};
            if (request.workspace_root.empty()
                || !supplied_root.is_absolute())
            {
                return rejected(
                    TransactionCode::invalid_root,
                    "The approved workspace root must be absolute.",
                    authorization);
            }

            std::error_code error;
            const fs::file_status supplied_status =
                fs::symlink_status(supplied_root, error);
            if (error || !fs::exists(supplied_status)
                || !fs::is_directory(supplied_status))
            {
                return rejected(
                    TransactionCode::invalid_root,
                    "The approved workspace root does not exist as a "
                    "directory.",
                    authorization);
            }
            if (fs::is_symlink(supplied_status))            {
                return rejected(
                    TransactionCode::link_rejected,
                    "The approved workspace root may not be a symbolic link.",
                    authorization);
            }

            const fs::path root = fs::canonical(supplied_root, error);
            if (error || root.empty())
            {
                return rejected(
                    TransactionCode::invalid_root,
                    "The approved workspace root could not be canonicalized.",
                    authorization);
            }


            std::vector<FileEvidence> evidence{};
            prepared.reserve(request.changes.size());
            evidence.reserve(request.changes.size());
            std::uint64_t total_bytes = 0u;

            for (std::size_t index = 0u;
                 index < request.changes.size();
                 ++index)
            {
                const SourceChange& source = request.changes[index];
                if (source.stable_operation_id == 0u
                    || !valid_state(source.before)
                    || !valid_state(source.after)
                    || source.before == source.after)
                {
                    return rejected(
                        TransactionCode::invalid_change,
                        "Every approved source change needs a stable identity "
                        "and distinct valid content states.",
                        authorization,
                        std::move(evidence));
                }

                const auto relative =
                    guard::canonical_relative_path(source.relative_path);
                if (!relative
                    || relative->size() > limits_.maximum_path_bytes)
                {
                    return rejected(
                        TransactionCode::invalid_path,
                        "A source change path is not a canonical bounded "
                        "workspace-relative path.",
                        authorization,
                        std::move(evidence));
                }
                for (const PreparedChange& prior : prepared)
                {
                    if (same_path_identity(prior.change.relative_path, *relative)
                        || prior.change.stable_operation_id
                            == source.stable_operation_id)
                    {
                        return rejected(
                            TransactionCode::duplicate_path,
                            "Duplicate source path or operation identity was "
                            "rejected.",
                            authorization,
                            std::move(evidence));
                    }
                }

                SourceChange change = source;
                change.relative_path = *relative;
                if (change.before.byte_count > limits_.maximum_file_bytes
                    || change.after.byte_count > limits_.maximum_file_bytes
                    || change.replacement_bytes.size()
                        > limits_.maximum_file_bytes
                    || !checked_add(total_bytes, change.before.byte_count,
                        limits_.maximum_total_bytes)
                    || !checked_add(total_bytes, change.after.byte_count,
                        limits_.maximum_total_bytes))
                {
                    return rejected(
                        TransactionCode::size_limit_exceeded,
                        "The approved transaction exceeds its file or total "
                        "byte budget.",
                        authorization,
                        std::move(evidence));
                }

                const ContentState replacement_state =
                    state_for(change.replacement_bytes);
                if ((!change.after.exists
                        && !change.replacement_bytes.empty())
                    || (change.after.exists
                        && replacement_state != change.after))
                {
                    return rejected(
                        TransactionCode::postimage_mismatch,
                        "Replacement bytes do not match the approved "
                        "postimage.",
                        authorization,
                        std::move(evidence));
                }

                const fs::path relative_path{*relative};
                std::string parent_status;
                const TransactionCode parent_code =
                    validate_parent_chain(
                        root,
                        relative_path.parent_path(),
                        parent_status);
                if (parent_code != TransactionCode::none)
                {
                    return rejected(
                        parent_code,
                        std::move(parent_status),
                        authorization,
                        std::move(evidence));
                }

                PreparedChange item{
                    .change = std::move(change),
                    .destination = root / relative_path,
                    .evidence_index = evidence.size()};

                const fs::file_status destination_status =
                    fs::symlink_status(item.destination, error);
                const bool destination_exists =
                    !error && fs::exists(destination_status);
                if (error && !missing_error(error))
                {
                    return rejected(
                        TransactionCode::filesystem_failure,
                        "Destination state could not be inspected: "
                            + error.message(),
                        authorization,
                        std::move(evidence));
                }
                error.clear();
                if (destination_exists
                    && fs::is_symlink(destination_status))
                {
                    return rejected(
                        TransactionCode::link_rejected,
                        "A source destination may not be a symbolic link.",
                        authorization,
                        std::move(evidence));
                }
                if (destination_exists
                    && !fs::is_regular_file(destination_status))
                {
                    return rejected(
                        TransactionCode::destination_type_rejected,                        "A source destination exists but is not a regular "
                        "file.",
                        authorization,
                        std::move(evidence));
                }
                if (destination_exists != item.change.before.exists)
                {
                    return rejected(
                        TransactionCode::preimage_mismatch,
                        "The source file existence state changed after "
                        "approval.",
                        authorization,
                        std::move(evidence));
                }

                if (destination_exists)
                {
                    auto bytes = read_regular_file(
                        item.destination,
                        limits_.maximum_file_bytes,
                        error);
                    if (!bytes || state_for(*bytes) != item.change.before)
                    {
                        return rejected(
                            TransactionCode::preimage_mismatch,
                            "The source file bytes changed after approval.",
                            authorization,
                            std::move(evidence));
                    }
                    item.original_bytes = std::move(*bytes);
                }

                evidence.push_back({
                    .stable_operation_id =
                        item.change.stable_operation_id,
                    .relative_path = item.change.relative_path,
                    .before = item.change.before,
                    .after = item.change.after,
                    .preimage_verified = true});
                prepared.push_back(std::move(item));
            }

            TransactionResult result{
                .code = TransactionCode::none,
                .status = "All approved source preimages were verified.",
                .files = std::move(evidence),
                .all_preimages_verified = true};

            static std::atomic<std::uint64_t> next_nonce{1u};
            const std::uint64_t nonce =
                next_nonce.fetch_add(1u, std::memory_order_relaxed);

            for (std::size_t index = 0u;
                 index < prepared.size();
                 ++index)
            {
                PreparedChange& item = prepared[index];
                if (item.change.after.exists)
                {
                    const auto temporary = unused_sibling(
                        item.destination, "write", nonce, index);
                    if (!temporary)
                    {
                        cleanup_transaction_files(prepared);
                        result.code =
                            TransactionCode::temporary_path_unavailable;
                        result.status =
                            "No unused same-directory temporary path was "
                            "available.";
                        finalize(result, authorization);
                        return result;
                    }
                    item.temporary = *temporary;

                    const std::span<const char> replacement_chars{
                        item.change.replacement_bytes.data(),
                        item.change.replacement_bytes.size()};
                    if (!platform::filesystem::exclusive_create_and_write(
                            item.temporary,
                            std::as_bytes(replacement_chars),
                            error))
                    {
                        cleanup_transaction_files(prepared);
                        result.code =
                            TransactionCode::temporary_write_failed;
                        result.status =
                            "An approved replacement temporary file could "
                            "not be created exclusively and flushed.";
                        finalize(result, authorization);
                        return result;
                    }
                    if (item.change.before.exists)
                    {
                        std::error_code permission_error{};
                        const fs::file_status original_status =
                            fs::status(item.destination, permission_error);
                        if (permission_error)
                        {
                            cleanup_transaction_files(prepared);
                            result.code = TransactionCode::filesystem_failure;
                            result.status =
                                "Existing source metadata could not be inspected.";
                            finalize(result, authorization);
                            return result;
                        }
                        fs::permissions(
                            item.temporary,
                            original_status.permissions(),
                            fs::perm_options::replace,
                            permission_error);
                        if (permission_error)
                        {
                            cleanup_transaction_files(prepared);
                            result.code = TransactionCode::temporary_write_failed;
                            result.status =
                                "Replacement source permissions could not be preserved.";
                            finalize(result, authorization);
                            return result;
                        }
                    }
                    auto verified = read_regular_file(
                        item.temporary,
                        limits_.maximum_file_bytes,
                        error);
                    if (!verified
                        || *verified != item.change.replacement_bytes
                        || state_for(*verified) != item.change.after)
                    {
                        cleanup_transaction_files(prepared);
                        result.code =
                            TransactionCode::temporary_verification_failed;
                        result.status =
                            "Temporary replacement bytes did not match the "
                            "approved postimage.";
                        finalize(result, authorization);
                        return result;
                    }
                }
                result.files[item.evidence_index].
                    replacement_verified = true;

                if (item.change.before.exists)
                {
                    const auto backup = unused_sibling(
                        item.destination, "backup", nonce, index);
                    if (!backup)
                    {
                        cleanup_transaction_files(prepared);
                        result.code =
                            TransactionCode::temporary_path_unavailable;
                        result.status =
                            "No unused same-directory rollback path was "
                            "available.";
                        finalize(result, authorization);
                        return result;
                    }
                    item.backup = *backup;
                }
            }
            result.all_temporaries_verified = true;
            for (const PreparedChange& item : prepared)
            {
                if (!verify_destination(item, false, limits_))
                {
                    cleanup_transaction_files(prepared);
                    result.code = TransactionCode::preimage_mismatch;
                    result.status =
                        "A source preimage changed while the approved "
                        "transaction was being prepared.";
                    finalize(result, authorization);
                    return result;
                }
            }

            bool commit_failed = false;
            std::string commit_error{};
            for (PreparedChange& item : prepared)
            {
                std::string parent_status{};
                const TransactionCode parent_code = validate_parent_chain(
                    root,
                    fs::path{item.change.relative_path}.parent_path(),
                    parent_status);
                if (parent_code != TransactionCode::none
                    || !verify_destination(item, false, limits_))
                {
                    commit_failed = true;
                    commit_error = parent_code != TransactionCode::none
                        ? std::move(parent_status)
                        : "source preimage changed immediately before commit";
                    break;
                }

                if (item.change.before.exists)
                {
                    if (!platform::filesystem::
                            atomic_replace_same_filesystem(
                                item.destination,
                                item.backup,
                                error))
                    {
                        commit_failed = true;
                        commit_error = error.message();
                        break;
                    }
                    item.original_moved = true;
                }

                if (item.change.after.exists)
                {
                    if (!platform::filesystem::
                            atomic_replace_same_filesystem(
                                item.temporary,
                                item.destination,
                                error))
                    {
                        commit_failed = true;
                        commit_error = error.message();
                        break;
                    }
                    item.replacement_installed = true;
                }
                result.files[item.evidence_index].committed = true;
            }

            if (!commit_failed)
            {
                for (const PreparedChange& item : prepared)
                {
                    if (!verify_destination(item, true, limits_))
                    {
                        commit_failed = true;
                        commit_error =
                            "committed destination did not match the "
                            "approved postimage";
                        break;
                    }
                }
            }

            if (commit_failed)
            {
                const bool rolled_back =
                    rollback_prepared(prepared, result, limits_);
                cleanup_transaction_files(prepared);
                result.code = rolled_back
                    ? TransactionCode::commit_failed
                    : TransactionCode::rollback_failed;
                result.status = rolled_back
                    ? "Source transaction commit failed and every prior "
                        "change was rolled back: " + commit_error
                    : "Source transaction commit failed and rollback was "
                        "incomplete: " + commit_error;
                finalize(result, authorization);
                return result;
            }

            result.committed = true;
            result.rollback_complete = true;
            result.status =
                "Exact approved source postimages committed.";
            for (PreparedChange& item : prepared)
            {
                if (item.original_moved)
                {
                    std::error_code cleanup_error;
                    if (!fs::remove(item.backup, cleanup_error)
                        && cleanup_error)
                    {
                        result.backup_cleanup_complete = false;
                    }
                    else
                    {
                        item.original_moved = false;
                    }
                }
                remove_if_present(item.temporary);
            }
            if (!result.backup_cleanup_complete)
            {
                result.status +=
                    " One or more rollback backups require cleanup.";
            }
            finalize(result, authorization);
            return result;
        }
        catch (const fs::filesystem_error& exception)
        {
            return exceptional_failure(
                TransactionCode::filesystem_failure,
                exception.what(),
                authorization,
                prepared,
                limits_);
        }
        catch (const std::exception& exception)
        {
            return exceptional_failure(
                TransactionCode::filesystem_failure,
                exception.what(),
                authorization,
                prepared,
                limits_);
        }
        catch (...)
        {
            return exceptional_failure(
                TransactionCode::filesystem_failure,
                "Unexpected source transaction failure.",
                authorization,
                prepared,
                limits_);
        }
    }

    SourceWorkspaceMaterializer::SourceWorkspaceMaterializer(
        WorkspaceLimits limits)
        : limits_(limits)
    {
    }

    const WorkspaceLimits&
    SourceWorkspaceMaterializer::limits() const noexcept
    {
        return limits_;
    }

    WorkspaceResult SourceWorkspaceMaterializer::materialize(
        const WorkspaceRequest& request) const
    {
        WorkspaceResult result{};
        const auto fail = [&](WorkspaceCode code, std::string status)
        {
            result.code = code;
            result.status = std::move(status);
            return result;
        };
        const auto valid_limits = [&]() noexcept
        {
            return limits_.maximum_include_paths > 0u
                && limits_.maximum_include_paths <= 64u
                && limits_.maximum_excluded_components <= 128u
                && limits_.maximum_files > 0u
                && limits_.maximum_files <= 65'536u
                && limits_.maximum_file_bytes > 0u
                && limits_.maximum_file_bytes <= 256u * 1024u * 1024u
                && limits_.maximum_total_bytes >= limits_.maximum_file_bytes
                && limits_.maximum_total_bytes <= 2ull * 1024ull * 1024ull * 1024ull
                && limits_.maximum_path_bytes > 0u
                && limits_.maximum_path_bytes <= 16u * 1024u;
        };
        if (!valid_limits())
        {
            return fail(
                WorkspaceCode::invalid_limits,
                "Disposable source-workspace limits are invalid.");
        }
        if (request.include_paths.empty()
            || request.include_paths.size() > limits_.maximum_include_paths
            || request.excluded_components.size()
                > limits_.maximum_excluded_components)
        {
            return fail(
                WorkspaceCode::invalid_request,
                "Disposable source-workspace include or exclusion bounds are invalid.");
        }

        const auto component_usable = [](std::string_view component) noexcept
        {
            return !component.empty()
                && component != "."
                && component != ".."
                && component.find('/') == std::string_view::npos
                && component.find('\\') == std::string_view::npos
                && std::none_of(
                    component.begin(),
                    component.end(),
                    [](char value)
                    {
                        const unsigned char byte =
                            static_cast<unsigned char>(value);
                        return byte == 0u || byte < 0x20u;
                    });
        };
        if (std::any_of(
                request.excluded_components.begin(),
                request.excluded_components.end(),
                [&](const std::string& component)
                {
                    return !component_usable(component);
                }))
        {
            return fail(
                WorkspaceCode::invalid_request,
                "A disposable source-workspace exclusion component is invalid.");
        }

        try
        {
            const fs::path source_root =
                fs::path{request.source_root}.lexically_normal();
            const fs::path workspace_root =
                fs::path{request.workspace_root}.lexically_normal();
            if (!source_root.is_absolute()
                || !workspace_root.is_absolute()
                || source_root == workspace_root)
            {
                return fail(
                    WorkspaceCode::invalid_root,
                    "Source and disposable workspace roots must be distinct absolute paths.");
            }

            std::error_code error{};
            const fs::file_status source_status =
                fs::symlink_status(source_root, error);
            if (error || fs::is_symlink(source_status)
                || !fs::is_directory(source_status))
            {
                return fail(
                    WorkspaceCode::invalid_root,
                    "The source root is unavailable or link-backed.");
            }
            error.clear();
            const fs::file_status workspace_status =
                fs::symlink_status(workspace_root, error);
            if (error || fs::is_symlink(workspace_status)
                || !fs::is_directory(workspace_status))
            {
                return fail(
                    WorkspaceCode::invalid_root,
                    "The disposable workspace root is unavailable or link-backed.");
            }

            fs::recursive_directory_iterator destination_cursor{
                workspace_root,
                fs::directory_options::skip_permission_denied,
                error};
            const fs::recursive_directory_iterator destination_end{};
            if (error)
            {
                return fail(
                    WorkspaceCode::invalid_root,
                    "The disposable workspace root could not be inspected.");
            }
            while (destination_cursor != destination_end)
            {
                error.clear();
                const fs::file_status status =
                    destination_cursor->symlink_status(error);
                if (error || fs::is_symlink(status))
                {
                    return fail(
                        WorkspaceCode::link_rejected,
                        "The disposable workspace already contains an unsafe link.");
                }
                if (!fs::is_directory(status))
                {
                    return fail(
                        WorkspaceCode::destination_not_empty,
                        "The disposable workspace already contains file data.");
                }
                destination_cursor.increment(error);
                if (error)
                {
                    return fail(
                        WorkspaceCode::invalid_root,
                        "The disposable workspace could not be inspected exactly.");
                }
            }

            struct PlannedFile final
            {
                std::string relative_path{};
                fs::path source{};
                std::uint64_t byte_count{};
                ContentDigest source_digest{};
            };
            std::vector<PlannedFile> files{};
            files.reserve((std::min)(
                limits_.maximum_files,
                std::size_t{4'096u}));

            const auto excluded_path = [&](const fs::path& relative)
            {
                if (owned_runtime_workspace_path(relative))
                    return true;
                for (const fs::path& component : relative)
                {
                    const std::string text = component.generic_string();
                    if (std::any_of(
                            request.excluded_components.begin(),
                            request.excluded_components.end(),
                            [&](const std::string& excluded)
                            {
                                return same_path_identity(text, excluded);
                            }))
                    {
                        return true;
                    }
                }
                return false;
            };
            const auto cancelled = [&]()
            {
                return request.cancellation_requested
                    && request.cancellation_requested();
            };
            const auto add_file = [&](const fs::path& source,
                                      const fs::path& relative)
                -> WorkspaceCode
            {
                const std::string relative_text = relative.generic_string();
                if (relative_text.empty()
                    || relative_text.size() > limits_.maximum_path_bytes
                    || files.size() >= limits_.maximum_files)
                {
                    return WorkspaceCode::size_limit_exceeded;
                }
                if (std::any_of(
                        files.begin(),
                        files.end(),
                        [&](const PlannedFile& existing)
                        {
                            return same_path_identity(
                                existing.relative_path,
                                relative_text);
                        }))
                {
                    return WorkspaceCode::invalid_request;
                }

                std::error_code size_error{};
                const std::uintmax_t size = fs::file_size(source, size_error);
                if (size_error || size > limits_.maximum_file_bytes
                    || size > (std::numeric_limits<std::uint64_t>::max)())
                {
                    return WorkspaceCode::size_limit_exceeded;
                }
                if (!checked_add(
                        result.total_bytes,
                        static_cast<std::uint64_t>(size),
                        limits_.maximum_total_bytes))
                {
                    return WorkspaceCode::size_limit_exceeded;
                }
                files.push_back({
                    .relative_path = relative_text,
                    .source = source,
                    .byte_count = static_cast<std::uint64_t>(size)});
                return WorkspaceCode::none;
            };

            for (const std::string& include_text : request.include_paths)
            {
                if (cancelled())
                {
                    return fail(
                        WorkspaceCode::cancelled,
                        "Disposable workspace materialization was cancelled.");
                }
                const auto canonical =
                    guard::canonical_relative_path(include_text);
                if (!canonical || *canonical != include_text)
                {
                    return fail(
                        WorkspaceCode::invalid_request,
                        "A disposable workspace include path is not canonical.");
                }
                const fs::path relative{*canonical};
                if (excluded_path(relative))
                {
                    return fail(
                        WorkspaceCode::invalid_request,
                        "An include path is also excluded from the disposable workspace.");
                }

                fs::path current = source_root;
                for (const fs::path& component : relative)
                {
                    current /= component;
                    error.clear();
                    const fs::file_status component_status =
                        fs::symlink_status(current, error);
                    if (error || fs::is_symlink(component_status))
                    {
                        return fail(
                            WorkspaceCode::link_rejected,
                            "A source include path crosses an unsafe link.");
                    }
                }

                error.clear();
                const fs::file_status include_status =
                    fs::symlink_status(current, error);
                if (error || !fs::exists(include_status))
                {
                    return fail(
                        WorkspaceCode::invalid_request,
                        "A requested source include path does not exist.");
                }
                if (fs::is_regular_file(include_status))
                {
                    const WorkspaceCode added = add_file(current, relative);
                    if (added != WorkspaceCode::none)
                    {
                        return fail(
                            added,
                            "A requested source file exceeds workspace bounds or is duplicated.");
                    }
                    continue;
                }
                if (!fs::is_directory(include_status))
                {
                    return fail(
                        WorkspaceCode::invalid_request,
                        "A requested source include is not a regular file or directory.");
                }

                fs::recursive_directory_iterator cursor{
                    current,
                    fs::directory_options::skip_permission_denied,
                    error};
                const fs::recursive_directory_iterator end{};
                if (error)
                {
                    return fail(
                        WorkspaceCode::filesystem_failure,
                        "A requested source directory could not be enumerated.");
                }
                while (cursor != end)
                {
                    if (cancelled())
                    {
                        return fail(
                            WorkspaceCode::cancelled,
                            "Disposable workspace materialization was cancelled.");
                    }
                    const fs::directory_entry entry = *cursor;
                    const fs::path entry_relative =
                        entry.path().lexically_relative(source_root);
                    if (excluded_path(entry_relative))
                    {
                        error.clear();
                        if (entry.is_directory(error) && !error)
                            cursor.disable_recursion_pending();
                    }
                    else
                    {
                        error.clear();
                        const fs::file_status entry_status =
                            entry.symlink_status(error);
                        if (error || fs::is_symlink(entry_status))
                        {
                            return fail(
                                WorkspaceCode::link_rejected,
                                "The source tree contains a link inside an admitted include.");
                        }
                        if (fs::is_regular_file(entry_status))
                        {
                            const WorkspaceCode added =
                                add_file(entry.path(), entry_relative);
                            if (added != WorkspaceCode::none)
                            {
                                return fail(
                                    added,
                                    "The source tree exceeds disposable workspace bounds or aliases a path.");
                            }
                        }
                        else if (!fs::is_directory(entry_status))
                        {
                            return fail(
                                WorkspaceCode::invalid_request,
                                "The source tree contains an unsupported filesystem entry.");
                        }
                    }
                    cursor.increment(error);
                    if (error)
                    {
                        return fail(
                            WorkspaceCode::filesystem_failure,
                            "The source tree enumeration failed.");
                    }
                }
            }

            std::ranges::sort(
                files,
                {},
                &PlannedFile::relative_path);
            std::string manifest{"EPOCH_AI_WORKSPACE_V1\n"};
            manifest += "FILE_COUNT " + std::to_string(files.size()) + "\n";
            manifest += "TOTAL_BYTES "
                + std::to_string(result.total_bytes) + "\n";

            for (PlannedFile& file : files)
            {
                if (cancelled())
                {
                    return fail(
                        WorkspaceCode::cancelled,
                        "Disposable workspace materialization was cancelled.");
                }
                error.clear();
                const auto source_bytes = read_regular_file(
                    file.source,
                    limits_.maximum_file_bytes,
                    error);
                if (!source_bytes
                    || source_bytes->size() != file.byte_count)
                {
                    return fail(
                        WorkspaceCode::copy_failed,
                        "A source file changed or became unreadable before copy.");
                }
                file.source_digest = digest(*source_bytes);

                const fs::path destination =
                    workspace_root / fs::path{file.relative_path};
                error.clear();
                if (!fs::create_directories(destination.parent_path(), error)
                    && error)
                {
                    return fail(
                        WorkspaceCode::copy_failed,
                        "A disposable workspace directory could not be created.");
                }
                error.clear();
                const fs::file_status destination_status =
                    fs::symlink_status(destination, error);
                const bool destination_missing = missing_error(error)
                    || (!error && destination_status.type()
                        == fs::file_type::not_found);
                if (!destination_missing)
                {
                    return fail(
                        WorkspaceCode::destination_not_empty,
                        "A disposable workspace destination already exists.");
                }

                std::ofstream output{
                    destination,
                    std::ios::binary | std::ios::trunc};
                if (!output.is_open())
                {
                    return fail(
                        WorkspaceCode::copy_failed,
                        "A disposable source copy could not be opened.");
                }
                output.write(
                    source_bytes->data(),
                    static_cast<std::streamsize>(source_bytes->size()));
                output.flush();
                if (!output)
                {
                    return fail(
                        WorkspaceCode::copy_failed,
                        "A disposable source copy could not be written exactly.");
                }
                output.close();

                error.clear();
                const auto copied_bytes = read_regular_file(
                    destination,
                    limits_.maximum_file_bytes,
                    error);
                if (!copied_bytes
                    || *copied_bytes != *source_bytes
                    || digest(*copied_bytes) != file.source_digest)
                {
                    return fail(
                        WorkspaceCode::verification_failed,
                        "A disposable source copy failed exact verification.");
                }
                error.clear();
                const auto source_after = read_regular_file(
                    file.source,
                    limits_.maximum_file_bytes,
                    error);
                if (!source_after
                    || digest(*source_after) != file.source_digest
                    || source_after->size() != file.byte_count)
                {
                    return fail(
                        WorkspaceCode::verification_failed,
                        "Live source changed while the disposable workspace was materialized.");
                }

                manifest += "FILE " + file.relative_path + " "
                    + std::to_string(file.byte_count) + " "
                    + digest_hex(file.source_digest) + "\n";
            }

            for (const PlannedFile& file : files)
            {
                error.clear();
                const auto source_after = read_regular_file(
                    file.source,
                    limits_.maximum_file_bytes,
                    error);
                if (!source_after
                    || source_after->size() != file.byte_count
                    || digest(*source_after) != file.source_digest)
                {
                    return fail(
                        WorkspaceCode::verification_failed,
                        "Live source changed before workspace materialization completed.");
                }
            }

            result.file_count = files.size();
            result.evidence_manifest = std::move(manifest);
            result.evidence_digest = digest(result.evidence_manifest);
            result.source_unchanged = true;
            result.completed = true;
            result.code = WorkspaceCode::none;
            result.status =
                "Disposable source workspace materialized and verified without modifying live source.";
            return result;
        }
        catch (const fs::filesystem_error& exception)
        {
            return fail(
                WorkspaceCode::filesystem_failure,
                std::string{"Disposable workspace filesystem failure: "}
                    + exception.what());
        }
        catch (const std::exception& exception)
        {
            return fail(
                WorkspaceCode::filesystem_failure,
                std::string{"Disposable workspace failure: "}
                    + exception.what());
        }
        catch (...)
        {
            return fail(
                WorkspaceCode::filesystem_failure,
                "Disposable workspace failed with an unknown exception.");
        }
    }
}
