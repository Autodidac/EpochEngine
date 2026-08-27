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

export module editor.project_textures;

import asset.texture_artifact;
import asset.texture_import;
import authoring.texture;
import capability.profile;
import platform.budgets;
import project.texture_admission;
import project.texture_pipeline;
import project.texture_resources;
import project.texture_source;
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
        source_rejected,
        editable_not_open,
        unsaved_changes,
        compilation_rejected,
        edit_rejected,
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
        case ControllerCode::source_rejected: return "source_rejected";
        case ControllerCode::editable_not_open: return "editable_not_open";
        case ControllerCode::unsaved_changes: return "unsaved_changes";
        case ControllerCode::compilation_rejected: return "compilation_rejected";
        case ControllerCode::edit_rejected: return "edit_rejected";
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

    struct TexturePreview final
    {
        std::string logical_path{};
        asset::texture::ContentHash artifact_key{};
        std::uint32_t width{};
        std::uint32_t height{};
        std::vector<std::uint8_t> rgba8{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            const std::uint64_t expected =
                static_cast<std::uint64_t>(width)
                * static_cast<std::uint64_t>(height)
                * 4u;
            return !logical_path.empty()
                && !artifact_key.empty()
                && width != 0u
                && height != 0u
                && expected == rgba8.size();
        }
    };

    struct TextureThumbnailPolicy final
    {
        std::uint32_t maximum_width{96u};
        std::uint32_t maximum_height{96u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_width != 0u
                && maximum_height != 0u
                && maximum_width <= 512u
                && maximum_height <= 512u;
        }
    };

    struct TextureThumbnail final
    {
        std::string logical_path{};
        asset::texture::ContentHash artifact_key{};
        std::uint32_t width{};
        std::uint32_t height{};
        std::vector<std::uint8_t> rgba8{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            const std::uint64_t expected =
                static_cast<std::uint64_t>(width)
                * static_cast<std::uint64_t>(height)
                * 4u;
            return !logical_path.empty()
                && !artifact_key.empty()
                && width != 0u
                && height != 0u
                && expected == rgba8.size();
        }
    };

    [[nodiscard]] std::optional<TextureThumbnail> make_texture_thumbnail(
        const TexturePreview& preview,
        TextureThumbnailPolicy policy = {}) noexcept;

    struct ControllerMetrics final
    {
        std::uint64_t source_bytes{};
        std::uint64_t decoded_bytes{};
        std::uint32_t texture_count{};
        std::uint32_t maximum_width{};
        std::uint32_t maximum_height{};
        bool has_selection{};
    };

    struct CatalogRefreshResult final
    {
        ControllerCode code{ControllerCode::invalid_controller};
        std::uint32_t discovered_sources{};
        std::uint32_t restored_artifacts{};
        std::uint32_t regenerated_artifacts{};
        std::uint32_t skipped_sources{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ControllerCode::ready
                || code == ControllerCode::unchanged;
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
        project_texture_sources::SourceCode source_code{
            project_texture_sources::SourceCode::invalid_store};
        authoring::texture::ResultCode authoring_code{
            authoring::texture::ResultCode::invalid_document};
        asset::texture::ArtifactCompilationStatus compilation_status{
            asset::texture::ArtifactCompilationStatus::invalid_document};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == ControllerCode::ready
                    || code == ControllerCode::unchanged)
                && entry.has_value()
                && static_cast<bool>(*entry);
        }
    };

    struct EditableTextureState final
    {
        std::string logical_path{};
        std::filesystem::path source_path{};
        authoring::texture::DocumentRevision revision{};
        authoring::texture::CanvasDescriptor canvas{};
        std::vector<authoring::texture::LayerInfo> layers{};
        authoring::texture::LayerHandle selected_layer{};
        authoring::texture::DocumentMetrics metrics{};
        bool dirty{};
        bool can_undo{};
        bool can_redo{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return !logical_path.empty()
                && !source_path.empty()
                && revision.sequence != 0u
                && !revision.content.empty();
        }
    };

    struct TextureEditResult final
    {
        ControllerCode code{ControllerCode::editable_not_open};
        authoring::texture::ResultCode authoring_code{
            authoring::texture::ResultCode::invalid_document};
        authoring::texture::DocumentRevision revision{};
        authoring::texture::LayerHandle layer{};
        std::uint64_t affected_tiles{};
        bool dirty{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == ControllerCode::ready
                    || code == ControllerCode::unchanged)
                && (authoring_code
                        == authoring::texture::ResultCode::success
                    || authoring_code
                        == authoring::texture::ResultCode::unchanged);
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
        [[nodiscard]] ControllerMetrics metrics() const noexcept;
        [[nodiscard]] CatalogRefreshResult refresh_catalog() noexcept;

        [[nodiscard]] ControllerResult import_source(
            const std::filesystem::path& sourcePath,
            const capability::SubsystemProfile& renderer,
            const Budgets& platformBudgets) noexcept;
        [[nodiscard]] ControllerResult create_editable(
            std::string_view logicalPath,
            authoring::texture::CanvasDescriptor descriptor,
            const capability::SubsystemProfile& renderer,
            const Budgets& platformBudgets) noexcept;
        [[nodiscard]] ControllerResult open_editable(
            std::string_view logicalPath,
            const capability::SubsystemProfile& renderer,
            const Budgets& platformBudgets) noexcept;
        [[nodiscard]] ControllerResult save_editable(
            const capability::SubsystemProfile& renderer,
            const Budgets& platformBudgets) noexcept;
        [[nodiscard]] std::optional<EditableTextureState>
            editable_state() const;
        [[nodiscard]] std::optional<TexturePreview>
            editable_preview() const noexcept;
        [[nodiscard]] TextureEditResult select_editable_layer(
            authoring::texture::LayerHandle layer) noexcept;
        [[nodiscard]] TextureEditResult create_editable_layer(
            authoring::texture::LayerDescriptor descriptor) noexcept;
        [[nodiscard]] TextureEditResult create_editable_layer_mask(
            authoring::texture::LayerHandle owner,
            std::string name) noexcept;
        [[nodiscard]] TextureEditResult remove_editable_layer(
            authoring::texture::LayerHandle layer) noexcept;
        [[nodiscard]] TextureEditResult move_editable_layer(
            authoring::texture::LayerHandle layer,
            std::uint32_t insertionIndex) noexcept;
        [[nodiscard]] TextureEditResult set_editable_layer_properties(
            authoring::texture::LayerHandle layer,
            authoring::texture::LayerDescriptor descriptor) noexcept;
        [[nodiscard]] TextureEditResult paint_editable(
            authoring::texture::StrokeDescriptor stroke) noexcept;
        [[nodiscard]] TextureEditResult undo_editable() noexcept;
        [[nodiscard]] TextureEditResult redo_editable() noexcept;
        [[nodiscard]] ControllerResult select(
            std::string_view logicalPath) noexcept;
        [[nodiscard]] std::optional<TexturePreview> preview(
            std::string_view logicalPath) noexcept;
        [[nodiscard]] std::optional<TexturePreview>
            selected_preview() noexcept;
        [[nodiscard]] std::optional<scene::SceneTextureMaterialSnapshot>
            selected_material() const noexcept;
        [[nodiscard]] SceneTextureLeaseResult bind_scene_materials(
            std::span<const scene::SceneTextureMaterialSnapshot>
                materials) noexcept;

        [[nodiscard]] const project_textures::ProjectTexturePipeline&
            pipeline() const noexcept;

    private:
        [[nodiscard]] ControllerResult publish_editable(
            const capability::SubsystemProfile& renderer,
            const Budgets& platformBudgets,
            bool persistSource) noexcept;
        [[nodiscard]] TextureEditResult edit_result(
            const authoring::texture::MutationResult& mutation) noexcept;
        void refresh_editable_selection() noexcept;
        [[nodiscard]] authoring::texture::TemporalPoint
            next_edit_time() const noexcept;

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
        project_texture_sources::ProjectTextureSourceStore source_store_;
        project_textures::ProjectTexturePipeline pipeline_;
        std::vector<TextureCatalogEntry> catalog_{};
        std::string selected_logical_path_{};
        std::unique_ptr<authoring::texture::TextureDocument>
            editable_document_{};
        std::string editable_logical_path_{};
        std::filesystem::path editable_source_path_{};
        authoring::texture::LayerHandle editable_selected_layer_{};
        bool editable_dirty_{};
    };

    enum class ControllerContractFailure : std::uint8_t
    {
        none,
        temporary_project,
        outside_assets_rejection,
        import,
        metrics,
        material_identity,
        thumbnail_generation,
        exact_restore,
        lease,
        historical_revision,
        restart_catalog,
        restart_reimport,
        stale_path,
        authoring_create,
        authoring_edit,
        authoring_unsaved_guard,
        authoring_undo_redo,
        authoring_layer_mask,
        authoring_save_reopen,
        authoring_cache_regeneration
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
        case ControllerContractFailure::metrics: return "metrics";
        case ControllerContractFailure::material_identity:
            return "material_identity";
        case ControllerContractFailure::thumbnail_generation:
            return "thumbnail_generation";
        case ControllerContractFailure::exact_restore: return "exact_restore";
        case ControllerContractFailure::lease: return "lease";
        case ControllerContractFailure::historical_revision:
            return "historical_revision";
        case ControllerContractFailure::restart_catalog:
            return "restart_catalog";
        case ControllerContractFailure::restart_reimport:
            return "restart_reimport";
        case ControllerContractFailure::stale_path: return "stale_path";
        case ControllerContractFailure::authoring_create: return "authoring_create";
        case ControllerContractFailure::authoring_edit: return "authoring_edit";
        case ControllerContractFailure::authoring_unsaved_guard: return "authoring_unsaved_guard";
        case ControllerContractFailure::authoring_undo_redo: return "authoring_undo_redo";
        case ControllerContractFailure::authoring_layer_mask: return "authoring_layer_mask";
        case ControllerContractFailure::authoring_save_reopen: return "authoring_save_reopen";
        case ControllerContractFailure::authoring_cache_regeneration: return "authoring_cache_regeneration";
        }
        return "unknown";
    }

    [[nodiscard]] ControllerContractFailure
        project_texture_controller_contract_failure() noexcept;
}
