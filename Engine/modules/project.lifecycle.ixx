/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <limits>
#include <string_view>

export module project.lifecycle;

export namespace epochengine::project_lifecycle
{
    enum class Request : std::uint8_t
    {
        save,
        materialize,
        build,
        run
    };

    enum class Action : std::uint8_t
    {
        blocked,
        save_scene,
        materialize_shell,
        prepare_build_inputs,
        build_project,
        wait_for_build,
        launch_runtime,
        focus_runtime
    };

    enum class BlockReason : std::uint8_t
    {
        none,
        no_project,
        scene_commit_failed,
        invalid_generation,
        missing_generation_evidence,
        scene_generation_mismatch,
        shell_generation_mismatch,
        build_input_generation_mismatch,
        active_build_generation_mismatch,
        artifact_unverified,
        artifact_generation_mismatch,
        runtime_generation_mismatch
    };

    [[nodiscard]] constexpr std::string_view action_name(Action action) noexcept
    {
        switch (action)
        {
        case Action::blocked: return "Blocked";
        case Action::save_scene: return "Save scene";
        case Action::materialize_shell: return "Materialize project";
        case Action::prepare_build_inputs: return "Prepare build inputs";
        case Action::build_project: return "Build project";
        case Action::wait_for_build: return "Wait for build";
        case Action::launch_runtime: return "Launch runtime";
        case Action::focus_runtime: return "Focus runtime";
        }
        return "Unknown";
    }

    [[nodiscard]] constexpr std::string_view block_reason_name(
        BlockReason reason) noexcept
    {
        switch (reason)
        {
        case BlockReason::none: return "None";
        case BlockReason::no_project: return "No project selected";
        case BlockReason::scene_commit_failed: return "Scene commit failed";
        case BlockReason::invalid_generation: return "Invalid generation";
        case BlockReason::missing_generation_evidence:
            return "Generation evidence missing";
        case BlockReason::scene_generation_mismatch:
            return "Scene generation mismatch";
        case BlockReason::shell_generation_mismatch:
            return "Project shell generation mismatch";
        case BlockReason::build_input_generation_mismatch:
            return "Build input generation mismatch";
        case BlockReason::active_build_generation_mismatch:
            return "Active build generation mismatch";
        case BlockReason::artifact_unverified: return "Artifact unverified";
        case BlockReason::artifact_generation_mismatch:
            return "Artifact generation mismatch";
        case BlockReason::runtime_generation_mismatch:
            return "Runtime generation mismatch";
        }
        return "Unknown";
    }

