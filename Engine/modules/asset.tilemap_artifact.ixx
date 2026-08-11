/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module asset.tilemap_artifact;

export namespace epochengine::asset::tilemap
{
    inline constexpr std::uint32_t artifact_schema_version = 1u;
    inline constexpr std::uint32_t invalid_compiled_index =
        (std::numeric_limits<std::uint32_t>::max)();

    struct ContentHash final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        friend constexpr bool operator==(
            const ContentHash& left,
            const ContentHash& right) noexcept
        {
            return left.words == right.words;
        }

        friend constexpr auto operator<=>(
            const ContentHash&,
            const ContentHash&) noexcept = default;
    };

    struct DocumentRevision final
    {
        ContentHash content{};
        std::uint64_t sequence{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !content.empty() && sequence != 0;
        }

        friend constexpr bool operator==(
            const DocumentRevision& left,
            const DocumentRevision& right) noexcept
        {
            return left.content.words == right.content.words
                && left.sequence == right.sequence;
        }

        friend constexpr auto operator<=>(
            const DocumentRevision&,
            const DocumentRevision&) noexcept = default;
    };

    struct UInt2 final
    {
        std::uint32_t x{};
        std::uint32_t y{};

        friend constexpr auto operator<=>(UInt2, UInt2) noexcept = default;
    };

    struct Float2 final
    {
        float x{};
        float y{};

        friend constexpr bool operator==(Float2, Float2) noexcept = default;
    };

    struct RectF final
    {
        float x{};
        float y{};
        float width{};
        float height{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width <= 0.0f || height <= 0.0f;
        }

        friend constexpr bool operator==(RectF, RectF) noexcept = default;
    };

    struct Rgba8 final
    {
        std::uint8_t r{255u};
        std::uint8_t g{255u};
        std::uint8_t b{255u};
        std::uint8_t a{255u};

        friend constexpr auto operator<=>(Rgba8, Rgba8) noexcept = default;
    };

    template<typename Tag>
    struct StableId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        friend constexpr auto operator<=>(StableId, StableId) noexcept = default;
    };

    struct TileSetTag final {};
    struct PaletteEntryTag final {};
    struct LayerTag final {};
    struct MapObjectTag final {};

    using TileSetId = StableId<TileSetTag>;
    using PaletteEntryId = StableId<PaletteEntryTag>;
    using LayerId = StableId<LayerTag>;
    using MapObjectId = StableId<MapObjectTag>;

    enum class FilterMode : std::uint8_t
    {
        nearest,
        linear
    };

    enum class AddressMode : std::uint8_t
    {
        clamp_to_edge,
        repeat,
        mirrored_repeat
    };

    enum class AlphaMode : std::uint8_t
    {
        opaque,
        mask,
        straight,
        premultiplied,
        additive
    };

    enum class ColorSpace : std::uint8_t
    {
        linear,
        srgb
    };

    enum class LayerPhase : std::uint8_t
    {
        background,
        world,
        foreground,
        overlay
    };

    enum class TileTransform : std::uint8_t
    {
        identity,
        flip_x,
        flip_y,
        flip_xy,
        rotate_90,
        rotate_180,
        rotate_270
    };

    enum class CollisionKind : std::uint8_t
    {
        none,
        full_cell,
        one_way_up,
        slope_up_right,
        slope_down_right,
        custom_box
    };

    struct CollisionShape final
    {
        CollisionKind kind{CollisionKind::none};
        RectF local_bounds{0.0f, 0.0f, 1.0f, 1.0f};
        std::uint32_t layer_bits{1u};
        std::uint32_t mask_bits{0xffff'ffffu};
        bool sensor{};

        friend constexpr bool operator==(
            const CollisionShape&,
            const CollisionShape&) noexcept = default;
    };

    struct TextureDependency final
    {
        std::string logical_path{};
        std::array<std::uint64_t, 4> artifact_key{};
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        std::uint64_t artifact_revision{};
        std::uint32_t stable_material_key{};
        UInt2 texture_extent{};
        FilterMode min_filter{FilterMode::nearest};
        FilterMode mag_filter{FilterMode::nearest};
        AddressMode address_u{AddressMode::clamp_to_edge};
        AddressMode address_v{AddressMode::clamp_to_edge};
        AlphaMode alpha{AlphaMode::premultiplied};
        ColorSpace color_space{ColorSpace::srgb};
        float alpha_cutoff{0.5f};

        friend bool operator==(
            const TextureDependency&,
            const TextureDependency&) noexcept = default;
    };

    struct CompiledTileSet final
    {
        TileSetId id{};
        std::uint32_t dependency_index{invalid_compiled_index};
        UInt2 tile_extent{16u, 16u};
        UInt2 grid{1u, 1u};
        UInt2 margin{};
        UInt2 spacing{};
        std::uint32_t tile_count{1u};

        friend constexpr bool operator==(
            const CompiledTileSet&,
            const CompiledTileSet&) noexcept = default;
    };

    struct CompiledPaletteEntry final
    {
        PaletteEntryId id{};
        std::uint32_t tile_set_index{invalid_compiled_index};
        std::uint32_t local_tile{};
        CollisionShape collision{};
        std::uint16_t animation_frame_count{1u};
        std::uint16_t animation_rate_millihertz{};
        std::uint32_t user_flags{};

        friend constexpr bool operator==(
            const CompiledPaletteEntry&,
            const CompiledPaletteEntry&) noexcept = default;
    };

    struct CompiledLayer final
    {
        LayerId id{};
        std::string name{};
        UInt2 extent_tiles{64u, 64u};
        UInt2 chunk_extent{32u, 32u};
        Float2 world_origin{};
        Float2 tile_world_extent{1.0f, 1.0f};
        Float2 parallax{1.0f, 1.0f};
        LayerPhase phase{LayerPhase::world};
        std::int32_t draw_layer{};
        float opacity{1.0f};
        bool visible{true};
        bool collision_source{};

        friend bool operator==(
            const CompiledLayer&,
            const CompiledLayer&) noexcept = default;
    };

    struct CompiledCell final
    {
        UInt2 coordinate{};
        std::uint32_t palette_index{invalid_compiled_index};
        TileTransform transform{TileTransform::identity};
        Rgba8 tint{};
        std::uint16_t animation_frame{};
        std::uint16_t flags{};

        friend constexpr bool operator==(
            const CompiledCell&,
            const CompiledCell&) noexcept = default;
    };

    struct CompiledChunk final
    {
        std::uint32_t layer_index{invalid_compiled_index};
        UInt2 coordinate{};
        UInt2 extent{};
        std::uint64_t revision{1u};
        RectF world_bounds{};
        std::uint32_t first_cell{};
        std::uint32_t cell_count{};

        friend constexpr bool operator==(
            const CompiledChunk&,
            const CompiledChunk&) noexcept = default;
    };

    struct CollisionPrimitive final
    {
        LayerId layer{};
        PaletteEntryId palette_entry{};
        UInt2 cell_coordinate{};
        CollisionKind kind{CollisionKind::none};
        RectF world_bounds{};
        std::uint32_t layer_bits{1u};
        std::uint32_t mask_bits{0xffff'ffffu};
        bool sensor{};

        friend constexpr bool operator==(
            const CollisionPrimitive&,
            const CollisionPrimitive&) noexcept = default;
    };

    struct CompiledMapObject final
    {
        MapObjectId id{};
        std::string name{};
        std::string type{};
        Float2 position{};
        Float2 size{1.0f, 1.0f};
        float rotation_radians{};
        std::uint32_t layer_bits{1u};
        std::uint32_t user_flags{};

        friend bool operator==(
            const CompiledMapObject&,
            const CompiledMapObject&) noexcept = default;
    };

    struct ArtifactIdentity final
    {
        ContentHash key{};
        DocumentRevision source_revision{};
        std::uint32_t schema_version{artifact_schema_version};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !key.empty() && static_cast<bool>(source_revision)
                && schema_version == artifact_schema_version;
        }

        friend constexpr auto operator<=>(
            const ArtifactIdentity&,
            const ArtifactIdentity&) noexcept = default;
    };

    struct CompiledTileMapArtifact final
    {
        ArtifactIdentity identity{};
        std::string name{};
        UInt2 extent_tiles{};
        UInt2 default_chunk_extent{32u, 32u};
        std::vector<TextureDependency> dependencies{};
        std::vector<CompiledTileSet> tile_sets{};
        std::vector<CompiledPaletteEntry> palette{};
        std::vector<CompiledLayer> layers{};
        std::vector<CompiledChunk> chunks{};
        std::vector<CompiledCell> cells{};
        std::vector<CollisionPrimitive> collision{};
        std::vector<CompiledMapObject> objects{};
    };

    struct ArtifactLimits final
    {
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_path_bytes{1'024u};
        std::uint32_t maximum_texture_dependencies{4'096u};
        std::uint32_t maximum_tile_sets{4'096u};
        std::uint32_t maximum_palette_entries{1'048'576u};
        std::uint32_t maximum_layers{4'096u};
        std::uint32_t maximum_chunks{1'048'576u};
        std::uint64_t maximum_occupied_cells{268'435'456u};
        std::uint64_t maximum_collision_primitives{268'435'456u};
        std::uint32_t maximum_objects{1'048'576u};
        std::uint64_t maximum_serialized_bytes{8ull * 1024ull * 1024ull * 1024ull};
        std::uint32_t maximum_map_dimension_tiles{1'048'576u};
        std::uint32_t maximum_chunk_extent{512u};
        std::uint32_t maximum_tile_extent{4'096u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_name_bytes != 0 && maximum_path_bytes != 0
                && maximum_texture_dependencies != 0 && maximum_tile_sets != 0
                && maximum_palette_entries != 0 && maximum_layers != 0
                && maximum_chunks != 0 && maximum_occupied_cells != 0
                && maximum_collision_primitives != 0 && maximum_objects != 0
                && maximum_serialized_bytes != 0
                && maximum_map_dimension_tiles != 0
                && maximum_chunk_extent != 0 && maximum_tile_extent != 0;
        }
    };

    enum class ArtifactCode : std::uint8_t
    {
        ready,
        invalid_limits,
        invalid_identity,
        invalid_map,
        invalid_dependency,
        invalid_tile_set,
        invalid_palette_entry,
        invalid_layer,
        invalid_chunk,
        invalid_cell,
        invalid_collision,
        invalid_object,
        duplicate_identity,
        dangling_reference,
        noncanonical_order,
        content_hash_mismatch,
        capacity_exceeded,
        arithmetic_overflow,
        unsupported_schema,
        malformed_payload,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view artifact_code_name(
        ArtifactCode code) noexcept
    {
        switch (code)
        {
        case ArtifactCode::ready: return "ready";
        case ArtifactCode::invalid_limits: return "invalid_limits";
        case ArtifactCode::invalid_identity: return "invalid_identity";
        case ArtifactCode::invalid_map: return "invalid_map";
        case ArtifactCode::invalid_dependency: return "invalid_dependency";
        case ArtifactCode::invalid_tile_set: return "invalid_tile_set";
        case ArtifactCode::invalid_palette_entry: return "invalid_palette_entry";
        case ArtifactCode::invalid_layer: return "invalid_layer";
        case ArtifactCode::invalid_chunk: return "invalid_chunk";
        case ArtifactCode::invalid_cell: return "invalid_cell";
        case ArtifactCode::invalid_collision: return "invalid_collision";
        case ArtifactCode::invalid_object: return "invalid_object";
        case ArtifactCode::duplicate_identity: return "duplicate_identity";
        case ArtifactCode::dangling_reference: return "dangling_reference";
        case ArtifactCode::noncanonical_order: return "noncanonical_order";
        case ArtifactCode::content_hash_mismatch: return "content_hash_mismatch";
        case ArtifactCode::capacity_exceeded: return "capacity_exceeded";
        case ArtifactCode::arithmetic_overflow: return "arithmetic_overflow";
        case ArtifactCode::unsupported_schema: return "unsupported_schema";
        case ArtifactCode::malformed_payload: return "malformed_payload";
        case ArtifactCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct ArtifactValidation final
    {
        ArtifactCode code{ArtifactCode::invalid_map};
        std::uint64_t occupied_cells{};
        std::uint64_t serialized_bytes_estimate{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ArtifactCode::ready;
        }
    };

    struct SerializationResult final
    {
        ArtifactCode code{ArtifactCode::malformed_payload};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ArtifactCode::ready && !bytes.empty();
        }
    };

    struct DeserializationResult final
    {
        ArtifactCode code{ArtifactCode::malformed_payload};
        CompiledTileMapArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ArtifactCode::ready;
        }
    };

    [[nodiscard]] bool valid_texture_dependency_path(
        std::string_view path) noexcept;

    [[nodiscard]] ContentHash hash_bytes(
        std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] ContentHash compiled_tilemap_payload_content(
        const CompiledTileMapArtifact& artifact) noexcept;

    [[nodiscard]] ArtifactValidation validate(
        const CompiledTileMapArtifact& artifact,
        const ArtifactLimits& limits = {}) noexcept;

    [[nodiscard]] SerializationResult serialize(
        const CompiledTileMapArtifact& artifact,
        const ArtifactLimits& limits = {}) noexcept;

    [[nodiscard]] DeserializationResult deserialize(
        std::span<const std::byte> bytes,
        const ArtifactLimits& limits = {}) noexcept;

    enum class ArtifactContractFailure : std::uint8_t
    {
        none,
        valid_artifact_rejected,
        deterministic_hash,
        round_trip,
        malformed_payload_accepted,
        hash_mismatch_accepted,
        dangling_reference_accepted,
        noncanonical_chunk_order_accepted,
        invalid_collision_accepted,
        invalid_animation_span_accepted,
        bounded_read_failed
    };

    [[nodiscard]] constexpr std::string_view artifact_contract_failure_name(
        ArtifactContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ArtifactContractFailure::none: return "pass";
        case ArtifactContractFailure::valid_artifact_rejected:
            return "valid_artifact_rejected";
        case ArtifactContractFailure::deterministic_hash:
            return "deterministic_hash";
        case ArtifactContractFailure::round_trip: return "round_trip";
        case ArtifactContractFailure::malformed_payload_accepted:
            return "malformed_payload_accepted";
        case ArtifactContractFailure::hash_mismatch_accepted:
            return "hash_mismatch_accepted";
        case ArtifactContractFailure::dangling_reference_accepted:
            return "dangling_reference_accepted";
        case ArtifactContractFailure::noncanonical_chunk_order_accepted:
            return "noncanonical_chunk_order_accepted";
        case ArtifactContractFailure::invalid_collision_accepted:
            return "invalid_collision_accepted";
        case ArtifactContractFailure::invalid_animation_span_accepted:
            return "invalid_animation_span_accepted";
        case ArtifactContractFailure::bounded_read_failed:
            return "bounded_read_failed";
        }
        return "unknown";
    }

    [[nodiscard]] ArtifactContractFailure run_artifact_contract() noexcept;
}
