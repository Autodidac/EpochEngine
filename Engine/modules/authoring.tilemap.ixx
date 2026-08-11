/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module authoring.tilemap;

export import asset.tilemap_artifact;
export import authoring.document;

export namespace epochengine::authoring::tilemap
{
    struct TileSetTag final {};
    struct PaletteEntryTag final {};
    struct LayerTag final {};
    struct MapObjectTag final {};

    using TileSetHandle = authoring::Handle<TileSetTag, std::uint32_t>;
    using PaletteEntryHandle = authoring::Handle<PaletteEntryTag, std::uint32_t>;
    using LayerHandle = authoring::Handle<LayerTag, std::uint32_t>;
    using MapObjectHandle = authoring::Handle<MapObjectTag, std::uint32_t>;
    using DocumentHandle = authoring::DocumentHandle;
    using OperationId = authoring::OperationId;
    using DocumentRevision = asset::tilemap::DocumentRevision;
    using ContentHash = asset::tilemap::ContentHash;
    using UInt2 = asset::tilemap::UInt2;
    using Float2 = asset::tilemap::Float2;
    using Rgba8 = asset::tilemap::Rgba8;
    using TileTransform = asset::tilemap::TileTransform;
    using CollisionShape = asset::tilemap::CollisionShape;
    using LayerPhase = asset::tilemap::LayerPhase;
    using TextureDependency = asset::tilemap::TextureDependency;

