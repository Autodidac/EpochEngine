/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "../../include/engine.config.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <new>
#include <span>
#include <string>
#include <utility>

module project.tilemap_runtime;

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import authoring.tilemap;
#endif

namespace epochengine::project_tilemap_runtime
{
    namespace
    {
        [[nodiscard]] bool exact_texture_identity(
            const asset::tilemap::TextureDependency& dependency,
            const project_textures::TexturePipelineResult& restored) noexcept
        {
            const asset::texture::ContentHash expectedKey{
                dependency.artifact_key};
            return restored.locator.project_key == dependency.project_key
                && restored.locator.asset_key == dependency.asset_key
                && restored.locator.artifact_key == expectedKey
                && restored.logical == canvas2d::LogicalTextureReference{
                    dependency.asset_key, dependency.artifact_revision};
        }

        [[nodiscard]] bool contains(
            std::span<const canvas2d::LogicalTextureReference> values,
            canvas2d::LogicalTextureReference value) noexcept
        {
            return std::find(values.begin(), values.end(), value)
                != values.end();
        }
    }

    ProjectTileMapRuntime::ProjectTileMapRuntime(
        std::string projectId,
        std::string projectRoot) noexcept
        : maps_(projectId, projectRoot),
          textures_(projectId, projectRoot)
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
          , sources_(std::move(projectId), std::move(projectRoot))
#endif
    {
    }

    bool ProjectTileMapRuntime::valid() const noexcept
    {
        const bool shared = maps_.valid() && textures_.valid()
            && maps_.project_key() == textures_.project_key();
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        return shared && sources_.valid()
            && sources_.project_id() == maps_.project_id();
#else
        return shared;
#endif
    }

    std::string_view ProjectTileMapRuntime::project_id() const noexcept
    {
        return maps_.project_id();
    }

    std::string_view ProjectTileMapRuntime::project_root() const noexcept
    {
        return maps_.project_root();
    }

    RuntimeMetrics ProjectTileMapRuntime::metrics() const noexcept
    {
        return metrics_;
    }

