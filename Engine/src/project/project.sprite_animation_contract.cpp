/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

module project.sprite_animation;

namespace epochengine::project_sprite_animation
{
    namespace
    {
        namespace fs = std::filesystem;

        [[nodiscard]] std::uint64_t next_contract_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(
                1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        struct ContractRoot final
        {
            fs::path path{};

            ContractRoot()
            {
                std::error_code error{};
                path = fs::temp_directory_path(error);
                if (error)
                {
                    path.clear();
                    return;
                }
                path /= "epoch_sprite_animation_contract_"
                    + std::to_string(next_contract_id());
                fs::remove_all(path, error);
                error.clear();
                fs::create_directories(path, error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                fs::remove_all(path, error);
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return !path.empty();
            }
        };

        [[nodiscard]] MaterialSource make_material()
        {
            MaterialSource material{};
            material.logical_texture_path = "Assets/Textures/hero.epochtexture";
            material.texture_artifact_key.words = {
                0x10u, 0x20u, 0x30u, 0x40u};
            material.texture_artifact_revision = 11u;
            material.stable_material_key = 0x4852'4f31u;
            material.texture_extent = {48u, 16u};
            return material;
        }

        [[nodiscard]] AnimationSource make_animation(
            std::string_view name,
            PlaybackMode playback)
        {
            AnimationSource animation{};
            animation.id = stable_animation_id(name);
            animation.name = std::string{name};
            animation.playback = playback;
            const MaterialSource material = make_material();
            for (std::uint32_t index = 0u; index < 3u; ++index)
            {
                const std::string frameName = std::string{name}
                    + ".frame." + std::to_string(index);
                animation.frames.push_back({
                    stable_frame_id(frameName),
                    material,
                    {index * 16u, 0u, 16u, 16u},
                    2u,
                    8 * 65'536,
                    16 * 65'536});
            }
            animation.events.push_back({
                stable_event_id(std::string{name} + ".reverse"),
                animation.frames[1].id,
                0u,
                EventSemantic::custom,
                EventDirection::reverse_only,
                {},
                0x22u});
            animation.events.push_back({
                stable_event_id(std::string{name} + ".footstep"),
                animation.frames[1].id,
                0u,
                EventSemantic::footstep,
                EventDirection::forward_only,
                "Assets/Audio/hero_step.wav",
                0x11u});
            animation.events.push_back({
                stable_event_id(std::string{name} + ".finish"),
                animation.frames[2].id,
                1u,
                EventSemantic::impact,
                EventDirection::forward_only,
                "Assets/Audio/hero_finish.wav",
                0x33u});
            return animation;
        }

        [[nodiscard]] SpriteAnimationSource make_source(
            std::uint64_t sequence)
        {
            SpriteAnimationSource source{};
            source.id = stable_document_id("contract.sprite.animations");
            source.name = "Contract Sprite Animations";
            source.revision.sequence = sequence;
            source.animations.push_back(make_animation(
                "ping", PlaybackMode::ping_pong));
            source.animations.push_back(make_animation(
                "once", PlaybackMode::once));
            source.animations.push_back(make_animation(
                "loop", PlaybackMode::loop));
            return source;
        }

        [[nodiscard]] const CompiledAnimation* find_animation(
            const CompiledSpriteAnimationArtifact& artifact,
            AnimationId id) noexcept
        {
            for (const CompiledAnimation& animation : artifact.animations)
            {
                if (animation.id == id)
                    return &animation;
            }
            return nullptr;
        }

        [[nodiscard]] bool write_malformed_file(
            const fs::path& path) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return false;
                constexpr char bytes[]{'E', 'P', 'S', 'P', 'R', 'S', 'R'};
                output.write(bytes, sizeof(bytes));
                output.flush();
                return static_cast<bool>(output);
            }
            catch (...)
            {
                return false;
            }
        }
    }

    ContractFailure project_sprite_animation_contract_failure() noexcept
    {
        const AnimationLimits limits{};
        const project_assets::RegistryLimits registryLimits{};
        if (stable_document_id("contract").value == 0u
            || stable_animation_id("contract").value == 0u
            || stable_frame_id("contract").value == 0u
            || stable_event_id("contract").value == 0u
            || stable_animation_id("contract")
                != stable_animation_id("contract")
            || stable_animation_id("contract").value
                == stable_frame_id("contract").value)
        {
            return ContractFailure::stable_identity;
        }

        SpriteAnimationSource source = make_source(7u);
        if (seal_source(source, limits, registryLimits)
                != ValidationCode::ready
            || validate_source(source, limits, registryLimits)
                != ValidationCode::ready
            || source.animations.size() != 3u
            || !source.revision)
        {
            return ContractFailure::valid_source_rejected;
        }

        SpriteAnimationSource duplicateFrame = source;
        duplicateFrame.revision.sequence = 8u;
        duplicateFrame.animations[1].frames[0].id =
            duplicateFrame.animations[0].frames[0].id;
        if (seal_source(duplicateFrame, limits, registryLimits)
            != ValidationCode::duplicate_frame)
        {
            return ContractFailure::bounded_validation;
        }
        SpriteAnimationSource invalidRectangle = source;
        invalidRectangle.revision.sequence = 8u;
        invalidRectangle.animations[0].frames[0].source_rectangle.width = 49u;
        if (seal_source(invalidRectangle, limits, registryLimits)
            != ValidationCode::invalid_rectangle)
        {
            return ContractFailure::bounded_validation;
        }
        AnimationLimits smallLimits = limits;
        smallLimits.maximum_animations = 2u;
        if (!smallLimits.valid()
            || validate_source(source, smallLimits, registryLimits)
                != ValidationCode::animation_limit_exceeded)
        {
            return ContractFailure::bounded_validation;
        }
        AnimationLimits eventLimits = limits;
        eventLimits.maximum_events_per_tick = 1u;
        if (!eventLimits.valid()
            || validate_source(source, eventLimits, registryLimits)
                != ValidationCode::event_tick_limit_exceeded)
        {
            return ContractFailure::bounded_validation;
        }
        AnimationLimits byteLimits = limits;
        byteLimits.maximum_serialized_bytes = 512u;
        const EncodedBytes bounded = serialize_source(
            source, byteLimits, registryLimits);
        if (bounded.code != CodecCode::serialized_budget_exceeded)
            return ContractFailure::bounded_validation;

        const EncodedBytes sourceBytes = serialize_source(
            source, limits, registryLimits);
        const EncodedBytes repeatedSourceBytes = serialize_source(
            source, limits, registryLimits);
        const DecodedSource restoredSource = sourceBytes
            ? deserialize_source(sourceBytes.bytes, limits, registryLimits)
            : DecodedSource{};
        if (!sourceBytes || !repeatedSourceBytes || !restoredSource
            || sourceBytes.bytes != repeatedSourceBytes.bytes
            || restoredSource.source != source)
        {
            return ContractFailure::source_roundtrip;
        }

        std::vector<std::byte> truncated = sourceBytes.bytes;
        truncated.resize(truncated.size() - 1u);
        std::vector<std::byte> corrupted = sourceBytes.bytes;
        corrupted.back() ^= std::byte{0x5au};
        std::vector<std::byte> trailing = sourceBytes.bytes;
        trailing.push_back(std::byte{0u});
        if (deserialize_source(truncated, limits, registryLimits)
            || deserialize_source(corrupted, limits, registryLimits)
            || deserialize_source(trailing, limits, registryLimits))
        {
            return ContractFailure::malformed_source_accepted;
        }

        const CompileResult compiled = compile_artifact(
            "sprite-animation-contract", source, limits, registryLimits);
        const EncodedBytes artifactBytes = compiled
            ? serialize_artifact(compiled.artifact, limits, registryLimits)
            : EncodedBytes{};
        const EncodedBytes repeatedArtifactBytes = compiled
            ? serialize_artifact(compiled.artifact, limits, registryLimits)
            : EncodedBytes{};
        const DecodedArtifact restoredArtifact = artifactBytes
            ? deserialize_artifact(
                artifactBytes.bytes, limits, registryLimits)
            : DecodedArtifact{};
        if (!compiled || !artifactBytes || !repeatedArtifactBytes
            || !restoredArtifact
            || artifactBytes.bytes != repeatedArtifactBytes.bytes
            || restoredArtifact.artifact != compiled.artifact)
        {
            return ContractFailure::artifact_roundtrip;
        }

        CompiledSpriteAnimationArtifact brokenArtifact = compiled.artifact;
        brokenArtifact.artifact_hash.words[0] ^= 1u;
        if (validate_artifact(brokenArtifact, limits, registryLimits)
            != ValidationCode::content_hash_mismatch)
        {
            return ContractFailure::artifact_integrity;
        }
        brokenArtifact = compiled.artifact;
        brokenArtifact.frames[0].material.texture_asset_key ^= 1u;
        brokenArtifact.artifact_hash = artifact_content_hash(brokenArtifact);
        if (validate_artifact(brokenArtifact, limits, registryLimits)
            != ValidationCode::invalid_material)
        {
            return ContractFailure::artifact_integrity;
        }
        std::vector<std::byte> corruptedArtifact = artifactBytes.bytes;
        corruptedArtifact.back() ^= std::byte{0x33u};
        if (deserialize_artifact(
                corruptedArtifact, limits, registryLimits))
        {
            return ContractFailure::artifact_integrity;
        }

        const std::uint64_t projectKey =
            project_assets::stable_project_identity(
                "sprite-animation-contract", registryLimits);
        const MaterialIdentity& material = compiled.artifact.frames[0].material;
        if (material.project_key != projectKey
            || material.texture_asset_key
                != project_assets::stable_asset_identity(
                    projectKey,
                    project_assets::AssetKind::texture,
                    material.logical_texture_path)
            || material.logical_texture_path
                != "Assets/Textures/hero.epochtexture"
            || material.texture_artifact_key.empty()
            || material.texture_artifact_revision != 11u
            || material.stable_material_key != 0x4852'4f31u)
        {
            return ContractFailure::material_identity;
        }

        const AnimationId onceId = stable_animation_id("once");
        const AnimationId loopId = stable_animation_id("loop");
        const AnimationId pingId = stable_animation_id("ping");
        const CompiledAnimation* once = find_animation(
            compiled.artifact, onceId);
        const CompiledAnimation* loop = find_animation(
            compiled.artifact, loopId);
        const CompiledAnimation* ping = find_animation(
            compiled.artifact, pingId);
        if (!once || !loop || !ping || once->total_ticks != 6u
            || loop->total_ticks != 6u || ping->total_ticks != 6u)
        {
            return ContractFailure::artifact_integrity;
        }

        const RuntimeSpriteSample onceEnd = sample_animation(
            compiled.artifact,
            {onceId, 100, TemporalDirection::forward},
            limits,
            registryLimits);
        const RuntimeSpriteSample onceExactEnd = sample_animation(
            compiled.artifact,
            {onceId, 5, TemporalDirection::forward},
            limits,
            registryLimits);
        if (!onceEnd || onceEnd.frame != stable_frame_id("once.frame.2")
            || onceEnd.animation_tick != 5u || !onceEnd.terminal
            || !onceEnd.travel_forward || !onceEnd.events.empty()
            || !onceExactEnd || !onceExactEnd.terminal
            || onceExactEnd.events.size() != 1u
            || onceExactEnd.events[0].semantic != EventSemantic::impact)
        {
            return ContractFailure::once_sampling;
        }

        const RuntimeSpriteSample loopWrapped = sample_animation(
            compiled.artifact,
            {loopId, 6, TemporalDirection::forward},
            limits,
            registryLimits);
        const RuntimeSpriteSample loopNegative = sample_animation(
            compiled.artifact,
            {loopId, -1, TemporalDirection::forward},
            limits,
            registryLimits);
        if (!loopWrapped || loopWrapped.animation_tick != 0u
            || loopWrapped.frame != stable_frame_id("loop.frame.0")
            || loopWrapped.cycle != 1
            || !loopNegative || loopNegative.animation_tick != 5u
            || loopNegative.frame != stable_frame_id("loop.frame.2")
            || loopNegative.cycle != -1)
        {
            return ContractFailure::loop_sampling;
        }

        const RuntimeSpriteSample pingReverseLeg = sample_animation(
            compiled.artifact,
            {pingId, 6, TemporalDirection::forward},
            limits,
            registryLimits);
        const RuntimeSpriteSample pingWrapped = sample_animation(
            compiled.artifact,
            {pingId, 10, TemporalDirection::forward},
            limits,
            registryLimits);
        if (!pingReverseLeg || pingReverseLeg.animation_tick != 4u
            || pingReverseLeg.frame != stable_frame_id("ping.frame.2")
            || pingReverseLeg.travel_forward
            || !pingWrapped || pingWrapped.animation_tick != 0u
            || pingWrapped.frame != stable_frame_id("ping.frame.0")
            || pingWrapped.cycle != 1 || !pingWrapped.travel_forward)
        {
            return ContractFailure::ping_pong_sampling;
        }

        const RuntimeSpriteSample reverseStart = sample_animation(
            compiled.artifact,
            {onceId, 0, TemporalDirection::reverse},
            limits,
            registryLimits);
        const RuntimeSpriteSample reverseEnd = sample_animation(
            compiled.artifact,
            {onceId, 5, TemporalDirection::reverse},
            limits,
            registryLimits);
        if (!reverseStart
            || reverseStart.frame != stable_frame_id("once.frame.2")
            || reverseStart.animation_tick != 5u
            || reverseStart.travel_forward
            || !reverseEnd
            || reverseEnd.frame != stable_frame_id("once.frame.0")
            || reverseEnd.animation_tick != 0u
            || reverseEnd.travel_forward)
        {
            return ContractFailure::reverse_sampling;
        }

        const RuntimeSpriteSample frozen = sample_animation(
            compiled.artifact,
            {onceId, 2, TemporalDirection::frozen},
            limits,
            registryLimits);
        if (!frozen || frozen.frame != stable_frame_id("once.frame.1")
            || frozen.animation_tick != 2u || !frozen.events.empty())
        {
            return ContractFailure::frozen_sampling;
        }

        const RuntimeSpriteSample forwardEvent = sample_animation(
            compiled.artifact,
            {onceId, 2, TemporalDirection::forward},
            limits,
            registryLimits);
        const RuntimeSpriteSample reverseEvent = sample_animation(
            compiled.artifact,
            {onceId, 3, TemporalDirection::reverse},
            limits,
            registryLimits);
        if (!forwardEvent || forwardEvent.events.size() != 1u
            || forwardEvent.events[0].semantic != EventSemantic::footstep
            || forwardEvent.events[0].logical_audio_path
                != "Assets/Audio/hero_step.wav"
            || forwardEvent.events[0].audio_asset_key == 0u
            || !reverseEvent || reverseEvent.events.size() != 1u
            || reverseEvent.events[0].semantic != EventSemantic::custom
            || reverseEvent.events[0].direction
                != EventDirection::reverse_only)
        {
            return ContractFailure::event_sampling;
        }

        for (std::int64_t tick = -20; tick <= 20; ++tick)
        {
            for (const TemporalDirection direction : {
                TemporalDirection::forward,
                TemporalDirection::reverse,
                TemporalDirection::frozen})
            {
                const PlaybackRequest request{pingId, tick, direction};
                const RuntimeSpriteSample first = sample_animation(
                    compiled.artifact, request, limits, registryLimits);
                const RuntimeSpriteSample second = sample_animation(
                    restoredArtifact.artifact,
                    request,
                    limits,
                    registryLimits);
                if (!first || first != second)
                    return ContractFailure::deterministic_replay;
            }
        }

        ContractRoot root{};
        if (!root.valid())
            return ContractFailure::temporary_root;
        ProjectSpriteAnimationStore store{
            "sprite-animation-contract", root.path};
        const StoredSource savedSource = store.save_source(source);
        const StoredSource unchangedSource = store.save_source(source);
        if (!savedSource || unchangedSource.code != StoreCode::unchanged
            || savedSource.storage_path.lexically_relative(root.path)
                != fs::path{canonical_source_path})
        {
            return ContractFailure::source_store;
        }
        ProjectSpriteAnimationStore reopened{
            "sprite-animation-contract", root.path};
        const LoadedSource loadedSource = reopened.load_source();
        if (!loadedSource || loadedSource.source != source)
            return ContractFailure::source_store;

        const StoredArtifact savedArtifact = store.publish_artifact(
            compiled.artifact);
        const StoredArtifact unchangedArtifact = store.publish_artifact(
            compiled.artifact);
        if (!savedArtifact || unchangedArtifact.code != StoreCode::unchanged
            || savedArtifact.storage_path.lexically_relative(root.path)
                != fs::path{canonical_artifact_path})
        {
            return ContractFailure::artifact_store;
        }
        const LoadedArtifact loadedArtifact = reopened.load_artifact();
        if (!loadedArtifact || loadedArtifact.artifact != compiled.artifact)
            return ContractFailure::artifact_store;
        const DefaultActorSheet defaultSheet{
            .material = make_material(),
            .tile_extent = {16u, 16u},
            .grid = {3u, 1u},
            .margin = {},
            .spacing = {},
            .tile_count = 3u};
        DefaultActorSheet oversizedDefaultSheet = defaultSheet;
        oversizedDefaultSheet.tile_extent.x =
            static_cast<std::uint32_t>(
                (std::numeric_limits<std::int32_t>::max)() / 32'768) + 1u;
        const SpriteAnimationSource defaultSource =
            make_default_actor_source(defaultSheet, 1u);
        const CompileResult defaultArtifact = compile_artifact(
            "sprite-animation-contract", defaultSource, limits, registryLimits);
        const RuntimeSpriteSample defaultRun = defaultArtifact
            ? sample_animation(
                defaultArtifact.artifact,
                {default_actor_animation_id(ActorPose::run), 4,
                    TemporalDirection::forward},
                limits,
                registryLimits)
            : RuntimeSpriteSample{};
        if (!defaultSheet.valid() || oversizedDefaultSheet.valid()
            || !defaultSource.revision
            || defaultSource.animations.size() != 4u
            || !defaultArtifact || !defaultRun
            || defaultRun.frame
                != stable_frame_id("actor.run.frame.1")
            || classify_actor_pose({0.0, 0.0, true, false})
                != ActorPose::idle
            || classify_actor_pose({1.0, 0.0, true, false})
                != ActorPose::run
            || classify_actor_pose({0.0, -1.0, false, false})
                != ActorPose::rise
            || classify_actor_pose({0.0, 1.0, false, false})
                != ActorPose::fall
            || classify_actor_pose({1.0, 0.0, true, true})
                != ActorPose::idle)
        {
            return ContractFailure::default_actor_animation;
        }

        ContractRoot preparedRoot{};
        if (!preparedRoot.valid())
            return ContractFailure::temporary_root;
        const PreparedSpriteAnimations prepared =
            prepare_project_sprite_animations(
                "sprite-animation-contract",
                preparedRoot.path,
                canonical_source_path,
                &defaultSheet);
        std::error_code removeError{};
        fs::remove(
            preparedRoot.path / fs::path{canonical_source_path},
            removeError);
        const PreparedSpriteAnimations compiledOnly =
            prepare_project_sprite_animations(
                "sprite-animation-contract",
                preparedRoot.path,
                canonical_source_path);
        const PreparedSpriteAnimations invalidDeclaration =
            prepare_project_sprite_animations(
                "sprite-animation-contract",
                preparedRoot.path,
                "Assets/Animations/not_canonical.epochanim");
        if (!prepared || !prepared.source_materialized
            || !prepared.library_changed || removeError
            || !compiledOnly
            || compiledOnly.artifact != prepared.artifact
            || compiledOnly.source_materialized
            || invalidDeclaration.code
                != PreparationCode::invalid_request)
        {
            return ContractFailure::compiled_only_restore;
        }
        CompiledSpriteAnimationArtifact noncanonicalArtifact =
            compiled.artifact;
        noncanonicalArtifact.name = "Not Derived From Source";
        noncanonicalArtifact.artifact_hash =
            artifact_content_hash(noncanonicalArtifact);
        if (store.publish_artifact(noncanonicalArtifact).code
            != StoreCode::invalid_value)
        {
            return ContractFailure::artifact_integrity;
        }

        SpriteAnimationSource older = source;
        older.revision.sequence = 6u;
        if (store.save_source(older).code != StoreCode::stale_revision)
            return ContractFailure::stale_source;
        SpriteAnimationSource conflicting = source;
        conflicting.name = "Conflicting Same Revision";
        if (seal_source(conflicting, limits, registryLimits)
                != ValidationCode::ready
            || store.save_source(conflicting).code
                != StoreCode::revision_conflict)
        {
            return ContractFailure::stale_source;
        }

        SpriteAnimationSource newer = source;
        newer.revision.sequence = 8u;
        newer.name = "Newer Contract Sprite Animations";
        if (seal_source(newer, limits, registryLimits)
                != ValidationCode::ready
            || !store.save_source(newer))
        {
            return ContractFailure::stale_source;
        }
        if (store.publish_artifact(compiled.artifact).code
                != StoreCode::stale_revision
            || reopened.load_artifact().code != StoreCode::stale_revision)
        {
            return ContractFailure::stale_artifact;
        }
        const CompileResult newerArtifact = compile_artifact(
            "sprite-animation-contract", newer, limits, registryLimits);
        if (!newerArtifact || !store.publish_artifact(newerArtifact.artifact))
            return ContractFailure::stale_artifact;

        const StoreMetrics metrics = store.metrics();
        if (metrics.source_save_requests < 5u || metrics.source_saves != 2u
            || metrics.artifact_save_requests < 4u
            || metrics.artifact_saves != 2u
            || metrics.unchanged_writes < 2u
            || metrics.rejected_operations < 3u
            || metrics.bytes_read == 0u || metrics.bytes_written == 0u)
        {
            return ContractFailure::metrics;
        }

        if (!write_malformed_file(reopened.source_path())
            || reopened.load_source().code != StoreCode::integrity_failure)
        {
            return ContractFailure::malformed_store;
        }

        return ContractFailure::none;
    }
}

#if defined(EPOCH_PROJECT_SPRITE_ANIMATION_CONTRACT_MAIN)
int main()
{
    return epochengine::project_sprite_animation::
        project_sprite_animation_contract_failure()
        == epochengine::project_sprite_animation::ContractFailure::none
        ? 0 : 1;
}
#endif
