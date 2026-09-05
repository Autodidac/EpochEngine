/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

module ai.development_executor;

namespace epochengine::ai::development_executor
{
    namespace
    {
        namespace fs = std::filesystem;

        struct TemporaryWorkspace final
        {
            fs::path root{};

            TemporaryWorkspace()
            {
                const auto stamp = std::chrono::steady_clock::now()
                    .time_since_epoch().count();
                root = fs::temp_directory_path()
                    / ("epoch_ai_executor_contract_"
                        + std::to_string(stamp));
                std::error_code error;
                fs::create_directories(root / "Engine" / "src", error);
                if (error)
                    root.clear();
            }

            ~TemporaryWorkspace()
            {
                if (root.empty())
                    return;
                std::error_code ignored;
                fs::remove_all(root, ignored);
            }
        };

        [[nodiscard]] bool write(
            const fs::path& path,
            std::string_view bytes)
        {
            std::ofstream output(
                path,
                std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                return false;
            output.write(
                bytes.data(),
                static_cast<std::streamsize>(bytes.size()));
            output.flush();
            return output.good();
        }

        [[nodiscard]] std::string read(const fs::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return {};
            return {
                std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{}};
        }

        [[nodiscard]] ContentState present(
            std::string_view bytes) noexcept
        {
            return {
                .exists = true,
                .digest = digest(bytes),
                .byte_count =
                    static_cast<std::uint64_t>(bytes.size())};
        }

        [[nodiscard]] development_guard::ExecutionPermit authorization(
            std::string_view name)
        {
            namespace guard = development_guard;
            const guard::SessionPolicy session{
                .identity = {0x4558454355544f52ull, 1u},
                .workspace_id = "epoch.executor.contract",
                .opened_at = 1u,
                .expires_at = 1'000u,
                .limits = {},
                .workspace_allowlist = {{
                    guard::WorkspaceArea::engine_source,
                    "Engine",
                    guard::WorkspacePermission::read}},
                .approved_operator_ids = {"operator.contract"}};
            guard::DevelopmentGuard development_guard{session};
            const guard::ProposalReceipt proposal = development_guard.submit({
                .session = session.identity,
                .title = "Executor authorization contract",
                .rationale = "Issue a real guard-owned permit for " + std::string{name},
                .created_at = 10u,
                .expires_at = 500u,
                .operations = {{
                    .stable_id = 1u,
                    .category = guard::OperationCategory::inspect,
                    .risk = guard::RiskCategory::read_only,
                    .summary = "Authorize the bounded executor contract.",
                    .workspace_intents = {{
                        guard::WorkspaceArea::engine_source,
                        "Engine",
                        guard::WorkspacePermission::read}}}}},
                20u);
            if (!proposal)
                return {};
            if (!development_guard.review({
                    .session = session.identity,
                    .proposal = proposal.proposal,
                    .proposal_digest = proposal.digest,
                    .reviewer_id = "review.contract",
                    .disposition = guard::ReviewDisposition::accept,
                    .note = "Contract paths and limits reviewed."},
                    30u))
            {
                return {};
            }
            const guard::PermitReceipt permit =
                development_guard.issue_execution_permit(
                    session.identity,
                    proposal.proposal,
                    proposal.digest,
                    40u,
                    50u);
            return permit ? permit.permit : guard::ExecutionPermit{};
        }

        [[nodiscard]] bool no_transaction_artifacts(
            const fs::path& root)
        {
            std::error_code error;
            for (fs::recursive_directory_iterator iterator{
                     root,
                     fs::directory_options::skip_permission_denied,
                     error},
                 end;
                 iterator != end;
                 iterator.increment(error))
            {
                if (error)
                    return false;
                if (iterator->path().filename().generic_string()
                        .find(".epoch_ai_")
                    != std::string::npos)
                {
                    return false;
                }
            }
            return !error;
        }

        [[nodiscard]] int digest_contract()
        {
            if (digest_hex(digest(""))
                != "e3b0c44298fc1c149afbf4c8996fb924"
                   "27ae41e4649b934ca495991b7852b855")
            {
                return 11;
            }
            if (digest_hex(digest("abc"))
                != "ba7816bf8f01cfea414140de5dae2223"
                   "b00361a396177a9cb410ff61f20015ad")
            {
                return 12;
            }
            if (digest("abc") == digest("abd"))
                return 13;
            return 0;
        }

        [[nodiscard]] bool create_update_delete_contract()
        {
            TemporaryWorkspace workspace{};
            if (workspace.root.empty())
                return false;

            const fs::path file =
                workspace.root / "Engine" / "src" / "sample.cpp";
            SourceTransactionExecutor executor{};

            const std::string first{"int value = 1;\n"};
            const TransactionResult created = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("create"),
                .changes = {{
                    .stable_operation_id = 1u,
                    .relative_path = "Engine/src/sample.cpp",
                    .before = {},
                    .after = present(first),
                    .replacement_bytes = first}}});
            if (!created || read(file) != first
                || !created.all_preimages_verified
                || !created.all_temporaries_verified
                || created.files.size() != 1u
                || !created.files.front().committed
                || !created.evidence_digest.valid()
                || created.evidence_manifest.find(
                    "authorization_sha256")
                    == std::string::npos)
            {
                return false;
            }

