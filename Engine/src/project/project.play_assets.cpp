// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "../../include/core.format_text.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

import asset.mesh_artifact;
import core.logger;
import editor.project_textures;
import package.source_install;
import project.planetary;
import render.preview_grid;
import scene.runtime;
import scene.snapshot;

#include "project.play_assets.hpp"

namespace epochengine::project_play_assets
{
    void admit(const std::filesystem::path& root,const std::string& projectId,
        std::span<const scene_runtime::RuntimeEntity> entities,
        const project_planetary::Settings* packagedSettings,
        std::optional<project_planetary::Settings>& planetarySettings,
        std::optional<project_planetary::SourceGeneration>& planetarySource,
        previewgrid::MeshFrameLimits& frameLimits,
        std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>>& meshes,
        std::unordered_map<std::uint64_t,std::shared_ptr<const previewgrid::MeshBaseColorBinding>>& baseColorTextures,
        std::string& viewStatus)
    {
        const auto settings=project_planetary::load_settings(root);
        if(packagedSettings)planetarySettings=*packagedSettings;
        else if(settings)
        {
            const auto attached=package_source::load_project_attachment(root,"planetary_terrain");
            if(attached.source_attached())
            {
                const auto source=project_planetary::capture_source_generation(root,attached.content_hash);
                if(source&&project_planetary::source_generation_is_current(root,*source))
                {planetarySettings=settings;planetarySource=source;}
            }
        }
        const auto budget=planetarySettings
            ?project_planetary::scene_mesh_budget(*planetarySettings):project_planetary::SceneMeshBudget{};
        frameLimits={budget.frame_indices,budget.frame_bytes};
        std::uint64_t mesh_bytes{}, mesh_indices{},ordinary_bytes{},ordinary_indices{};
        std::optional<editor_project_textures::ProjectTextureController> textures;
        for (const auto& entity : entities)
        {
            if (!entity.mesh_asset) continue;
            const bool planetary=entity.category=="Planetary Terrain"&&planetarySettings.has_value();
            const auto& reference = *entity.mesh_asset;
            const std::uint64_t bytes = 64u + std::uint64_t(reference.vertex_count) * sizeof(asset::mesh::Vertex)
                + std::uint64_t(reference.index_count) * sizeof(std::uint32_t);
            if (meshes.size() >= 64u || bytes>budget.cache_bytes-(std::min)(budget.cache_bytes,mesh_bytes)
                || reference.index_count>budget.frame_indices-(std::min)(budget.frame_indices,mesh_indices)
                || (!planetary&&(bytes>std::uint64_t{16u}*1024u*1024u-(std::min)(std::uint64_t{16u}*1024u*1024u,ordinary_bytes)
                    ||reference.index_count>196'608u-(std::min)(std::uint64_t{196'608u},ordinary_indices))))
            {
                logger::get("Engine.Assets").log(logger::LogLevel::WARN, "Project mesh residency or geometry budget exceeded; mesh was not loaded.", std::source_location::current());
                continue;
            }
            auto limits=planetary?budget.artifact:asset::mesh::Limits{};
            limits.maximum_bytes=(std::min)(limits.maximum_bytes,budget.cache_bytes-mesh_bytes);
            auto loaded = asset::mesh::read_project_artifact(root, reference.logical_path, {reference.artifact_key},limits);
            if (!loaded || loaded.artifact->vertices().size() != reference.vertex_count
                || loaded.artifact->indices().size() != reference.index_count)
            {
                logger::get("Engine.Assets").log(logger::LogLevel::WARN, "Project mesh failed file/content admission: " + reference.logical_path, std::source_location::current());
                continue;
            }
            meshes.emplace(entity.source_id, std::move(loaded.artifact));
            if (entity.texture_material)
            {
                if (!textures) textures.emplace(projectId, root);
                const auto& material = *entity.texture_material;
                auto resolved = (material.alpha == scene::SceneTextureAlphaMode::opaque
                    || material.alpha == scene::SceneTextureAlphaMode::cutout)
                    ? textures->resolve_scene_material(material) : editor_project_textures::ResolvedTextureMaterial{};
                if (resolved && resolved.artifact)
                {
                    auto binding = std::make_shared<previewgrid::MeshBaseColorBinding>(
                        previewgrid::MeshBaseColorBinding{std::move(resolved.artifact),resolved.logical,resolved.sampler,
                            material.alpha == scene::SceneTextureAlphaMode::cutout,material.alpha_cutoff,
                            !material.logical_path.starts_with("Textures/Forest/reference_bark_foliage_v1")
                                && !material.logical_path.starts_with("Textures/Forest/project_bark_foliage_")});
                    if (previewgrid::valid_mesh_base_color_binding(*binding))
                        baseColorTextures.emplace(entity.source_id,std::move(binding));
                }
                if (!baseColorTextures.contains(entity.source_id))
                {
                    viewStatus = "Textured mesh drawing refused: its exact opaque/cutout base-color texture is unavailable.";
                    logger::get("Engine.Assets").log(logger::LogLevel::WARN,
                        viewStatus + " " + material.logical_path,std::source_location::current());
                }
            }
            mesh_bytes += bytes;
            mesh_indices += reference.index_count;
            if(!planetary){ordinary_bytes+=bytes;ordinary_indices+=reference.index_count;}
        }
    }
}
