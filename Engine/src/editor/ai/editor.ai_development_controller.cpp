/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module editor.ai_development_controller;

import ai.development_guard;
import ai.development_executor;
import ai.development_proposal_codec;
import core.sha256;

namespace epochengine::editor_ai_development
{
    namespace guard = ai::development_guard;

    namespace
    {
        constexpr std::size_t maximum_operations = 12u;
        constexpr std::size_t maximum_evidence_items = 12u;
        constexpr std::size_t maximum_text_bytes = 4096u;

        [[nodiscard]] EvidenceDigest sha256_content(
            std::string_view content) noexcept
        {
            EvidenceDigest result{};
            result.bytes = core::sha256::hash(content).bytes;
            return result;
        }

        [[nodiscard]] guard::WorkspacePermission readable_writable() noexcept
        {
            return guard::WorkspacePermission::read
                | guard::WorkspacePermission::write
                | guard::WorkspacePermission::create;
        }

        [[nodiscard]] guard::WorkspacePermission build_permissions() noexcept
        {
            return readable_writable() | guard::WorkspacePermission::execute;
        }

        [[nodiscard]] guard::SessionPolicy make_policy(
            const SessionConfiguration& configuration)
        {
            guard::GuardLimits limits{};
            limits.maximum_allowlist_rules = 4u;
            limits.maximum_operators = 1u;
            limits.maximum_proposals = 1u;
            limits.maximum_operations_per_proposal = maximum_operations;
            limits.maximum_intents_per_operation = 3u;
            limits.maximum_evidence_items = maximum_evidence_items;
            limits.maximum_evidence_records = 1u;
            limits.maximum_audit_events = 16u;
            limits.maximum_string_bytes = maximum_text_bytes;
            limits.maximum_total_proposal_bytes = 64u * 1024u;
            limits.maximum_total_evidence_bytes = 256u * 1024u;

            const std::uint64_t session_lifetime = configuration.expires_at.value
                > configuration.opened_at.value
                ? configuration.expires_at.value - configuration.opened_at.value
                : 0u;
            limits.maximum_proposal_lifetime = (std::max)(
                std::uint64_t{1u},
                (std::min)(session_lifetime, std::uint64_t{24u * 60u * 60u}));
            limits.maximum_approval_lifetime = (std::max)(
                std::uint64_t{1u},
                (std::min)(session_lifetime, std::uint64_t{60u * 60u}));
            limits.maximum_permit_lifetime = (std::max)(
                std::uint64_t{1u},
                (std::min)(session_lifetime, std::uint64_t{5u * 60u}));

            return {
                .identity = {
                    configuration.session_value,
                    configuration.session_generation},
                .workspace_id = configuration.workspace_id,
                .opened_at = configuration.opened_at.value,
                .expires_at = configuration.expires_at.value,
                .limits = limits,
                .workspace_allowlist = {
                    {guard::WorkspaceArea::engine_source,
                        configuration.engine_source_root,
                        readable_writable()},
                    {guard::WorkspaceArea::project_source,
                        configuration.project_source_root,
                        readable_writable()},
                    {guard::WorkspaceArea::build_output,
                        configuration.build_output_root,
                        build_permissions()},
                    {guard::WorkspaceArea::evidence,
                        configuration.evidence_root,
                        readable_writable()}},
                .approved_operator_ids = {configuration.operator_id}};
        }

