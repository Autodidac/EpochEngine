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
#include <cstring>
#include <limits>
#include <new>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

module asset.tilemap_artifact;

namespace epochengine::asset::tilemap
{
    namespace detail
    {
        inline constexpr std::array<std::byte, 8> artifact_magic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'T'}, std::byte{'I'},
            std::byte{'L'}, std::byte{'E'}, std::byte{'M'}, std::byte{'P'}};

        template<typename Enum>
        [[nodiscard]] constexpr bool enum_between(
            Enum value,
            Enum first,
            Enum last) noexcept
        {
            return value >= first && value <= last;
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

        [[nodiscard]] constexpr bool checked_multiply(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& output) noexcept
        {
            if (left != 0
                && right > (std::numeric_limits<std::uint64_t>::max)() / left)
            {
                return false;
            }
            output = left * right;
            return true;
        }

        [[nodiscard]] bool finite(Float2 value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        [[nodiscard]] bool finite(RectF value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y)
                && std::isfinite(value.width) && std::isfinite(value.height);
        }

        [[nodiscard]] bool valid_collision_shape(
            const CollisionShape& shape) noexcept
        {
            if (!enum_between(
                    shape.kind,
                    CollisionKind::none,
                    CollisionKind::custom_box)
                || !finite(shape.local_bounds)
                || shape.layer_bits == 0u || shape.mask_bits == 0u)
            {
                return false;
            }
            if (shape.kind == CollisionKind::none)
                return true;
            if (shape.local_bounds.empty())
                return false;
            return shape.local_bounds.x >= 0.0f
                && shape.local_bounds.y >= 0.0f
                && shape.local_bounds.x + shape.local_bounds.width <= 1.0f
                && shape.local_bounds.y + shape.local_bounds.height <= 1.0f;
        }

        class Writer final
        {
        public:
            explicit Writer(std::uint64_t limit) noexcept : limit_(limit) {}

            [[nodiscard]] bool good() const noexcept
            {
                return good_;
            }

            [[nodiscard]] std::vector<std::byte> take() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::byte> bytes)
            {
                if (!reserve(bytes.size()))
                    return;
                bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
            }

            template<std::unsigned_integral Value>
            void integer(Value value)
            {
                if (!reserve(sizeof(Value)))
                    return;
                for (std::size_t byte = 0; byte < sizeof(Value); ++byte)
                {
                    bytes_.push_back(static_cast<std::byte>(
                        (value >> (byte * 8u)) & static_cast<Value>(0xffu)));
                }
            }

            template<std::signed_integral Value>
            void integer(Value value)
            {
                using Unsigned = std::make_unsigned_t<Value>;
                integer(static_cast<Unsigned>(value));
            }

            template<typename Enum>
                requires std::is_enum_v<Enum>
            void enumeration(Enum value)
            {
                using Underlying = std::underlying_type_t<Enum>;
                integer(static_cast<Underlying>(value));
            }

            void boolean(bool value)
            {
                integer<std::uint8_t>(value ? 1u : 0u);
            }

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

            void hash(const ContentHash& hash)
            {
                for (const std::uint64_t word : hash.words)
                    integer(word);
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

            void rect(RectF value)
            {
                floating(value.x);
                floating(value.y);
                floating(value.width);
                floating(value.height);
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

            [[nodiscard]] bool good() const noexcept
            {
                return good_;
            }

            [[nodiscard]] bool done() const noexcept
            {
                return good_ && offset_ == bytes_.size();
            }

            [[nodiscard]] std::span<const std::byte> raw(std::size_t count)
            {
                if (!good_ || count > bytes_.size() - offset_)
                {
                    good_ = false;
                    return {};
                }
                const std::span<const std::byte> result = bytes_.subspan(
                    offset_, count);
                offset_ += count;
                return result;
            }

            template<std::unsigned_integral Value>
            [[nodiscard]] Value integer()
            {
                const auto source = raw(sizeof(Value));
                if (!good_)
                    return {};
                Value value{};
                for (std::size_t byte = 0; byte < sizeof(Value); ++byte)
                {
                    value |= static_cast<Value>(
                        std::to_integer<std::uint8_t>(source[byte]))
                        << (byte * 8u);
                }
                return value;
            }

            template<std::signed_integral Value>
            [[nodiscard]] Value signed_integer()
            {
                using Unsigned = std::make_unsigned_t<Value>;
                return static_cast<Value>(integer<Unsigned>());
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
                const auto source = raw(size);
                if (!good_)
                    return {};
                return std::string{
                    reinterpret_cast<const char*>(source.data()), source.size()};
            }

            [[nodiscard]] ContentHash hash()
            {
                ContentHash value{};
                for (std::uint64_t& word : value.words)
                    word = integer<std::uint64_t>();
                return value;
            }

            [[nodiscard]] UInt2 uint2()
            {
                return {integer<std::uint32_t>(), integer<std::uint32_t>()};
            }

            [[nodiscard]] Float2 float2()
            {
                return {floating(), floating()};
            }

            [[nodiscard]] RectF rect()
            {
                return {floating(), floating(), floating(), floating()};
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t offset_{};
            bool good_{true};
        };

        void write_collision(Writer& writer, const CollisionShape& collision)
        {
            writer.enumeration(collision.kind);
            writer.rect(collision.local_bounds);
            writer.integer(collision.layer_bits);
            writer.integer(collision.mask_bits);
            writer.boolean(collision.sensor);
        }

        [[nodiscard]] CollisionShape read_collision(Reader& reader)
        {
            CollisionShape collision{};
            collision.kind = reader.enumeration<CollisionKind>();
            collision.local_bounds = reader.rect();
            collision.layer_bits = reader.integer<std::uint32_t>();
            collision.mask_bits = reader.integer<std::uint32_t>();
            collision.sensor = reader.boolean();
            return collision;
        }

        void write_payload(
            Writer& writer,
            const CompiledTileMapArtifact& artifact)
        {
            writer.integer(artifact.identity.schema_version);
            writer.hash(artifact.identity.source_revision.content);
            writer.integer(artifact.identity.source_revision.sequence);
            writer.string(artifact.name);
            writer.uint2(artifact.extent_tiles);
            writer.uint2(artifact.default_chunk_extent);

            writer.integer(static_cast<std::uint32_t>(artifact.dependencies.size()));
            for (const TextureDependency& dependency : artifact.dependencies)
            {
                writer.string(dependency.logical_path);
                for (const std::uint64_t word : dependency.artifact_key)
                    writer.integer(word);
                writer.integer(dependency.project_key);
                writer.integer(dependency.asset_key);
                writer.integer(dependency.artifact_revision);
                writer.integer(dependency.stable_material_key);
                writer.uint2(dependency.texture_extent);
                writer.enumeration(dependency.min_filter);
                writer.enumeration(dependency.mag_filter);
                writer.enumeration(dependency.address_u);
                writer.enumeration(dependency.address_v);
                writer.enumeration(dependency.alpha);
                writer.enumeration(dependency.color_space);
                writer.floating(dependency.alpha_cutoff);
            }

            writer.integer(static_cast<std::uint32_t>(artifact.tile_sets.size()));
            for (const CompiledTileSet& tileSet : artifact.tile_sets)
            {
                writer.integer(tileSet.id.value);
                writer.integer(tileSet.dependency_index);
                writer.uint2(tileSet.tile_extent);
                writer.uint2(tileSet.grid);
                writer.uint2(tileSet.margin);
                writer.uint2(tileSet.spacing);
                writer.integer(tileSet.tile_count);
            }

            writer.integer(static_cast<std::uint32_t>(artifact.palette.size()));
            for (const CompiledPaletteEntry& entry : artifact.palette)
            {
                writer.integer(entry.id.value);
                writer.integer(entry.tile_set_index);
                writer.integer(entry.local_tile);
                write_collision(writer, entry.collision);
                writer.integer(entry.animation_frame_count);
                writer.integer(entry.animation_rate_millihertz);
                writer.integer(entry.user_flags);
            }

            writer.integer(static_cast<std::uint32_t>(artifact.layers.size()));
            for (const CompiledLayer& layer : artifact.layers)
            {
                writer.integer(layer.id.value);
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
                writer.boolean(layer.collision_source);
            }

            writer.integer(static_cast<std::uint32_t>(artifact.chunks.size()));
            for (const CompiledChunk& chunk : artifact.chunks)
            {
                writer.integer(chunk.layer_index);
                writer.uint2(chunk.coordinate);
                writer.uint2(chunk.extent);
                writer.integer(chunk.revision);
                writer.rect(chunk.world_bounds);
                writer.integer(chunk.first_cell);
                writer.integer(chunk.cell_count);
            }

            writer.integer(static_cast<std::uint64_t>(artifact.cells.size()));
            for (const CompiledCell& cell : artifact.cells)
            {
                writer.uint2(cell.coordinate);
                writer.integer(cell.palette_index);
                writer.enumeration(cell.transform);
                writer.integer(cell.tint.r);
                writer.integer(cell.tint.g);
                writer.integer(cell.tint.b);
                writer.integer(cell.tint.a);
                writer.integer(cell.animation_frame);
                writer.integer(cell.flags);
            }

            writer.integer(static_cast<std::uint64_t>(artifact.collision.size()));
            for (const CollisionPrimitive& primitive : artifact.collision)
            {
                writer.integer(primitive.layer.value);
                writer.integer(primitive.palette_entry.value);
                writer.uint2(primitive.cell_coordinate);
                writer.enumeration(primitive.kind);
                writer.rect(primitive.world_bounds);
                writer.integer(primitive.layer_bits);
                writer.integer(primitive.mask_bits);
                writer.boolean(primitive.sensor);
            }

            writer.integer(static_cast<std::uint32_t>(artifact.objects.size()));
            for (const CompiledMapObject& object : artifact.objects)
            {
                writer.integer(object.id.value);
                writer.string(object.name);
                writer.string(object.type);
                writer.float2(object.position);
                writer.float2(object.size);
                writer.floating(object.rotation_radians);
                writer.integer(object.layer_bits);
                writer.integer(object.user_flags);
            }
        }

        [[nodiscard]] std::vector<std::byte> payload_bytes(
            const CompiledTileMapArtifact& artifact,
            std::uint64_t limit)
        {
            Writer writer{limit};
            write_payload(writer, artifact);
            return writer.good() ? writer.take() : std::vector<std::byte>{};
        }

        template<typename Value, typename Key>
        [[nodiscard]] bool strictly_ordered_unique(
            std::span<const Value> values,
            Key key) noexcept
        {
            for (std::size_t index = 1; index < values.size(); ++index)
            {
                if (!(key(values[index - 1]) < key(values[index])))
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool same_payload(
            const CompiledTileMapArtifact& left,
            const CompiledTileMapArtifact& right) noexcept
        {
            return left.identity == right.identity
                && left.name == right.name
                && left.extent_tiles == right.extent_tiles
                && left.default_chunk_extent == right.default_chunk_extent
                && left.dependencies == right.dependencies
                && left.tile_sets == right.tile_sets
                && left.palette == right.palette
                && left.layers == right.layers
                && left.chunks == right.chunks
                && left.cells == right.cells
                && left.collision == right.collision
                && left.objects == right.objects;
        }
    }

    bool valid_texture_dependency_path(std::string_view path) noexcept
    {
        if (path.empty() || path.front() == '/' || path.front() == '\\'
            || path.find('\\') != std::string_view::npos
            || path.find(':') != std::string_view::npos)
        {
            return false;
        }
        std::size_t begin{};
        while (begin < path.size())
        {
            const std::size_t end = path.find('/', begin);
            const std::string_view segment = path.substr(
                begin,
                end == std::string_view::npos ? path.size() - begin : end - begin);
            if (segment.empty() || segment == "." || segment == "..")
                return false;
            for (const unsigned char character : segment)
            {
                if (character < 0x20u || character == 0x7fu)
                    return false;
            }
            if (end == std::string_view::npos)
                break;
            begin = end + 1u;
        }
        return true;
    }

    ContentHash hash_bytes(std::span<const std::byte> bytes) noexcept
    {
        ContentHash result{{
            14695981039346656037ull,
            1099511628211ull ^ 0x9e3779b97f4a7c15ull,
            0x6a09e667f3bcc909ull,
            0xbb67ae8584caa73bull}};
        constexpr std::array<std::uint64_t, 4> primes{
            1099511628211ull,
            0x100000001b3ull + 0x9e37ull,
            0x9e3779b185ebca87ull,
            0xc2b2ae3d27d4eb4full};
        std::uint64_t position{};
        for (const std::byte byte : bytes)
        {
            const std::uint64_t value = std::to_integer<std::uint8_t>(byte);
            for (std::size_t lane = 0; lane < result.words.size(); ++lane)
            {
                result.words[lane] ^= value + position + (lane * 0x9du);
                result.words[lane] *= primes[lane];
                result.words[lane] ^= result.words[lane] >> (17u + lane * 3u);
            }
            ++position;
        }
        for (std::size_t lane = 0; lane < result.words.size(); ++lane)
        {
            result.words[lane] ^= static_cast<std::uint64_t>(bytes.size())
                + lane * 0x9e3779b97f4a7c15ull;
            if (result.words[lane] == 0u)
                result.words[lane] = primes[lane];
        }
        return result;
    }

    ContentHash compiled_tilemap_payload_content(
        const CompiledTileMapArtifact& artifact) noexcept
    {
        try
        {
            const auto bytes = detail::payload_bytes(
                artifact,
                (std::numeric_limits<std::uint64_t>::max)());
            return bytes.empty() ? ContentHash{} : hash_bytes(bytes);
        }
        catch (...)
        {
            return {};
        }
    }

    ArtifactValidation validate(
        const CompiledTileMapArtifact& artifact,
        const ArtifactLimits& limits) noexcept
    {
        ArtifactValidation result{};
        if (!limits.valid())
        {
            result.code = ArtifactCode::invalid_limits;
            return result;
        }
        if (!artifact.identity)
        {
            result.code = artifact.identity.schema_version == artifact_schema_version
                ? ArtifactCode::invalid_identity
                : ArtifactCode::unsupported_schema;
            return result;
        }
        if (artifact.name.empty() || artifact.name.size() > limits.maximum_name_bytes
            || artifact.extent_tiles.x == 0u || artifact.extent_tiles.y == 0u
            || artifact.extent_tiles.x > limits.maximum_map_dimension_tiles
            || artifact.extent_tiles.y > limits.maximum_map_dimension_tiles
            || artifact.default_chunk_extent.x == 0u
            || artifact.default_chunk_extent.y == 0u
            || artifact.default_chunk_extent.x > limits.maximum_chunk_extent
            || artifact.default_chunk_extent.y > limits.maximum_chunk_extent)
        {
            result.code = ArtifactCode::invalid_map;
            return result;
        }
        if (artifact.dependencies.size() > limits.maximum_texture_dependencies
            || artifact.tile_sets.size() > limits.maximum_tile_sets
            || artifact.palette.size() > limits.maximum_palette_entries
            || artifact.layers.size() > limits.maximum_layers
            || artifact.chunks.size() > limits.maximum_chunks
            || artifact.cells.size() > limits.maximum_occupied_cells
            || artifact.collision.size() > limits.maximum_collision_primitives
            || artifact.objects.size() > limits.maximum_objects)
        {
            result.code = ArtifactCode::capacity_exceeded;
            return result;
        }

        for (std::size_t index = 0; index < artifact.dependencies.size(); ++index)
        {
            const TextureDependency& dependency = artifact.dependencies[index];
            const bool artifactKey = std::any_of(
                dependency.artifact_key.begin(),
                dependency.artifact_key.end(),
                [](std::uint64_t word) { return word != 0u; });
            if (!valid_texture_dependency_path(dependency.logical_path)
                || dependency.logical_path.size() > limits.maximum_path_bytes
                || !artifactKey || dependency.project_key == 0u
                || dependency.asset_key == 0u || dependency.artifact_revision == 0u
                || dependency.stable_material_key == 0u
                || dependency.texture_extent.x == 0u
                || dependency.texture_extent.y == 0u
                || !detail::enum_between(
                    dependency.min_filter, FilterMode::nearest, FilterMode::linear)
                || !detail::enum_between(
                    dependency.mag_filter, FilterMode::nearest, FilterMode::linear)
                || !detail::enum_between(
                    dependency.address_u,
                    AddressMode::clamp_to_edge,
                    AddressMode::mirrored_repeat)
                || !detail::enum_between(
                    dependency.address_v,
                    AddressMode::clamp_to_edge,
                    AddressMode::mirrored_repeat)
                || !detail::enum_between(
                    dependency.alpha, AlphaMode::opaque, AlphaMode::additive)
                || !detail::enum_between(
                    dependency.color_space, ColorSpace::linear, ColorSpace::srgb)
                || !std::isfinite(dependency.alpha_cutoff)
                || dependency.alpha_cutoff < 0.0f
                || dependency.alpha_cutoff > 1.0f)
            {
                result.code = ArtifactCode::invalid_dependency;
                return result;
            }
            for (std::size_t prior = 0; prior < index; ++prior)
            {
                const TextureDependency& other = artifact.dependencies[prior];
                if ((other.project_key == dependency.project_key
                        && other.asset_key == dependency.asset_key
                        && other.artifact_revision == dependency.artifact_revision)
                    || other.stable_material_key == dependency.stable_material_key)
                {
                    result.code = ArtifactCode::duplicate_identity;
                    return result;
                }
            }
        }

        if (!detail::strictly_ordered_unique(
                std::span{artifact.tile_sets},
                [](const CompiledTileSet& value) { return value.id.value; }))
        {
            result.code = ArtifactCode::noncanonical_order;
            return result;
        }
        for (const CompiledTileSet& tileSet : artifact.tile_sets)
        {
            std::uint64_t gridCapacity{};
            std::uint64_t width{};
            std::uint64_t height{};
            std::uint64_t spacingX{};
            std::uint64_t spacingY{};
            if (!tileSet.id || tileSet.dependency_index >= artifact.dependencies.size()
                || tileSet.tile_extent.x == 0u || tileSet.tile_extent.y == 0u
                || tileSet.tile_extent.x > limits.maximum_tile_extent
                || tileSet.tile_extent.y > limits.maximum_tile_extent
                || tileSet.grid.x == 0u || tileSet.grid.y == 0u
                || tileSet.tile_count == 0u
                || !detail::checked_multiply(tileSet.grid.x, tileSet.grid.y, gridCapacity)
                || tileSet.tile_count > gridCapacity
                || !detail::checked_multiply(
                    tileSet.grid.x, tileSet.tile_extent.x, width)
                || !detail::checked_multiply(
                    tileSet.grid.y, tileSet.tile_extent.y, height)
                || !detail::checked_multiply(
                    tileSet.grid.x - 1u, tileSet.spacing.x, spacingX)
                || !detail::checked_multiply(
                    tileSet.grid.y - 1u, tileSet.spacing.y, spacingY)
                || !detail::checked_add(width, spacingX, width)
                || !detail::checked_add(height, spacingY, height)
                || !detail::checked_add(
                    width, std::uint64_t{2u} * tileSet.margin.x, width)
                || !detail::checked_add(
                    height, std::uint64_t{2u} * tileSet.margin.y, height)
                || width > artifact.dependencies[tileSet.dependency_index].texture_extent.x
                || height > artifact.dependencies[tileSet.dependency_index].texture_extent.y)
            {
                result.code = ArtifactCode::invalid_tile_set;
                return result;
            }
        }

        if (!detail::strictly_ordered_unique(
                std::span{artifact.palette},
                [](const CompiledPaletteEntry& value) { return value.id.value; }))
        {
            result.code = ArtifactCode::noncanonical_order;
            return result;
        }
        for (const CompiledPaletteEntry& entry : artifact.palette)
        {
            if (!entry.id || entry.tile_set_index >= artifact.tile_sets.size()
                || entry.local_tile >= artifact.tile_sets[entry.tile_set_index].tile_count
                || !detail::valid_collision_shape(entry.collision)
                || entry.animation_frame_count == 0u
                || static_cast<std::uint32_t>(entry.animation_frame_count)
                    > artifact.tile_sets[entry.tile_set_index].tile_count
                        - entry.local_tile)
            {
                result.code = ArtifactCode::invalid_palette_entry;
                return result;
            }
        }

        if (!detail::strictly_ordered_unique(
                std::span{artifact.layers},
                [](const CompiledLayer& value) { return value.id.value; }))
        {
            result.code = ArtifactCode::noncanonical_order;
            return result;
        }
        for (const CompiledLayer& layer : artifact.layers)
        {
            if (!layer.id || layer.name.empty()
                || layer.name.size() > limits.maximum_name_bytes
                || layer.extent_tiles.x == 0u || layer.extent_tiles.y == 0u
                || layer.extent_tiles.x > artifact.extent_tiles.x
                || layer.extent_tiles.y > artifact.extent_tiles.y
                || layer.chunk_extent.x == 0u || layer.chunk_extent.y == 0u
                || layer.chunk_extent.x > limits.maximum_chunk_extent
                || layer.chunk_extent.y > limits.maximum_chunk_extent
                || !detail::finite(layer.world_origin)
                || !detail::finite(layer.tile_world_extent)
                || layer.tile_world_extent.x <= 0.0f
                || layer.tile_world_extent.y <= 0.0f
                || !detail::finite(layer.parallax)
                || !detail::enum_between(
                    layer.phase, LayerPhase::background, LayerPhase::overlay)
                || !std::isfinite(layer.opacity)
                || layer.opacity < 0.0f || layer.opacity > 1.0f)
            {
                result.code = ArtifactCode::invalid_layer;
                return result;
            }
        }

        auto chunkKey = [](const CompiledChunk& chunk)
        {
            return std::tuple{chunk.layer_index, chunk.coordinate.y, chunk.coordinate.x};
        };
        if (!detail::strictly_ordered_unique(
                std::span{artifact.chunks}, chunkKey))
        {
            result.code = ArtifactCode::noncanonical_order;
            return result;
        }
        std::uint64_t expectedFirstCell{};
        for (const CompiledChunk& chunk : artifact.chunks)
        {
            if (chunk.layer_index >= artifact.layers.size()
                || chunk.extent.x == 0u || chunk.extent.y == 0u
                || chunk.revision == 0u || chunk.world_bounds.empty()
                || !detail::finite(chunk.world_bounds)
                || chunk.first_cell != expectedFirstCell)
            {
                result.code = ArtifactCode::invalid_chunk;
                return result;
            }
            const CompiledLayer& layer = artifact.layers[chunk.layer_index];
            const std::uint64_t originX =
                std::uint64_t{chunk.coordinate.x} * layer.chunk_extent.x;
            const std::uint64_t originY =
                std::uint64_t{chunk.coordinate.y} * layer.chunk_extent.y;
            if (chunk.extent.x > layer.chunk_extent.x
                || chunk.extent.y > layer.chunk_extent.y
                || originX + chunk.extent.x > layer.extent_tiles.x
                || originY + chunk.extent.y > layer.extent_tiles.y
                || chunk.first_cell > artifact.cells.size()
                || chunk.cell_count > artifact.cells.size() - chunk.first_cell)
            {
                result.code = ArtifactCode::invalid_chunk;
                return result;
            }
            UInt2 previous{};
            bool havePrevious{};
            for (std::uint32_t offset = 0u; offset < chunk.cell_count; ++offset)
            {
                const CompiledCell& cell = artifact.cells[chunk.first_cell + offset];
                if (cell.palette_index >= artifact.palette.size()
                    || cell.coordinate.x < originX || cell.coordinate.y < originY
                    || cell.coordinate.x >= originX + chunk.extent.x
                    || cell.coordinate.y >= originY + chunk.extent.y
                    || !detail::enum_between(
                        cell.transform,
                        TileTransform::identity,
                        TileTransform::rotate_270)
                    || cell.animation_frame
                        >= artifact.palette[cell.palette_index].animation_frame_count)
                {
                    result.code = ArtifactCode::invalid_cell;
                    return result;
                }
                if (havePrevious
                    && !(std::tuple{previous.y, previous.x}
                        < std::tuple{cell.coordinate.y, cell.coordinate.x}))
                {
                    result.code = ArtifactCode::noncanonical_order;
                    return result;
                }
                previous = cell.coordinate;
                havePrevious = true;
            }
            expectedFirstCell += chunk.cell_count;
        }
        if (expectedFirstCell != artifact.cells.size())
        {
            result.code = ArtifactCode::dangling_reference;
            return result;
        }

        for (const CollisionPrimitive& primitive : artifact.collision)
        {
            if (!primitive.layer || !primitive.palette_entry
                || primitive.kind == CollisionKind::none
                || !detail::enum_between(
                    primitive.kind,
                    CollisionKind::full_cell,
                    CollisionKind::custom_box)
                || primitive.world_bounds.empty()
                || !detail::finite(primitive.world_bounds)
                || primitive.cell_coordinate.x >= artifact.extent_tiles.x
                || primitive.cell_coordinate.y >= artifact.extent_tiles.y
                || primitive.layer_bits == 0u || primitive.mask_bits == 0u)
            {
                result.code = ArtifactCode::invalid_collision;
                return result;
            }
            const auto layer = std::ranges::lower_bound(
                artifact.layers, primitive.layer.value, {},
                [](const CompiledLayer& value) { return value.id.value; });
            const auto palette = std::ranges::lower_bound(
                artifact.palette, primitive.palette_entry.value, {},
                [](const CompiledPaletteEntry& value) { return value.id.value; });
            const bool layerFound = layer != artifact.layers.end()
                && layer->id == primitive.layer;
            const bool paletteFound = palette != artifact.palette.end()
                && palette->id == primitive.palette_entry;
            if (!layerFound || !paletteFound)
            {
                result.code = ArtifactCode::dangling_reference;
                return result;
            }
        }

        if (!detail::strictly_ordered_unique(
                std::span{artifact.objects},
                [](const CompiledMapObject& value) { return value.id.value; }))
        {
            result.code = ArtifactCode::noncanonical_order;
            return result;
        }
        for (const CompiledMapObject& object : artifact.objects)
        {
            if (!object.id || object.name.empty() || object.type.empty()
                || object.name.size() > limits.maximum_name_bytes
                || object.type.size() > limits.maximum_name_bytes
                || !detail::finite(object.position) || !detail::finite(object.size)
                || object.size.x <= 0.0f || object.size.y <= 0.0f
                || !std::isfinite(object.rotation_radians)
                || object.layer_bits == 0u)
            {
                result.code = ArtifactCode::invalid_object;
                return result;
            }
        }

        const ContentHash expected = compiled_tilemap_payload_content(artifact);
        if (expected.empty() || expected != artifact.identity.key)
        {
            result.code = ArtifactCode::content_hash_mismatch;
            return result;
        }
        const auto payload = detail::payload_bytes(
            artifact, limits.maximum_serialized_bytes);
        if (payload.empty())
        {
            result.code = ArtifactCode::capacity_exceeded;
            return result;
        }
        result.serialized_bytes_estimate = payload.size()
            + detail::artifact_magic.size() + sizeof(std::uint32_t)
            + sizeof(ContentHash);
        if (result.serialized_bytes_estimate > limits.maximum_serialized_bytes)
        {
            result.code = ArtifactCode::capacity_exceeded;
            return result;
        }
        result.occupied_cells = artifact.cells.size();
        result.code = ArtifactCode::ready;
        return result;
    }

    SerializationResult serialize(
        const CompiledTileMapArtifact& artifact,
        const ArtifactLimits& limits) noexcept
    {
        try
        {
            const ArtifactValidation validation = validate(artifact, limits);
            if (!validation)
                return {validation.code, {}};
            detail::Writer writer{limits.maximum_serialized_bytes};
            writer.raw(detail::artifact_magic);
            writer.integer(artifact_schema_version);
            writer.hash(artifact.identity.key);
            detail::write_payload(writer, artifact);
            if (!writer.good())
                return {ArtifactCode::capacity_exceeded, {}};
            return {ArtifactCode::ready, writer.take()};
        }
        catch (const std::bad_alloc&)
        {
            return {ArtifactCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ArtifactCode::malformed_payload, {}};
        }
    }

    DeserializationResult deserialize(
        std::span<const std::byte> bytes,
        const ArtifactLimits& limits) noexcept
    {
        try
        {
            if (!limits.valid())
                return {ArtifactCode::invalid_limits, {}};
            if (bytes.empty() || bytes.size() > limits.maximum_serialized_bytes)
                return {ArtifactCode::capacity_exceeded, {}};
            detail::Reader reader{bytes};
            if (!std::ranges::equal(
                    reader.raw(detail::artifact_magic.size()),
                    detail::artifact_magic))
                return {ArtifactCode::malformed_payload, {}};
            const std::uint32_t envelopeSchema = reader.integer<std::uint32_t>();
            if (envelopeSchema != artifact_schema_version)
                return {ArtifactCode::unsupported_schema, {}};

            CompiledTileMapArtifact artifact{};
            artifact.identity.key = reader.hash();
            artifact.identity.schema_version = reader.integer<std::uint32_t>();
            artifact.identity.source_revision.content = reader.hash();
            artifact.identity.source_revision.sequence = reader.integer<std::uint64_t>();
            artifact.name = reader.string(limits.maximum_name_bytes);
            artifact.extent_tiles = reader.uint2();
            artifact.default_chunk_extent = reader.uint2();

            const auto readCount = [&reader](std::uint64_t maximum)
                -> std::uint32_t
            {
                const std::uint32_t count = reader.integer<std::uint32_t>();
                if (count > maximum)
                    static_cast<void>(reader.raw((std::numeric_limits<std::size_t>::max)()));
                return count;
            };

            const std::uint32_t dependencyCount = readCount(
                limits.maximum_texture_dependencies);
            artifact.dependencies.reserve(dependencyCount);
            for (std::uint32_t index = 0; index < dependencyCount && reader.good(); ++index)
            {
                TextureDependency dependency{};
                dependency.logical_path = reader.string(limits.maximum_path_bytes);
                for (std::uint64_t& word : dependency.artifact_key)
                    word = reader.integer<std::uint64_t>();
                dependency.project_key = reader.integer<std::uint64_t>();
                dependency.asset_key = reader.integer<std::uint64_t>();
                dependency.artifact_revision = reader.integer<std::uint64_t>();
                dependency.stable_material_key = reader.integer<std::uint32_t>();
                dependency.texture_extent = reader.uint2();
                dependency.min_filter = reader.enumeration<FilterMode>();
                dependency.mag_filter = reader.enumeration<FilterMode>();
                dependency.address_u = reader.enumeration<AddressMode>();
                dependency.address_v = reader.enumeration<AddressMode>();
                dependency.alpha = reader.enumeration<AlphaMode>();
                dependency.color_space = reader.enumeration<ColorSpace>();
                dependency.alpha_cutoff = reader.floating();
                artifact.dependencies.push_back(std::move(dependency));
            }

            const std::uint32_t tileSetCount = readCount(limits.maximum_tile_sets);
            artifact.tile_sets.reserve(tileSetCount);
            for (std::uint32_t index = 0; index < tileSetCount && reader.good(); ++index)
            {
                CompiledTileSet tileSet{};
                tileSet.id.value = reader.integer<std::uint64_t>();
                tileSet.dependency_index = reader.integer<std::uint32_t>();
                tileSet.tile_extent = reader.uint2();
                tileSet.grid = reader.uint2();
                tileSet.margin = reader.uint2();
                tileSet.spacing = reader.uint2();
                tileSet.tile_count = reader.integer<std::uint32_t>();
                artifact.tile_sets.push_back(tileSet);
            }

            const std::uint32_t paletteCount = readCount(limits.maximum_palette_entries);
            artifact.palette.reserve(paletteCount);
            for (std::uint32_t index = 0; index < paletteCount && reader.good(); ++index)
            {
                CompiledPaletteEntry entry{};
                entry.id.value = reader.integer<std::uint64_t>();
                entry.tile_set_index = reader.integer<std::uint32_t>();
                entry.local_tile = reader.integer<std::uint32_t>();
                entry.collision = detail::read_collision(reader);
                entry.animation_frame_count = reader.integer<std::uint16_t>();
                entry.animation_rate_millihertz = reader.integer<std::uint16_t>();
                entry.user_flags = reader.integer<std::uint32_t>();
                artifact.palette.push_back(entry);
            }

            const std::uint32_t layerCount = readCount(limits.maximum_layers);
            artifact.layers.reserve(layerCount);
            for (std::uint32_t index = 0; index < layerCount && reader.good(); ++index)
            {
                CompiledLayer layer{};
                layer.id.value = reader.integer<std::uint64_t>();
                layer.name = reader.string(limits.maximum_name_bytes);
                layer.extent_tiles = reader.uint2();
                layer.chunk_extent = reader.uint2();
                layer.world_origin = reader.float2();
                layer.tile_world_extent = reader.float2();
                layer.parallax = reader.float2();
                layer.phase = reader.enumeration<LayerPhase>();
                layer.draw_layer = reader.signed_integer<std::int32_t>();
                layer.opacity = reader.floating();
                layer.visible = reader.boolean();
                layer.collision_source = reader.boolean();
                artifact.layers.push_back(std::move(layer));
            }

            const std::uint32_t chunkCount = readCount(limits.maximum_chunks);
            artifact.chunks.reserve(chunkCount);
            for (std::uint32_t index = 0; index < chunkCount && reader.good(); ++index)
            {
                CompiledChunk chunk{};
                chunk.layer_index = reader.integer<std::uint32_t>();
                chunk.coordinate = reader.uint2();
                chunk.extent = reader.uint2();
                chunk.revision = reader.integer<std::uint64_t>();
                chunk.world_bounds = reader.rect();
                chunk.first_cell = reader.integer<std::uint32_t>();
                chunk.cell_count = reader.integer<std::uint32_t>();
                artifact.chunks.push_back(chunk);
            }

            const std::uint64_t cellCount = reader.integer<std::uint64_t>();
            if (cellCount > limits.maximum_occupied_cells
                || cellCount > (std::numeric_limits<std::size_t>::max)())
            {
                return {ArtifactCode::capacity_exceeded, {}};
            }
            artifact.cells.reserve(static_cast<std::size_t>(cellCount));
            for (std::uint64_t index = 0; index < cellCount && reader.good(); ++index)
            {
                CompiledCell cell{};
                cell.coordinate = reader.uint2();
                cell.palette_index = reader.integer<std::uint32_t>();
                cell.transform = reader.enumeration<TileTransform>();
                cell.tint.r = reader.integer<std::uint8_t>();
                cell.tint.g = reader.integer<std::uint8_t>();
                cell.tint.b = reader.integer<std::uint8_t>();
                cell.tint.a = reader.integer<std::uint8_t>();
                cell.animation_frame = reader.integer<std::uint16_t>();
                cell.flags = reader.integer<std::uint16_t>();
                artifact.cells.push_back(cell);
            }

            const std::uint64_t collisionCount = reader.integer<std::uint64_t>();
            if (collisionCount > limits.maximum_collision_primitives
                || collisionCount > (std::numeric_limits<std::size_t>::max)())
            {
                return {ArtifactCode::capacity_exceeded, {}};
            }
            artifact.collision.reserve(static_cast<std::size_t>(collisionCount));
            for (std::uint64_t index = 0; index < collisionCount && reader.good(); ++index)
            {
                CollisionPrimitive primitive{};
                primitive.layer.value = reader.integer<std::uint64_t>();
                primitive.palette_entry.value = reader.integer<std::uint64_t>();
                primitive.cell_coordinate = reader.uint2();
                primitive.kind = reader.enumeration<CollisionKind>();
                primitive.world_bounds = reader.rect();
                primitive.layer_bits = reader.integer<std::uint32_t>();
                primitive.mask_bits = reader.integer<std::uint32_t>();
                primitive.sensor = reader.boolean();
                artifact.collision.push_back(primitive);
            }

            const std::uint32_t objectCount = readCount(limits.maximum_objects);
            artifact.objects.reserve(objectCount);
            for (std::uint32_t index = 0; index < objectCount && reader.good(); ++index)
            {
                CompiledMapObject object{};
                object.id.value = reader.integer<std::uint64_t>();
                object.name = reader.string(limits.maximum_name_bytes);
                object.type = reader.string(limits.maximum_name_bytes);
                object.position = reader.float2();
                object.size = reader.float2();
                object.rotation_radians = reader.floating();
                object.layer_bits = reader.integer<std::uint32_t>();
                object.user_flags = reader.integer<std::uint32_t>();
                artifact.objects.push_back(std::move(object));
            }

            if (!reader.done())
                return {ArtifactCode::malformed_payload, {}};
            const ArtifactValidation validation = validate(artifact, limits);
            if (!validation)
                return {validation.code, {}};
            return {ArtifactCode::ready, std::move(artifact)};
        }
        catch (const std::bad_alloc&)
        {
            return {ArtifactCode::allocation_failure, {}};
        }
        catch (...)
        {
            return {ArtifactCode::malformed_payload, {}};
        }
    }

    ArtifactContractFailure run_artifact_contract() noexcept
    {
        CompiledTileMapArtifact artifact{};
        artifact.name = "ContractMap";
        artifact.extent_tiles = {8u, 8u};
        artifact.default_chunk_extent = {4u, 4u};
        artifact.identity.source_revision = {
            {{11u, 12u, 13u, 14u}}, 7u};
        artifact.dependencies.push_back({
            .logical_path = "Assets/Textures/contract.ppm",
            .artifact_key = {21u, 22u, 23u, 24u},
            .project_key = 31u,
            .asset_key = 32u,
            .artifact_revision = 33u,
            .stable_material_key = 34u,
            .texture_extent = {32u, 16u}
        });
        artifact.tile_sets.push_back({
            .id = {101u},
            .dependency_index = 0u,
            .tile_extent = {16u, 16u},
            .grid = {2u, 1u},
            .tile_count = 2u
        });
        artifact.palette.push_back({
            .id = {201u},
            .tile_set_index = 0u,
            .local_tile = 1u,
            .collision = {
                .kind = CollisionKind::full_cell,
                .local_bounds = {0.0f, 0.0f, 1.0f, 1.0f},
                .layer_bits = 1u,
                .mask_bits = 3u
            }
        });
        artifact.layers.push_back({
            .id = {301u},
            .name = "World",
            .extent_tiles = {8u, 8u},
            .chunk_extent = {4u, 4u},
            .tile_world_extent = {1.0f, 1.0f},
            .collision_source = true
        });
        artifact.chunks.push_back({
            .layer_index = 0u,
            .coordinate = {0u, 0u},
            .extent = {4u, 4u},
            .revision = 2u,
            .world_bounds = {0.0f, 0.0f, 4.0f, 4.0f},
            .first_cell = 0u,
            .cell_count = 2u
        });
        artifact.cells.push_back({
            .coordinate = {0u, 0u},
            .palette_index = 0u
        });
        artifact.cells.push_back({
            .coordinate = {1u, 0u},
            .palette_index = 0u,
            .transform = TileTransform::flip_x,
            .tint = {200u, 210u, 220u, 255u}
        });
        artifact.collision.push_back({
            .layer = {301u},
            .palette_entry = {201u},
            .cell_coordinate = {0u, 0u},
            .kind = CollisionKind::full_cell,
            .world_bounds = {0.0f, 0.0f, 1.0f, 1.0f},
            .layer_bits = 1u,
            .mask_bits = 3u
        });
        artifact.collision.push_back({
            .layer = {301u},
            .palette_entry = {201u},
            .cell_coordinate = {1u, 0u},
            .kind = CollisionKind::full_cell,
            .world_bounds = {1.0f, 0.0f, 1.0f, 1.0f},
            .layer_bits = 1u,
            .mask_bits = 3u
        });
        artifact.objects.push_back({
            .id = {401u},
            .name = "PlayerSpawn",
            .type = "spawn",
            .position = {2.0f, 2.0f}
        });
        artifact.identity.key = compiled_tilemap_payload_content(artifact);
        if (!validate(artifact))
            return ArtifactContractFailure::valid_artifact_rejected;
        if (artifact.identity.key != compiled_tilemap_payload_content(artifact))
            return ArtifactContractFailure::deterministic_hash;

        const SerializationResult encoded = serialize(artifact);
        if (!encoded)
            return ArtifactContractFailure::round_trip;
        const DeserializationResult decoded = deserialize(encoded.bytes);
        if (!decoded || !detail::same_payload(artifact, decoded.artifact))
            return ArtifactContractFailure::round_trip;

        std::vector<std::byte> malformed = encoded.bytes;
        malformed.pop_back();
        if (deserialize(malformed))
            return ArtifactContractFailure::malformed_payload_accepted;

        CompiledTileMapArtifact wrongHash = artifact;
        ++wrongHash.identity.key.words[0];
        if (validate(wrongHash))
            return ArtifactContractFailure::hash_mismatch_accepted;

        CompiledTileMapArtifact dangling = artifact;
        dangling.palette[0].tile_set_index = 7u;
        dangling.identity.key = compiled_tilemap_payload_content(dangling);
        if (validate(dangling))
            return ArtifactContractFailure::dangling_reference_accepted;

        CompiledTileMapArtifact invalidCollision = artifact;
        invalidCollision.collision[0].world_bounds.width = 0.0f;
        invalidCollision.identity.key = compiled_tilemap_payload_content(
            invalidCollision);
        if (validate(invalidCollision))
            return ArtifactContractFailure::invalid_collision_accepted;

        CompiledTileMapArtifact invalidAnimation = artifact;
        invalidAnimation.palette[0].animation_frame_count = 2u;
        invalidAnimation.identity.key = compiled_tilemap_payload_content(
            invalidAnimation);
        if (validate(invalidAnimation))
            return ArtifactContractFailure::invalid_animation_span_accepted;

        ArtifactLimits tiny{};
        tiny.maximum_serialized_bytes = encoded.bytes.size() - 1u;
        if (deserialize(encoded.bytes, tiny).code != ArtifactCode::capacity_exceeded)
            return ArtifactContractFailure::bounded_read_failed;
        return ArtifactContractFailure::none;
    }
}