    PreparedTileMap ProjectTileMapRuntime::prepare(
        const RuntimeRequest& request) noexcept
    {
        ++metrics_.prepare_requests;
        if (!valid())
            return reject(RuntimeCode::invalid_runtime, "runtime unavailable");
        if (request.logical_path.empty()
            || canvas2d::validate(request.project)
                != canvas2d::ResultCode::success)
        {
            return reject(RuntimeCode::invalid_request, "invalid map request");
        }

        try
        {
            project_tilemaps::RestoredTileMapArtifact restored{};
            Provenance provenance = Provenance::none;
            bool libraryChanged = false;
            bool sourceResolved = false;

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (request.source_policy != SourcePolicy::compiled_only)
            {
                auto loaded = sources_.load(request.logical_path);
                if (loaded)
                {
                    sourceResolved = true;
                    auto compiled = loaded.document->compile(
                        maps_.limits().library.artifact);
                    if (!compiled)
                    {
                        return reject(
                            RuntimeCode::source_compile_failure,
                            "canonical tile-map source did not compile");
                    }
                    auto publication = maps_.publish(
                        request.logical_path, compiled.artifact);
                    if (!publication)
                    {
                        return reject(
                            RuntimeCode::map_publication_failure,
                            "compiled tile map was not published");
                    }
                    libraryChanged = publication.library_code
                        == decltype(publication.library_code)::ready;
                    restored = {
                        publication.code,
                        std::move(publication),
                        std::move(compiled.artifact)};
                    provenance = Provenance::source_compiled;
                    ++metrics_.source_compilations;
                }
                else if (loaded.code
                    != project_tilemap_sources::SourceCode::not_found)
                {
                    return reject(
                        RuntimeCode::source_failure,
                        "canonical tile-map source could not be read");
                }
                else if (request.source_policy == SourcePolicy::require_source)
                {
                    return reject(
                        RuntimeCode::source_not_found,
                        "required canonical tile-map source was not found");
                }
            }
#else
            if (request.source_policy == SourcePolicy::require_source)
            {
                return reject(
                    RuntimeCode::source_unavailable,
                    "authoring source support is not compiled into this build");
            }
#endif

            if (!sourceResolved)
            {
                restored = maps_.restore_latest_artifact(
                    request.logical_path);
                if (!restored)
                {
                    return reject(
                        RuntimeCode::map_restore_failure,
                        "compiled tile-map artifact was not found");
                }
                provenance = Provenance::library_restored;
                ++metrics_.library_restorations;
            }

            std::vector<canvas2d::LogicalTextureReference> logicalTextures{};
            logicalTextures.reserve(restored.artifact.dependencies.size());
            for (const auto& dependency : restored.artifact.dependencies)
            {
                if (dependency.project_key != maps_.project_key())
                {
                    return reject(
                        RuntimeCode::foreign_texture_dependency,
                        "tile map references a foreign project texture");
                }
                const asset::texture::ContentHash artifactKey{
                    dependency.artifact_key};
                const auto texture = textures_.restore_exact(
                    dependency.logical_path, artifactKey);
                if (!texture)
                {
                    return reject(
                        RuntimeCode::texture_restore_failure,
                        "exact tile-set texture artifact was not restored");
                }
                if (!exact_texture_identity(dependency, texture))
                {
                    return reject(
                        RuntimeCode::texture_identity_mismatch,
                        "tile-set texture identity did not match the map");
                }
                if (!contains(logicalTextures, texture.logical))
                    logicalTextures.push_back(texture.logical);
                ++metrics_.texture_restorations;
            }

            canvas2d::scene_content::ResourceLease lease{};
            if (!logicalTextures.empty())
            {
                auto binding = textures_.bind_canvas2d(logicalTextures);
                if (!binding)
                {
                    return reject(
                        RuntimeCode::texture_binding_failure,
                        "tile-set textures did not produce a Canvas2D lease");
                }
                lease = std::move(binding.lease);
            }

            auto view = request.view;
            if (view.world_bounds.empty())
            {
                view.world_bounds = canvas2d::tilemap_runtime::artifact_world_bounds(
                    restored.artifact);
            }
            if (view.world_bounds.empty())
            {
                const float width = static_cast<float>(
                    request.project.logical_canvas.width)
                    / request.project.pixels_per_world_unit;
                const float height = static_cast<float>(
                    request.project.logical_canvas.height)
                    / request.project.pixels_per_world_unit;
                view.world_bounds = {
                    -width * 0.5f, -height * 0.5f, width, height};
            }
            auto visible = canvas2d::tilemap_runtime::compile_visible(
                restored.artifact, view, maps_.limits().visible);
            if (!visible)
            {
                return reject(
                    RuntimeCode::visibility_failure,
                    "tile map could not compile its visible region");
            }

            canvas2d::CameraState camera = request.camera;
            if (request.use_project_pixel_density)
            {
                camera.pixels_per_world_unit =
                    request.project.pixels_per_world_unit;
                camera.pixel_snap = request.project.pixel_snap;
            }
            if (request.center_camera_on_map)
            {
                camera.center = {
                    view.world_bounds.x + view.world_bounds.width * 0.5f,
                    view.world_bounds.y + view.world_bounds.height * 0.5f};
            }

            PreparedTileMap prepared{};
            prepared.code = RuntimeCode::ready;
            prepared.provenance = provenance;
            prepared.map = std::move(restored.map);
            prepared.artifact_identity = restored.artifact.identity;
            prepared.scene.project = request.project;
            prepared.scene.camera = camera;
            prepared.scene.sprites = std::move(visible.sprites);
            prepared.scene.resources = std::move(lease);
            prepared.scene.clear_color = request.clear_color;
            prepared.scene.letterbox_color = request.letterbox_color;
            prepared.scene.source_revision = (std::max)(
                std::uint64_t{1},
                restored.artifact.identity.source_revision.sequence);
            prepared.collision = std::move(visible.collision);
            prepared.objects = std::move(visible.objects);
            prepared.diagnostic = std::string{provenance_name(provenance)};
            prepared.library_changed = libraryChanged;
            if (!prepared.scene.valid())
            {
                return reject(
                    RuntimeCode::scene_validation_failure,
                    "prepared Canvas2D scene failed validation");
            }
            ++metrics_.prepared_scenes;
            return prepared;
        }
        catch (const std::bad_alloc&)
        {
            return reject(RuntimeCode::allocation_failure, "allocation failure");
        }
        catch (...)
        {
            return reject(
                RuntimeCode::invalid_request,
                "unexpected preparation failure");
        }
    }

    PreparedTileMap ProjectTileMapRuntime::reject(
        RuntimeCode code,
        std::string diagnostic) noexcept
    {
        ++metrics_.rejected_requests;
        PreparedTileMap result{};
        result.code = code;
        result.diagnostic = std::move(diagnostic);
        return result;
    }
}
