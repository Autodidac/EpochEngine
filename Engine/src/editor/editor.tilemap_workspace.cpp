/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

module editor.tilemap_workspace;

import project.asset_registry;

namespace epochengine::editor_tilemaps
{
    namespace
    {
        namespace fs = std::filesystem;
        using Tool = gui_lib::tile_workspace::Tool;
        using CellCoordinate = gui_lib::tile_workspace::CellCoordinate;

        [[nodiscard]] constexpr bool valid_coordinate(
            CellCoordinate coordinate) noexcept
        {
            return coordinate.x != gui_lib::tile_workspace::invalid_index
                && coordinate.y != gui_lib::tile_workspace::invalid_index;
        }

        [[nodiscard]] constexpr authoring::tilemap::UInt2 tile_coordinate(
            CellCoordinate coordinate) noexcept
        {
            return {coordinate.x, coordinate.y};
        }

        [[nodiscard]] std::uint32_t stable_material_key(
            std::uint64_t assetKey,
            std::uint64_t artifactRevision) noexcept
        {
            const std::uint64_t folded = assetKey
                ^ (assetKey >> 32u)
                ^ artifactRevision
                ^ (artifactRevision >> 32u);
            const std::uint32_t key = static_cast<std::uint32_t>(folded);
            return key == 0u ? 1u : key;
        }

        [[nodiscard]] std::string texture_set_name(
            std::string_view logicalPath)
        {
            const fs::path path{logicalPath};
            std::string name = path.stem().generic_string();
            if (name.empty())
                name = "Tiles";
            return name;
        }

        [[nodiscard]] std::uint64_t packed_coordinate(
            authoring::tilemap::UInt2 coordinate) noexcept
        {
            return (static_cast<std::uint64_t>(coordinate.y) << 32u)
                | coordinate.x;
        }

        [[nodiscard]] gui_lib::Vec2 gui_lib_point(gui::Vec2 point) noexcept
        {
            return {point.x, point.y};
        }

        [[nodiscard]] gui::Vec2 gui_point(gui_lib::Vec2 point) noexcept
        {
            return {point.x, point.y};
        }

        [[nodiscard]] gui::Vec2 gui_size(gui_lib::Rect rect) noexcept
        {
            return {rect.size.x, rect.size.y};
        }

        [[nodiscard]] bool point_in_rect(
            gui::Vec2 point,
            gui_lib::Rect rect) noexcept
        {
            return gui_lib::contains(rect, gui_lib_point(point));
        }

        [[nodiscard]] gui_lib::Rect object_canvas_rect(
            const gui_lib::tile_workspace::TileCanvasLayout& canvas,
            const authoring::tilemap::MapObjectDescriptor& object) noexcept
        {
            const float cosine = std::abs(std::cos(object.rotation_radians));
            const float sine = std::abs(std::sin(object.rotation_radians));
            const float width = (object.size.x * cosine + object.size.y * sine)
                * canvas.cell_pixels;
            const float height = (object.size.x * sine + object.size.y * cosine)
                * canvas.cell_pixels;
            const float displayWidth = (std::max)(8.0f, width);
            const float displayHeight = (std::max)(8.0f, height);
            const gui_lib::Vec2 center{
                canvas.origin.x + object.position.x * canvas.cell_pixels,
                canvas.origin.y + object.position.y * canvas.cell_pixels};
            return {
                {center.x - displayWidth * 0.5f,
                 center.y - displayHeight * 0.5f},
                {displayWidth, displayHeight}};
        }

        [[nodiscard]] gui_lib::Rect clipped_rect(
            gui_lib::Rect value,
            gui_lib::Rect clip) noexcept
        {
            const float left = (std::max)(value.position.x, clip.position.x);
            const float top = (std::max)(value.position.y, clip.position.y);
            const float right = (std::min)(
                value.position.x + value.size.x,
                clip.position.x + clip.size.x);
            const float bottom = (std::min)(
                value.position.y + value.size.y,
                clip.position.y + clip.size.y);
            if (right <= left || bottom <= top)
                return {};
            return {{left, top}, {right - left, bottom - top}};
        }

        [[nodiscard]] std::string map_status_line(
            const TileMapWorkspaceController& controller,
            const authoring::tilemap::DocumentSnapshot& snapshot)
        {
            std::string line = controller.dirty() ? "Modified | " : "Saved | ";
            line += std::to_string(snapshot.descriptor.extent_tiles.x);
            line += "x";
            line += std::to_string(snapshot.descriptor.extent_tiles.y);
            line += " | ";
            line += std::to_string(snapshot.layers.size());
            line += " layers | ";
            line += std::to_string(snapshot.palette.size());
            line += " tiles | ";
            line += std::to_string(snapshot.objects.size());
            line += " objects";
            return line;
        }

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
                path /= "epoch_editor_tilemap_workspace_contract_"
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
    }

    TileMapWorkspaceController::TileMapWorkspaceController(
        std::string projectId,
        std::filesystem::path projectRoot,
        std::string logicalPath,
        ControllerLimits limits) noexcept
        : project_root_(std::move(projectRoot)),
          logical_path_(std::move(logicalPath)),
          limits_(limits),
          source_(projectId, project_root_, limits_.source),
          pipeline_(
              std::move(projectId),
              project_root_.generic_string(),
              limits_.pipeline)
    {
        if (valid())
            status_ = "Tile map source is ready to open.";
        else
            status_ = "Tile map controller configuration is invalid.";
    }

    bool TileMapWorkspaceController::valid() const noexcept
    {
        return limits_.valid() && source_.valid() && pipeline_.valid()
            && source_.project_id() == pipeline_.project_id()
            && project_root_ == source_.project_root()
            && !logical_path_.empty();
    }

    std::string_view TileMapWorkspaceController::project_id() const noexcept
    {
        return source_.project_id();
    }

    const std::filesystem::path&
        TileMapWorkspaceController::project_root() const noexcept
    {
        return project_root_;
    }

    std::string_view TileMapWorkspaceController::logical_path() const noexcept
    {
        return logical_path_;
    }

    const ControllerLimits&
        TileMapWorkspaceController::limits() const noexcept
    {
        return limits_;
    }

    ControllerMetrics TileMapWorkspaceController::metrics() const noexcept
    {
        return metrics_;
    }

    std::string_view TileMapWorkspaceController::status() const noexcept
    {
        return status_;
    }

    bool TileMapWorkspaceController::dirty() const noexcept
    {
        return document_ && document_->valid()
            && document_->revision() != saved_revision_;
    }

    bool TileMapWorkspaceController::has_document() const noexcept
    {
        return document_ && document_->valid();
    }

    bool TileMapWorkspaceController::can_undo() const noexcept
    {
        return has_document() && document_->can_undo();
    }

    bool TileMapWorkspaceController::can_redo() const noexcept
    {
        return has_document() && document_->can_redo();
    }

    const authoring::tilemap::Document*
        TileMapWorkspaceController::document() const noexcept
    {
        return document_.get();
    }

    authoring::tilemap::DocumentSnapshot
        TileMapWorkspaceController::snapshot() const
    {
        return has_document()
            ? document_->snapshot()
            : authoring::tilemap::DocumentSnapshot{};
    }

    gui_lib::tile_workspace::WorkspaceState&
        TileMapWorkspaceController::ui_state() noexcept
    {
        return ui_state_;
    }

    const gui_lib::tile_workspace::WorkspaceState&
        TileMapWorkspaceController::ui_state() const noexcept
    {
        return ui_state_;
    }

    std::optional<CellCoordinate>
        TileMapWorkspaceController::selected_cell() const noexcept
    {
        return selected_cell_;
    }

    const authoring::tilemap::LayerDescriptor*
        TileMapWorkspaceController::selected_layer_draft() const noexcept
    {
        return selected_layer_draft_
            ? std::addressof(*selected_layer_draft_)
            : nullptr;
    }

    bool TileMapWorkspaceController::selected_layer_draft_dirty() const noexcept
    {
        return selected_layer_draft_dirty_;
    }

    const authoring::tilemap::CollisionShape*
        TileMapWorkspaceController::selected_palette_collision_draft()
            const noexcept
    {
        return selected_palette_collision_draft_
            ? std::addressof(*selected_palette_collision_draft_)
            : nullptr;
    }

    bool TileMapWorkspaceController::selected_palette_collision_draft_dirty()
        const noexcept
    {
        return selected_palette_collision_draft_dirty_;
    }

    std::optional<authoring::tilemap::MapObjectHandle>
        TileMapWorkspaceController::selected_object() const noexcept
    {
        return selected_object_;
    }

    const authoring::tilemap::MapObjectDescriptor*
        TileMapWorkspaceController::selected_object_draft() const noexcept
    {
        return selected_object_draft_
            ? std::addressof(*selected_object_draft_)
            : nullptr;
    }

    bool TileMapWorkspaceController::selected_object_draft_dirty() const noexcept
    {
        return selected_object_draft_dirty_;
    }

    ControllerResult TileMapWorkspaceController::open_or_create(
        authoring::tilemap::MapDescriptor descriptor) noexcept
    {
        ++metrics_.commands;
        if (!valid())
            return reject(ControllerCode::invalid_controller);

        auto loaded = source_.load(logical_path_);
        if (loaded)
        {
            document_ = std::move(loaded.document);
            saved_revision_ = document_->revision();
            selected_cell_.reset();
            selected_layer_handle_.reset();
            selected_layer_draft_.reset();
            selected_layer_draft_dirty_ = false;
            selected_palette_handle_.reset();
            selected_palette_collision_draft_.reset();
            selected_palette_collision_draft_dirty_ = false;
            clear_object_selection();
            normalize_selection();
            status_ = "Opened tile map source from Assets/Maps.";
            return {
                ControllerCode::ready,
                authoring::tilemap::ResultCode::success,
                loaded.code,
                project_tilemaps::PipelineCode::ready};
        }
        if (loaded.code != project_tilemap_sources::SourceCode::not_found)
        {
            return reject(
                ControllerCode::source_failure,
                authoring::tilemap::ResultCode::invalid_document,
                loaded.code);
        }
        --metrics_.commands;
        return create_new(std::move(descriptor));
    }