    enum class ResultCode : std::uint8_t
    {
        success,
        unchanged,
        invalid_limits,
        invalid_document,
        invalid_descriptor,
        invalid_handle,
        stale_handle,
        duplicate_name,
        duplicate_texture_dependency,
        referenced_resource,
        dangling_reference,
        locked_layer,
        out_of_bounds,
        nothing_to_undo,
        nothing_to_redo,
        capacity_exceeded,
        history_limit_exceeded,
        arithmetic_overflow,
        malformed_payload,
        unsupported_schema,
        compile_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view result_code_name(
        ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::unchanged: return "unchanged";
        case ResultCode::invalid_limits: return "invalid_limits";
        case ResultCode::invalid_document: return "invalid_document";
        case ResultCode::invalid_descriptor: return "invalid_descriptor";
        case ResultCode::invalid_handle: return "invalid_handle";
        case ResultCode::stale_handle: return "stale_handle";
        case ResultCode::duplicate_name: return "duplicate_name";
        case ResultCode::duplicate_texture_dependency:
            return "duplicate_texture_dependency";
        case ResultCode::referenced_resource: return "referenced_resource";
        case ResultCode::dangling_reference: return "dangling_reference";
        case ResultCode::locked_layer: return "locked_layer";
        case ResultCode::out_of_bounds: return "out_of_bounds";
        case ResultCode::nothing_to_undo: return "nothing_to_undo";
        case ResultCode::nothing_to_redo: return "nothing_to_redo";
        case ResultCode::capacity_exceeded: return "capacity_exceeded";
        case ResultCode::history_limit_exceeded: return "history_limit_exceeded";
        case ResultCode::arithmetic_overflow: return "arithmetic_overflow";
        case ResultCode::malformed_payload: return "malformed_payload";
        case ResultCode::unsupported_schema: return "unsupported_schema";
        case ResultCode::compile_failure: return "compile_failure";
        case ResultCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct MapDescriptor final
    {
        std::string name{"TileMap"};
        UInt2 extent_tiles{64u, 64u};
        UInt2 default_chunk_extent{32u, 32u};
        std::uint32_t schema_version{1u};

        friend bool operator==(
            const MapDescriptor&,
            const MapDescriptor&) noexcept = default;
    };

    struct DocumentLimits final
    {
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_path_bytes{1'024u};
        std::uint32_t maximum_map_dimension_tiles{1'048'576u};
        std::uint32_t maximum_chunk_extent{512u};
        std::uint32_t maximum_tile_extent{4'096u};
        std::uint32_t maximum_tile_sets{4'096u};
        std::uint32_t maximum_palette_entries{1'048'576u};
        std::uint32_t maximum_layers{4'096u};
        std::uint32_t maximum_chunks{1'048'576u};
        std::uint64_t maximum_occupied_cells{268'435'456u};
        std::uint32_t maximum_objects{1'048'576u};
        std::uint32_t maximum_cell_edits_per_operation{1'048'576u};
        std::uint32_t maximum_operations{1'048'576u};
        std::uint64_t maximum_history_bytes{2ull * 1024ull * 1024ull * 1024ull};
        std::uint64_t maximum_snapshot_bytes{8ull * 1024ull * 1024ull * 1024ull};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_name_bytes != 0u && maximum_path_bytes != 0u
                && maximum_map_dimension_tiles != 0u
                && maximum_chunk_extent != 0u && maximum_tile_extent != 0u
                && maximum_tile_sets != 0u && maximum_palette_entries != 0u
                && maximum_layers != 0u && maximum_chunks != 0u
                && maximum_occupied_cells != 0u && maximum_objects != 0u
                && maximum_cell_edits_per_operation != 0u
                && maximum_operations != 0u && maximum_history_bytes != 0u
                && maximum_snapshot_bytes != 0u;
        }
    };

    struct TileSetDescriptor final
    {
        std::string name{"Tiles"};
        TextureDependency texture{};
        UInt2 tile_extent{16u, 16u};
        UInt2 grid{1u, 1u};
        UInt2 margin{};
        UInt2 spacing{};
        std::uint32_t tile_count{1u};

        friend bool operator==(
            const TileSetDescriptor&,
            const TileSetDescriptor&) noexcept = default;
    };

    struct PaletteEntryDescriptor final
    {
        std::string name{"Tile"};
        TileSetHandle tile_set{};
        std::uint32_t local_tile{};
        CollisionShape collision{};
        std::uint16_t animation_frame_count{1u};
        std::uint16_t animation_rate_millihertz{};
        std::uint32_t user_flags{};

        friend bool operator==(
            const PaletteEntryDescriptor&,
            const PaletteEntryDescriptor&) noexcept = default;
    };

    struct LayerDescriptor final
    {
        std::string name{"Layer"};
        UInt2 extent_tiles{64u, 64u};
        UInt2 chunk_extent{32u, 32u};
        Float2 world_origin{};
        Float2 tile_world_extent{1.0f, 1.0f};
        Float2 parallax{1.0f, 1.0f};
        LayerPhase phase{LayerPhase::world};
        std::int32_t draw_layer{};
        float opacity{1.0f};
        bool visible{true};
        bool locked{};
        bool collision_source{};

        friend bool operator==(
            const LayerDescriptor&,
            const LayerDescriptor&) noexcept = default;
    };

    struct MapObjectDescriptor final
    {
        std::string name{"Object"};
        std::string type{"marker"};
        Float2 position{};
        Float2 size{1.0f, 1.0f};
        float rotation_radians{};
        std::uint32_t layer_bits{1u};
        std::uint32_t user_flags{};

        friend bool operator==(
            const MapObjectDescriptor&,
            const MapObjectDescriptor&) noexcept = default;
    };

    struct CellValue final
    {
        PaletteEntryHandle palette{};
        TileTransform transform{TileTransform::identity};
        Rgba8 tint{};
        std::uint16_t animation_frame{};
        std::uint16_t flags{};

        [[nodiscard]] constexpr bool occupied() const noexcept
        {
            return palette.valid();
        }

        friend constexpr bool operator==(
            const CellValue&,
            const CellValue&) noexcept = default;
    };

    struct CellEdit final
    {
        UInt2 coordinate{};
        CellValue value{};

        friend constexpr bool operator==(
            const CellEdit&,
            const CellEdit&) noexcept = default;
    };

    struct ChunkSnapshot final
    {
        LayerHandle layer{};
        UInt2 coordinate{};
        UInt2 extent{};
        std::uint64_t revision{1u};
        std::vector<CellEdit> occupied_cells{};

        friend bool operator==(
            const ChunkSnapshot&,
            const ChunkSnapshot&) noexcept = default;
    };

    struct TileSetSnapshot final
    {
        TileSetHandle handle{};
        TileSetDescriptor descriptor{};

        friend bool operator==(
            const TileSetSnapshot&,
            const TileSetSnapshot&) noexcept = default;
    };

    struct PaletteEntrySnapshot final
    {
        PaletteEntryHandle handle{};
        PaletteEntryDescriptor descriptor{};

        friend bool operator==(
            const PaletteEntrySnapshot&,
            const PaletteEntrySnapshot&) noexcept = default;
    };

    struct LayerSnapshot final
    {
        LayerHandle handle{};
        LayerDescriptor descriptor{};

        friend bool operator==(
            const LayerSnapshot&,
            const LayerSnapshot&) noexcept = default;
    };

    struct MapObjectSnapshot final
    {
        MapObjectHandle handle{};
        MapObjectDescriptor descriptor{};

        friend bool operator==(
            const MapObjectSnapshot&,
            const MapObjectSnapshot&) noexcept = default;
    };

    struct DocumentSnapshot final
    {
        DocumentHandle handle{};
        MapDescriptor descriptor{};
        DocumentRevision revision{};
        std::vector<TileSetSnapshot> tile_sets{};
        std::vector<PaletteEntrySnapshot> palette{};
        std::vector<LayerSnapshot> layers{};
        std::vector<ChunkSnapshot> chunks{};
        std::vector<MapObjectSnapshot> objects{};
    };

    struct TileSetCreatedOperation final
    {
        TileSetHandle handle{};
        TileSetDescriptor descriptor{};
    };

    struct TileSetRemovedOperation final
    {
        TileSetHandle handle{};
        TileSetDescriptor descriptor{};
    };

    struct PaletteEntryCreatedOperation final
    {
        PaletteEntryHandle handle{};
        PaletteEntryDescriptor descriptor{};
    };

    struct PaletteEntryRemovedOperation final
    {
        PaletteEntryHandle handle{};
        PaletteEntryDescriptor descriptor{};
    };

    struct LayerCreatedOperation final
    {
        LayerHandle handle{};
        LayerDescriptor descriptor{};
    };

    struct LayerRemovedOperation final
    {
        LayerHandle handle{};
        LayerDescriptor descriptor{};
        std::vector<ChunkSnapshot> removed_chunks{};
    };

    struct LayerPropertiesChangedOperation final
    {
        LayerHandle handle{};
        LayerDescriptor before{};
        LayerDescriptor after{};
    };

    struct PaletteCollisionChangedOperation final
    {
        PaletteEntryHandle handle{};
        CollisionShape before{};
        CollisionShape after{};
    };

    struct CellDelta final
    {
        UInt2 coordinate{};
        CellValue before{};
        CellValue after{};

        friend constexpr bool operator==(
            const CellDelta&,
            const CellDelta&) noexcept = default;
    };

    struct CellsPaintedOperation final
    {
        LayerHandle layer{};
        std::vector<CellDelta> deltas{};
    };

    struct MapObjectCreatedOperation final
    {
        MapObjectHandle handle{};
        MapObjectDescriptor descriptor{};
    };

    struct MapObjectRemovedOperation final
    {
        MapObjectHandle handle{};
        MapObjectDescriptor descriptor{};
    };

    struct MapObjectChangedOperation final
    {
        MapObjectHandle handle{};
        MapObjectDescriptor before{};
        MapObjectDescriptor after{};
    };

    using OperationPayload = std::variant<
        TileSetCreatedOperation,
        TileSetRemovedOperation,
        PaletteEntryCreatedOperation,
        PaletteEntryRemovedOperation,
        LayerCreatedOperation,
        LayerRemovedOperation,
        LayerPropertiesChangedOperation,
        PaletteCollisionChangedOperation,
        CellsPaintedOperation,
        MapObjectCreatedOperation,
        MapObjectRemovedOperation,
        MapObjectChangedOperation>;

    struct OperationRecord final
    {
        OperationId id{};
        DocumentRevision before{};
        DocumentRevision after{};
        OperationPayload payload{};
        std::uint64_t retained_bytes{};
        bool currently_applied{};
    };

    struct MutationResult final
    {
        ResultCode code{ResultCode::invalid_document};
        DocumentRevision revision{};
        OperationId operation{};
        TileSetHandle tile_set{};
        PaletteEntryHandle palette_entry{};
        LayerHandle layer{};
        MapObjectHandle object{};
        std::uint64_t affected_cells{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success || code == ResultCode::unchanged;
        }
    };

    struct DocumentMetrics final
    {
        std::uint64_t active_tile_sets{};
        std::uint64_t active_palette_entries{};
        std::uint64_t active_layers{};
        std::uint64_t active_chunks{};
        std::uint64_t occupied_cells{};
        std::uint64_t collision_cells{};
        std::uint64_t active_objects{};
        std::uint64_t operation_count{};
        std::uint64_t applied_operation_count{};
        std::uint64_t retained_history_bytes{};
        std::uint64_t revision_sequence{};
    };

    struct CreateResult;
    struct RestoreResult;

    struct CompilationResult final
    {
        ResultCode code{ResultCode::compile_failure};
        asset::tilemap::CompiledTileMapArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct SnapshotSerializationResult final
    {
        ResultCode code{ResultCode::malformed_payload};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success && !bytes.empty();
        }
    };

    class Document final
    {
    public:
        Document(Document&&) noexcept;
        Document& operator=(Document&&) noexcept;
        ~Document();

        Document(const Document&) = delete;
        Document& operator=(const Document&) = delete;

        [[nodiscard]] static CreateResult create(
            DocumentHandle handle,
            MapDescriptor descriptor,
            DocumentLimits limits = {}) noexcept;

        [[nodiscard]] static RestoreResult restore(
            const DocumentSnapshot& snapshot,
            DocumentLimits limits = {}) noexcept;

        [[nodiscard]] static RestoreResult deserialize(
            std::span<const std::byte> bytes,
            DocumentLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] DocumentHandle handle() const noexcept;
        [[nodiscard]] const MapDescriptor& descriptor() const noexcept;
        [[nodiscard]] const DocumentRevision& revision() const noexcept;
        [[nodiscard]] const DocumentLimits& limits() const noexcept;
        [[nodiscard]] DocumentMetrics metrics() const noexcept;
        [[nodiscard]] bool can_undo() const noexcept;
        [[nodiscard]] bool can_redo() const noexcept;
        [[nodiscard]] std::vector<OperationRecord> history() const;

        [[nodiscard]] MutationResult create_tile_set(
            TileSetDescriptor descriptor) noexcept;
        [[nodiscard]] MutationResult remove_tile_set(
            TileSetHandle handle) noexcept;
        [[nodiscard]] MutationResult create_palette_entry(
            PaletteEntryDescriptor descriptor) noexcept;
        [[nodiscard]] MutationResult remove_palette_entry(
            PaletteEntryHandle handle) noexcept;
        [[nodiscard]] MutationResult create_layer(
            LayerDescriptor descriptor) noexcept;
        [[nodiscard]] MutationResult remove_layer(
            LayerHandle handle) noexcept;
        [[nodiscard]] MutationResult set_layer_properties(
            LayerHandle handle,
            LayerDescriptor descriptor) noexcept;
        [[nodiscard]] MutationResult set_palette_collision(
            PaletteEntryHandle handle,
            CollisionShape collision) noexcept;
        [[nodiscard]] MutationResult paint_cells(
            LayerHandle layer,
            std::span<const CellEdit> edits) noexcept;
        [[nodiscard]] MutationResult create_object(
            MapObjectDescriptor descriptor) noexcept;
        [[nodiscard]] MutationResult remove_object(
            MapObjectHandle handle) noexcept;
        [[nodiscard]] MutationResult set_object_properties(
            MapObjectHandle handle,
            MapObjectDescriptor descriptor) noexcept;
        [[nodiscard]] MutationResult undo() noexcept;
        [[nodiscard]] MutationResult redo() noexcept;

        [[nodiscard]] std::optional<TileSetDescriptor> tile_set(
            TileSetHandle handle) const;
        [[nodiscard]] std::optional<PaletteEntryDescriptor> palette_entry(
            PaletteEntryHandle handle) const;
        [[nodiscard]] std::optional<LayerDescriptor> layer(
            LayerHandle handle) const;
        [[nodiscard]] std::optional<CellValue> cell(
            LayerHandle layer,
            UInt2 coordinate) const;
        [[nodiscard]] std::optional<MapObjectDescriptor> object(
            MapObjectHandle handle) const;

        [[nodiscard]] DocumentSnapshot snapshot() const;
        [[nodiscard]] SnapshotSerializationResult serialize() const noexcept;
        [[nodiscard]] CompilationResult compile(
            const asset::tilemap::ArtifactLimits& limits = {}) const noexcept;

    private:
        struct Impl;
        explicit Document(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> impl_{};
    };

    struct CreateResult final
    {
        ResultCode code{ResultCode::invalid_document};
        std::unique_ptr<Document> document{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success && document != nullptr;
        }
    };

    struct RestoreResult final
    {
        ResultCode code{ResultCode::invalid_document};
        std::unique_ptr<Document> document{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success && document != nullptr;
        }
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        create_document,
        create_tile_set,
        create_palette,
        create_layer,
        paint_cells,
        erase_cells,
        collision_compile,
        deterministic_compile,
        source_round_trip,
        artifact_round_trip,
        undo_redo,
        stale_handle,
        locked_layer,
        out_of_bounds,
        referenced_resource,
        history_bound,
        malformed_source_accepted
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::create_document: return "create_document";
        case ContractFailure::create_tile_set: return "create_tile_set";
        case ContractFailure::create_palette: return "create_palette";
        case ContractFailure::create_layer: return "create_layer";
        case ContractFailure::paint_cells: return "paint_cells";
        case ContractFailure::erase_cells: return "erase_cells";
        case ContractFailure::collision_compile: return "collision_compile";
        case ContractFailure::deterministic_compile: return "deterministic_compile";
        case ContractFailure::source_round_trip: return "source_round_trip";
        case ContractFailure::artifact_round_trip: return "artifact_round_trip";
        case ContractFailure::undo_redo: return "undo_redo";
        case ContractFailure::stale_handle: return "stale_handle";
        case ContractFailure::locked_layer: return "locked_layer";
        case ContractFailure::out_of_bounds: return "out_of_bounds";
        case ContractFailure::referenced_resource: return "referenced_resource";
        case ContractFailure::history_bound: return "history_bound";
        case ContractFailure::malformed_source_accepted:
            return "malformed_source_accepted";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
