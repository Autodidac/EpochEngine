/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "../include/engine.config.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

export module project.tilemap_runtime;

import asset.tilemap_artifact;
import project.texture_pipeline;
import project.tilemap_pipeline;
import render.canvas2d;
import render.canvas2d_scene;
import render.canvas2d_tilemap;

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import project.tilemap_source;
#endif

export namespace epochengine::project_tilemap_runtime
{
    enum class RuntimeCode : std::uint8_t
    {
        ready,
        invalid_runtime,
        invalid_request,
        source_unavailable,
        source_not_found,
        source_failure,
        source_compile_failure,
        map_publication_failure,
        map_restore_failure,
        foreign_texture_dependency,
        texture_restore_failure,
        texture_identity_mismatch,
        texture_binding_failure,
        visibility_failure,
        scene_validation_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view runtime_code_name(
        RuntimeCode code) noexcept
    {
        switch (code)
        {
        case RuntimeCode::ready: return "ready";
        case RuntimeCode::invalid_runtime: return "invalid_runtime";
        case RuntimeCode::invalid_request: return "invalid_request";
        case RuntimeCode::source_unavailable: return "source_unavailable";
        case RuntimeCode::source_not_found: return "source_not_found";
        case RuntimeCode::source_failure: return "source_failure";
        case RuntimeCode::source_compile_failure:
            return "source_compile_failure";
        case RuntimeCode::map_publication_failure:
            return "map_publication_failure";
        case RuntimeCode::map_restore_failure: return "map_restore_failure";
        case RuntimeCode::foreign_texture_dependency:
            return "foreign_texture_dependency";
        case RuntimeCode::texture_restore_failure:
            return "texture_restore_failure";
        case RuntimeCode::texture_identity_mismatch:
            return "texture_identity_mismatch";
        case RuntimeCode::texture_binding_failure:
            return "texture_binding_failure";
        case RuntimeCode::visibility_failure: return "visibility_failure";
        case RuntimeCode::scene_validation_failure:
            return "scene_validation_failure";
        case RuntimeCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    enum class SourcePolicy : std::uint8_t
    {
        prefer_source,
        compiled_only,
        require_source
    };

    enum class Provenance : std::uint8_t
    {
        none,
        source_compiled,
        library_restored
    };

    [[nodiscard]] constexpr std::string_view provenance_name(
        Provenance provenance) noexcept
    {
        switch (provenance)
        {
        case Provenance::none: return "none";
        case Provenance::source_compiled: return "source_compiled";
        case Provenance::library_restored: return "library_restored";
        }
        return "unknown";
    }

    struct RuntimeRequest final
    {
        std::string logical_path{"Assets/Maps/main.epochmap"};
        SourcePolicy source_policy{SourcePolicy::prefer_source};
        canvas2d::ProjectSettings project{};
        canvas2d::CameraState camera{};
        canvas2d::tilemap_runtime::ViewRequest view{};
        canvas2d::LinearColor clear_color{0.03f, 0.04f, 0.06f, 1.0f};
        canvas2d::LinearColor letterbox_color{0.0f, 0.0f, 0.0f, 1.0f};
        bool use_project_pixel_density{true};
        bool center_camera_on_map{true};
    };

    struct RuntimeMetrics final
    {
        std::uint64_t prepare_requests{};
        std::uint64_t prepared_scenes{};
        std::uint64_t source_compilations{};
        std::uint64_t library_restorations{};
        std::uint64_t texture_restorations{};
        std::uint64_t rejected_requests{};
    };

    struct PreparedTileMap final
    {
        RuntimeCode code{RuntimeCode::invalid_runtime};
        Provenance provenance{Provenance::none};
        project_tilemaps::TileMapPipelineResult map{};
        asset::tilemap::ArtifactIdentity artifact_identity{};
        canvas2d::scene_content::SceneContent scene{};
        std::vector<asset::tilemap::TextureDependency> texture_dependencies{};
        std::vector<asset::tilemap::CompiledTileSet> tile_sets{};
        std::vector<asset::tilemap::CollisionPrimitive> collision{};
        std::vector<canvas2d::tilemap_runtime::VisibleObject> objects{};
        std::string diagnostic{};
        bool library_changed{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == RuntimeCode::ready
                && static_cast<bool>(map)
                && static_cast<bool>(artifact_identity)
                && scene.valid();
        }
    };

    class ProjectTileMapRuntime final
    {
    public:
        ProjectTileMapRuntime(
            std::string projectId,
            std::string projectRoot) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::string_view project_root() const noexcept;
        [[nodiscard]] RuntimeMetrics metrics() const noexcept;
        [[nodiscard]] PreparedTileMap prepare(
            const RuntimeRequest& request) noexcept;

    private:
        [[nodiscard]] PreparedTileMap reject(
            RuntimeCode code,
            std::string diagnostic) noexcept;

        project_tilemaps::ProjectTileMapPipeline maps_;
        project_textures::ProjectTexturePipeline textures_;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        project_tilemap_sources::ProjectTileMapSourceStore sources_;
#endif
        RuntimeMetrics metrics_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        texture_publication,
        source_creation,
        source_save,
        runtime_construction,
        source_regeneration,
        library_artifact_missing,
        resource_closure,
        compiled_only_restore,
        source_required_gate,
        malformed_source_gate,
        metrics
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::temporary_root: return "temporary_root";
        case ContractFailure::texture_publication: return "texture_publication";
        case ContractFailure::source_creation: return "source_creation";
        case ContractFailure::source_save: return "source_save";
        case ContractFailure::runtime_construction:
            return "runtime_construction";
        case ContractFailure::source_regeneration: return "source_regeneration";
        case ContractFailure::library_artifact_missing:
            return "library_artifact_missing";
        case ContractFailure::resource_closure: return "resource_closure";
        case ContractFailure::compiled_only_restore:
            return "compiled_only_restore";
        case ContractFailure::source_required_gate:
            return "source_required_gate";
        case ContractFailure::malformed_source_gate:
            return "malformed_source_gate";
        case ContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
