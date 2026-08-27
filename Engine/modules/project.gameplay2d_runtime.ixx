/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module project.gameplay2d_runtime;

export import audio.playback_runtime;
export import platform.budgets;
export import project.actor2d_runtime;
export import project.input_profile;
export import project.sprite_animation;
export import project.tilemap_runtime;
export import render.canvas2d_scene;

export namespace epochengine::project_gameplay2d
{
    enum class InputFallbackPolicy : std::uint8_t
    {
        require_source_or_artifact,
        allow_legacy_default
    };

    struct ProjectPreparationRequest final
    {
        std::string project_id{};
        std::filesystem::path project_root{};
        project_tilemap_runtime::RuntimeRequest tilemap{};
        std::string input_profile_path{};
        std::string sprite_animation_path{};
        std::string audio_profile_path{};
        InputFallbackPolicy input_fallback{
            InputFallbackPolicy::require_source_or_artifact};
        bool request_physical_audio{true};
        bool allow_compatibility_audio{};
        bool require_sprite_animation{};
        bool require_audio{};
    };

    struct AudioProgram final
    {
        audio::PlaybackSessionRequest request{};
        audio::ClipId jump{};
        audio::ClipId land{};
        std::vector<audio::ClipId> autoplay{};
        bool authored{};

        [[nodiscard]] bool valid() const noexcept;
    };

    struct PreparationEvidence final
    {
        project_tilemap_runtime::Provenance tilemap_provenance{
            project_tilemap_runtime::Provenance::none};
        bool tilemap_library_changed{};
        bool input_compiled_from_source{};
        bool input_restored_from_artifact{};
        bool input_legacy_fallback{};
        bool animation_source_materialized{};
        bool animation_library_changed{};
        bool audio_compiled_from_source{};
        bool audio_restored_from_artifact{};
        std::string tilemap_diagnostic{};
        std::string input_diagnostic{};
        std::string animation_diagnostic{};
        std::string audio_diagnostic{};
    };

    struct PreparedProject final
    {
        std::uint64_t project_key{};
        canvas2d::scene_content::SceneContent base_scene{};
        project_actor2d::ActorConfiguration actor{};
        std::vector<project_actor2d::StaticCollision> collision{};
        project_input::CompiledInputProfile input{};
        std::optional<
            project_sprite_animation::CompiledSpriteAnimationArtifact>
            animations{};
        std::optional<AudioProgram> audio_program{};
        PreparationEvidence evidence{};

        [[nodiscard]] bool valid() const noexcept;
    };

