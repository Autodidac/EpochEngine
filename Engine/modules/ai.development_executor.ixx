/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

export module ai.development_executor;

import ai.development_guard;

export namespace epochengine::ai::development_executor
{
    inline constexpr std::uint32_t transaction_schema_version = 1u;

    struct ContentDigest final
    {
        std::array<std::uint8_t, 32u> bytes{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            for (const std::uint8_t byte : bytes)
            {
                if (byte != 0u)
                    return true;
            }
            return false;
        }

        friend constexpr bool operator==(
            const ContentDigest& left,
            const ContentDigest& right) noexcept
        {
            for (std::size_t index = 0u;
                 index < left.bytes.size();
                 ++index)
            {
                if (left.bytes[index] != right.bytes[index])
                    return false;
            }
            return true;
        }
    };

    struct ContentState final
    {
        bool exists{};
        ContentDigest digest{};
        std::uint64_t byte_count{};

        friend constexpr bool operator==(
            const ContentState& left,
            const ContentState& right) noexcept
        {
            return left.exists == right.exists
                && left.digest == right.digest
                && left.byte_count == right.byte_count;
        }
    };

    struct SourceChange final
    {
        std::uint64_t stable_operation_id{};
        std::string relative_path{};
        ContentState before{};
        ContentState after{};
        std::string replacement_bytes{};
    };

    struct TransactionLimits final
    {
        std::size_t maximum_changes{32u};
        std::uint64_t maximum_file_bytes{16u * 1024u * 1024u};
        std::uint64_t maximum_total_bytes{64u * 1024u * 1024u};
        std::size_t maximum_path_bytes{1024u};
    };

    enum class TransactionCode : std::uint8_t
    {
        none,
        invalid_limits,
        invalid_authorization,
        invalid_root,
        invalid_change,
        invalid_path,
        duplicate_path,
        link_rejected,
        parent_missing,
        destination_type_rejected,
        preimage_mismatch,
        postimage_mismatch,
        size_limit_exceeded,
        temporary_path_unavailable,
        temporary_write_failed,
        temporary_verification_failed,
        commit_failed,
        rollback_failed,
        filesystem_failure
    };

    struct FileEvidence final
    {
        std::uint64_t stable_operation_id{};
        std::string relative_path{};
        ContentState before{};
        ContentState after{};
        bool preimage_verified{};
        bool replacement_verified{};
        bool committed{};
        bool rolled_back{};
    };

    struct TransactionRequest final
    {
        std::string workspace_root{};
        development_guard::ExecutionPermit permit{};
        std::vector<SourceChange> changes{};
    };