    struct ProjectStamp final
    {
        std::uint64_t project_key{};
        std::uint64_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return project_key != 0u && generation != 0u;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const ProjectStamp&,
            const ProjectStamp&) noexcept = default;
    };

    struct BuildInputEvidence final
    {
        ProjectStamp project{};
        std::uint64_t scene_generation{};
        std::uint64_t input_generation{};

        [[nodiscard]] constexpr bool valid_for(
            ProjectStamp selected,
            std::uint64_t committedSceneGeneration) const noexcept
        {
            return project.valid()
                && project == selected
                && scene_generation != 0u
                && scene_generation == committedSceneGeneration
                && input_generation != 0u;
        }
    };

    struct ActiveBuildEvidence final
    {
        ProjectStamp project{};
        std::uint64_t input_generation{};
        std::uint64_t build_generation{};

        [[nodiscard]] constexpr bool valid_for(
            ProjectStamp selected,
            const BuildInputEvidence& inputs) const noexcept
        {
            return project.valid()
                && project == selected
                && inputs.project.valid()
                && inputs.project == selected
                && inputs.scene_generation != 0u
                && input_generation != 0u
                && input_generation == inputs.input_generation
                && build_generation != 0u;
        }
    };

    struct BuildArtifactEvidence final
    {
        ProjectStamp project{};
        std::uint64_t input_generation{};
        std::uint64_t build_generation{};
        std::uint64_t artifact_generation{};
        bool verified{};

        [[nodiscard]] constexpr bool valid_for(
            ProjectStamp selected,
            const BuildInputEvidence& inputs) const noexcept
        {
            return verified
                && project.valid()
                && project == selected
                && inputs.project.valid()
                && inputs.project == selected
                && inputs.scene_generation != 0u
                && input_generation != 0u
                && input_generation == inputs.input_generation
                && build_generation != 0u
                && artifact_generation != 0u;
        }
    };

    struct RuntimeEvidence final
    {
        ProjectStamp project{};
        std::uint64_t artifact_generation{};
        std::uint64_t runtime_generation{};

        [[nodiscard]] constexpr bool valid_for(
            ProjectStamp selected,
            const BuildArtifactEvidence& artifact) const noexcept
        {
            return project.valid()
                && project == selected
                && artifact.verified
                && artifact.project.valid()
                && artifact.project == selected
                && artifact.build_generation != 0u
                && artifact_generation != 0u
                && artifact_generation == artifact.artifact_generation
                && runtime_generation != 0u;
        }
    };

    struct GenerationEvidence final
    {
        ProjectStamp selected_project{};
        std::uint64_t committed_scene_generation{};
        ProjectStamp materialized_shell_project{};
        BuildInputEvidence build_inputs{};
        BuildArtifactEvidence output_artifact{};
        RuntimeEvidence runtime{};
        ActiveBuildEvidence active_build{};
    };

    struct Evidence final
    {
        bool project_selected{};
        bool scene_committed{};
        bool shell_materialized{};
        bool build_inputs_current{};
        bool output_exists{};
        bool build_active{};
        bool runtime_active{};
        bool require_generation_evidence{true};
        GenerationEvidence generations{};
    };

    struct Decision final
    {
        Action action{Action::blocked};
        BlockReason reason{BlockReason::none};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return action != Action::blocked;
        }
    };

    [[nodiscard]] constexpr Decision decide_legacy(
        Request request,
        const Evidence& evidence) noexcept
    {
        if (!evidence.project_selected)
            return {Action::blocked, BlockReason::no_project};
        if (request == Request::save)
            return {Action::save_scene, BlockReason::none};
        if (!evidence.scene_committed)
            return {Action::save_scene, BlockReason::none};
        if (request == Request::materialize)
        {
            return evidence.shell_materialized
                ? Decision{Action::save_scene, BlockReason::none}
                : Decision{Action::materialize_shell, BlockReason::none};
        }
        if (!evidence.shell_materialized)
            return {Action::materialize_shell, BlockReason::none};
        if (evidence.build_active)
            return {Action::wait_for_build, BlockReason::none};
        if (request == Request::build)
            return {Action::build_project, BlockReason::none};
        if (!evidence.build_inputs_current || !evidence.output_exists)
            return {Action::build_project, BlockReason::none};
        if (evidence.runtime_active)
            return {Action::focus_runtime, BlockReason::none};
        return {Action::launch_runtime, BlockReason::none};
    }

    [[nodiscard]] constexpr Decision decide_verified(
        Request request,
        const Evidence& evidence) noexcept
    {
        if (!evidence.project_selected)
            return {Action::blocked, BlockReason::no_project};
        if (request == Request::save)
            return {Action::save_scene, BlockReason::none};
        if (!evidence.scene_committed)
            return {Action::save_scene, BlockReason::none};

        const GenerationEvidence& generations = evidence.generations;
        if (!generations.selected_project.valid()
            || generations.committed_scene_generation == 0u)
        {
            return {Action::blocked, BlockReason::missing_generation_evidence};
        }
        if (request == Request::materialize)
        {
            return evidence.shell_materialized
                && generations.materialized_shell_project
                    == generations.selected_project
                ? Decision{Action::save_scene, BlockReason::none}
                : Decision{
                    Action::materialize_shell,
                    BlockReason::shell_generation_mismatch};
        }
        if (!evidence.shell_materialized
            || generations.materialized_shell_project
                != generations.selected_project)
        {
            return {
                Action::materialize_shell,
                BlockReason::shell_generation_mismatch};
        }

        const bool inputsCurrent = evidence.build_inputs_current
            && generations.build_inputs.valid_for(
                generations.selected_project,
                generations.committed_scene_generation);
        if (evidence.build_active)
        {
            if (!inputsCurrent
                || !generations.active_build.valid_for(
                    generations.selected_project,
                    generations.build_inputs))
            {
                return {
                    Action::blocked,
                    BlockReason::active_build_generation_mismatch};
            }
            return {Action::wait_for_build, BlockReason::none};
        }
        if (!inputsCurrent)
        {
            return {
                Action::prepare_build_inputs,
                BlockReason::build_input_generation_mismatch};
        }
        if (request == Request::build)
            return {Action::build_project, BlockReason::none};

        if (!evidence.output_exists)
            return {Action::build_project, BlockReason::none};
        if (!generations.output_artifact.verified)
        {
            return {
                Action::build_project,
                BlockReason::artifact_unverified};
        }
        if (!generations.output_artifact.valid_for(
                generations.selected_project,
                generations.build_inputs))
        {
            return {
                Action::build_project,
                BlockReason::artifact_generation_mismatch};
        }

        if (evidence.runtime_active)
        {
            if (!generations.runtime.valid_for(
                    generations.selected_project,
                    generations.output_artifact))
            {
                return {
                    Action::blocked,
                    BlockReason::runtime_generation_mismatch};
            }
            return {Action::focus_runtime, BlockReason::none};
        }
        return {Action::launch_runtime, BlockReason::none};
    }

    [[nodiscard]] constexpr Decision decide(
        Request request,
        const Evidence& evidence) noexcept
    {
        return evidence.require_generation_evidence
            ? decide_verified(request, evidence)
            : decide_legacy(request, evidence);
    }

    class AttemptTracker final
    {
    public:
        [[nodiscard]] constexpr std::uint64_t begin() noexcept
        {
            if (active_generation_ != 0u
                || latest_generation_ == (std::numeric_limits<std::uint64_t>::max)())
            {
                return 0u;
            }
            active_generation_ = ++latest_generation_;
            return active_generation_;
        }

        [[nodiscard]] constexpr bool complete(
            std::uint64_t generation) noexcept
        {
            if (generation == 0u || generation != active_generation_)
                return false;
            completed_generation_ = generation;
            active_generation_ = 0u;
            return true;
        }

        [[nodiscard]] constexpr bool cancel(
            std::uint64_t generation) noexcept
        {
            if (generation == 0u || generation != active_generation_)
                return false;
            active_generation_ = 0u;
            return true;
        }

        [[nodiscard]] constexpr bool active() const noexcept
        {
            return active_generation_ != 0u;
        }

        [[nodiscard]] constexpr std::uint64_t active_generation() const noexcept
        {
            return active_generation_;
        }

        [[nodiscard]] constexpr std::uint64_t completed_generation() const noexcept
        {
            return completed_generation_;
        }

    private:
        std::uint64_t latest_generation_{};
        std::uint64_t active_generation_{};
        std::uint64_t completed_generation_{};
    };

    class BuildEvidenceTracker final
    {
    public:
        [[nodiscard]] constexpr std::uint64_t begin(
            ProjectStamp project,
            std::uint64_t inputGeneration) noexcept
        {
            if (!project.valid() || inputGeneration == 0u)
                return 0u;
            const std::uint64_t generation = attempts_.begin();
            if (generation == 0u)
                return 0u;
            active_project_ = project;
            active_input_generation_ = inputGeneration;
            return generation;
        }

        [[nodiscard]] constexpr bool complete(
            std::uint64_t buildGeneration,
            std::uint64_t artifactGeneration,
            bool verified) noexcept
        {
            if (!verified
                || artifactGeneration == 0u
                || artifactGeneration <= artifact_.artifact_generation
                || buildGeneration != attempts_.active_generation())
            {
                return false;
            }
            artifact_ = {
                .project = active_project_,
                .input_generation = active_input_generation_,
                .build_generation = buildGeneration,
                .artifact_generation = artifactGeneration,
                .verified = true};
            active_project_ = {};
            active_input_generation_ = 0u;
            return attempts_.complete(buildGeneration);
        }

        [[nodiscard]] constexpr bool cancel(
            std::uint64_t buildGeneration) noexcept
        {
            if (!attempts_.cancel(buildGeneration))
                return false;
            active_project_ = {};
            active_input_generation_ = 0u;
            return true;
        }

        constexpr void invalidate() noexcept { artifact_ = {}; }

        [[nodiscard]] constexpr bool active() const noexcept
        {
            return attempts_.active();
        }

        [[nodiscard]] constexpr ActiveBuildEvidence active_evidence() const noexcept
        {
            return {
                .project = active_project_,
                .input_generation = active_input_generation_,
                .build_generation = attempts_.active_generation()};
        }

        [[nodiscard]] constexpr BuildArtifactEvidence artifact() const noexcept
        {
            return artifact_;
        }

    private:
        AttemptTracker attempts_{};
        ProjectStamp active_project_{};
        std::uint64_t active_input_generation_{};
        BuildArtifactEvidence artifact_{};
    };

    class EvidenceLedger final
    {
    public:
        [[nodiscard]] constexpr bool select_project(
            std::uint64_t projectKey) noexcept
        {
            if (projectKey == 0u)
            {
                clear_project();
                return false;
            }
            if (evidence_.project_selected
                && evidence_.generations.selected_project.project_key
                    == projectKey)
            {
                return true;
            }

            if (builds_.active())
                (void)builds_.cancel(builds_.active_evidence().build_generation);
            builds_.invalidate();

            const std::uint64_t generation = next_generation(
                selection_generation_);
            if (generation == 0u)
            {
                clear_project();
                return false;
            }

            evidence_ = Evidence{.require_generation_evidence = true};
            evidence_.project_selected = true;
            evidence_.generations.selected_project = {
                projectKey,
                generation};
            build_input_fingerprint_ = 0u;
            return true;
        }

        constexpr void clear_project() noexcept
        {
            if (builds_.active())
                (void)builds_.cancel(builds_.active_evidence().build_generation);
            builds_.invalidate();
            evidence_ = Evidence{.require_generation_evidence = true};
            build_input_fingerprint_ = 0u;
        }

        [[nodiscard]] constexpr bool record_scene_commit(
            std::uint64_t sceneGeneration) noexcept
        {
            if (!evidence_.project_selected || sceneGeneration == 0u)
                return false;
            evidence_.scene_committed = true;
            evidence_.generations.committed_scene_generation = sceneGeneration;
            if (evidence_.generations.build_inputs.project
                    == evidence_.generations.selected_project
                && evidence_.generations.build_inputs.input_generation != 0u)
            {
                evidence_.generations.build_inputs.scene_generation =
                    sceneGeneration;
            }
            return true;
        }

        [[nodiscard]] constexpr bool record_shell_materialized() noexcept
        {
            if (!evidence_.project_selected)
                return false;
            evidence_.shell_materialized = true;
            evidence_.generations.materialized_shell_project =
                evidence_.generations.selected_project;
            return true;
        }

        [[nodiscard]] constexpr bool observe_build_inputs(
            std::uint64_t fingerprint) noexcept
        {
            if (!evidence_.project_selected || !evidence_.scene_committed
                || !evidence_.shell_materialized || fingerprint == 0u
                || builds_.active())
            {
                return false;
            }

            if (fingerprint == build_input_fingerprint_
                && evidence_.generations.build_inputs.project
                    == evidence_.generations.selected_project
                && evidence_.generations.build_inputs.input_generation != 0u)
            {
                evidence_.build_inputs_current = true;
                evidence_.generations.build_inputs.scene_generation =
                    evidence_.generations.committed_scene_generation;
                return true;
            }

            const std::uint64_t generation = next_generation(
                input_generation_);
            if (generation == 0u)
                return false;

            build_input_fingerprint_ = fingerprint;
            evidence_.build_inputs_current = true;
            evidence_.generations.build_inputs = {
                .project = evidence_.generations.selected_project,
                .scene_generation =
                    evidence_.generations.committed_scene_generation,
                .input_generation = generation};
            builds_.invalidate();
            evidence_.output_exists = false;
            evidence_.generations.output_artifact = {};
            evidence_.runtime_active = false;
            evidence_.generations.runtime = {};
            return true;
        }

        constexpr void mark_build_inputs_stale() noexcept
        {
            evidence_.build_inputs_current = false;
        }

        [[nodiscard]] constexpr std::uint64_t begin_build() noexcept
        {
            if (!evidence_.build_inputs_current || builds_.active())
                return 0u;
            const std::uint64_t generation = builds_.begin(
                evidence_.generations.selected_project,
                evidence_.generations.build_inputs.input_generation);
            if (generation == 0u)
                return 0u;
            evidence_.build_active = true;
            evidence_.generations.active_build = builds_.active_evidence();
            return generation;
        }

        [[nodiscard]] constexpr bool complete_build(
            std::uint64_t buildGeneration,
            bool verified) noexcept
        {
            if (!verified)
            {
                (void)fail_build(buildGeneration);
                return false;
            }
            const std::uint64_t artifactGeneration = next_value(
                artifact_generation_);
            if (artifactGeneration == 0u
                || !builds_.complete(
                    buildGeneration,
                    artifactGeneration,
                    true))
            {
                return false;
            }
            artifact_generation_ = artifactGeneration;
            evidence_.build_active = false;
            evidence_.generations.active_build = {};
            evidence_.output_exists = true;
            evidence_.generations.output_artifact = builds_.artifact();
            return true;
        }

        [[nodiscard]] constexpr bool fail_build(
            std::uint64_t buildGeneration) noexcept
        {
            if (!builds_.cancel(buildGeneration))
                return false;
            builds_.invalidate();
            evidence_.build_active = false;
            evidence_.generations.active_build = {};
            evidence_.generations.output_artifact = {};
            evidence_.output_exists = false;
            evidence_.runtime_active = false;
            evidence_.generations.runtime = {};
            return true;
        }

        constexpr void observe_output_missing() noexcept
        {
            builds_.invalidate();
            evidence_.output_exists = false;
            evidence_.generations.output_artifact = {};
            evidence_.runtime_active = false;
            evidence_.generations.runtime = {};
        }

        [[nodiscard]] constexpr bool record_runtime_started() noexcept
        {
            if (!evidence_.output_exists
                || !evidence_.generations.output_artifact.valid_for(
                    evidence_.generations.selected_project,
                    evidence_.generations.build_inputs))
            {
                return false;
            }
            const std::uint64_t generation = next_generation(
                runtime_generation_);
            if (generation == 0u)
                return false;
            evidence_.runtime_active = true;
            evidence_.generations.runtime = {
                .project = evidence_.generations.selected_project,
                .artifact_generation = evidence_.generations.output_artifact
                    .artifact_generation,
                .runtime_generation = generation};
            return true;
        }

        constexpr void record_runtime_stopped() noexcept
        {
            evidence_.runtime_active = false;
            evidence_.generations.runtime = {};
        }

        [[nodiscard]] constexpr Decision decision(
            Request request) const noexcept
        {
            return epochengine::project_lifecycle::decide(request, evidence_);
        }

        [[nodiscard]] constexpr const Evidence& evidence() const noexcept
        {
            return evidence_;
        }

        [[nodiscard]] constexpr std::uint64_t build_input_fingerprint()
            const noexcept
        {
            return build_input_fingerprint_;
        }

    private:
        [[nodiscard]] static constexpr std::uint64_t next_value(
            std::uint64_t current) noexcept
        {
            return current == (std::numeric_limits<std::uint64_t>::max)()
                ? 0u
                : current + 1u;
        }

        [[nodiscard]] static constexpr std::uint64_t next_generation(
            std::uint64_t& current) noexcept
        {
            const std::uint64_t next = next_value(current);
            if (next != 0u)
                current = next;
            return next;
        }

        Evidence evidence_{.require_generation_evidence = true};
        BuildEvidenceTracker builds_{};
        std::uint64_t selection_generation_{};
        std::uint64_t input_generation_{};
        std::uint64_t artifact_generation_{};
        std::uint64_t runtime_generation_{};
        std::uint64_t build_input_fingerprint_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        missing_project,
        save_first,
        materialize_first,
        build_first,
        launch,
        active_build,
        focus_runtime,
        generation_tracking,
        missing_strict_evidence,
        stale_inputs,
        stale_active_build,
        stale_artifact,
        mismatched_runtime,
        artifact_tracking,
        artifact_generation_reuse,
        ledger_flow,
        ledger_scene_refresh,
        ledger_project_switch,
        ledger_project_close,
        ledger_failed_build,
        ledger_output_missing
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::missing_project: return "missing_project";
        case ContractFailure::save_first: return "save_first";
        case ContractFailure::materialize_first: return "materialize_first";
        case ContractFailure::build_first: return "build_first";
        case ContractFailure::launch: return "launch";
        case ContractFailure::active_build: return "active_build";
        case ContractFailure::focus_runtime: return "focus_runtime";
        case ContractFailure::generation_tracking: return "generation_tracking";
        case ContractFailure::missing_strict_evidence: return "missing_strict_evidence";
        case ContractFailure::stale_inputs: return "stale_inputs";
        case ContractFailure::stale_active_build: return "stale_active_build";
        case ContractFailure::stale_artifact: return "stale_artifact";
        case ContractFailure::mismatched_runtime: return "mismatched_runtime";
        case ContractFailure::artifact_tracking: return "artifact_tracking";
        case ContractFailure::artifact_generation_reuse: return "artifact_generation_reuse";
        case ContractFailure::ledger_flow: return "ledger_flow";
        case ContractFailure::ledger_scene_refresh: return "ledger_scene_refresh";
        case ContractFailure::ledger_project_switch: return "ledger_project_switch";
        case ContractFailure::ledger_project_close: return "ledger_project_close";
        case ContractFailure::ledger_failed_build: return "ledger_failed_build";
        case ContractFailure::ledger_output_missing: return "ledger_output_missing";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr ContractFailure runtime_contract_failure() noexcept
    {
        Evidence evidence{.require_generation_evidence = false};
        if (decide(Request::run, evidence).reason != BlockReason::no_project)
            return ContractFailure::missing_project;

        evidence.project_selected = true;
        if (decide(Request::run, evidence).action != Action::save_scene)
            return ContractFailure::save_first;
        evidence.scene_committed = true;
        if (decide(Request::run, evidence).action != Action::materialize_shell)
            return ContractFailure::materialize_first;
        evidence.shell_materialized = true;
        if (decide(Request::run, evidence).action != Action::build_project)
            return ContractFailure::build_first;
        evidence.build_inputs_current = true;
        evidence.output_exists = true;
        if (decide(Request::run, evidence).action != Action::launch_runtime)
            return ContractFailure::launch;
        evidence.build_active = true;
        if (decide(Request::run, evidence).action != Action::wait_for_build)
            return ContractFailure::active_build;
        evidence.build_active = false;
        evidence.runtime_active = true;
        if (decide(Request::run, evidence).action != Action::focus_runtime)
            return ContractFailure::focus_runtime;

        AttemptTracker attempts{};
        const std::uint64_t first = attempts.begin();
        if (first == 0u || attempts.begin() != 0u
            || !attempts.active() || !attempts.complete(first)
            || attempts.active() || attempts.completed_generation() != first)
        {
            return ContractFailure::generation_tracking;
        }

        Evidence strict{
            .project_selected = true,
            .scene_committed = true,
            .shell_materialized = true,
            .build_inputs_current = true,
            .output_exists = true,
            .require_generation_evidence = true};
        if (decide(Request::run, strict).reason
            != BlockReason::missing_generation_evidence)
        {
            return ContractFailure::missing_strict_evidence;
        }

        constexpr ProjectStamp project{0x4f13u, 7u};
        strict.generations.selected_project = project;
        strict.generations.committed_scene_generation = 11u;
        strict.generations.materialized_shell_project = project;
        strict.generations.build_inputs = {
            .project = project,
            .scene_generation = 10u,
            .input_generation = 13u};
        if (decide(Request::run, strict).action != Action::prepare_build_inputs)
            return ContractFailure::stale_inputs;

        strict.generations.build_inputs.scene_generation = 11u;
        strict.build_active = true;
        strict.generations.active_build = {
            .project = ProjectStamp{project.project_key, project.generation + 1u},
            .input_generation = 13u,
            .build_generation = 3u};
        if (decide(Request::run, strict).reason
            != BlockReason::active_build_generation_mismatch)
        {
            return ContractFailure::stale_active_build;
        }
        strict.generations.active_build.project = project;
        if (decide(Request::run, strict).action != Action::wait_for_build)
            return ContractFailure::active_build;
        strict.build_active = false;

        strict.generations.output_artifact = {
            .project = project,
            .input_generation = 12u,
            .build_generation = 3u,
            .artifact_generation = 17u,
            .verified = true};
        if (decide(Request::run, strict).reason
            != BlockReason::artifact_generation_mismatch)
        {
            return ContractFailure::stale_artifact;
        }

        strict.generations.output_artifact.input_generation = 13u;
        if (decide(Request::run, strict).action != Action::launch_runtime)
            return ContractFailure::launch;
        strict.runtime_active = true;
        strict.generations.runtime = {
            .project = project,
            .artifact_generation = 16u,
            .runtime_generation = 19u};
        if (decide(Request::run, strict).reason
            != BlockReason::runtime_generation_mismatch)
        {
            return ContractFailure::mismatched_runtime;
        }
        strict.generations.runtime.artifact_generation = 17u;
        if (decide(Request::run, strict).action != Action::focus_runtime)
            return ContractFailure::focus_runtime;

        BuildEvidenceTracker tracker{};
        const std::uint64_t build = tracker.begin(project, 13u);
        if (build == 0u || tracker.begin(project, 13u) != 0u
            || !tracker.active_evidence().valid_for(
                project, strict.generations.build_inputs)
            || !tracker.complete(build, 17u, true)
            || !tracker.artifact().valid_for(
                project, strict.generations.build_inputs))
        {
            return ContractFailure::artifact_tracking;
        }

        const std::uint64_t nextBuild = tracker.begin(project, 13u);
        if (nextBuild == 0u
            || tracker.complete(nextBuild, 17u, true)
            || !tracker.complete(nextBuild, 18u, true)
            || tracker.artifact().artifact_generation != 18u)
        {
            return ContractFailure::artifact_generation_reuse;
        }

        EvidenceLedger ledger{};
        if (!ledger.select_project(project.project_key)
            || ledger.decision(Request::run).action != Action::save_scene
            || !ledger.record_scene_commit(21u)
            || ledger.decision(Request::run).action != Action::materialize_shell
            || !ledger.record_shell_materialized()
            || ledger.decision(Request::run).action
                != Action::prepare_build_inputs
            || !ledger.observe_build_inputs(0x101u))
        {
            return ContractFailure::ledger_flow;
        }
        const std::uint64_t ledgerBuild = ledger.begin_build();
        if (ledgerBuild == 0u
            || ledger.decision(Request::run).action != Action::wait_for_build
            || ledger.observe_build_inputs(0x102u)
            || !ledger.complete_build(ledgerBuild, true)
            || ledger.decision(Request::run).action != Action::launch_runtime
            || !ledger.record_runtime_started()
            || ledger.decision(Request::run).action != Action::focus_runtime)
        {
            return ContractFailure::ledger_flow;
        }

        ledger.record_runtime_stopped();
        const auto artifactBeforeScene =
            ledger.evidence().generations.output_artifact;
        if (!ledger.record_scene_commit(22u)
            || !ledger.observe_build_inputs(0x101u)
            || ledger.evidence().generations.output_artifact
                .artifact_generation != artifactBeforeScene.artifact_generation
            || ledger.decision(Request::run).action != Action::launch_runtime)
        {
            return ContractFailure::ledger_scene_refresh;
        }

        if (!ledger.observe_build_inputs(0x202u)
            || ledger.decision(Request::run).action != Action::build_project)
        {
            return ContractFailure::ledger_flow;
        }
        const std::uint64_t staleBuild = ledger.begin_build();
        if (staleBuild == 0u
            || !ledger.select_project(project.project_key + 1u)
            || ledger.complete_build(staleBuild, true)
            || ledger.decision(Request::run).action != Action::save_scene)
        {
            return ContractFailure::ledger_project_switch;
        }

        const std::uint64_t closedProjectGeneration =
            ledger.evidence().generations.selected_project.generation;
        ledger.clear_project();
        if (ledger.evidence().project_selected
            || ledger.evidence().generations.selected_project.valid()
            || ledger.decision(Request::save).reason
                != BlockReason::no_project
            || !ledger.select_project(project.project_key + 1u)
            || ledger.evidence().generations.selected_project.generation
                <= closedProjectGeneration)
        {
            return ContractFailure::ledger_project_close;
        }

        if (!ledger.record_scene_commit(31u)
            || !ledger.record_shell_materialized()
            || !ledger.observe_build_inputs(0x303u))
        {
            return ContractFailure::ledger_failed_build;
        }
        const std::uint64_t failedBuild = ledger.begin_build();
        if (failedBuild == 0u
            || !ledger.fail_build(failedBuild)
            || ledger.complete_build(failedBuild, true)
            || ledger.decision(Request::run).action != Action::build_project)
        {
            return ContractFailure::ledger_failed_build;
        }

        const std::uint64_t recoveredBuild = ledger.begin_build();
        if (recoveredBuild == 0u
            || recoveredBuild == failedBuild
            || !ledger.complete_build(recoveredBuild, true)
            || ledger.decision(Request::run).action != Action::launch_runtime)
        {
            return ContractFailure::ledger_output_missing;
        }
        ledger.observe_output_missing();
        if (ledger.decision(Request::run).action != Action::build_project)
            return ContractFailure::ledger_output_missing;
        return ContractFailure::none;
    }
}