    enum class PreparationCode : std::uint8_t
    {
        ready,
        invalid_request,
        tilemap_failure,
        input_failure,
        animation_failure,
        audio_failure,
        cross_project_artifact,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view preparation_code_name(
        PreparationCode code) noexcept
    {
        switch (code)
        {
        case PreparationCode::ready: return "ready";
        case PreparationCode::invalid_request: return "invalid_request";
        case PreparationCode::tilemap_failure: return "tilemap_failure";
        case PreparationCode::input_failure: return "input_failure";
        case PreparationCode::animation_failure: return "animation_failure";
        case PreparationCode::audio_failure: return "audio_failure";
        case PreparationCode::cross_project_artifact:
            return "cross_project_artifact";
        case PreparationCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct PreparationResult final
    {
        PreparationCode code{PreparationCode::invalid_request};
        PreparedProject project{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PreparationCode::ready && project.valid();
        }
    };

    [[nodiscard]] PreparationResult prepare_project(
        const ProjectPreparationRequest& request) noexcept;

    enum class SessionCode : std::uint8_t
    {
        ready,
        paused,
        degraded,
        closed,
        busy,
        invalid_project,
        invalid_input,
        input_failure,
        actor_failure,
        animation_failure,
        scene_failure,
        audio_failure,
        sequence_exhausted,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view session_code_name(
        SessionCode code) noexcept
    {
        switch (code)
        {
        case SessionCode::ready: return "ready";
        case SessionCode::paused: return "paused";
        case SessionCode::degraded: return "degraded";
        case SessionCode::closed: return "closed";
        case SessionCode::busy: return "busy";
        case SessionCode::invalid_project: return "invalid_project";
        case SessionCode::invalid_input: return "invalid_input";
        case SessionCode::input_failure: return "input_failure";
        case SessionCode::actor_failure: return "actor_failure";
        case SessionCode::animation_failure: return "animation_failure";
        case SessionCode::scene_failure: return "scene_failure";
        case SessionCode::audio_failure: return "audio_failure";
        case SessionCode::sequence_exhausted: return "sequence_exhausted";
        case SessionCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct SessionLimits final
    {
        std::uint32_t maximum_fixed_steps_per_frame{8u};
        double maximum_frame_seconds{0.25};
        std::uint32_t maximum_action_impulses{32u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_fixed_steps_per_frame != 0u
                && maximum_fixed_steps_per_frame <= 32u
                && maximum_frame_seconds > 0.0
                && maximum_frame_seconds <= 1.0
                && maximum_action_impulses != 0u
                && maximum_action_impulses <= 1'024u;
        }
    };

    struct SessionMetrics final
    {
        std::uint64_t sessions_opened{};
        std::uint64_t sessions_closed{};
        std::uint64_t input_frames{};
        std::uint64_t rejected_input_frames{};
        std::uint64_t fixed_steps{};
        std::uint64_t actor_events{};
        std::uint64_t animation_samples{};
        std::uint64_t animation_failures{};
        std::uint64_t scene_rebuilds{};
        std::uint64_t scene_failures{};
        std::uint64_t audio_triggers{};
        std::uint64_t audio_failures{};
        std::uint64_t audio_frames{};
        std::uint64_t pause_transitions{};
        std::uint64_t resets{};

        friend constexpr bool operator==(
            const SessionMetrics&,
            const SessionMetrics&) noexcept = default;
    };

    enum class RuntimeCostCode : std::uint8_t
    {
        ready,
        closed,
        invalid_scene,
        canvas_compile_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view runtime_cost_code_name(
        RuntimeCostCode code) noexcept
    {
        switch (code)
        {
        case RuntimeCostCode::ready: return "ready";
        case RuntimeCostCode::closed: return "closed";
        case RuntimeCostCode::invalid_scene: return "invalid_scene";
        case RuntimeCostCode::canvas_compile_failure:
            return "canvas_compile_failure";
        case RuntimeCostCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct RuntimeCostSnapshot final
    {
        RuntimeCostCode code{RuntimeCostCode::closed};
        std::uint64_t scene_revision{};
        std::uint32_t logical_canvas_width{};
        std::uint32_t logical_canvas_height{};
        std::uint64_t logical_canvas_pixels{};
        std::uint64_t source_sprites{};
        std::uint64_t source_texture_views{};
        std::uint64_t source_texture_bytes{};
        std::uint64_t emitted_sprites{};
        std::uint64_t emitted_batches{};
        std::uint64_t emitted_vertices{};
        std::uint64_t emitted_indices{};
        std::uint64_t material_count{};
        std::uint32_t maximum_sprites_per_batch{};
        std::uint32_t maximum_batches{};
        std::uint64_t canvas_rejections{};
        std::uint32_t collision_surfaces{};
        std::uint32_t peak_contacts_per_step{};
        std::uint64_t input_frames{};
        std::uint64_t rejected_input_frames{};
        std::uint64_t fixed_steps{};
        std::uint32_t audio_resident_clips{};
        std::uint32_t audio_bound_buses{};
        std::uint64_t audio_resident_bytes{};
        std::uint64_t audio_frames_mixed{};
        std::uint64_t audio_frames_submitted{};
        std::uint64_t audio_failures{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == RuntimeCostCode::ready;
        }
    };

    enum class RuntimeBudgetCode : std::uint8_t
    {
        within_budget,
        over_budget,
        invalid_snapshot,
        invalid_budget,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view runtime_budget_code_name(
        RuntimeBudgetCode code) noexcept
    {
        switch (code)
        {
        case RuntimeBudgetCode::within_budget: return "within_budget";
        case RuntimeBudgetCode::over_budget: return "over_budget";
        case RuntimeBudgetCode::invalid_snapshot: return "invalid_snapshot";
        case RuntimeBudgetCode::invalid_budget: return "invalid_budget";
        case RuntimeBudgetCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    enum class RuntimeBudgetViolation : std::uint32_t
    {
        canvas_pixels = 1u << 0u,
        logical_texture_bytes = 1u << 1u,
        visible_sprites = 1u << 2u,
        batches = 1u << 3u,
        collision_surfaces = 1u << 4u,
        resident_audio_bytes = 1u << 5u,
        canvas_rejections = 1u << 6u
    };

    [[nodiscard]] constexpr std::uint32_t runtime_budget_violation_bit(
        RuntimeBudgetViolation violation) noexcept
    {
        return static_cast<std::uint32_t>(violation);
    }

    struct RuntimeBudgetAssessment final
    {
        RuntimeBudgetCode code{RuntimeBudgetCode::invalid_snapshot};
        std::uint32_t violation_mask{};
        Canvas2DBudgets limits{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == RuntimeBudgetCode::within_budget;
        }

        [[nodiscard]] bool violates(
            RuntimeBudgetViolation violation) const noexcept
        {
            return (violation_mask
                & runtime_budget_violation_bit(violation)) != 0u;
        }
    };

    [[nodiscard]] RuntimeBudgetAssessment assess_runtime_costs(
        const RuntimeCostSnapshot& costs,
        const Budgets& budgets) noexcept;

    struct AdvanceResult final
    {
        SessionCode code{SessionCode::closed};
        project_actor2d::ActorState actor{};
        std::optional<project_sprite_animation::RuntimeSpriteSample> animation{};
        std::vector<project_actor2d::ActorEvent> events{};
        std::uint32_t fixed_steps{};
        std::uint64_t scene_revision{};
        audio::PlaybackRuntimeCode audio_code{
            audio::PlaybackRuntimeCode::success};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SessionCode::ready
                || code == SessionCode::paused
                || code == SessionCode::degraded;
        }
    };

    struct RuntimeSnapshot final
    {
        bool active{};
        SessionCode code{SessionCode::closed};
        project_actor2d::ActorState actor{};
        std::optional<project_sprite_animation::RuntimeSpriteSample> animation{};
        bool facing_left{};
        std::uint64_t scene_revision{};
        std::uint64_t last_input_frame{};
        SessionMetrics metrics{};
        std::string diagnostic{};
    };

    class Gameplay2DRuntime final
    {
    public:
        explicit Gameplay2DRuntime(SessionLimits limits = {}) noexcept;
        ~Gameplay2DRuntime();

        Gameplay2DRuntime(Gameplay2DRuntime&&) noexcept;
        Gameplay2DRuntime& operator=(Gameplay2DRuntime&&) noexcept;

        Gameplay2DRuntime(const Gameplay2DRuntime&) = delete;
        Gameplay2DRuntime& operator=(const Gameplay2DRuntime&) = delete;

        [[nodiscard]] SessionCode open(
            PreparedProject project,
            audio::PlaybackRuntime* process_audio_runtime = nullptr) noexcept;
        [[nodiscard]] AdvanceResult advance(
            const project_input::InputSnapshot& input,
            std::span<const project_input::ActionImpulse> impulses = {},
            double frame_seconds = 1.0 / 60.0) noexcept;
        [[nodiscard]] SessionCode set_paused(bool paused) noexcept;
        [[nodiscard]] SessionCode reset() noexcept;
        [[nodiscard]] SessionCode close() noexcept;

        [[nodiscard]] bool active() const noexcept;
        [[nodiscard]] SessionCode code() const noexcept;
        [[nodiscard]] const project_input::CompiledInputProfile*
            input_profile() const noexcept;
        [[nodiscard]] const canvas2d::scene_content::SceneContent*
            scene() const noexcept;
        [[nodiscard]] project_actor2d::ActorConfiguration
            actor_configuration() const noexcept;
        [[nodiscard]] project_actor2d::ActorState actor_state() const noexcept;
        [[nodiscard]] SessionMetrics metrics() const noexcept;
        [[nodiscard]] RuntimeCostSnapshot cost_snapshot() const;
        [[nodiscard]] RuntimeSnapshot snapshot() const;
        [[nodiscard]] std::string_view diagnostic() const noexcept;

    private:
        struct Impl;
        [[nodiscard]] static bool refresh_animation(Impl& state) noexcept;
        [[nodiscard]] static bool rebuild_scene(Impl& state) noexcept;
        [[nodiscard]] static audio::PlaybackRuntimeCode route_audio_events(
            Impl& state,
            std::span<const project_actor2d::ActorEvent> events) noexcept;
        std::unique_ptr<Impl> impl_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_project_accepted,
        session_open,
        deterministic_replay,
        input_mapping,
        collision,
        animation,
        audio_cue,
        canvas_publication,
        stale_input,
        pause_reset,
        repeated_session,
        bounded_session_soak,
        teardown,
        cost_snapshot_unavailable,
        cost_snapshot_canvas,
        cost_snapshot_physics,
        cost_snapshot_audio_clips,
        cost_snapshot_audio_buses,
        cost_snapshot_audio_bytes,
        cost_budget_portable,
        cost_budget_detection,
        metrics
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::invalid_project_accepted:
            return "invalid_project_accepted";
        case ContractFailure::session_open: return "session_open";
        case ContractFailure::deterministic_replay: return "deterministic_replay";
        case ContractFailure::input_mapping: return "input_mapping";
        case ContractFailure::collision: return "collision";
        case ContractFailure::animation: return "animation";
        case ContractFailure::audio_cue: return "audio_cue";
        case ContractFailure::canvas_publication: return "canvas_publication";
        case ContractFailure::stale_input: return "stale_input";
        case ContractFailure::pause_reset: return "pause_reset";
        case ContractFailure::repeated_session: return "repeated_session";
        case ContractFailure::bounded_session_soak:
            return "bounded_session_soak";
        case ContractFailure::teardown: return "teardown";
        case ContractFailure::cost_snapshot_unavailable:
            return "cost_snapshot_unavailable";
        case ContractFailure::cost_snapshot_canvas:
            return "cost_snapshot_canvas";
        case ContractFailure::cost_snapshot_physics:
            return "cost_snapshot_physics";
        case ContractFailure::cost_snapshot_audio_clips:
            return "cost_snapshot_audio_clips";
        case ContractFailure::cost_snapshot_audio_buses:
            return "cost_snapshot_audio_buses";
        case ContractFailure::cost_snapshot_audio_bytes:
            return "cost_snapshot_audio_bytes";
        case ContractFailure::cost_budget_portable:
            return "cost_budget_portable";
        case ContractFailure::cost_budget_detection:
            return "cost_budget_detection";
        case ContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
