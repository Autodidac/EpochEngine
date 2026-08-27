/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "engine.config.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module project.gameplay2d_runtime;

import asset.texture_artifact;
import asset.tilemap_artifact;
import project.audio_profile;
import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_tilemap;

namespace epochengine::project_gameplay2d
{
    namespace
    {
        namespace fs = std::filesystem;

        inline constexpr audio::ClipId compatibilityJumpCue{
            0x4550'4f43'485f'3244ull, 1u};
        inline constexpr audio::ClipId compatibilityLandCue{
            0x4550'4f43'485f'3244ull, 2u};

        [[nodiscard]] std::uint64_t stable_audio_session_id(
            std::string_view projectId) noexcept
        {
            std::uint64_t value{1469598103934665603ull};
            for (const unsigned char byte : projectId)
            {
                value ^= byte;
                value *= 1099511628211ull;
            }
            value ^= 0x4155'4449'4f5f'3244ull;
            value *= 1099511628211ull;
            return value == 0u ? 1u : value;
        }

        [[nodiscard]] audio::OwnedPcmClip make_compatibility_tone(
            audio::ClipId id,
            float frequency,
            float durationSeconds,
            float amplitude)
        {
            constexpr std::uint32_t sampleRate{48'000u};
            constexpr double tau{6.283185307179586476925286766559};
            const std::uint64_t frameCount = static_cast<std::uint64_t>(
                std::llround(
                    static_cast<double>(sampleRate) * durationSeconds));
            audio::OwnedPcmClip clip{
                .id = id,
                .format = {
                    .sample_format = audio::PcmSampleFormat::float32_interleaved,
                    .channel_layout = audio::PcmChannelLayout::mono,
                    .sample_rate = sampleRate},
                .frame_count = frameCount};
            clip.interleaved_samples.resize(frameCount);
            for (std::uint64_t frame = 0u; frame < frameCount; ++frame)
            {
                const double progress = frameCount <= 1u
                    ? 1.0
                    : static_cast<double>(frame)
                        / static_cast<double>(frameCount - 1u);
                const double phase = tau * static_cast<double>(frequency)
                    * static_cast<double>(frame)
                    / static_cast<double>(sampleRate);
                const double envelope = (1.0 - progress) * (1.0 - progress);
                clip.interleaved_samples[frame] = static_cast<float>(
                    std::sin(phase) * envelope * amplitude);
            }
            return clip;
        }

        [[nodiscard]] AudioProgram make_compatibility_audio(
            std::string_view projectId,
            bool requestPhysicalAudio)
        {
            AudioProgram program{};
            program.request.stable_session_id =
                stable_audio_session_id(projectId);
            program.request.request_physical_output = requestPhysicalAudio;
            program.request.cues.push_back({
                .clip = make_compatibility_tone(
                    compatibilityJumpCue, 660.0f, 0.095f, 0.24f),
                .gain = 1.0f,
                .looping = false});
            program.request.cues.push_back({
                .clip = make_compatibility_tone(
                    compatibilityLandCue, 165.0f, 0.075f, 0.20f),
                .gain = 1.0f,
                .looping = false});
            program.jump = compatibilityJumpCue;
            program.land = compatibilityLandCue;
            return program;
        }

        struct PreparedInput final
        {
            std::optional<project_input::CompiledInputProfile> artifact{};
            bool compiled_from_source{};
            bool restored_from_artifact{};
            bool legacy_fallback{};
            std::string diagnostic{};
        };

        [[nodiscard]] bool normalized_path_is(
            std::string_view candidate,
            std::string_view canonical) noexcept
        {
            try
            {
                return fs::path{candidate}.lexically_normal().generic_string()
                    == canonical;
            }
            catch (...)
            {
                return false;
            }
        }

        [[nodiscard]] PreparedInput prepare_input(
            const ProjectPreparationRequest& request) noexcept
        {
            PreparedInput result{};
            try
            {
                if (!normalized_path_is(
                        request.input_profile_path,
                        project_input::canonical_source_path))
                {
                    result.diagnostic = "unsupported project input path";
                    return result;
                }

                project_input::ProjectInputProfileStore store{
                    request.project_id, request.project_root};
                if (!store.valid())
                {
                    result.diagnostic = "invalid project input store";
                    return result;
                }

                std::error_code error{};
                const bool sourceExists = fs::exists(store.source_path(), error);
                if (error)
                {
                    result.diagnostic =
                        "project input source existence check failed";
                    return result;
                }
                if (sourceExists)
                {
                    const auto loaded = store.load_source();
                    if (!loaded)
                    {
                        result.diagnostic = std::string{"project input source "}
                            + std::string{project_input::store_code_name(
                                loaded.code)};
                        return result;
                    }
                    auto compiled = project_input::compile_profile(
                        request.project_id, loaded.source);
                    if (!compiled)
                    {
                        result.diagnostic = std::string{"project input compile "}
                            + std::string{project_input::validation_code_name(
                                compiled.code)};
                        return result;
                    }
                    const auto published = store.publish_artifact(
                        compiled.artifact);
                    if (!published)
                    {
                        result.diagnostic = std::string{
                            "project input artifact publication "}
                            + std::string{project_input::store_code_name(
                                published.code)};
                        return result;
                    }
                    result.artifact = std::move(compiled.artifact);
                    result.compiled_from_source = true;
                    result.diagnostic = published.code
                            == project_input::StoreCode::unchanged
                        ? "project input source compiled; Library artifact current"
                        : "project input source compiled and published";
                    return result;
                }

                const auto loadedArtifact = store.load_artifact();
                if (loadedArtifact)
                {
                    result.artifact = loadedArtifact.artifact;
                    result.restored_from_artifact = true;
                    result.diagnostic =
                        "compiled project input restored from Library";
                    return result;
                }

                if (request.input_fallback
                    != InputFallbackPolicy::allow_legacy_default)
                {
                    result.diagnostic =
                        "project input source and Library artifact are missing";
                    return result;
                }
                auto fallback = project_input::compile_profile(
                    request.project_id,
                    project_input::make_legacy_default_profile());
                if (!fallback)
                {
                    result.diagnostic =
                        "legacy default project input compile failed";
                    return result;
                }
                result.artifact = std::move(fallback.artifact);
                result.legacy_fallback = true;
                result.diagnostic =
                    "in-memory legacy project input fallback";
                return result;
            }
            catch (const std::bad_alloc&)
            {
                result.diagnostic = "project input allocation failure";
                return result;
            }
            catch (...)
            {
                result.diagnostic = "project input preparation failure";
                return result;
            }
        }

        [[nodiscard]] std::uint64_t collision_identity(
            const asset::tilemap::CollisionPrimitive& primitive,
            std::uint64_t sequence) noexcept
        {
            std::uint64_t value = 1469598103934665603ull;
            const auto mix = [&](std::uint64_t word) noexcept
            {
                value ^= word;
                value *= 1099511628211ull;
            };
            mix(primitive.layer.value);
            mix(primitive.palette_entry.value);
            mix(primitive.cell_coordinate.x);
            mix(primitive.cell_coordinate.y);
            mix(static_cast<std::uint64_t>(primitive.kind));
            mix(sequence);
            return value == 0u ? sequence + 1u : value;
        }

        [[nodiscard]] project_actor2d::StaticCollisionKind collision_kind(
            asset::tilemap::CollisionKind kind) noexcept
        {
            using AssetKind = asset::tilemap::CollisionKind;
            using RuntimeKind = project_actor2d::StaticCollisionKind;
            switch (kind)
            {
            case AssetKind::full_cell:
            case AssetKind::custom_box: return RuntimeKind::solid_box;
            case AssetKind::one_way_up: return RuntimeKind::one_way_up;
            case AssetKind::slope_up_right: return RuntimeKind::slope_up_right;
            case AssetKind::slope_down_right:
                return RuntimeKind::slope_down_right;
            case AssetKind::none: return RuntimeKind::unsupported;
            }
            return RuntimeKind::unsupported;
        }

        [[nodiscard]] std::vector<project_actor2d::StaticCollision>
        convert_collision(
            std::span<const asset::tilemap::CollisionPrimitive> collision)
        {
            std::vector<project_actor2d::StaticCollision> result{};
            result.reserve(collision.size());
            std::uint64_t sequence{};
            bool previousWasFullCell{};
            for (const auto& primitive : collision)
            {
                ++sequence;
                const auto kind = collision_kind(primitive.kind);
                if (kind == project_actor2d::StaticCollisionKind::unsupported)
                {
                    previousWasFullCell = false;
                    continue;
                }

                project_actor2d::StaticCollision converted{
                    .stable_id = collision_identity(primitive, sequence),
                    .bounds = {
                        primitive.world_bounds.x,
                        primitive.world_bounds.y,
                        primitive.world_bounds.width,
                        primitive.world_bounds.height},
                    .layer_bits = primitive.layer_bits,
                    .mask_bits = primitive.mask_bits,
                    .sensor = primitive.sensor,
                    .kind = kind};
                const bool fullCell = primitive.kind
                    == asset::tilemap::CollisionKind::full_cell;
                if (fullCell && previousWasFullCell && !result.empty())
                {
                    auto& previous = result.back();
                    const double previousRight =
                        previous.bounds.x + previous.bounds.width;
                    constexpr double adjacencyTolerance{1.0e-6};
                    if (previous.kind == converted.kind
                        && previous.layer_bits == converted.layer_bits
                        && previous.mask_bits == converted.mask_bits
                        && previous.sensor == converted.sensor
                        && std::abs(previous.bounds.y - converted.bounds.y)
                            <= adjacencyTolerance
                        && std::abs(
                            previous.bounds.height
                                - converted.bounds.height)
                            <= adjacencyTolerance
                        && std::abs(previousRight - converted.bounds.x)
                            <= adjacencyTolerance)
                    {
                        previous.bounds.width += converted.bounds.width;
                        continue;
                    }
                }
                result.push_back(converted);
                previousWasFullCell = fullCell;
            }
            return result;
        }

        [[nodiscard]] project_actor2d::ActorConfiguration actor_configuration(
            std::span<const canvas2d::tilemap_runtime::VisibleObject> objects)
            noexcept
        {
            project_actor2d::ActorConfiguration result{};
            for (const auto& object : objects)
            {
                if (object.type == "spawn" || object.name == "PlayerSpawn")
                {
                    result.spawn_x = object.position.x;
                    result.spawn_y = object.position.y;
                    break;
                }
            }
            return result;
        }

        [[nodiscard]] std::optional<
            project_sprite_animation::DefaultActorSheet>
        default_actor_sheet(
            const project_tilemap_runtime::PreparedTileMap& map)
        {
            namespace animation = project_sprite_animation;
            for (const auto& tileSet : map.tile_sets)
            {
                if (tileSet.dependency_index >= map.texture_dependencies.size()
                    || tileSet.tile_count == 0u)
                {
                    continue;
                }
                const auto& dependency =
                    map.texture_dependencies[tileSet.dependency_index];
                animation::DefaultActorSheet result{};
                result.material.logical_texture_path = dependency.logical_path;
                result.material.texture_artifact_key =
                    asset::texture::ContentHash{dependency.artifact_key};
                result.material.texture_artifact_revision =
                    dependency.artifact_revision;
                result.material.stable_material_key =
                    dependency.stable_material_key;
                result.material.texture_extent = {
                    dependency.texture_extent.x,
                    dependency.texture_extent.y};
                result.tile_extent = {
                    tileSet.tile_extent.x, tileSet.tile_extent.y};
                result.grid = {tileSet.grid.x, tileSet.grid.y};
                result.margin = {tileSet.margin.x, tileSet.margin.y};
                result.spacing = {tileSet.spacing.x, tileSet.spacing.y};
                result.tile_count = tileSet.tile_count;
                if (result.valid())
                    return result;
            }
            return std::nullopt;
        }

        [[nodiscard]] bool contains_clip(
            const audio::PlaybackSessionRequest& request,
            audio::ClipId id) noexcept
        {
            if (!id)
                return false;
            return std::ranges::any_of(
                request.cues,
                [id](const audio::PlaybackCueDefinition& cue) noexcept
                {
                    return cue.clip.id == id;
                });
        }

        [[nodiscard]] project_actor2d::FixedInputFrame actor_input(
            const project_input::CompiledInputProfile& profile,
            const project_input::ActionFrame& frame) noexcept
        {
            project_actor2d::FixedInputFrame result{};
            result.sequence = frame.frame_index;
            const auto action = [&](project_input::ActionSemantic semantic)
                -> const project_input::ActionValue*
            {
                return project_input::find_action(frame, semantic, profile);
            };
            if (const auto* value = action(
                    project_input::ActionSemantic::move_x))
            {
                result.move_x = static_cast<double>(value->value_q15)
                    / static_cast<double>(project_input::normalized_unit);
            }
            if (const auto* value = action(
                    project_input::ActionSemantic::move_y))
            {
                result.move_y = static_cast<double>(value->value_q15)
                    / static_cast<double>(project_input::normalized_unit);
            }
            if (const auto* value = action(
                    project_input::ActionSemantic::jump))
            {
                result.jump_pressed = value->pressed;
            }
            if (const auto* value = action(
                    project_input::ActionSemantic::pause))
            {
                result.pause_pressed = value->pressed;
            }
            if (const auto* value = action(
                    project_input::ActionSemantic::reset))
            {
                result.reset_pressed = value->pressed;
            }
            return result;
        }

        [[nodiscard]] canvas2d::scene_content::SceneContent compose_scene(
            const canvas2d::scene_content::SceneContent& base,
            const project_actor2d::ActorConfiguration& configuration,
            const project_actor2d::ActorState& actor,
            const project_sprite_animation::RuntimeSpriteSample* animation,
            bool facingLeft)
        {
            auto scene = base;
            std::uint32_t nextSpriteIndex{};
            std::uint64_t nextSequence{1u};
            std::int32_t actorLayer{};
            for (const auto& candidate : scene.sprites)
            {
                if (candidate.sprite.index != canvas2d::invalid_index)
                {
                    nextSpriteIndex = (std::max)(
                        nextSpriteIndex, candidate.sprite.index + 1u);
                }
                nextSequence = (std::max)(
                    nextSequence, candidate.stable_sequence + 1u);
                actorLayer = (std::max)(actorLayer, candidate.layer + 1);
            }

            canvas2d::SpriteSubmission sprite{};
            sprite.sprite = {nextSpriteIndex, 1u};
            sprite.material.stable_key = 0xe001'0001u;
            sprite.material.source = canvas2d::SpriteSourceKind::solid_color;
            sprite.material.alpha = canvas2d::SpriteAlphaMode::opaque;
            sprite.material.color_space = canvas2d::SpriteColorSpace::linear;
            sprite.transform.position = {
                static_cast<float>(actor.x), static_cast<float>(actor.y)};
            sprite.transform.size = {
                static_cast<float>(configuration.width),
                static_cast<float>(configuration.height)};
            sprite.tint = actor.paused
                ? canvas2d::LinearColor{0.95f, 0.72f, 0.18f, 1.0f}
                : (actor.grounded
                    ? canvas2d::LinearColor{0.14f, 0.82f, 0.72f, 1.0f}
                    : canvas2d::LinearColor{0.20f, 0.62f, 1.0f, 1.0f});

            if (animation && *animation
                && animation->material.texture_extent.x != 0u
                && animation->material.texture_extent.y != 0u)
            {
                const canvas2d::LogicalTextureReference logical{
                    animation->material.texture_asset_key,
                    animation->material.texture_artifact_revision};
                const auto matchingMaterial = std::ranges::find_if(
                    base.sprites,
                    [logical](const canvas2d::SpriteSubmission& candidate)
                    {
                        return candidate.material.source
                                == canvas2d::SpriteSourceKind::texture
                            && candidate.material.logical_texture == logical;
                    });
                if (matchingMaterial != base.sprites.end())
                {
                    sprite.material = matchingMaterial->material;
                    const float textureWidth = static_cast<float>(
                        animation->material.texture_extent.x);
                    const float textureHeight = static_cast<float>(
                        animation->material.texture_extent.y);
                    sprite.source_uv = {
                        static_cast<float>(animation->source_rectangle.x)
                            / textureWidth,
                        static_cast<float>(animation->source_rectangle.y)
                            / textureHeight,
                        static_cast<float>(animation->source_rectangle.width)
                            / textureWidth,
                        static_cast<float>(animation->source_rectangle.height)
                            / textureHeight};
                    sprite.tint = actor.paused
                        ? canvas2d::LinearColor{0.72f, 0.72f, 0.72f, 1.0f}
                        : canvas2d::LinearColor{1.0f, 1.0f, 1.0f, 1.0f};
                    sprite.flip_x = facingLeft;
                }
            }
            sprite.phase = canvas2d::SpritePhase::world;
            sprite.layer = actorLayer;
            sprite.stable_sequence = nextSequence;
            scene.sprites.push_back(std::move(sprite));
            scene.source_revision = base.source_revision
                ^ (actor.revision * 0x9e3779b97f4a7c15ull)
                ^ (facingLeft ? 0xd6e8feb86659fd93ull : 0u);
            if (animation && *animation)
                scene.source_revision ^= animation->frame.value;
            if (scene.source_revision == 0u)
                scene.source_revision = 1u;
            return scene;
        }
    }

    bool AudioProgram::valid() const noexcept
    {
        if (request.stable_session_id == 0u || request.cues.empty())
            return false;
        std::vector<audio::ClipId> identities{};
        try
        {
            identities.reserve(request.cues.size());
            for (const auto& cue : request.cues)
            {
                if (!cue.clip.id
                    || std::ranges::find(identities, cue.clip.id)
                        != identities.end())
                {
                    return false;
                }
                identities.push_back(cue.clip.id);
            }
            if (jump && !contains_clip(request, jump))
                return false;
            if (land && !contains_clip(request, land))
                return false;
            identities.clear();
            for (const auto cue : autoplay)
            {
                if (!contains_clip(request, cue)
                    || std::ranges::find(identities, cue)
                        != identities.end())
                {
                    return false;
                }
                identities.push_back(cue);
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool PreparedProject::valid() const noexcept
    {
        if (project_key == 0u || input.project_key != project_key
            || !base_scene.valid()
            || project_input::validate_compiled_profile(input)
                != project_input::ValidationCode::ready
            || project_actor2d::ActorRuntime::validate_configuration(actor)
                != project_actor2d::ResultCode::ready)
        {
            return false;
        }
        try
        {
            if (project_actor2d::ActorRuntime::validate_collision(
                    collision, actor.maximum_collision_surfaces)
                != project_actor2d::ResultCode::ready)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }
        if (animations
            && (animations->project_key != project_key
                || project_sprite_animation::validate_artifact(*animations)
                    != project_sprite_animation::ValidationCode::ready))
        {
            return false;
        }
        return !audio_program || audio_program->valid();
    }

    PreparationResult prepare_project(
        const ProjectPreparationRequest& request) noexcept
    {
        PreparationResult result{};
        try
        {
            if (request.project_id.empty() || request.project_root.empty()
                || request.tilemap.logical_path.empty()
                || request.input_profile_path.empty())
            {
                result.diagnostic =
                    "project identity, root, tilemap, and input are required";
                return result;
            }

            project_tilemap_runtime::ProjectTileMapRuntime tilemaps{
                request.project_id,
                request.project_root.generic_string()};
            auto preparedMap = tilemaps.prepare(request.tilemap);
            if (!preparedMap)
            {
                result.code = PreparationCode::tilemap_failure;
                result.diagnostic = std::string{"tilemap "}
                    + std::string{project_tilemap_runtime::runtime_code_name(
                        preparedMap.code)} + ": " + preparedMap.diagnostic;
                return result;
            }

            PreparedInput preparedInput = prepare_input(request);
            if (!preparedInput.artifact)
            {
                result.code = PreparationCode::input_failure;
                result.diagnostic = preparedInput.diagnostic;
                return result;
            }
            const std::uint64_t projectKey =
                preparedInput.artifact->project_key;
            if (projectKey == 0u)
            {
                result.code = PreparationCode::input_failure;
                result.diagnostic = "project input has no project identity";
                return result;
            }

            std::optional<
                project_sprite_animation::CompiledSpriteAnimationArtifact>
                animations{};
            project_sprite_animation::PreparedSpriteAnimations
                preparedAnimations{};
            if (!request.sprite_animation_path.empty())
            {
                const auto fallback = default_actor_sheet(preparedMap);
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_ANIMATION_EDITOR
                const auto* fallbackSource = fallback ? &*fallback : nullptr;
#else
                const project_sprite_animation::DefaultActorSheet*
                    fallbackSource = nullptr;
#endif
                preparedAnimations =
                    project_sprite_animation::prepare_project_sprite_animations(
                        request.project_id,
                        request.project_root,
                        request.sprite_animation_path,
                        fallbackSource);
                if (!preparedAnimations)
                {
                    if (request.require_sprite_animation)
                    {
                        result.code = PreparationCode::animation_failure;
                        result.diagnostic = preparedAnimations.diagnostic;
                        return result;
                    }
                }
                else
                {
                    if (preparedAnimations.artifact.project_key != projectKey)
                    {
                        result.code = PreparationCode::cross_project_artifact;
                        result.diagnostic =
                            "sprite animation belongs to another project";
                        return result;
                    }
                    animations = std::move(preparedAnimations.artifact);
                }
            }
            else if (request.require_sprite_animation)
            {
                result.code = PreparationCode::animation_failure;
                result.diagnostic = "project sprite animation is required";
                return result;
            }

            std::optional<AudioProgram> audioProgram{};
            project_audio::PreparedAudioSession preparedAudio{};
            if (!request.audio_profile_path.empty())
            {
                if (!normalized_path_is(
                        request.audio_profile_path,
                        project_audio::canonical_source_path))
                {
                    result.code = PreparationCode::audio_failure;
                    result.diagnostic = "unsupported project audio path";
                    return result;
                }
                project_audio::ProjectAudioProfileStore audioStore{
                    request.project_id, request.project_root};
                preparedAudio = audioStore.prepare(
                    request.request_physical_audio);
                if (!preparedAudio)
                {
                    if (request.require_audio)
                    {
                        result.code = PreparationCode::audio_failure;
                        result.diagnostic = preparedAudio.diagnostic;
                        return result;
                    }
                }
                else
                {
                    if (preparedAudio.session.project_key != projectKey)
                    {
                        result.code = PreparationCode::cross_project_artifact;
                        result.diagnostic =
                            "project audio belongs to another project";
                        return result;
                    }
                    AudioProgram program{};
                    program.request = std::move(preparedAudio.session.request);
                    if (const auto* jump = preparedAudio.session.find(
                            project_audio::CueSemantic::jump))
                    {
                        program.jump = jump->runtime_clip;
                    }
                    if (const auto* land = preparedAudio.session.find(
                            project_audio::CueSemantic::land))
                    {
                        program.land = land->runtime_clip;
                    }
                    program.autoplay = preparedAudio.session.autoplay_clips();
                    program.authored = true;
                    if (!program.valid())
                    {
                        if (request.require_audio)
                        {
                            result.code = PreparationCode::audio_failure;
                            result.diagnostic =
                                "compiled project audio is invalid";
                            return result;
                        }
                        preparedAudio.diagnostic =
                            "compiled project audio is invalid; audio disabled";
                    }
                    else
                    {
                        audioProgram = std::move(program);
                    }
                }
            }
            else if (request.require_audio)
            {
                result.code = PreparationCode::audio_failure;
                result.diagnostic = "project audio is required";
                return result;
            }
            else if (request.allow_compatibility_audio)
            {
                auto compatibility = make_compatibility_audio(
                    request.project_id, request.request_physical_audio);
                if (!compatibility.valid())
                {
                    result.code = PreparationCode::audio_failure;
                    result.diagnostic =
                        "procedural compatibility audio is invalid";
                    return result;
                }
                audioProgram = std::move(compatibility);
            }
            PreparedProject project{};
            project.project_key = projectKey;
            project.base_scene = std::move(preparedMap.scene);
            project.actor = actor_configuration(preparedMap.objects);
            project.collision = convert_collision(preparedMap.collision);
            project.input = std::move(*preparedInput.artifact);
            project.animations = std::move(animations);
            project.audio_program = std::move(audioProgram);
            project.evidence.tilemap_provenance = preparedMap.provenance;
            project.evidence.tilemap_library_changed =
                preparedMap.library_changed;
            project.evidence.input_compiled_from_source =
                preparedInput.compiled_from_source;
            project.evidence.input_restored_from_artifact =
                preparedInput.restored_from_artifact;
            project.evidence.input_legacy_fallback =
                preparedInput.legacy_fallback;
            project.evidence.animation_source_materialized =
                preparedAnimations.source_materialized;
            project.evidence.animation_library_changed =
                preparedAnimations.library_changed;
            project.evidence.audio_compiled_from_source =
                preparedAudio.compiled_from_source;
            project.evidence.audio_restored_from_artifact =
                preparedAudio.restored_from_artifact;
            project.evidence.tilemap_diagnostic =
                std::move(preparedMap.diagnostic);
            project.evidence.input_diagnostic =
                std::move(preparedInput.diagnostic);
            project.evidence.animation_diagnostic =
                std::move(preparedAnimations.diagnostic);
            project.evidence.audio_diagnostic =
                request.audio_profile_path.empty()
                    && request.allow_compatibility_audio
                ? "procedural compatibility audio prepared"
                : std::move(preparedAudio.diagnostic);
            if (!project.valid())
            {
                result.code = PreparationCode::cross_project_artifact;
                result.diagnostic =
                    "prepared gameplay project failed runtime validation";
                return result;
            }
            result.code = PreparationCode::ready;
            result.project = std::move(project);
            result.diagnostic = "project gameplay artifacts prepared";
            return result;
        }
        catch (const std::bad_alloc&)
        {
            result.code = PreparationCode::allocation_failure;
            result.diagnostic = "project gameplay preparation allocation failure";
            return result;
        }
        catch (...)
        {
            result.code = PreparationCode::invalid_request;
            result.diagnostic = "project gameplay preparation failed";
            return result;
        }
    }

    struct Gameplay2DRuntime::Impl final
    {
        explicit Impl(SessionLimits value) noexcept
            : limits(value)
        {
        }

        SessionLimits limits{};
        SessionCode code{SessionCode::closed};
        PreparedProject project{};
        std::unique_ptr<project_actor2d::ActorRuntime> actor{};
        audio::PlaybackRuntime* audio_runtime{};
        audio::PlaybackSessionHandle audio_session{};
        std::optional<project_sprite_animation::RuntimeSpriteSample>
            animation{};
        project_sprite_animation::AnimationId animation_id{};
        std::uint64_t animation_start_tick{};
        std::optional<canvas2d::scene_content::SceneContent> scene{};
        double accumulator{};
        std::uint64_t last_input_frame{};
        bool facing_left{};
        bool active{};
        SessionMetrics metrics{};
        std::string diagnostic{"gameplay runtime is closed"};
    };

    bool Gameplay2DRuntime::refresh_animation(
        Impl& state) noexcept
    {
        state.animation.reset();
        if (!state.project.animations || !state.actor)
            return true;
        try
        {
            const auto actor = state.actor->state();
            const auto pose = project_sprite_animation::classify_actor_pose({
                .velocity_x = actor.velocity_x,
                .velocity_y = actor.velocity_y,
                .grounded = actor.grounded,
                .paused = actor.paused});
            const auto animationId =
                project_sprite_animation::default_actor_animation_id(pose);
            if (animationId != state.animation_id
                || actor.fixed_tick < state.animation_start_tick)
            {
                state.animation_id = animationId;
                state.animation_start_tick = actor.fixed_tick;
            }
            const std::uint64_t relativeTick =
                actor.fixed_tick - state.animation_start_tick;
            const std::int64_t tick = relativeTick
                    > static_cast<std::uint64_t>(
                        (std::numeric_limits<std::int64_t>::max)())
                ? (std::numeric_limits<std::int64_t>::max)()
                : static_cast<std::int64_t>(relativeTick);
            auto sample = project_sprite_animation::sample_animation(
                *state.project.animations,
                {
                    .animation = animationId,
                    .tick = tick,
                    .direction = actor.paused
                        ? project_sprite_animation::TemporalDirection::frozen
                        : project_sprite_animation::TemporalDirection::forward});
            if (!sample)
            {
                ++state.metrics.animation_failures;
                state.diagnostic = "sprite animation sample rejected";
                return false;
            }
            state.animation = std::move(sample);
            ++state.metrics.animation_samples;
            return true;
        }
        catch (...)
        {
            ++state.metrics.animation_failures;
            state.diagnostic = "sprite animation sampling failed";
            return false;
        }
    }

    bool Gameplay2DRuntime::rebuild_scene(
        Impl& state) noexcept
    {
        if (!state.actor)
            return false;
        try
        {
            auto next = compose_scene(
                state.project.base_scene,
                state.actor->configuration(),
                state.actor->state(),
                state.animation ? &*state.animation : nullptr,
                state.facing_left);
            if (!next.valid())
            {
                ++state.metrics.scene_failures;
                state.diagnostic = "gameplay Canvas2D scene is invalid";
                return false;
            }
            state.scene = std::move(next);
            ++state.metrics.scene_rebuilds;
            return true;
        }
        catch (...)
        {
            ++state.metrics.scene_failures;
            state.diagnostic = "gameplay Canvas2D scene allocation failure";
            return false;
        }
    }

    [[nodiscard]] audio::ClipId event_cue(
        const AudioProgram& program,
        project_actor2d::ActorEventKind kind) noexcept
    {
        switch (kind)
        {
        case project_actor2d::ActorEventKind::jump_started:
            return program.jump;
        case project_actor2d::ActorEventKind::landed:
            return program.land;
        case project_actor2d::ActorEventKind::left_ground:
        case project_actor2d::ActorEventKind::reset:
        case project_actor2d::ActorEventKind::pause_changed:
            return {};
        }
        return {};
    }

    audio::PlaybackRuntimeCode Gameplay2DRuntime::route_audio_events(
        Impl& state,
        std::span<const project_actor2d::ActorEvent> events) noexcept
    {
        if (!state.project.audio_program || !state.audio_runtime
            || !state.audio_session)
        {
            return audio::PlaybackRuntimeCode::success;
        }
        auto code = audio::PlaybackRuntimeCode::success;
        for (const auto& event : events)
        {
            switch (event.kind)
            {
            case project_actor2d::ActorEventKind::pause_changed:
                code = state.audio_runtime->set_paused(
                    state.audio_session, event.enabled);
                break;
            case project_actor2d::ActorEventKind::reset:
                code = state.audio_runtime->reset(state.audio_session);
                break;
            case project_actor2d::ActorEventKind::jump_started:
            case project_actor2d::ActorEventKind::landed:
            case project_actor2d::ActorEventKind::left_ground:
                if (const auto cue = event_cue(
                        *state.project.audio_program, event.kind);
                    cue)
                {
                    code = state.audio_runtime->trigger(
                        state.audio_session, cue);
                    if (code == audio::PlaybackRuntimeCode::success)
                        ++state.metrics.audio_triggers;
                }
                break;
            }
            if (code != audio::PlaybackRuntimeCode::success)
            {
                ++state.metrics.audio_failures;
                state.diagnostic = std::string{"gameplay audio event "}
                    + std::string{audio::playback_runtime_code_name(code)};
                return code;
            }
        }
        return code;
    }

    Gameplay2DRuntime::Gameplay2DRuntime(SessionLimits limits) noexcept
    {
        try
        {
            impl_ = std::make_unique<Impl>(limits);
            if (!limits.valid())
            {
                impl_->code = SessionCode::invalid_project;
                impl_->diagnostic = "invalid gameplay session limits";
            }
        }
        catch (...)
        {
            impl_.reset();
        }
    }

    Gameplay2DRuntime::~Gameplay2DRuntime()
    {
        (void)close();
    }

    Gameplay2DRuntime::Gameplay2DRuntime(Gameplay2DRuntime&&) noexcept = default;
    Gameplay2DRuntime& Gameplay2DRuntime::operator=(
        Gameplay2DRuntime&& other) noexcept
    {
        if (this == &other)
            return *this;
        (void)close();
        impl_ = std::move(other.impl_);
        return *this;
    }

    SessionCode Gameplay2DRuntime::open(
        PreparedProject project,
        audio::PlaybackRuntime* processAudioRuntime) noexcept
    {
        if (!impl_)
            return SessionCode::allocation_failure;
        if (impl_->active)
            return SessionCode::busy;
        if (!impl_->limits.valid() || !project.valid())
        {
            impl_->code = SessionCode::invalid_project;
            impl_->diagnostic = "invalid prepared gameplay project";
            return impl_->code;
        }
        if (project.audio_program && !processAudioRuntime)
        {
            impl_->code = SessionCode::audio_failure;
            impl_->diagnostic =
                "project audio requires a process-owned playback runtime";
            return impl_->code;
        }

        try
        {
            auto actor = std::make_unique<project_actor2d::ActorRuntime>(
                project.actor);
            const auto collisionCode = actor->replace_collision(
                project.collision);
            if (collisionCode != project_actor2d::ResultCode::ready)
            {
                impl_->code = SessionCode::actor_failure;
                impl_->diagnostic = std::string{"actor collision "}
                    + std::string{project_actor2d::result_code_name(
                        collisionCode)};
                return impl_->code;
            }

            audio::PlaybackSessionHandle audioSession{};
            if (project.audio_program)
            {
                auto opened = processAudioRuntime->open_session(
                    project.audio_program->request);
                if (!opened)
                {
                    impl_->code = SessionCode::audio_failure;
                    impl_->diagnostic = std::string{"audio session "}
                        + std::string{audio::playback_runtime_code_name(
                            opened.code)};
                    return impl_->code;
                }
                audioSession = opened.handle;
                for (const auto cue : project.audio_program->autoplay)
                {
                    const auto triggered = processAudioRuntime->trigger(
                        audioSession, cue);
                    if (triggered != audio::PlaybackRuntimeCode::success)
                    {
                        (void)processAudioRuntime->close_session(audioSession);
                        impl_->code = SessionCode::audio_failure;
                        impl_->diagnostic =
                            std::string{"project audio autoplay "}
                            + std::string{audio::playback_runtime_code_name(
                                triggered)};
                        return impl_->code;
                    }
                }
            }

            impl_->project = std::move(project);
            impl_->actor = std::move(actor);
            impl_->audio_runtime = processAudioRuntime;
            impl_->audio_session = audioSession;
            impl_->animation.reset();
            impl_->animation_id = {};
            impl_->animation_start_tick = 0u;
            impl_->scene.reset();
            impl_->accumulator = 0.0;
            impl_->last_input_frame = 0u;
            impl_->facing_left = false;
            impl_->active = true;
            impl_->code = SessionCode::ready;
            impl_->diagnostic = "gameplay session ready";
            ++impl_->metrics.sessions_opened;
            if (!refresh_animation(*impl_) || !rebuild_scene(*impl_))
            {
                impl_->code = SessionCode::degraded;
            }
            return impl_->code;
        }
        catch (const std::bad_alloc&)
        {
            impl_->code = SessionCode::allocation_failure;
            impl_->diagnostic = "gameplay session allocation failure";
            return impl_->code;
        }
        catch (...)
        {
            impl_->code = SessionCode::invalid_project;
            impl_->diagnostic = "gameplay session open failed";
            return impl_->code;
        }
    }

    AdvanceResult Gameplay2DRuntime::advance(
        const project_input::InputSnapshot& input,
        std::span<const project_input::ActionImpulse> impulses,
        double frameSeconds) noexcept
    {
        AdvanceResult result{};
        if (!impl_ || !impl_->active || !impl_->actor)
            return result;
        result.actor = impl_->actor->state();
        if (input.frame_index == 0u
            || input.frame_index <= impl_->last_input_frame
            || !std::isfinite(frameSeconds) || frameSeconds < 0.0
            || impulses.size() > impl_->limits.maximum_action_impulses)
        {
            ++impl_->metrics.rejected_input_frames;
            impl_->code = SessionCode::invalid_input;
            impl_->diagnostic = "invalid or stale gameplay input frame";
            result.code = impl_->code;
            return result;
        }

        try
        {
            auto actions = project_input::evaluate_action_frame(
                impl_->project.input, input);
            if (!actions)
            {
                ++impl_->metrics.rejected_input_frames;
                impl_->code = SessionCode::input_failure;
                impl_->diagnostic = "project input evaluation rejected";
                result.code = impl_->code;
                return result;
            }
            const auto injection = project_input::inject_action_impulses(
                impl_->project.input, actions, impulses);
            if (injection != project_input::InjectionCode::ready)
            {
                ++impl_->metrics.rejected_input_frames;
                impl_->code = SessionCode::input_failure;
                impl_->diagnostic = std::string{"project input impulse "}
                    + std::string{project_input::injection_code_name(injection)};
                result.code = impl_->code;
                return result;
            }

            const double stepSeconds = 1.0
                / static_cast<double>(
                    impl_->actor->configuration().fixed_steps_per_second);
            const double boundedFrame = (std::min)(
                frameSeconds, impl_->limits.maximum_frame_seconds);
            double nextAccumulator = impl_->accumulator + boundedFrame;
            std::uint32_t fixedSteps = static_cast<std::uint32_t>(
                std::floor(nextAccumulator / stepSeconds));
            fixedSteps = (std::min)(
                fixedSteps, impl_->limits.maximum_fixed_steps_per_frame);
            nextAccumulator -= stepSeconds * static_cast<double>(fixedSteps);

            const auto actorInput = actor_input(impl_->project.input, actions);
            bool facingLeft = impl_->facing_left;
            if (actorInput.move_x < -0.05)
                facingLeft = true;
            else if (actorInput.move_x > 0.05)
                facingLeft = false;

            auto advanced = impl_->actor->advance(actorInput, fixedSteps);
            if (!advanced)
            {
                ++impl_->metrics.rejected_input_frames;
                impl_->code = SessionCode::actor_failure;
                impl_->diagnostic = std::string{"actor advance "}
                    + std::string{project_actor2d::result_code_name(
                        advanced.code)};
                result.code = impl_->code;
                result.actor = impl_->actor->state();
                return result;
            }

            impl_->last_input_frame = input.frame_index;
            impl_->accumulator = advanced.code
                    == project_actor2d::ResultCode::paused
                ? 0.0 : nextAccumulator;
            impl_->facing_left = facingLeft;
            ++impl_->metrics.input_frames;
            impl_->metrics.fixed_steps += advanced.steps_committed;
            impl_->metrics.actor_events += advanced.events.size();

            auto audioCode = route_audio_events(*impl_, advanced.events);
            const bool animationReady = refresh_animation(*impl_);
            const bool sceneReady = rebuild_scene(*impl_);
            if (impl_->audio_runtime && impl_->audio_session
                && boundedFrame > 0.0)
            {
                const auto audioFrame = impl_->audio_runtime->advance(
                    impl_->audio_session,
                    (std::max)(boundedFrame, 0.000'001));
                audioCode = audioFrame.code;
                if (audioFrame)
                    ++impl_->metrics.audio_frames;
                else
                {
                    ++impl_->metrics.audio_failures;
                    impl_->diagnostic = std::string{"gameplay audio frame "}
                        + std::string{audio::playback_runtime_code_name(
                            audioFrame.code)};
                }
            }

            const bool audioReady = audioCode
                    == audio::PlaybackRuntimeCode::success
                || audioCode
                    == audio::PlaybackRuntimeCode::physical_queue_saturated;
            if (!animationReady || !sceneReady || !audioReady)
                impl_->code = SessionCode::degraded;
            else if (advanced.code == project_actor2d::ResultCode::paused)
                impl_->code = SessionCode::paused;
            else
            {
                impl_->code = SessionCode::ready;
                impl_->diagnostic = "gameplay frame ready";
            }

            result.code = impl_->code;
            result.actor = advanced.state;
            result.animation = impl_->animation;
            result.events = std::move(advanced.events);
            result.fixed_steps = advanced.steps_committed;
            result.scene_revision = impl_->scene
                ? impl_->scene->source_revision : 0u;
            result.audio_code = audioCode;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            impl_->code = SessionCode::allocation_failure;
            impl_->diagnostic = "gameplay frame allocation failure";
            result.code = impl_->code;
            return result;
        }
        catch (...)
        {
            impl_->code = SessionCode::input_failure;
            impl_->diagnostic = "gameplay frame failed";
            result.code = impl_->code;
            return result;
        }
    }

    SessionCode Gameplay2DRuntime::set_paused(bool paused) noexcept
    {
        if (!impl_ || !impl_->active || !impl_->actor)
            return SessionCode::closed;
        const auto code = impl_->actor->set_paused(paused);
        if (code != project_actor2d::ResultCode::ready
            && code != project_actor2d::ResultCode::paused)
        {
            impl_->code = SessionCode::actor_failure;
            return impl_->code;
        }
        bool degraded{};
        if (impl_->audio_runtime && impl_->audio_session)
        {
            const auto audioCode = impl_->audio_runtime->set_paused(
                impl_->audio_session, paused);
            if (audioCode != audio::PlaybackRuntimeCode::success)
            {
                ++impl_->metrics.audio_failures;
                degraded = true;
            }
        }
        ++impl_->metrics.pause_transitions;
        degraded = !refresh_animation(*impl_) || !rebuild_scene(*impl_)
            || degraded;
        impl_->code = degraded
            ? SessionCode::degraded
            : (paused ? SessionCode::paused : SessionCode::ready);
        return impl_->code;
    }

    SessionCode Gameplay2DRuntime::reset() noexcept
    {
        if (!impl_ || !impl_->active || !impl_->actor)
            return SessionCode::closed;
        const auto code = impl_->actor->reset();
        if (code != project_actor2d::ResultCode::ready)
        {
            impl_->code = SessionCode::actor_failure;
            return impl_->code;
        }
        impl_->accumulator = 0.0;
        impl_->animation_id = {};
        impl_->animation_start_tick = 0u;
        bool degraded{};
        if (impl_->audio_runtime && impl_->audio_session)
        {
            const auto audioCode = impl_->audio_runtime->reset(
                impl_->audio_session);
            if (audioCode != audio::PlaybackRuntimeCode::success)
            {
                ++impl_->metrics.audio_failures;
                degraded = true;
            }
        }
        ++impl_->metrics.resets;
        degraded = !refresh_animation(*impl_) || !rebuild_scene(*impl_)
            || degraded;
        impl_->code = degraded ? SessionCode::degraded : SessionCode::ready;
        return impl_->code;
    }

    SessionCode Gameplay2DRuntime::close() noexcept
    {
        if (!impl_ || !impl_->active)
            return SessionCode::closed;
        if (impl_->audio_runtime && impl_->audio_session)
        {
            const auto code = impl_->audio_runtime->close_session(
                impl_->audio_session);
            if (code != audio::PlaybackRuntimeCode::success)
                ++impl_->metrics.audio_failures;
        }
        impl_->audio_session = {};
        impl_->audio_runtime = nullptr;
        impl_->scene.reset();
        impl_->animation.reset();
        impl_->actor.reset();
        impl_->project = {};
        impl_->accumulator = 0.0;
        impl_->last_input_frame = 0u;
        impl_->active = false;
        impl_->code = SessionCode::closed;
        impl_->diagnostic = "gameplay session closed";
        ++impl_->metrics.sessions_closed;
        return impl_->code;
    }

    bool Gameplay2DRuntime::active() const noexcept
    {
        return impl_ && impl_->active;
    }

    SessionCode Gameplay2DRuntime::code() const noexcept
    {
        return impl_ ? impl_->code : SessionCode::allocation_failure;
    }

    const project_input::CompiledInputProfile*
    Gameplay2DRuntime::input_profile() const noexcept
    {
        return impl_ && impl_->active ? &impl_->project.input : nullptr;
    }

    const canvas2d::scene_content::SceneContent*
    Gameplay2DRuntime::scene() const noexcept
    {
        return impl_ && impl_->active && impl_->scene
            ? &*impl_->scene : nullptr;
    }

    project_actor2d::ActorConfiguration
    Gameplay2DRuntime::actor_configuration() const noexcept
    {
        return impl_ && impl_->actor
            ? impl_->actor->configuration()
            : project_actor2d::ActorConfiguration{};
    }

    project_actor2d::ActorState Gameplay2DRuntime::actor_state() const noexcept
    {
        return impl_ && impl_->actor
            ? impl_->actor->state() : project_actor2d::ActorState{};
    }

    SessionMetrics Gameplay2DRuntime::metrics() const noexcept
    {
        return impl_ ? impl_->metrics : SessionMetrics{};
    }

    RuntimeCostSnapshot Gameplay2DRuntime::cost_snapshot() const
    {
        RuntimeCostSnapshot result{};
        if (!impl_)
        {
            result.code = RuntimeCostCode::allocation_failure;
            result.diagnostic = "gameplay runtime is unavailable";
            return result;
        }
        if (!impl_->active)
        {
            result.code = RuntimeCostCode::closed;
            result.diagnostic = "gameplay runtime is closed";
            return result;
        }
        if (!impl_->scene || !impl_->actor)
        {
            result.code = RuntimeCostCode::invalid_scene;
            result.diagnostic = "gameplay runtime has no current scene";
            return result;
        }

        try
        {
            const auto& scene = *impl_->scene;
            const auto actorMetrics = impl_->actor->metrics();
            result.scene_revision = scene.source_revision;
            result.logical_canvas_width = scene.project.logical_canvas.width;
            result.logical_canvas_height = scene.project.logical_canvas.height;
            result.logical_canvas_pixels =
                static_cast<std::uint64_t>(result.logical_canvas_width)
                * static_cast<std::uint64_t>(result.logical_canvas_height);
            result.source_sprites = scene.sprites.size();
            result.source_texture_views =
                scene.resources.bindings.textures.size();
            for (const auto& texture : scene.resources.bindings.textures)
            {
                const std::uint64_t bytes =
                    static_cast<std::uint64_t>(texture.pixels.size())
                    * sizeof(canvas2d::cpu::Rgba8);
                const std::uint64_t available =
                    (std::numeric_limits<std::uint64_t>::max)()
                    - result.source_texture_bytes;
                result.source_texture_bytes += (std::min)(bytes, available);
            }
            result.maximum_sprites_per_batch =
                scene.project.maximum_sprites_per_batch;
            result.maximum_batches = scene.project.maximum_batches;
            result.collision_surfaces = actorMetrics.collision_surfaces;
            result.peak_contacts_per_step =
                actorMetrics.peak_contacts_per_step;
            result.input_frames = impl_->metrics.input_frames;
            result.rejected_input_frames =
                impl_->metrics.rejected_input_frames;
            result.fixed_steps = impl_->metrics.fixed_steps;
            result.audio_failures = impl_->metrics.audio_failures;

            const auto plan = canvas2d::compile_canvas2d_submission(
                scene.submission(
                    impl_->last_input_frame == 0u
                        ? 1u : impl_->last_input_frame),
                scene.project.logical_canvas);
            if (!plan)
            {
                result.code = RuntimeCostCode::canvas_compile_failure;
                result.diagnostic =
                    "current gameplay Canvas2D scene did not compile";
                return result;
            }

            const auto& spriteMetrics = plan.sprites.diagnostics;
            result.emitted_sprites = spriteMetrics.emitted_sprites;
            result.emitted_batches = spriteMetrics.emitted_batches;
            result.emitted_vertices = spriteMetrics.emitted_vertices;
            result.emitted_indices = spriteMetrics.emitted_indices;
            result.material_count = spriteMetrics.material_count;
            result.canvas_rejections =
                spriteMetrics.invalid_handle_rejections
                + spriteMetrics.invalid_material_rejections
                + spriteMetrics.invalid_sprite_rejections
                + spriteMetrics.material_collision_rejections
                + spriteMetrics.duplicate_sort_key_rejections
                + spriteMetrics.submission_limit_rejections
                + spriteMetrics.material_limit_rejections
                + spriteMetrics.batch_limit_rejections
                + spriteMetrics.geometry_limit_rejections
                + plan.diagnostics.invalid_tile_sets
                + plan.diagnostics.invalid_tile_layers
                + plan.diagnostics.invalid_tile_chunks
                + plan.diagnostics.missing_tile_set_references
                + plan.diagnostics.missing_tile_layer_references;

            if (impl_->audio_runtime && impl_->audio_session)
            {
                const auto audioSnapshot =
                    impl_->audio_runtime->snapshot();
                result.audio_resident_clips =
                    audioSnapshot.mixer.resident_clips;
                result.audio_bound_buses =
                    audioSnapshot.mixer.bound_buses;
                result.audio_resident_bytes =
                    audioSnapshot.mixer.resident_bytes;
                result.audio_frames_mixed =
                    audioSnapshot.metrics.frames_mixed;
                result.audio_frames_submitted =
                    audioSnapshot.metrics.frames_submitted;
                result.audio_failures +=
                    audioSnapshot.metrics.physical_failures;
            }

            result.code = RuntimeCostCode::ready;
            result.diagnostic =
                result.canvas_rejections == 0u
                    ? "gameplay runtime costs are within declared Canvas2D limits"
                    : "gameplay runtime compiled with rejected Canvas2D submissions";
            return result;
        }
        catch (const std::bad_alloc&)
        {
            result.code = RuntimeCostCode::allocation_failure;
            result.diagnostic =
                "gameplay runtime cost snapshot allocation failed";
            return result;
        }
        catch (...)
        {
            result.code = RuntimeCostCode::invalid_scene;
            result.diagnostic = "gameplay runtime cost snapshot failed";
            return result;
        }
    }
    RuntimeBudgetAssessment assess_runtime_costs(
        const RuntimeCostSnapshot& costs,
        const Budgets& budgets) noexcept
    {
        RuntimeBudgetAssessment result{};
        result.limits = budgets.canvas2d;
        if (!costs)
        {
            result.code = RuntimeBudgetCode::invalid_snapshot;
            result.diagnostic = std::string{"gameplay cost snapshot "}
                + std::string{runtime_cost_code_name(costs.code)};
            return result;
        }
        if (!budgets.canvas2d.valid())
        {
            result.code = RuntimeBudgetCode::invalid_budget;
            result.diagnostic = "gameplay 2D platform budget is invalid";
            return result;
        }

        try
        {
            auto record = [&result](
                RuntimeBudgetViolation violation,
                bool exceeded,
                std::string_view label)
            {
                if (!exceeded)
                    return;
                result.violation_mask |=
                    runtime_budget_violation_bit(violation);
                if (result.diagnostic.empty())
                    result.diagnostic = "gameplay 2D budget exceeded: ";
                else
                    result.diagnostic += ", ";
                result.diagnostic += label;
            };

            const auto& limits = budgets.canvas2d;
            record(
                RuntimeBudgetViolation::canvas_pixels,
                costs.logical_canvas_pixels
                    > limits.maximum_logical_canvas_pixels,
                "canvas_pixels");
            record(
                RuntimeBudgetViolation::logical_texture_bytes,
                costs.source_texture_bytes
                    > limits.maximum_logical_texture_bytes,
                "logical_texture_bytes");
            record(
                RuntimeBudgetViolation::visible_sprites,
                costs.emitted_sprites > limits.maximum_visible_sprites,
                "visible_sprites");
            record(
                RuntimeBudgetViolation::batches,
                costs.emitted_batches > limits.maximum_batches,
                "batches");
            record(
                RuntimeBudgetViolation::collision_surfaces,
                costs.collision_surfaces
                    > limits.maximum_collision_surfaces,
                "collision_surfaces");
            record(
                RuntimeBudgetViolation::resident_audio_bytes,
                costs.audio_resident_bytes
                    > limits.maximum_resident_audio_bytes,
                "resident_audio_bytes");
            record(
                RuntimeBudgetViolation::canvas_rejections,
                costs.canvas_rejections != 0u,
                "canvas_rejections");

            if (result.violation_mask == 0u)
            {
                result.code = RuntimeBudgetCode::within_budget;
                result.diagnostic =
                    "gameplay 2D costs are within the selected platform budget";
            }
            else
                result.code = RuntimeBudgetCode::over_budget;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            result.code = RuntimeBudgetCode::allocation_failure;
            result.diagnostic = "gameplay 2D budget assessment allocation failed";
            return result;
        }
        catch (...)
        {
            result.code = RuntimeBudgetCode::invalid_snapshot;
            result.diagnostic = "gameplay 2D budget assessment failed";
            return result;
        }
    }

    RuntimeSnapshot Gameplay2DRuntime::snapshot() const
    {
        if (!impl_)
            return {.code = SessionCode::allocation_failure};
        return {
            .active = impl_->active,
            .code = impl_->code,
            .actor = actor_state(),
            .animation = impl_->animation,
            .facing_left = impl_->facing_left,
            .scene_revision = impl_->scene
                ? impl_->scene->source_revision : 0u,
            .last_input_frame = impl_->last_input_frame,
            .metrics = impl_->metrics,
            .diagnostic = impl_->diagnostic};
    }

    std::string_view Gameplay2DRuntime::diagnostic() const noexcept
    {
        static constexpr std::string_view unavailable{
            "gameplay runtime allocation failure"};
        return impl_ ? std::string_view{impl_->diagnostic} : unavailable;
    }
}
