// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

// Includes follow the project, scene and preview-grid imports.
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
        std::string& viewStatus);
}
