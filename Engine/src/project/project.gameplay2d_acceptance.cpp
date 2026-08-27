/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include "epoch.engine.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

import asset.texture_artifact;
import authoring.gui_compiler;
import authoring.gui_document;
import authoring.texture;
import core.sha256;
import audio.playback_runtime;
import project.actor2d_runtime;
import project.gameplay2d_runtime;
import perf.tier;
import platform.budgets;
import project.audio_profile;
import project.gui_library;
import project.input_profile;
import project.sprite_animation;
import project.texture_pipeline;
import project.texture_source;
import project.tilemap_runtime;
import render.canvas2d_scene;

namespace epochengine::project
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::uint64_t maximum_acceptance_source_bytes =
            512ull * 1024ull * 1024ull;
        void trace_regeneration(
            const ArtifactAcceptanceRequest& request,
            const char* stage) noexcept
        {
            if (request.progress_sink)
                request.progress_sink(stage);
        }

        struct SourceDigest final
        {
            fs::path path{};
            core::sha256::Digest digest{};
        };

        struct TextureRegeneration final
        {
            std::string logical_path{};
            asset::texture::ContentHash artifact_key{};
            asset::texture::TextureCompileProfile profile{};
            fs::path artifact_path{};
        };

        [[nodiscard]] bool path_is_within(
            const fs::path& root,
            const fs::path& candidate) noexcept
        {
            const fs::path relative = candidate.lexically_relative(root);
            return !relative.empty() && relative != fs::path{"."}
                && *relative.begin() != fs::path{".."};
        }

        [[nodiscard]] std::optional<fs::path> canonical_regular_file(
            const fs::path& root,
            const fs::path& candidate) noexcept
        {
            std::error_code error{};
            const fs::file_status status = fs::symlink_status(candidate, error);
            if (error || fs::is_symlink(status) || !fs::is_regular_file(status))
                return std::nullopt;
            const fs::path canonical = fs::canonical(candidate, error);
            if (error || !path_is_within(root, canonical))
                return std::nullopt;
            return canonical;
        }

        [[nodiscard]] std::optional<core::sha256::Digest> hash_file(
            const fs::path& path,
            std::uint64_t maximum_bytes = maximum_acceptance_source_bytes) noexcept
        {
            std::error_code error{};
            const std::uintmax_t bytes = fs::file_size(path, error);
            if (error || bytes == 0u || bytes > maximum_bytes)
                return std::nullopt;

            std::ifstream input{path, std::ios::binary};
            if (!input)
                return std::nullopt;
            core::sha256::Hasher hasher{};
            std::array<std::uint8_t, 64u * 1024u> buffer{};
            while (input)
            {
                input.read(
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = input.gcount();
                if (count > 0)
                {
                    hasher.update(std::span<const std::uint8_t>{
                        buffer.data(), static_cast<std::size_t>(count)});
                }
            }
            if (!input.eof())
                return std::nullopt;
            return hasher.finish();
        }

        [[nodiscard]] std::optional<std::vector<std::byte>> read_file_bytes(
            const fs::path& path,
            std::uint64_t maximum_bytes) noexcept
        {
            std::error_code error{};
            const std::uintmax_t bytes = fs::file_size(path, error);
            if (error || bytes == 0u || bytes > maximum_bytes)
                return std::nullopt;
            std::vector<std::byte> result(static_cast<std::size_t>(bytes));
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return std::nullopt;
            input.read(
                reinterpret_cast<char*>(result.data()),
                static_cast<std::streamsize>(result.size()));
            if (!input
                || input.gcount() != static_cast<std::streamsize>(result.size()))
            {
                return std::nullopt;
            }
            return result;
        }

        [[nodiscard]] bool same_path(
            const fs::path& left,
            const fs::path& right) noexcept
        {
            std::error_code left_error{};
            std::error_code right_error{};
            const fs::path normalized_left = fs::weakly_canonical(
                left, left_error);
            const fs::path normalized_right = fs::weakly_canonical(
                right, right_error);
            return !left_error && !right_error
                && normalized_left == normalized_right;
        }

        class ExactArtifactQuarantine final
        {
        public:
            explicit ExactArtifactQuarantine(fs::path library_root) noexcept
                : library_root_(std::move(library_root))
            {
            }

            ExactArtifactQuarantine(const ExactArtifactQuarantine&) = delete;
            ExactArtifactQuarantine& operator=(
                const ExactArtifactQuarantine&) = delete;

            ~ExactArtifactQuarantine()
            {
                if (!committed_)
                    rollback();
            }

            [[nodiscard]] bool stage(const fs::path& artifact) noexcept
            {
                std::error_code error{};
                const auto resolved = canonical_regular_file(
                    library_root_, artifact);
                if (!resolved)
                    return false;

                fs::path backup = *resolved;
                backup += ".epoch_regeneration_backup";
                const bool backup_exists = fs::exists(backup, error);
                if (error)
                    return false;
                if (backup_exists)
                {
                    const fs::file_status backup_status =
                        fs::symlink_status(backup, error);
                    if (error || fs::is_symlink(backup_status)
                        || !fs::is_regular_file(backup_status)
                        || !fs::remove(backup, error) || error)
                    {
                        return false;
                    }
                }

                fs::rename(*resolved, backup, error);
                if (error)
                    return false;
                entries_.push_back({*resolved, std::move(backup)});
                return true;
            }

            [[nodiscard]] std::uint32_t size() const noexcept
            {
                return static_cast<std::uint32_t>(entries_.size());
            }

            [[nodiscard]] bool commit() noexcept
            {
                committed_ = true;
                bool cleaned = true;
                for (const Entry& entry : entries_)
                {
                    std::error_code error{};
                    cleaned = fs::remove(entry.backup, error)
                        && !error && cleaned;
                }
                return cleaned;
            }

        private:
            struct Entry final
            {
                fs::path artifact{};
                fs::path backup{};
            };

            void rollback() noexcept
            {
                for (auto entry = entries_.rbegin(); entry != entries_.rend();
                     ++entry)
                {
                    std::error_code error{};
                    if (fs::exists(entry->artifact, error) && !error)
                        (void)fs::remove(entry->artifact, error);
                    error.clear();
                    if (fs::exists(entry->backup, error) && !error)
                        fs::rename(entry->backup, entry->artifact, error);
                }
            }

            fs::path library_root_{};
            std::vector<Entry> entries_{};
            bool committed_{};
        };
    }

    ArtifactRegenerationReport VerifyLibraryRegeneration(
        const ArtifactAcceptanceRequest& request) noexcept
    {
        ArtifactRegenerationReport report{};
        const auto bit = [](ArtifactKind kind) noexcept
        {
            return static_cast<std::uint32_t>(kind);
        };
        const auto fail = [&](std::string stage, std::string diagnostic)
        {
            report.stage = std::move(stage);
            report.diagnostic = std::move(diagnostic);
            return report;
        };

        try
        {
            trace_regeneration(request, "start");
            const ArtifactAcceptanceReport baseline = VerifyArtifacts(request);
            if (!baseline.succeeded)
            {
                return fail(
                    "baseline",
                    "artifact baseline failed at " + baseline.stage + ": "
                        + baseline.diagnostic);
            }
            trace_regeneration(request, "baseline_complete");
            report.required_mask = baseline.required_mask
                & ~bit(ArtifactKind::scene);
            if (report.required_mask == 0u)
                return fail("request", "no derived artifacts were declared");

            std::error_code error{};
            const fs::path root = fs::canonical(
                fs::path{request.project_root}, error);
            if (error || !fs::is_directory(root, error) || error)
                return fail("project_root", "project root is not accessible");
            const fs::path library_root = fs::canonical(root / "Library", error);
            if (error || !fs::is_directory(library_root, error) || error
                || !path_is_within(root, library_root))
            {
                return fail("library", "project Library is not accessible");
            }

            trace_regeneration(request, "root_ready");
            std::vector<SourceDigest> sources{};
            std::vector<fs::path> artifacts{};
            std::vector<TextureRegeneration> textures{};
            const auto add_source = [&](const fs::path& candidate) -> bool
            {
                const auto resolved = canonical_regular_file(root, candidate);
                if (!resolved)
                    return false;
                if (std::ranges::any_of(
                        sources,
                        [&](const SourceDigest& source)
                        {
                            return source.path == *resolved;
                        }))
                {
                    return true;
                }
                const auto digest = hash_file(*resolved);
                if (!digest)
                    return false;
                sources.push_back({*resolved, *digest});
                return true;
            };
            const auto add_artifact = [&](const fs::path& candidate) -> bool
            {
                const auto resolved = canonical_regular_file(
                    library_root, candidate);
                if (!resolved)
                    return false;
                if (std::ranges::find(artifacts, *resolved) == artifacts.end())
                    artifacts.push_back(*resolved);
                return true;
            };

            if (!request.scene_path.empty()
                && !add_source(root / fs::path{request.scene_path}))
            {
                return fail("scene_source", "scene source could not be bounded and hashed");
            }

            trace_regeneration(request, "scene_source_ready");
            if (!request.tilemap_path.empty())
            {
                if (!add_source(root / fs::path{request.tilemap_path}))
                {
                    return fail(
                        "tilemap_source",
                        "tilemap source could not be bounded and hashed");
                }
                project_tilemap_runtime::ProjectTileMapRuntime map_runtime{
                    std::string{request.project_id}, root.string()};
                const auto prepared = map_runtime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        project_tilemap_runtime::SourcePolicy::require_source});
                if (!prepared
                    || prepared.provenance
                        != project_tilemap_runtime::Provenance::source_compiled
                    || !add_artifact(prepared.map.locator.storage_path))
                {
                    return fail(
                        "tilemap_preflight",
                        prepared.diagnostic.empty()
                            ? "tilemap source did not produce a bounded artifact"
                            : prepared.diagnostic);
                }

                project_textures::ProjectTexturePipeline texture_pipeline{
                    std::string{request.project_id}, root.string()};
                project_texture_sources::ProjectTextureSourceStore texture_sources{
                    std::string{request.project_id}, root};
                if (!texture_pipeline.valid() || !texture_sources.valid())
                    return fail("texture_preflight", "project texture stores are invalid");
                for (const auto& dependency : prepared.texture_dependencies)
                {
                    const asset::texture::ContentHash key{
                        dependency.artifact_key};
                    const auto restored = texture_pipeline.restore_exact(
                        dependency.logical_path, key);
                    const auto loaded = texture_sources.load(
                        dependency.logical_path);
                    if (!restored || !loaded
                        || !add_source(loaded.location.storage_path)
                        || !add_artifact(restored.locator.storage_path))
                    {
                        return fail(
                            "texture_preflight",
                            "tilemap texture source/artifact pair is incomplete");
                    }
                    textures.push_back({
                        dependency.logical_path,
                        key,
                        restored.locator.profile,
                        fs::path{restored.locator.storage_path}});
                }
            }

            trace_regeneration(request, "tilemap_preflight_complete");
            if (!request.input_profile_path.empty())
            {
                trace_regeneration(request, "tilemap_regeneration_complete");
                project_input::ProjectInputProfileStore store{
                    std::string{request.project_id}, root};
                const auto loaded = store.load_source();
                if (!store.valid() || !loaded
                    || !add_source(loaded.storage_path)
                    || !add_artifact(store.artifact_path()))
                {
                    return fail(
                        "input_preflight",
                        "input source/artifact pair is incomplete");
                }
            }

            trace_regeneration(request, "input_preflight_complete");
            if (!request.sprite_animation_path.empty())
            {
                project_sprite_animation::ProjectSpriteAnimationStore store{
                    std::string{request.project_id}, root};
                const auto loaded = store.load_source();
                if (!store.valid() || !loaded
                    || !add_source(loaded.storage_path)
                    || !add_artifact(store.artifact_path()))
                {
                    return fail(
                        "animation_preflight",
                        "sprite animation source/artifact pair is incomplete");
                }
            }

            trace_regeneration(request, "animation_preflight_complete");
            if (!request.audio_profile_path.empty())
            {
                trace_regeneration(request, "animation_regeneration_complete");
                project_audio::ProjectAudioProfileStore store{
                    std::string{request.project_id}, root};
                const auto loaded = store.load();
                if (!store.valid() || !loaded || !add_source(loaded.path)
                    || !add_artifact(store.artifact_path()))
                {
                    return fail(
                        "audio_preflight",
                        "audio source/artifact pair is incomplete");
                }
                for (const auto& cue : loaded.source.cues)
                {
                    if (!add_source(root / fs::path{cue.logical_audio_path}))
                    {
                        return fail(
                            "audio_source",
                            "referenced audio source could not be bounded and hashed");
                    }
                }
            }

            trace_regeneration(request, "audio_preflight_complete");
            if (!request.gui_path.empty())
            {
                const fs::path gui_source = root / fs::path{request.gui_path};
                const auto gui_bytes = read_file_bytes(
                    gui_source, 64ull * 1024ull * 1024ull);
                const auto decoded = gui_bytes
                    ? authoring::gui::deserialize_gui_document(*gui_bytes)
                    : authoring::gui::DeserializedGuiDocument{};
                project_gui::ArtifactLibrary library{
                    std::string{request.project_id}, root.string()};
                const auto loaded = library.load_latest(request.gui_path);
                if (!add_source(gui_source) || !decoded || !library.valid()
                    || !loaded || !add_artifact(loaded.locator.storage_path))
                {
                    return fail(
                        "gui_preflight",
                        "GUI source/artifact pair is incomplete");
                }
            }

            trace_regeneration(request, "gui_preflight_complete");
            ExactArtifactQuarantine quarantine{library_root};
            trace_regeneration(request, "gui_regeneration_complete");
            for (const fs::path& artifact : artifacts)
            {
                if (!quarantine.stage(artifact))
                {
                    return fail(
                        "quarantine",
                        "an exact derived artifact could not be quarantined safely");
                }
            }
            report.artifact_files_quarantined = quarantine.size();

            trace_regeneration(request, "quarantine_complete");
            if (!textures.empty())
            {
                project_textures::ProjectTexturePipeline pipeline{
                    std::string{request.project_id}, root.string()};
                project_texture_sources::ProjectTextureSourceStore source_store{
                    std::string{request.project_id}, root};
                for (const TextureRegeneration& recipe : textures)
                {
                    const auto loaded = source_store.load(recipe.logical_path);
                    if (!loaded)
                        return fail("texture_regeneration", "texture source reload failed");
                    const auto compiled = loaded.document->compile_artifact(
                        recipe.profile);
                    const auto published = pipeline.publish(
                        recipe.logical_path, compiled);
                    if (!compiled || !published
                        || published.locator.artifact_key.words
                            != recipe.artifact_key.words
                        || !same_path(
                            published.locator.storage_path,
                            recipe.artifact_path))
                    {
                        return fail(
                            "texture_regeneration",
                            "texture source did not reproduce its exact artifact");
                    }
                }
            }

            if (!request.tilemap_path.empty())
            {
                trace_regeneration(request, "texture_regeneration_complete");
                project_tilemap_runtime::ProjectTileMapRuntime source_runtime{
                    std::string{request.project_id}, root.string()};
                const auto regenerated = source_runtime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        project_tilemap_runtime::SourcePolicy::require_source});
                project_tilemap_runtime::ProjectTileMapRuntime compiled_runtime{
                    std::string{request.project_id}, root.string()};
                const auto restored = compiled_runtime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        project_tilemap_runtime::SourcePolicy::compiled_only});
                if (!regenerated || !regenerated.library_changed
                    || regenerated.provenance
                        != project_tilemap_runtime::Provenance::source_compiled
                    || !restored
                    || restored.provenance
                        != project_tilemap_runtime::Provenance::library_restored)
                {
                    return fail(
                        "tilemap_regeneration",
                        regenerated.diagnostic.empty()
                            ? "tilemap source/compiled-only round trip failed"
                            : regenerated.diagnostic);
                }
                report.regenerated_mask |= bit(ArtifactKind::tilemap);
            }

            if (!request.input_profile_path.empty())
            {
                project_input::ProjectInputProfileStore store{
                    std::string{request.project_id}, root};
                const auto source = store.load_source();
                const auto compiled = source
                    ? project_input::compile_profile(
                        request.project_id, source.source)
                    : project_input::CompiledProfileResult{};
                const auto published = compiled
                    ? store.publish_artifact(compiled.artifact)
                    : project_input::StoredArtifact{};
                const auto restored = published
                    ? store.load_artifact()
                    : project_input::LoadedArtifact{};
                if (!source || !compiled || !published || !restored)
                    return fail("input_regeneration", "input artifact regeneration failed");
                report.regenerated_mask |= bit(ArtifactKind::input_profile);
            }

            if (!request.sprite_animation_path.empty())
            {
                trace_regeneration(request, "input_regeneration_complete");
                const auto prepared = project_sprite_animation::
                    prepare_project_sprite_animations(
                        request.project_id,
                        root,
                        request.sprite_animation_path,
                        nullptr);
                project_sprite_animation::ProjectSpriteAnimationStore store{
                    std::string{request.project_id}, root};
                const auto restored = store.load_artifact();
                if (!prepared || prepared.source_materialized
                    || !prepared.library_changed || !restored)
                {
                    return fail(
                        "animation_regeneration",
                        prepared.diagnostic.empty()
                            ? "sprite animation regeneration failed"
                            : prepared.diagnostic);
                }
                report.regenerated_mask |= bit(ArtifactKind::sprite_animation);
            }

            if (!request.audio_profile_path.empty())
            {
                project_audio::ProjectAudioProfileStore store{
                    std::string{request.project_id}, root};
                const auto prepared = store.prepare(false);
                const auto restored = store.load_artifact(false);
                if (!prepared || !prepared.compiled_from_source
                    || prepared.restored_from_artifact || !restored)
                {
                    return fail(
                        "audio_regeneration",
                        prepared.diagnostic.empty()
                            ? "audio artifact regeneration failed"
                            : prepared.diagnostic);
                }
                report.regenerated_mask |= bit(ArtifactKind::audio_profile);
            }

            if (!request.gui_path.empty())
            {
                trace_regeneration(request, "audio_regeneration_complete");
                constexpr std::uint64_t maximum_gui_bytes =
                    64ull * 1024ull * 1024ull;
                const auto bytes = read_file_bytes(
                    root / fs::path{request.gui_path}, maximum_gui_bytes);
                const auto decoded = bytes
                    ? authoring::gui::deserialize_gui_document(*bytes)
                    : authoring::gui::DeserializedGuiDocument{};
                const auto compiled = decoded
                    ? authoring::gui::compile_gui_document(*decoded.snapshot)
                    : authoring::gui::CompileResult{};
                project_gui::ArtifactLibrary library{
                    std::string{request.project_id}, root.string()};
                const auto published = compiled
                    ? library.persist(request.gui_path, compiled.artifact)
                    : project_gui::ArtifactLocator{};
                const auto restored = published
                    ? library.load_exact(
                        request.gui_path, compiled.artifact.identity.key)
                    : project_gui::LoadedArtifact{};
                if (!bytes || !decoded || !compiled || !published || !restored)
                    return fail("gui_regeneration", "GUI artifact regeneration failed");
                report.regenerated_mask |= bit(ArtifactKind::gui);
            }

            for (const fs::path& artifact : artifacts)
            {
                if (!canonical_regular_file(library_root, artifact))
                {
                    return fail(
                        "artifact_verification",
                        "a regenerated artifact is missing or unsafe");
                }
            }
            trace_regeneration(request, "artifact_files_verified");
            const ArtifactAcceptanceReport regenerated = VerifyArtifacts(request);
            if (!regenerated.succeeded)
            {
                return fail(
                    "acceptance",
                    "regenerated artifact acceptance failed at "
                        + regenerated.stage + ": " + regenerated.diagnostic);
            }

            trace_regeneration(request, "regenerated_acceptance_complete");
            for (const SourceDigest& source : sources)
            {
                const auto current = hash_file(source.path);
                if (!current || *current != source.digest)
                {
                    return fail(
                        "source_preservation",
                        "an authored source changed during Library regeneration");
                }
            }
            report.source_files_verified =
                static_cast<std::uint32_t>(sources.size());
            if (report.regenerated_mask != report.required_mask)
            {
                return fail(
                    "coverage",
                    "not every declared derived artifact was regenerated");
            }
            if (!quarantine.commit())
            {
                return fail(
                    "cleanup",
                    "regeneration passed but old artifact quarantine cleanup failed");
            }

            trace_regeneration(request, "source_preservation_complete");
            report.succeeded = true;
            report.stage = "complete";
            report.diagnostic =
                "declared Library artifacts regenerated from byte-identical project sources";
            return report;
        }
        catch (const std::exception& exception)
        {
            return fail("exception", exception.what());
        }
        catch (...)
        {
            return fail(
                "exception",
                "Library regeneration failed with an unknown exception");
        }
    }

    GameplayAcceptanceReport VerifyPlayable2D(
        const ArtifactAcceptanceRequest& request) noexcept
    {
        namespace fs = std::filesystem;
        GameplayAcceptanceReport report{};
        const auto fail = [&](std::string stage, std::string diagnostic)
        {
            report.stage = std::move(stage);
            report.diagnostic = std::move(diagnostic);
            return report;
        };

        if (request.project_id.empty() || request.project_root.empty()
            || request.tilemap_path.empty()
            || request.input_profile_path.empty())
        {
            return fail(
                "request",
                "playable 2D acceptance requires project, tilemap, and input identity");
        }

        try
        {
            std::error_code error{};
            const fs::path root = fs::canonical(
                fs::path{request.project_root}, error);
            if (error || !fs::is_directory(root, error) || error)
            {
                return fail(
                    "project_root",
                    "project root is not an accessible directory");
            }

            auto prepared = project_gameplay2d::prepare_project({
                .project_id = std::string{request.project_id},
                .project_root = root,
                .tilemap = {
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        project_tilemap_runtime::SourcePolicy::prefer_source},
                .input_profile_path =
                    std::string{request.input_profile_path},
                .sprite_animation_path =
                    std::string{request.sprite_animation_path},
                .audio_profile_path =
                    std::string{request.audio_profile_path},
                .input_fallback = project_gameplay2d::InputFallbackPolicy::
                    require_source_or_artifact,
                .request_physical_audio = false,
                .require_sprite_animation =
                    !request.sprite_animation_path.empty(),
                .require_audio = !request.audio_profile_path.empty()});
            if (!prepared)
            {
                return fail(
                    "prepare",
                    std::string{project_gameplay2d::preparation_code_name(
                        prepared.code)} + ": " + prepared.diagnostic);
            }

            const auto preparedActor = prepared.project.actor;
            const std::size_t preparedCollisionCount =
                prepared.project.collision.size();
            std::string preparedCollisionBounds{"none"};
            if (!prepared.project.collision.empty())
            {
                const auto& bounds = prepared.project.collision.front().bounds;
                preparedCollisionBounds = std::to_string(bounds.x) + ","
                    + std::to_string(bounds.y) + ","
                    + std::to_string(bounds.width) + ","
                    + std::to_string(bounds.height);
            }

            audio::PlaybackRuntime audioRuntime{};
            project_gameplay2d::Gameplay2DRuntime gameplay{};
            const auto opened = gameplay.open(
                std::move(prepared.project), &audioRuntime);
            if (opened != project_gameplay2d::SessionCode::ready)
            {
                return fail(
                    "open",
                    std::string{project_gameplay2d::session_code_name(opened)}
                        + ": " + std::string{gameplay.diagnostic()});
            }

            std::uint64_t frame{};
            const auto advance = [&gameplay, &frame](
                std::span<const project_input::ActionImpulse> impulses = {})
            {
                return gameplay.advance(
                    {.frame_index = ++frame}, impulses, 1.0 / 60.0);
            };
            for (std::uint32_t index = 0u; index < 120u; ++index)
            {
                if (!advance())
                    return fail("settle", std::string{gameplay.diagnostic()});
            }
            const auto settled = gameplay.actor_state();
            if (!settled.grounded)
            {
                return fail(
                    "collision",
                    "actor did not settle on authored collision; spawn="
                        + std::to_string(preparedActor.spawn_x) + ","
                        + std::to_string(preparedActor.spawn_y)
                        + " final=" + std::to_string(settled.x) + ","
                        + std::to_string(settled.y)
                        + " velocity="
                        + std::to_string(settled.velocity_x) + ","
                        + std::to_string(settled.velocity_y)
                        + " collision_count="
                        + std::to_string(preparedCollisionCount)
                        + " first_bounds=" + preparedCollisionBounds);
            }

            const std::array move{
                project_input::ActionImpulse{
                    .semantic = project_input::ActionSemantic::move_x,
                    .value_q15 = project_input::normalized_unit}};
            for (std::uint32_t index = 0u; index < 20u; ++index)
            {
                if (!advance(move))
                    return fail("move", std::string{gameplay.diagnostic()});
            }
            const auto moved = gameplay.actor_state();
            if (moved.x <= settled.x)
                return fail("move", "authored move action did not move the actor");
            if (!moved.grounded)
            {
                return fail(
                    "move_collision",
                    "actor lost authored ground while traversing the starter floor");
            }

            const std::array jump{
                project_input::ActionImpulse{
                    .semantic = project_input::ActionSemantic::jump,
                    .value_q15 = project_input::normalized_unit,
                    .pressed = true}};
            const auto jumped = advance(jump);
            bool sawJump{};
            for (const auto& event : jumped.events)
            {
                sawJump = sawJump
                    || event.kind
                        == project_actor2d::ActorEventKind::jump_started;
            }
            if (!jumped)
                return fail("jump", std::string{gameplay.diagnostic()});
            if (!sawJump)
            {
                return fail(
                    "jump",
                    jumped.actor.grounded
                        ? "authored jump press produced no jump event while grounded"
                        : "actor lost authored ground before the jump press was consumed");
            }

            bool sawLand{};
            for (std::uint32_t index = 0u; index < 180u && !sawLand; ++index)
            {
                const auto advanced = advance();
                if (!advanced)
                    return fail("airborne", std::string{gameplay.diagnostic()});
                for (const auto& event : advanced.events)
                {
                    sawLand = sawLand
                        || event.kind
                            == project_actor2d::ActorEventKind::landed;
                }
            }
            if (!sawLand || !gameplay.actor_state().grounded)
                return fail("land", "actor did not land on authored collision");

            int canvasOwner{};
            const auto* scene = gameplay.scene();
            if (!scene)
                return fail("canvas", "gameplay produced no Canvas2D scene");
            const auto published = canvas2d::scene_content::publish(
                &canvasOwner, *scene);
            const auto acquired = canvas2d::scene_content::acquire(
                &canvasOwner);
            const auto plan = canvas2d::scene_content::compile(
                acquired, {640u, 360u});
            if (!published || !acquired || !plan)
            {
                (void)canvas2d::scene_content::retire(&canvasOwner);
                return fail("canvas", "gameplay Canvas2D publication failed");
            }
            report.canvas_hash = acquired.content_hash;
            if (canvas2d::scene_content::retire(&canvasOwner)
                != canvas2d::scene_content::SceneCode::ready)
            {
                return fail("canvas", "gameplay Canvas2D retirement failed");
            }

            const auto costs = gameplay.cost_snapshot();
            if (!costs)
            {
                return fail(
                    "costs",
                    std::string{"gameplay cost snapshot "}
                        + std::string{
                            project_gameplay2d::runtime_cost_code_name(
                                costs.code)}
                        + ": " + costs.diagnostic);
            }
            report.logical_texture_bytes = costs.source_texture_bytes;
            report.emitted_sprites = costs.emitted_sprites;
            report.emitted_batches = costs.emitted_batches;
            report.collision_surfaces = costs.collision_surfaces;
            report.peak_contacts_per_step =
                costs.peak_contacts_per_step;
            report.audio_resident_bytes = costs.audio_resident_bytes;
            report.canvas_rejections = costs.canvas_rejections;
            const auto portableBudget =
                platform::recommended_budgets_for_tier(
                    perf::tier::mobile_30);
            const auto portableAssessment =
                project_gameplay2d::assess_runtime_costs(
                    costs, portableBudget);
            report.portable_budget_violations =
                portableAssessment.violation_mask;
            if (!portableAssessment)
            {
                return fail(
                    "portable_budget",
                    std::string{
                        project_gameplay2d::runtime_budget_code_name(
                            portableAssessment.code)}
                        + ": " + portableAssessment.diagnostic);
            }
            if (report.emitted_sprites == 0u
                || report.emitted_batches == 0u
                || report.collision_surfaces == 0u
                || report.peak_contacts_per_step == 0u
                || report.canvas_rejections != 0u)
            {
                return fail(
                    "costs",
                    "gameplay cost evidence is incomplete or over budget");
            }
            if (!request.audio_profile_path.empty()
                && report.audio_resident_bytes == 0u)
            {
                return fail(
                    "costs",
                    "gameplay audio declared no resident PCM bytes");
            }

            const auto metrics = gameplay.metrics();
            report.input_frames = metrics.input_frames;
            report.fixed_steps = metrics.fixed_steps;
            report.animation_samples = metrics.animation_samples;
            report.audio_triggers = metrics.audio_triggers;
            if (!request.sprite_animation_path.empty()
                && report.animation_samples == 0u)
            {
                return fail("animation", "sprite animation produced no samples");
            }
            if (!request.audio_profile_path.empty()
                && report.audio_triggers < 2u)
            {
                return fail("audio", "jump and land cues were not routed");
            }
            if (gameplay.close() != project_gameplay2d::SessionCode::closed
                || gameplay.active() || gameplay.scene() != nullptr
                || audioRuntime.snapshot().session_active)
            {
                return fail(
                    "teardown",
                    "gameplay session did not stop cleanly");
            }

            report.succeeded = true;
            report.stage = "complete";
            report.diagnostic =
                "playable 2D project input, simulation, presentation, audio, and teardown accepted";
            return report;
        }
        catch (const std::exception& exception)
        {
            return fail("exception", exception.what());
        }
        catch (...)
        {
            return fail(
                "exception",
                "playable 2D acceptance failed with an unknown exception");
        }
    }
}