    struct TransactionResult final
    {
        TransactionCode code{TransactionCode::invalid_authorization};
        std::string status{};
        std::vector<FileEvidence> files{};
        std::string evidence_manifest{};
        ContentDigest evidence_digest{};
        bool all_preimages_verified{};
        bool all_temporaries_verified{};
        bool committed{};
        bool rollback_attempted{};
        bool rollback_complete{};
        bool backup_cleanup_complete{true};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == TransactionCode::none && committed;
        }
    };

    [[nodiscard]] constexpr std::string_view to_string(
        TransactionCode code) noexcept
    {
        switch (code)
        {
        case TransactionCode::none: return "none";
        case TransactionCode::invalid_limits: return "invalid_limits";
        case TransactionCode::invalid_authorization:
            return "invalid_authorization";
        case TransactionCode::invalid_root: return "invalid_root";
        case TransactionCode::invalid_change: return "invalid_change";
        case TransactionCode::invalid_path: return "invalid_path";
        case TransactionCode::duplicate_path: return "duplicate_path";
        case TransactionCode::link_rejected: return "link_rejected";
        case TransactionCode::parent_missing: return "parent_missing";
        case TransactionCode::destination_type_rejected:
            return "destination_type_rejected";
        case TransactionCode::preimage_mismatch: return "preimage_mismatch";
        case TransactionCode::postimage_mismatch: return "postimage_mismatch";
        case TransactionCode::size_limit_exceeded:
            return "size_limit_exceeded";
        case TransactionCode::temporary_path_unavailable:
            return "temporary_path_unavailable";
        case TransactionCode::temporary_write_failed:
            return "temporary_write_failed";
        case TransactionCode::temporary_verification_failed:
            return "temporary_verification_failed";
        case TransactionCode::commit_failed: return "commit_failed";
        case TransactionCode::rollback_failed: return "rollback_failed";
        case TransactionCode::filesystem_failure:
            return "filesystem_failure";
        }
        return "unknown";
    }

    [[nodiscard]] ContentDigest digest(std::string_view bytes) noexcept;
    [[nodiscard]] std::string digest_hex(const ContentDigest& value);

    enum class WorkspaceCode : std::uint8_t
    {
        none,
        invalid_limits,
        invalid_root,
        invalid_request,
        destination_not_empty,
        link_rejected,
        size_limit_exceeded,
        cancelled,
        copy_failed,
        verification_failed,
        filesystem_failure
    };

    struct WorkspaceLimits final
    {
        std::size_t maximum_include_paths{16u};
        std::size_t maximum_excluded_components{32u};
        std::size_t maximum_files{4'096u};
        std::uint64_t maximum_file_bytes{64u * 1024u * 1024u};
        std::uint64_t maximum_total_bytes{512u * 1024u * 1024u};
        std::size_t maximum_path_bytes{4'096u};
    };

    struct WorkspaceRequest final
    {
        std::string source_root{};
        std::string workspace_root{};
        std::vector<std::string> include_paths{};
        std::vector<std::string> excluded_components{};
        std::function<bool()> cancellation_requested{};
    };

    struct WorkspaceResult final
    {
        WorkspaceCode code{WorkspaceCode::invalid_request};
        std::string status{};
        std::string evidence_manifest{};
        ContentDigest evidence_digest{};
        std::size_t file_count{};
        std::uint64_t total_bytes{};
        bool source_unchanged{};
        bool completed{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == WorkspaceCode::none
                && source_unchanged
                && completed;
        }
    };

    [[nodiscard]] constexpr std::string_view to_string(
        WorkspaceCode code) noexcept
    {
        switch (code)
        {
        case WorkspaceCode::none: return "none";
        case WorkspaceCode::invalid_limits: return "invalid_limits";
        case WorkspaceCode::invalid_root: return "invalid_root";
        case WorkspaceCode::invalid_request: return "invalid_request";
        case WorkspaceCode::destination_not_empty:
            return "destination_not_empty";
        case WorkspaceCode::link_rejected: return "link_rejected";
        case WorkspaceCode::size_limit_exceeded:
            return "size_limit_exceeded";
        case WorkspaceCode::cancelled: return "cancelled";
        case WorkspaceCode::copy_failed: return "copy_failed";
        case WorkspaceCode::verification_failed:
            return "verification_failed";
        case WorkspaceCode::filesystem_failure:
            return "filesystem_failure";
        }
        return "unknown";
    }

    class SourceWorkspaceMaterializer final
    {
    public:
        explicit SourceWorkspaceMaterializer(
            WorkspaceLimits limits = {});

        [[nodiscard]] WorkspaceResult materialize(
            const WorkspaceRequest& request) const;

        [[nodiscard]] const WorkspaceLimits& limits() const noexcept;

    private:
        WorkspaceLimits limits_{};
    };

    class SourceTransactionExecutor final
    {
    public:
        explicit SourceTransactionExecutor(
            TransactionLimits limits = {});

        [[nodiscard]] TransactionResult execute(
            const TransactionRequest& request) const;

        [[nodiscard]] const TransactionLimits& limits() const noexcept;

    private:
        TransactionLimits limits_{};
    };

    [[nodiscard]] bool run_contract();
}
