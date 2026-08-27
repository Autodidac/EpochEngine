/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <array>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <string>

import scripting.compiler;

namespace epochengine::compiler::contract
{
    enum class Failure : int
    {
        none,
        setup,
        success_plan,
        deterministic_command,
        malformed,
        missing,
        invalid_name,
        traversal,
        outside_workspace,
        expected_source,
        source_limit,
        compiler_unavailable,
        temporary_collision,
        sidecar_collision,
        output_nonregular,
        cancelled,
        execution,
        publication,
        stale_output
    };

    [[nodiscard]] Failure run(const std::filesystem::path& parent) noexcept
    {
        namespace fs = std::filesystem;
        try
        {
            const fs::path root =
                parent / "epoch_script_compiler_contract_workspace";
            const fs::path scripts = root / "assets" / "scripts";
            std::error_code error{};
            fs::create_directories(scripts, error);
            if (error)
                return Failure::setup;

            const fs::path source = scripts / "player.ascript.cpp";
#ifdef _WIN32
            const fs::path output = scripts / "player.dll";
#else
            const fs::path output = scripts / "libplayer.so";
#endif
            {
                std::ofstream stream(source, std::ios::binary | std::ios::trunc);
                stream << "extern \"C\" void run_script(void*) {}\n";
                if (!stream)
                    return Failure::setup;
            }

            CompileRequest request{
                .source_path = source,
                .output_path = output,
                .workspace_root = root,
                .request_generation = 7u,
                .project_generation = 3u};
            const CompilePlanningResult first = plan_script_compile(request);
            if (!first)
            {
                std::ofstream diagnostic(
                    parent / "script_compiler_contract.txt");
                diagnostic << compile_refusal_name(first.refusal)
                    << ": " << first.message;
                return Failure::success_plan;
            }
            if (first.plan.source_path != fs::weakly_canonical(source)
                || first.plan.output_path
                    != (fs::weakly_canonical(scripts) / output.filename())
                || first.plan.compiler_path.empty()
                || !first.plan.compiler_path.is_absolute()
                || first.plan.temporary_output_path == first.plan.output_path
                || first.plan.temporary_output_path.parent_path()
                    != first.plan.output_path.parent_path())
            {
                return Failure::success_plan;
            }
            const CompilePlanningResult second = plan_script_compile(request);
            if (!second
                || second.plan.compiler_path != first.plan.compiler_path
                || second.plan.command_description
                    != first.plan.command_description)
            {
                return Failure::deterministic_command;
            }

            CompileRequest malformed = request;
            malformed.request_generation = 0u;
            if (plan_script_compile(malformed).refusal
                != CompileRefusal::malformed_request)
            {
                return Failure::malformed;
            }

            CompileRequest missing = request;
            missing.source_path = scripts / "missing.ascript.cpp";
            if (plan_script_compile(missing).refusal
                != CompileRefusal::missing_source)
            {
                return Failure::missing;
            }

            CompileRequest invalidName = request;
            invalidName.source_path = scripts / "player.cpp";
            if (plan_script_compile(invalidName).refusal
                != CompileRefusal::invalid_source_name)
            {
                return Failure::invalid_name;
            }

            CompileRequest traversal = request;
            traversal.source_path = scripts / ".." / "player.ascript.cpp";
            if (plan_script_compile(traversal).refusal
                != CompileRefusal::path_traversal)
            {
                return Failure::traversal;
            }

            CompileRequest outside = request;
#ifdef _WIN32
            outside.output_path = parent / "outside.dll";
#else
            outside.output_path = parent / "liboutside.so";
#endif
            if (plan_script_compile(outside).refusal
                != CompileRefusal::path_outside_workspace)
            {
                return Failure::outside_workspace;
            }

            CompileRequest stale = request;
            stale.require_expected_source = true;
            stale.expected_source = first.plan.source_before;
            stale.expected_source.size_bytes += 1u;
            if (plan_script_compile(stale).refusal
                != CompileRefusal::source_evidence_mismatch)
            {
                return Failure::expected_source;
            }

            CompileRequest bounded = request;
            bounded.maximum_source_bytes = 1u;
            if (plan_script_compile(bounded).refusal
                != CompileRefusal::source_too_large)
            {
                return Failure::source_limit;
            }

            CompileRequest unavailable = request;
#ifdef _WIN32
            unavailable.compiler_path = scripts / "missing_compiler.exe";
#else
            unavailable.compiler_path = scripts / "missing_compiler";
#endif
            if (plan_script_compile(unavailable).refusal
                != CompileRefusal::compiler_unavailable)
            {
                return Failure::compiler_unavailable;
            }

            {
                std::ofstream occupied(
                    first.plan.temporary_output_path,
                    std::ios::binary | std::ios::trunc);
                occupied << "operator-owned collision\n";
                if (!occupied)
                    return Failure::setup;
            }
            const FileProbeResult temporaryBefore = inspect_file(
                first.plan.temporary_output_path, kDefaultMaximumArtifactBytes);
            if (!temporaryBefore
                || plan_script_compile(request).refusal
                    != CompileRefusal::temporary_path_occupied)
            {
                return Failure::temporary_collision;
            }
            const FileProbeResult temporaryAfter = inspect_file(
                first.plan.temporary_output_path, kDefaultMaximumArtifactBytes);
            if (!temporaryAfter
                || !(temporaryAfter.evidence == temporaryBefore.evidence))
            {
                return Failure::temporary_collision;
            }
            fs::remove(first.plan.temporary_output_path, error);
            if (error)
                return Failure::setup;

            const fs::path sidecar{
                first.plan.temporary_output_path.string() + ".obj"};
            {
                std::ofstream occupied(
                    sidecar, std::ios::binary | std::ios::trunc);
                occupied << "operator-owned sidecar\n";
                if (!occupied)
                    return Failure::setup;
            }
            const FileProbeResult sidecarBefore = inspect_file(
                sidecar, kDefaultMaximumArtifactBytes);
            if (!sidecarBefore
                || plan_script_compile(request).refusal
                    != CompileRefusal::temporary_path_occupied)
            {
                return Failure::sidecar_collision;
            }
            const FileProbeResult sidecarAfter = inspect_file(
                sidecar, kDefaultMaximumArtifactBytes);
            if (!sidecarAfter
                || !(sidecarAfter.evidence == sidecarBefore.evidence))
            {
                return Failure::sidecar_collision;
            }
            fs::remove(sidecar, error);
            if (error)
                return Failure::setup;

#ifdef _WIN32
            const fs::path nonregular = scripts / "occupied.dll";
#else
            const fs::path nonregular = scripts / "occupied.so";
#endif
            fs::create_directory(nonregular, error);
            if (error)
                return Failure::setup;
            CompileRequest outputDirectory = request;
            outputDirectory.output_path = nonregular;
            if (plan_script_compile(outputDirectory).refusal
                != CompileRefusal::filesystem_error)
            {
                return Failure::output_nonregular;
            }

            std::stop_source stop{};
            stop.request_stop();
            CompileRequest cancelled = request;
            cancelled.cancellation = stop.get_token();
            if (plan_script_compile(cancelled).refusal
                != CompileRefusal::cancelled)
            {
                return Failure::cancelled;
            }

            {
                std::ofstream staleOutput(
                    output, std::ios::binary | std::ios::trunc);
                staleOutput
                    << "stale artifact must survive until publication\n";
                if (!staleOutput)
                    return Failure::setup;
            }
            const FileProbeResult staleOutput = inspect_file(
                output, kDefaultMaximumArtifactBytes);
            if (!staleOutput)
                return Failure::setup;

            CompileRequest execution = request;
            execution.request_generation = 8u;
            const CompileResult compiled = compile_script(execution);
            if (!compiled)
            {
                std::ofstream diagnostic(
                    parent / "script_compiler_contract.txt");
                diagnostic << compile_status_name(compiled.status) << "/"
                    << compile_refusal_name(compiled.refusal)
                    << ": " << compiled.message;
                return Failure::execution;
            }
            if (!compiled.evidence.compiler_started
                || !compiled.evidence.scope_revalidated
                || !compiled.evidence.source_revalidated
                || !compiled.evidence.output_ownership_revalidated
                || !compiled.evidence.candidate_verified
                || !compiled.evidence.publication_scope_revalidated
                || !compiled.evidence.atomic_replace_committed
                || !compiled.evidence.published_artifact_verified
                || compiled.evidence.published.size_bytes == 0u
                || compiled.evidence.candidate.size_bytes
                    != compiled.evidence.published.size_bytes
                || !(compiled.evidence.candidate.content
                    == compiled.evidence.published.content))
            {
                return Failure::publication;
            }

            const std::array<fs::path, 5> temporaryArtifacts{
                compiled.evidence.temporary_output_path,
                fs::path{
                    compiled.evidence.temporary_output_path.string() + ".obj"},
                fs::path{
                    compiled.evidence.temporary_output_path.string() + ".pdb"},
                fs::path{
                    compiled.evidence.temporary_output_path.string() + ".lib"},
                fs::path{
                    compiled.evidence.temporary_output_path.string() + ".exp"}};
            for (const auto& temporary : temporaryArtifacts)
            {
                if (fs::exists(temporary, error) || error)
                    return Failure::publication;
            }
            if (compiled.evidence.published.content
                == staleOutput.evidence.content)
            {
                return Failure::stale_output;
            }
            return Failure::none;
        }
        catch (...)
        {
            return Failure::setup;
        }
    }
}

#if defined(EPOCH_SCRIPT_COMPILER_CONTRACT_MAIN)
int main(int argc, char** argv)
{
    if (argc != 2 || argv == nullptr || argv[1] == nullptr)
        return static_cast<int>(
            epochengine::compiler::contract::Failure::setup);
    return static_cast<int>(
        epochengine::compiler::contract::run(argv[1]));
}
#endif