        [[nodiscard]] std::string digest_hex(
            const guard::ProposalDigest& digest)
        {
            static constexpr std::array<char, 16u> digits{
                '0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
            std::string result{};
            result.resize(digest.bytes.size() * 2u);
            for (std::size_t index = 0u; index < digest.bytes.size(); ++index)
            {
                const std::uint8_t byte = digest.bytes[index];
                result[index * 2u] = digits[byte >> 4u];
                result[index * 2u + 1u] = digits[byte & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] bool text_is_usable(std::string_view text) noexcept
        {
            if (text.empty() || text.size() > maximum_text_bytes)
                return false;
            return std::none_of(text.begin(), text.end(), [](char value)
            {
                const unsigned char byte = static_cast<unsigned char>(value);
                return byte == 0u || (byte < 0x20u && value != '\n'
                    && value != '\r' && value != '\t');
            });
        }

        [[nodiscard]] guard::EvidenceCategory guard_evidence_kind(
            EvidenceKind kind) noexcept
        {
            switch (kind)
            {
            case EvidenceKind::patch: return guard::EvidenceCategory::patch;
            case EvidenceKind::build_log: return guard::EvidenceCategory::build_log;
            case EvidenceKind::runtime_report:
                return guard::EvidenceCategory::runtime_report;
            case EvidenceKind::diagnostic: return guard::EvidenceCategory::diagnostic;
            }
            return guard::EvidenceCategory::diagnostic;
        }

        [[nodiscard]] std::vector<EvidenceKind> evidence_kinds(
            guard::EvidenceRequirement requirements)
        {
            std::vector<EvidenceKind> result{};
            if (guard::has_evidence(requirements, guard::EvidenceRequirement::patch))
                result.push_back(EvidenceKind::patch);
            if (guard::has_evidence(requirements, guard::EvidenceRequirement::build_log))
                result.push_back(EvidenceKind::build_log);
            if (guard::has_evidence(requirements, guard::EvidenceRequirement::runtime_report))
                result.push_back(EvidenceKind::runtime_report);
            if (guard::has_evidence(requirements, guard::EvidenceRequirement::diagnostic))
                result.push_back(EvidenceKind::diagnostic);
            return result;
        }

        [[nodiscard]] bool valid_content_state(
            const ContentState& state) noexcept
        {
            return state.exists
                ? state.digest.valid()
                : !state.digest.valid() && state.byte_count == 0u;
        }

        [[nodiscard]] bool same_content_state(
            const ContentState& left,
            const ContentState& right) noexcept
        {
            if (left.exists != right.exists
                || left.byte_count != right.byte_count)
            {
                return false;
            }
            for (std::size_t index = 0u;
                 index < left.digest.bytes.size();
                 ++index)
            {
                if (left.digest.bytes[index] != right.digest.bytes[index])
                    return false;
            }
            return true;
        }

        [[nodiscard]] guard::ContentState guard_content_state(
            const ContentState& state) noexcept
        {
            guard::ProposalDigest digest{};
            digest.bytes = state.digest.bytes;
            return {
                .kind = state.exists
                    ? guard::ContentStateKind::sha256
                    : guard::ContentStateKind::absent,
                .digest = digest,
                .byte_count = state.byte_count};
        }

        [[nodiscard]] guard::WorkspacePermission write_permission(
            const ContentState& before,
            const ContentState& after) noexcept
        {
            if (!before.exists)
                return guard::WorkspacePermission::create;
            if (!after.exists)
                return guard::WorkspacePermission::remove;
            return guard::WorkspacePermission::write;
        }


        struct FileSnapshot final
        {
            std::optional<ContentState> state{};
            std::string status{};
            std::string bytes{};

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return state.has_value();
            }
        };

        [[nodiscard]] bool source_operation_kind(OperationKind kind) noexcept
        {
            return kind == OperationKind::engine_source_edit
                || kind == OperationKind::project_source_edit;
        }

        [[nodiscard]] constexpr bool source_name_character(
            char value,
            bool allowUnderscore) noexcept
        {
            return (value >= 'a' && value <= 'z')
                || (value >= '0' && value <= '9')
                || (allowUnderscore && value == '_');
        }

        [[nodiscard]] bool valid_first_party_cpp_filename(
            std::string_view path) noexcept
        {
            const std::size_t slash = path.find_last_of('/');
            const std::string_view filename = slash == std::string_view::npos
                ? path : path.substr(slash + 1u);
            const std::size_t extensionDot = filename.find_last_of('.');
            if (extensionDot == std::string_view::npos
                || extensionDot == 0u
                || extensionDot + 1u == filename.size())
            {
                return false;
            }

            const std::string_view stem = filename.substr(0u, extensionDot);
            const std::size_t ownershipDot = stem.find('.');
            if (ownershipDot == std::string_view::npos
                || ownershipDot == 0u
                || ownershipDot + 1u == stem.size()
                || stem.find('.', ownershipDot + 1u)
                    != std::string_view::npos)
            {
                return false;
            }

            const std::string_view owner = stem.substr(0u, ownershipDot);
            const std::string_view subject = stem.substr(ownershipDot + 1u);
            return std::all_of(owner.begin(), owner.end(), [](char value)
                {
                    return source_name_character(value, false);
                })
                && std::all_of(subject.begin(), subject.end(), [](char value)
                {
                    return source_name_character(value, true);
                })
                && subject.front() != '_'
                && subject.back() != '_';
        }

        [[nodiscard]] std::optional<std::string>
            validate_model_source_path(
                OperationKind kind,
                std::string_view path)
        {
            if (kind != OperationKind::engine_source_edit)
                return std::nullopt;

            static constexpr std::array legacyRoots{
                std::string_view{"Engine/Source/"},
                std::string_view{"Engine/Include/"},
                std::string_view{"Engine/Modules/"}};
            if (std::any_of(
                    legacyRoots.begin(),
                    legacyRoots.end(),
                    [path](std::string_view root)
                    {
                        return path.starts_with(root);
                    }))
            {
                return "Engine source proposal rejected: legacy capitalized "
                    "source roots are not part of Epoch. Inspect Engine/src, "
                    "Engine/modules, and Engine/include before proposing code.";
            }

            const bool firstPartyRoot =
                path.starts_with("Engine/src/")
                || path.starts_with("Engine/modules/")
                || path.starts_with("Engine/include/");
            const bool cppSource = path.ends_with(".cpp")
                || path.ends_with(".hpp")
                || path.ends_with(".h")
                || path.ends_with(".inl")
                || path.ends_with(".ixx");
            if (firstPartyRoot && cppSource
                && !valid_first_party_cpp_filename(path))
            {
                return "Engine source proposal rejected: first-party C++ "
                    "filenames require <owner>.<subject_role> with lowercase "
                    "owner and underscore-separated subject words.";
            }
            if (path.ends_with(".ixx")
                && !path.starts_with("Engine/modules/"))
            {
                return "Engine source proposal rejected: C++23 module "
                    "interfaces belong under Engine/modules.";
            }
            return std::nullopt;
        }

        [[nodiscard]] FileSnapshot snapshot_source_file(
            const SessionConfiguration& configuration,
            std::string_view relativePath,
            std::uint64_t maximumBytes)
        {
            const std::string_view configuredRoot =
                configuration.source_snapshot_root.empty()
                ? std::string_view{configuration.workspace_root}
                : std::string_view{configuration.source_snapshot_root};
            if (configuredRoot.empty())
                return {{}, "Source proposal ingestion requires a source snapshot root."};

            const std::filesystem::path root =
                std::filesystem::path{configuredRoot}.lexically_normal();
            if (!root.is_absolute())
                return {{}, "Source proposal snapshot root must be absolute."};

            const auto canonical = guard::canonical_relative_path(relativePath);
            if (!canonical || *canonical != relativePath)
                return {{}, "Source proposal path is not canonical."};

            std::error_code error{};
            const std::filesystem::file_status rootStatus =
                std::filesystem::symlink_status(root, error);
            if (error || std::filesystem::is_symlink(rootStatus)
                || !std::filesystem::is_directory(rootStatus))
            {
                return {{}, "Source proposal snapshot root is unavailable or unsafe."};
            }

            std::filesystem::path current = root;
            const std::filesystem::path relative{*canonical};
            for (auto component = relative.begin(); component != relative.end(); ++component)
            {
                current /= *component;
                const bool finalComponent = std::next(component) == relative.end();
                error.clear();
                const std::filesystem::file_status status =
                    std::filesystem::symlink_status(current, error);
                if (error)
                {
                    if (error == std::errc::no_such_file_or_directory
                        && finalComponent)
                    {
                        return {ContentState{}, {}};
                    }
                    return {{}, "Source proposal path could not be inspected safely."};
                }
                if (status.type() == std::filesystem::file_type::not_found)
                {
                    if (finalComponent)
                        return {ContentState{}, {}};
                    return {{}, "Source proposal parent directory does not exist."};
                }
                if (std::filesystem::is_symlink(status))
                    return {{}, "Source proposal path crosses a symbolic link."};
                if (!finalComponent && !std::filesystem::is_directory(status))
                    return {{}, "Source proposal path crosses a non-directory component."};
                if (finalComponent && !std::filesystem::is_regular_file(status))
                    return {{}, "Source proposal target must be a regular file or absent."};
            }

            error.clear();
            const std::uintmax_t size = std::filesystem::file_size(current, error);
            if (error || size > maximumBytes)
                return {{}, "Source proposal preimage exceeds the bounded file size."};

            std::ifstream input{current, std::ios::binary};
            if (!input)
                return {{}, "Source proposal preimage could not be opened."};
            std::string bytes(static_cast<std::size_t>(size), '\0');
            if (!bytes.empty())
            {
                input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                if (!input || input.gcount()
                        != static_cast<std::streamsize>(bytes.size()))
                {
                    return {{}, "Source proposal preimage could not be read exactly."};
                }
            }
            return {
                ContentState{
                    .exists = true,
                    .digest = sha256_content(bytes),
                    .byte_count = static_cast<std::uint64_t>(bytes.size())},
                {},
                std::move(bytes)};
        }

        [[nodiscard]] bool prepare_sandbox_preimage(
            const SessionConfiguration& configuration,
            std::string_view relativePath,
            const FileSnapshot& snapshot,
            std::string& status)
        {
            status.clear();
            if (!snapshot.state || configuration.workspace_root.empty())
            {
                status = "Source sandbox preparation requires an exact snapshot and writable root.";
                return false;
            }

            const std::filesystem::path sandboxRoot =
                std::filesystem::path{configuration.workspace_root}.lexically_normal();
            const std::string_view configuredSourceRoot =
                configuration.source_snapshot_root.empty()
                ? std::string_view{configuration.workspace_root}
                : std::string_view{configuration.source_snapshot_root};
            const std::filesystem::path sourceRoot =
                std::filesystem::path{configuredSourceRoot}.lexically_normal();
            if (!sandboxRoot.is_absolute())
            {
                status = "Source sandbox root must be absolute.";
                return false;
            }
            if (sandboxRoot == sourceRoot)
                return true;

            const auto canonical = guard::canonical_relative_path(relativePath);
            if (!canonical || *canonical != relativePath)
            {
                status = "Source sandbox path is not canonical.";
                return false;
            }

            std::error_code error{};
            const std::filesystem::file_status rootStatus =
                std::filesystem::symlink_status(sandboxRoot, error);
            if (error || std::filesystem::is_symlink(rootStatus)
                || !std::filesystem::is_directory(rootStatus))
            {
                status = "Source sandbox root is unavailable or unsafe.";
                return false;
            }

            const std::filesystem::path relative{*canonical};
            std::filesystem::path parent = sandboxRoot;
            for (const auto& component : relative.parent_path())
            {
                parent /= component;
                error.clear();
                auto entry = std::filesystem::symlink_status(parent, error);
                if ((error == std::errc::no_such_file_or_directory)
                    || (!error && entry.type()
                        == std::filesystem::file_type::not_found))
                {
                    error.clear();
                    if (!std::filesystem::create_directory(parent, error)
                        || error)
                    {
                        status = "Source sandbox parent directory could not be created.";
                        return false;
                    }
                    entry = std::filesystem::symlink_status(parent, error);
                }
                if (error || std::filesystem::is_symlink(entry)
                    || !std::filesystem::is_directory(entry))
                {
                    status = "Source sandbox parent path is unsafe.";
                    return false;
                }
            }

            const std::filesystem::path destination = sandboxRoot / relative;
            error.clear();
            const auto destinationStatus =
                std::filesystem::symlink_status(destination, error);
            const bool destinationMissing =
                error == std::errc::no_such_file_or_directory
                || (!error && destinationStatus.type()
                    == std::filesystem::file_type::not_found);
            if (!snapshot.state->exists)
            {
                if (!destinationMissing)
                {
                    status = "An absent source preimage already exists in the sandbox.";
                    return false;
                }
                return true;
            }
            if (!destinationMissing
                && (error || std::filesystem::is_symlink(destinationStatus)
                    || !std::filesystem::is_regular_file(destinationStatus)))
            {
                status = "Source sandbox destination is not a regular file.";
                return false;
            }
            if (destinationMissing)
            {
                std::ofstream output{destination, std::ios::binary | std::ios::trunc};
                output.write(snapshot.bytes.data(),
                    static_cast<std::streamsize>(snapshot.bytes.size()));
                if (!output)
                {
                    status = "Source preimage could not be materialized in the sandbox.";
                    return false;
                }
            }

            std::ifstream input{destination, std::ios::binary};
            if (!input.is_open())
            {
                status = "Source sandbox preimage could not be reopened for verification.";
                return false;
            }
            const std::string actual{
                std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{}};
            if (input.bad() || actual != snapshot.bytes
                || sha256_content(actual) != snapshot.state->digest)
            {
                status = "Source sandbox preimage verification failed.";
                return false;
            }
            return true;
        }

        [[nodiscard]] std::optional<guard::DevelopmentOperation> make_operation(
            const OperationRequest& request,
            std::uint64_t stable_id,
            const SessionConfiguration& configuration)
        {
            if (!text_is_usable(request.summary)
                || !valid_content_state(request.before)
                || !valid_content_state(request.after))
            {
                return std::nullopt;
            }

            const bool writes_source =
                request.kind == OperationKind::engine_source_edit
                || request.kind == OperationKind::project_source_edit;
            if (writes_source == same_content_state(
                    request.before, request.after))
            {
                return std::nullopt;
            }

            guard::DevelopmentOperation result{};
            result.stable_id = stable_id;
            result.summary = request.summary;
            switch (request.kind)
            {
            case OperationKind::build:
                result.category = guard::OperationCategory::build;
                result.risk = guard::RiskCategory::child_process;
                result.workspace_intents = {
                    {guard::WorkspaceArea::engine_source,
                        configuration.engine_source_root,
                        guard::WorkspacePermission::read},
                    {guard::WorkspaceArea::project_source,
                        configuration.project_source_root,
                        guard::WorkspacePermission::read},
                    {guard::WorkspaceArea::build_output,
                        request.relative_path.empty()
                            ? configuration.build_output_root
                            : request.relative_path,
                        guard::WorkspacePermission::write
                            | guard::WorkspacePermission::create}};
                break;
            case OperationKind::run:
                if (request.relative_path.empty())
                    return std::nullopt;
                result.category = guard::OperationCategory::run;
                result.risk = guard::RiskCategory::runtime_execution;
                result.workspace_intents = {{
                    guard::WorkspaceArea::build_output,
                    request.relative_path,
                    guard::WorkspacePermission::read
                        | guard::WorkspacePermission::execute}};
                break;
            case OperationKind::engine_source_edit:
            case OperationKind::project_source_edit:
                if (request.relative_path.empty())
                    return std::nullopt;
                result.category =
                    request.kind == OperationKind::engine_source_edit
                    ? guard::OperationCategory::source_write
                    : guard::OperationCategory::project_write;
                result.risk =
                    request.kind == OperationKind::engine_source_edit
                    ? guard::RiskCategory::engine_source_write
                    : guard::RiskCategory::workspace_write;
                const guard::WorkspaceArea area =
                    request.kind == OperationKind::engine_source_edit
                    ? guard::WorkspaceArea::engine_source
                    : guard::WorkspaceArea::project_source;
                result.workspace_intents = {{
                    area,
                    request.relative_path,
                    write_permission(request.before, request.after)}};
                result.content_transitions = {{
                    .relative_path = request.relative_path,
                    .before = guard_content_state(request.before),
                    .after = guard_content_state(request.after)}};
                break;
            }
            return result;
        }

        [[nodiscard]] std::string guard_status(
            std::string_view action,
            guard::GuardCode code)
        {
            std::string status{action};
            status += ": ";
            status += guard::to_string(code);
            return status;
        }

        [[nodiscard]] ControllerResult invalid_phase(
            ControllerPhase actual,
            std::string_view required)
        {
            std::string status{"Phase is "};
            status += to_string(actual);
            status += "; ";
            status += required;
            status += " required.";
            return {ControllerCode::invalid_phase, std::move(status)};
        }
    }

    PromotionCandidateResult verify_source_promotion_candidate(
        std::string_view liveSourceRoot,
        std::string_view sandboxRoot,
        const std::vector<OperationSummary>& operations)
    {
        if (liveSourceRoot.empty() || sandboxRoot.empty())
        {
            return {
                PromotionCandidateCode::invalid_root,
                "Live promotion requires distinct absolute live and sandbox roots."};
        }

        std::error_code error{};
        const std::filesystem::path requestedLive =
            std::filesystem::path{liveSourceRoot}.lexically_normal();
        const std::filesystem::path requestedSandbox =
            std::filesystem::path{sandboxRoot}.lexically_normal();
        if (!requestedLive.is_absolute() || !requestedSandbox.is_absolute())
        {
            return {
                PromotionCandidateCode::invalid_root,
                "Live promotion roots must be absolute."};
        }
        const std::filesystem::path live =
            std::filesystem::weakly_canonical(requestedLive, error);
        if (error)
        {
            return {
                PromotionCandidateCode::invalid_root,
                "The live source root could not be canonicalized."};
        }
        error.clear();
        const std::filesystem::path sandbox =
            std::filesystem::weakly_canonical(requestedSandbox, error);
        if (error || live == sandbox)
        {
            return {
                PromotionCandidateCode::invalid_root,
                "Live promotion requires distinct canonical live and sandbox roots."};
        }
        if (operations.empty() || operations.size() > 12u)
        {
            return {
                PromotionCandidateCode::invalid_operations,
                "Live promotion requires one bounded reviewed source operation set."};
        }

        SessionConfiguration liveConfiguration{};
        liveConfiguration.workspace_root = live.generic_string();
        liveConfiguration.source_snapshot_root = live.generic_string();
        SessionConfiguration sandboxConfiguration{};
        sandboxConfiguration.workspace_root = sandbox.generic_string();
        sandboxConfiguration.source_snapshot_root = sandbox.generic_string();
        for (const OperationSummary& operation : operations)
        {
            if ((operation.kind != OperationKind::engine_source_edit
                    && operation.kind != OperationKind::project_source_edit)
                || operation.relative_path.empty()
                || !operation.before.exists
                || !operation.after.exists
                || !operation.before.digest.valid()
                || !operation.after.digest.valid())
            {
                return {
                    PromotionCandidateCode::invalid_operations,
                    "Only exact existing-file source transitions can be promoted."};
            }

            const FileSnapshot liveSnapshot = snapshot_source_file(
                liveConfiguration,
                operation.relative_path,
                16u * 1024u * 1024u);
            if (!liveSnapshot.state)
            {
                return {
                    PromotionCandidateCode::unsafe_path,
                    liveSnapshot.status.empty()
                        ? std::string{"The live source path is unsafe."}
                        : liveSnapshot.status};
            }
            if (*liveSnapshot.state != operation.before)
            {
                return {
                    PromotionCandidateCode::live_preimage_mismatch,
                    "Live source changed after review; the verified candidate cannot be promoted."};
            }

            const FileSnapshot sandboxSnapshot = snapshot_source_file(
                sandboxConfiguration,
                operation.relative_path,
                16u * 1024u * 1024u);
            if (!sandboxSnapshot.state)
            {
                return {
                    PromotionCandidateCode::unsafe_path,
                    sandboxSnapshot.status.empty()
                        ? std::string{"The sandbox source path is unsafe."}
                        : sandboxSnapshot.status};
            }
            if (*sandboxSnapshot.state != operation.after)
            {
                return {
                    PromotionCandidateCode::sandbox_postimage_mismatch,
                    "The sandbox no longer contains the compiler-tested exact postimage."};
            }
        }
        return {
            PromotionCandidateCode::ready,
            "Live preimages and compiler-tested sandbox postimages match the reviewed operation set."};
    }

    EvidenceDigest evidence_digest(std::string_view content) noexcept
    {
        return sha256_content(content);
    }

    struct DevelopmentController::Impl final
    {
        explicit Impl(SessionConfiguration source_configuration)
            : configuration(std::move(source_configuration)),
              guard_policy(make_policy(configuration)),
              development_guard(guard_policy)
        {
            const guard::GuardResult validation =
                guard::DevelopmentGuard::validate_policy(guard_policy);
            if (validation)
            {
                phase = ControllerPhase::ready;
                last_code = ControllerCode::none;
                status = "Guarded AI development session ready.";
            }
            else
            {
                phase = ControllerPhase::unavailable;
                last_code = ControllerCode::invalid_configuration;
                status = guard_status("Invalid controller configuration", validation.code);
            }
        }

        [[nodiscard]] ControllerResult remember(ControllerResult result)
        {
            last_code = result.code;
            status = result.status;
            return result;
        }

        [[nodiscard]] std::optional<LogicalTime> resolve_time(
            LogicalTime requested) noexcept
        {
            if (configuration.clock_policy
                == ClockPolicy::externally_driven_contract)
            {
                if (!configuration.workspace_id.starts_with(
                        "epoch.editor.contract")
                    || requested.value < configuration.opened_at.value
                    || requested.value < last_external_time)
                {
                    return std::nullopt;
                }
                last_external_time = requested.value;
                return requested;
            }

            const auto elapsed = std::chrono::duration_cast<
                std::chrono::seconds>(
                    std::chrono::steady_clock::now() - steady_origin).count();
            if (elapsed < 0
                || static_cast<std::uint64_t>(elapsed)
                    > (std::numeric_limits<std::uint64_t>::max)()
                        - configuration.opened_at.value)
            {
                return std::nullopt;
            }
            return LogicalTime{
                configuration.opened_at.value
                    + static_cast<std::uint64_t>(elapsed)};
        }

        SessionConfiguration configuration{};
        guard::SessionPolicy guard_policy{};
        guard::DevelopmentGuard development_guard;
        ControllerPhase phase{ControllerPhase::unavailable};
        ControllerCode last_code{ControllerCode::invalid_configuration};
        std::string status{};
        std::optional<guard::ProposalReceipt> proposal{};
        std::optional<guard::ExecutionPermit> permit{};
        std::vector<OperationSummary> operation_summaries{};
        std::vector<SourceExecutionPayload> model_source_payloads{};
        std::chrono::steady_clock::time_point steady_origin{
            std::chrono::steady_clock::now()};
        std::uint64_t last_external_time{};
        std::recursive_mutex execution_mutex{};
        bool execution_claimed{};
        bool trusted_source_completion{};
    };

    DevelopmentController::DevelopmentController(SessionConfiguration configuration)
        : impl_(std::make_unique<Impl>(std::move(configuration)))
    {
    }

    DevelopmentController::~DevelopmentController() = default;
    DevelopmentController::DevelopmentController(DevelopmentController&&) noexcept = default;
    DevelopmentController& DevelopmentController::operator=(DevelopmentController&&) noexcept = default;

    ControllerResult DevelopmentController::propose(
        ProposalRequest request,
        LogicalTime now)
    {
        if (!impl_)
            return {ControllerCode::invalid_configuration, "Controller has no session."};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
        }
        now = *trusted_time;
        if (impl_->phase != ControllerPhase::ready)
            return impl_->remember(invalid_phase(impl_->phase, "ready phase"));
        if (!text_is_usable(request.title) || !text_is_usable(request.rationale)
            || request.operations.empty() || request.operations.size() > maximum_operations
            || request.expires_at.value <= now.value)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Proposal title, rationale, operations, or lifetime is invalid."});
        }

