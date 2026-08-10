/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module editor.project_textures;

import asset.texture_artifact;
import asset.texture_import;
import capability.profile;
import platform.budgets;
import project.texture_admission;
import project.texture_pipeline;
import project.texture_resources;
import render.canvas2d;
import render.canvas2d_scene;
import scene.snapshot;

export namespace epochengine::editor_project_textures
{
    enum class ControllerCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_controller,
        source_outside_assets,
        unsupported_source,
        import_rejected,
        admission_rejected,
        publication_rejected,
        selection_not_found,
        exact_restore_rejected,
        lease_rejected,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view controller_code_name(
        ControllerCode code) noexcept
    {
        switch (code)
        {
        case ControllerCode::ready: return "ready";
        case ControllerCode::unchanged: return "unchanged";
        case ControllerCode::invalid_controller: return "invalid_controller";
        case ControllerCode::source_outside_assets: return "source_outside_assets";
        case ControllerCode::unsupported_source: return "unsupported_source";
        case ControllerCode::import_rejected: return "import_rejected";
        case ControllerCode::admission_rejected: return "admission_rejected";
        case ControllerCode::publication_rejected: return "publication_rejected";
        case ControllerCode::selection_not_found: return "selection_not_found";
        case ControllerCode::exact_restore_rejected: return "exact_restore_rejected";
        case ControllerCode::lease_rejected: return "lease_rejected";
        case ControllerCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct TextureCatalogEntry final
    {
        std::string logical_path{};
        std::filesystem::path source_path{};
        asset::texture::ContentHash artifact_key{};
        canvas2d::LogicalTextureReference logical{};
        std::uint64_t source_sequence{};
        std::uint64_t source_bytes{};
        std::uint64_t decoded_bytes{};
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return !logical_path.empty()
                && !source_path.empty()
                && !artifact_key.empty()
                && static_cast<bool>(logical)
                && source_sequence != 0u
                && width != 0u
                && height != 0u;
        }
    };

    struct ControllerResult final
    {
        ControllerCode code{ControllerCode::invalid_controller};
        asset::texture::TextureImportStatus import_status{
            asset::texture::TextureImportStatus::invalid_request};
        project_textures::TextureAdmissionReason admission_reason{
            project_textures::TextureAdmissionReason::invalid_policy};
        project_textures::PipelineCode pipeline_code{
            project_textures::PipelineCode::invalid_pipeline};
        std::optional<TextureCatalogEntry> entry{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == ControllerCode::ready
                    || code == ControllerCode::unchanged)
                && entry.has_value()
                && static_cast<bool>(*entry);
        }
    };

    struct ResolvedTextureMaterial final
    {
        scene::SceneTextureMaterialSnapshot material{};
        canvas2d::LogicalTextureReference logical{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return material.valid() && static_cast<bool>(logical);
        }
    };

    struct SceneTextureLeaseResult final
    {
        ControllerCode code{ControllerCode::invalid_controller};
        project_textures::PipelineCode pipeline_code{
            project_textures::PipelineCode::invalid_pipeline};
        project_textures::ResourceCode resource_code{
            project_textures::ResourceCode::invalid_registry};
        std::vector<ResolvedTextureMaterial> materials{};
        canvas2d::scene_content::ResourceLease lease{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ControllerCode::ready
                && lease.valid()
                && !materials.empty();
        }
    };

    class ProjectTextureController final
    {
    public:
        ProjectTextureController(
            std::string projectId,
            std::filesystem::path projectRoot) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] std::span<const TextureCatalogEntry> catalog() const noexcept;
        [[nodiscard]] std::string_view selected_logical_path() const noexcept;
        [[nodiscard]] const TextureCatalogEntry* selected() const noexcept;

        [[nodiscard]] ControllerResult import_source(
            const std::filesystem::path& sourcePath,
            const capability::SubsystemProfile& renderer,
            const Budgets& platformBudgets) noexcept;
        [[nodiscard]] ControllerResult select(
            std::string_view logicalPath) noexcept;
        [[nodiscard]] std::optional<scene::SceneTextureMaterialSnapshot>
            selected_material() const noexcept;
        [[nodiscard]] SceneTextureLeaseResult bind_scene_materials(
            std::span<const scene::SceneTextureMaterialSnapshot>
                materials) noexcept;

        [[nodiscard]] const project_textures::ProjectTexturePipeline&
            pipeline() const noexcept;

    private:
        [[nodiscard]] ControllerResult reject(
            ControllerCode code,
            asset::texture::TextureImportStatus importStatus =
                asset::texture::TextureImportStatus::invalid_request,
            project_textures::TextureAdmissionReason admissionReason =
                project_textures::TextureAdmissionReason::invalid_policy,
            project_textures::PipelineCode pipelineCode =
                project_textures::PipelineCode::invalid_pipeline) const noexcept;

        std::filesystem::path project_root_{};
        std::filesystem::path assets_root_{};
        project_textures::ProjectTexturePipeline pipeline_;
        std::vector<TextureCatalogEntry> catalog_{};
        std::string selected_logical_path_{};
    };

    enum class ControllerContractFailure : std::uint8_t
    {
        none,
        temporary_project,
        outside_assets_rejection,
        import,
        material_identity,
        exact_restore,
        lease,
        historical_revision,
        stale_path
    };

    [[nodiscard]] constexpr std::string_view
        controller_contract_failure_name(
            ControllerContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ControllerContractFailure::none: return "pass";
        case ControllerContractFailure::temporary_project:
            return "temporary_project";
        case ControllerContractFailure::outside_assets_rejection:
            return "outside_assets_rejection";
        case ControllerContractFailure::import: return "import";
        case ControllerContractFailure::material_identity:
            return "material_identity";
        case ControllerContractFailure::exact_restore: return "exact_restore";
        case ControllerContractFailure::lease: return "lease";
        case ControllerContractFailure::historical_revision:
            return "historical_revision";
        case ControllerContractFailure::stale_path: return "stale_path";
        }
        return "unknown";
    }

    [[nodiscard]] ControllerContractFailure
        project_texture_controller_contract_failure() noexcept;
}
