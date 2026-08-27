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

export module editor.tilemap_workspace;

import authoring.tilemap;
import editor.project_textures;
import epoch.gui.tile_workspace;
import gui.engine;
import project.tilemap_pipeline;
import project.tilemap_source;
import render.canvas2d_tilemap;

export namespace epochengine::editor_tilemaps
{
    enum class ControllerCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_controller,
        source_failure,
        document_failure,
        pipeline_failure,
        texture_required,
        texture_project_mismatch,
        invalid_tile_extent,
        palette_required,
        layer_required,
        object_required,
        tool_not_applicable,
        operation_limit,
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
        case ControllerCode::source_failure: return "source_failure";
        case ControllerCode::document_failure: return "document_failure";
        case ControllerCode::pipeline_failure: return "pipeline_failure";
        case ControllerCode::texture_required: return "texture_required";
        case ControllerCode::texture_project_mismatch:
            return "texture_project_mismatch";
        case ControllerCode::invalid_tile_extent: return "invalid_tile_extent";
        case ControllerCode::palette_required: return "palette_required";
        case ControllerCode::layer_required: return "layer_required";
        case ControllerCode::object_required: return "object_required";
        case ControllerCode::tool_not_applicable: return "tool_not_applicable";
        case ControllerCode::operation_limit: return "operation_limit";
        case ControllerCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct ControllerLimits final
    {
        project_tilemap_sources::SourceLimits source{};
        project_tilemaps::TileMapPipelineLimits pipeline{};
        std::uint32_t maximum_palette_tiles_per_texture{4'096u};
        std::uint32_t maximum_fill_cells{262'144u};
        std::uint32_t maximum_visible_cell_widgets{4'096u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return source.valid() && pipeline.valid()
                && maximum_palette_tiles_per_texture != 0u
                && maximum_fill_cells != 0u
                && maximum_visible_cell_widgets != 0u
                && source.registry.maximum_assets
                    == pipeline.registry.maximum_assets
                && source.registry.maximum_path_bytes
                    == pipeline.registry.maximum_path_bytes
                && source.registry.maximum_project_id_bytes
                    == pipeline.registry.maximum_project_id_bytes;
        }
    };

    struct ControllerMetrics final
    {
        std::uint64_t commands{};
        std::uint64_t mutations{};
        std::uint64_t saves{};
        std::uint64_t compilations{};
        std::uint64_t preview_artifact_compilations{};
        std::uint64_t preview_compilations{};
        std::uint64_t rejected_commands{};
        std::uint64_t painted_cells{};
        std::uint64_t rendered_cell_widgets{};
    };

    struct ControllerResult final
    {
        ControllerCode code{ControllerCode::invalid_controller};
        authoring::tilemap::ResultCode document_code{
            authoring::tilemap::ResultCode::invalid_document};
        project_tilemap_sources::SourceCode source_code{
            project_tilemap_sources::SourceCode::invalid_store};
        project_tilemaps::PipelineCode pipeline_code{
            project_tilemaps::PipelineCode::invalid_pipeline};
        std::uint64_t affected_cells{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ControllerCode::ready
                || code == ControllerCode::unchanged;
        }
    };

    struct PublishResult final
    {
        ControllerResult result{};
        project_tilemap_sources::SourceLocation source{};
        project_tilemaps::TileMapPipelineResult compiled{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(result)
                && static_cast<bool>(source)
                && static_cast<bool>(compiled);
        }
    };

    struct RuntimePreviewResult final
    {
        ControllerResult result{};
        const asset::tilemap::CompiledTileMapArtifact* artifact{};
        canvas2d::tilemap_runtime::VisibleCompilation visible{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(result) && artifact
                && static_cast<bool>(visible);
        }
    };

    struct CanvasRenderOptions final
    {
        gui::WidgetBounds viewport{};
        bool interactive{true};
        bool show_inspector{true};
    };

    struct CanvasRenderResult final
    {
        bool visible{};
        bool input_captured{};
        bool changed{};
        std::uint32_t rendered_cells{};
        gui_lib::tile_workspace::CellCoordinate hovered_cell{
            gui_lib::tile_workspace::invalid_index,
            gui_lib::tile_workspace::invalid_index};
    };

    class TileMapWorkspaceController final
    {
    public:
        TileMapWorkspaceController(
            std::string projectId,
            std::filesystem::path projectRoot,
            std::string logicalPath = "Assets/Maps/main.epochmap",
            ControllerLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] std::string_view logical_path() const noexcept;
        [[nodiscard]] const ControllerLimits& limits() const noexcept;
        [[nodiscard]] ControllerMetrics metrics() const noexcept;
        [[nodiscard]] std::string_view status() const noexcept;
        [[nodiscard]] bool dirty() const noexcept;
        [[nodiscard]] bool has_document() const noexcept;
        [[nodiscard]] bool can_undo() const noexcept;
        [[nodiscard]] bool can_redo() const noexcept;

        [[nodiscard]] const authoring::tilemap::Document* document() const noexcept;
        [[nodiscard]] authoring::tilemap::DocumentSnapshot snapshot() const;
        [[nodiscard]] gui_lib::tile_workspace::WorkspaceState& ui_state() noexcept;
        [[nodiscard]] const gui_lib::tile_workspace::WorkspaceState&
            ui_state() const noexcept;
        [[nodiscard]] std::optional<gui_lib::tile_workspace::CellCoordinate>
            selected_cell() const noexcept;
        [[nodiscard]] const authoring::tilemap::LayerDescriptor*
            selected_layer_draft() const noexcept;
        [[nodiscard]] bool selected_layer_draft_dirty() const noexcept;
        [[nodiscard]] const authoring::tilemap::CollisionShape*
            selected_palette_collision_draft() const noexcept;
        [[nodiscard]] bool selected_palette_collision_draft_dirty()
            const noexcept;
        [[nodiscard]] std::optional<authoring::tilemap::MapObjectHandle>
            selected_object() const noexcept;
        [[nodiscard]] const authoring::tilemap::MapObjectDescriptor*
            selected_object_draft() const noexcept;
        [[nodiscard]] bool selected_object_draft_dirty() const noexcept;

        [[nodiscard]] ControllerResult open_or_create(
            authoring::tilemap::MapDescriptor descriptor = {}) noexcept;
        [[nodiscard]] ControllerResult create_new(
            authoring::tilemap::MapDescriptor descriptor = {}) noexcept;
        [[nodiscard]] ControllerResult reload() noexcept;
        [[nodiscard]] ControllerResult restore_context_handoff(
            std::span<const std::byte> serializedDocument,
            bool dirty) noexcept;
        void restore_view_state(
            std::uint8_t tool,
            std::uint32_t selectedPalette,
            std::uint32_t selectedLayer,
            float zoom,
            gui_lib::Vec2 pan) noexcept;
        [[nodiscard]] ControllerResult select_layer(
            std::uint32_t index) noexcept;
        [[nodiscard]] ControllerResult select_palette(
            std::uint32_t index) noexcept;
        [[nodiscard]] ControllerResult select_tool(
            gui_lib::tile_workspace::Tool tool) noexcept;
        [[nodiscard]] ControllerResult create_layer(
            std::string name = "Layer") noexcept;
        [[nodiscard]] ControllerResult stage_selected_layer(
            authoring::tilemap::LayerDescriptor descriptor) noexcept;
        [[nodiscard]] ControllerResult apply_selected_layer() noexcept;
        [[nodiscard]] ControllerResult duplicate_selected_layer() noexcept;
        [[nodiscard]] ControllerResult remove_selected_layer() noexcept;
        [[nodiscard]] ControllerResult attach_texture(
            const editor_project_textures::TextureCatalogEntry& texture,
            std::uint64_t textureProjectKey,
            authoring::tilemap::UInt2 tileExtent) noexcept;
        [[nodiscard]] ControllerResult apply_cell(
            gui_lib::tile_workspace::CellCoordinate coordinate) noexcept;
        [[nodiscard]] ControllerResult select_object(
            authoring::tilemap::MapObjectHandle handle) noexcept;
        void clear_object_selection() noexcept;
        [[nodiscard]] ControllerResult stage_selected_object(
            authoring::tilemap::MapObjectDescriptor descriptor) noexcept;
        [[nodiscard]] ControllerResult apply_selected_object() noexcept;
        [[nodiscard]] ControllerResult duplicate_selected_object() noexcept;
        [[nodiscard]] ControllerResult remove_selected_object() noexcept;
        [[nodiscard]] ControllerResult begin_object_drag(
            authoring::tilemap::MapObjectHandle handle,
            authoring::tilemap::Float2 mapPoint) noexcept;
        [[nodiscard]] ControllerResult update_object_drag(
            authoring::tilemap::Float2 mapPoint) noexcept;
        [[nodiscard]] ControllerResult end_object_drag(bool commit) noexcept;
        [[nodiscard]] bool object_drag_active() const noexcept;
        [[nodiscard]] ControllerResult stage_selected_palette_collision(
            authoring::tilemap::CollisionShape collision) noexcept;
        [[nodiscard]] ControllerResult apply_selected_palette_collision()
            noexcept;
        [[nodiscard]] ControllerResult set_selected_palette_collision(
            authoring::tilemap::CollisionShape collision) noexcept;
        [[nodiscard]] ControllerResult undo() noexcept;
        [[nodiscard]] ControllerResult redo() noexcept;
        [[nodiscard]] PublishResult save_and_publish() noexcept;
        [[nodiscard]] RuntimePreviewResult compile_runtime_preview(
            std::uint64_t simulatedMilliseconds) noexcept;

        void begin_canvas_pan(gui_lib::Vec2 pointer) noexcept;
        void update_canvas_pan(gui_lib::Vec2 pointer) noexcept;
        void end_canvas_pan() noexcept;
        [[nodiscard]] bool canvas_pan_active() const noexcept;
        void record_rendered_cell_widgets(std::uint32_t count) noexcept;

    private:
        [[nodiscard]] ControllerResult accept(
            authoring::tilemap::MutationResult mutation) noexcept;
        [[nodiscard]] ControllerResult reject(
            ControllerCode code,
            authoring::tilemap::ResultCode documentCode =
                authoring::tilemap::ResultCode::invalid_document,
            project_tilemap_sources::SourceCode sourceCode =
                project_tilemap_sources::SourceCode::invalid_store,
            project_tilemaps::PipelineCode pipelineCode =
                project_tilemaps::PipelineCode::invalid_pipeline) noexcept;
        void normalize_selection() noexcept;
        [[nodiscard]] std::optional<authoring::tilemap::LayerHandle>
            selected_layer() const noexcept;
        [[nodiscard]] std::optional<authoring::tilemap::PaletteEntryHandle>
            selected_palette() const noexcept;
        [[nodiscard]] ControllerResult fill_cell(
            authoring::tilemap::LayerHandle layer,
            authoring::tilemap::UInt2 coordinate,
            authoring::tilemap::CellValue replacement) noexcept;

        std::filesystem::path project_root_{};
        std::string logical_path_{};
        ControllerLimits limits_{};
        project_tilemap_sources::ProjectTileMapSourceStore source_;
        project_tilemaps::ProjectTileMapPipeline pipeline_;
        std::unique_ptr<authoring::tilemap::Document> document_{};
        gui_lib::tile_workspace::WorkspaceState ui_state_{};
        authoring::tilemap::DocumentRevision saved_revision_{};
        ControllerMetrics metrics_{};
        std::string status_{"Tile map is not open."};
        bool canvas_pan_active_{};
        gui_lib::Vec2 canvas_pan_pointer_{};
        std::optional<gui_lib::tile_workspace::CellCoordinate>
            selected_cell_{};
        std::optional<authoring::tilemap::LayerHandle>
            selected_layer_handle_{};
        std::optional<authoring::tilemap::LayerDescriptor>
            selected_layer_draft_{};
        bool selected_layer_draft_dirty_{};
        std::optional<authoring::tilemap::PaletteEntryHandle>
            selected_palette_handle_{};
        std::optional<authoring::tilemap::CollisionShape>
            selected_palette_collision_draft_{};
        bool selected_palette_collision_draft_dirty_{};
        std::optional<authoring::tilemap::MapObjectHandle>
            selected_object_{};
        std::optional<authoring::tilemap::MapObjectDescriptor>
            selected_object_draft_{};
        bool selected_object_draft_dirty_{};
        bool object_drag_active_{};
        authoring::tilemap::Float2 object_drag_offset_{};
        std::optional<asset::tilemap::CompiledTileMapArtifact>
            preview_artifact_{};
        authoring::tilemap::DocumentRevision preview_revision_{};
    };

    [[nodiscard]] CanvasRenderResult render_canvas_workspace(
        TileMapWorkspaceController& controller,
        const CanvasRenderOptions& options) noexcept;

    enum class ControllerContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        construction,
        create,
        layer,
        layer_edit,
        layer_duplicate,
        layer_remove,
        layer_lock,
        texture_project_guard,
        texture_attach,
        select,
        pencil,
        erase,
        fill,
        collision,
        object_create,
        object_edit,
        object_drag,
        object_duplicate,
        object_remove,
        undo_redo,
        source_publish,
        context_handoff,
        runtime_preview,
        reload,
        compiled_restore,
        metrics
    };

    [[nodiscard]] constexpr std::string_view
        controller_contract_failure_name(
            ControllerContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ControllerContractFailure::none: return "pass";
        case ControllerContractFailure::temporary_root: return "temporary_root";
        case ControllerContractFailure::construction: return "construction";
        case ControllerContractFailure::create: return "create";
        case ControllerContractFailure::layer: return "layer";
        case ControllerContractFailure::layer_edit: return "layer_edit";
        case ControllerContractFailure::layer_duplicate:
            return "layer_duplicate";
        case ControllerContractFailure::layer_remove: return "layer_remove";
        case ControllerContractFailure::layer_lock: return "layer_lock";
        case ControllerContractFailure::texture_project_guard:
            return "texture_project_guard";
        case ControllerContractFailure::texture_attach: return "texture_attach";
        case ControllerContractFailure::select: return "select";
        case ControllerContractFailure::pencil: return "pencil";
        case ControllerContractFailure::erase: return "erase";
        case ControllerContractFailure::fill: return "fill";
        case ControllerContractFailure::collision: return "collision";
        case ControllerContractFailure::object_create: return "object_create";
        case ControllerContractFailure::object_edit: return "object_edit";
        case ControllerContractFailure::object_drag: return "object_drag";
        case ControllerContractFailure::object_duplicate:
            return "object_duplicate";
        case ControllerContractFailure::object_remove: return "object_remove";
        case ControllerContractFailure::undo_redo: return "undo_redo";
        case ControllerContractFailure::source_publish: return "source_publish";
        case ControllerContractFailure::context_handoff: return "context_handoff";
        case ControllerContractFailure::runtime_preview: return "runtime_preview";
        case ControllerContractFailure::reload: return "reload";
        case ControllerContractFailure::compiled_restore:
            return "compiled_restore";
        case ControllerContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ControllerContractFailure
        tilemap_workspace_controller_contract_failure() noexcept;
}
