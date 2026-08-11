/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

module authoring.tilemap;

namespace epochengine::authoring::tilemap
{
    namespace detail
    {
        inline constexpr std::uint32_t source_schema_version = 1u;
        inline constexpr std::array<std::byte, 8> source_magic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'T'}, std::byte{'M'},
            std::byte{'A'}, std::byte{'P'}, std::byte{'D'}, std::byte{'1'}};

        [[nodiscard]] bool finite(Float2 value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        [[nodiscard]] bool finite(asset::tilemap::RectF value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y)
                && std::isfinite(value.width) && std::isfinite(value.height);
        }

        [[nodiscard]] constexpr bool valid_transform(TileTransform value) noexcept
        {
            return value >= TileTransform::identity
                && value <= TileTransform::rotate_270;
        }

        [[nodiscard]] constexpr bool valid_phase(LayerPhase value) noexcept
        {
            return value >= LayerPhase::background
                && value <= LayerPhase::overlay;
        }

        [[nodiscard]] bool valid_collision(
            const CollisionShape& collision) noexcept
        {
            if (collision.kind < asset::tilemap::CollisionKind::none
                || collision.kind > asset::tilemap::CollisionKind::custom_box
                || !finite(collision.local_bounds)
                || collision.layer_bits == 0u || collision.mask_bits == 0u)
            {
                return false;
            }
            if (collision.kind == asset::tilemap::CollisionKind::none)
                return true;
            return !collision.local_bounds.empty()
                && collision.local_bounds.x >= 0.0f
                && collision.local_bounds.y >= 0.0f
                && collision.local_bounds.x + collision.local_bounds.width <= 1.0f
                && collision.local_bounds.y + collision.local_bounds.height <= 1.0f;
        }

        [[nodiscard]] constexpr bool checked_multiply(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& output) noexcept
        {
            if (left != 0u
                && right > (std::numeric_limits<std::uint64_t>::max)() / left)
            {
                return false;
            }
            output = left * right;
            return true;
        }

        [[nodiscard]] constexpr bool checked_add(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& output) noexcept
        {
            if (right > (std::numeric_limits<std::uint64_t>::max)() - left)
                return false;
            output = left + right;
            return true;
        }

        template<typename Handle>
        [[nodiscard]] constexpr std::uint64_t stable_id(Handle handle) noexcept
        {
            if (!handle)
                return 0u;
            return (std::uint64_t{handle.generation} << 32u)
                | (std::uint64_t{handle.index} + 1u);
        }

        [[nodiscard]] bool valid_map_descriptor(
            const MapDescriptor& descriptor,
            const DocumentLimits& limits) noexcept
        {
            return limits.valid() && descriptor.schema_version == 1u
                && !descriptor.name.empty()
                && descriptor.name.size() <= limits.maximum_name_bytes
                && descriptor.extent_tiles.x != 0u
                && descriptor.extent_tiles.y != 0u
                && descriptor.extent_tiles.x <= limits.maximum_map_dimension_tiles
                && descriptor.extent_tiles.y <= limits.maximum_map_dimension_tiles
                && descriptor.default_chunk_extent.x != 0u
                && descriptor.default_chunk_extent.y != 0u
                && descriptor.default_chunk_extent.x <= limits.maximum_chunk_extent
                && descriptor.default_chunk_extent.y <= limits.maximum_chunk_extent;
        }

        [[nodiscard]] bool valid_texture_dependency(
            const TextureDependency& texture,
            const DocumentLimits& limits) noexcept
        {
            const bool artifactKey = std::any_of(
                texture.artifact_key.begin(), texture.artifact_key.end(),
                [](std::uint64_t word) { return word != 0u; });
            return asset::tilemap::valid_texture_dependency_path(
                    texture.logical_path)
                && texture.logical_path.size() <= limits.maximum_path_bytes
                && artifactKey && texture.project_key != 0u
                && texture.asset_key != 0u && texture.artifact_revision != 0u
                && texture.stable_material_key != 0u
                && texture.texture_extent.x != 0u
                && texture.texture_extent.y != 0u
                && texture.min_filter >= asset::tilemap::FilterMode::nearest
                && texture.min_filter <= asset::tilemap::FilterMode::linear
                && texture.mag_filter >= asset::tilemap::FilterMode::nearest
                && texture.mag_filter <= asset::tilemap::FilterMode::linear
                && texture.address_u >= asset::tilemap::AddressMode::clamp_to_edge
                && texture.address_u <= asset::tilemap::AddressMode::mirrored_repeat
                && texture.address_v >= asset::tilemap::AddressMode::clamp_to_edge
                && texture.address_v <= asset::tilemap::AddressMode::mirrored_repeat
                && texture.alpha >= asset::tilemap::AlphaMode::opaque
                && texture.alpha <= asset::tilemap::AlphaMode::additive
                && texture.color_space >= asset::tilemap::ColorSpace::linear
                && texture.color_space <= asset::tilemap::ColorSpace::srgb
                && std::isfinite(texture.alpha_cutoff)
                && texture.alpha_cutoff >= 0.0f
                && texture.alpha_cutoff <= 1.0f;
        }

        [[nodiscard]] bool valid_tile_set_descriptor(
            const TileSetDescriptor& descriptor,
            const DocumentLimits& limits) noexcept
        {
            if (descriptor.name.empty()
                || descriptor.name.size() > limits.maximum_name_bytes
                || !valid_texture_dependency(descriptor.texture, limits)
                || descriptor.tile_extent.x == 0u
                || descriptor.tile_extent.y == 0u
                || descriptor.tile_extent.x > limits.maximum_tile_extent
                || descriptor.tile_extent.y > limits.maximum_tile_extent
                || descriptor.grid.x == 0u || descriptor.grid.y == 0u
                || descriptor.tile_count == 0u)
            {
                return false;
            }
            std::uint64_t capacity{};
            std::uint64_t width{};
            std::uint64_t height{};
            std::uint64_t spacingX{};
            std::uint64_t spacingY{};
            return checked_multiply(
                    descriptor.grid.x, descriptor.grid.y, capacity)
                && descriptor.tile_count <= capacity
                && checked_multiply(
                    descriptor.grid.x, descriptor.tile_extent.x, width)
                && checked_multiply(
                    descriptor.grid.y, descriptor.tile_extent.y, height)
                && checked_multiply(
                    descriptor.grid.x - 1u, descriptor.spacing.x, spacingX)
                && checked_multiply(
                    descriptor.grid.y - 1u, descriptor.spacing.y, spacingY)
                && checked_add(width, spacingX, width)
                && checked_add(height, spacingY, height)
                && checked_add(
                    width, std::uint64_t{2u} * descriptor.margin.x, width)
                && checked_add(
                    height, std::uint64_t{2u} * descriptor.margin.y, height)
                && width <= descriptor.texture.texture_extent.x
                && height <= descriptor.texture.texture_extent.y;
        }

        [[nodiscard]] bool valid_layer_descriptor(
            const LayerDescriptor& descriptor,
            const MapDescriptor& map,
            const DocumentLimits& limits) noexcept
        {
            return !descriptor.name.empty()
                && descriptor.name.size() <= limits.maximum_name_bytes
                && descriptor.extent_tiles.x != 0u
                && descriptor.extent_tiles.y != 0u
                && descriptor.extent_tiles.x <= map.extent_tiles.x
                && descriptor.extent_tiles.y <= map.extent_tiles.y
                && descriptor.chunk_extent.x != 0u
                && descriptor.chunk_extent.y != 0u
                && descriptor.chunk_extent.x <= limits.maximum_chunk_extent
                && descriptor.chunk_extent.y <= limits.maximum_chunk_extent
                && finite(descriptor.world_origin)
                && finite(descriptor.tile_world_extent)
                && descriptor.tile_world_extent.x > 0.0f
                && descriptor.tile_world_extent.y > 0.0f
                && finite(descriptor.parallax) && valid_phase(descriptor.phase)
                && std::isfinite(descriptor.opacity)
                && descriptor.opacity >= 0.0f && descriptor.opacity <= 1.0f;
        }

        [[nodiscard]] bool valid_object_descriptor(
            const MapObjectDescriptor& descriptor,
            const DocumentLimits& limits) noexcept
        {
            return !descriptor.name.empty() && !descriptor.type.empty()
                && descriptor.name.size() <= limits.maximum_name_bytes
                && descriptor.type.size() <= limits.maximum_name_bytes
                && finite(descriptor.position) && finite(descriptor.size)
                && descriptor.size.x > 0.0f && descriptor.size.y > 0.0f
                && std::isfinite(descriptor.rotation_radians)
                && descriptor.layer_bits != 0u;
        }

        class Writer final
        {
        public:
            explicit Writer(std::uint64_t limit) noexcept : limit_(limit) {}

            [[nodiscard]] bool good() const noexcept { return good_; }
            [[nodiscard]] std::vector<std::byte> take() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::byte> value)
            {
                if (!reserve(value.size()))
                    return;
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }

            template<std::unsigned_integral Value>
            void integer(Value value)
            {
                if (!reserve(sizeof(Value)))
                    return;
                for (std::size_t byte = 0u; byte < sizeof(Value); ++byte)
                {
                    bytes_.push_back(static_cast<std::byte>(
                        (value >> (byte * 8u)) & static_cast<Value>(0xffu)));
                }
            }

            template<std::signed_integral Value>
            void integer(Value value)
            {
                integer(static_cast<std::make_unsigned_t<Value>>(value));
            }

            template<typename Enum>
                requires std::is_enum_v<Enum>
            void enumeration(Enum value)
            {
                integer(static_cast<std::underlying_type_t<Enum>>(value));
            }

            void boolean(bool value) { integer<std::uint8_t>(value ? 1u : 0u); }
            void floating(float value)
            {
                integer(std::bit_cast<std::uint32_t>(value));
            }
            void string(std::string_view value)
            {
                if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
                {
                    good_ = false;
                    return;
                }
                integer(static_cast<std::uint32_t>(value.size()));
                raw(std::as_bytes(std::span{value.data(), value.size()}));
            }
            void uint2(UInt2 value)
            {
                integer(value.x);
                integer(value.y);
            }
            void float2(Float2 value)
            {
                floating(value.x);
                floating(value.y);
            }
            void rect(asset::tilemap::RectF value)
            {
                floating(value.x);
                floating(value.y);
                floating(value.width);
                floating(value.height);
            }
            void hash(ContentHash value)
            {
                for (const std::uint64_t word : value.words)
                    integer(word);
            }
            template<typename Handle>
            void handle(Handle value)
            {
                integer(value.index);
                integer(value.generation);
            }

        private:
            [[nodiscard]] bool reserve(std::size_t additional)
            {
                if (!good_ || additional > limit_
                    || bytes_.size() > limit_ - additional)
                {
                    good_ = false;
                    return false;
                }
                bytes_.reserve(bytes_.size() + additional);
                return true;
            }

            std::vector<std::byte> bytes_{};
            std::uint64_t limit_{};
            bool good_{true};
        };

        class Reader final
        {
        public:
            explicit Reader(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool good() const noexcept { return good_; }
            [[nodiscard]] bool done() const noexcept
            {
                return good_ && offset_ == bytes_.size();
            }
            void fail() noexcept { good_ = false; }

            [[nodiscard]] std::span<const std::byte> raw(std::size_t count)
            {
                if (!good_ || count > bytes_.size() - offset_)
                {
                    good_ = false;
                    return {};
                }
                const auto result = bytes_.subspan(offset_, count);
                offset_ += count;
                return result;
            }

            template<std::unsigned_integral Value>
            [[nodiscard]] Value integer()
            {
                const auto valueBytes = raw(sizeof(Value));
                if (!good_)
                    return {};
                Value value{};
                for (std::size_t byte = 0u; byte < sizeof(Value); ++byte)
                {
                    value |= static_cast<Value>(
                        std::to_integer<std::uint8_t>(valueBytes[byte]))
                        << (byte * 8u);
                }
                return value;
            }

            template<std::signed_integral Value>
            [[nodiscard]] Value signed_integer()
            {
                return static_cast<Value>(integer<std::make_unsigned_t<Value>>());
            }

            template<typename Enum>
                requires std::is_enum_v<Enum>
            [[nodiscard]] Enum enumeration()
            {
                using Underlying = std::underlying_type_t<Enum>;
                if constexpr (std::is_signed_v<Underlying>)
                    return static_cast<Enum>(signed_integer<Underlying>());
                else
                    return static_cast<Enum>(integer<Underlying>());
            }

            [[nodiscard]] bool boolean()
            {
                const std::uint8_t value = integer<std::uint8_t>();
                if (value > 1u)
                    good_ = false;
                return value != 0u;
            }
            [[nodiscard]] float floating()
            {
                return std::bit_cast<float>(integer<std::uint32_t>());
            }
            [[nodiscard]] std::string string(std::uint32_t maximumBytes)
            {
                const std::uint32_t size = integer<std::uint32_t>();
                if (!good_ || size > maximumBytes)
                {
                    good_ = false;
                    return {};
                }
                const auto value = raw(size);
                if (!good_)
                    return {};
                return std::string{
                    reinterpret_cast<const char*>(value.data()), value.size()};
            }
            [[nodiscard]] UInt2 uint2()
            {
                return {integer<std::uint32_t>(), integer<std::uint32_t>()};
            }
            [[nodiscard]] Float2 float2()
            {
                return {floating(), floating()};
            }
            [[nodiscard]] asset::tilemap::RectF rect()
            {
                return {floating(), floating(), floating(), floating()};
            }
            [[nodiscard]] ContentHash hash()
            {
                ContentHash value{};
                for (std::uint64_t& word : value.words)
                    word = integer<std::uint64_t>();
                return value;
            }
            template<typename Handle>
            [[nodiscard]] Handle handle()
            {
                return {
                    integer<typename Handle::index_type>(),
                    integer<std::uint32_t>()};
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t offset_{};
            bool good_{true};
        };

        void write_texture(Writer& writer, const TextureDependency& value)
        {
            writer.string(value.logical_path);
            for (const std::uint64_t word : value.artifact_key)
                writer.integer(word);
            writer.integer(value.project_key);
            writer.integer(value.asset_key);
            writer.integer(value.artifact_revision);
            writer.integer(value.stable_material_key);
            writer.uint2(value.texture_extent);
            writer.enumeration(value.min_filter);
            writer.enumeration(value.mag_filter);
            writer.enumeration(value.address_u);
            writer.enumeration(value.address_v);
            writer.enumeration(value.alpha);
            writer.enumeration(value.color_space);
            writer.floating(value.alpha_cutoff);
        }

        [[nodiscard]] TextureDependency read_texture(
            Reader& reader,
            const DocumentLimits& limits)
        {
            TextureDependency value{};
            value.logical_path = reader.string(limits.maximum_path_bytes);
            for (std::uint64_t& word : value.artifact_key)
                word = reader.integer<std::uint64_t>();
            value.project_key = reader.integer<std::uint64_t>();
            value.asset_key = reader.integer<std::uint64_t>();
            value.artifact_revision = reader.integer<std::uint64_t>();
            value.stable_material_key = reader.integer<std::uint32_t>();
            value.texture_extent = reader.uint2();
            value.min_filter = reader.enumeration<asset::tilemap::FilterMode>();
            value.mag_filter = reader.enumeration<asset::tilemap::FilterMode>();
            value.address_u = reader.enumeration<asset::tilemap::AddressMode>();
            value.address_v = reader.enumeration<asset::tilemap::AddressMode>();
            value.alpha = reader.enumeration<asset::tilemap::AlphaMode>();
            value.color_space = reader.enumeration<asset::tilemap::ColorSpace>();
            value.alpha_cutoff = reader.floating();
            return value;
        }

        void write_collision(Writer& writer, const CollisionShape& value)
        {
            writer.enumeration(value.kind);
            writer.rect(value.local_bounds);
            writer.integer(value.layer_bits);
            writer.integer(value.mask_bits);
            writer.boolean(value.sensor);
        }

        [[nodiscard]] CollisionShape read_collision(Reader& reader)
        {
            CollisionShape value{};
            value.kind = reader.enumeration<asset::tilemap::CollisionKind>();
            value.local_bounds = reader.rect();
            value.layer_bits = reader.integer<std::uint32_t>();
            value.mask_bits = reader.integer<std::uint32_t>();
            value.sensor = reader.boolean();
            return value;
        }

        void write_cell(Writer& writer, const CellValue& value)
        {
            writer.handle(value.palette);
            writer.enumeration(value.transform);
            writer.integer(value.tint.r);
            writer.integer(value.tint.g);
            writer.integer(value.tint.b);
            writer.integer(value.tint.a);
            writer.integer(value.animation_frame);
            writer.integer(value.flags);
        }

        [[nodiscard]] CellValue read_cell(Reader& reader)
        {
            CellValue value{};
            value.palette = reader.handle<PaletteEntryHandle>();
            value.transform = reader.enumeration<TileTransform>();
            value.tint.r = reader.integer<std::uint8_t>();
            value.tint.g = reader.integer<std::uint8_t>();
            value.tint.b = reader.integer<std::uint8_t>();
            value.tint.a = reader.integer<std::uint8_t>();
            value.animation_frame = reader.integer<std::uint16_t>();
            value.flags = reader.integer<std::uint16_t>();
            return value;
        }

        [[nodiscard]] std::uint64_t operation_bytes(
            const OperationPayload& payload) noexcept
        {
            return std::visit([](const auto& operation) -> std::uint64_t
            {
                using Type = std::remove_cvref_t<decltype(operation)>;
                std::uint64_t result = sizeof(Type);
                if constexpr (std::same_as<Type, CellsPaintedOperation>)
                    result += operation.deltas.size() * sizeof(CellDelta);
                else if constexpr (std::same_as<Type, LayerRemovedOperation>)
                {
                    for (const ChunkSnapshot& chunk : operation.removed_chunks)
                    {
                        result += sizeof(ChunkSnapshot)
                            + chunk.occupied_cells.size() * sizeof(CellEdit);
                    }
                }
                return result;
            }, payload);
        }

        [[nodiscard]] asset::tilemap::UInt2 chunk_extent_at(
            const LayerDescriptor& layer,
            UInt2 coordinate) noexcept
        {
            const std::uint64_t originX =
                std::uint64_t{coordinate.x} * layer.chunk_extent.x;
            const std::uint64_t originY =
                std::uint64_t{coordinate.y} * layer.chunk_extent.y;
            return {
                static_cast<std::uint32_t>((std::min)(
                    std::uint64_t{layer.chunk_extent.x},
                    std::uint64_t{layer.extent_tiles.x} - originX)),
                static_cast<std::uint32_t>((std::min)(
                    std::uint64_t{layer.chunk_extent.y},
                    std::uint64_t{layer.extent_tiles.y} - originY))};
        }
    }

    struct Document::Impl final
    {
        template<typename Descriptor>
        struct Slot final
        {
            std::uint32_t generation{1u};
            std::optional<Descriptor> descriptor{};
        };

        struct Chunk final
        {
            UInt2 coordinate{};
            UInt2 extent{};
            std::uint64_t revision{1u};
            std::vector<CellValue> cells{};
            std::uint64_t occupied{};
        };

        struct LayerSlot final
        {
            std::uint32_t generation{1u};
            std::optional<LayerDescriptor> descriptor{};
            std::vector<Chunk> chunks{};
        };

        DocumentHandle handle{};
        MapDescriptor descriptor{};
        DocumentLimits limits{};
        DocumentRevision revision{};
        std::vector<Slot<TileSetDescriptor>> tile_sets{};
        std::vector<Slot<PaletteEntryDescriptor>> palette{};
        std::vector<LayerSlot> layers{};
        std::vector<Slot<MapObjectDescriptor>> objects{};
        std::vector<OperationRecord> operations{};
        std::size_t operation_cursor{};
        std::uint64_t retained_history_bytes{};
        std::uint64_t next_operation_id{1u};

        template<typename Handle, typename SlotType>
        [[nodiscard]] static SlotType* resolve(
            std::vector<SlotType>& slots,
            Handle handle) noexcept
        {
            if (!handle || handle.index >= slots.size())
                return nullptr;
            SlotType& slot = slots[handle.index];
            return slot.generation == handle.generation && slot.descriptor
                ? &slot : nullptr;
        }

        template<typename Handle, typename SlotType>
        [[nodiscard]] static const SlotType* resolve(
            const std::vector<SlotType>& slots,
            Handle handle) noexcept
        {
            if (!handle || handle.index >= slots.size())
                return nullptr;
            const SlotType& slot = slots[handle.index];
            return slot.generation == handle.generation && slot.descriptor
                ? &slot : nullptr;
        }

        template<typename Handle, typename SlotType>
        [[nodiscard]] static ResultCode handle_failure(
            const std::vector<SlotType>& slots,
            Handle handle) noexcept
        {
            if (!handle)
                return ResultCode::invalid_handle;
            return handle.index < slots.size()
                ? ResultCode::stale_handle
                : ResultCode::invalid_handle;
        }

        [[nodiscard]] Chunk* find_chunk(
            LayerSlot& layer,
            UInt2 coordinate) noexcept
        {
            const auto found = std::lower_bound(
                layer.chunks.begin(), layer.chunks.end(), coordinate,
                [](const Chunk& candidate, UInt2 key)
                {
                    return std::tuple{candidate.coordinate.y, candidate.coordinate.x}
                        < std::tuple{key.y, key.x};
                });
            return found != layer.chunks.end() && found->coordinate == coordinate
                ? &*found : nullptr;
        }

        [[nodiscard]] const Chunk* find_chunk(
            const LayerSlot& layer,
            UInt2 coordinate) const noexcept
        {
            const auto found = std::lower_bound(
                layer.chunks.begin(), layer.chunks.end(), coordinate,
                [](const Chunk& candidate, UInt2 key)
                {
                    return std::tuple{candidate.coordinate.y, candidate.coordinate.x}
                        < std::tuple{key.y, key.x};
                });
            return found != layer.chunks.end() && found->coordinate == coordinate
                ? &*found : nullptr;
        }

        [[nodiscard]] Chunk& ensure_chunk(
            LayerSlot& layer,
            UInt2 coordinate)
        {
            auto found = std::lower_bound(
                layer.chunks.begin(), layer.chunks.end(), coordinate,
                [](const Chunk& candidate, UInt2 key)
                {
                    return std::tuple{candidate.coordinate.y, candidate.coordinate.x}
                        < std::tuple{key.y, key.x};
                });
            if (found != layer.chunks.end() && found->coordinate == coordinate)
                return *found;
            const UInt2 extent = detail::chunk_extent_at(
                *layer.descriptor, coordinate);
            const std::uint64_t cellCount =
                static_cast<std::uint64_t>(extent.x) * extent.y;
            Chunk chunk{};
            chunk.coordinate = coordinate;
            chunk.extent = extent;
            chunk.cells.resize(static_cast<std::size_t>(cellCount));
            found = layer.chunks.insert(found, std::move(chunk));
            return *found;
        }

        [[nodiscard]] CellValue cell_value(
            const LayerSlot& layer,
            UInt2 coordinate) const noexcept
        {
            const LayerDescriptor& descriptorValue = *layer.descriptor;
            const UInt2 chunkCoordinate{
                coordinate.x / descriptorValue.chunk_extent.x,
                coordinate.y / descriptorValue.chunk_extent.y};
            const Chunk* const chunk = find_chunk(layer, chunkCoordinate);
            if (!chunk)
                return {};
            const UInt2 local{
                coordinate.x % descriptorValue.chunk_extent.x,
                coordinate.y % descriptorValue.chunk_extent.y};
            const std::size_t index = static_cast<std::size_t>(local.y)
                * chunk->extent.x + local.x;
            return index < chunk->cells.size() ? chunk->cells[index] : CellValue{};
        }

        void set_cell(
            LayerSlot& layer,
            UInt2 coordinate,
            CellValue value)
        {
            const LayerDescriptor& descriptorValue = *layer.descriptor;
            const UInt2 chunkCoordinate{
                coordinate.x / descriptorValue.chunk_extent.x,
                coordinate.y / descriptorValue.chunk_extent.y};
            Chunk* chunk = find_chunk(layer, chunkCoordinate);
            if (!chunk && !value.occupied())
                return;
            if (!chunk)
                chunk = &ensure_chunk(layer, chunkCoordinate);
            const UInt2 local{
                coordinate.x % descriptorValue.chunk_extent.x,
                coordinate.y % descriptorValue.chunk_extent.y};
            const std::size_t index = static_cast<std::size_t>(local.y)
                * chunk->extent.x + local.x;
            CellValue& target = chunk->cells[index];
            const bool before = target.occupied();
            const bool after = value.occupied();
            target = value;
            if (before != after)
            {
                if (after)
                    ++chunk->occupied;
                else
                    --chunk->occupied;
            }
            ++chunk->revision;
            if (chunk->revision == 0u)
                chunk->revision = 1u;
            if (chunk->occupied == 0u)
            {
                layer.chunks.erase(std::find_if(
                    layer.chunks.begin(), layer.chunks.end(),
                    [&](const Chunk& candidate)
                    {
                        return candidate.coordinate == chunkCoordinate;
                    }));
            }
        }

        [[nodiscard]] bool palette_referenced(PaletteEntryHandle handle) const noexcept
        {
            for (const LayerSlot& layer : layers)
            {
                if (!layer.descriptor)
                    continue;
                for (const Chunk& chunk : layer.chunks)
                {
                    if (std::any_of(
                            chunk.cells.begin(), chunk.cells.end(),
                            [&](const CellValue& value)
                            {
                                return value.palette == handle;
                            }))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        [[nodiscard]] bool tile_set_referenced(TileSetHandle handle) const noexcept
        {
            return std::any_of(
                palette.begin(), palette.end(),
                [&](const Slot<PaletteEntryDescriptor>& slot)
                {
                    return slot.descriptor
                        && slot.descriptor->tile_set == handle;
                });
        }

        [[nodiscard]] bool reserve_history(
            const OperationPayload& payload) const noexcept
        {
            const std::uint64_t bytes = detail::operation_bytes(payload);
            std::uint64_t retained = retained_history_bytes;
            for (std::size_t index = operation_cursor; index < operations.size(); ++index)
                retained -= operations[index].retained_bytes;
            return operation_cursor < limits.maximum_operations
                && bytes <= limits.maximum_history_bytes
                && retained <= limits.maximum_history_bytes - bytes;
        }

        void truncate_redo() noexcept
        {
            while (operations.size() > operation_cursor)
            {
                retained_history_bytes -= operations.back().retained_bytes;
                operations.pop_back();
            }
        }

        [[nodiscard]] std::vector<std::byte> canonical_state_bytes() const
        {
            detail::Writer writer{limits.maximum_snapshot_bytes};
            writer.string(descriptor.name);
            writer.uint2(descriptor.extent_tiles);
            writer.uint2(descriptor.default_chunk_extent);
            writer.integer(descriptor.schema_version);
            writer.handle(handle);

            std::uint32_t count{};
            for (const auto& slot : tile_sets)
                count += slot.descriptor ? 1u : 0u;
            writer.integer(count);
            for (std::uint32_t index = 0u; index < tile_sets.size(); ++index)
            {
                const auto& slot = tile_sets[index];
                if (!slot.descriptor)
                    continue;
                writer.handle(TileSetHandle{index, slot.generation});
                writer.string(slot.descriptor->name);
                detail::write_texture(writer, slot.descriptor->texture);
                writer.uint2(slot.descriptor->tile_extent);
                writer.uint2(slot.descriptor->grid);
                writer.uint2(slot.descriptor->margin);
                writer.uint2(slot.descriptor->spacing);
                writer.integer(slot.descriptor->tile_count);
            }

            count = 0u;
            for (const auto& slot : palette)
                count += slot.descriptor ? 1u : 0u;
            writer.integer(count);
            for (std::uint32_t index = 0u; index < palette.size(); ++index)
            {
                const auto& slot = palette[index];
                if (!slot.descriptor)
                    continue;
                writer.handle(PaletteEntryHandle{index, slot.generation});
                writer.string(slot.descriptor->name);
                writer.handle(slot.descriptor->tile_set);
                writer.integer(slot.descriptor->local_tile);
                detail::write_collision(writer, slot.descriptor->collision);
                writer.integer(slot.descriptor->animation_frame_count);
                writer.integer(slot.descriptor->animation_rate_millihertz);
                writer.integer(slot.descriptor->user_flags);
            }

            count = 0u;
            for (const auto& slot : layers)
                count += slot.descriptor ? 1u : 0u;
            writer.integer(count);
            for (std::uint32_t index = 0u; index < layers.size(); ++index)
            {
                const LayerSlot& slot = layers[index];
                if (!slot.descriptor)
                    continue;
                writer.handle(LayerHandle{index, slot.generation});
                const LayerDescriptor& layer = *slot.descriptor;
                writer.string(layer.name);
                writer.uint2(layer.extent_tiles);
                writer.uint2(layer.chunk_extent);
                writer.float2(layer.world_origin);
                writer.float2(layer.tile_world_extent);
                writer.float2(layer.parallax);
                writer.enumeration(layer.phase);
                writer.integer(layer.draw_layer);
                writer.floating(layer.opacity);
                writer.boolean(layer.visible);
                writer.boolean(layer.locked);
                writer.boolean(layer.collision_source);
                writer.integer(static_cast<std::uint32_t>(slot.chunks.size()));
                for (const Chunk& chunk : slot.chunks)
                {
                    writer.uint2(chunk.coordinate);
                    writer.uint2(chunk.extent);
                    writer.integer(chunk.revision);
                    writer.integer(chunk.occupied);
                    for (std::uint32_t y = 0u; y < chunk.extent.y; ++y)
                    {
                        for (std::uint32_t x = 0u; x < chunk.extent.x; ++x)
                        {
                            const CellValue& value = chunk.cells[
                                static_cast<std::size_t>(y) * chunk.extent.x + x];
                            if (!value.occupied())
                                continue;
                            writer.uint2({x, y});
                            detail::write_cell(writer, value);
                        }
                    }
                }
            }

            count = 0u;
            for (const auto& slot : objects)
                count += slot.descriptor ? 1u : 0u;
            writer.integer(count);
            for (std::uint32_t index = 0u; index < objects.size(); ++index)
            {
                const auto& slot = objects[index];
                if (!slot.descriptor)
                    continue;
                writer.handle(MapObjectHandle{index, slot.generation});
                writer.string(slot.descriptor->name);
                writer.string(slot.descriptor->type);
                writer.float2(slot.descriptor->position);
                writer.float2(slot.descriptor->size);
                writer.floating(slot.descriptor->rotation_radians);
                writer.integer(slot.descriptor->layer_bits);
                writer.integer(slot.descriptor->user_flags);
            }
            return writer.good() ? writer.take() : std::vector<std::byte>{};
        }

        [[nodiscard]] ContentHash state_content() const noexcept
        {
            try
            {
                const auto bytes = canonical_state_bytes();
                return bytes.empty() ? ContentHash{}
                    : asset::tilemap::hash_bytes(bytes);
            }
            catch (...)
            {
                return {};
            }
        }

        void advance_revision() noexcept
        {
            ++revision.sequence;
            if (revision.sequence == 0u)
                revision.sequence = 1u;
            revision.content = state_content();
        }

        [[nodiscard]] MutationResult commit(OperationPayload payload) noexcept
        {
            const std::uint64_t bytes = detail::operation_bytes(payload);
            truncate_redo();
            OperationRecord record{};
            record.id = {next_operation_id++};
            if (next_operation_id == 0u)
                next_operation_id = 1u;
            record.before = revision;
            record.payload = std::move(payload);
            record.retained_bytes = bytes;
            record.currently_applied = true;
            advance_revision();
            record.after = revision;
            operations.push_back(std::move(record));
            operation_cursor = operations.size();
            retained_history_bytes += bytes;
            return {
                .code = ResultCode::success,
                .revision = revision,
                .operation = operations.back().id};
        }

        void set_active_state(
            const TileSetCreatedOperation& operation,
            bool active)
        {
            auto& slot = tile_sets[operation.handle.index];
            slot.generation = operation.handle.generation;
            slot.descriptor = active
                ? std::optional{operation.descriptor} : std::nullopt;
        }
        void set_active_state(
            const TileSetRemovedOperation& operation,
            bool active)
        {
            auto& slot = tile_sets[operation.handle.index];
            slot.generation = operation.handle.generation;
            slot.descriptor = active
                ? std::optional{operation.descriptor} : std::nullopt;
        }
        void set_active_state(
            const PaletteEntryCreatedOperation& operation,
            bool active)
        {
            auto& slot = palette[operation.handle.index];
            slot.generation = operation.handle.generation;
            slot.descriptor = active
                ? std::optional{operation.descriptor} : std::nullopt;
        }
        void set_active_state(
            const PaletteEntryRemovedOperation& operation,
            bool active)
        {
            auto& slot = palette[operation.handle.index];
            slot.generation = operation.handle.generation;
            slot.descriptor = active
                ? std::optional{operation.descriptor} : std::nullopt;
        }
        void set_active_state(
            const MapObjectCreatedOperation& operation,
            bool active)
        {
            auto& slot = objects[operation.handle.index];
            slot.generation = operation.handle.generation;
            slot.descriptor = active
                ? std::optional{operation.descriptor} : std::nullopt;
        }
        void set_active_state(
            const MapObjectRemovedOperation& operation,
            bool active)
        {
            auto& slot = objects[operation.handle.index];
            slot.generation = operation.handle.generation;
            slot.descriptor = active
                ? std::optional{operation.descriptor} : std::nullopt;
        }

        void restore_layer_chunks(
            LayerSlot& slot,
            std::span<const ChunkSnapshot> snapshots)
        {
            slot.chunks.clear();
            for (const ChunkSnapshot& snapshot : snapshots)
            {
                Chunk chunk{};
                chunk.coordinate = snapshot.coordinate;
                chunk.extent = snapshot.extent;
                chunk.revision = snapshot.revision;
                chunk.cells.resize(
                    static_cast<std::size_t>(chunk.extent.x) * chunk.extent.y);
                for (const CellEdit& edit : snapshot.occupied_cells)
                {
                    const UInt2 local{
                        edit.coordinate.x % slot.descriptor->chunk_extent.x,
                        edit.coordinate.y % slot.descriptor->chunk_extent.y};
                    chunk.cells[static_cast<std::size_t>(local.y)
                        * chunk.extent.x + local.x] = edit.value;
                    ++chunk.occupied;
                }
                slot.chunks.push_back(std::move(chunk));
            }
        }

        void apply_payload(const OperationPayload& payload, bool forward)
        {
            std::visit([&](const auto& operation)
            {
                using Type = std::remove_cvref_t<decltype(operation)>;
                if constexpr (std::same_as<Type, TileSetCreatedOperation>)
                    set_active_state(operation, forward);
                else if constexpr (std::same_as<Type, TileSetRemovedOperation>)
                    set_active_state(operation, !forward);
                else if constexpr (std::same_as<Type, PaletteEntryCreatedOperation>)
                    set_active_state(operation, forward);
                else if constexpr (std::same_as<Type, PaletteEntryRemovedOperation>)
                    set_active_state(operation, !forward);
                else if constexpr (std::same_as<Type, LayerCreatedOperation>)
                {
                    LayerSlot& slot = layers[operation.handle.index];
                    slot.generation = operation.handle.generation;
                    slot.descriptor = forward
                        ? std::optional{operation.descriptor} : std::nullopt;
                    slot.chunks.clear();
                }
                else if constexpr (std::same_as<Type, LayerRemovedOperation>)
                {
                    LayerSlot& slot = layers[operation.handle.index];
                    slot.generation = operation.handle.generation;
                    slot.descriptor = !forward
                        ? std::optional{operation.descriptor} : std::nullopt;
                    if (forward)
                        slot.chunks.clear();
                    else
                        restore_layer_chunks(slot, operation.removed_chunks);
                }
                else if constexpr (std::same_as<Type,
                                       LayerPropertiesChangedOperation>)
                {
                    layers[operation.handle.index].descriptor =
                        forward ? operation.after : operation.before;
                }
                else if constexpr (std::same_as<Type,
                                       PaletteCollisionChangedOperation>)
                {
                    palette[operation.handle.index].descriptor->collision =
                        forward ? operation.after : operation.before;
                }
                else if constexpr (std::same_as<Type, CellsPaintedOperation>)
                {
                    LayerSlot& layer = layers[operation.layer.index];
                    for (const CellDelta& delta : operation.deltas)
                        set_cell(layer, delta.coordinate,
                            forward ? delta.after : delta.before);
                }
                else if constexpr (std::same_as<Type, MapObjectCreatedOperation>)
                    set_active_state(operation, forward);
                else if constexpr (std::same_as<Type, MapObjectRemovedOperation>)
                    set_active_state(operation, !forward);
                else if constexpr (std::same_as<Type, MapObjectChangedOperation>)
                {
                    objects[operation.handle.index].descriptor =
                        forward ? operation.after : operation.before;
                }
            }, payload);
        }
    };

    Document::Document(std::unique_ptr<Impl> impl) noexcept
        : impl_(std::move(impl))
    {
    }

    Document::Document(Document&&) noexcept = default;
    Document& Document::operator=(Document&&) noexcept = default;
    Document::~Document() = default;

    CreateResult Document::create(
        DocumentHandle handle,
        MapDescriptor descriptor,
        DocumentLimits limits) noexcept
    {
        try
        {
            if (!handle)
                return {ResultCode::invalid_handle, {}};
            if (!detail::valid_map_descriptor(descriptor, limits))
            {
                return {
                    limits.valid() ? ResultCode::invalid_descriptor
                                   : ResultCode::invalid_limits,
                    {}};
            }
            auto impl = std::make_unique<Impl>();
            impl->handle = handle;
            impl->descriptor = std::move(descriptor);
            impl->limits = limits;
            impl->revision.sequence = 1u;
            impl->revision.content = impl->state_content();
            if (impl->revision.content.empty())
                return {ResultCode::allocation_failure, {}};
            return {
                ResultCode::success,
                std::unique_ptr<Document>{new Document{std::move(impl)}}};
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ResultCode::invalid_document, {}};
        }
    }

    bool Document::valid() const noexcept
    {
        return impl_ && impl_->handle && impl_->revision
            && detail::valid_map_descriptor(impl_->descriptor, impl_->limits);
    }

    DocumentHandle Document::handle() const noexcept
    {
        return impl_ ? impl_->handle : DocumentHandle{};
    }

    const MapDescriptor& Document::descriptor() const noexcept
    {
        static const MapDescriptor invalid{};
        return impl_ ? impl_->descriptor : invalid;
    }

    const DocumentRevision& Document::revision() const noexcept
    {
        static const DocumentRevision invalid{};
        return impl_ ? impl_->revision : invalid;
    }

    const DocumentLimits& Document::limits() const noexcept
    {
        static const DocumentLimits invalid{};
        return impl_ ? impl_->limits : invalid;
    }

    DocumentMetrics Document::metrics() const noexcept
    {
        DocumentMetrics metrics{};
        if (!impl_)
            return metrics;
        for (const auto& slot : impl_->tile_sets)
            metrics.active_tile_sets += slot.descriptor ? 1u : 0u;
        for (const auto& slot : impl_->palette)
            metrics.active_palette_entries += slot.descriptor ? 1u : 0u;
        for (const auto& slot : impl_->layers)
        {
            if (!slot.descriptor)
                continue;
            ++metrics.active_layers;
            metrics.active_chunks += slot.chunks.size();
            for (const Impl::Chunk& chunk : slot.chunks)
            {
                metrics.occupied_cells += chunk.occupied;
                if (slot.descriptor->collision_source)
                {
                    for (const CellValue& value : chunk.cells)
                    {
                        if (!value.occupied())
                            continue;
                        const auto* palette = Impl::resolve(
                            impl_->palette, value.palette);
                        if (palette && palette->descriptor->collision.kind
                            != asset::tilemap::CollisionKind::none)
                        {
                            ++metrics.collision_cells;
                        }
                    }
                }
            }
        }
        for (const auto& slot : impl_->objects)
            metrics.active_objects += slot.descriptor ? 1u : 0u;
        metrics.operation_count = impl_->operations.size();
        metrics.applied_operation_count = impl_->operation_cursor;
        metrics.retained_history_bytes = impl_->retained_history_bytes;
        metrics.revision_sequence = impl_->revision.sequence;
        return metrics;
    }

    bool Document::can_undo() const noexcept
    {
        return impl_ && impl_->operation_cursor != 0u;
    }

    bool Document::can_redo() const noexcept
    {
        return impl_ && impl_->operation_cursor < impl_->operations.size();
    }

    std::vector<OperationRecord> Document::history() const
    {
        if (!impl_)
            return {};
        std::vector<OperationRecord> result = impl_->operations;
        for (std::size_t index = 0u; index < result.size(); ++index)
            result[index].currently_applied = index < impl_->operation_cursor;
        return result;
    }

    MutationResult Document::create_tile_set(
        TileSetDescriptor descriptor) noexcept
    {
        try
        {
            if (!valid())
                return {ResultCode::invalid_document};
            if (!detail::valid_tile_set_descriptor(descriptor, impl_->limits))
                return {ResultCode::invalid_descriptor, impl_->revision};
            if (metrics().active_tile_sets >= impl_->limits.maximum_tile_sets)
                return {ResultCode::capacity_exceeded, impl_->revision};
            for (const auto& slot : impl_->tile_sets)
            {
                if (!slot.descriptor)
                    continue;
                if (slot.descriptor->name == descriptor.name)
                    return {ResultCode::duplicate_name, impl_->revision};
                if (slot.descriptor->texture.stable_material_key
                        == descriptor.texture.stable_material_key
                    && slot.descriptor->texture != descriptor.texture)
                {
                    return {
                        ResultCode::duplicate_texture_dependency,
                        impl_->revision};
                }
            }
            const TileSetHandle handle{
                static_cast<std::uint32_t>(impl_->tile_sets.size()), 1u};
            TileSetCreatedOperation operation{handle, std::move(descriptor)};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            impl_->tile_sets.push_back({1u, operation.descriptor});
            MutationResult result = impl_->commit(std::move(payload));
            result.tile_set = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
        catch (...)
        {
            return {ResultCode::invalid_document,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
    }

    MutationResult Document::remove_tile_set(TileSetHandle handle) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        Impl::Slot<TileSetDescriptor>* slot = Impl::resolve(
            impl_->tile_sets, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->tile_sets, handle), impl_->revision};
        if (impl_->tile_set_referenced(handle))
            return {ResultCode::referenced_resource, impl_->revision};
        try
        {
            TileSetRemovedOperation operation{handle, *slot->descriptor};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            slot->descriptor.reset();
            MutationResult result = impl_->commit(std::move(payload));
            result.tile_set = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::create_palette_entry(
        PaletteEntryDescriptor descriptor) noexcept
    {
        try
        {
            if (!valid())
                return {ResultCode::invalid_document};
            const auto* tileSet = Impl::resolve(impl_->tile_sets, descriptor.tile_set);
            if (!tileSet)
            {
                return {
                    Impl::handle_failure(impl_->tile_sets, descriptor.tile_set),
                    impl_->revision};
            }
            if (descriptor.name.empty()
                || descriptor.name.size() > impl_->limits.maximum_name_bytes
                || descriptor.local_tile >= tileSet->descriptor->tile_count
                || !detail::valid_collision(descriptor.collision)
                || descriptor.animation_frame_count == 0u)
            {
                return {ResultCode::invalid_descriptor, impl_->revision};
            }
            if (metrics().active_palette_entries
                >= impl_->limits.maximum_palette_entries)
            {
                return {ResultCode::capacity_exceeded, impl_->revision};
            }
            if (std::any_of(
                    impl_->palette.begin(), impl_->palette.end(),
                    [&](const auto& slot)
                    {
                        return slot.descriptor
                            && slot.descriptor->name == descriptor.name;
                    }))
            {
                return {ResultCode::duplicate_name, impl_->revision};
            }
            const PaletteEntryHandle handle{
                static_cast<std::uint32_t>(impl_->palette.size()), 1u};
            PaletteEntryCreatedOperation operation{handle, std::move(descriptor)};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            impl_->palette.push_back({1u, operation.descriptor});
            MutationResult result = impl_->commit(std::move(payload));
            result.palette_entry = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
    }

    MutationResult Document::remove_palette_entry(
        PaletteEntryHandle handle) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        auto* slot = Impl::resolve(impl_->palette, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->palette, handle), impl_->revision};
        if (impl_->palette_referenced(handle))
            return {ResultCode::referenced_resource, impl_->revision};
        try
        {
            PaletteEntryRemovedOperation operation{handle, *slot->descriptor};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            slot->descriptor.reset();
            MutationResult result = impl_->commit(std::move(payload));
            result.palette_entry = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::create_layer(LayerDescriptor descriptor) noexcept
    {
        try
        {
            if (!valid())
                return {ResultCode::invalid_document};
            if (!detail::valid_layer_descriptor(
                    descriptor, impl_->descriptor, impl_->limits))
            {
                return {ResultCode::invalid_descriptor, impl_->revision};
            }
            if (metrics().active_layers >= impl_->limits.maximum_layers)
                return {ResultCode::capacity_exceeded, impl_->revision};
            if (std::any_of(
                    impl_->layers.begin(), impl_->layers.end(),
                    [&](const auto& slot)
                    {
                        return slot.descriptor
                            && slot.descriptor->name == descriptor.name;
                    }))
            {
                return {ResultCode::duplicate_name, impl_->revision};
            }
            const LayerHandle handle{
                static_cast<std::uint32_t>(impl_->layers.size()), 1u};
            LayerCreatedOperation operation{handle, std::move(descriptor)};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            impl_->layers.push_back({1u, operation.descriptor, {}});
            MutationResult result = impl_->commit(std::move(payload));
            result.layer = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
    }

    MutationResult Document::remove_layer(LayerHandle handle) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        Impl::LayerSlot* slot = Impl::resolve(impl_->layers, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->layers, handle), impl_->revision};
        try
        {
            LayerRemovedOperation operation{};
            operation.handle = handle;
            operation.descriptor = *slot->descriptor;
            for (const Impl::Chunk& chunk : slot->chunks)
            {
                ChunkSnapshot snapshot{};
                snapshot.layer = handle;
                snapshot.coordinate = chunk.coordinate;
                snapshot.extent = chunk.extent;
                snapshot.revision = chunk.revision;
                for (std::uint32_t y = 0u; y < chunk.extent.y; ++y)
                {
                    for (std::uint32_t x = 0u; x < chunk.extent.x; ++x)
                    {
                        const CellValue& value = chunk.cells[
                            static_cast<std::size_t>(y) * chunk.extent.x + x];
                        if (value.occupied())
                        {
                            snapshot.occupied_cells.push_back({
                                {chunk.coordinate.x * slot->descriptor->chunk_extent.x + x,
                                 chunk.coordinate.y * slot->descriptor->chunk_extent.y + y},
                                value});
                        }
                    }
                }
                operation.removed_chunks.push_back(std::move(snapshot));
            }
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            const std::uint64_t affected = std::accumulate(
                slot->chunks.begin(), slot->chunks.end(), std::uint64_t{},
                [](std::uint64_t total, const Impl::Chunk& chunk)
                {
                    return total + chunk.occupied;
                });
            slot->descriptor.reset();
            slot->chunks.clear();
            MutationResult result = impl_->commit(std::move(payload));
            result.layer = handle;
            result.affected_cells = affected;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::set_layer_properties(
        LayerHandle handle,
        LayerDescriptor descriptor) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        Impl::LayerSlot* slot = Impl::resolve(impl_->layers, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->layers, handle), impl_->revision};
        if (!detail::valid_layer_descriptor(
                descriptor, impl_->descriptor, impl_->limits))
        {
            return {ResultCode::invalid_descriptor, impl_->revision};
        }
        if (!slot->chunks.empty()
            && (descriptor.extent_tiles != slot->descriptor->extent_tiles
                || descriptor.chunk_extent != slot->descriptor->chunk_extent))
        {
            return {ResultCode::referenced_resource, impl_->revision};
        }
        if (descriptor == *slot->descriptor)
            return {ResultCode::unchanged, impl_->revision};
        if (std::any_of(
                impl_->layers.begin(), impl_->layers.end(),
                [&](const Impl::LayerSlot& candidate)
                {
                    return &candidate != slot && candidate.descriptor
                        && candidate.descriptor->name == descriptor.name;
                }))
        {
            return {ResultCode::duplicate_name, impl_->revision};
        }
        try
        {
            LayerPropertiesChangedOperation operation{
                handle, *slot->descriptor, std::move(descriptor)};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            slot->descriptor = operation.after;
            MutationResult result = impl_->commit(std::move(payload));
            result.layer = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::set_palette_collision(
        PaletteEntryHandle handle,
        CollisionShape collision) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        auto* slot = Impl::resolve(impl_->palette, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->palette, handle), impl_->revision};
        if (!detail::valid_collision(collision))
            return {ResultCode::invalid_descriptor, impl_->revision};
        if (collision == slot->descriptor->collision)
            return {ResultCode::unchanged, impl_->revision};
        try
        {
            PaletteCollisionChangedOperation operation{
                handle, slot->descriptor->collision, collision};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            slot->descriptor->collision = collision;
            MutationResult result = impl_->commit(std::move(payload));
            result.palette_entry = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::paint_cells(
        LayerHandle layerHandle,
        std::span<const CellEdit> edits) noexcept
    {
        try
        {
            if (!valid())
                return {ResultCode::invalid_document};
            Impl::LayerSlot* layer = Impl::resolve(impl_->layers, layerHandle);
            if (!layer)
                return {Impl::handle_failure(impl_->layers, layerHandle), impl_->revision};
            if (layer->descriptor->locked)
                return {ResultCode::locked_layer, impl_->revision};
            if (edits.empty())
                return {ResultCode::unchanged, impl_->revision};
            if (edits.size() > impl_->limits.maximum_cell_edits_per_operation)
                return {ResultCode::capacity_exceeded, impl_->revision};

            std::vector<CellEdit> ordered{edits.begin(), edits.end()};
            std::sort(ordered.begin(), ordered.end(),
                [](const CellEdit& left, const CellEdit& right)
                {
                    return std::tuple{left.coordinate.y, left.coordinate.x}
                        < std::tuple{right.coordinate.y, right.coordinate.x};
                });
            for (std::size_t index = 0u; index < ordered.size(); ++index)
            {
                const CellEdit& edit = ordered[index];
                if (edit.coordinate.x >= layer->descriptor->extent_tiles.x
                    || edit.coordinate.y >= layer->descriptor->extent_tiles.y)
                {
                    return {ResultCode::out_of_bounds, impl_->revision};
                }
                if (index != 0u
                    && ordered[index - 1u].coordinate == edit.coordinate)
                {
                    return {ResultCode::invalid_descriptor, impl_->revision};
                }
                if (edit.value.occupied())
                {
                    const auto* palette = Impl::resolve(
                        impl_->palette, edit.value.palette);
                    if (!palette)
                    {
                        return {
                            Impl::handle_failure(
                                impl_->palette, edit.value.palette),
                            impl_->revision};
                    }
                    if (!detail::valid_transform(edit.value.transform)
                        || edit.value.animation_frame
                            >= palette->descriptor->animation_frame_count)
                    {
                        return {ResultCode::invalid_descriptor, impl_->revision};
                    }
                }
            }

            CellsPaintedOperation operation{};
            operation.layer = layerHandle;
            for (const CellEdit& edit : ordered)
            {
                const CellValue before = impl_->cell_value(*layer, edit.coordinate);
                if (before != edit.value)
                {
                    operation.deltas.push_back({
                        edit.coordinate, before, edit.value});
                }
            }
            if (operation.deltas.empty())
                return {ResultCode::unchanged, impl_->revision};

            const DocumentMetrics beforeMetrics = metrics();
            std::uint64_t additions{};
            std::uint64_t removals{};
            for (const CellDelta& delta : operation.deltas)
            {
                additions += !delta.before.occupied() && delta.after.occupied();
                removals += delta.before.occupied() && !delta.after.occupied();
            }
            if (beforeMetrics.occupied_cells - removals
                > impl_->limits.maximum_occupied_cells - additions)
            {
                return {ResultCode::capacity_exceeded, impl_->revision};
            }

            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            for (const CellDelta& delta : operation.deltas)
                impl_->set_cell(*layer, delta.coordinate, delta.after);
            MutationResult result = impl_->commit(std::move(payload));
            result.layer = layerHandle;
            result.affected_cells = operation.deltas.size();
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
        catch (...)
        {
            return {ResultCode::invalid_document,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
    }

    MutationResult Document::create_object(
        MapObjectDescriptor descriptor) noexcept
    {
        try
        {
            if (!valid())
                return {ResultCode::invalid_document};
            if (!detail::valid_object_descriptor(descriptor, impl_->limits))
                return {ResultCode::invalid_descriptor, impl_->revision};
            if (metrics().active_objects >= impl_->limits.maximum_objects)
                return {ResultCode::capacity_exceeded, impl_->revision};
            if (std::any_of(
                    impl_->objects.begin(), impl_->objects.end(),
                    [&](const auto& slot)
                    {
                        return slot.descriptor
                            && slot.descriptor->name == descriptor.name;
                    }))
            {
                return {ResultCode::duplicate_name, impl_->revision};
            }
            const MapObjectHandle handle{
                static_cast<std::uint32_t>(impl_->objects.size()), 1u};
            MapObjectCreatedOperation operation{handle, std::move(descriptor)};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            impl_->objects.push_back({1u, operation.descriptor});
            MutationResult result = impl_->commit(std::move(payload));
            result.object = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure,
                impl_ ? impl_->revision : DocumentRevision{}};
        }
    }

    MutationResult Document::remove_object(MapObjectHandle handle) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        auto* slot = Impl::resolve(impl_->objects, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->objects, handle), impl_->revision};
        try
        {
            MapObjectRemovedOperation operation{handle, *slot->descriptor};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            slot->descriptor.reset();
            MutationResult result = impl_->commit(std::move(payload));
            result.object = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::set_object_properties(
        MapObjectHandle handle,
        MapObjectDescriptor descriptor) noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        auto* slot = Impl::resolve(impl_->objects, handle);
        if (!slot)
            return {Impl::handle_failure(impl_->objects, handle), impl_->revision};
        if (!detail::valid_object_descriptor(descriptor, impl_->limits))
            return {ResultCode::invalid_descriptor, impl_->revision};
        if (descriptor == *slot->descriptor)
            return {ResultCode::unchanged, impl_->revision};
        if (std::any_of(
                impl_->objects.begin(), impl_->objects.end(),
                [&](const auto& candidate)
                {
                    return &candidate != slot && candidate.descriptor
                        && candidate.descriptor->name == descriptor.name;
                }))
        {
            return {ResultCode::duplicate_name, impl_->revision};
        }
        try
        {
            MapObjectChangedOperation operation{
                handle, *slot->descriptor, std::move(descriptor)};
            OperationPayload payload = operation;
            if (!impl_->reserve_history(payload))
                return {ResultCode::history_limit_exceeded, impl_->revision};
            slot->descriptor = operation.after;
            MutationResult result = impl_->commit(std::move(payload));
            result.object = handle;
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, impl_->revision};
        }
    }

    MutationResult Document::undo() noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        if (impl_->operation_cursor == 0u)
            return {ResultCode::nothing_to_undo, impl_->revision};
        try
        {
            OperationRecord& record = impl_->operations[impl_->operation_cursor - 1u];
            impl_->apply_payload(record.payload, false);
            --impl_->operation_cursor;
            record.currently_applied = false;
            impl_->advance_revision();
            return {
                .code = ResultCode::success,
                .revision = impl_->revision,
                .operation = record.id};
        }
        catch (...)
        {
            return {ResultCode::invalid_document, impl_->revision};
        }
    }

    MutationResult Document::redo() noexcept
    {
        if (!valid())
            return {ResultCode::invalid_document};
        if (impl_->operation_cursor >= impl_->operations.size())
            return {ResultCode::nothing_to_redo, impl_->revision};
        try
        {
            OperationRecord& record = impl_->operations[impl_->operation_cursor];
            impl_->apply_payload(record.payload, true);
            ++impl_->operation_cursor;
            record.currently_applied = true;
            impl_->advance_revision();
            return {
                .code = ResultCode::success,
                .revision = impl_->revision,
                .operation = record.id};
        }
        catch (...)
        {
            return {ResultCode::invalid_document, impl_->revision};
        }
    }

    std::optional<TileSetDescriptor> Document::tile_set(
        TileSetHandle handle) const
    {
        if (!impl_)
            return std::nullopt;
        const auto* slot = Impl::resolve(impl_->tile_sets, handle);
        return slot ? slot->descriptor : std::nullopt;
    }

    std::optional<PaletteEntryDescriptor> Document::palette_entry(
        PaletteEntryHandle handle) const
    {
        if (!impl_)
            return std::nullopt;
        const auto* slot = Impl::resolve(impl_->palette, handle);
        return slot ? slot->descriptor : std::nullopt;
    }

    std::optional<LayerDescriptor> Document::layer(LayerHandle handle) const
    {
        if (!impl_)
            return std::nullopt;
        const auto* slot = Impl::resolve(impl_->layers, handle);
        return slot ? slot->descriptor : std::nullopt;
    }

    std::optional<CellValue> Document::cell(
        LayerHandle layerHandle,
        UInt2 coordinate) const
    {
        if (!impl_)
            return std::nullopt;
        const Impl::LayerSlot* layer = Impl::resolve(impl_->layers, layerHandle);
        if (!layer || coordinate.x >= layer->descriptor->extent_tiles.x
            || coordinate.y >= layer->descriptor->extent_tiles.y)
        {
            return std::nullopt;
        }
        return impl_->cell_value(*layer, coordinate);
    }

    std::optional<MapObjectDescriptor> Document::object(
        MapObjectHandle handle) const
    {
        if (!impl_)
            return std::nullopt;
        const auto* slot = Impl::resolve(impl_->objects, handle);
        return slot ? slot->descriptor : std::nullopt;
    }

    DocumentSnapshot Document::snapshot() const
    {
        DocumentSnapshot result{};
        if (!impl_)
            return result;
        result.handle = impl_->handle;
        result.descriptor = impl_->descriptor;
        result.revision = impl_->revision;
        for (std::uint32_t index = 0u; index < impl_->tile_sets.size(); ++index)
        {
            const auto& slot = impl_->tile_sets[index];
            if (slot.descriptor)
            {
                result.tile_sets.push_back({
                    {index, slot.generation}, *slot.descriptor});
            }
        }
        for (std::uint32_t index = 0u; index < impl_->palette.size(); ++index)
        {
            const auto& slot = impl_->palette[index];
            if (slot.descriptor)
            {
                result.palette.push_back({
                    {index, slot.generation}, *slot.descriptor});
            }
        }
        for (std::uint32_t index = 0u; index < impl_->layers.size(); ++index)
        {
            const Impl::LayerSlot& slot = impl_->layers[index];
            if (!slot.descriptor)
                continue;
            const LayerHandle handle{index, slot.generation};
            result.layers.push_back({handle, *slot.descriptor});
            for (const Impl::Chunk& chunk : slot.chunks)
            {
                ChunkSnapshot snapshot{};
                snapshot.layer = handle;
                snapshot.coordinate = chunk.coordinate;
                snapshot.extent = chunk.extent;
                snapshot.revision = chunk.revision;
                for (std::uint32_t y = 0u; y < chunk.extent.y; ++y)
                {
                    for (std::uint32_t x = 0u; x < chunk.extent.x; ++x)
                    {
                        const CellValue& value = chunk.cells[
                            static_cast<std::size_t>(y) * chunk.extent.x + x];
                        if (!value.occupied())
                            continue;
                        snapshot.occupied_cells.push_back({
                            {chunk.coordinate.x * slot.descriptor->chunk_extent.x + x,
                             chunk.coordinate.y * slot.descriptor->chunk_extent.y + y},
                            value});
                    }
                }
                result.chunks.push_back(std::move(snapshot));
            }
        }
        for (std::uint32_t index = 0u; index < impl_->objects.size(); ++index)
        {
            const auto& slot = impl_->objects[index];
            if (slot.descriptor)
            {
                result.objects.push_back({
                    {index, slot.generation}, *slot.descriptor});
            }
        }
        return result;
    }

    RestoreResult Document::restore(
        const DocumentSnapshot& snapshot,
        DocumentLimits limits) noexcept
    {
        try
        {
            if (!snapshot.handle || !snapshot.revision
                || !detail::valid_map_descriptor(snapshot.descriptor, limits))
            {
                return {ResultCode::invalid_document, {}};
            }
            if (snapshot.tile_sets.size() > limits.maximum_tile_sets
                || snapshot.palette.size() > limits.maximum_palette_entries
                || snapshot.layers.size() > limits.maximum_layers
                || snapshot.chunks.size() > limits.maximum_chunks
                || snapshot.objects.size() > limits.maximum_objects)
            {
                return {ResultCode::capacity_exceeded, {}};
            }
            auto impl = std::make_unique<Impl>();
            impl->handle = snapshot.handle;
            impl->descriptor = snapshot.descriptor;
            impl->limits = limits;
            impl->revision = snapshot.revision;

            for (const TileSetSnapshot& value : snapshot.tile_sets)
            {
                if (!value.handle
                    || value.handle.index >= limits.maximum_tile_sets
                    || !detail::valid_tile_set_descriptor(value.descriptor, limits))
                {
                    return {ResultCode::invalid_descriptor, {}};
                }
                if (impl->tile_sets.size() <= value.handle.index)
                    impl->tile_sets.resize(value.handle.index + 1u);
                auto& slot = impl->tile_sets[value.handle.index];
                if (slot.descriptor)
                    return {ResultCode::invalid_descriptor, {}};
                slot.generation = value.handle.generation;
                slot.descriptor = value.descriptor;
            }
            for (const PaletteEntrySnapshot& value : snapshot.palette)
            {
                if (!value.handle
                    || value.handle.index >= limits.maximum_palette_entries
                    || value.descriptor.name.empty()
                    || value.descriptor.name.size() > limits.maximum_name_bytes
                    || !Impl::resolve(impl->tile_sets, value.descriptor.tile_set)
                    || !detail::valid_collision(value.descriptor.collision)
                    || value.descriptor.animation_frame_count == 0u)
                {
                    return {ResultCode::dangling_reference, {}};
                }
                const auto* tileSet = Impl::resolve(
                    impl->tile_sets, value.descriptor.tile_set);
                if (value.descriptor.local_tile >= tileSet->descriptor->tile_count)
                    return {ResultCode::invalid_descriptor, {}};
                if (impl->palette.size() <= value.handle.index)
                    impl->palette.resize(value.handle.index + 1u);
                auto& slot = impl->palette[value.handle.index];
                if (slot.descriptor)
                    return {ResultCode::invalid_descriptor, {}};
                slot.generation = value.handle.generation;
                slot.descriptor = value.descriptor;
            }
            for (const LayerSnapshot& value : snapshot.layers)
            {
                if (!value.handle || value.handle.index >= limits.maximum_layers
                    || !detail::valid_layer_descriptor(
                        value.descriptor, snapshot.descriptor, limits))
                {
                    return {ResultCode::invalid_descriptor, {}};
                }
                if (impl->layers.size() <= value.handle.index)
                    impl->layers.resize(value.handle.index + 1u);
                Impl::LayerSlot& slot = impl->layers[value.handle.index];
                if (slot.descriptor)
                    return {ResultCode::invalid_descriptor, {}};
                slot.generation = value.handle.generation;
                slot.descriptor = value.descriptor;
            }
            std::uint64_t occupied{};
            for (const ChunkSnapshot& value : snapshot.chunks)
            {
                Impl::LayerSlot* layer = Impl::resolve(impl->layers, value.layer);
                if (!layer || value.revision == 0u
                    || value.extent != detail::chunk_extent_at(
                        *layer->descriptor, value.coordinate))
                {
                    return {ResultCode::dangling_reference, {}};
                }
                const UInt2 chunkGrid{
                    (layer->descriptor->extent_tiles.x
                        + layer->descriptor->chunk_extent.x - 1u)
                        / layer->descriptor->chunk_extent.x,
                    (layer->descriptor->extent_tiles.y
                        + layer->descriptor->chunk_extent.y - 1u)
                        / layer->descriptor->chunk_extent.y};
                if (value.coordinate.x >= chunkGrid.x
                    || value.coordinate.y >= chunkGrid.y)
                {
                    return {ResultCode::out_of_bounds, {}};
                }
                Impl::Chunk chunk{};
                chunk.coordinate = value.coordinate;
                chunk.extent = value.extent;
                chunk.revision = value.revision;
                chunk.cells.resize(
                    static_cast<std::size_t>(chunk.extent.x) * chunk.extent.y);
                for (const CellEdit& edit : value.occupied_cells)
                {
                    if (!edit.value.occupied()
                        || !Impl::resolve(impl->palette, edit.value.palette)
                        || !detail::valid_transform(edit.value.transform)
                        || edit.coordinate.x >= layer->descriptor->extent_tiles.x
                        || edit.coordinate.y >= layer->descriptor->extent_tiles.y)
                    {
                        return {ResultCode::dangling_reference, {}};
                    }
                    const UInt2 expectedChunk{
                        edit.coordinate.x / layer->descriptor->chunk_extent.x,
                        edit.coordinate.y / layer->descriptor->chunk_extent.y};
                    if (expectedChunk != value.coordinate)
                        return {ResultCode::out_of_bounds, {}};
                    const UInt2 local{
                        edit.coordinate.x % layer->descriptor->chunk_extent.x,
                        edit.coordinate.y % layer->descriptor->chunk_extent.y};
                    CellValue& target = chunk.cells[
                        static_cast<std::size_t>(local.y) * chunk.extent.x + local.x];
                    if (target.occupied())
                        return {ResultCode::invalid_descriptor, {}};
                    const auto* palette = Impl::resolve(
                        impl->palette, edit.value.palette);
                    if (edit.value.animation_frame
                        >= palette->descriptor->animation_frame_count)
                    {
                        return {ResultCode::invalid_descriptor, {}};
                    }
                    target = edit.value;
                    ++chunk.occupied;
                    ++occupied;
                }
                if (chunk.occupied == 0u)
                    return {ResultCode::invalid_descriptor, {}};
                layer->chunks.push_back(std::move(chunk));
            }
            if (occupied > limits.maximum_occupied_cells)
                return {ResultCode::capacity_exceeded, {}};
            for (Impl::LayerSlot& layer : impl->layers)
            {
                std::sort(layer.chunks.begin(), layer.chunks.end(),
                    [](const Impl::Chunk& left, const Impl::Chunk& right)
                    {
                        return std::tuple{left.coordinate.y, left.coordinate.x}
                            < std::tuple{right.coordinate.y, right.coordinate.x};
                    });
                for (std::size_t index = 1u; index < layer.chunks.size(); ++index)
                {
                    if (layer.chunks[index - 1u].coordinate
                        == layer.chunks[index].coordinate)
                    {
                        return {ResultCode::invalid_descriptor, {}};
                    }
                }
            }
            for (const MapObjectSnapshot& value : snapshot.objects)
            {
                if (!value.handle || value.handle.index >= limits.maximum_objects
                    || !detail::valid_object_descriptor(value.descriptor, limits))
                {
                    return {ResultCode::invalid_descriptor, {}};
                }
                if (impl->objects.size() <= value.handle.index)
                    impl->objects.resize(value.handle.index + 1u);
                auto& slot = impl->objects[value.handle.index];
                if (slot.descriptor)
                    return {ResultCode::invalid_descriptor, {}};
                slot.generation = value.handle.generation;
                slot.descriptor = value.descriptor;
            }
            const ContentHash expected = impl->state_content();
            if (expected.empty() || expected != snapshot.revision.content)
                return {ResultCode::invalid_document, {}};
            return {
                ResultCode::success,
                std::unique_ptr<Document>{new Document{std::move(impl)}}};
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ResultCode::malformed_payload, {}};
        }
    }

    SnapshotSerializationResult Document::serialize() const noexcept
    {
        try
        {
            if (!valid())
                return {ResultCode::invalid_document, {}};
            detail::Writer writer{impl_->limits.maximum_snapshot_bytes};
            writer.raw(detail::source_magic);
            writer.integer(detail::source_schema_version);
            writer.hash(impl_->revision.content);
            writer.integer(impl_->revision.sequence);
            const auto state = impl_->canonical_state_bytes();
            writer.integer(static_cast<std::uint64_t>(state.size()));
            writer.raw(state);
            if (!writer.good())
                return {ResultCode::capacity_exceeded, {}};
            return {ResultCode::success, writer.take()};
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ResultCode::malformed_payload, {}};
        }
    }

    RestoreResult Document::deserialize(
        std::span<const std::byte> bytes,
        DocumentLimits limits) noexcept
    {
        try
        {
            if (!limits.valid())
                return {ResultCode::invalid_limits, {}};
            if (bytes.empty() || bytes.size() > limits.maximum_snapshot_bytes)
                return {ResultCode::capacity_exceeded, {}};
            detail::Reader reader{bytes};
            if (!std::ranges::equal(
                    reader.raw(detail::source_magic.size()), detail::source_magic))
            {
                return {ResultCode::malformed_payload, {}};
            }
            if (reader.integer<std::uint32_t>() != detail::source_schema_version)
                return {ResultCode::unsupported_schema, {}};
            DocumentSnapshot snapshot{};
            snapshot.revision.content = reader.hash();
            snapshot.revision.sequence = reader.integer<std::uint64_t>();
            const std::uint64_t stateBytes = reader.integer<std::uint64_t>();
            if (!reader.good() || stateBytes > limits.maximum_snapshot_bytes)
                return {ResultCode::capacity_exceeded, {}};
            detail::Reader state{reader.raw(static_cast<std::size_t>(stateBytes))};
            if (!reader.done())
                return {ResultCode::malformed_payload, {}};

            snapshot.descriptor.name = state.string(limits.maximum_name_bytes);
            snapshot.descriptor.extent_tiles = state.uint2();
            snapshot.descriptor.default_chunk_extent = state.uint2();
            snapshot.descriptor.schema_version = state.integer<std::uint32_t>();
            snapshot.handle = state.handle<DocumentHandle>();

            const auto boundedCount = [&](std::uint32_t maximum)
            {
                const std::uint32_t count = state.integer<std::uint32_t>();
                if (count > maximum)
                    state.fail();
                return count;
            };
            const std::uint32_t tileSetCount = boundedCount(limits.maximum_tile_sets);
            snapshot.tile_sets.reserve(tileSetCount);
            for (std::uint32_t index = 0u; index < tileSetCount && state.good(); ++index)
            {
                TileSetSnapshot value{};
                value.handle = state.handle<TileSetHandle>();
                value.descriptor.name = state.string(limits.maximum_name_bytes);
                value.descriptor.texture = detail::read_texture(state, limits);
                value.descriptor.tile_extent = state.uint2();
                value.descriptor.grid = state.uint2();
                value.descriptor.margin = state.uint2();
                value.descriptor.spacing = state.uint2();
                value.descriptor.tile_count = state.integer<std::uint32_t>();
                snapshot.tile_sets.push_back(std::move(value));
            }
            const std::uint32_t paletteCount = boundedCount(
                limits.maximum_palette_entries);
            snapshot.palette.reserve(paletteCount);
            for (std::uint32_t index = 0u; index < paletteCount && state.good(); ++index)
            {
                PaletteEntrySnapshot value{};
                value.handle = state.handle<PaletteEntryHandle>();
                value.descriptor.name = state.string(limits.maximum_name_bytes);
                value.descriptor.tile_set = state.handle<TileSetHandle>();
                value.descriptor.local_tile = state.integer<std::uint32_t>();
                value.descriptor.collision = detail::read_collision(state);
                value.descriptor.animation_frame_count = state.integer<std::uint16_t>();
                value.descriptor.animation_rate_millihertz = state.integer<std::uint16_t>();
                value.descriptor.user_flags = state.integer<std::uint32_t>();
                snapshot.palette.push_back(std::move(value));
            }
            const std::uint32_t layerCount = boundedCount(limits.maximum_layers);
            snapshot.layers.reserve(layerCount);
            for (std::uint32_t index = 0u; index < layerCount && state.good(); ++index)
            {
                LayerSnapshot layer{};
                layer.handle = state.handle<LayerHandle>();
                layer.descriptor.name = state.string(limits.maximum_name_bytes);
                layer.descriptor.extent_tiles = state.uint2();
                layer.descriptor.chunk_extent = state.uint2();
                layer.descriptor.world_origin = state.float2();
                layer.descriptor.tile_world_extent = state.float2();
                layer.descriptor.parallax = state.float2();
                layer.descriptor.phase = state.enumeration<LayerPhase>();
                layer.descriptor.draw_layer = state.signed_integer<std::int32_t>();
                layer.descriptor.opacity = state.floating();
                layer.descriptor.visible = state.boolean();
                layer.descriptor.locked = state.boolean();
                layer.descriptor.collision_source = state.boolean();
                snapshot.layers.push_back(layer);
                const std::uint32_t chunkCount = boundedCount(limits.maximum_chunks);
                for (std::uint32_t chunkIndex = 0u;
                     chunkIndex < chunkCount && state.good(); ++chunkIndex)
                {
                    ChunkSnapshot chunk{};
                    chunk.layer = layer.handle;
                    chunk.coordinate = state.uint2();
                    chunk.extent = state.uint2();
                    chunk.revision = state.integer<std::uint64_t>();
                    const std::uint64_t occupied = state.integer<std::uint64_t>();
                    if (occupied > limits.maximum_occupied_cells
                        || occupied > limits.maximum_snapshot_bytes)
                    {
                        state.fail();
                        break;
                    }
                    chunk.occupied_cells.reserve(static_cast<std::size_t>(occupied));
                    for (std::uint64_t cellIndex = 0u;
                         cellIndex < occupied && state.good(); ++cellIndex)
                    {
                        const UInt2 local = state.uint2();
                        chunk.occupied_cells.push_back({
                            {chunk.coordinate.x * layer.descriptor.chunk_extent.x
                                + local.x,
                             chunk.coordinate.y * layer.descriptor.chunk_extent.y
                                + local.y},
                            detail::read_cell(state)});
                    }
                    snapshot.chunks.push_back(std::move(chunk));
                }
            }
            const std::uint32_t objectCount = boundedCount(limits.maximum_objects);
            snapshot.objects.reserve(objectCount);
            for (std::uint32_t index = 0u; index < objectCount && state.good(); ++index)
            {
                MapObjectSnapshot value{};
                value.handle = state.handle<MapObjectHandle>();
                value.descriptor.name = state.string(limits.maximum_name_bytes);
                value.descriptor.type = state.string(limits.maximum_name_bytes);
                value.descriptor.position = state.float2();
                value.descriptor.size = state.float2();
                value.descriptor.rotation_radians = state.floating();
                value.descriptor.layer_bits = state.integer<std::uint32_t>();
                value.descriptor.user_flags = state.integer<std::uint32_t>();
                snapshot.objects.push_back(std::move(value));
            }
            if (!state.done())
                return {ResultCode::malformed_payload, {}};
            return restore(snapshot, limits);
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ResultCode::malformed_payload, {}};
        }
    }

    CompilationResult Document::compile(
        const asset::tilemap::ArtifactLimits& artifactLimits) const noexcept
    {
        try
        {
            if (!valid() || !artifactLimits.valid())
                return {ResultCode::invalid_document, {}};
            asset::tilemap::CompiledTileMapArtifact artifact{};
            artifact.name = impl_->descriptor.name;
            artifact.extent_tiles = impl_->descriptor.extent_tiles;
            artifact.default_chunk_extent = impl_->descriptor.default_chunk_extent;
            artifact.identity.source_revision = impl_->revision;

            std::vector<std::uint32_t> tileSetIndexes(
                impl_->tile_sets.size(), asset::tilemap::invalid_compiled_index);
            for (std::uint32_t sourceIndex = 0u;
                 sourceIndex < impl_->tile_sets.size(); ++sourceIndex)
            {
                const auto& slot = impl_->tile_sets[sourceIndex];
                if (!slot.descriptor)
                    continue;
                const TextureDependency& texture = slot.descriptor->texture;
                auto dependency = std::find_if(
                    artifact.dependencies.begin(), artifact.dependencies.end(),
                    [&](const TextureDependency& candidate)
                    {
                        return candidate == texture;
                    });
                std::uint32_t dependencyIndex{};
                if (dependency == artifact.dependencies.end())
                {
                    dependencyIndex = static_cast<std::uint32_t>(
                        artifact.dependencies.size());
                    artifact.dependencies.push_back(texture);
                }
                else
                {
                    dependencyIndex = static_cast<std::uint32_t>(
                        dependency - artifact.dependencies.begin());
                }
                tileSetIndexes[sourceIndex] = static_cast<std::uint32_t>(
                    artifact.tile_sets.size());
                artifact.tile_sets.push_back({
                    .id = {detail::stable_id(
                        TileSetHandle{sourceIndex, slot.generation})},
                    .dependency_index = dependencyIndex,
                    .tile_extent = slot.descriptor->tile_extent,
                    .grid = slot.descriptor->grid,
                    .margin = slot.descriptor->margin,
                    .spacing = slot.descriptor->spacing,
                    .tile_count = slot.descriptor->tile_count});
            }

            std::vector<std::uint32_t> paletteIndexes(
                impl_->palette.size(), asset::tilemap::invalid_compiled_index);
            for (std::uint32_t sourceIndex = 0u;
                 sourceIndex < impl_->palette.size(); ++sourceIndex)
            {
                const auto& slot = impl_->palette[sourceIndex];
                if (!slot.descriptor)
                    continue;
                const TileSetHandle tileSet = slot.descriptor->tile_set;
                if (tileSet.index >= tileSetIndexes.size()
                    || tileSetIndexes[tileSet.index]
                        == asset::tilemap::invalid_compiled_index)
                {
                    return {ResultCode::dangling_reference, {}};
                }
                paletteIndexes[sourceIndex] = static_cast<std::uint32_t>(
                    artifact.palette.size());
                artifact.palette.push_back({
                    .id = {detail::stable_id(
                        PaletteEntryHandle{sourceIndex, slot.generation})},
                    .tile_set_index = tileSetIndexes[tileSet.index],
                    .local_tile = slot.descriptor->local_tile,
                    .collision = slot.descriptor->collision,
                    .animation_frame_count =
                        slot.descriptor->animation_frame_count,
                    .animation_rate_millihertz =
                        slot.descriptor->animation_rate_millihertz,
                    .user_flags = slot.descriptor->user_flags});
            }

            std::vector<std::uint32_t> layerIndexes(
                impl_->layers.size(), asset::tilemap::invalid_compiled_index);
            for (std::uint32_t sourceIndex = 0u;
                 sourceIndex < impl_->layers.size(); ++sourceIndex)
            {
                const Impl::LayerSlot& slot = impl_->layers[sourceIndex];
                if (!slot.descriptor)
                    continue;
                layerIndexes[sourceIndex] = static_cast<std::uint32_t>(
                    artifact.layers.size());
                artifact.layers.push_back({
                    .id = {detail::stable_id(
                        LayerHandle{sourceIndex, slot.generation})},
                    .name = slot.descriptor->name,
                    .extent_tiles = slot.descriptor->extent_tiles,
                    .chunk_extent = slot.descriptor->chunk_extent,
                    .world_origin = slot.descriptor->world_origin,
                    .tile_world_extent = slot.descriptor->tile_world_extent,
                    .parallax = slot.descriptor->parallax,
                    .phase = slot.descriptor->phase,
                    .draw_layer = slot.descriptor->draw_layer,
                    .opacity = slot.descriptor->opacity,
                    .visible = slot.descriptor->visible,
                    .collision_source = slot.descriptor->collision_source});
            }

            for (std::uint32_t sourceLayer = 0u;
                 sourceLayer < impl_->layers.size(); ++sourceLayer)
            {
                const Impl::LayerSlot& slot = impl_->layers[sourceLayer];
                if (!slot.descriptor)
                    continue;
                const std::uint32_t compiledLayer = layerIndexes[sourceLayer];
                for (const Impl::Chunk& sourceChunk : slot.chunks)
                {
                    asset::tilemap::CompiledChunk chunk{};
                    chunk.layer_index = compiledLayer;
                    chunk.coordinate = sourceChunk.coordinate;
                    chunk.extent = sourceChunk.extent;
                    chunk.revision = sourceChunk.revision;
                    chunk.world_bounds = {
                        slot.descriptor->world_origin.x
                            + sourceChunk.coordinate.x
                                * slot.descriptor->chunk_extent.x
                                * slot.descriptor->tile_world_extent.x,
                        slot.descriptor->world_origin.y
                            + sourceChunk.coordinate.y
                                * slot.descriptor->chunk_extent.y
                                * slot.descriptor->tile_world_extent.y,
                        sourceChunk.extent.x
                            * slot.descriptor->tile_world_extent.x,
                        sourceChunk.extent.y
                            * slot.descriptor->tile_world_extent.y};
                    chunk.first_cell = static_cast<std::uint32_t>(
                        artifact.cells.size());
                    for (std::uint32_t y = 0u; y < sourceChunk.extent.y; ++y)
                    {
                        for (std::uint32_t x = 0u; x < sourceChunk.extent.x; ++x)
                        {
                            const CellValue& sourceCell = sourceChunk.cells[
                                static_cast<std::size_t>(y)
                                    * sourceChunk.extent.x + x];
                            if (!sourceCell.occupied())
                                continue;
                            if (sourceCell.palette.index >= paletteIndexes.size()
                                || paletteIndexes[sourceCell.palette.index]
                                    == asset::tilemap::invalid_compiled_index)
                            {
                                return {ResultCode::dangling_reference, {}};
                            }
                            const UInt2 mapCoordinate{
                                sourceChunk.coordinate.x
                                    * slot.descriptor->chunk_extent.x + x,
                                sourceChunk.coordinate.y
                                    * slot.descriptor->chunk_extent.y + y};
                            const std::uint32_t paletteIndex =
                                paletteIndexes[sourceCell.palette.index];
                            artifact.cells.push_back({
                                .coordinate = mapCoordinate,
                                .palette_index = paletteIndex,
                                .transform = sourceCell.transform,
                                .tint = sourceCell.tint,
                                .animation_frame = sourceCell.animation_frame,
                                .flags = sourceCell.flags});
                            const asset::tilemap::CompiledPaletteEntry& palette =
                                artifact.palette[paletteIndex];
                            if (slot.descriptor->collision_source
                                && palette.collision.kind
                                    != asset::tilemap::CollisionKind::none)
                            {
                                const auto local = palette.collision.local_bounds;
                                artifact.collision.push_back({
                                    .layer = artifact.layers[compiledLayer].id,
                                    .palette_entry = palette.id,
                                    .cell_coordinate = mapCoordinate,
                                    .kind = palette.collision.kind,
                                    .world_bounds = {
                                        slot.descriptor->world_origin.x
                                            + (mapCoordinate.x + local.x)
                                                * slot.descriptor->tile_world_extent.x,
                                        slot.descriptor->world_origin.y
                                            + (mapCoordinate.y + local.y)
                                                * slot.descriptor->tile_world_extent.y,
                                        local.width
                                            * slot.descriptor->tile_world_extent.x,
                                        local.height
                                            * slot.descriptor->tile_world_extent.y},
                                    .layer_bits = palette.collision.layer_bits,
                                    .mask_bits = palette.collision.mask_bits,
                                    .sensor = palette.collision.sensor});
                            }
                        }
                    }
                    chunk.cell_count = static_cast<std::uint32_t>(
                        artifact.cells.size() - chunk.first_cell);
                    if (chunk.cell_count != 0u)
                        artifact.chunks.push_back(chunk);
                }
            }

            for (std::uint32_t sourceIndex = 0u;
                 sourceIndex < impl_->objects.size(); ++sourceIndex)
            {
                const auto& slot = impl_->objects[sourceIndex];
                if (!slot.descriptor)
                    continue;
                artifact.objects.push_back({
                    .id = {detail::stable_id(
                        MapObjectHandle{sourceIndex, slot.generation})},
                    .name = slot.descriptor->name,
                    .type = slot.descriptor->type,
                    .position = slot.descriptor->position,
                    .size = slot.descriptor->size,
                    .rotation_radians = slot.descriptor->rotation_radians,
                    .layer_bits = slot.descriptor->layer_bits,
                    .user_flags = slot.descriptor->user_flags});
            }
            artifact.identity.key =
                asset::tilemap::compiled_tilemap_payload_content(artifact);
            const asset::tilemap::ArtifactValidation validation =
                asset::tilemap::validate(artifact, artifactLimits);
            if (!validation)
                return {ResultCode::compile_failure, {}};
            return {ResultCode::success, std::move(artifact)};
        }
        catch (const std::bad_alloc&)
        {
            return {ResultCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ResultCode::compile_failure, {}};
        }
    }
}
