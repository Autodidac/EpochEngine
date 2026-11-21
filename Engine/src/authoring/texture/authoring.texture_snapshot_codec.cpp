// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

module authoring.texture;
import authoring.texture_detail;

namespace epochengine::authoring::texture
{
    using detail::StableHash;
    using detail::hash_canvas;
    using detail::hash_layer_descriptor;
    using detail::valid_limits;
    using detail::valid_layer_descriptor;

    namespace
    {
        constexpr std::uint64_t kMaximumSerializedDocumentBytes =
            512ull * 1024ull * 1024ull;
        constexpr std::uint32_t kTextureSourceSchemaVersion = 3u;
        constexpr std::array<std::byte, 8> kTextureSourceMagic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'T'}, std::byte{'E'},
            std::byte{'X'}, std::byte{'D'}, std::byte{'0'}, std::byte{'1'}};

        class ByteWriter final
        {
        public:
            explicit ByteWriter(std::uint64_t limit) noexcept
                : limit_(limit)
            {
            }

            [[nodiscard]] bool good() const noexcept
            {
                return good_;
            }

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
                        (value >> (byte * 8u))
                        & static_cast<Value>(0xffu)));
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

            void boolean(bool value)
            {
                integer<std::uint8_t>(value ? 1u : 0u);
            }

            void string(std::string_view value)
            {
                if (value.size()
                    > (std::numeric_limits<std::uint32_t>::max)())
                {
                    good_ = false;
                    return;
                }
                integer(static_cast<std::uint32_t>(value.size()));
                raw(std::as_bytes(std::span{value.data(), value.size()}));
            }

            void hash(ContentHash value)
            {
                for (const std::uint64_t word : value.words)
                    integer(word);
            }

        private:
            [[nodiscard]] bool reserve(std::size_t additional)
            {
                if (!good_
                    || additional > limit_
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

        class ByteReader final
        {
        public:
            explicit ByteReader(
                std::span<const std::byte> bytes) noexcept
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

            void fail() noexcept
            {
                good_ = false;
            }

            [[nodiscard]] std::span<const std::byte> raw(
                std::size_t count)
            {
                if (!good_ || count > bytes_.size() - offset_)
                {
                    good_ = false;
                    return {};
                }
                const auto value = bytes_.subspan(offset_, count);
                offset_ += count;
                return value;
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
                return static_cast<Value>(
                    integer<std::make_unsigned_t<Value>>());
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

            [[nodiscard]] std::string string(
                std::uint32_t maximumBytes)
            {
                const std::uint32_t size = integer<std::uint32_t>();
                if (!good_ || size > maximumBytes)
                {
                    good_ = false;
                    return {};
                }
                const auto bytes = raw(size);
                if (!good_)
                    return {};
                return std::string{
                    reinterpret_cast<const char*>(bytes.data()),
                    bytes.size()};
            }

            [[nodiscard]] ContentHash hash()
            {
                ContentHash value{};
                for (std::uint64_t& word : value.words)
                    word = integer<std::uint64_t>();
                return value;
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t offset_{};
            bool good_{true};
        };

        void hash_layer_descriptor_v1(
            StableHash& hash,
            const LayerDescriptor& descriptor) noexcept
        {
            hash.add_string(descriptor.name);
            hash.add_integral(descriptor.blend);
            hash.add_integral(descriptor.opacity);
            hash.add_integral(descriptor.visible);
            hash.add_integral(descriptor.locked);
        }

        void hash_layer_descriptor_v2(
            StableHash& hash,
            const LayerDescriptor& descriptor) noexcept
        {
            hash.add_string(descriptor.name);
            hash.add_integral(descriptor.blend);
            hash.add_integral(descriptor.opacity);
            hash.add_integral(descriptor.transform.offset_x_pixels);
            hash.add_integral(descriptor.transform.offset_y_pixels);
            hash.add_integral(descriptor.transform.mirror_x);
            hash.add_integral(descriptor.transform.mirror_y);
            hash.add_integral(descriptor.filter.brightness);
            hash.add_integral(descriptor.filter.grayscale);
            hash.add_integral(descriptor.filter.invert);
            hash.add_integral(descriptor.visible);
            hash.add_integral(descriptor.locked);
        }

        [[nodiscard]] ContentHash snapshot_content_hash(
            const DocumentSnapshot& snapshot,
            std::uint32_t sourceSchemaVersion) noexcept
        {
            const bool legacyV1 = sourceSchemaVersion == 1u;
            const bool legacyV2 = sourceSchemaVersion == 2u;
            StableHash documentHash{};
            documentHash.add_string(legacyV1
                ? "epoch.texture.document.v1"
                : legacyV2
                    ? "epoch.texture.document.v2"
                    : "epoch.texture.document.v3");
            hash_canvas(documentHash, snapshot.descriptor);
            documentHash.add_integral(snapshot.layers.size());
            for (std::uint32_t orderIndex = 0u;
                 orderIndex < snapshot.layers.size();
                 ++orderIndex)
            {
                const LayerSnapshot& layer = snapshot.layers[orderIndex];
                StableHash layerHash{};
                layerHash.add_string(legacyV1
                    ? "epoch.texture.layer.v1"
                    : legacyV2
                        ? "epoch.texture.layer.v2"
                        : "epoch.texture.layer.v3");
                if (legacyV1)
                    hash_layer_descriptor_v1(layerHash, layer.descriptor);
                else if (legacyV2)
                    hash_layer_descriptor_v2(layerHash, layer.descriptor);
                else
                    hash_layer_descriptor(layerHash, layer.descriptor);
                layerHash.add_integral(layer.tiles.size());
                for (const TileSnapshot& tile : layer.tiles)
                {
                    layerHash.add_integral(tile.coordinate.mip);
                    layerHash.add_integral(tile.coordinate.x);
                    layerHash.add_integral(tile.coordinate.y);
                    layerHash.add_integral(tile.width);
                    layerHash.add_integral(tile.height);
                    layerHash.add_bytes(
                        tile.texels.data(),
                        tile.texels.size());
                }
                documentHash.add_integral(orderIndex);
                documentHash.add_integral(layer.handle.index);
                documentHash.add_integral(layer.handle.generation);
                documentHash.add_hash(layerHash.finish());
            }
            return documentHash.finish();
        }
    }

    RestoreResult TextureDocument::deserialize(
        std::span<const std::byte> bytes,
        DocumentLimits limits) noexcept
    {
        try
        {
            if (!valid_limits(limits)
                || bytes.empty()
                || bytes.size() > kMaximumSerializedDocumentBytes)
            {
                return {
                    ResultCode::serialized_budget_exceeded,
                    {}
                };
            }

            ByteReader reader{ bytes };
            if (!std::ranges::equal(
                    reader.raw(kTextureSourceMagic.size()),
                    kTextureSourceMagic))
            {
                return { ResultCode::malformed_payload, {} };
            }
            const std::uint32_t sourceSchemaVersion =
                reader.integer<std::uint32_t>();
            if (sourceSchemaVersion != 1u
                && sourceSchemaVersion != 2u
                && sourceSchemaVersion != kTextureSourceSchemaVersion)
            {
                return { ResultCode::unsupported_schema, {} };
            }

            DocumentSnapshot snapshot{};
            snapshot.revision.content = reader.hash();
            snapshot.revision.sequence = reader.integer<std::uint64_t>();
            snapshot.handle.index = reader.integer<std::uint32_t>();
            snapshot.handle.generation = reader.integer<std::uint32_t>();
            snapshot.branch.timeline = reader.integer<std::uint64_t>();
            snapshot.branch.branch = reader.integer<std::uint64_t>();
            snapshot.current_time.stream = snapshot.branch;
            snapshot.current_time.tick =
                reader.signed_integer<std::int64_t>();

            snapshot.descriptor.width = reader.integer<std::uint32_t>();
            snapshot.descriptor.height = reader.integer<std::uint32_t>();
            snapshot.descriptor.tile_extent =
                reader.integer<std::uint16_t>();
            snapshot.descriptor.mip_count =
                reader.integer<std::uint8_t>();
            snapshot.descriptor.format =
                reader.enumeration<PixelFormat>();
            snapshot.descriptor.color_space =
                reader.enumeration<ColorSpace>();
            snapshot.descriptor.schema_version =
                reader.integer<std::uint32_t>();

            snapshot.history.mode =
                reader.enumeration<HistoryMode>();
            snapshot.history.checkpoint_interval_operations =
                reader.integer<std::uint32_t>();
            snapshot.history.maximum_operations =
                reader.integer<std::uint32_t>();
            snapshot.history.maximum_checkpoints =
                reader.integer<std::uint32_t>();
            snapshot.history.maximum_operation_bytes =
                reader.integer<std::uint64_t>();
            snapshot.history.maximum_checkpoint_bytes =
                reader.integer<std::uint64_t>();
            snapshot.history.allow_branching = reader.boolean();

            const std::uint32_t layerCount =
                reader.integer<std::uint32_t>();
            if (!reader.good() || layerCount > limits.maximum_layers)
                return { ResultCode::invalid_descriptor, {} };
            snapshot.layers.reserve(layerCount);
            std::uint64_t totalTiles{};
            std::uint64_t totalBytes{};
            for (std::uint32_t layerIndex = 0u;
                 layerIndex < layerCount && reader.good();
                 ++layerIndex)
            {
                LayerSnapshot layer{};
                layer.handle.index = reader.integer<std::uint32_t>();
                layer.handle.generation =
                    reader.integer<std::uint32_t>();
                layer.descriptor.name =
                    reader.string(limits.maximum_layer_name_bytes);
                layer.descriptor.blend =
                    reader.enumeration<BlendMode>();
                layer.descriptor.opacity =
                    reader.integer<std::uint16_t>();
                layer.descriptor.visible = reader.boolean();
                layer.descriptor.locked = reader.boolean();

                const std::uint64_t layerTileCount =
                    reader.integer<std::uint64_t>();
                if (!reader.good()
                    || layerTileCount
                        > limits.maximum_sparse_tiles - (std::min)(
                            totalTiles,
                            limits.maximum_sparse_tiles)
                    || layerTileCount
                        > static_cast<std::uint64_t>(
                            (std::numeric_limits<std::size_t>::max)()))
                {
                    return { ResultCode::tile_limit_exceeded, {} };
                }
                layer.tiles.reserve(
                    static_cast<std::size_t>(layerTileCount));
                for (std::uint64_t tileIndex = 0u;
                     tileIndex < layerTileCount && reader.good();
                     ++tileIndex)
                {
                    TileSnapshot tile{};
                    tile.coordinate.mip =
                        reader.integer<std::uint8_t>();
                    tile.coordinate.x =
                        reader.integer<std::uint32_t>();
                    tile.coordinate.y =
                        reader.integer<std::uint32_t>();
                    tile.width = reader.integer<std::uint16_t>();
                    tile.height = reader.integer<std::uint16_t>();
                    tile.content = reader.hash();
                    const std::uint64_t byteCount =
                        reader.integer<std::uint64_t>();
                    if (!reader.good()
                        || byteCount
                            > limits.maximum_canonical_tile_bytes
                        || totalBytes
                            > limits.maximum_canonical_tile_bytes
                                - byteCount
                        || byteCount
                            > static_cast<std::uint64_t>(
                                (std::numeric_limits<std::size_t>::max)()))
                    {
                        return {
                            ResultCode::
                                canonical_memory_budget_exceeded,
                            {}
                        };
                    }
                    const auto texels = reader.raw(
                        static_cast<std::size_t>(byteCount));
                    if (!reader.good())
                        return { ResultCode::malformed_payload, {} };
                    tile.texels.assign(texels.begin(), texels.end());
                    totalBytes += byteCount;
                    layer.tiles.push_back(std::move(tile));
                }
                totalTiles += layerTileCount;
                snapshot.layers.push_back(std::move(layer));
            }
            if (sourceSchemaVersion >= 2u)
            {
                const std::uint32_t descriptorExtensionCount =
                    reader.integer<std::uint32_t>();
                if (!reader.good()
                    || descriptorExtensionCount != snapshot.layers.size())
                {
                    return { ResultCode::invalid_descriptor, {} };
                }
                for (std::uint32_t layerIndex = 0u;
                     layerIndex < descriptorExtensionCount;
                     ++layerIndex)
                {
                    const LayerHandle extensionHandle{
                        .index = reader.integer<std::uint32_t>(),
                        .generation = reader.integer<std::uint32_t>()
                    };
                    LayerDescriptor& descriptor =
                        snapshot.layers[layerIndex].descriptor;
                    descriptor.transform.offset_x_pixels =
                        reader.signed_integer<std::int32_t>();
                    descriptor.transform.offset_y_pixels =
                        reader.signed_integer<std::int32_t>();
                    descriptor.transform.mirror_x = reader.boolean();
                    descriptor.transform.mirror_y = reader.boolean();
                    descriptor.filter.brightness =
                        reader.signed_integer<std::int16_t>();
                    descriptor.filter.grayscale = reader.boolean();
                    descriptor.filter.invert = reader.boolean();
                    if (!reader.good())
                        return { ResultCode::malformed_payload, {} };
                    if (extensionHandle
                            != snapshot.layers[layerIndex].handle
                        || !valid_layer_descriptor(descriptor, limits))
                    {
                        return { ResultCode::invalid_descriptor, {} };
                    }
                }
            }
            if (sourceSchemaVersion >= 3u)
            {
                const std::uint32_t maskExtensionCount =
                    reader.integer<std::uint32_t>();
                if (!reader.good()
                    || maskExtensionCount != snapshot.layers.size())
                {
                    return { ResultCode::invalid_descriptor, {} };
                }
                for (std::uint32_t layerIndex = 0u;
                     layerIndex < maskExtensionCount;
                     ++layerIndex)
                {
                    const LayerHandle extensionHandle{
                        .index = reader.integer<std::uint32_t>(),
                        .generation = reader.integer<std::uint32_t>()
                    };
                    LayerDescriptor& descriptor =
                        snapshot.layers[layerIndex].descriptor;
                    descriptor.role = reader.enumeration<LayerRole>();
                    descriptor.mask.source.index =
                        reader.integer<std::uint32_t>();
                    descriptor.mask.source.generation =
                        reader.integer<std::uint32_t>();
                    descriptor.mask.strength =
                        reader.integer<std::uint16_t>();
                    descriptor.mask.invert = reader.boolean();
                    if (!reader.good())
                        return { ResultCode::malformed_payload, {} };
                    if (extensionHandle
                            != snapshot.layers[layerIndex].handle
                        || !valid_layer_descriptor(descriptor, limits))
                    {
                        return { ResultCode::invalid_descriptor, {} };
                    }
                }
            }
            if (!reader.done())
                return { ResultCode::malformed_payload, {} };
            if (sourceSchemaVersion < kTextureSourceSchemaVersion)
            {
                const ContentHash legacyContent =
                    snapshot_content_hash(
                        snapshot,
                        sourceSchemaVersion);
                if (legacyContent.empty()
                    || legacyContent != snapshot.revision.content)
                {
                    return { ResultCode::integrity_failure, {} };
                }
                snapshot.revision.content = snapshot_content_hash(
                    snapshot,
                    kTextureSourceSchemaVersion);
            }
            return restore(snapshot, limits);
        }
        catch (const std::bad_alloc&)
        {
            return { ResultCode::allocation_failure, {} };
        }
        catch (...)
        {
            return { ResultCode::malformed_payload, {} };
        }
    }

    SnapshotSerializationResult TextureDocument::serialize(
        std::uint64_t maximumBytes) const noexcept
    {
        try
        {
            if (!valid())
                return { ResultCode::invalid_document, {} };
            if (maximumBytes == 0u
                || maximumBytes > kMaximumSerializedDocumentBytes)
            {
                return {
                    ResultCode::serialized_budget_exceeded,
                    {}
                };
            }

            const DocumentSnapshot state = snapshot();
            ByteWriter writer{ maximumBytes };
            writer.raw(kTextureSourceMagic);
            writer.integer(kTextureSourceSchemaVersion);
            writer.hash(state.revision.content);
            writer.integer(state.revision.sequence);
            writer.integer(state.handle.index);
            writer.integer(state.handle.generation);
            writer.integer(state.branch.timeline);
            writer.integer(state.branch.branch);
            writer.integer(state.current_time.tick);

            writer.integer(state.descriptor.width);
            writer.integer(state.descriptor.height);
            writer.integer(state.descriptor.tile_extent);
            writer.integer(state.descriptor.mip_count);
            writer.enumeration(state.descriptor.format);
            writer.enumeration(state.descriptor.color_space);
            writer.integer(state.descriptor.schema_version);

            writer.enumeration(state.history.mode);
            writer.integer(
                state.history.checkpoint_interval_operations);
            writer.integer(state.history.maximum_operations);
            writer.integer(state.history.maximum_checkpoints);
            writer.integer(state.history.maximum_operation_bytes);
            writer.integer(state.history.maximum_checkpoint_bytes);
            writer.boolean(state.history.allow_branching);

            if (state.layers.size()
                > (std::numeric_limits<std::uint32_t>::max)())
            {
                return {
                    ResultCode::serialized_budget_exceeded,
                    {}
                };
            }
            writer.integer(
                static_cast<std::uint32_t>(state.layers.size()));
            for (const LayerSnapshot& layer : state.layers)
            {
                writer.integer(layer.handle.index);
                writer.integer(layer.handle.generation);
                writer.string(layer.descriptor.name);
                writer.enumeration(layer.descriptor.blend);
                writer.integer(layer.descriptor.opacity);
                writer.boolean(layer.descriptor.visible);
                writer.boolean(layer.descriptor.locked);
                writer.integer(
                    static_cast<std::uint64_t>(layer.tiles.size()));
                for (const TileSnapshot& tile : layer.tiles)
                {
                    writer.integer(tile.coordinate.mip);
                    writer.integer(tile.coordinate.x);
                    writer.integer(tile.coordinate.y);
                    writer.integer(tile.width);
                    writer.integer(tile.height);
                    writer.hash(tile.content);
                    writer.integer(
                        static_cast<std::uint64_t>(
                            tile.texels.size()));
                    writer.raw(tile.texels);
                }
            }
            writer.integer(
                static_cast<std::uint32_t>(state.layers.size()));
            for (const LayerSnapshot& layer : state.layers)
            {
                writer.integer(layer.handle.index);
                writer.integer(layer.handle.generation);
                writer.integer(
                    layer.descriptor.transform.offset_x_pixels);
                writer.integer(
                    layer.descriptor.transform.offset_y_pixels);
                writer.boolean(layer.descriptor.transform.mirror_x);
                writer.boolean(layer.descriptor.transform.mirror_y);
                writer.integer(layer.descriptor.filter.brightness);
                writer.boolean(layer.descriptor.filter.grayscale);
                writer.boolean(layer.descriptor.filter.invert);
            }
            writer.integer(
                static_cast<std::uint32_t>(state.layers.size()));
            for (const LayerSnapshot& layer : state.layers)
            {
                writer.integer(layer.handle.index);
                writer.integer(layer.handle.generation);
                writer.enumeration(layer.descriptor.role);
                writer.integer(layer.descriptor.mask.source.index);
                writer.integer(layer.descriptor.mask.source.generation);
                writer.integer(layer.descriptor.mask.strength);
                writer.boolean(layer.descriptor.mask.invert);
            }
            if (!writer.good())
            {
                return {
                    ResultCode::serialized_budget_exceeded,
                    {}
                };
            }
            return {
                ResultCode::success,
                writer.take()
            };
        }
        catch (const std::bad_alloc&)
        {
            return { ResultCode::allocation_failure, {} };
        }
        catch (...)
        {
            return { ResultCode::malformed_payload, {} };
        }
    }
}