        std::vector<guard::DevelopmentOperation> operations{};
        std::vector<OperationSummary> summaries{};
        operations.reserve(request.operations.size());
        summaries.reserve(request.operations.size());
        for (std::size_t index = 0u; index < request.operations.size(); ++index)
        {
            const OperationRequest& source = request.operations[index];
            auto operation = make_operation(
                source, static_cast<std::uint64_t>(index + 1u), impl_->configuration);
            if (!operation)
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    "Proposal contains an invalid operation."});
            }
            summaries.push_back({
                .stable_id = operation->stable_id,
                .kind = source.kind,
                .summary = source.summary,
                .relative_path = source.relative_path,
                .before = source.before,
                .after = source.after,
                .writes_source = source.kind == OperationKind::engine_source_edit
                    || source.kind == OperationKind::project_source_edit,
                .starts_process = source.kind == OperationKind::build
                    || source.kind == OperationKind::run});
            operations.push_back(std::move(*operation));
        }

        const guard::ProposalReceipt receipt = impl_->development_guard.submit({
            .session = impl_->guard_policy.identity,
            .title = std::move(request.title),
            .rationale = std::move(request.rationale),
            .created_at = now.value,
            .expires_at = request.expires_at.value,
            .operations = std::move(operations)}, now.value);
        if (!receipt)
        {
            return impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Proposal rejected", receipt.code)});
        }

        impl_->proposal = receipt;
        impl_->operation_summaries = std::move(summaries);
        impl_->phase = ControllerPhase::proposed;
        return impl_->remember({
            ControllerCode::none,
            "Immutable proposal staged for review."});
    }


    ControllerResult DevelopmentController::propose_model_reply(
        std::string_view raw_reply,
        OperationKind expected_source_kind,
        LogicalTime now)
    {
        if (!impl_)
            return {ControllerCode::invalid_configuration, "Controller has no session."};
        if (impl_->phase != ControllerPhase::ready)
            return impl_->remember(invalid_phase(impl_->phase, "ready phase"));
        if (!source_operation_kind(expected_source_kind))
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Model proposal ingestion accepts source-edit domains only."});
        }

        const auto decoded =
            ai::development_proposal_codec::decode(raw_reply);
        if (!decoded)
        {
            std::string status{"Model proposal rejected at line "};
            status += std::to_string(decoded.line);
            status += " (";
            status += ai::development_proposal_codec::code_name(decoded.code);
            status += "): ";
            status += decoded.status;
            return impl_->remember({
                ControllerCode::invalid_request,
                std::move(status)});
        }

        const auto expectedArea =
            expected_source_kind == OperationKind::engine_source_edit
            ? ai::development_proposal_codec::SourceArea::engine
            : ai::development_proposal_codec::SourceArea::project;
        if (now.value > (std::numeric_limits<std::uint64_t>::max)()
                - decoded.proposal.lifetime_seconds)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Model proposal lifetime overflows the logical clock."});
        }

        ProposalRequest request{};
        request.title = decoded.proposal.title;
        request.rationale = decoded.proposal.rationale;
        request.expires_at = {
            now.value + decoded.proposal.lifetime_seconds};
        request.operations.reserve(decoded.proposal.changes.size());

        std::vector<SourceExecutionPayload> payloads{};
        payloads.reserve(decoded.proposal.changes.size());
        for (std::size_t index = 0u;
             index < decoded.proposal.changes.size(); ++index)
        {
            const auto& change = decoded.proposal.changes[index];
            if (change.area != expectedArea)
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    "Model proposal mixes source areas outside the selected workspace."});
            }

            if (const auto pathStatus = validate_model_source_path(
                    expected_source_kind,
                    change.relative_path))
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    *pathStatus});
            }

            const FileSnapshot before = snapshot_source_file(
                impl_->configuration,
                change.relative_path,
                16u * 1024u * 1024u);
            if (!before)
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    before.status});
            }
            std::string postimage = change.replacement_bytes;
            if (change.edit_kind
                == ai::development_proposal_codec::SourceEditKind::
                    replace_exact_block)
            {
                if (!before.state->exists)
                {
                    return impl_->remember({
                        ControllerCode::invalid_request,
                        "Exact-block source edits require an existing preimage."});
                }
                const std::size_t first =
                    before.bytes.find(change.match_bytes);
                const std::size_t second =
                    first == std::string::npos
                    ? std::string::npos
                    : before.bytes.find(
                        change.match_bytes,
                        first + change.match_bytes.size());
                if (change.match_bytes.empty()
                    || first == std::string::npos
                    || second != std::string::npos)
                {
                    return impl_->remember({
                        ControllerCode::invalid_request,
                        "Exact-block source edit rejected because its search "
                        "bytes are absent or ambiguous in the host-captured preimage."});
                }
                postimage = before.bytes;
                postimage.replace(
                    first,
                    change.match_bytes.size(),
                    change.replacement_bytes);
            }
            if (postimage.size() > 16u * 1024u * 1024u)
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    "Source proposal postimage exceeds the bounded file size."});
            }
            std::string sandboxStatus{};
            if (!prepare_sandbox_preimage(
                    impl_->configuration,
                    change.relative_path,
                    before,
                    sandboxStatus))
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    std::move(sandboxStatus)});
            }

            const ContentState after{
                .exists = true,
                .digest = sha256_content(postimage),
                .byte_count = static_cast<std::uint64_t>(
                    postimage.size())};
            request.operations.push_back({
                .kind = expected_source_kind,
                .summary = change.summary,
                .relative_path = change.relative_path,
                .before = *before.state,
                .after = after});
            payloads.push_back({
                .stable_operation_id =
                    static_cast<std::uint64_t>(index + 1u),
                .relative_path = change.relative_path,
                .replacement_bytes = std::move(postimage)});
        }

        const ControllerResult proposed = propose(std::move(request), now);
        if (!proposed)
            return proposed;
        impl_->model_source_payloads = std::move(payloads);
        return impl_->remember({
            ControllerCode::none,
            "Bounded model source proposal staged with host-captured preimages."});
    }

    ControllerResult DevelopmentController::review(
        std::string reviewer_id,
        std::string note,
        LogicalTime now)
    {
        if (!impl_ || !impl_->proposal)
            return {ControllerCode::invalid_phase, "No staged proposal exists."};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
        }
        now = *trusted_time;
        if (impl_->phase != ControllerPhase::proposed)
            return impl_->remember(invalid_phase(impl_->phase, "proposed phase"));
        if (!text_is_usable(reviewer_id) || note.size() > maximum_text_bytes)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Reviewer identity or note is invalid."});
        }
        const guard::GuardResult result = impl_->development_guard.review({
            .session = impl_->guard_policy.identity,
            .proposal = impl_->proposal->proposal,
            .proposal_digest = impl_->proposal->digest,
            .reviewer_id = std::move(reviewer_id),
            .disposition = guard::ReviewDisposition::accept,
            .note = std::move(note)}, now.value);
        if (!result)
        {
            return impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Review rejected", result.code)});
        }
        impl_->phase = ControllerPhase::reviewed;
        return impl_->remember({ControllerCode::none, "Proposal review recorded."});
    }

    ControllerResult DevelopmentController::approve(
        std::string operator_id,
        std::string note,
        LogicalTime now,
        LogicalTime expires_at)
    {
        if (!impl_ || !impl_->proposal)
            return {ControllerCode::invalid_phase, "No reviewed proposal exists."};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
        }
        now = *trusted_time;
        if (impl_->phase != ControllerPhase::reviewed)
            return impl_->remember(invalid_phase(impl_->phase, "reviewed phase"));
        if (operator_id != impl_->configuration.operator_id
            || !text_is_usable(operator_id) || note.size() > maximum_text_bytes
            || expires_at.value <= now.value)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Exact configured operator approval and a valid lifetime are required."});
        }
        const guard::GuardResult result = impl_->development_guard.approve({
            .session = impl_->guard_policy.identity,
            .proposal = impl_->proposal->proposal,
            .proposal_digest = impl_->proposal->digest,
            .operator_id = std::move(operator_id),
            .disposition = guard::ApprovalDisposition::approve,
            .expires_at = expires_at.value,
            .note = std::move(note)}, now.value);
        if (!result)
        {
            return impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Approval rejected", result.code)});
        }
        impl_->phase = ControllerPhase::approved;
        return impl_->remember({
            ControllerCode::none,
            "Exact proposal digest approved by the configured operator."});
    }

    ControllerResult DevelopmentController::authorize(
        LogicalTime now,
        Duration permit_lifetime)
    {
        if (!impl_ || !impl_->proposal)
            return {ControllerCode::invalid_phase, "No approved proposal exists."};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
        }
        now = *trusted_time;
        if (impl_->phase != ControllerPhase::approved)
            return impl_->remember(invalid_phase(impl_->phase, "approved phase"));
        if (permit_lifetime.value == 0u)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Permit lifetime must be non-zero."});
        }
        const guard::PermitReceipt receipt =
            impl_->development_guard.issue_execution_permit(
                impl_->guard_policy.identity,
                impl_->proposal->proposal,
                impl_->proposal->digest,
                now.value,
                permit_lifetime.value);
        if (!receipt)
        {
            return impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Authorization rejected", receipt.code)});
        }
        impl_->permit = receipt.permit;
        impl_->phase = ControllerPhase::authorized;
        return impl_->remember({
            ControllerCode::none,
            "Single-use execution permit issued; host execution may now begin."});
    }

    SourceExecutionReport
    DevelopmentController::execute_authorized_source_changes(
        std::vector<SourceExecutionPayload> payloads,
        LogicalTime now)
    {
        SourceExecutionReport report{};
        if (!impl_ || !impl_->proposal || !impl_->permit)
        {
            report.controller_result = {
                ControllerCode::invalid_phase,
                "No authorized source transaction exists."};
            return report;
        }
        std::scoped_lock execution_lock{impl_->execution_mutex};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            report.controller_result = impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
            return report;
        }
        now = *trusted_time;
        if (impl_->phase != ControllerPhase::authorized)
        {
            report.controller_result = impl_->remember(
                invalid_phase(impl_->phase, "authorized phase"));
            return report;
        }
        if (now.value < impl_->permit->issued_time()
            || now.value > impl_->permit->expiry()
            || now.value > impl_->configuration.expires_at.value)
        {
            report.controller_result = impl_->remember({
                ControllerCode::guard_rejected,
                "The single-use source transaction permit is not active."});
            return report;
        }
        if (impl_->configuration.workspace_root.empty())
        {
            report.controller_result = impl_->remember({
                ControllerCode::invalid_configuration,
                "Source execution requires an absolute configured workspace root."});
            return report;
        }

        const auto guarded = impl_->development_guard.snapshot(
            impl_->proposal->proposal);
        if (!guarded
            || guarded->digest != impl_->proposal->digest
            || guarded->operations.empty()
            || guarded->operations.size() != payloads.size())
        {
            report.controller_result = impl_->remember({
                ControllerCode::invalid_request,
                "Payload count does not match the exact authorized source operations."});
            return report;
        }

        std::vector<ai::development_executor::SourceChange> changes{};
        changes.reserve(guarded->operations.size());
        for (const guard::DevelopmentOperation& operation :
             guarded->operations)
        {
            if ((operation.category
                    != guard::OperationCategory::source_write
                    && operation.category
                    != guard::OperationCategory::project_write)
                || operation.content_transitions.size() != 1u)
            {
                report.controller_result = impl_->remember({
                    ControllerCode::invalid_request,
                    "Source execution accepts source-only proposals with one exact transition per operation."});
                return report;
            }

            const guard::ContentTransition& transition =
                operation.content_transitions.front();
            const auto payload = std::find_if(
                payloads.begin(),
                payloads.end(),
                [&](const SourceExecutionPayload& candidate)
                {
                    return candidate.stable_operation_id
                            == operation.stable_id
                        && guard::canonical_relative_path(
                            candidate.relative_path)
                            == std::optional<std::string>{
                                transition.relative_path};
                });
            if (payload == payloads.end())
            {
                report.controller_result = impl_->remember({
                    ControllerCode::invalid_request,
                    "A replacement payload does not match its authorized operation identity and path."});
                return report;
            }

            const auto map_state = [](const guard::ContentState& state)
            {
                ai::development_executor::ContentDigest digest{};
                digest.bytes = state.digest.bytes;
                return ai::development_executor::ContentState{
                    .exists = state.kind == guard::ContentStateKind::sha256,
                    .digest = digest,
                    .byte_count = state.byte_count};
            };
            changes.push_back({
                .stable_operation_id = operation.stable_id,
                .relative_path = transition.relative_path,
                .before = map_state(transition.before),
                .after = map_state(transition.after),
                .replacement_bytes = payload->replacement_bytes});
        }

        const guard::GuardResult claimed =
            impl_->development_guard.claim_execution_permit(
                *impl_->permit,
                "epoch.source_transaction_executor",
                now.value);
        if (!claimed)
        {
            report.controller_result = impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Source execution claim rejected", claimed.code)});
            return report;
        }
        impl_->execution_claimed = true;

        const ai::development_executor::SourceTransactionExecutor executor{{
            .maximum_changes = maximum_operations,
            .maximum_file_bytes = 16u * 1024u * 1024u,
            .maximum_total_bytes = 64u * 1024u * 1024u,
            .maximum_path_bytes = maximum_text_bytes}};
        auto transaction = executor.execute({
            .workspace_root = impl_->configuration.workspace_root,
            .permit = *impl_->permit,
            .changes = std::move(changes)});

        report.attempted = true;
        report.transaction_code =
            std::string{ai::development_executor::to_string(
                transaction.code)};
        report.transaction_status = transaction.status;
        report.evidence_manifest =
            std::move(transaction.evidence_manifest);
        report.evidence_digest.bytes =
            transaction.evidence_digest.bytes;
        report.committed = transaction.committed;
        report.rollback_attempted =
            transaction.rollback_attempted;
        report.rollback_complete =
            transaction.rollback_complete;

        std::string locator{
            "receipt/guarded_ai_source_transaction/"};
        locator += ai::development_executor::digest_hex(
            transaction.evidence_digest);
        impl_->trusted_source_completion = true;
        report.controller_result = complete(
            CompletionRequest{
                .actor_id = "epoch.source_transaction_executor",
                .outcome = transaction
                    ? CompletionOutcome::succeeded
                    : CompletionOutcome::failed,
                .summary = transaction.status,
                .evidence = {{
                    .kind = transaction
                        ? EvidenceKind::patch
                        : EvidenceKind::diagnostic,
                    .locator = std::move(locator),
                    .content_digest = report.evidence_digest,
                    .summary = transaction.status,
                    .verified = transaction.evidence_digest.valid()}}},
            now);
        impl_->trusted_source_completion = false;
        return report;
    }

    SourceExecutionReport
    DevelopmentController::execute_authorized_model_source_changes(
        LogicalTime now)
    {
        SourceExecutionReport report{};
        if (!impl_ || impl_->model_source_payloads.empty())
        {
            report.controller_result = {
                ControllerCode::invalid_request,
                "No decoded model source payloads are staged."};
            return report;
        }

        report = execute_authorized_source_changes(
            impl_->model_source_payloads, now);
        if (report.attempted
            || (impl_->phase != ControllerPhase::authorized
                && impl_->phase != ControllerPhase::approved
                && impl_->phase != ControllerPhase::reviewed
                && impl_->phase != ControllerPhase::proposed))
        {
            impl_->model_source_payloads.clear();
        }
        return report;
    }

    ControllerResult DevelopmentController::complete(
        CompletionRequest completion,
        LogicalTime now)
    {
        if (!impl_ || !impl_->permit)
            return {ControllerCode::invalid_phase, "No execution permit exists."};
        std::scoped_lock execution_lock{impl_->execution_mutex};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
        }
        now = *trusted_time;
        if (impl_->phase != ControllerPhase::authorized)
            return impl_->remember(invalid_phase(impl_->phase, "authorized phase"));
        const bool source_proposal = std::any_of(
            impl_->operation_summaries.begin(),
            impl_->operation_summaries.end(),
            [](const OperationSummary& operation)
            {
                return operation.writes_source;
            });
        if (source_proposal && !impl_->trusted_source_completion)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Source completion evidence is accepted only from the trusted transaction executor."});
        }
        if (!text_is_usable(completion.actor_id)
            || !text_is_usable(completion.summary)
            || completion.evidence.empty()
            || completion.evidence.size() > maximum_evidence_items)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Completion requires a valid actor, summary, and bounded evidence."});
        }

        std::vector<guard::EvidenceItem> evidence{};
        evidence.reserve(completion.evidence.size());
        for (EvidenceItem& source : completion.evidence)
        {
            if (!text_is_usable(source.locator) || !text_is_usable(source.summary)
                || !source.content_digest.valid() || !source.verified)
            {
                return impl_->remember({
                    ControllerCode::invalid_request,
                    "Completion evidence must be verified and content-addressed."});
            }
            guard::ProposalDigest content_digest{};
            content_digest.bytes = source.content_digest.bytes;
            evidence.push_back({
                .category = guard_evidence_kind(source.kind),
                .locator = std::move(source.locator),
                .content_digest = content_digest,
                .summary = std::move(source.summary),
                .verified = true});
        }

        if (!impl_->trusted_source_completion
            && !impl_->execution_claimed)
        {
            const guard::GuardResult claimed =
                impl_->development_guard.claim_execution_permit(
                    *impl_->permit,
                    completion.actor_id,
                    now.value);
            if (!claimed)
            {
                return impl_->remember({
                    ControllerCode::guard_rejected,
                    guard_status("Execution claim rejected", claimed.code)});
            }
            impl_->execution_claimed = true;
        }

        const bool succeeded = completion.outcome == CompletionOutcome::succeeded;
        const guard::GuardResult result = impl_->development_guard.record_evidence({
            .permit = *impl_->permit,
            .actor_id = std::move(completion.actor_id),
            .outcome = succeeded
                ? guard::ExecutionOutcome::succeeded
                : guard::ExecutionOutcome::failed,
            .summary = std::move(completion.summary),
            .items = std::move(evidence)}, now.value);
        if (!result)
        {
            return impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Completion evidence rejected", result.code)});
        }
        impl_->phase = succeeded ? ControllerPhase::completed : ControllerPhase::failed;
        return impl_->remember({
            ControllerCode::none,
            succeeded
                ? "Verified completion evidence recorded; permit consumed."
                : "Verified failure evidence recorded; permit consumed."});
    }

    ControllerResult DevelopmentController::cancel(
        std::string actor_id,
        std::string reason,
        LogicalTime now)
    {
        if (!impl_ || !impl_->proposal)
            return {ControllerCode::invalid_phase, "No active proposal exists."};
        const auto trusted_time = impl_->resolve_time(now);
        if (!trusted_time)
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Controller time must come from its trusted monotonic clock."});
        }
        now = *trusted_time;
        if (impl_->phase == ControllerPhase::completed
            || impl_->phase == ControllerPhase::failed
            || impl_->phase == ControllerPhase::cancelled)
        {
            return impl_->remember(invalid_phase(impl_->phase, "non-terminal phase"));
        }
        if (!text_is_usable(actor_id) || !text_is_usable(reason))
        {
            return impl_->remember({
                ControllerCode::invalid_request,
                "Cancellation actor and reason are required."});
        }
        const guard::GuardResult result = impl_->development_guard.cancel({
            .session = impl_->guard_policy.identity,
            .proposal = impl_->proposal->proposal,
            .proposal_digest = impl_->proposal->digest,
            .actor_id = std::move(actor_id),
            .reason = std::move(reason)}, now.value);
        if (!result)
        {
            return impl_->remember({
                ControllerCode::guard_rejected,
                guard_status("Cancellation rejected", result.code)});
        }
        impl_->phase = ControllerPhase::cancelled;
        impl_->model_source_payloads.clear();
        return impl_->remember({
            ControllerCode::none,
            "Proposal cancelled and any issued permit invalidated."});
    }

    ControllerSnapshot DevelopmentController::snapshot() const
    {
        if (!impl_)
        {
            return {
                .phase = ControllerPhase::unavailable,
                .last_code = ControllerCode::invalid_configuration,
                .status = "Controller has no session."};
        }

        ControllerSnapshot result{
            .phase = impl_->phase,
            .last_code = impl_->last_code,
            .status = impl_->status,
            .workspace_id = impl_->configuration.workspace_id,
            .operations = impl_->operation_summaries,
            .single_use_permit_issued = impl_->permit.has_value(),
            .permit_consumed = impl_->permit.has_value()
                && (impl_->phase == ControllerPhase::completed
                    || impl_->phase == ControllerPhase::failed
                    || impl_->phase == ControllerPhase::cancelled),
            .model_source_payloads_ready =
                !impl_->model_source_payloads.empty()
                && impl_->phase != ControllerPhase::completed
                && impl_->phase != ControllerPhase::failed
                && impl_->phase != ControllerPhase::cancelled,
            .audit_event_count = impl_->development_guard.audit().size(),
            .evidence_record_count = impl_->development_guard.evidence().size()};

        if (!impl_->proposal)
            return result;
        result.digest_hex = digest_hex(impl_->proposal->digest);
        if (const auto proposal =
                impl_->development_guard.snapshot(impl_->proposal->proposal))
        {
            result.proposal_title = proposal->title;
            result.required_evidence = evidence_kinds(proposal->required_evidence);
            result.operator_approval_required = proposal->requires_operator_approval;
            result.exact_operator_approval_recorded = proposal->approval.has_value()
                && proposal->approval->proposal_digest == proposal->digest
                && proposal->approval->operator_id == impl_->configuration.operator_id
                && proposal->approval->disposition == guard::ApprovalDisposition::approve;
        }
        return result;
    }
}