            const std::string second{"int value = 2;\n"};
            const TransactionResult updated = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("update"),
                .changes = {{
                    .stable_operation_id = 2u,
                    .relative_path = "Engine/src/sample.cpp",
                    .before = present(first),
                    .after = present(second),
                    .replacement_bytes = second}}});
            if (!updated || read(file) != second)
                return false;

            const TransactionResult removed = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("delete"),
                .changes = {{
                    .stable_operation_id = 3u,
                    .relative_path = "Engine/src/sample.cpp",
                    .before = present(second),
                    .after = {},
                    .replacement_bytes = {}}}});
            return removed && !fs::exists(file)
                && no_transaction_artifacts(workspace.root);
        }

        [[nodiscard]] bool multi_file_contract()
        {
            TemporaryWorkspace workspace{};
            if (workspace.root.empty())
                return false;
            const fs::path first_path =
                workspace.root / "Engine" / "src" / "first.cpp";
            const fs::path second_path =
                workspace.root / "Engine" / "src" / "second.cpp";
            if (!write(first_path, "first-before\n")
                || !write(second_path, "second-before\n"))
            {
                return false;
            }

            SourceTransactionExecutor executor{};
            const TransactionResult result = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("two-files"),
                .changes = {
                    {
                        .stable_operation_id = 10u,
                        .relative_path = "Engine/src/first.cpp",
                        .before = present("first-before\n"),
                        .after = present("first-after\n"),
                        .replacement_bytes = "first-after\n"
                    },
                    {
                        .stable_operation_id = 11u,
                        .relative_path = "Engine/src/second.cpp",
                        .before = present("second-before\n"),
                        .after = present("second-after\n"),
                        .replacement_bytes = "second-after\n"
                    }
                }});
            return result
                && result.files.size() == 2u
                && read(first_path) == "first-after\n"
                && read(second_path) == "second-after\n"
                && no_transaction_artifacts(workspace.root);
        }

        [[nodiscard]] bool stale_preimage_contract()
        {
            TemporaryWorkspace workspace{};
            if (workspace.root.empty())
                return false;
            const fs::path file =
                workspace.root / "Engine" / "src" / "stale.cpp";
            if (!write(file, "changed-after-approval\n"))
                return false;

            SourceTransactionExecutor executor{};
            const TransactionResult result = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("stale"),
                .changes = {{
                    .stable_operation_id = 20u,
                    .relative_path = "Engine/src/stale.cpp",
                    .before = present("approved-preimage\n"),
                    .after = present("requested-postimage\n"),
                    .replacement_bytes = "requested-postimage\n"}}});
            return result.code == TransactionCode::preimage_mismatch
                && read(file) == "changed-after-approval\n"
                && !result.committed
                && no_transaction_artifacts(workspace.root);
        }

        [[nodiscard]] bool malformed_request_contract()
        {
            TemporaryWorkspace workspace{};
            if (workspace.root.empty())
                return false;

            SourceTransactionExecutor executor{};
            const TransactionResult forged_authorization = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = {},
                .changes = {{
                    .stable_operation_id = 29u,
                    .relative_path = "Engine/src/forged.cpp",
                    .before = {},
                    .after = present("forged"),
                    .replacement_bytes = "forged"}}});
            if (forged_authorization.code
                != TransactionCode::invalid_authorization)
            {
                return false;
            }

            const TransactionResult reserved_alias = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("reserved-alias"),
                .changes = {{
                    .stable_operation_id = 30u,
                    .relative_path = "Engine/src/CON.cpp",
                    .before = {},
                    .after = present("reserved"),
                    .replacement_bytes = "reserved"}}});
            if (reserved_alias.code != TransactionCode::invalid_path)
                return false;

            const TransactionResult traversal = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("traversal"),
                .changes = {{
                    .stable_operation_id = 30u,
                    .relative_path = "../outside.cpp",
                    .before = {},
                    .after = present("outside"),
                    .replacement_bytes = "outside"}}});
            if (traversal.code != TransactionCode::invalid_path)
                return false;

            const TransactionResult duplicate = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("duplicate"),
                .changes = {
                    {
                        .stable_operation_id = 31u,
                        .relative_path = "Engine/src/one.cpp",
                        .before = {},
                        .after = present("one"),
                        .replacement_bytes = "one"
                    },
                    {
                        .stable_operation_id = 32u,
                        .relative_path = "Engine/src/one.cpp",
                        .before = {},
                        .after = present("two"),
                        .replacement_bytes = "two"
                    }
                }});
            if (duplicate.code != TransactionCode::duplicate_path)
                return false;

            const TransactionResult wrong_postimage = executor.execute({
                .workspace_root = workspace.root.string(),
                .permit = authorization("wrong-postimage"),
                .changes = {{
                    .stable_operation_id = 33u,
                    .relative_path = "Engine/src/wrong.cpp",
                    .before = {},
                    .after = present("approved"),
                    .replacement_bytes = "different"}}});
            if (wrong_postimage.code
                != TransactionCode::postimage_mismatch)
            {
                return false;
            }

            const TransactionResult relative_root = executor.execute({
                .workspace_root = "relative",
                .permit = authorization("root"),
                .changes = {{
                    .stable_operation_id = 34u,
                    .relative_path = "Engine/src/root.cpp",
                    .before = {},
                    .after = present("root"),
                    .replacement_bytes = "root"}}});
            return relative_root.code == TransactionCode::invalid_root
                && no_transaction_artifacts(workspace.root);
        }

        [[nodiscard]] bool workspace_materializer_contract()
        {
            TemporaryWorkspace workspace{};
            if (workspace.root.empty())
                return false;

            const fs::path source = workspace.root / "source";
            const fs::path sandbox = workspace.root / "sandbox";
            const fs::path cancelled = workspace.root / "cancelled";
            const fs::path bounded = workspace.root / "bounded";
            std::error_code error{};
            fs::create_directories(source / "Engine/src/ai", error);
            if (error)
                return false;
            fs::create_directories(source / "Engine/modules", error);
            if (error)
                return false;
            fs::create_directories(
                source / "Engine/vcpkg_installed/x64-windows/lib",
                error);
            if (error)
                return false;
            fs::create_directories(sandbox / "logs", error);
            if (error)
                return false;
            fs::create_directories(cancelled, error);
            if (error)
                return false;
            fs::create_directories(bounded, error);
            if (error)
                return false;

            constexpr std::string_view solution{"solution-v1"};
            constexpr std::string_view implementation{"implementation-v1"};
            constexpr std::string_view module{"module-v1"};
            if (!write(source / "Engine.sln", solution)
                || !write(
                    source / "Engine/src/ai/ai.worker.cpp",
                    implementation)
                || !write(
                    source / "Engine/modules/ai.worker.ixx",
                    module)
                || !write(
                    source / "Engine/vcpkg_installed/x64-windows/lib/skip.lib",
                    "dependency-cache"))
            {
                return false;
            }

            const SourceWorkspaceMaterializer materializer{};
            const WorkspaceRequest request{
                .source_root = source.generic_string(),
                .workspace_root = sandbox.generic_string(),
                .include_paths = {"Engine.sln", "Engine"},
                .excluded_components = {
                    "build",
                    "built",
                    "Bin",
                    "cache",
                    "vcpkg_installed"}};
            const WorkspaceResult copied =
                materializer.materialize(request);
            const std::uint64_t expectedBytes =
                solution.size() + implementation.size() + module.size();
            if (!copied
                || copied.file_count != 3u
                || copied.total_bytes != expectedBytes
                || copied.evidence_digest != digest(copied.evidence_manifest)
                || copied.evidence_manifest.find(
                    "Engine/src/ai/ai.worker.cpp")
                    == std::string::npos
                || copied.evidence_manifest.find("vcpkg_installed")
                    != std::string::npos
                || read(sandbox / "Engine.sln") != solution
                || read(sandbox / "Engine/src/ai/ai.worker.cpp")
                    != implementation
                || read(sandbox / "Engine/modules/ai.worker.ixx")
                    != module
                || fs::exists(
                    sandbox
                        / "Engine/vcpkg_installed/x64-windows/lib/skip.lib",
                    error)
                || read(source / "Engine.sln") != solution
                || read(source / "Engine/src/ai/ai.worker.cpp")
                    != implementation)
            {
                return false;
            }

            if (materializer.materialize(request).code
                    != WorkspaceCode::destination_not_empty)
            {
                return false;
            }

            WorkspaceRequest cancelledRequest = request;
            cancelledRequest.workspace_root = cancelled.generic_string();
            cancelledRequest.cancellation_requested = []
            {
                return true;
            };
            if (materializer.materialize(cancelledRequest).code
                    != WorkspaceCode::cancelled)
            {
                return false;
            }

            WorkspaceLimits tiny{};
            tiny.maximum_file_bytes = 8u;
            tiny.maximum_total_bytes = 8u;
            SourceWorkspaceMaterializer boundedMaterializer{tiny};
            WorkspaceRequest boundedRequest = request;
            boundedRequest.workspace_root = bounded.generic_string();
            return boundedMaterializer.materialize(boundedRequest).code
                    == WorkspaceCode::size_limit_exceeded
                && read(source / "Engine.sln") == solution
                && read(source / "Engine/src/ai/ai.worker.cpp")
                    == implementation;
        }

        [[nodiscard]] bool runtime_workspace_exclusion_contract()
        {
            TemporaryWorkspace fixture{};
            if (fixture.root.empty()) return false;
            const auto source = fixture.root / "runtime-source";
            const auto candidate = fixture.root / "runtime-candidate";
            const auto repair = fixture.root / "runtime-repair";
            const auto successor = fixture.root / "runtime-successor";
            const auto refused = fixture.root / "runtime-refused";
            const fs::path runtime{"Engine/examples/EpochEditor/workspace"};
            const fs::path implementationPath{"Engine/src/ai.worker.cpp"};
            const fs::path legitimatePath{"Engine/src/workspace/core.document.cpp"};
            const fs::path nearPath{
                "Engine/examples/EpochEditor/workspace_sources/core.document.cpp"};
            std::error_code error{};
            for (const auto& directory : {source / runtime,
                    source / legitimatePath.parent_path(), source / nearPath.parent_path(),
                    candidate, repair, successor, refused})
            {
                fs::create_directories(directory, error);
                if (error) return false;
            }
            constexpr std::string_view solution{"runtime-exclusion-solution"};
            constexpr std::string_view implementation{"source-before-choice"};
            constexpr std::string_view chosen{"source-after-choice"};
            constexpr std::string_view legitimate{"legitimate-workspace-source"};
            constexpr std::string_view nearby{"legitimate-prefix-neighbor"};
            // Synthetic fixture bytes only; no real conversation is inspected.
            constexpr std::string_view conversation{"synthetic-conversation-canary"};
            constexpr std::string_view trace{"synthetic-tool-trace-canary"};
            if (!write(source / "Engine.sln", solution)
                || !write(source / implementationPath, implementation)
                || !write(source / legitimatePath, legitimate)
                || !write(source / nearPath, nearby)
                || !write(source / runtime / "model_exchange.jsonl", conversation)
                || !write(source / runtime / "tool_trace.jsonl", trace))
                return false;

            const SourceWorkspaceMaterializer materializer{};
            WorkspaceRequest request{
                .source_root = source.generic_string(),
                .workspace_root = candidate.generic_string(),
                .include_paths = {"Engine.sln", "Engine"}};
            const auto verify = [&](const WorkspaceResult& result, const fs::path& root,
                                    std::string_view expectedImplementation)
            {
                error.clear();
                return result && result.file_count == 4u
                    && result.total_bytes == solution.size() + expectedImplementation.size()
                        + legitimate.size() + nearby.size()
                    && result.evidence_digest == digest(result.evidence_manifest)
                    && result.evidence_manifest.find(runtime.generic_string() + "/")
                        == std::string::npos
                    && result.evidence_manifest.find(conversation) == std::string::npos
                    && result.evidence_manifest.find(trace) == std::string::npos
                    && read(root / "Engine.sln") == solution
                    && read(root / implementationPath) == expectedImplementation
                    && read(root / legitimatePath) == legitimate
                    && read(root / nearPath) == nearby
                    && !fs::exists(root / runtime, error) && !error;
            };
            if (!verify(materializer.materialize(request), candidate, implementation))
                return false;
            // Repair uses a fresh destination but the same production copier.
            request.workspace_root = repair.generic_string();
            if (!verify(materializer.materialize(request), repair, implementation))
                return false;

            // Explicitly naming a runtime file cannot bypass the owned exclusion.
            auto direct = request;
            direct.workspace_root = refused.generic_string();
            direct.include_paths = {(runtime / "model_exchange.jsonl").generic_string()};
            if (materializer.materialize(direct).code != WorkspaceCode::invalid_request
                || !fs::is_empty(refused, error) || error)
                return false;
#if defined(_WIN32)
            direct.include_paths = {
                "Engine/Examples/EPOCHEDITOR/WORKSPACE/tool_trace.jsonl"};
            if (materializer.materialize(direct).code != WorkspaceCode::invalid_request)
                return false;
#endif

            // A chosen preview may write its own runtime conversation. The next
            // generation keeps the selected source change, never that activity.
            fs::create_directories(candidate / runtime, error);
            if (error || !write(candidate / implementationPath, chosen)
                || !write(candidate / runtime / "model_exchange.jsonl", conversation)
                || !write(candidate / runtime / "tool_trace.jsonl", trace))
                return false;
            request.source_root = candidate.generic_string();
            request.workspace_root = successor.generic_string();
            return verify(materializer.materialize(request), successor, chosen)
                && read(source / implementationPath) == implementation
                && read(source / runtime / "model_exchange.jsonl") == conversation
                && read(source / runtime / "tool_trace.jsonl") == trace
                && read(candidate / runtime / "model_exchange.jsonl") == conversation
                && read(candidate / runtime / "tool_trace.jsonl") == trace;
        }
    }

    [[nodiscard]] int contract_failure_code()
    {
        if (const int code = digest_contract(); code != 0) return code;
        if (!create_update_delete_contract()) return 2;
        if (!multi_file_contract()) return 3;
        if (!stale_preimage_contract()) return 4;
        if (!malformed_request_contract()) return 5;
        if (!workspace_materializer_contract()) return 6;
        if (!runtime_workspace_exclusion_contract()) return 7;
        return 0;
    }

    bool run_contract()
    {
        return contract_failure_code() == 0;
    }
}

extern "C++" int main()
{
    return epochengine::ai::development_executor::contract_failure_code();
}
