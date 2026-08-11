/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module project.sprite_animation;

import asset.texture_artifact;
import authoring.document;
import project.asset_registry;

export namespace epochengine::project_sprite_animation
{
    inline constexpr std::string_view canonical_source_path{
        "Assets/Animations/sprite_animations.epochanim"};
    inline constexpr std::string_view canonical_artifact_path{
        "Library/Animations/sprite_animations.epochanimc"};
    inline constexpr std::uint32_t source_schema_version{1u};
    inline constexpr std::uint32_t artifact_schema_version{1u};

    struct AnimationArtifactDigest final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        friend constexpr auto operator<=>(
            const AnimationArtifactDigest&,
            const AnimationArtifactDigest&) noexcept = default;
    };

    template<typename Tag>
    struct StableId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr auto operator<=>(StableId, StableId) noexcept = default;
    };

    struct DocumentTag final {};
    struct AnimationTag final {};
    struct FrameTag final {};
    struct EventTag final {};

    using DocumentId = StableId<DocumentTag>;
    using AnimationId = StableId<AnimationTag>;
    using FrameId = StableId<FrameTag>;
    using EventId = StableId<EventTag>;

    struct UInt2 final
    {
        std::uint32_t x{};
        std::uint32_t y{};

        friend constexpr auto operator<=>(UInt2, UInt2) noexcept = default;
    };

    struct SourceRectangle final
    {
        std::uint32_t x{};
        std::uint32_t y{};
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width == 0u || height == 0u;
        }

        friend constexpr auto operator<=>(
            SourceRectangle, SourceRectangle) noexcept = default;
    };

    enum class PlaybackMode : std::uint8_t
    {
        once,
        loop,
        ping_pong
    };

    enum class TemporalDirection : std::uint8_t
    {
        forward,
        reverse,
        frozen
    };

    enum class EventSemantic : std::uint8_t
    {
        audio_cue,
        footstep,
        impact,
        custom
    };

    enum class EventDirection : std::uint8_t
    {
        forward_only,
        reverse_only,
        both
    };

    struct MaterialSource final
    {
        std::string logical_texture_path{};
        asset::texture::ContentHash texture_artifact_key{};
        std::uint64_t texture_artifact_revision{};
        std::uint32_t stable_material_key{};
        UInt2 texture_extent{};

        friend bool operator==(
            const MaterialSource& left,
            const MaterialSource& right) noexcept
        {
            return left.logical_texture_path == right.logical_texture_path
                && left.texture_artifact_key.words
                    == right.texture_artifact_key.words
                && left.texture_artifact_revision
                    == right.texture_artifact_revision
                && left.stable_material_key == right.stable_material_key
                && left.texture_extent == right.texture_extent;
        }
    };

    struct FrameSource final
    {
        FrameId id{};
        MaterialSource material{};
        SourceRectangle source_rectangle{};
        std::uint32_t duration_ticks{1u};
        std::int32_t pivot_x_q16{};
        std::int32_t pivot_y_q16{};

        friend bool operator==(
            const FrameSource&, const FrameSource&) noexcept = default;
    };

    struct AnimationEventSource final
    {
        EventId id{};
        FrameId frame{};
        std::uint32_t tick_in_frame{};
        EventSemantic semantic{EventSemantic::custom};
        EventDirection direction{EventDirection::forward_only};
        std::string logical_audio_path{};
        std::uint64_t payload{};

        friend bool operator==(
            const AnimationEventSource&,
            const AnimationEventSource&) noexcept = default;
    };

    struct AnimationSource final
    {
        AnimationId id{};
        std::string name{};
        PlaybackMode playback{PlaybackMode::loop};
        std::vector<FrameSource> frames{};
        std::vector<AnimationEventSource> events{};

        friend bool operator==(
            const AnimationSource&, const AnimationSource&) noexcept = default;
    };

    struct SpriteAnimationSource final
    {
        DocumentId id{};
        std::string name{};
        authoring::DocumentRevision revision{};
        std::vector<AnimationSource> animations{};

        friend bool operator==(
            const SpriteAnimationSource& left,
            const SpriteAnimationSource& right) noexcept
        {
            return left.id == right.id && left.name == right.name
                && left.revision.content.words
                    == right.revision.content.words
                && left.revision.sequence == right.revision.sequence
                && left.animations == right.animations;
        }
    };

    enum class ActorPose : std::uint8_t
    {
        idle,
        run,
        rise,
        fall
    };

    struct ActorMotionState final
    {
        double velocity_x{};
        double velocity_y{};
        bool grounded{};
        bool paused{};

        friend constexpr bool operator==(
            const ActorMotionState&,
            const ActorMotionState&) noexcept = default;
    };

    struct DefaultActorSheet final
    {
        MaterialSource material{};
        UInt2 tile_extent{16u, 16u};
        UInt2 grid{1u, 1u};
        UInt2 margin{};
        UInt2 spacing{};
        std::uint32_t tile_count{1u};

        [[nodiscard]] bool valid() const noexcept;
    };

    [[nodiscard]] ActorPose classify_actor_pose(
        const ActorMotionState& state) noexcept;
    [[nodiscard]] AnimationId default_actor_animation_id(
        ActorPose pose) noexcept;
    [[nodiscard]] SpriteAnimationSource make_default_actor_source(
        const DefaultActorSheet& sheet,
        std::uint64_t sequence = 1u);
    struct AnimationLimits final
    {
        std::uint32_t maximum_name_bytes{128u};
        std::uint32_t maximum_path_bytes{1'024u};
        std::uint32_t maximum_animations{1'024u};
        std::uint32_t maximum_frames{65'536u};
        std::uint32_t maximum_frames_per_animation{4'096u};
        std::uint32_t maximum_events{131'072u};
        std::uint32_t maximum_events_per_animation{8'192u};
        std::uint32_t maximum_events_per_tick{256u};
        std::uint32_t maximum_texture_dimension{32'768u};
        std::uint32_t maximum_frame_duration_ticks{1'000'000u};
        std::uint64_t maximum_animation_duration_ticks{4'000'000'000ull};
        std::uint64_t maximum_serialized_bytes{64ull * 1024ull * 1024ull};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_name_bytes != 0u && maximum_name_bytes <= 16'384u
                && maximum_path_bytes != 0u && maximum_path_bytes <= 65'536u
                && maximum_animations != 0u
                && maximum_animations <= 65'536u
                && maximum_frames != 0u
                && maximum_frames_per_animation != 0u
                && maximum_frames_per_animation <= maximum_frames
                && maximum_events != 0u
                && maximum_events_per_animation != 0u
                && maximum_events_per_animation <= maximum_events
                && maximum_events_per_tick != 0u
                && maximum_events_per_tick <= maximum_events_per_animation
                && maximum_texture_dimension != 0u
                && maximum_texture_dimension <= 1'048'576u
                && maximum_frame_duration_ticks != 0u
                && maximum_animation_duration_ticks != 0u
                && maximum_animation_duration_ticks
                    <= static_cast<std::uint64_t>(
                        (std::numeric_limits<std::int64_t>::max)() / 4)
                && maximum_serialized_bytes >= 512u
                && maximum_serialized_bytes <= 1024ull * 1024ull * 1024ull;
        }
    };

    enum class ValidationCode : std::uint8_t
    {
        ready,
        invalid_limits,
        invalid_document,
        invalid_revision,
        invalid_name,
        invalid_animation,
        invalid_frame,
        invalid_material,
        invalid_rectangle,
        invalid_event,
        invalid_path,
        duplicate_animation,
        duplicate_frame,
        duplicate_event,
        dangling_frame,
        noncanonical_order,
        animation_limit_exceeded,
        frame_limit_exceeded,
        event_limit_exceeded,
        duration_limit_exceeded,
        event_tick_limit_exceeded,
        content_hash_mismatch,
        invalid_project,
        invalid_artifact,
        unsupported_schema,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view validation_code_name(
        ValidationCode code) noexcept
    {
        switch (code)
        {
        case ValidationCode::ready: return "ready";
        case ValidationCode::invalid_limits: return "invalid_limits";
        case ValidationCode::invalid_document: return "invalid_document";
        case ValidationCode::invalid_revision: return "invalid_revision";
        case ValidationCode::invalid_name: return "invalid_name";
        case ValidationCode::invalid_animation: return "invalid_animation";
        case ValidationCode::invalid_frame: return "invalid_frame";
        case ValidationCode::invalid_material: return "invalid_material";
        case ValidationCode::invalid_rectangle: return "invalid_rectangle";
        case ValidationCode::invalid_event: return "invalid_event";
        case ValidationCode::invalid_path: return "invalid_path";
        case ValidationCode::duplicate_animation: return "duplicate_animation";
        case ValidationCode::duplicate_frame: return "duplicate_frame";
        case ValidationCode::duplicate_event: return "duplicate_event";
        case ValidationCode::dangling_frame: return "dangling_frame";
        case ValidationCode::noncanonical_order: return "noncanonical_order";
        case ValidationCode::animation_limit_exceeded:
            return "animation_limit_exceeded";
        case ValidationCode::frame_limit_exceeded:
            return "frame_limit_exceeded";
        case ValidationCode::event_limit_exceeded:
            return "event_limit_exceeded";
        case ValidationCode::duration_limit_exceeded:
            return "duration_limit_exceeded";
        case ValidationCode::event_tick_limit_exceeded:
            return "event_tick_limit_exceeded";
        case ValidationCode::content_hash_mismatch:
            return "content_hash_mismatch";
        case ValidationCode::invalid_project: return "invalid_project";
        case ValidationCode::invalid_artifact: return "invalid_artifact";
        case ValidationCode::unsupported_schema: return "unsupported_schema";
        case ValidationCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    [[nodiscard]] DocumentId stable_document_id(
        std::string_view stableName) noexcept;
    [[nodiscard]] AnimationId stable_animation_id(
        std::string_view stableName) noexcept;
    [[nodiscard]] FrameId stable_frame_id(
        std::string_view stableName) noexcept;
    [[nodiscard]] EventId stable_event_id(
        std::string_view stableName) noexcept;
    [[nodiscard]] authoring::ContentHash source_content_hash(
        const SpriteAnimationSource& source) noexcept;
    [[nodiscard]] ValidationCode validate_source(
        const SpriteAnimationSource& source,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] ValidationCode seal_source(
        SpriteAnimationSource& source,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    enum class CodecCode : std::uint8_t
    {
        ready,
        invalid_value,
        serialized_budget_exceeded,
        malformed_data,
        unsupported_version,
        integrity_failure,
        trailing_data,
        allocation_failure
    };

    struct EncodedBytes final
    {
        CodecCode code{CodecCode::invalid_value};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready && !bytes.empty();
        }
    };

    struct DecodedSource final
    {
        CodecCode code{CodecCode::malformed_data};
        SpriteAnimationSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready;
        }
    };

    [[nodiscard]] EncodedBytes serialize_source(
        const SpriteAnimationSource& source,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] DecodedSource deserialize_source(
        std::span<const std::byte> bytes,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    struct MaterialIdentity final
    {
        std::uint64_t project_key{};
        std::uint64_t texture_asset_key{};
        std::string logical_texture_path{};
        asset::texture::ContentHash texture_artifact_key{};
        std::uint64_t texture_artifact_revision{};
        std::uint32_t stable_material_key{};
        UInt2 texture_extent{};

        friend bool operator==(
            const MaterialIdentity& left,
            const MaterialIdentity& right) noexcept
        {
            return left.project_key == right.project_key
                && left.texture_asset_key == right.texture_asset_key
                && left.logical_texture_path == right.logical_texture_path
                && left.texture_artifact_key.words
                    == right.texture_artifact_key.words
                && left.texture_artifact_revision
                    == right.texture_artifact_revision
                && left.stable_material_key == right.stable_material_key
                && left.texture_extent == right.texture_extent;
        }
    };

    struct CompiledFrame final
    {
        FrameId id{};
        MaterialIdentity material{};
        SourceRectangle source_rectangle{};
        std::uint64_t first_tick{};
        std::uint32_t duration_ticks{};
        std::int32_t pivot_x_q16{};
        std::int32_t pivot_y_q16{};

        friend bool operator==(
            const CompiledFrame&, const CompiledFrame&) noexcept = default;
    };

    struct CompiledEvent final
    {
        EventId id{};
        FrameId frame{};
        std::uint64_t animation_tick{};
        std::uint32_t tick_in_frame{};
        EventSemantic semantic{EventSemantic::custom};
        EventDirection direction{EventDirection::forward_only};
        std::string logical_audio_path{};
        std::uint64_t audio_asset_key{};
        std::uint64_t payload{};

        friend bool operator==(
            const CompiledEvent&, const CompiledEvent&) noexcept = default;
    };

    struct CompiledAnimation final
    {
        AnimationId id{};
        std::string name{};
        PlaybackMode playback{PlaybackMode::loop};
        std::uint32_t first_frame{};
        std::uint32_t frame_count{};
        std::uint32_t first_event{};
        std::uint32_t event_count{};
        std::uint64_t total_ticks{};

        friend bool operator==(
            const CompiledAnimation&,
            const CompiledAnimation&) noexcept = default;
    };

    struct CompiledSpriteAnimationArtifact final
    {
        std::uint32_t schema_version{artifact_schema_version};
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        DocumentId document_id{};
        std::string name{};
        authoring::DocumentRevision source_revision{};
        AnimationArtifactDigest artifact_hash{};
        std::vector<CompiledAnimation> animations{};
        std::vector<CompiledFrame> frames{};
        std::vector<CompiledEvent> events{};

        friend bool operator==(
            const CompiledSpriteAnimationArtifact& left,
            const CompiledSpriteAnimationArtifact& right) noexcept
        {
            return left.schema_version == right.schema_version
                && left.project_key == right.project_key
                && left.asset_key == right.asset_key
                && left.document_id == right.document_id
                && left.name == right.name
                && left.source_revision.content.words
                    == right.source_revision.content.words
                && left.source_revision.sequence
                    == right.source_revision.sequence
                && left.artifact_hash == right.artifact_hash
                && left.animations == right.animations
                && left.frames == right.frames
                && left.events == right.events;
        }
    };

    struct CompileResult final
    {
        ValidationCode code{ValidationCode::invalid_artifact};
        CompiledSpriteAnimationArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ValidationCode::ready;
        }
    };

    [[nodiscard]] AnimationArtifactDigest artifact_content_hash(
        const CompiledSpriteAnimationArtifact& artifact) noexcept;
    [[nodiscard]] ValidationCode validate_artifact(
        const CompiledSpriteAnimationArtifact& artifact,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] CompileResult compile_artifact(
        std::string_view projectId,
        const SpriteAnimationSource& source,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] EncodedBytes serialize_artifact(
        const CompiledSpriteAnimationArtifact& artifact,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    struct DecodedArtifact final
    {
        CodecCode code{CodecCode::malformed_data};
        CompiledSpriteAnimationArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready;
        }
    };

    [[nodiscard]] DecodedArtifact deserialize_artifact(
        std::span<const std::byte> bytes,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    struct PlaybackRequest final
    {
        AnimationId animation{};
        std::int64_t tick{};
        TemporalDirection direction{TemporalDirection::forward};

        friend constexpr auto operator<=>(
            const PlaybackRequest&, const PlaybackRequest&) noexcept = default;
    };

    enum class SampleCode : std::uint8_t
    {
        ready,
        invalid_artifact,
        invalid_request,
        animation_not_found,
        event_budget_exceeded,
        allocation_failure
    };

    struct RuntimeSpriteSample final
    {
        SampleCode code{SampleCode::invalid_request};
        AnimationId animation{};
        FrameId frame{};
        MaterialIdentity material{};
        SourceRectangle source_rectangle{};
        std::uint64_t animation_tick{};
        std::uint32_t frame_tick{};
        std::int64_t cycle{};
        bool travel_forward{};
        bool terminal{};
        std::vector<CompiledEvent> events{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SampleCode::ready;
        }

        friend bool operator==(
            const RuntimeSpriteSample&, const RuntimeSpriteSample&) noexcept = default;
    };

    [[nodiscard]] RuntimeSpriteSample sample_animation(
        const CompiledSpriteAnimationArtifact& artifact,
        const PlaybackRequest& request,
        const AnimationLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    struct StoreLimits final
    {
        AnimationLimits animation{};
        project_assets::RegistryLimits registry{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return animation.valid() && registry.valid();
        }
    };

    enum class StoreCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_store,
        invalid_value,
        not_found,
        stale_revision,
        revision_conflict,
        directory_failure,
        read_failure,
        write_failure,
        integrity_failure,
        atomic_replace_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view store_code_name(
        StoreCode code) noexcept
    {
        switch (code)
        {
        case StoreCode::ready: return "ready";
        case StoreCode::unchanged: return "unchanged";
        case StoreCode::invalid_store: return "invalid_store";
        case StoreCode::invalid_value: return "invalid_value";
        case StoreCode::not_found: return "not_found";
        case StoreCode::stale_revision: return "stale_revision";
        case StoreCode::revision_conflict: return "revision_conflict";
        case StoreCode::directory_failure: return "directory_failure";
        case StoreCode::read_failure: return "read_failure";
        case StoreCode::write_failure: return "write_failure";
        case StoreCode::integrity_failure: return "integrity_failure";
        case StoreCode::atomic_replace_failure:
            return "atomic_replace_failure";
        case StoreCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }
    struct StoreMetrics final
    {
        std::uint64_t source_save_requests{};
        std::uint64_t source_saves{};
        std::uint64_t source_load_requests{};
        std::uint64_t source_loads{};
        std::uint64_t artifact_save_requests{};
        std::uint64_t artifact_saves{};
        std::uint64_t artifact_load_requests{};
        std::uint64_t artifact_loads{};
        std::uint64_t unchanged_writes{};
        std::uint64_t rejected_operations{};
        std::uint64_t bytes_read{};
        std::uint64_t bytes_written{};
    };

    struct StoredSource final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        authoring::DocumentRevision revision{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready || code == StoreCode::unchanged)
                && !storage_path.empty() && revision
                && serialized_bytes != 0u;
        }
    };

    struct LoadedSource final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        SpriteAnimationSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == StoreCode::ready && !storage_path.empty();
        }
    };

    struct StoredArtifact final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        AnimationArtifactDigest artifact_hash{};
        authoring::DocumentRevision source_revision{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready || code == StoreCode::unchanged)
                && !storage_path.empty() && !artifact_hash.empty()
                && source_revision && serialized_bytes != 0u;
        }
    };

    struct LoadedArtifact final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path storage_path{};
        CompiledSpriteAnimationArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == StoreCode::ready && !storage_path.empty();
        }
    };

    class ProjectSpriteAnimationStore final
    {
    public:
        ProjectSpriteAnimationStore(
            std::string projectId,
            std::filesystem::path projectRoot,
            StoreLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] std::filesystem::path source_path() const;
        [[nodiscard]] std::filesystem::path artifact_path() const;
        [[nodiscard]] const StoreLimits& limits() const noexcept;
        [[nodiscard]] StoreMetrics metrics() const noexcept;

        [[nodiscard]] StoredSource save_source(
            const SpriteAnimationSource& source) noexcept;
        [[nodiscard]] LoadedSource load_source() noexcept;
        [[nodiscard]] StoredArtifact publish_artifact(
            const CompiledSpriteAnimationArtifact& artifact) noexcept;
        [[nodiscard]] LoadedArtifact load_artifact() noexcept;

    private:
        [[nodiscard]] StoredSource reject_source(StoreCode code) noexcept;
        [[nodiscard]] LoadedSource reject_source_load(StoreCode code) noexcept;
        [[nodiscard]] StoredArtifact reject_artifact(StoreCode code) noexcept;
        [[nodiscard]] LoadedArtifact reject_artifact_load(
            StoreCode code) noexcept;

        std::string project_id_{};
        std::filesystem::path project_root_{};
        StoreLimits limits_{};
        std::uint64_t project_key_{};
        StoreMetrics metrics_{};
    };

    enum class PreparationCode : std::uint8_t
    {
        ready,
        invalid_request,
        invalid_store,
        source_failure,
        source_compile_failure,
        artifact_failure,
        no_animation_data,
        allocation_failure
    };

    struct PreparedSpriteAnimations final
    {
        PreparationCode code{PreparationCode::invalid_request};
        CompiledSpriteAnimationArtifact artifact{};
        bool source_materialized{};
        bool library_changed{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PreparationCode::ready
                && !artifact.artifact_hash.empty();
        }
    };

    [[nodiscard]] PreparedSpriteAnimations prepare_project_sprite_animations(
        std::string_view projectId,
        const std::filesystem::path& projectRoot,
        std::string_view declaredPath,
        const DefaultActorSheet* fallbackSheet = nullptr) noexcept;

    [[nodiscard]] constexpr std::string_view preparation_code_name(
        PreparationCode code) noexcept
    {
        switch (code)
        {
        case PreparationCode::ready: return "ready";
        case PreparationCode::invalid_request: return "invalid_request";
        case PreparationCode::invalid_store: return "invalid_store";
        case PreparationCode::source_failure: return "source_failure";
        case PreparationCode::source_compile_failure:
            return "source_compile_failure";
        case PreparationCode::artifact_failure: return "artifact_failure";
        case PreparationCode::no_animation_data: return "no_animation_data";
        case PreparationCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    enum class ContractFailure : std::uint8_t
    {
        none,
        stable_identity,
        valid_source_rejected,
        bounded_validation,
        source_roundtrip,
        malformed_source_accepted,
        artifact_roundtrip,
        artifact_integrity,
        material_identity,
        once_sampling,
        loop_sampling,
        ping_pong_sampling,
        reverse_sampling,
        frozen_sampling,
        event_sampling,
        deterministic_replay,
        temporary_root,
        source_store,
        artifact_store,
        stale_source,
        stale_artifact,
        malformed_store,
        compiled_only_restore,
        default_actor_animation,
        metrics
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::stable_identity: return "stable_identity";
        case ContractFailure::valid_source_rejected:
            return "valid_source_rejected";
        case ContractFailure::bounded_validation: return "bounded_validation";
        case ContractFailure::source_roundtrip: return "source_roundtrip";
        case ContractFailure::malformed_source_accepted:
            return "malformed_source_accepted";
        case ContractFailure::artifact_roundtrip: return "artifact_roundtrip";
        case ContractFailure::artifact_integrity: return "artifact_integrity";
        case ContractFailure::material_identity: return "material_identity";
        case ContractFailure::once_sampling: return "once_sampling";
        case ContractFailure::loop_sampling: return "loop_sampling";
        case ContractFailure::ping_pong_sampling:
            return "ping_pong_sampling";
        case ContractFailure::reverse_sampling: return "reverse_sampling";
        case ContractFailure::frozen_sampling: return "frozen_sampling";
        case ContractFailure::event_sampling: return "event_sampling";
        case ContractFailure::deterministic_replay:
            return "deterministic_replay";
        case ContractFailure::temporary_root: return "temporary_root";
        case ContractFailure::source_store: return "source_store";
        case ContractFailure::artifact_store: return "artifact_store";
        case ContractFailure::stale_source: return "stale_source";
        case ContractFailure::stale_artifact: return "stale_artifact";
        case ContractFailure::malformed_store: return "malformed_store";
        case ContractFailure::compiled_only_restore:
            return "compiled_only_restore";
        case ContractFailure::default_actor_animation:
            return "default_actor_animation";
        case ContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure
        project_sprite_animation_contract_failure() noexcept;
}