    ControllerResult TileMapWorkspaceController::create_new(
        authoring::tilemap::MapDescriptor descriptor) noexcept
    {
        ++metrics_.commands;
        if (!valid())
            return reject(ControllerCode::invalid_controller);

        std::uint32_t documentIndex = static_cast<std::uint32_t>(
            pipeline_.project_key() ^ (pipeline_.project_key() >> 32u));
        if (documentIndex
            == authoring::tilemap::DocumentHandle::invalid_index)
        {
            documentIndex = 0u;
        }
        auto created = authoring::tilemap::Document::create(
            {documentIndex, 1u}, std::move(descriptor), limits_.source.document);
        if (!created)
        {
            return reject(
                ControllerCode::document_failure, created.code);
        }

        authoring::tilemap::LayerDescriptor layer{};
        layer.name = "Ground";
        layer.collision_source = true;
        layer.extent_tiles = created.document->descriptor().extent_tiles;
        layer.chunk_extent =
            created.document->descriptor().default_chunk_extent;
        auto layerResult = created.document->create_layer(std::move(layer));
        if (!layerResult)
        {
            return reject(
                ControllerCode::document_failure, layerResult.code);
        }

        document_ = std::move(created.document);
        saved_revision_ = {};
        selected_cell_.reset();
        selected_layer_handle_.reset();
        selected_layer_draft_.reset();
        selected_layer_draft_dirty_ = false;
        selected_palette_handle_.reset();
        selected_palette_collision_draft_.reset();
        selected_palette_collision_draft_dirty_ = false;
        clear_object_selection();
        ui_state_ = {};
        normalize_selection();
        ++metrics_.mutations;
        status_ = "Created an unsaved temporal tile map with a Ground layer.";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::not_found,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::reload() noexcept
    {
        ++metrics_.commands;
        if (!valid())
            return reject(ControllerCode::invalid_controller);
        auto loaded = source_.load(logical_path_);
        if (!loaded)
        {
            return reject(
                ControllerCode::source_failure,
                authoring::tilemap::ResultCode::invalid_document,
                loaded.code);
        }
        document_ = std::move(loaded.document);
        saved_revision_ = document_->revision();
        selected_cell_.reset();
        selected_layer_handle_.reset();
        selected_layer_draft_.reset();
        selected_layer_draft_dirty_ = false;
        selected_palette_handle_.reset();
        selected_palette_collision_draft_.reset();
        selected_palette_collision_draft_dirty_ = false;
        clear_object_selection();
        normalize_selection();
        status_ = "Reloaded the saved temporal tile map source.";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            loaded.code,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::restore_context_handoff(
        std::span<const std::byte> serializedDocument,
        bool dirty) noexcept
    {
        ++metrics_.commands;
        if (!valid() || serializedDocument.empty())
            return reject(ControllerCode::invalid_controller);
        auto restored = authoring::tilemap::Document::deserialize(
            serializedDocument, limits_.source.document);
        if (!restored)
        {
            return reject(
                ControllerCode::document_failure, restored.code);
        }
        document_ = std::move(restored.document);
        saved_revision_ = dirty
            ? authoring::tilemap::DocumentRevision{}
            : document_->revision();
        selected_cell_.reset();
        selected_layer_handle_.reset();
        selected_layer_draft_.reset();
        selected_layer_draft_dirty_ = false;
        selected_palette_handle_.reset();
        selected_palette_collision_draft_.reset();
        selected_palette_collision_draft_dirty_ = false;
        clear_object_selection();
        preview_artifact_.reset();
        preview_revision_ = {};
        normalize_selection();
        status_ = dirty
            ? "Restored unsaved temporal tile-map history across the context switch."
            : "Restored the saved temporal tile map across the context switch.";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }


    void TileMapWorkspaceController::restore_view_state(
        std::uint8_t tool,
        std::uint32_t selectedPalette,
        std::uint32_t selectedLayer,
        float zoom,
        gui_lib::Vec2 pan) noexcept
    {
        const auto maximumTool = static_cast<std::uint8_t>(Tool::collision);
        ui_state_.tool = tool <= maximumTool
            ? static_cast<Tool>(tool)
            : Tool::pencil;
        ui_state_.selected_palette = selectedPalette;
        ui_state_.selected_layer = selectedLayer;
        ui_state_.zoom = zoom;
        ui_state_.pan = pan;
        selected_layer_handle_.reset();
        selected_layer_draft_.reset();
        selected_layer_draft_dirty_ = false;
        selected_palette_handle_.reset();
        selected_palette_collision_draft_.reset();
        selected_palette_collision_draft_dirty_ = false;
        normalize_selection();
    }


    ControllerResult TileMapWorkspaceController::select_layer(
        std::uint32_t index) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        const auto current = document_->snapshot();
        if (index >= current.layers.size())
            return reject(ControllerCode::layer_required);
        ui_state_.selected_layer = index;
        selected_layer_handle_ = current.layers[index].handle;
        selected_layer_draft_ = current.layers[index].descriptor;
        selected_layer_draft_dirty_ = false;
        selected_cell_.reset();
        clear_object_selection();
        status_ = "Selected layer "
            + current.layers[index].descriptor.name + ".";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::select_palette(
        std::uint32_t index) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        const auto current = document_->snapshot();
        if (index >= current.palette.size())
            return reject(ControllerCode::palette_required);
        ui_state_.selected_palette = index;
        selected_palette_handle_ = current.palette[index].handle;
        selected_palette_collision_draft_ =
            current.palette[index].descriptor.collision;
        selected_palette_collision_draft_dirty_ = false;
        status_ = "Selected palette tile "
            + current.palette[index].descriptor.name + ".";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::select_tool(
        Tool tool) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        ui_state_.tool = tool;
        status_ = std::string(gui_lib::tile_workspace::tool_name(tool))
            + " tool selected.";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::create_layer(
        std::string name) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        const auto current = document_->snapshot();
        if (name.empty() || name == "Layer")
            name = "Layer " + std::to_string(current.layers.size() + 1u);
        authoring::tilemap::LayerDescriptor descriptor{};
        descriptor.name = std::move(name);
        descriptor.extent_tiles = document_->descriptor().extent_tiles;
        descriptor.chunk_extent = document_->descriptor().default_chunk_extent;
        descriptor.draw_layer = static_cast<std::int32_t>(current.layers.size());
        const auto mutation = document_->create_layer(std::move(descriptor));
        const ControllerResult accepted = accept(mutation);
        if (accepted)
        {
            normalize_selection();
            const auto refreshed = document_->snapshot();
            ui_state_.selected_layer = static_cast<std::uint32_t>(
                refreshed.layers.size() - 1u);
            selected_layer_handle_ = mutation.layer;
            selected_layer_draft_ = mutation.layer
                ? document_->layer(mutation.layer)
                : std::optional<authoring::tilemap::LayerDescriptor>{};
            selected_layer_draft_dirty_ = false;
            selected_cell_.reset();
            clear_object_selection();
            status_ = "Created a temporal tile layer.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::stage_selected_layer(
        authoring::tilemap::LayerDescriptor descriptor) noexcept
    {
        ++metrics_.commands;
        const auto layer = selected_layer();
        if (!has_document() || !layer)
            return reject(ControllerCode::layer_required);
        const auto current = document_->layer(*layer);
        if (!current)
        {
            selected_layer_handle_.reset();
            selected_layer_draft_.reset();
            selected_layer_draft_dirty_ = false;
            return reject(ControllerCode::layer_required);
        }
        selected_layer_draft_dirty_ = descriptor != *current;
        selected_layer_draft_ = std::move(descriptor);
        status_ = selected_layer_draft_dirty_
            ? "Layer changes are staged. Apply to commit one semantic operation."
            : "Layer properties match the committed document.";
        return {
            selected_layer_draft_dirty_
                ? ControllerCode::ready
                : ControllerCode::unchanged,
            selected_layer_draft_dirty_
                ? authoring::tilemap::ResultCode::success
                : authoring::tilemap::ResultCode::unchanged,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::apply_selected_layer() noexcept
    {
        ++metrics_.commands;
        const auto layer = selected_layer();
        if (!has_document() || !layer || !selected_layer_draft_)
            return reject(ControllerCode::layer_required);
        if (!selected_layer_draft_dirty_)
        {
            return {
                ControllerCode::unchanged,
                authoring::tilemap::ResultCode::unchanged,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready};
        }
        ControllerResult accepted = accept(document_->set_layer_properties(
            *layer, *selected_layer_draft_));
        if (accepted)
        {
            selected_layer_draft_ = document_->layer(*layer);
            selected_layer_draft_dirty_ = false;
            status_ = "Committed the staged tile-layer properties.";
        }
        return accepted;
    }

    ControllerResult
        TileMapWorkspaceController::duplicate_selected_layer() noexcept
    {
        ++metrics_.commands;
        const auto layer = selected_layer();
        if (!has_document() || !layer || !selected_layer_draft_)
            return reject(ControllerCode::layer_required);

        authoring::tilemap::LayerDescriptor duplicate =
            *selected_layer_draft_;
        const auto current = document_->snapshot();
        const std::size_t maximumName = document_->limits().maximum_name_bytes;
        constexpr std::string_view copySuffix{" Copy"};
        if (maximumName <= copySuffix.size())
            return reject(ControllerCode::operation_limit);

        const std::string baseName = duplicate.name;
        auto nameExists = [&](std::string_view name)
        {
            return std::any_of(
                current.layers.begin(),
                current.layers.end(),
                [&](const auto& candidate)
                {
                    return candidate.descriptor.name == name;
                });
        };
        for (std::uint32_t ordinal = 1u;; ++ordinal)
        {
            std::string suffix = ordinal == 1u
                ? std::string{copySuffix}
                : std::string{copySuffix} + " " + std::to_string(ordinal);
            if (suffix.size() >= maximumName)
                return reject(ControllerCode::operation_limit);
            duplicate.name = baseName.substr(
                0u, (std::min)(baseName.size(), maximumName - suffix.size()));
            duplicate.name += suffix;
            if (!nameExists(duplicate.name))
                break;
            if (ordinal >= document_->limits().maximum_layers)
                return reject(ControllerCode::operation_limit);
        }
        const auto highestLayer = std::max_element(
            current.layers.begin(),
            current.layers.end(),
            [](const auto& left, const auto& right)
            {
                return left.descriptor.draw_layer
                    < right.descriptor.draw_layer;
            });
        duplicate.draw_layer = highestLayer == current.layers.end()
            ? 0
            : highestLayer->descriptor.draw_layer == (std::numeric_limits<
                    std::int32_t>::max)()
                ? highestLayer->descriptor.draw_layer
                : highestLayer->descriptor.draw_layer + 1;

        const auto mutation = document_->create_layer(std::move(duplicate));
        ControllerResult accepted = accept(mutation);
        if (accepted && mutation.layer)
        {
            const auto refreshed = document_->snapshot();
            const auto selected = std::find_if(
                refreshed.layers.begin(),
                refreshed.layers.end(),
                [&](const auto& candidate)
                {
                    return candidate.handle == mutation.layer;
                });
            selected_layer_handle_ = mutation.layer;
            selected_layer_draft_ = document_->layer(mutation.layer);
            selected_layer_draft_dirty_ = false;
            if (selected != refreshed.layers.end())
            {
                ui_state_.selected_layer = static_cast<std::uint32_t>(
                    selected - refreshed.layers.begin());
            }
            selected_cell_.reset();
            clear_object_selection();
            status_ = "Duplicated and selected the tile layer.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::remove_selected_layer() noexcept
    {
        ++metrics_.commands;
        const auto layer = selected_layer();
        if (!has_document() || !layer)
            return reject(ControllerCode::layer_required);
        const auto current = document_->snapshot();
        if (current.layers.size() <= 1u)
            return reject(ControllerCode::operation_limit);

        const std::uint32_t removedIndex = ui_state_.selected_layer;
        ControllerResult accepted = accept(document_->remove_layer(*layer));
        if (accepted)
        {
            selected_layer_handle_.reset();
            selected_layer_draft_.reset();
            selected_layer_draft_dirty_ = false;
            ui_state_.selected_layer = removedIndex == 0u
                ? 0u
                : removedIndex - 1u;
            selected_cell_.reset();
            clear_object_selection();
            normalize_selection();
            status_ = "Removed the selected tile layer.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::attach_texture(
        const editor_project_textures::TextureCatalogEntry& texture,
        std::uint64_t textureProjectKey,
        authoring::tilemap::UInt2 tileExtent) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        if (!texture)
            return reject(ControllerCode::texture_required);
        if (textureProjectKey == 0u
            || textureProjectKey != pipeline_.project_key())
        {
            return reject(ControllerCode::texture_project_mismatch);
        }
        if (tileExtent.x == 0u || tileExtent.y == 0u
            || tileExtent.x > texture.width || tileExtent.y > texture.height
            || texture.width % tileExtent.x != 0u
            || texture.height % tileExtent.y != 0u)
        {
            return reject(ControllerCode::invalid_tile_extent);
        }

        const std::uint32_t columns = texture.width / tileExtent.x;
        const std::uint32_t rows = texture.height / tileExtent.y;
        const std::uint64_t tileCount64 =
            static_cast<std::uint64_t>(columns) * rows;
        const auto current = document_->snapshot();
        if (tileCount64 == 0u
            || tileCount64 > limits_.maximum_palette_tiles_per_texture
            || tileCount64 > (std::numeric_limits<std::uint32_t>::max)()
            || current.palette.size() + tileCount64
                > document_->limits().maximum_palette_entries)
        {
            return reject(ControllerCode::operation_limit);
        }

        authoring::tilemap::TextureDependency dependency{};
        dependency.logical_path = texture.logical_path;
        dependency.artifact_key = texture.artifact_key.words;
        dependency.project_key = pipeline_.project_key();
        dependency.asset_key = texture.logical.asset_key;
        dependency.artifact_revision = texture.logical.artifact_revision;
        dependency.stable_material_key = stable_material_key(
            texture.logical.asset_key, texture.logical.artifact_revision);
        dependency.texture_extent = {texture.width, texture.height};

        authoring::tilemap::TileSetDescriptor tileSet{};
        tileSet.name = texture_set_name(texture.logical_path);
        tileSet.texture = std::move(dependency);
        tileSet.tile_extent = tileExtent;
        tileSet.grid = {columns, rows};
        tileSet.tile_count = static_cast<std::uint32_t>(tileCount64);
        const auto createdTileSet = document_->create_tile_set(
            std::move(tileSet));
        if (!createdTileSet)
        {
            return reject(
                ControllerCode::document_failure, createdTileSet.code);
        }

        const std::uint32_t firstPalette = static_cast<std::uint32_t>(
            current.palette.size());
        for (std::uint32_t localTile = 0u;
             localTile < static_cast<std::uint32_t>(tileCount64);
             ++localTile)
        {
            authoring::tilemap::PaletteEntryDescriptor palette{};
            palette.name = texture_set_name(texture.logical_path)
                + " " + std::to_string(localTile);
            palette.tile_set = createdTileSet.tile_set;
            palette.local_tile = localTile;
            const auto createdPalette = document_->create_palette_entry(
                std::move(palette));
            if (!createdPalette)
            {
                return reject(
                    ControllerCode::document_failure,
                    createdPalette.code);
            }
        }

        metrics_.mutations += tileCount64 + 1u;
        normalize_selection();
        ui_state_.selected_palette = firstPalette;
        status_ = "Attached " + texture.logical_path + " as "
            + std::to_string(tileCount64) + " palette tiles.";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready,
            tileCount64};
    }

    ControllerResult TileMapWorkspaceController::apply_cell(
        CellCoordinate coordinate) noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !valid_coordinate(coordinate))
            return reject(ControllerCode::invalid_controller);
        const auto layer = selected_layer();
        if (!layer)
            return reject(ControllerCode::layer_required);
        const authoring::tilemap::UInt2 mapCoordinate =
            tile_coordinate(coordinate);
        const auto extent = document_->descriptor().extent_tiles;
        if (mapCoordinate.x >= extent.x || mapCoordinate.y >= extent.y)
        {
            return reject(
                ControllerCode::document_failure,
                authoring::tilemap::ResultCode::out_of_bounds);
        }

        if (ui_state_.tool == Tool::select)
        {
            selected_cell_ = coordinate;
            status_ = "Selected cell " + std::to_string(coordinate.x)
                + ", " + std::to_string(coordinate.y) + ".";
            return {
                ControllerCode::ready,
                authoring::tilemap::ResultCode::success,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready};
        }
        if (ui_state_.tool == Tool::collision)
        {
            const auto palette = selected_palette();
            if (!palette)
                return reject(ControllerCode::palette_required);
            auto descriptor = document_->palette_entry(*palette);
            if (!descriptor)
                return reject(ControllerCode::palette_required);
            authoring::tilemap::CollisionShape collision =
                descriptor->collision;
            collision.kind = descriptor->collision.kind
                    == asset::tilemap::CollisionKind::none
                ? asset::tilemap::CollisionKind::full_cell
                : asset::tilemap::CollisionKind::none;
            return set_selected_palette_collision(collision);
        }
        if (ui_state_.tool == Tool::object)
        {
            authoring::tilemap::MapObjectDescriptor object{};
            object.name = "Object "
                + std::to_string(document_->metrics().active_objects + 1u);
            object.position = {
                static_cast<float>(mapCoordinate.x) + 0.5f,
                static_cast<float>(mapCoordinate.y) + 0.5f};
            const auto mutation = document_->create_object(std::move(object));
            ControllerResult accepted = accept(mutation);
            if (accepted && mutation.object)
            {
                selected_object_ = mutation.object;
                selected_object_draft_ = document_->object(mutation.object);
                selected_object_draft_dirty_ = false;
                selected_cell_.reset();
                status_ = "Created and selected a temporal map object.";
            }
            return accepted;
        }

        authoring::tilemap::CellValue replacement{};
        if (ui_state_.tool != Tool::eraser)
        {
            const auto palette = selected_palette();
            if (!palette)
                return reject(ControllerCode::palette_required);
            replacement.palette = *palette;
        }
        if (ui_state_.tool == Tool::fill)
            return fill_cell(*layer, mapCoordinate, replacement);
        if (ui_state_.tool != Tool::pencil
            && ui_state_.tool != Tool::eraser)
        {
            return reject(ControllerCode::tool_not_applicable);
        }

        const std::array edit{
            authoring::tilemap::CellEdit{mapCoordinate, replacement}};
        ControllerResult accepted = accept(
            document_->paint_cells(*layer, edit));
        if (accepted)
            selected_cell_ = coordinate;
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::select_object(
        authoring::tilemap::MapObjectHandle handle) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        const auto descriptor = document_->object(handle);
        if (!descriptor)
            return reject(ControllerCode::object_required);
        selected_object_ = handle;
        selected_object_draft_ = *descriptor;
        selected_object_draft_dirty_ = false;
        object_drag_active_ = false;
        object_drag_offset_ = {};
        selected_cell_.reset();
        status_ = "Selected map object " + descriptor->name + ".";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    void TileMapWorkspaceController::clear_object_selection() noexcept
    {
        selected_object_.reset();
        selected_object_draft_.reset();
        selected_object_draft_dirty_ = false;
        object_drag_active_ = false;
        object_drag_offset_ = {};
    }

    ControllerResult TileMapWorkspaceController::stage_selected_object(
        authoring::tilemap::MapObjectDescriptor descriptor) noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !selected_object_)
            return reject(ControllerCode::object_required);
        const auto current = document_->object(*selected_object_);
        if (!current)
        {
            clear_object_selection();
            return reject(ControllerCode::object_required);
        }
        selected_object_draft_dirty_ = descriptor != *current;
        selected_object_draft_ = std::move(descriptor);
        status_ = selected_object_draft_dirty_
            ? "Map object changes are staged. Apply to commit one semantic operation."
            : "Map object matches the committed document.";
        return {
            selected_object_draft_dirty_
                ? ControllerCode::ready
                : ControllerCode::unchanged,
            selected_object_draft_dirty_
                ? authoring::tilemap::ResultCode::success
                : authoring::tilemap::ResultCode::unchanged,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::apply_selected_object() noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !selected_object_ || !selected_object_draft_)
            return reject(ControllerCode::object_required);
        if (!selected_object_draft_dirty_)
        {
            return {
                ControllerCode::unchanged,
                authoring::tilemap::ResultCode::unchanged,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready};
        }
        ControllerResult accepted = accept(document_->set_object_properties(
            *selected_object_, *selected_object_draft_));
        if (accepted)
        {
            selected_object_draft_ = document_->object(*selected_object_);
            selected_object_draft_dirty_ = false;
            status_ = "Committed the staged map object properties.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::duplicate_selected_object() noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !selected_object_ || !selected_object_draft_)
            return reject(ControllerCode::object_required);
        authoring::tilemap::MapObjectDescriptor duplicate =
            *selected_object_draft_;
        constexpr std::string_view suffix{" Copy"};
        const std::size_t maximumName = document_->limits().maximum_name_bytes;
        if (maximumName <= suffix.size())
            return reject(ControllerCode::operation_limit);
        if (duplicate.name.size() + suffix.size() > maximumName)
            duplicate.name.resize(maximumName - suffix.size());
        duplicate.name += suffix;
        duplicate.position.x += 0.5f;
        duplicate.position.y += 0.5f;
        const auto mutation = document_->create_object(std::move(duplicate));
        ControllerResult accepted = accept(mutation);
        if (accepted && mutation.object)
        {
            selected_object_ = mutation.object;
            selected_object_draft_ = document_->object(mutation.object);
            selected_object_draft_dirty_ = false;
            selected_cell_.reset();
            status_ = "Duplicated and selected the map object.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::remove_selected_object() noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !selected_object_)
            return reject(ControllerCode::object_required);
        ControllerResult accepted = accept(
            document_->remove_object(*selected_object_));
        if (accepted)
        {
            clear_object_selection();
            status_ = "Removed the selected map object.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::begin_object_drag(
        authoring::tilemap::MapObjectHandle handle,
        authoring::tilemap::Float2 mapPoint) noexcept
    {
        ++metrics_.commands;
        if (!has_document()
            || !std::isfinite(mapPoint.x)
            || !std::isfinite(mapPoint.y))
        {
            return reject(ControllerCode::object_required);
        }
        const auto descriptor = document_->object(handle);
        if (!descriptor)
            return reject(ControllerCode::object_required);
        selected_object_ = handle;
        selected_object_draft_ = *descriptor;
        selected_object_draft_dirty_ = false;
        selected_cell_.reset();
        object_drag_offset_ = {
            mapPoint.x - descriptor->position.x,
            mapPoint.y - descriptor->position.y};
        object_drag_active_ = true;
        status_ = "Dragging map object " + descriptor->name
            + "; release commits one semantic operation.";
        return {
            ControllerCode::ready,
            authoring::tilemap::ResultCode::success,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::update_object_drag(
        authoring::tilemap::Float2 mapPoint) noexcept
    {
        if (!has_document() || !object_drag_active_
            || !selected_object_ || !selected_object_draft_
            || !std::isfinite(mapPoint.x)
            || !std::isfinite(mapPoint.y))
        {
            return reject(ControllerCode::object_required);
        }

        auto staged = *selected_object_draft_;
        const float cosine = std::abs(std::cos(staged.rotation_radians));
        const float sine = std::abs(std::sin(staged.rotation_radians));
        const float halfWidth =
            (staged.size.x * cosine + staged.size.y * sine) * 0.5f;
        const float halfHeight =
            (staged.size.x * sine + staged.size.y * cosine) * 0.5f;
        const auto extent = document_->descriptor().extent_tiles;
        const float mapWidth = static_cast<float>(extent.x);
        const float mapHeight = static_cast<float>(extent.y);
        const float minimumX = (std::min)(halfWidth, mapWidth * 0.5f);
        const float maximumX = (std::max)(minimumX, mapWidth - minimumX);
        const float minimumY = (std::min)(halfHeight, mapHeight * 0.5f);
        const float maximumY = (std::max)(minimumY, mapHeight - minimumY);
        staged.position = {
            (std::clamp)(
                mapPoint.x - object_drag_offset_.x,
                minimumX,
                maximumX),
            (std::clamp)(
                mapPoint.y - object_drag_offset_.y,
                minimumY,
                maximumY)};

        const auto committed = document_->object(*selected_object_);
        if (!committed)
        {
            clear_object_selection();
            return reject(ControllerCode::object_required);
        }
        const bool changed = staged != *selected_object_draft_;
        selected_object_draft_ = std::move(staged);
        selected_object_draft_dirty_ =
            *selected_object_draft_ != *committed;
        if (changed)
            status_ = "Previewing the staged map-object transform.";
        return {
            changed ? ControllerCode::ready : ControllerCode::unchanged,
            changed
                ? authoring::tilemap::ResultCode::success
                : authoring::tilemap::ResultCode::unchanged,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult TileMapWorkspaceController::end_object_drag(
        bool commit) noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !object_drag_active_
            || !selected_object_ || !selected_object_draft_)
        {
            return reject(ControllerCode::object_required);
        }
        object_drag_active_ = false;
        object_drag_offset_ = {};
        if (commit)
            return apply_selected_object();

        const auto committed = document_->object(*selected_object_);
        if (!committed)
        {
            clear_object_selection();
            return reject(ControllerCode::object_required);
        }
        selected_object_draft_ = *committed;
        selected_object_draft_dirty_ = false;
        status_ = "Cancelled the staged map-object drag.";
        return {
            ControllerCode::unchanged,
            authoring::tilemap::ResultCode::unchanged,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    bool TileMapWorkspaceController::object_drag_active() const noexcept
    {
        return object_drag_active_;
    }

    ControllerResult
        TileMapWorkspaceController::stage_selected_palette_collision(
            authoring::tilemap::CollisionShape collision) noexcept
    {
        ++metrics_.commands;
        const auto palette = selected_palette();
        if (!has_document() || !palette)
            return reject(ControllerCode::palette_required);
        const auto current = document_->palette_entry(*palette);
        if (!current)
        {
            selected_palette_handle_.reset();
            selected_palette_collision_draft_.reset();
            selected_palette_collision_draft_dirty_ = false;
            return reject(ControllerCode::palette_required);
        }
        selected_palette_collision_draft_dirty_ =
            collision != current->collision;
        selected_palette_collision_draft_ = std::move(collision);
        status_ = selected_palette_collision_draft_dirty_
            ? "Collision changes are staged. Apply to commit one semantic operation."
            : "Collision properties match the committed palette tile.";
        return {
            selected_palette_collision_draft_dirty_
                ? ControllerCode::ready
                : ControllerCode::unchanged,
            selected_palette_collision_draft_dirty_
                ? authoring::tilemap::ResultCode::success
                : authoring::tilemap::ResultCode::unchanged,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready};
    }

    ControllerResult
        TileMapWorkspaceController::apply_selected_palette_collision() noexcept
    {
        ++metrics_.commands;
        const auto palette = selected_palette();
        if (!has_document() || !palette
            || !selected_palette_collision_draft_)
        {
            return reject(ControllerCode::palette_required);
        }
        if (!selected_palette_collision_draft_dirty_)
        {
            return {
                ControllerCode::unchanged,
                authoring::tilemap::ResultCode::unchanged,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready};
        }
        ControllerResult accepted = accept(document_->set_palette_collision(
            *palette, *selected_palette_collision_draft_));
        if (accepted)
        {
            const auto committed = document_->palette_entry(*palette);
            selected_palette_collision_draft_ = committed
                ? std::optional{committed->collision}
                : std::nullopt;
            selected_palette_collision_draft_dirty_ = false;
            status_ = "Committed the staged palette collision properties.";
        }
        return accepted;
    }

    ControllerResult
        TileMapWorkspaceController::set_selected_palette_collision(
            authoring::tilemap::CollisionShape collision) noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        const auto palette = selected_palette();
        if (!palette)
            return reject(ControllerCode::palette_required);
        ControllerResult accepted = accept(
            document_->set_palette_collision(*palette, collision));
        if (accepted)
        {
            selected_palette_collision_draft_ = std::move(collision);
            selected_palette_collision_draft_dirty_ = false;
            status_ = "Updated collision intent for the selected palette tile.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::undo() noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        ControllerResult accepted = accept(document_->undo());
        if (accepted)
        {
            selected_layer_draft_dirty_ = false;
            selected_palette_collision_draft_dirty_ = false;
            selected_object_draft_dirty_ = false;
            normalize_selection();
            status_ = "Undid the last semantic tile-map operation.";
        }
        return accepted;
    }

    ControllerResult TileMapWorkspaceController::redo() noexcept
    {
        ++metrics_.commands;
        if (!has_document())
            return reject(ControllerCode::invalid_controller);
        ControllerResult accepted = accept(document_->redo());
        if (accepted)
        {
            selected_layer_draft_dirty_ = false;
            selected_palette_collision_draft_dirty_ = false;
            selected_object_draft_dirty_ = false;
            normalize_selection();
            status_ = "Redid the next semantic tile-map operation.";
        }
        return accepted;
    }
    void TileMapWorkspaceController::record_rendered_cell_widgets(
        std::uint32_t count) noexcept
    {
        metrics_.rendered_cell_widgets += count;
    }


    PublishResult TileMapWorkspaceController::save_and_publish() noexcept
    {
        ++metrics_.commands;
        if (!has_document() || !valid())
        {
            return {reject(ControllerCode::invalid_controller)};
        }

        const auto sourceLocation = source_.save(
            logical_path_, *document_);
        if (!sourceLocation)
        {
            return {
                reject(
                    ControllerCode::source_failure,
                    authoring::tilemap::ResultCode::invalid_document,
                    sourceLocation.code),
                sourceLocation};
        }

        const auto compilation = document_->compile();
        if (!compilation)
        {
            return {
                reject(
                    ControllerCode::document_failure,
                    compilation.code,
                    sourceLocation.code),
                sourceLocation};
        }
        ++metrics_.compilations;
        const auto published = pipeline_.publish(
            logical_path_, compilation.artifact);
        if (!published)
        {
            return {
                reject(
                    ControllerCode::pipeline_failure,
                    authoring::tilemap::ResultCode::success,
                    sourceLocation.code,
                    published.code),
                sourceLocation,
                published};
        }

        saved_revision_ = document_->revision();
        ++metrics_.saves;
        status_ = sourceLocation.code
                == project_tilemap_sources::SourceCode::unchanged
            && published.code == project_tilemaps::PipelineCode::unchanged
            ? "Tile map source and compiled artifact are current."
            : "Saved temporal source and published the exact compiled tile map.";
        return {
            ControllerResult{
                sourceLocation.code
                        == project_tilemap_sources::SourceCode::unchanged
                    && published.code
                        == project_tilemaps::PipelineCode::unchanged
                    ? ControllerCode::unchanged
                    : ControllerCode::ready,
                authoring::tilemap::ResultCode::success,
                sourceLocation.code,
                published.code},
            sourceLocation,
            published};
    }

    RuntimePreviewResult
        TileMapWorkspaceController::compile_runtime_preview(
            std::uint64_t simulatedMilliseconds) noexcept
    {
        if (!has_document() || !valid())
            return {reject(ControllerCode::invalid_controller)};

        const auto revision = document_->revision();
        if (!preview_artifact_ || preview_revision_ != revision)
        {
            auto compilation = document_->compile(
                limits_.pipeline.library.artifact);
            if (!compilation)
            {
                preview_artifact_.reset();
                preview_revision_ = {};
                return {
                    reject(
                        ControllerCode::document_failure,
                        compilation.code)};
            }
            preview_artifact_ = std::move(compilation.artifact);
            preview_revision_ = revision;
            ++metrics_.preview_artifact_compilations;
        }

        const auto& artifact = *preview_artifact_;
        float minimumX = 0.0f;
        float minimumY = 0.0f;
        float maximumX = static_cast<float>(
            artifact.extent_tiles.x);
        float maximumY = static_cast<float>(
            artifact.extent_tiles.y);
        for (const auto& layer : artifact.layers)
        {
            minimumX = (std::min)(minimumX, layer.world_origin.x);
            minimumY = (std::min)(minimumY, layer.world_origin.y);
            maximumX = (std::max)(
                maximumX,
                layer.world_origin.x
                    + static_cast<float>(layer.extent_tiles.x)
                        * layer.tile_world_extent.x);
            maximumY = (std::max)(
                maximumY,
                layer.world_origin.y
                    + static_cast<float>(layer.extent_tiles.y)
                        * layer.tile_world_extent.y);
        }
        const canvas2d::tilemap_runtime::ViewRequest view{
            .world_bounds = {
                minimumX,
                minimumY,
                (std::max)(1.0f, maximumX - minimumX),
                (std::max)(1.0f, maximumY - minimumY)},
            .simulated_milliseconds = simulatedMilliseconds,
            .include_collision = true,
            .include_objects = true};
        auto visible = canvas2d::tilemap_runtime::compile_visible(
            artifact, view, limits_.pipeline.visible);
        if (!visible)
        {
            return {
                reject(ControllerCode::pipeline_failure),
                &artifact,
                std::move(visible)};
        }
        ++metrics_.preview_compilations;
        return {
            ControllerResult{
                ControllerCode::ready,
                authoring::tilemap::ResultCode::success,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready,
                visible.sprites.size()},
            &artifact,
            std::move(visible)};
    }

    void TileMapWorkspaceController::begin_canvas_pan(
        gui_lib::Vec2 pointer) noexcept
    {
        canvas_pan_active_ = true;
        canvas_pan_pointer_ = pointer;
    }

    void TileMapWorkspaceController::update_canvas_pan(
        gui_lib::Vec2 pointer) noexcept
    {
        if (!canvas_pan_active_)
            return;
        ui_state_.pan.x += pointer.x - canvas_pan_pointer_.x;
        ui_state_.pan.y += pointer.y - canvas_pan_pointer_.y;
        canvas_pan_pointer_ = pointer;
    }

    void TileMapWorkspaceController::end_canvas_pan() noexcept
    {
        canvas_pan_active_ = false;
    }

    bool TileMapWorkspaceController::canvas_pan_active() const noexcept
    {
        return canvas_pan_active_;
    }

    ControllerResult TileMapWorkspaceController::accept(
        authoring::tilemap::MutationResult mutation) noexcept
    {
        if (!mutation)
        {
            return reject(
                ControllerCode::document_failure, mutation.code);
        }
        if (mutation.code == authoring::tilemap::ResultCode::success)
        {
            ++metrics_.mutations;
            metrics_.painted_cells += mutation.affected_cells;
        }
        status_ = mutation.code == authoring::tilemap::ResultCode::unchanged
            ? "The temporal operation did not change the document."
            : "Committed a semantic tile-map operation.";
        return {
            mutation.code == authoring::tilemap::ResultCode::unchanged
                ? ControllerCode::unchanged
                : ControllerCode::ready,
            mutation.code,
            project_tilemap_sources::SourceCode::ready,
            project_tilemaps::PipelineCode::ready,
            mutation.affected_cells};
    }

    ControllerResult TileMapWorkspaceController::reject(
        ControllerCode code,
        authoring::tilemap::ResultCode documentCode,
        project_tilemap_sources::SourceCode sourceCode,
        project_tilemaps::PipelineCode pipelineCode) noexcept
    {
        ++metrics_.rejected_commands;
        status_ = "Tile-map command rejected: ";
        status_ += controller_code_name(code);
        if (code == ControllerCode::document_failure)
        {
            status_ += " / ";
            status_ += authoring::tilemap::result_code_name(documentCode);
        }
        else if (code == ControllerCode::source_failure)
        {
            status_ += " / ";
            status_ += project_tilemap_sources::source_code_name(sourceCode);
        }
        else if (code == ControllerCode::pipeline_failure)
        {
            status_ += " / ";
            status_ += project_tilemaps::pipeline_code_name(pipelineCode);
        }
        return {code, documentCode, sourceCode, pipelineCode};
    }

    void TileMapWorkspaceController::normalize_selection() noexcept
    {
        const auto current = snapshot();
        gui_lib::tile_workspace::controller().normalize_state(
            ui_state_,
            static_cast<std::uint32_t>(current.palette.size()),
            static_cast<std::uint32_t>(current.layers.size()));
        if (current.layers.empty())
        {
            selected_layer_handle_.reset();
            selected_layer_draft_.reset();
            selected_layer_draft_dirty_ = false;
        }
        else
        {
            auto selected = current.layers.end();
            if (selected_layer_handle_)
            {
                selected = std::find_if(
                    current.layers.begin(),
                    current.layers.end(),
                    [&](const auto& candidate)
                    {
                        return candidate.handle == *selected_layer_handle_;
                    });
            }
            if (selected == current.layers.end())
            {
                const std::uint32_t resolvedIndex =
                    ui_state_.selected_layer < current.layers.size()
                    ? ui_state_.selected_layer
                    : 0u;
                selected = current.layers.begin()
                    + static_cast<std::ptrdiff_t>(resolvedIndex);
                selected_layer_handle_ = selected->handle;
                selected_layer_draft_dirty_ = false;
            }
            ui_state_.selected_layer = static_cast<std::uint32_t>(
                selected - current.layers.begin());
            if (!selected_layer_draft_dirty_)
                selected_layer_draft_ = selected->descriptor;
        }
        if (current.palette.empty())
        {
            selected_palette_handle_.reset();
            selected_palette_collision_draft_.reset();
            selected_palette_collision_draft_dirty_ = false;
        }
        else
        {
            auto selected = current.palette.end();
            if (selected_palette_handle_)
            {
                selected = std::find_if(
                    current.palette.begin(),
                    current.palette.end(),
                    [&](const auto& candidate)
                    {
                        return candidate.handle == *selected_palette_handle_;
                    });
            }
            if (selected == current.palette.end())
            {
                const std::uint32_t resolvedIndex =
                    ui_state_.selected_palette < current.palette.size()
                    ? ui_state_.selected_palette
                    : 0u;
                selected = current.palette.begin()
                    + static_cast<std::ptrdiff_t>(resolvedIndex);
                selected_palette_handle_ = selected->handle;
                selected_palette_collision_draft_dirty_ = false;
            }
            ui_state_.selected_palette = static_cast<std::uint32_t>(
                selected - current.palette.begin());
            if (!selected_palette_collision_draft_dirty_)
            {
                selected_palette_collision_draft_ =
                    selected->descriptor.collision;
            }
        }
        if (selected_cell_)
        {
            if (selected_cell_->x >= current.descriptor.extent_tiles.x
                || selected_cell_->y >= current.descriptor.extent_tiles.y)
            {
                selected_cell_.reset();
            }
        }
        if (selected_object_)
        {
            const auto descriptor = document_->object(*selected_object_);
            if (!descriptor)
            {
                clear_object_selection();
            }
            else if (!selected_object_draft_dirty_)
            {
                selected_object_draft_ = *descriptor;
            }
        }
    }

    std::optional<authoring::tilemap::LayerHandle>
        TileMapWorkspaceController::selected_layer() const noexcept
    {
        if (!has_document())
            return std::nullopt;
        if (!selected_layer_handle_
            || !document_->layer(*selected_layer_handle_))
        {
            return std::nullopt;
        }
        return selected_layer_handle_;
    }

    std::optional<authoring::tilemap::PaletteEntryHandle>
        TileMapWorkspaceController::selected_palette() const noexcept
    {
        if (!has_document())
            return std::nullopt;
        if (!selected_palette_handle_
            || !document_->palette_entry(*selected_palette_handle_))
            return std::nullopt;
        return selected_palette_handle_;
    }

    ControllerResult TileMapWorkspaceController::fill_cell(
        authoring::tilemap::LayerHandle layer,
        authoring::tilemap::UInt2 coordinate,
        authoring::tilemap::CellValue replacement) noexcept
    {
        const auto target = document_->cell(layer, coordinate);
        if (!target)
        {
            return reject(
                ControllerCode::document_failure,
                authoring::tilemap::ResultCode::invalid_handle);
        }
        if (*target == replacement)
        {
            return {
                ControllerCode::unchanged,
                authoring::tilemap::ResultCode::unchanged,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready};
        }

        const auto extent = document_->descriptor().extent_tiles;
        std::deque<authoring::tilemap::UInt2> pending{};
        std::unordered_set<std::uint64_t> visited{};
        std::vector<authoring::tilemap::CellEdit> edits{};
        try
        {
            pending.push_back(coordinate);
            visited.reserve((std::min)(
                static_cast<std::uint64_t>(limits_.maximum_fill_cells),
                static_cast<std::uint64_t>(extent.x) * extent.y));
            edits.reserve((std::min)(limits_.maximum_fill_cells, 4096u));
            while (!pending.empty())
            {
                const auto current = pending.front();
                pending.pop_front();
                if (current.x >= extent.x || current.y >= extent.y)
                    continue;
                if (!visited.insert(packed_coordinate(current)).second)
                    continue;
                const auto value = document_->cell(layer, current);
                if (!value || *value != *target)
                    continue;
                if (edits.size() >= limits_.maximum_fill_cells)
                    return reject(ControllerCode::operation_limit);
                edits.push_back({current, replacement});
                if (current.x > 0u)
                    pending.push_back({current.x - 1u, current.y});
                if (current.x + 1u < extent.x)
                    pending.push_back({current.x + 1u, current.y});
                if (current.y > 0u)
                    pending.push_back({current.x, current.y - 1u});
                if (current.y + 1u < extent.y)
                    pending.push_back({current.x, current.y + 1u});
            }
        }
        catch (...)
        {
            return reject(ControllerCode::allocation_failure);
        }
        if (edits.empty())
        {
            return {
                ControllerCode::unchanged,
                authoring::tilemap::ResultCode::unchanged,
                project_tilemap_sources::SourceCode::ready,
                project_tilemaps::PipelineCode::ready};
        }
        ControllerResult accepted = accept(
            document_->paint_cells(layer, edits));
        if (accepted)
            selected_cell_ = CellCoordinate{coordinate.x, coordinate.y};
        return accepted;
    }

    CanvasRenderResult render_canvas_workspace(
        TileMapWorkspaceController& controller,
        const CanvasRenderOptions& options) noexcept
    {
        CanvasRenderResult result{};
        if (!controller.has_document()
            || options.viewport.size.x < 220.0f
            || options.viewport.size.y < 180.0f)
        {
            return result;
        }

        try
        {
            const auto current = controller.snapshot();
            auto& state = controller.ui_state();
            const gui_lib::tile_workspace::WorkspaceLimits workspaceLimits{
                .minimum_zoom = 0.75f,
                .maximum_zoom = 8.0f,
                .minimum_canvas_width = 160.0f,
                .minimum_canvas_height = 120.0f,
                .minimum_sidebar_width = 128.0f,
                .maximum_sidebar_width = 300.0f,
                .minimum_inspector_width = 144.0f,
                .maximum_inspector_width = 300.0f};
            gui_lib::tile_workspace::controller().normalize_state(
                state,
                static_cast<std::uint32_t>(current.palette.size()),
                static_cast<std::uint32_t>(current.layers.size()),
                workspaceLimits);

            const auto layout = gui_lib::tile_workspace::controller().make_layout({
                .area = {
                    {options.viewport.position.x, options.viewport.position.y},
                    {options.viewport.size.x, options.viewport.size.y}},
                .toolbar_height = 36.0f,
                .status_height = 24.0f,
                .sidebar_width = 214.0f,
                .inspector_width = 206.0f,
                .gap = 5.0f,
                .padding = 5.0f,
                .palette_fraction = 0.62f,
                .show_inspector = options.show_inspector,
                .limits = workspaceLimits});
            if (!layout.valid)
                return result;

            result.visible = true;
            const gui::Vec2 pointer = gui::mouse_position();
            const gui_lib::Vec2 libraryPointer = gui_lib_point(pointer);

            gui::panel_rect(gui_point(layout.toolbar.position), gui_size(layout.toolbar));
            gui::set_cursor({layout.toolbar.position.x + 5.0f, layout.toolbar.position.y + 4.0f});
            const std::array toolButtons{
                gui::SegmentedButtonSpec{"Select", 58.0f, state.tool == Tool::select},
                gui::SegmentedButtonSpec{"Pencil", 60.0f, state.tool == Tool::pencil},
                gui::SegmentedButtonSpec{"Erase", 56.0f, state.tool == Tool::eraser},
                gui::SegmentedButtonSpec{"Fill", 48.0f, state.tool == Tool::fill},
                gui::SegmentedButtonSpec{"Object", 60.0f, state.tool == Tool::object},
                gui::SegmentedButtonSpec{"Collision", 76.0f, state.tool == Tool::collision}};
            if (const auto selected = gui::segmented_button_row(
                    toolButtons, 28.0f, 3.0f))
            {
                constexpr std::array tools{
                    Tool::select,
                    Tool::pencil,
                    Tool::eraser,
                    Tool::fill,
                    Tool::object,
                    Tool::collision};
                if (*selected < tools.size())
                {
                    result.changed = static_cast<bool>(
                        controller.select_tool(tools[*selected]));
                    result.input_captured = true;
                }
            }

            gui::panel_rect(gui_point(layout.palette.position), gui_size(layout.palette));
            gui::set_cursor({layout.palette.position.x + 6.0f, layout.palette.position.y + 4.0f});
            gui::label("Tile Palette");
            const gui_lib::Rect paletteViewport{
                {layout.palette.position.x + 4.0f,
                 layout.palette.position.y + 26.0f},
                {(std::max)(1.0f, layout.palette.size.x - 8.0f),
                 (std::max)(1.0f, layout.palette.size.y - 30.0f)}};
            if (point_in_rect(pointer, paletteViewport))
            {
                const int wheel = gui::consume_mouse_wheel_delta();
                if (wheel != 0)
                {
                    state.palette_scroll -= static_cast<float>(wheel) * 44.0f;
                    result.input_captured = true;
                }
            }
            const auto paletteGrid =
                gui_lib::tile_workspace::controller().make_palette_grid({
                    .viewport = paletteViewport,
                    .item_count = static_cast<std::uint32_t>(current.palette.size()),
                    .preferred_cell_size = 52.0f,
                    .minimum_cell_size = 32.0f,
                    .maximum_cell_size = 72.0f,
                    .gap = 4.0f,
                    .padding = 4.0f,
                    .scroll_y = state.palette_scroll,
                    .maximum_columns = 8u});
            state.palette_scroll = paletteGrid.scroll_y;
            if (paletteGrid.valid)
            {
                const std::uint32_t first = paletteGrid.first_visible_row
                    * paletteGrid.columns;
                const std::uint32_t last = (std::min)(
                    paletteGrid.item_count,
                    paletteGrid.past_last_visible_row * paletteGrid.columns);
                for (std::uint32_t index = first; index < last; ++index)
                {
                    const auto cell =
                        gui_lib::tile_workspace::controller().make_palette_cell(
                            paletteGrid,
                            index,
                            libraryPointer,
                            state.selected_palette == index);
                    if (!cell.visible)
                        continue;
                    gui::set_cursor(gui_point(cell.content.position));
                    if (gui::button_selected(
                            std::to_string(
                                current.palette[index].descriptor.local_tile),
                            gui_size(cell.content),
                            state.selected_palette == index))
                    {
                        result.changed = static_cast<bool>(
                            controller.select_palette(index));
                        result.input_captured = true;
                    }
                }
            }

            gui::panel_rect(gui_point(layout.layers.position), gui_size(layout.layers));
            gui::set_cursor({layout.layers.position.x + 6.0f, layout.layers.position.y + 4.0f});
            gui::label("Layers");
            const float layerWidth = (std::max)(
                32.0f, layout.layers.size.x - 12.0f);
            gui::set_cursor({
                layout.layers.position.x + 6.0f,
                layout.layers.position.y + 27.0f});
            const std::array layerActions{
                gui::InlineButtonSpec{
                    .label = "+ Layer",
                    .width = 58.0f},
                gui::InlineButtonSpec{
                    .label = "Duplicate",
                    .width = 76.0f,
                    .enabled = !current.layers.empty()},
                gui::InlineButtonSpec{
                    .label = "Delete",
                    .width = 56.0f,
                    .enabled = current.layers.size() > 1u}};
            if (const auto action = gui::inline_button_row(
                    layerActions, 24.0f, 4.0f))
            {
                ControllerResult changed{};
                if (*action == 0u)
                    changed = controller.create_layer();
                else if (*action == 1u)
                    changed = controller.duplicate_selected_layer();
                else
                    changed = controller.remove_selected_layer();
                result.changed = static_cast<bool>(changed) || result.changed;
                result.input_captured = true;
            }
            const std::uint32_t visibleLayers = static_cast<std::uint32_t>(
                (std::max)(0.0f, layout.layers.size.y - 57.0f) / 27.0f);
            const std::uint32_t firstLayer = (std::min)(
                static_cast<std::uint32_t>(current.layers.size()),
                static_cast<std::uint32_t>(state.layer_scroll));
            const std::uint32_t lastLayer = (std::min)(
                static_cast<std::uint32_t>(current.layers.size()),
                firstLayer + visibleLayers);
            for (std::uint32_t index = firstLayer; index < lastLayer; ++index)
            {
                gui::set_cursor({
                    layout.layers.position.x + 6.0f,
                    layout.layers.position.y + 54.0f
                        + static_cast<float>(index - firstLayer) * 27.0f});
                std::string layerLabel{};
                if (!current.layers[index].descriptor.visible)
                    layerLabel += "[Hidden] ";
                if (current.layers[index].descriptor.locked)
                    layerLabel += "[Locked] ";
                layerLabel += current.layers[index].descriptor.name;
                if (gui::button_selected(
                        layerLabel,
                        {layerWidth, 23.0f},
                        state.selected_layer == index))
                {
                    result.changed = static_cast<bool>(
                        controller.select_layer(index));
                    result.input_captured = true;
                }
            }

            gui::panel_rect(gui_point(layout.canvas.position), gui_size(layout.canvas));
            const auto canvas = gui_lib::tile_workspace::controller().make_canvas({
                .viewport = {
                    {layout.canvas.position.x + 4.0f,
                     layout.canvas.position.y + 4.0f},
                    {(std::max)(1.0f, layout.canvas.size.x - 8.0f),
                     (std::max)(1.0f, layout.canvas.size.y - 8.0f)}},
                .map_width = current.descriptor.extent_tiles.x,
                .map_height = current.descriptor.extent_tiles.y,
                .base_cell_pixels = 28.0f,
                .zoom = state.zoom,
                .pan = state.pan,
                .overscroll_cells = 1.0f,
                .limits = workspaceLimits});
            if (canvas.valid)
            {
                state.zoom = canvas.zoom;
                state.pan = canvas.pan;
                if (gui_lib::contains(canvas.viewport, libraryPointer))
                {
                    result.hovered_cell =
                        gui_lib::tile_workspace::controller().cell_at(
                            canvas, libraryPointer);
                    const int wheel = gui::consume_mouse_wheel_delta();
                    if (wheel != 0)
                    {
                        const auto view =
                            gui_lib::tile_workspace::controller().zoom_at(
                                canvas,
                                libraryPointer,
                                static_cast<float>(wheel),
                                workspaceLimits);
                        state.zoom = view.zoom;
                        state.pan = view.pan;
                        result.input_captured = true;
                    }
                    if (gui::was_mouse_right_pressed())
                    {
                        controller.begin_canvas_pan(libraryPointer);
                        result.input_captured = true;
                    }
                    if (controller.canvas_pan_active()
                        && gui::is_mouse_right_down())
                    {
                        controller.update_canvas_pan(libraryPointer);
                        result.input_captured = true;
                    }
                }
                if (controller.canvas_pan_active()
                    && gui::was_mouse_right_released())
                {
                    controller.end_canvas_pan();
                    result.input_captured = true;
                }

                std::optional<authoring::tilemap::MapObjectHandle>
                    pointerObject{};
                for (auto object = current.objects.rbegin();
                     object != current.objects.rend();
                     ++object)
                {
                    const auto* descriptor = std::addressof(object->descriptor);
                    if (controller.selected_object()
                            == std::optional{object->handle}
                        && controller.selected_object_draft())
                    {
                        descriptor = controller.selected_object_draft();
                    }
                    const auto bounds = clipped_rect(
                        object_canvas_rect(canvas, *descriptor),
                        canvas.viewport);
                    if (bounds.size.x > 0.0f && bounds.size.y > 0.0f
                        && point_in_rect(pointer, bounds))
                    {
                        pointerObject = object->handle;
                        break;
                    }
                }

                const authoring::tilemap::Float2 mapPointer{
                    (libraryPointer.x - canvas.origin.x) / canvas.cell_pixels,
                    (libraryPointer.y - canvas.origin.y) / canvas.cell_pixels};
                if (options.interactive && pointerObject
                    && gui::was_mouse_pressed())
                {
                    const auto started = controller.begin_object_drag(
                        *pointerObject, mapPointer);
                    result.changed = static_cast<bool>(started)
                        || result.changed;
                    result.input_captured = true;
                }
                if (controller.object_drag_active())
                {
                    if (gui::is_mouse_down())
                    {
                        const auto staged =
                            controller.update_object_drag(mapPointer);
                        result.changed =
                            staged.code == ControllerCode::ready
                            || result.changed;
                        result.input_captured = true;
                    }
                    else
                    {
                        const auto committed =
                            controller.end_object_drag(true);
                        result.changed =
                            committed.code == ControllerCode::ready
                            || result.changed;
                        result.input_captured = true;
                    }
                }

                std::uint32_t rendered{};
                for (std::uint32_t y = canvas.visible_cells.first.y;
                     y < canvas.visible_cells.past_last.y;
                     ++y)
                {
                    for (std::uint32_t x = canvas.visible_cells.first.x;
                         x < canvas.visible_cells.past_last.x;
                         ++x)
                    {
                        if (rendered
                            >= controller.limits().maximum_visible_cell_widgets)
                        {
                            break;
                        }
                        const CellCoordinate coordinate{x, y};
                        const auto bounds =
                            gui_lib::tile_workspace::controller().cell_rect(
                                canvas, coordinate, state.show_grid ? 1.0f : 0.0f);
                        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f)
                            continue;
                        bool occupied{};
                        std::string_view collisionMarker{};
                        if (state.selected_layer < current.layers.size())
                        {
                            const auto value = controller.document()->cell(
                                current.layers[state.selected_layer].handle,
                                {x, y});
                            occupied = value && value->occupied();
                            if (occupied && state.tool == Tool::collision)
                            {
                                const auto palette =
                                    controller.document()->palette_entry(
                                        value->palette);
                                if (palette)
                                {
                                    switch (palette->collision.kind)
                                    {
                                    case asset::tilemap::CollisionKind::none:
                                        break;
                                    case asset::tilemap::CollisionKind::full_cell:
                                        collisionMarker = "S";
                                        break;
                                    case asset::tilemap::CollisionKind::one_way_up:
                                        collisionMarker = "1W";
                                        break;
                                    case asset::tilemap::CollisionKind::slope_up_right:
                                        collisionMarker = "R+";
                                        break;
                                    case asset::tilemap::CollisionKind::slope_down_right:
                                        collisionMarker = "R-";
                                        break;
                                    case asset::tilemap::CollisionKind::custom_box:
                                        collisionMarker = "BOX";
                                        break;
                                    }
                                }
                            }
                        }
                        const bool selected = controller.selected_cell()
                            == std::optional<CellCoordinate>{coordinate};
                        gui::set_cursor(gui_point(bounds.position));
                        const bool objectOwnsPointer = pointerObject
                            && point_in_rect(pointer, bounds);
                        if (objectOwnsPointer)
                        {
                            gui::panel_rect(
                                gui_point(bounds.position),
                                gui_size(bounds));
                        }
                        else if (gui::button_selected(
                                     collisionMarker,
                                     gui_size(bounds),
                                     occupied || selected)
                                 && options.interactive)
                        {
                            const auto changed = controller.apply_cell(coordinate);
                            result.changed = result.changed
                                || static_cast<bool>(changed);
                            result.input_captured = true;
                        }
                        ++rendered;
                    }
                    if (rendered
                        >= controller.limits().maximum_visible_cell_widgets)
                    {
                        break;
                    }
                }
                result.rendered_cells = rendered;

                for (const auto& object : current.objects)
                {
                    const auto* descriptor = std::addressof(object.descriptor);
                    const bool selected = controller.selected_object()
                        == std::optional{object.handle};
                    if (selected && controller.selected_object_draft())
                        descriptor = controller.selected_object_draft();
                    const auto bounds = clipped_rect(
                        object_canvas_rect(canvas, *descriptor),
                        canvas.viewport);
                    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f)
                        continue;
                    gui::set_cursor(gui_point(bounds.position));
                    if (gui::button_selected(
                            descriptor->name,
                            gui_size(bounds),
                            selected)
                        && options.interactive)
                    {
                        const auto changed =
                            controller.select_object(object.handle);
                        result.changed = result.changed
                            || static_cast<bool>(changed);
                        result.input_captured = true;
                    }
                }
            }

            if (layout.inspector_visible)
            {
                gui::panel_rect(
                    gui_point(layout.inspector.position),
                    gui_size(layout.inspector));
                gui::set_cursor({
                    layout.inspector.position.x + 6.0f,
                    layout.inspector.position.y + 5.0f});
                gui::begin_scroll_area({
                    .id = "tilemap.inspector.scroll",
                    .size = {
                        (std::max)(80.0f, layout.inspector.size.x - 12.0f),
                        (std::max)(48.0f, layout.inspector.size.y - 10.0f)},
                    .content_height = 760.0f,
                    .draw_background = false,
                    .show_scrollbar = true});
                gui::label("Tile Inspector");
                gui::property_row(
                    "Tool",
                    gui_lib::tile_workspace::tool_name(state.tool),
                    64.0f);
                gui::property_row(
                    "Zoom",
                    std::to_string(static_cast<int>(state.zoom * 100.0f)) + "%",
                    64.0f);
                gui::property_row(
                    "Cell",
                    valid_coordinate(result.hovered_cell)
                        ? std::to_string(result.hovered_cell.x) + ", "
                            + std::to_string(result.hovered_cell.y)
                        : std::string{"-"},
                    64.0f);
                gui::property_row(
                    "History",
                    std::to_string(controller.document()->metrics().operation_count),
                    64.0f);
                gui::property_row(
                    "Objects",
                    std::to_string(current.objects.size()),
                    64.0f);

                if (state.tool == Tool::collision
                    && controller.selected_palette_collision_draft())
                {
                    authoring::tilemap::CollisionShape draft =
                        *controller.selected_palette_collision_draft();
                    bool draftChanged{};
                    const float inspectorWidth =
                        (std::max)(80.0f, layout.inspector.size.x - 12.0f);
                    gui::label(
                        controller.selected_palette_collision_draft_dirty()
                        ? "Palette Collision *"
                        : "Palette Collision");

                    const std::array primaryKinds{
                        gui::SegmentedButtonSpec{
                            "None", 48.0f,
                            draft.kind == asset::tilemap::CollisionKind::none},
                        gui::SegmentedButtonSpec{
                            "Solid", 52.0f,
                            draft.kind
                                == asset::tilemap::CollisionKind::full_cell},
                        gui::SegmentedButtonSpec{
                            "One Way", 68.0f,
                            draft.kind
                                == asset::tilemap::CollisionKind::one_way_up}};
                    if (const auto selected = gui::segmented_button_row(
                            primaryKinds, 25.0f, 3.0f))
                    {
                        constexpr std::array kinds{
                            asset::tilemap::CollisionKind::none,
                            asset::tilemap::CollisionKind::full_cell,
                            asset::tilemap::CollisionKind::one_way_up};
                        if (*selected < kinds.size())
                        {
                            draft.kind = kinds[*selected];
                            draftChanged = true;
                        }
                    }
                    const std::array shapedKinds{
                        gui::SegmentedButtonSpec{
                            "Rise Right", 76.0f,
                            draft.kind
                                == asset::tilemap::CollisionKind::slope_up_right},
                        gui::SegmentedButtonSpec{
                            "Fall Right", 76.0f,
                            draft.kind
                                == asset::tilemap::CollisionKind::slope_down_right},
                        gui::SegmentedButtonSpec{
                            "Box", 42.0f,
                            draft.kind
                                == asset::tilemap::CollisionKind::custom_box}};
                    if (const auto selected = gui::segmented_button_row(
                            shapedKinds, 25.0f, 3.0f))
                    {
                        constexpr std::array kinds{
                            asset::tilemap::CollisionKind::slope_up_right,
                            asset::tilemap::CollisionKind::slope_down_right,
                            asset::tilemap::CollisionKind::custom_box};
                        if (*selected < kinds.size())
                        {
                            draft.kind = kinds[*selected];
                            draftChanged = true;
                        }
                    }

                    const auto boundsX = gui::slider({
                        .id = "tilemap.collision.bounds_x",
                        .label = "Bounds X",
                        .minimum = 0.0f,
                        .maximum = (std::max)(
                            0.0f, 1.0f - draft.local_bounds.width),
                        .value = draft.local_bounds.x,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (boundsX.changed)
                    {
                        draft.local_bounds.x = boundsX.value;
                        draft.local_bounds.width = (std::min)(
                            draft.local_bounds.width,
                            1.0f - draft.local_bounds.x);
                        draftChanged = true;
                    }
                    const auto boundsY = gui::slider({
                        .id = "tilemap.collision.bounds_y",
                        .label = "Bounds Y",
                        .minimum = 0.0f,
                        .maximum = (std::max)(
                            0.0f, 1.0f - draft.local_bounds.height),
                        .value = draft.local_bounds.y,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (boundsY.changed)
                    {
                        draft.local_bounds.y = boundsY.value;
                        draft.local_bounds.height = (std::min)(
                            draft.local_bounds.height,
                            1.0f - draft.local_bounds.y);
                        draftChanged = true;
                    }
                    const auto boundsWidth = gui::slider({
                        .id = "tilemap.collision.bounds_width",
                        .label = "Bounds Width",
                        .minimum = 0.05f,
                        .maximum = (std::max)(
                            0.05f, 1.0f - draft.local_bounds.x),
                        .value = draft.local_bounds.width,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (boundsWidth.changed)
                    {
                        draft.local_bounds.width = boundsWidth.value;
                        draftChanged = true;
                    }
                    const auto boundsHeight = gui::slider({
                        .id = "tilemap.collision.bounds_height",
                        .label = "Bounds Height",
                        .minimum = 0.05f,
                        .maximum = (std::max)(
                            0.05f, 1.0f - draft.local_bounds.y),
                        .value = draft.local_bounds.height,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (boundsHeight.changed)
                    {
                        draft.local_bounds.height = boundsHeight.value;
                        draftChanged = true;
                    }
                    gui::property_row(
                        "Layer Bits",
                        std::to_string(draft.layer_bits),
                        76.0f);
                    gui::property_row(
                        "Mask Bits",
                        std::to_string(draft.mask_bits),
                        76.0f);
                    gui::property_row(
                        "Sensor",
                        draft.sensor ? "Unsupported" : "Off",
                        76.0f);

                    if (draftChanged)
                    {
                        result.changed = static_cast<bool>(
                            controller.stage_selected_palette_collision(
                                draft)) || result.changed;
                        result.input_captured = true;
                    }
                    const std::array collisionActions{
                        gui::InlineButtonSpec{
                            .label = "Apply",
                            .width = 52.0f,
                            .enabled = controller
                                .selected_palette_collision_draft_dirty()},
                        gui::InlineButtonSpec{
                            .label = "Full Bounds",
                            .width = 82.0f}};
                    if (const auto action = gui::inline_button_row(
                            collisionActions, 26.0f, 4.0f))
                    {
                        ControllerResult changed{};
                        if (*action == 0u)
                        {
                            changed =
                                controller.apply_selected_palette_collision();
                        }
                        else
                        {
                            draft.local_bounds = {0.0f, 0.0f, 1.0f, 1.0f};
                            changed =
                                controller.stage_selected_palette_collision(
                                    draft);
                        }
                        result.changed = static_cast<bool>(changed)
                            || result.changed;
                        result.input_captured = true;
                    }
                }
                else if (const auto* selectedDraft =
                             controller.selected_object_draft())
                {
                    authoring::tilemap::MapObjectDescriptor draft =
                        *selectedDraft;
                    bool draftChanged{};
                    gui::label(controller.selected_object_draft_dirty()
                        ? "Object Properties *"
                        : "Object Properties");
                    gui::label("Name");
                    draftChanged = gui::edit_box(
                        draft.name,
                        {(std::max)(80.0f, layout.inspector.size.x - 12.0f),
                         28.0f},
                        controller.document()->limits().maximum_name_bytes)
                        .changed || draftChanged;
                    gui::label("Type");
                    draftChanged = gui::edit_box(
                        draft.type,
                        {(std::max)(80.0f, layout.inspector.size.x - 12.0f),
                         28.0f},
                        controller.document()->limits().maximum_name_bytes)
                        .changed || draftChanged;

                    const float inspectorWidth =
                        (std::max)(80.0f, layout.inspector.size.x - 12.0f);
                    auto x = gui::slider({
                        .id = "tilemap.object.position_x",
                        .label = "X",
                        .minimum = 0.0f,
                        .maximum = static_cast<float>(
                            current.descriptor.extent_tiles.x),
                        .value = draft.position.x,
                        .step = 0.25f,
                        .size = {inspectorWidth, 28.0f}});
                    if (x.changed)
                    {
                        draft.position.x = x.value;
                        draftChanged = true;
                    }
                    auto y = gui::slider({
                        .id = "tilemap.object.position_y",
                        .label = "Y",
                        .minimum = 0.0f,
                        .maximum = static_cast<float>(
                            current.descriptor.extent_tiles.y),
                        .value = draft.position.y,
                        .step = 0.25f,
                        .size = {inspectorWidth, 28.0f}});
                    if (y.changed)
                    {
                        draft.position.y = y.value;
                        draftChanged = true;
                    }
                    auto width = gui::slider({
                        .id = "tilemap.object.width",
                        .label = "Width",
                        .minimum = 0.25f,
                        .maximum = (std::max)(
                            1.0f,
                            static_cast<float>(
                                current.descriptor.extent_tiles.x)),
                        .value = draft.size.x,
                        .step = 0.25f,
                        .size = {inspectorWidth, 28.0f}});
                    if (width.changed)
                    {
                        draft.size.x = width.value;
                        draftChanged = true;
                    }
                    auto height = gui::slider({
                        .id = "tilemap.object.height",
                        .label = "Height",
                        .minimum = 0.25f,
                        .maximum = (std::max)(
                            1.0f,
                            static_cast<float>(
                                current.descriptor.extent_tiles.y)),
                        .value = draft.size.y,
                        .step = 0.25f,
                        .size = {inspectorWidth, 28.0f}});
                    if (height.changed)
                    {
                        draft.size.y = height.value;
                        draftChanged = true;
                    }
                    constexpr float radiansToDegrees =
                        57.295779513082320876f;
                    constexpr float degreesToRadians =
                        0.01745329251994329577f;
                    auto rotation = gui::slider({
                        .id = "tilemap.object.rotation",
                        .label = "Rotation",
                        .minimum = -180.0f,
                        .maximum = 180.0f,
                        .value = draft.rotation_radians * radiansToDegrees,
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (rotation.changed)
                    {
                        draft.rotation_radians =
                            rotation.value * degreesToRadians;
                        draftChanged = true;
                    }
                    if (draftChanged)
                    {
                        result.changed = static_cast<bool>(
                            controller.stage_selected_object(
                                std::move(draft))) || result.changed;
                        result.input_captured = true;
                    }

                    const std::array objectActions{
                        gui::InlineButtonSpec{
                            .label = "Apply",
                            .width = 52.0f,
                            .enabled =
                                controller.selected_object_draft_dirty()},
                        gui::InlineButtonSpec{
                            .label = "Duplicate",
                            .width = 76.0f},
                        gui::InlineButtonSpec{
                            .label = "Delete",
                            .width = 56.0f}};
                    if (const auto action = gui::inline_button_row(
                            objectActions, 26.0f, 4.0f))
                    {
                        ControllerResult changed{};
                        if (*action == 0u)
                            changed = controller.apply_selected_object();
                        else if (*action == 1u)
                            changed = controller.duplicate_selected_object();
                        else
                            changed = controller.remove_selected_object();
                        result.changed = static_cast<bool>(changed)
                            || result.changed;
                        result.input_captured = true;
                    }
                }
                else if (const auto* selectedLayer =
                             controller.selected_layer_draft())
                {
                    authoring::tilemap::LayerDescriptor draft =
                        *selectedLayer;
                    bool draftChanged{};
                    const float inspectorWidth =
                        (std::max)(80.0f, layout.inspector.size.x - 12.0f);
                    gui::label(controller.selected_layer_draft_dirty()
                        ? "Layer Properties *"
                        : "Layer Properties");
                    gui::label("Name");
                    draftChanged = gui::edit_box(
                        draft.name,
                        {inspectorWidth, 28.0f},
                        controller.document()->limits().maximum_name_bytes)
                        .changed || draftChanged;

                    draftChanged = gui::toggle_switch(
                        "Visible",
                        draft.visible,
                        {inspectorWidth, 27.0f}) || draftChanged;
                    draftChanged = gui::toggle_switch(
                        "Locked",
                        draft.locked,
                        {inspectorWidth, 27.0f}) || draftChanged;
                    draftChanged = gui::toggle_switch(
                        "Collision Source",
                        draft.collision_source,
                        {inspectorWidth, 27.0f}) || draftChanged;

                    const std::array phaseButtons{
                        gui::SegmentedButtonSpec{
                            "Bkg", 40.0f,
                            draft.phase == asset::tilemap::LayerPhase::background},
                        gui::SegmentedButtonSpec{
                            "World", 44.0f,
                            draft.phase == asset::tilemap::LayerPhase::world},
                        gui::SegmentedButtonSpec{
                            "Front", 44.0f,
                            draft.phase == asset::tilemap::LayerPhase::foreground},
                        gui::SegmentedButtonSpec{
                            "Overlay", 48.0f,
                            draft.phase == asset::tilemap::LayerPhase::overlay}};
                    if (const auto phase = gui::segmented_button_row(
                            phaseButtons, 25.0f, 3.0f))
                    {
                        constexpr std::array phases{
                            asset::tilemap::LayerPhase::background,
                            asset::tilemap::LayerPhase::world,
                            asset::tilemap::LayerPhase::foreground,
                            asset::tilemap::LayerPhase::overlay};
                        if (*phase < phases.size())
                        {
                            draft.phase = phases[*phase];
                            draftChanged = true;
                        }
                    }

                    const auto opacity = gui::slider({
                        .id = "tilemap.layer.opacity",
                        .label = "Opacity",
                        .minimum = 0.0f,
                        .maximum = 1.0f,
                        .value = draft.opacity,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (opacity.changed)
                    {
                        draft.opacity = opacity.value;
                        draftChanged = true;
                    }
                    const auto drawLayer = gui::slider({
                        .id = "tilemap.layer.draw_order",
                        .label = "Draw Order",
                        .minimum = -128.0f,
                        .maximum = 128.0f,
                        .value = static_cast<float>(draft.draw_layer),
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (drawLayer.changed)
                    {
                        draft.draw_layer = static_cast<std::int32_t>(
                            std::lround(drawLayer.value));
                        draftChanged = true;
                    }
                    const auto parallaxX = gui::slider({
                        .id = "tilemap.layer.parallax_x",
                        .label = "Parallax X",
                        .minimum = 0.0f,
                        .maximum = 2.0f,
                        .value = draft.parallax.x,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (parallaxX.changed)
                    {
                        draft.parallax.x = parallaxX.value;
                        draftChanged = true;
                    }
                    const auto parallaxY = gui::slider({
                        .id = "tilemap.layer.parallax_y",
                        .label = "Parallax Y",
                        .minimum = 0.0f,
                        .maximum = 2.0f,
                        .value = draft.parallax.y,
                        .step = 0.05f,
                        .size = {inspectorWidth, 28.0f}});
                    if (parallaxY.changed)
                    {
                        draft.parallax.y = parallaxY.value;
                        draftChanged = true;
                    }
                    if (draftChanged)
                    {
                        result.changed = static_cast<bool>(
                            controller.stage_selected_layer(std::move(draft)))
                            || result.changed;
                        result.input_captured = true;
                    }

                    const std::array actions{
                        gui::InlineButtonSpec{
                            .label = "Apply",
                            .width = 52.0f,
                            .enabled =
                                controller.selected_layer_draft_dirty()},
                        gui::InlineButtonSpec{
                            .label = "Duplicate",
                            .width = 76.0f},
                        gui::InlineButtonSpec{
                            .label = "Delete",
                            .width = 56.0f,
                            .enabled = current.layers.size() > 1u}};
                    if (const auto action = gui::inline_button_row(
                            actions, 26.0f, 4.0f))
                    {
                        ControllerResult changed{};
                        if (*action == 0u)
                            changed = controller.apply_selected_layer();
                        else if (*action == 1u)
                            changed = controller.duplicate_selected_layer();
                        else
                            changed = controller.remove_selected_layer();
                        result.changed = static_cast<bool>(changed)
                            || result.changed;
                        result.input_captured = true;
                    }
                }

                if (!current.objects.empty())
                {
                    gui::label("Map Objects");
                    const std::size_t visibleObjectCount = (std::min)(
                        current.objects.size(), std::size_t{6u});
                    for (std::size_t index = 0u;
                         index < visibleObjectCount;
                         ++index)
                    {
                        const auto& object = current.objects[index];
                        if (gui::button_selected(
                                object.descriptor.name,
                                {(std::max)(
                                     80.0f,
                                     layout.inspector.size.x - 12.0f),
                                 24.0f},
                                controller.selected_object()
                                    == std::optional{object.handle}))
                        {
                            result.changed = static_cast<bool>(
                                controller.select_object(object.handle))
                                || result.changed;
                            result.input_captured = true;
                        }
                    }
                }
                gui::end_scroll_area();
            }

            gui::panel_rect(gui_point(layout.status.position), gui_size(layout.status));
            gui::set_cursor({layout.status.position.x + 6.0f, layout.status.position.y + 3.0f});
            gui::label(map_status_line(controller, current));
            if (point_in_rect(pointer, layout.area)
                && (gui::is_mouse_down() || gui::is_mouse_right_down()))
            {
                result.input_captured = true;
            }
            controller.record_rendered_cell_widgets(result.rendered_cells);
            return result;
        }
        catch (...)
        {
            return result;
        }
    }

    ControllerContractFailure
        tilemap_workspace_controller_contract_failure() noexcept
    {
        ContractRoot root{};
        if (!root.valid())
            return ControllerContractFailure::temporary_root;

        constexpr std::string_view projectId{
            "epoch.editor-tilemap-workspace.contract"};
        TileMapWorkspaceController controller{
            std::string{projectId}, root.path,
            "Assets/Maps/contract.epochmap"};
        if (!controller.valid())
            return ControllerContractFailure::construction;
        if (!controller.open_or_create(authoring::tilemap::MapDescriptor{
                .name = "Contract",
                .extent_tiles = {8u, 8u},
                .default_chunk_extent = {4u, 4u}}))
        {
            return ControllerContractFailure::create;
        }
        if (controller.snapshot().layers.size() != 1u
            || !controller.create_layer("Details")
            || controller.snapshot().layers.size() != 2u)
        {
            return ControllerContractFailure::layer;
        }
        if (!controller.selected_layer_draft())
            return ControllerContractFailure::layer_edit;
        auto gameplayLayer = *controller.selected_layer_draft();
        gameplayLayer.name = "Gameplay";
        gameplayLayer.parallax = {0.75f, 0.85f};
        gameplayLayer.phase = asset::tilemap::LayerPhase::foreground;
        gameplayLayer.draw_layer = 4;
        gameplayLayer.opacity = 0.65f;
        gameplayLayer.visible = false;
        gameplayLayer.locked = true;
        gameplayLayer.collision_source = false;
        const auto operationsBeforeLayerEdit =
            controller.document()->metrics().operation_count;
        if (!controller.stage_selected_layer(gameplayLayer)
            || !controller.selected_layer_draft_dirty()
            || !controller.apply_selected_layer()
            || controller.selected_layer_draft_dirty()
            || controller.document()->metrics().operation_count
                != operationsBeforeLayerEdit + 1u
            || controller.snapshot().layers[1].descriptor != gameplayLayer)
        {
            return ControllerContractFailure::layer_edit;
        }
        if (!controller.duplicate_selected_layer()
            || controller.snapshot().layers.size() != 3u
            || controller.snapshot().layers[2].descriptor.name
                != "Gameplay Copy"
            || controller.snapshot().layers[2].descriptor.draw_layer != 5)
        {
            return ControllerContractFailure::layer_duplicate;
        }
        if (!controller.remove_selected_layer()
            || controller.snapshot().layers.size() != 2u
            || controller.ui_state().selected_layer != 1u
            || controller.snapshot().layers[1].descriptor != gameplayLayer)
        {
            return ControllerContractFailure::layer_remove;
        }

        editor_project_textures::TextureCatalogEntry texture{
            .logical_path = "Assets/Textures/tiles.rgba",
            .source_path = root.path / "Assets" / "Textures" / "tiles.ppm",
            .artifact_key = {{11u, 22u, 33u, 44u}},
            .logical = {91u, 7u},
            .source_sequence = 3u,
            .source_bytes = 203u,
            .decoded_bytes = 256u,
            .width = 8u,
            .height = 8u};
        const std::uint64_t projectKey = project_assets::stable_project_identity(
            projectId);
        if (controller.attach_texture(texture, projectKey + 1u, {2u, 2u}).code
            != ControllerCode::texture_project_mismatch)
        {
            return ControllerContractFailure::texture_project_guard;
        }
        if (!controller.attach_texture(texture, projectKey, {2u, 2u})
            || controller.snapshot().palette.size() != 16u)
        {
            return ControllerContractFailure::texture_attach;
        }
        if (!controller.select_layer(1u)
            || !controller.select_palette(0u)
            || !controller.select_tool(Tool::pencil))
        {
            return ControllerContractFailure::layer_lock;
        }
        const auto lockedPaint = controller.apply_cell({0u, 0u});
        if (lockedPaint.code != ControllerCode::document_failure
            || lockedPaint.document_code
                != authoring::tilemap::ResultCode::locked_layer)
        {
            return ControllerContractFailure::layer_lock;
        }
        gameplayLayer.locked = false;
        if (!controller.stage_selected_layer(gameplayLayer)
            || !controller.apply_selected_layer()
            || controller.snapshot().layers[1].descriptor != gameplayLayer)
        {
            return ControllerContractFailure::layer_lock;
        }
        if (!controller.select_layer(0u) || !controller.select_palette(0u)
            || !controller.select_tool(Tool::pencil))
        {
            return ControllerContractFailure::select;
        }
        if (!controller.apply_cell({1u, 1u}))
        {
            return ControllerContractFailure::pencil;
        }
        if (!controller.select_tool(Tool::eraser)
            || !controller.apply_cell({1u, 1u})
            || controller.document()->cell(
                controller.snapshot().layers[0].handle, {1u, 1u})->occupied())
        {
            return ControllerContractFailure::erase;
        }
        if (!controller.select_tool(Tool::fill)
            || !controller.apply_cell({0u, 0u})
            || controller.document()->metrics().occupied_cells != 64u)
        {
            return ControllerContractFailure::fill;
        }
        if (!controller.select_tool(Tool::collision)
            || !controller.apply_cell({0u, 0u}))
        {
            return ControllerContractFailure::collision;
        }
        const auto collision = controller.document()->palette_entry(
            controller.snapshot().palette[0].handle);
        if (!collision || collision->collision.kind
            != asset::tilemap::CollisionKind::full_cell)
        {
            return ControllerContractFailure::collision;
        }
        authoring::tilemap::CollisionShape authoredCollision =
            collision->collision;
        authoredCollision.kind =
            asset::tilemap::CollisionKind::slope_up_right;
        authoredCollision.local_bounds = {0.1f, 0.15f, 0.8f, 0.75f};
        authoredCollision.layer_bits = 4u;
        authoredCollision.mask_bits = 7u;
        const auto operationsBeforeCollision =
            controller.document()->metrics().operation_count;
        if (!controller.stage_selected_palette_collision(authoredCollision)
            || !controller.selected_palette_collision_draft_dirty()
            || controller.document()->palette_entry(
                    controller.snapshot().palette[0].handle)->collision
                == authoredCollision
            || !controller.apply_selected_palette_collision()
            || controller.selected_palette_collision_draft_dirty()
            || controller.document()->metrics().operation_count
                != operationsBeforeCollision + 1u)
        {
            return ControllerContractFailure::collision;
        }
        const auto committedCollision = controller.document()->palette_entry(
            controller.snapshot().palette[0].handle);
        if (!committedCollision
            || committedCollision->collision != authoredCollision)
        {
            return ControllerContractFailure::collision;
        }
        if (!controller.select_tool(Tool::object)
            || !controller.apply_cell({2u, 3u})
            || !controller.selected_object()
            || controller.snapshot().objects.size() != 1u)
        {
            return ControllerContractFailure::object_create;
        }
        auto object = *controller.selected_object_draft();
        object.name = "Spawn Trigger";
        object.type = "trigger";
        object.position = {3.25f, 4.5f};
        object.size = {2.0f, 1.5f};
        object.rotation_radians = 0.25f;
        if (!controller.stage_selected_object(object)
            || !controller.selected_object_draft_dirty()
            || !controller.apply_selected_object()
            || controller.selected_object_draft_dirty())
        {
            return ControllerContractFailure::object_edit;
        }
        const auto committedObject = controller.document()->object(
            *controller.selected_object());
        if (!committedObject || *committedObject != object)
            return ControllerContractFailure::object_edit;
        const auto operationCountBeforeDrag =
            controller.document()->metrics().operation_count;
        if (!controller.begin_object_drag(
                *controller.selected_object(), object.position)
            || !controller.object_drag_active()
            || !controller.update_object_drag({6.0f, 5.0f})
            || !controller.selected_object_draft_dirty())
        {
            return ControllerContractFailure::object_drag;
        }
        const auto objectBeforeDragCommit = controller.document()->object(
            *controller.selected_object());
        if (!objectBeforeDragCommit
            || objectBeforeDragCommit->position != object.position
            || !controller.end_object_drag(true)
            || controller.object_drag_active()
            || controller.selected_object_draft_dirty()
            || controller.document()->metrics().operation_count
                != operationCountBeforeDrag + 1u)
        {
            return ControllerContractFailure::object_drag;
        }
        const auto draggedObject = controller.document()->object(
            *controller.selected_object());
        if (!draggedObject
            || draggedObject->position
                != authoring::tilemap::Float2{6.0f, 5.0f})
        {
            return ControllerContractFailure::object_drag;
        }
        if (!controller.begin_object_drag(
                *controller.selected_object(), draggedObject->position)
            || !controller.update_object_drag({7.0f, 6.0f})
            || !controller.end_object_drag(false)
            || controller.document()->object(*controller.selected_object())
                != draggedObject)
        {
            return ControllerContractFailure::object_drag;
        }
        if (!controller.duplicate_selected_object()
            || controller.snapshot().objects.size() != 2u)
        {
            return ControllerContractFailure::object_duplicate;
        }
        if (!controller.remove_selected_object()
            || controller.selected_object()
            || controller.snapshot().objects.size() != 1u)
        {
            return ControllerContractFailure::object_remove;
        }
        if (!controller.undo() || !controller.redo())
            return ControllerContractFailure::undo_redo;
        const auto preview = controller.compile_runtime_preview(250u);
        if (!preview || preview.visible.sprites.size() != 64u
            || preview.visible.collision.size() != 64u
            || preview.visible.objects.size() != 1u
            || preview.visible.metrics.visible_layers != 1u
            || preview.visible.metrics.hidden_layers != 1u
            || !preview.artifact
            || preview.artifact->layers.size() != 2u
            || preview.artifact->layers[1].name != "Gameplay"
            || preview.artifact->layers[1].visible
            || preview.artifact->layers[1].opacity != 0.65f
            || preview.artifact->layers[1].draw_layer != 4
            || preview.artifact->collision.size() != 64u
            || preview.artifact->collision[0].kind
                != asset::tilemap::CollisionKind::slope_up_right
            || preview.artifact->collision[0].world_bounds
                != asset::tilemap::RectF{0.1f, 0.15f, 0.8f, 0.75f}
            || preview.artifact->collision[0].layer_bits != 4u
            || preview.artifact->collision[0].mask_bits != 7u
            || preview.visible.collision[0]
                != preview.artifact->collision[0]
            || preview.visible.objects[0].name != "Spawn Trigger"
            || preview.visible.objects[0].position.x != 6.0f
            || preview.visible.objects[0].position.y != 5.0f)
        {
            return ControllerContractFailure::runtime_preview;
        }

        const auto handoffBytes = controller.document()->serialize();
        TileMapWorkspaceController handedOff{
            std::string{projectId}, root.path,
            "Assets/Maps/handoff.epochmap"};
        if (!handoffBytes || !handedOff.valid()
            || !handedOff.restore_context_handoff(
                handoffBytes.bytes, true)
            || !handedOff.dirty()
            || handedOff.document()->metrics().occupied_cells != 64u
            || handedOff.snapshot().layers
                != controller.snapshot().layers
            || handedOff.snapshot().palette
                != controller.snapshot().palette
            || handedOff.snapshot().objects.size() != 1u
            || handedOff.snapshot().objects[0].descriptor
                != controller.snapshot().objects[0].descriptor)
        {
            return ControllerContractFailure::context_handoff;
        }

        const auto savedLayers = controller.snapshot().layers;
        const auto savedPalette = controller.snapshot().palette;
        const auto savedObject = controller.snapshot().objects[0];
        const PublishResult published = controller.save_and_publish();
        if (!published || controller.dirty())
            return ControllerContractFailure::source_publish;
        if (!controller.select_object(savedObject.handle))
            return ControllerContractFailure::reload;
        auto temporaryObject = *controller.selected_object_draft();
        temporaryObject.name = "Unsaved Temporary Trigger";
        if (!controller.stage_selected_object(std::move(temporaryObject))
            || !controller.apply_selected_object()
            || !controller.select_tool(Tool::eraser)
            || !controller.apply_cell({0u, 0u}) || !controller.dirty()
            || !controller.reload() || controller.dirty()
            || controller.document()->metrics().occupied_cells != 64u
            || controller.snapshot().layers != savedLayers
            || controller.snapshot().palette != savedPalette
            || controller.snapshot().objects.size() != 1u
            || controller.snapshot().objects[0] != savedObject)
        {
            return ControllerContractFailure::reload;
        }
        project_tilemaps::ProjectTileMapPipeline restored{
            std::string{projectId}, root.path.generic_string()};
        const auto restoredArtifact = restored.restore_exact_artifact(
            controller.logical_path(),
            published.compiled.locator.artifact_key);
        if (!restored.valid() || !restoredArtifact
            || restoredArtifact.artifact.layers.size() != 2u
            || restoredArtifact.artifact.layers[1].name != "Gameplay"
            || restoredArtifact.artifact.layers[1].visible
            || restoredArtifact.artifact.layers[1].opacity != 0.65f
            || restoredArtifact.artifact.layers[1].draw_layer != 4
            || restoredArtifact.artifact.collision.size() != 64u
            || restoredArtifact.artifact.collision[0].kind
                != asset::tilemap::CollisionKind::slope_up_right
            || restoredArtifact.artifact.collision[0].world_bounds
                != asset::tilemap::RectF{0.1f, 0.15f, 0.8f, 0.75f}
            || restoredArtifact.artifact.collision[0].layer_bits != 4u
            || restoredArtifact.artifact.collision[0].mask_bits != 7u
            || restoredArtifact.artifact.objects.size() != 1u
            || restoredArtifact.artifact.objects[0].name != "Spawn Trigger"
            || restoredArtifact.artifact.objects[0].position.x != 6.0f
            || restoredArtifact.artifact.objects[0].position.y != 5.0f)
        {
            return ControllerContractFailure::compiled_restore;
        }

        const auto metrics = controller.metrics();
        if (metrics.commands < 27u || metrics.mutations < 24u
            || metrics.saves != 1u || metrics.compilations != 1u
            || metrics.preview_artifact_compilations != 1u
            || metrics.preview_compilations != 1u
            || metrics.rejected_commands != 2u
            || metrics.painted_cells < 66u)
        {
            return ControllerContractFailure::metrics;
        }
        return ControllerContractFailure::none;
    }
}
