/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

module authoring.texture;

namespace epochengine::authoring::texture
{
    namespace
    {
        constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ull;
        constexpr std::uint64_t kSubpixelsPerPixel = 256;
        constexpr std::uint64_t kChannelMaximum = 65'535;

        class StableHash final
        {
        public:
            StableHash() noexcept
                : words_{
                    0xcbf29ce484222325ull,
                    0x9e3779b97f4a7c15ull,
                    0x6a09e667f3bcc909ull,
                    0xbb67ae8584caa73bull
                }
            {
            }

            void add_byte(std::uint8_t value) noexcept
            {
                words_[0] ^= value;
                words_[0] *= kFnvPrime;

                words_[1] ^= static_cast<std::uint64_t>(value)
                    + 0x9e3779b97f4a7c15ull
                    + (words_[1] << 6)
                    + (words_[1] >> 2);
                words_[1] = std::rotl(words_[1], 17) *
                    0xbf58476d1ce4e5b9ull;

                words_[2] += static_cast<std::uint64_t>(value)
                    + (words_[0] ^ std::rotl(words_[1], 11));
                words_[2] ^= words_[2] >> 29;
                words_[2] *= 0x94d049bb133111ebull;

                words_[3] ^= static_cast<std::uint64_t>(value)
                    + std::rotl(words_[0], 7)
                    + std::rotl(words_[2], 31);
                words_[3] *= 0x9e3779b185ebca87ull;
            }

            template <typename Value>
            void add_integral(Value value) noexcept
            {
                static_assert(
                    std::is_integral_v<Value> || std::is_enum_v<Value>);
                using Raw = typename std::conditional_t<
                    std::is_enum_v<Value>,
                    std::underlying_type<Value>,
                    std::type_identity<Value>>::type;
                if constexpr (std::is_same_v<std::remove_cv_t<Raw>, bool>)
                {
                    add_byte(value ? 1u : 0u);
                }
                else
                {
                    using Unsigned = std::make_unsigned_t<Raw>;
                    Unsigned bits = static_cast<Unsigned>(value);
                    for (std::size_t index = 0;
                         index < sizeof(Unsigned);
                         ++index)
                    {
                        add_byte(static_cast<std::uint8_t>(
                            bits & static_cast<Unsigned>(0xff)));
                        bits >>= 8;
                    }
                }
            }

            void add_bytes(const std::byte* bytes, std::size_t size) noexcept
            {
                add_integral(size);
                for (std::size_t index = 0; index < size; ++index)
                {
                    add_byte(std::to_integer<std::uint8_t>(bytes[index]));
                }
            }

            void add_string(std::string_view value) noexcept
            {
                add_integral(value.size());
                for (const char character : value)
                {
                    add_byte(static_cast<std::uint8_t>(character));
                }
            }

            void add_hash(const ContentHash& hash) noexcept
            {
                for (const std::uint64_t word : hash.words)
                {
                    add_integral(word);
                }
            }

            [[nodiscard]] ContentHash finish() const noexcept
            {
                ContentHash result{ words_ };
                for (std::size_t index = 0; index < result.words.size(); ++index)
                {
                    std::uint64_t value = result.words[index]
                        ^ std::rotl(
                            result.words[(index + 1) % result.words.size()],
                            static_cast<int>(13 + index * 7));
                    value ^= value >> 30;
                    value *= 0xbf58476d1ce4e5b9ull;
                    value ^= value >> 27;
                    value *= 0x94d049bb133111ebull;
                    value ^= value >> 31;
                    result.words[index] = value;
                }
                if (result.empty())
                {
                    result.words[0] = 1;
                }
                return result;
            }

        private:
            std::array<std::uint64_t, 4> words_{};
        };

        struct TileCoordinateLess final
        {
            [[nodiscard]] bool operator()(
                const TileCoordinate& left,
                const TileCoordinate& right) const noexcept
            {
                if (left.mip != right.mip)
                    return left.mip < right.mip;
                if (left.y != right.y)
                    return left.y < right.y;
                return left.x < right.x;
            }
        };

        struct TileData final
        {
            std::uint16_t width{};
            std::uint16_t height{};
            std::vector<std::byte> texels{};

            [[nodiscard]] friend bool operator==(
                const TileData&,
                const TileData&) noexcept = default;
        };

        struct LayerSlot final
        {
            std::uint32_t generation{ 1 };
            bool active{};
            LayerDescriptor descriptor{};
            std::map<TileCoordinate, TileData, TileCoordinateLess> tiles{};
        };

        struct DocumentState final
        {
            std::vector<LayerSlot> slots{};
            std::vector<std::uint32_t> order{};
        };

        struct SlotDelta final
        {
            std::uint32_t slot{};
            LayerSlot before{};
            LayerSlot after{};
            std::uint32_t orderIndex{};
        };

        struct MoveDelta final
        {
            std::uint32_t slot{};
            std::uint32_t before{};
            std::uint32_t after{};
        };

        struct PropertiesDelta final
        {
            std::uint32_t slot{};
            LayerDescriptor before{};
            LayerDescriptor after{};
        };

        struct TileDelta final
        {
            TileCoordinate coordinate{};
            std::optional<TileData> before{};
            std::optional<TileData> after{};
        };

        struct StrokeDelta final
        {
            std::uint32_t slot{};
            std::vector<TileDelta> tiles{};
        };

        using InternalDelta = std::variant<
            SlotDelta,
            MoveDelta,
            PropertiesDelta,
            StrokeDelta>;

        struct OperationEntry final
        {
            OperationRecord record{};
            InternalDelta delta{};
        };

        struct CheckpointEntry final
        {
            CheckpointInfo info{};
            DocumentState state{};
        };

        [[nodiscard]] constexpr std::uint32_t mip_dimension(
            std::uint32_t base,
            std::uint8_t mip) noexcept
        {
            return mip >= 32
                ? 1u
                : (std::max)(1u, base >> mip);
        }

        [[nodiscard]] constexpr bool checked_add(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& result) noexcept
        {
            if (right > (std::numeric_limits<std::uint64_t>::max)() - left)
                return false;
            result = left + right;
            return true;
        }

        [[nodiscard]] constexpr bool checked_multiply(
            std::uint64_t left,
            std::uint64_t right,
            std::uint64_t& result) noexcept
        {
            if (left != 0 &&
                right > (std::numeric_limits<std::uint64_t>::max)() / left)
            {
                return false;
            }
            result = left * right;
            return true;
        }

        [[nodiscard]] constexpr std::uint16_t tile_width(
            const CanvasDescriptor& descriptor,
            const TileCoordinate coordinate) noexcept
        {
            const std::uint32_t width =
                mip_dimension(descriptor.width, coordinate.mip);
            const std::uint64_t start =
                static_cast<std::uint64_t>(coordinate.x)
                * descriptor.tile_extent;
            if (start >= width)
                return 0;
            return static_cast<std::uint16_t>((std::min)(
                static_cast<std::uint64_t>(descriptor.tile_extent),
                static_cast<std::uint64_t>(width) - start));
        }

        [[nodiscard]] constexpr std::uint16_t tile_height(
            const CanvasDescriptor& descriptor,
            const TileCoordinate coordinate) noexcept
        {
            const std::uint32_t height =
                mip_dimension(descriptor.height, coordinate.mip);
            const std::uint64_t start =
                static_cast<std::uint64_t>(coordinate.y)
                * descriptor.tile_extent;
            if (start >= height)
                return 0;
            return static_cast<std::uint16_t>((std::min)(
                static_cast<std::uint64_t>(descriptor.tile_extent),
                static_cast<std::uint64_t>(height) - start));
        }

        [[nodiscard]] constexpr bool valid_coordinate(
            const CanvasDescriptor& descriptor,
            const TileCoordinate coordinate) noexcept
        {
            return coordinate.mip < descriptor.mip_count
                && tile_width(descriptor, coordinate) != 0
                && tile_height(descriptor, coordinate) != 0;
        }

        [[nodiscard]] bool all_clear(const TileData& tile) noexcept
        {
            return std::ranges::all_of(
                tile.texels,
                [](std::byte value)
                {
                    return value == std::byte{};
                });
        }

        [[nodiscard]] std::uint64_t tile_bytes(const LayerSlot& slot) noexcept
        {
            std::uint64_t result{};
            for (const auto& [coordinate, tile] : slot.tiles)
            {
                (void)coordinate;
                result += tile.texels.size();
            }
            return result;
        }

        [[nodiscard]] std::uint64_t state_tile_count(
            const DocumentState& state) noexcept
        {
            std::uint64_t result{};
            for (const LayerSlot& slot : state.slots)
            {
                if (slot.active)
                    result += slot.tiles.size();
            }
            return result;
        }

        [[nodiscard]] std::uint64_t state_tile_bytes(
            const DocumentState& state) noexcept
        {
            std::uint64_t result{};
            for (const LayerSlot& slot : state.slots)
            {
                if (slot.active)
                    result += tile_bytes(slot);
            }
            return result;
        }

        void hash_canvas(
            StableHash& hash,
            const CanvasDescriptor& descriptor) noexcept
        {
            hash.add_integral(descriptor.width);
            hash.add_integral(descriptor.height);
            hash.add_integral(descriptor.tile_extent);
            hash.add_integral(descriptor.mip_count);
            hash.add_integral(descriptor.format);
            hash.add_integral(descriptor.color_space);
            hash.add_integral(descriptor.schema_version);
        }

        void hash_layer_descriptor(
            StableHash& hash,
            const LayerDescriptor& descriptor) noexcept
        {
            hash.add_string(descriptor.name);
            hash.add_integral(descriptor.blend);
            hash.add_integral(descriptor.opacity);
            hash.add_integral(descriptor.visible);
            hash.add_integral(descriptor.locked);
        }

        void hash_tile(
            StableHash& hash,
            const TileCoordinate coordinate,
            const TileData& tile) noexcept
        {
            hash.add_integral(coordinate.mip);
            hash.add_integral(coordinate.x);
            hash.add_integral(coordinate.y);
            hash.add_integral(tile.width);
            hash.add_integral(tile.height);
            hash.add_bytes(tile.texels.data(), tile.texels.size());
        }

        [[nodiscard]] ContentHash layer_content_hash(
            const LayerSlot& slot) noexcept
        {
            StableHash hash{};
            hash.add_string("epoch.texture.layer.v1");
            hash_layer_descriptor(hash, slot.descriptor);
            hash.add_integral(slot.tiles.size());
            for (const auto& [coordinate, tile] : slot.tiles)
            {
                hash_tile(hash, coordinate, tile);
            }
            return hash.finish();
        }

        [[nodiscard]] ContentHash tile_content_hash(
            const TileCoordinate coordinate,
            std::uint16_t width,
            std::uint16_t height,
            const std::vector<std::byte>& texels) noexcept
        {
            StableHash hash{};
            hash.add_string("epoch.texture.tile.v1");
            hash.add_integral(coordinate.mip);
            hash.add_integral(coordinate.x);
            hash.add_integral(coordinate.y);
            hash.add_integral(width);
            hash.add_integral(height);
            hash.add_bytes(texels.data(), texels.size());
            return hash.finish();
        }

        [[nodiscard]] ContentHash document_content_hash(
            const CanvasDescriptor& descriptor,
            const DocumentState& state) noexcept
        {
            StableHash hash{};
            hash.add_string("epoch.texture.document.v1");
            hash_canvas(hash, descriptor);
            hash.add_integral(state.order.size());
            for (std::uint32_t orderIndex = 0;
                 orderIndex < state.order.size();
                 ++orderIndex)
            {
                const std::uint32_t slotIndex = state.order[orderIndex];
                if (slotIndex >= state.slots.size())
                    continue;
                const LayerSlot& slot = state.slots[slotIndex];
                if (!slot.active)
                    continue;
                hash.add_integral(orderIndex);
                hash.add_integral(slotIndex);
                hash.add_integral(slot.generation);
                hash.add_hash(layer_content_hash(slot));
            }
            return hash.finish();
        }

        [[nodiscard]] bool valid_limits(const DocumentLimits& limits) noexcept
        {
            return limits.maximum_dimension != 0
                && limits.maximum_tile_extent != 0
                && limits.maximum_mip_count != 0
                && limits.maximum_layers != 0
                && limits.maximum_sparse_tiles != 0
                && limits.maximum_canonical_tile_bytes >= 4
                && limits.maximum_stroke_samples != 0
                && limits.maximum_tiles_per_stroke != 0
                && limits.maximum_layer_name_bytes != 0
                && limits.maximum_brush_radius_subpixels != 0;
        }

        [[nodiscard]] bool valid_history(
            const HistoryPolicy& history) noexcept
        {
            if (history.mode == HistoryMode::disabled)
                return true;

            if (history.maximum_operations == 0
                || history.maximum_operation_bytes == 0)
            {
                return false;
            }

            if (history.mode == HistoryMode::checkpointed_operations)
            {
                return history.checkpoint_interval_operations != 0
                    && history.maximum_checkpoints != 0
                    && history.maximum_checkpoint_bytes != 0;
            }
            return true;
        }

        [[nodiscard]] bool valid_canvas(
            const CanvasDescriptor& descriptor,
            const DocumentLimits& limits) noexcept
        {
            if (descriptor.width == 0
                || descriptor.height == 0
                || descriptor.width > limits.maximum_dimension
                || descriptor.height > limits.maximum_dimension
                || descriptor.tile_extent == 0
                || descriptor.tile_extent > limits.maximum_tile_extent
                || descriptor.mip_count == 0
                || descriptor.mip_count > limits.maximum_mip_count
                || descriptor.mip_count > 32
                || descriptor.schema_version == 0)
            {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool valid_layer_descriptor(
            const LayerDescriptor& descriptor,
            const DocumentLimits& limits) noexcept
        {
            return !descriptor.name.empty()
                && descriptor.name.size() <= limits.maximum_layer_name_bytes;
        }

        [[nodiscard]] constexpr std::uint32_t next_generation(
            std::uint32_t generation) noexcept
        {
            ++generation;
            if (generation == 0)
                generation = 1;
            return generation;
        }

        [[nodiscard]] std::uint64_t integer_square_root(
            std::uint64_t value) noexcept
        {
            std::uint64_t result{};
            std::uint64_t bit = std::uint64_t{ 1 } << 62;
            while (bit > value)
                bit >>= 2;
            while (bit != 0)
            {
                if (value >= result + bit)
                {
                    value -= result + bit;
                    result = (result >> 1) + bit;
                }
                else
                {
                    result >>= 1;
                }
                bit >>= 2;
            }
            return result;
        }

        [[nodiscard]] std::uint32_t multiply_unit(
            std::uint32_t left,
            std::uint32_t right) noexcept
        {
            return static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(left) * right
                    + kChannelMaximum / 2)
                / kChannelMaximum);
        }

        [[nodiscard]] std::uint8_t blend_channel(
            std::uint8_t before,
            std::uint8_t source,
            std::uint32_t amount) noexcept
        {
            const std::uint64_t inverse = kChannelMaximum - amount;
            const std::uint64_t blended =
                static_cast<std::uint64_t>(before) * inverse
                + static_cast<std::uint64_t>(source) * amount
                + kChannelMaximum / 2;
            return static_cast<std::uint8_t>(blended / kChannelMaximum);
        }

        [[nodiscard]] std::uint64_t estimate_slot_bytes(
            const LayerSlot& slot) noexcept
        {
            return sizeof(LayerSlot)
                + slot.descriptor.name.size()
                + tile_bytes(slot);
        }

        [[nodiscard]] std::uint64_t estimate_delta_bytes(
            const InternalDelta& delta) noexcept
        {
            return std::visit(
                [](const auto& value) -> std::uint64_t
                {
                    using Type = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, SlotDelta>)
                    {
                        return sizeof(Type)
                            + estimate_slot_bytes(value.before)
                            + estimate_slot_bytes(value.after);
                    }
                    else if constexpr (
                        std::is_same_v<Type, PropertiesDelta>)
                    {
                        return sizeof(Type)
                            + value.before.name.size()
                            + value.after.name.size();
                    }
                    else if constexpr (std::is_same_v<Type, StrokeDelta>)
                    {
                        std::uint64_t bytes = sizeof(Type)
                            + value.tiles.size() * sizeof(TileDelta);
                        for (const TileDelta& tile : value.tiles)
                        {
                            if (tile.before)
                                bytes += tile.before->texels.size();
                            if (tile.after)
                                bytes += tile.after->texels.size();
                        }
                        return bytes;
                    }
                    else
                    {
                        return sizeof(Type);
                    }
                },
                delta);
        }

        [[nodiscard]] std::uint64_t estimate_payload_bytes(
            const TextureOperationPayload& payload) noexcept
        {
            return std::visit(
                [](const auto& value) -> std::uint64_t
                {
                    using Type = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, LayerCreatedOperation>)
                    {
                        return sizeof(Type) + value.descriptor.name.size();
                    }
                    else if constexpr (
                        std::is_same_v<Type, LayerPropertiesChangedOperation>)
                    {
                        return sizeof(Type)
                            + value.before.name.size()
                            + value.after.name.size();
                    }
                    else if constexpr (
                        std::is_same_v<Type, TextureStrokeAppliedOperation>)
                    {
                        return sizeof(Type)
                            + value.stroke.samples.size()
                                * sizeof(StrokeSample)
                            + value.affected_tiles.size()
                                * sizeof(TileCoordinate);
                    }
                    else
                    {
                        return sizeof(Type);
                    }
                },
                payload);
        }

        [[nodiscard]] std::uint64_t estimate_checkpoint_bytes(
            const DocumentState& state) noexcept
        {
            std::uint64_t bytes = sizeof(DocumentState)
                + state.slots.size() * sizeof(LayerSlot)
                + state.order.size() * sizeof(std::uint32_t);
            for (const LayerSlot& slot : state.slots)
            {
                bytes += slot.descriptor.name.size();
                bytes += tile_bytes(slot);
                bytes += slot.tiles.size()
                    * (sizeof(TileCoordinate) + sizeof(TileData));
            }
            return bytes;
        }

        [[nodiscard]] const char* operation_kind(
            const TextureOperationPayload& payload) noexcept
        {
            return std::visit(
                [](const auto& value) noexcept -> const char*
                {
                    using Type = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, LayerCreatedOperation>)
                    {
                        return "layer_created";
                    }
                    else if constexpr (
                        std::is_same_v<Type, LayerRemovedOperation>)
                    {
                        return "layer_removed";
                    }
                    else if constexpr (
                        std::is_same_v<Type, LayerMovedOperation>)
                    {
                        return "layer_moved";
                    }
                    else if constexpr (
                        std::is_same_v<Type, LayerPropertiesChangedOperation>)
                    {
                        return "layer_properties_changed";
                    }
                    else
                    {
                        return "texture_stroke_applied";
                    }
                },
                payload);
        }

        [[nodiscard]] std::uint64_t format_mip_bytes(
            ArtifactFormat format,
            std::uint32_t width,
            std::uint32_t height,
            bool& overflow) noexcept
        {
            std::uint64_t result{};
            const bool compressed =
                format == ArtifactFormat::bc1_rgb
                || format == ArtifactFormat::bc3_rgba
                || format == ArtifactFormat::bc5_normal
                || format == ArtifactFormat::bc7_rgba
                || format == ArtifactFormat::astc_4x4_rgba;
            if (!compressed)
            {
                std::uint64_t pixels{};
                overflow = !checked_multiply(width, height, pixels)
                    || !checked_multiply(pixels, 4, result);
                return overflow ? 0 : result;
            }

            const std::uint64_t blocksWide = (width + 3ull) / 4ull;
            const std::uint64_t blocksHigh = (height + 3ull) / 4ull;
            const std::uint64_t blockBytes =
                format == ArtifactFormat::bc1_rgb ? 8ull : 16ull;
            std::uint64_t blocks{};
            overflow = !checked_multiply(blocksWide, blocksHigh, blocks)
                || !checked_multiply(blocks, blockBytes, result);
            return overflow ? 0 : result;
        }

        [[nodiscard]] std::uint8_t full_mip_count(
            std::uint32_t width,
            std::uint32_t height) noexcept
        {
            std::uint8_t result{ 1 };
            while (width > 1 || height > 1)
            {
                width = (std::max)(1u, width / 2);
                height = (std::max)(1u, height / 2);
                ++result;
            }
            return result;
        }

        [[nodiscard]] bool artifact_mip_sizes(
            const CompiledTextureArtifactIdentity& artifact,
            std::vector<std::uint64_t>& result) noexcept
        {
            try
            {
                result.clear();
                result.reserve(artifact.mip_count);
                std::uint64_t total{};
                for (std::uint8_t mip = 0; mip < artifact.mip_count; ++mip)
                {
                    bool overflow{};
                    const std::uint64_t bytes = format_mip_bytes(
                        artifact.profile.format,
                        mip_dimension(artifact.width, mip),
                        mip_dimension(artifact.height, mip),
                        overflow);
                    if (overflow || !checked_add(total, bytes, total))
                        return false;
                    result.push_back(bytes);
                }
                return true;
            }
            catch (...)
            {
                result.clear();
                return false;
            }
        }

        [[nodiscard]] std::uint64_t range_sum(
            const std::vector<std::uint64_t>& values,
            std::uint8_t first,
            std::uint8_t count) noexcept
        {
            std::uint64_t result{};
            const std::size_t end = (std::min)(
                values.size(),
                static_cast<std::size_t>(first) + count);
            for (std::size_t index = first; index < end; ++index)
                result += values[index];
            return result;
        }

        [[nodiscard]] std::uint64_t logical_page_count(
            const CompiledTextureArtifactIdentity& artifact,
            std::uint32_t pageExtent,
            std::uint8_t first,
            std::uint8_t count) noexcept
        {
            std::uint64_t result{};
            const std::uint8_t end = static_cast<std::uint8_t>(
                (std::min)(
                    static_cast<unsigned>(artifact.mip_count),
                    static_cast<unsigned>(first) + count));
            for (std::uint8_t mip = first; mip < end; ++mip)
            {
                const std::uint64_t width =
                    mip_dimension(artifact.width, mip);
                const std::uint64_t height =
                    mip_dimension(artifact.height, mip);
                result += ((width + pageExtent - 1) / pageExtent)
                    * ((height + pageExtent - 1) / pageExtent);
            }
            return result;
        }
    }

    struct TextureDocument::Impl final
    {
        DocumentHandle document{};
        BranchIdentity stream{};
        CanvasDescriptor canvas{};
        HistoryPolicy history{};
        DocumentLimits limits{};
        bool initialized{};
        DocumentState state{};
        DocumentRevision revision{};
        TemporalPoint now{};
        std::vector<OperationEntry> operations{};
        std::size_t operationCursor{};
        std::uint64_t retainedOperationBytes{};
        std::vector<CheckpointEntry> checkpoints{};
        std::uint64_t retainedCheckpointBytes{};
        std::uint64_t nextOperation{ 1 };
        std::uint64_t nextCheckpoint{ 1 };
        std::uint64_t automaticCheckpointsSkipped{};
        mutable std::mutex mutex{};

        [[nodiscard]] ResultCode validate_time(
            TemporalPoint at) const noexcept
        {
            if (!at)
                return ResultCode::invalid_temporal_point;
            if (at.stream != stream)
                return ResultCode::temporal_stream_mismatch;
            if (at.tick < now.tick)
                return ResultCode::temporal_regression;
            return ResultCode::success;
        }

        [[nodiscard]] ResultCode validate_layer(
            LayerHandle handle,
            std::uint32_t& slotIndex) const noexcept
        {
            if (!handle)
                return ResultCode::invalid_handle;
            if (handle.index >= state.slots.size())
                return ResultCode::invalid_handle;
            const LayerSlot& slot = state.slots[handle.index];
            if (!slot.active || slot.generation != handle.generation)
                return ResultCode::stale_handle;
            slotIndex = handle.index;
            return ResultCode::success;
        }

        [[nodiscard]] LayerHandle layer_handle(
            std::uint32_t slotIndex) const noexcept
        {
            if (slotIndex >= state.slots.size())
                return {};
            return LayerHandle{
                slotIndex,
                state.slots[slotIndex].generation
            };
        }

        void refresh_revision() noexcept
        {
            revision.content = document_content_hash(canvas, state);
            ++revision.sequence;
            if (revision.sequence == 0)
                revision.sequence = 1;
        }

        void remove_pruned_checkpoints(std::size_t cursor) noexcept
        {
            for (auto iterator = checkpoints.begin();
                 iterator != checkpoints.end();)
            {
                if (iterator->info.operation_cursor > cursor)
                {
                    retainedCheckpointBytes -=
                        iterator->info.retained_bytes;
                    iterator = checkpoints.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }
        }

        [[nodiscard]] std::uint64_t pruned_operation_bytes() const noexcept
        {
            std::uint64_t result{};
            for (std::size_t index = operationCursor;
                 index < operations.size();
                 ++index)
            {
                result += operations[index].record.retained_bytes;
            }
            return result;
        }

        [[nodiscard]] ResultCode can_append(
            std::uint64_t retainedBytes) const noexcept
        {
            if (history.mode == HistoryMode::disabled)
                return ResultCode::success;

            const std::size_t retainedCount = operationCursor;
            if (retainedCount >= history.maximum_operations)
                return ResultCode::history_operation_limit_exceeded;

            const std::uint64_t pruned = pruned_operation_bytes();
            const std::uint64_t base = retainedOperationBytes - pruned;
            if (retainedBytes >
                history.maximum_operation_bytes - (std::min)(
                    base,
                    history.maximum_operation_bytes))
            {
                return ResultCode::history_memory_budget_exceeded;
            }
            return ResultCode::success;
        }

        void prune_redo()
        {
            if (operationCursor == operations.size())
                return;
            retainedOperationBytes -= pruned_operation_bytes();
            operations.erase(
                operations.begin()
                    + static_cast<std::ptrdiff_t>(operationCursor),
                operations.end());
            remove_pruned_checkpoints(operationCursor);
        }

        [[nodiscard]] bool capture_checkpoint_internal(
            TemporalPoint at,
            CheckpointInfo* captured)
        {
            if (history.mode != HistoryMode::checkpointed_operations)
                return false;
            if (checkpoints.size() >= history.maximum_checkpoints)
                return false;

            const std::uint64_t retained =
                estimate_checkpoint_bytes(state);
            if (retained >
                history.maximum_checkpoint_bytes
                    - (std::min)(
                        retainedCheckpointBytes,
                        history.maximum_checkpoint_bytes))
            {
                return false;
            }

            checkpoints.reserve(checkpoints.size() + 1);
            CheckpointEntry entry{};
            entry.info.handle = CheckpointHandle{ nextCheckpoint };
            entry.info.revision = revision;
            entry.info.operation_cursor = operationCursor;
            entry.info.retained_bytes = retained;
            entry.info.at = at;
            entry.state = state;
            checkpoints.push_back(std::move(entry));
            retainedCheckpointBytes += retained;
            if (captured)
                *captured = checkpoints.back().info;
            ++nextCheckpoint;
            if (nextCheckpoint == 0)
                nextCheckpoint = 1;
            return true;
        }

        [[nodiscard]] bool maybe_capture_automatic_checkpoint()
        {
            if (history.mode != HistoryMode::checkpointed_operations
                || operationCursor == 0
                || operationCursor
                    % history.checkpoint_interval_operations != 0)
            {
                return false;
            }

            try
            {
                if (capture_checkpoint_internal(now, nullptr))
                    return true;
            }
            catch (...)
            {
            }
            ++automaticCheckpointsSkipped;
            return false;
        }

        void apply_delta(const InternalDelta& delta, bool forward)
        {
            std::visit(
                [this, forward](const auto& value)
                {
                    using Type = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, SlotDelta>)
                    {
                        state.slots[value.slot] =
                            forward ? value.after : value.before;
                        if (forward)
                        {
                            if (value.after.active)
                            {
                                state.order.insert(
                                    state.order.begin()
                                        + static_cast<std::ptrdiff_t>(
                                            value.orderIndex),
                                    value.slot);
                            }
                            else
                            {
                                const auto found = std::ranges::find(
                                    state.order,
                                    value.slot);
                                if (found != state.order.end())
                                    state.order.erase(found);
                            }
                        }
                        else
                        {
                            if (value.before.active)
                            {
                                state.order.insert(
                                    state.order.begin()
                                        + static_cast<std::ptrdiff_t>(
                                            value.orderIndex),
                                    value.slot);
                            }
                            else
                            {
                                const auto found = std::ranges::find(
                                    state.order,
                                    value.slot);
                                if (found != state.order.end())
                                    state.order.erase(found);
                            }
                        }
                    }
                    else if constexpr (std::is_same_v<Type, MoveDelta>)
                    {
                        const auto found = std::ranges::find(
                            state.order,
                            value.slot);
                        if (found != state.order.end())
                            state.order.erase(found);
                        const std::uint32_t index =
                            forward ? value.after : value.before;
                        state.order.insert(
                            state.order.begin()
                                + static_cast<std::ptrdiff_t>(index),
                            value.slot);
                    }
                    else if constexpr (
                        std::is_same_v<Type, PropertiesDelta>)
                    {
                        state.slots[value.slot].descriptor =
                            forward ? value.after : value.before;
                    }
                    else
                    {
                        LayerSlot& layer = state.slots[value.slot];
                        for (const TileDelta& tile : value.tiles)
                        {
                            const std::optional<TileData>& selected =
                                forward ? tile.after : tile.before;
                            if (selected)
                                layer.tiles[tile.coordinate] = *selected;
                            else
                                layer.tiles.erase(tile.coordinate);
                        }
                    }
                },
                delta);
        }

        [[nodiscard]] MutationResult commit_operation(
            TextureOperationPayload payload,
            InternalDelta delta,
            TemporalPoint at,
            LayerHandle layer,
            std::uint64_t affectedTiles)
        {
            const DocumentRevision before = revision;
            const std::uint64_t retained =
                sizeof(OperationEntry)
                + estimate_payload_bytes(payload)
                + estimate_delta_bytes(delta);
            const ResultCode capacity = can_append(retained);
            if (capacity != ResultCode::success)
            {
                return MutationResult{
                    .code = capacity,
                    .revision = revision
                };
            }

            if (history.mode == HistoryMode::disabled)
            {
                apply_delta(delta, true);
                now = at;
                refresh_revision();
                return MutationResult{
                    .code = ResultCode::success,
                    .revision = revision,
                    .layer = layer,
                    .affected_tiles = affectedTiles
                };
            }

            operations.reserve(operationCursor + 1);
            OperationEntry entry{};
            entry.record.id = OperationId{ nextOperation };
            entry.record.at = at;
            entry.record.before = before;
            entry.record.payload = std::move(payload);
            entry.record.retained_bytes = retained;
            entry.delta = std::move(delta);
            prune_redo();

            apply_delta(entry.delta, true);
            now = at;
            refresh_revision();
            entry.record.after = revision;
            const OperationId operation = entry.record.id;
            operations.push_back(std::move(entry));
            retainedOperationBytes += retained;
            operationCursor = operations.size();
            ++nextOperation;
            if (nextOperation == 0)
                nextOperation = 1;

            MutationResult result{
                .code = ResultCode::success,
                .revision = revision,
                .operation = operation,
                .layer = layer,
                .affected_tiles = affectedTiles
            };
            result.automatic_checkpoint_captured =
                maybe_capture_automatic_checkpoint();
            return result;
        }
    };

    std::string content_hash_hex(const ContentHash& hash)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string result(64, '0');
        std::size_t cursor{};
        for (const std::uint64_t word : hash.words)
        {
            for (int shift = 60; shift >= 0; shift -= 4)
            {
                result[cursor++] =
                    digits[(word >> shift) & 0x0full];
            }
        }
        return result;
    }

    TextureDocument::TextureDocument(
        DocumentHandle handle,
        BranchIdentity branch,
        CanvasDescriptor descriptor,
        HistoryPolicy history,
        DocumentLimits limits) noexcept
    {
        try
        {
            auto implementation = std::make_unique<Impl>();
            implementation->document = handle;
            implementation->stream = branch;
            implementation->canvas = descriptor;
            implementation->history = history;
            implementation->limits = limits;
            implementation->now = TemporalPoint{ branch, 0 };
            implementation->initialized =
                static_cast<bool>(handle)
                && static_cast<bool>(branch)
                && valid_limits(limits)
                && valid_history(history)
                && valid_canvas(descriptor, limits);
            if (implementation->initialized)
            {
                implementation->revision.content =
                    document_content_hash(descriptor, implementation->state);
                implementation->revision.sequence = 1;
            }
            impl_ = std::move(implementation);
        }
        catch (...)
        {
            impl_.reset();
        }
    }

    TextureDocument::~TextureDocument() = default;
    TextureDocument::TextureDocument(TextureDocument&&) noexcept = default;
    TextureDocument& TextureDocument::operator=(
        TextureDocument&&) noexcept = default;

    bool TextureDocument::valid() const noexcept
    {
        return impl_ && impl_->initialized;
    }

    DocumentHandle TextureDocument::handle() const noexcept
    {
        return impl_ ? impl_->document : DocumentHandle{};
    }

    BranchIdentity TextureDocument::branch() const noexcept
    {
        return impl_ ? impl_->stream : BranchIdentity{ 0, 0 };
    }

    const CanvasDescriptor& TextureDocument::descriptor() const noexcept
    {
        static const CanvasDescriptor invalid{};
        return impl_ ? impl_->canvas : invalid;
    }

    const HistoryPolicy& TextureDocument::history_policy() const noexcept
    {
        static const HistoryPolicy invalid{};
        return impl_ ? impl_->history : invalid;
    }

    const DocumentLimits& TextureDocument::limits() const noexcept
    {
        static const DocumentLimits invalid{};
        return impl_ ? impl_->limits : invalid;
    }

    DocumentRevision TextureDocument::revision() const noexcept
    {
        if (!impl_)
            return {};
        const std::scoped_lock lock{ impl_->mutex };
        return impl_->revision;
    }

    TemporalPoint TextureDocument::current_time() const noexcept
    {
        if (!impl_)
            return {};
        const std::scoped_lock lock{ impl_->mutex };
        return impl_->now;
    }

    DocumentMetrics TextureDocument::metrics() const noexcept
    {
        if (!impl_)
            return {};
        const std::scoped_lock lock{ impl_->mutex };
        return DocumentMetrics{
            .active_layers = impl_->state.order.size(),
            .allocated_layer_slots = impl_->state.slots.size(),
            .sparse_tile_count = state_tile_count(impl_->state),
            .canonical_tile_bytes = state_tile_bytes(impl_->state),
            .operation_count = impl_->operations.size(),
            .applied_operation_count = impl_->operationCursor,
            .retained_operation_bytes = impl_->retainedOperationBytes,
            .checkpoint_count = impl_->checkpoints.size(),
            .retained_checkpoint_bytes = impl_->retainedCheckpointBytes,
            .automatic_checkpoints_skipped =
                impl_->automaticCheckpointsSkipped,
            .revision_sequence = impl_->revision.sequence
        };
    }

    std::vector<LayerInfo> TextureDocument::layers() const
    {
        std::vector<LayerInfo> result{};
        if (!impl_)
            return result;
        const std::scoped_lock lock{ impl_->mutex };
        result.reserve(impl_->state.order.size());
        for (std::uint32_t orderIndex = 0;
             orderIndex < impl_->state.order.size();
             ++orderIndex)
        {
            const std::uint32_t slotIndex = impl_->state.order[orderIndex];
            const LayerSlot& slot = impl_->state.slots[slotIndex];
            result.push_back(LayerInfo{
                .handle = LayerHandle{ slotIndex, slot.generation },
                .descriptor = slot.descriptor,
                .order = orderIndex,
                .stored_tile_count = slot.tiles.size(),
                .stored_bytes = tile_bytes(slot),
                .content = layer_content_hash(slot)
            });
        }
        return result;
    }

    std::optional<LayerInfo> TextureDocument::layer(
        LayerHandle handle) const
    {
        if (!impl_)
            return std::nullopt;
        const std::scoped_lock lock{ impl_->mutex };
        std::uint32_t slotIndex{};
        if (impl_->validate_layer(handle, slotIndex) != ResultCode::success)
            return std::nullopt;
        const auto found = std::ranges::find(impl_->state.order, slotIndex);
        if (found == impl_->state.order.end())
            return std::nullopt;
        const LayerSlot& slot = impl_->state.slots[slotIndex];
        return LayerInfo{
            .handle = handle,
            .descriptor = slot.descriptor,
            .order = static_cast<std::uint32_t>(
                std::distance(impl_->state.order.begin(), found)),
            .stored_tile_count = slot.tiles.size(),
            .stored_bytes = tile_bytes(slot),
            .content = layer_content_hash(slot)
        };
    }

    std::optional<TileSnapshot> TextureDocument::tile(
        LayerHandle layerHandle,
        TileCoordinate coordinate) const
    {
        if (!impl_)
            return std::nullopt;
        const std::scoped_lock lock{ impl_->mutex };
        std::uint32_t slotIndex{};
        if (impl_->validate_layer(layerHandle, slotIndex)
                != ResultCode::success
            || !valid_coordinate(impl_->canvas, coordinate))
        {
            return std::nullopt;
        }

        TileSnapshot result{
            .coordinate = coordinate,
            .width = tile_width(impl_->canvas, coordinate),
            .height = tile_height(impl_->canvas, coordinate)
        };
        const LayerSlot& slot = impl_->state.slots[slotIndex];
        const auto found = slot.tiles.find(coordinate);
        if (found != slot.tiles.end())
            result.texels = found->second.texels;
        result.content = tile_content_hash(
            coordinate,
            result.width,
            result.height,
            result.texels);
        return result;
    }

    PixelRgba8 TextureDocument::pixel(
        LayerHandle layerHandle,
        std::uint8_t mip,
        std::uint32_t x,
        std::uint32_t y) const noexcept
    {
        if (!impl_)
            return {};
        const std::scoped_lock lock{ impl_->mutex };
        std::uint32_t slotIndex{};
        if (impl_->validate_layer(layerHandle, slotIndex)
            != ResultCode::success)
        {
            return {};
        }
        if (mip >= impl_->canvas.mip_count
            || x >= mip_dimension(impl_->canvas.width, mip)
            || y >= mip_dimension(impl_->canvas.height, mip))
        {
            return {};
        }

        const TileCoordinate coordinate{
            mip,
            x / impl_->canvas.tile_extent,
            y / impl_->canvas.tile_extent
        };
        const LayerSlot& slot = impl_->state.slots[slotIndex];
        const auto found = slot.tiles.find(coordinate);
        if (found == slot.tiles.end())
            return {};
        const std::uint32_t localX =
            x % impl_->canvas.tile_extent;
        const std::uint32_t localY =
            y % impl_->canvas.tile_extent;
        const std::size_t offset =
            (static_cast<std::size_t>(localY) * found->second.width + localX)
            * 4;
        if (offset + 3 >= found->second.texels.size())
            return {};
        return PixelRgba8{
            std::to_integer<std::uint8_t>(found->second.texels[offset]),
            std::to_integer<std::uint8_t>(found->second.texels[offset + 1]),
            std::to_integer<std::uint8_t>(found->second.texels[offset + 2]),
            std::to_integer<std::uint8_t>(found->second.texels[offset + 3])
        };
    }

    MutationResult TextureDocument::create_layer(
        LayerDescriptor descriptor,
        std::uint32_t insertionIndex,
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        if (!valid_layer_descriptor(descriptor, impl_->limits)
            || insertionIndex > impl_->state.order.size())
        {
            return {
                .code = ResultCode::invalid_descriptor,
                .revision = impl_->revision
            };
        }
        if (impl_->state.order.size() >= impl_->limits.maximum_layers)
        {
            return {
                .code = ResultCode::layer_limit_exceeded,
                .revision = impl_->revision
            };
        }

        std::uint32_t slotIndex =
            static_cast<std::uint32_t>(impl_->state.slots.size());
        for (std::uint32_t index = 0;
             index < impl_->state.slots.size();
             ++index)
        {
            if (!impl_->state.slots[index].active)
            {
                slotIndex = index;
                break;
            }
        }

        LayerSlot before{};
        if (slotIndex < impl_->state.slots.size())
            before = impl_->state.slots[slotIndex];
        else
            before.generation = 1;
        LayerSlot after = before;
        after.active = true;
        after.descriptor = descriptor;
        after.tiles.clear();
        if (after.generation == 0)
            after.generation = 1;

        if (slotIndex == impl_->state.slots.size())
            impl_->state.slots.emplace_back(before);

        const LayerHandle handle{ slotIndex, after.generation };
        LayerCreatedOperation operation{
            .layer = handle,
            .descriptor = descriptor,
            .insertion_index = insertionIndex
        };
        SlotDelta delta{
            .slot = slotIndex,
            .before = std::move(before),
            .after = std::move(after),
            .orderIndex = insertionIndex
        };
        MutationResult result = impl_->commit_operation(
            std::move(operation),
            std::move(delta),
            at,
            handle,
            0);
        if (!result && slotIndex + 1 == impl_->state.slots.size()
            && !impl_->state.slots.back().active)
        {
            impl_->state.slots.pop_back();
        }
        return result;
    }

    MutationResult TextureDocument::remove_layer(
        LayerHandle layerHandle,
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        std::uint32_t slotIndex{};
        const ResultCode layerCode =
            impl_->validate_layer(layerHandle, slotIndex);
        if (layerCode != ResultCode::success)
            return { .code = layerCode, .revision = impl_->revision };

        const auto found = std::ranges::find(
            impl_->state.order,
            slotIndex);
        const std::uint32_t orderIndex = static_cast<std::uint32_t>(
            std::distance(impl_->state.order.begin(), found));
        LayerSlot before = impl_->state.slots[slotIndex];
        LayerSlot after{};
        after.generation = next_generation(before.generation);
        const ContentHash removedContent = layer_content_hash(before);
        LayerRemovedOperation operation{
            .layer = layerHandle,
            .previous_index = orderIndex,
            .removed_content = removedContent
        };
        SlotDelta delta{
            .slot = slotIndex,
            .before = std::move(before),
            .after = std::move(after),
            .orderIndex = orderIndex
        };
        return impl_->commit_operation(
            std::move(operation),
            std::move(delta),
            at,
            layerHandle,
            0);
    }

    MutationResult TextureDocument::move_layer(
        LayerHandle layerHandle,
        std::uint32_t insertionIndex,
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        std::uint32_t slotIndex{};
        const ResultCode layerCode =
            impl_->validate_layer(layerHandle, slotIndex);
        if (layerCode != ResultCode::success)
            return { .code = layerCode, .revision = impl_->revision };
        if (insertionIndex >= impl_->state.order.size())
        {
            return {
                .code = ResultCode::invalid_descriptor,
                .revision = impl_->revision
            };
        }

        const auto found = std::ranges::find(
            impl_->state.order,
            slotIndex);
        const std::uint32_t previousIndex = static_cast<std::uint32_t>(
            std::distance(impl_->state.order.begin(), found));
        if (previousIndex == insertionIndex)
        {
            return {
                .code = ResultCode::unchanged,
                .revision = impl_->revision,
                .layer = layerHandle
            };
        }

        LayerMovedOperation operation{
            .layer = layerHandle,
            .previous_index = previousIndex,
            .next_index = insertionIndex
        };
        MoveDelta delta{
            .slot = slotIndex,
            .before = previousIndex,
            .after = insertionIndex
        };
        return impl_->commit_operation(
            std::move(operation),
            std::move(delta),
            at,
            layerHandle,
            0);
    }

    MutationResult TextureDocument::set_layer_properties(
        LayerHandle layerHandle,
        LayerDescriptor descriptor,
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        std::uint32_t slotIndex{};
        const ResultCode layerCode =
            impl_->validate_layer(layerHandle, slotIndex);
        if (layerCode != ResultCode::success)
            return { .code = layerCode, .revision = impl_->revision };
        if (!valid_layer_descriptor(descriptor, impl_->limits))
        {
            return {
                .code = ResultCode::invalid_descriptor,
                .revision = impl_->revision
            };
        }

        const LayerDescriptor before =
            impl_->state.slots[slotIndex].descriptor;
        if (before == descriptor)
        {
            return {
                .code = ResultCode::unchanged,
                .revision = impl_->revision,
                .layer = layerHandle
            };
        }

        LayerPropertiesChangedOperation operation{
            .layer = layerHandle,
            .before = before,
            .after = descriptor
        };
        PropertiesDelta delta{
            .slot = slotIndex,
            .before = before,
            .after = std::move(descriptor)
        };
        return impl_->commit_operation(
            std::move(operation),
            std::move(delta),
            at,
            layerHandle,
            0);
    }

    MutationResult TextureDocument::apply_stroke(
        StrokeDescriptor stroke,
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        std::uint32_t slotIndex{};
        const ResultCode layerCode =
            impl_->validate_layer(stroke.target, slotIndex);
        if (layerCode != ResultCode::success)
            return { .code = layerCode, .revision = impl_->revision };
        if (impl_->state.slots[slotIndex].descriptor.locked)
        {
            return {
                .code = ResultCode::invalid_operation,
                .revision = impl_->revision
            };
        }
        if (stroke.program != BrushProgram::round_stamp_v1
            || stroke.algorithm_version != 1)
        {
            return {
                .code = ResultCode::unsupported_brush,
                .revision = impl_->revision
            };
        }
        if (stroke.mip >= impl_->canvas.mip_count
            || stroke.radius_subpixels == 0
            || stroke.radius_subpixels
                > impl_->limits.maximum_brush_radius_subpixels
            || stroke.channel_mask == 0
            || (stroke.channel_mask & 0xf0u) != 0)
        {
            return {
                .code = ResultCode::invalid_descriptor,
                .revision = impl_->revision
            };
        }
        if (stroke.samples.empty()
            || stroke.samples.size() > impl_->limits.maximum_stroke_samples)
        {
            return {
                .code = ResultCode::stroke_sample_limit_exceeded,
                .revision = impl_->revision
            };
        }

        const std::uint32_t width =
            mip_dimension(impl_->canvas.width, stroke.mip);
        const std::uint32_t height =
            mip_dimension(impl_->canvas.height, stroke.mip);

        // Bound raster work before any pixel or canonical tile allocation.
        std::map<TileCoordinate, bool, TileCoordinateLess> candidateTiles{};
        const auto saturatingOffset = [](
            std::int64_t value,
            std::uint64_t distance,
            bool add) noexcept
        {
            const std::int64_t signedDistance =
                static_cast<std::int64_t>(distance);
            if (add)
            {
                return value >
                    (std::numeric_limits<std::int64_t>::max)()
                        - signedDistance
                    ? (std::numeric_limits<std::int64_t>::max)()
                    : value + signedDistance;
            }
            return value <
                (std::numeric_limits<std::int64_t>::min)()
                    + signedDistance
                ? (std::numeric_limits<std::int64_t>::min)()
                : value - signedDistance;
        };
        const std::int64_t canvasMaximumX =
            static_cast<std::int64_t>(width)
            * static_cast<std::int64_t>(kSubpixelsPerPixel) - 1;
        const std::int64_t canvasMaximumY =
            static_cast<std::int64_t>(height)
            * static_cast<std::int64_t>(kSubpixelsPerPixel) - 1;
        for (const StrokeSample& sample : stroke.samples)
        {
            const std::uint64_t radius =
                (static_cast<std::uint64_t>(stroke.radius_subpixels)
                    * sample.pressure
                    + kChannelMaximum / 2)
                / kChannelMaximum;
            if (radius == 0)
                continue;

            const std::int64_t minimumX =
                saturatingOffset(sample.x_subpixels, radius, false);
            const std::int64_t maximumX =
                saturatingOffset(sample.x_subpixels, radius, true);
            const std::int64_t minimumY =
                saturatingOffset(sample.y_subpixels, radius, false);
            const std::int64_t maximumY =
                saturatingOffset(sample.y_subpixels, radius, true);
            if (maximumX < 0
                || maximumY < 0
                || minimumX > canvasMaximumX
                || minimumY > canvasMaximumY)
            {
                continue;
            }

            const std::uint32_t firstTileX =
                static_cast<std::uint32_t>((std::max)(
                    std::int64_t{},
                    minimumX))
                / static_cast<std::uint32_t>(kSubpixelsPerPixel)
                / impl_->canvas.tile_extent;
            const std::uint32_t lastTileX =
                static_cast<std::uint32_t>((std::min)(
                    canvasMaximumX,
                    maximumX))
                / static_cast<std::uint32_t>(kSubpixelsPerPixel)
                / impl_->canvas.tile_extent;
            const std::uint32_t firstTileY =
                static_cast<std::uint32_t>((std::max)(
                    std::int64_t{},
                    minimumY))
                / static_cast<std::uint32_t>(kSubpixelsPerPixel)
                / impl_->canvas.tile_extent;
            const std::uint32_t lastTileY =
                static_cast<std::uint32_t>((std::min)(
                    canvasMaximumY,
                    maximumY))
                / static_cast<std::uint32_t>(kSubpixelsPerPixel)
                / impl_->canvas.tile_extent;
            for (std::uint32_t tileY = firstTileY;
                 tileY <= lastTileY;
                 ++tileY)
            {
                for (std::uint32_t tileX = firstTileX;
                     tileX <= lastTileX;
                     ++tileX)
                {
                    candidateTiles.emplace(
                        TileCoordinate{ stroke.mip, tileX, tileY },
                        true);
                    if (candidateTiles.size()
                        > impl_->limits.maximum_tiles_per_stroke)
                    {
                        return {
                            .code =
                                ResultCode::stroke_tile_limit_exceeded,
                            .revision = impl_->revision
                        };
                    }
                }
            }
        }

        const LayerSlot& sourceLayer = impl_->state.slots[slotIndex];
        std::uint64_t projectedTileCount = state_tile_count(impl_->state);
        std::uint64_t projectedTileBytes = state_tile_bytes(impl_->state);
        for (const auto& [coordinate, selected] : candidateTiles)
        {
            (void)selected;
            if (sourceLayer.tiles.contains(coordinate))
                continue;

            std::uint64_t pixels{};
            std::uint64_t bytes{};
            if (!checked_multiply(
                    tile_width(impl_->canvas, coordinate),
                    tile_height(impl_->canvas, coordinate),
                    pixels)
                || !checked_multiply(pixels, 4, bytes)
                || !checked_add(
                    projectedTileBytes,
                    bytes,
                    projectedTileBytes))
            {
                return {
                    .code = ResultCode::arithmetic_overflow,
                    .revision = impl_->revision
                };
            }
            ++projectedTileCount;
        }
        if (projectedTileCount > impl_->limits.maximum_sparse_tiles)
        {
            return {
                .code = ResultCode::tile_limit_exceeded,
                .revision = impl_->revision
            };
        }
        if (projectedTileBytes
            > impl_->limits.maximum_canonical_tile_bytes)
        {
            return {
                .code = ResultCode::canonical_memory_budget_exceeded,
                .revision = impl_->revision
            };
        }

        std::map<TileCoordinate, TileData, TileCoordinateLess> changed{};

        for (const StrokeSample& sample : stroke.samples)
        {
            const std::uint64_t radius =
                (static_cast<std::uint64_t>(stroke.radius_subpixels)
                    * sample.pressure
                    + kChannelMaximum / 2)
                / kChannelMaximum;
            if (radius == 0)
                continue;

            const std::int64_t minimumX =
                sample.x_subpixels
                    <= (std::numeric_limits<std::int64_t>::min)()
                        + static_cast<std::int64_t>(radius)
                ? (std::numeric_limits<std::int64_t>::min)()
                : sample.x_subpixels - static_cast<std::int64_t>(radius);
            const std::int64_t maximumX =
                sample.x_subpixels
                    >= (std::numeric_limits<std::int64_t>::max)()
                        - static_cast<std::int64_t>(radius)
                ? (std::numeric_limits<std::int64_t>::max)()
                : sample.x_subpixels + static_cast<std::int64_t>(radius);
            const std::int64_t minimumY =
                sample.y_subpixels
                    <= (std::numeric_limits<std::int64_t>::min)()
                        + static_cast<std::int64_t>(radius)
                ? (std::numeric_limits<std::int64_t>::min)()
                : sample.y_subpixels - static_cast<std::int64_t>(radius);
            const std::int64_t maximumY =
                sample.y_subpixels
                    >= (std::numeric_limits<std::int64_t>::max)()
                        - static_cast<std::int64_t>(radius)
                ? (std::numeric_limits<std::int64_t>::max)()
                : sample.y_subpixels + static_cast<std::int64_t>(radius);

            const auto pixel_floor = [](std::int64_t value) noexcept
            {
                if (value >= 0)
                    return value / static_cast<std::int64_t>(
                        kSubpixelsPerPixel);
                return -(
                    (-value
                        + static_cast<std::int64_t>(
                            kSubpixelsPerPixel) - 1)
                    / static_cast<std::int64_t>(kSubpixelsPerPixel));
            };
            const std::int64_t firstX = (std::max<std::int64_t>)(
                0,
                pixel_floor(minimumX));
            const std::int64_t lastX = (std::min<std::int64_t>)(
                static_cast<std::int64_t>(width) - 1,
                pixel_floor(maximumX));
            const std::int64_t firstY = (std::max<std::int64_t>)(
                0,
                pixel_floor(minimumY));
            const std::int64_t lastY = (std::min<std::int64_t>)(
                static_cast<std::int64_t>(height) - 1,
                pixel_floor(maximumY));
            if (firstX > lastX || firstY > lastY)
                continue;

            const std::uint64_t inner =
                (radius * stroke.hardness + kChannelMaximum / 2)
                / kChannelMaximum;
            for (std::int64_t y = firstY; y <= lastY; ++y)
            {
                for (std::int64_t x = firstX; x <= lastX; ++x)
                {
                    const std::int64_t centerX =
                        x * static_cast<std::int64_t>(kSubpixelsPerPixel)
                        + static_cast<std::int64_t>(
                            kSubpixelsPerPixel / 2);
                    const std::int64_t centerY =
                        y * static_cast<std::int64_t>(kSubpixelsPerPixel)
                        + static_cast<std::int64_t>(
                            kSubpixelsPerPixel / 2);
                    const std::uint64_t deltaX = static_cast<std::uint64_t>(
                        centerX >= sample.x_subpixels
                            ? centerX - sample.x_subpixels
                            : sample.x_subpixels - centerX);
                    const std::uint64_t deltaY = static_cast<std::uint64_t>(
                        centerY >= sample.y_subpixels
                            ? centerY - sample.y_subpixels
                            : sample.y_subpixels - centerY);
                    if (deltaX > radius || deltaY > radius)
                        continue;
                    const std::uint64_t distanceSquared =
                        deltaX * deltaX + deltaY * deltaY;
                    if (distanceSquared > radius * radius)
                        continue;
                    const std::uint64_t distance =
                        integer_square_root(distanceSquared);
                    std::uint32_t coverage =
                        static_cast<std::uint32_t>(kChannelMaximum);
                    if (distance > inner && radius > inner)
                    {
                        coverage = static_cast<std::uint32_t>(
                            ((radius - distance) * kChannelMaximum)
                            / (radius - inner));
                    }
                    std::uint32_t amount = multiply_unit(
                        stroke.opacity,
                        sample.pressure);
                    amount = multiply_unit(amount, coverage);
                    if (amount == 0)
                        continue;

                    const TileCoordinate coordinate{
                        stroke.mip,
                        static_cast<std::uint32_t>(x)
                            / impl_->canvas.tile_extent,
                        static_cast<std::uint32_t>(y)
                            / impl_->canvas.tile_extent
                    };
                    auto changedTile = changed.find(coordinate);
                    if (changedTile == changed.end())
                    {
                        TileData tileData{
                            .width = tile_width(
                                impl_->canvas,
                                coordinate),
                            .height = tile_height(
                                impl_->canvas,
                                coordinate)
                        };
                        const auto existing =
                            sourceLayer.tiles.find(coordinate);
                        if (existing != sourceLayer.tiles.end())
                        {
                            tileData = existing->second;
                        }
                        else
                        {
                            tileData.texels.resize(
                                static_cast<std::size_t>(tileData.width)
                                    * tileData.height * 4);
                        }
                        changedTile = changed.emplace(
                            coordinate,
                            std::move(tileData)).first;
                    }

                    TileData& tile = changedTile->second;
                    const std::uint32_t localX =
                        static_cast<std::uint32_t>(x)
                        % impl_->canvas.tile_extent;
                    const std::uint32_t localY =
                        static_cast<std::uint32_t>(y)
                        % impl_->canvas.tile_extent;
                    const std::size_t offset =
                        (static_cast<std::size_t>(localY) * tile.width
                            + localX)
                        * 4;
                    const std::array<std::uint8_t, 4> source{
                        stroke.color.r,
                        stroke.color.g,
                        stroke.color.b,
                        stroke.color.a
                    };
                    for (std::size_t channel = 0; channel < 4; ++channel)
                    {
                        if ((stroke.channel_mask & (1u << channel)) == 0)
                            continue;
                        const std::uint8_t before =
                            std::to_integer<std::uint8_t>(
                                tile.texels[offset + channel]);
                        tile.texels[offset + channel] = std::byte{
                            blend_channel(
                                before,
                                source[channel],
                                amount)
                        };
                    }
                }
            }
        }

        StrokeDelta delta{ .slot = slotIndex };
        std::vector<TileCoordinate> affected{};
        delta.tiles.reserve(changed.size());
        affected.reserve(changed.size());
        std::uint64_t resultingTileCount =
            state_tile_count(impl_->state);
        std::uint64_t resultingBytes =
            state_tile_bytes(impl_->state);
        for (auto& [coordinate, afterData] : changed)
        {
            const auto beforeIterator =
                sourceLayer.tiles.find(coordinate);
            std::optional<TileData> before{};
            if (beforeIterator != sourceLayer.tiles.end())
                before = beforeIterator->second;
            std::optional<TileData> after{};
            if (!all_clear(afterData))
                after = std::move(afterData);
            if (before == after)
                continue;

            if (before)
            {
                --resultingTileCount;
                resultingBytes -= before->texels.size();
            }
            if (after)
            {
                ++resultingTileCount;
                if (!checked_add(
                        resultingBytes,
                        after->texels.size(),
                        resultingBytes))
                {
                    return {
                        .code = ResultCode::arithmetic_overflow,
                        .revision = impl_->revision
                    };
                }
            }
            affected.push_back(coordinate);
            delta.tiles.push_back(TileDelta{
                .coordinate = coordinate,
                .before = std::move(before),
                .after = std::move(after)
            });
        }

        if (delta.tiles.empty())
        {
            return {
                .code = ResultCode::unchanged,
                .revision = impl_->revision,
                .layer = stroke.target
            };
        }
        if (delta.tiles.size() > impl_->limits.maximum_tiles_per_stroke)
        {
            return {
                .code = ResultCode::stroke_tile_limit_exceeded,
                .revision = impl_->revision
            };
        }
        if (resultingTileCount > impl_->limits.maximum_sparse_tiles)
        {
            return {
                .code = ResultCode::tile_limit_exceeded,
                .revision = impl_->revision
            };
        }
        if (resultingBytes > impl_->limits.maximum_canonical_tile_bytes)
        {
            return {
                .code = ResultCode::canonical_memory_budget_exceeded,
                .revision = impl_->revision
            };
        }

        TextureStrokeAppliedOperation operation{
            .stroke = std::move(stroke),
            .affected_tiles = std::move(affected)
        };
        const LayerHandle target = operation.stroke.target;
        const std::uint64_t affectedCount = delta.tiles.size();
        return impl_->commit_operation(
            std::move(operation),
            std::move(delta),
            at,
            target,
            affectedCount);
    }

    bool TextureDocument::can_undo() const noexcept
    {
        if (!impl_)
            return false;
        const std::scoped_lock lock{ impl_->mutex };
        return impl_->history.mode != HistoryMode::disabled
            && impl_->operationCursor != 0;
    }

    bool TextureDocument::can_redo() const noexcept
    {
        if (!impl_)
            return false;
        const std::scoped_lock lock{ impl_->mutex };
        return impl_->history.mode != HistoryMode::disabled
            && impl_->operationCursor < impl_->operations.size();
    }

    MutationResult TextureDocument::undo(TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        if (impl_->history.mode == HistoryMode::disabled
            || impl_->operationCursor == 0)
        {
            return {
                .code = ResultCode::nothing_to_undo,
                .revision = impl_->revision
            };
        }

        OperationEntry& entry =
            impl_->operations[impl_->operationCursor - 1];
        impl_->apply_delta(entry.delta, false);
        --impl_->operationCursor;
        impl_->now = at;
        impl_->refresh_revision();
        return MutationResult{
            .code = ResultCode::success,
            .revision = impl_->revision,
            .operation = entry.record.id,
            .affected_tiles = std::visit(
                [](const auto& value) -> std::uint64_t
                {
                    using Type = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, StrokeDelta>)
                    {
                        return value.tiles.size();
                    }
                    else
                    {
                        return 0;
                    }
                },
                entry.delta)
        };
    }

    MutationResult TextureDocument::redo(TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        if (impl_->history.mode == HistoryMode::disabled
            || impl_->operationCursor >= impl_->operations.size())
        {
            return {
                .code = ResultCode::nothing_to_redo,
                .revision = impl_->revision
            };
        }

        OperationEntry& entry =
            impl_->operations[impl_->operationCursor];
        impl_->apply_delta(entry.delta, true);
        ++impl_->operationCursor;
        impl_->now = at;
        impl_->refresh_revision();
        return MutationResult{
            .code = ResultCode::success,
            .revision = impl_->revision,
            .operation = entry.record.id,
            .affected_tiles = std::visit(
                [](const auto& value) -> std::uint64_t
                {
                    using Type = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, StrokeDelta>)
                    {
                        return value.tiles.size();
                    }
                    else
                    {
                        return 0;
                    }
                },
                entry.delta)
        };
    }

    CheckpointResult TextureDocument::capture_checkpoint(
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time };
        if (impl_->history.mode != HistoryMode::checkpointed_operations)
            return { .code = ResultCode::invalid_operation };
        if (impl_->checkpoints.size() >=
            impl_->history.maximum_checkpoints)
        {
            return { .code = ResultCode::checkpoint_limit_exceeded };
        }

        CheckpointInfo captured{};
        if (!impl_->capture_checkpoint_internal(at, &captured))
        {
            return {
                .code = ResultCode::checkpoint_memory_budget_exceeded
            };
        }
        impl_->now = at;
        return {
            .code = ResultCode::success,
            .checkpoint = captured
        };
    }

    MutationResult TextureDocument::restore_checkpoint(
        CheckpointHandle checkpoint,
        TemporalPoint at)
    {
        if (!impl_ || !impl_->initialized)
            return { .code = ResultCode::invalid_document };
        const std::scoped_lock lock{ impl_->mutex };
        const ResultCode time = impl_->validate_time(at);
        if (time != ResultCode::success)
            return { .code = time, .revision = impl_->revision };
        if (!checkpoint)
        {
            return {
                .code = ResultCode::checkpoint_not_found,
                .revision = impl_->revision
            };
        }
        const auto found = std::ranges::find_if(
            impl_->checkpoints,
            [checkpoint](const CheckpointEntry& entry)
            {
                return entry.info.handle == checkpoint;
            });
        if (found == impl_->checkpoints.end())
        {
            return {
                .code = ResultCode::checkpoint_not_found,
                .revision = impl_->revision
            };
        }

        impl_->state = found->state;
        impl_->operationCursor = static_cast<std::size_t>((std::min)(
            found->info.operation_cursor,
            static_cast<std::uint64_t>(impl_->operations.size())));
        impl_->now = at;
        impl_->refresh_revision();
        return {
            .code = ResultCode::success,
            .revision = impl_->revision,
            .affected_tiles = state_tile_count(impl_->state)
        };
    }

    std::vector<CheckpointInfo> TextureDocument::checkpoints() const
    {
        std::vector<CheckpointInfo> result{};
        if (!impl_)
            return result;
        const std::scoped_lock lock{ impl_->mutex };
        result.reserve(impl_->checkpoints.size());
        for (const CheckpointEntry& entry : impl_->checkpoints)
            result.push_back(entry.info);
        return result;
    }

    std::vector<OperationSummary>
        TextureDocument::operation_summaries() const
    {
        std::vector<OperationSummary> result{};
        if (!impl_)
            return result;
        const std::scoped_lock lock{ impl_->mutex };
        result.reserve(impl_->operations.size());
        for (std::size_t index = 0;
             index < impl_->operations.size();
             ++index)
        {
            const OperationRecord& record =
                impl_->operations[index].record;
            result.push_back(OperationSummary{
                .id = record.id,
                .at = record.at,
                .kind = operation_kind(record.payload),
                .before = record.before,
                .after = record.after,
                .retained_bytes = record.retained_bytes,
                .currently_applied = index < impl_->operationCursor
            });
        }
        return result;
    }

    std::vector<OperationRecord> TextureDocument::operation_records() const
    {
        std::vector<OperationRecord> result{};
        if (!impl_)
            return result;
        const std::scoped_lock lock{ impl_->mutex };
        result.reserve(impl_->operations.size());
        for (const OperationEntry& entry : impl_->operations)
            result.push_back(entry.record);
        return result;
    }

    CompiledTextureArtifactIdentity
        TextureDocument::compiled_artifact_identity(
            TextureCompileProfile profile) const noexcept
    {
        if (!impl_ || !impl_->initialized
            || profile.compiler_schema_version == 0)
        {
            return {};
        }
        const std::scoped_lock lock{ impl_->mutex };
        const std::uint8_t mipCount =
            profile.mipmaps == MipmapPolicy::preserve_authored
                ? impl_->canvas.mip_count
                : full_mip_count(
                    impl_->canvas.width,
                    impl_->canvas.height);
        std::uint64_t bytes{};
        for (std::uint8_t mip = 0; mip < mipCount; ++mip)
        {
            bool overflow{};
            const std::uint64_t mipBytes = format_mip_bytes(
                profile.format,
                mip_dimension(impl_->canvas.width, mip),
                mip_dimension(impl_->canvas.height, mip),
                overflow);
            if (overflow || !checked_add(bytes, mipBytes, bytes))
                return {};
        }

        StableHash hash{};
        hash.add_string("epoch.texture.artifact.v1");
        hash.add_hash(impl_->revision.content);
        hash.add_integral(profile.format);
        hash.add_integral(profile.color_space);
        hash.add_integral(profile.mipmaps);
        hash.add_integral(profile.compiler_schema_version);
        hash.add_integral(profile.quality_tier);
        hash.add_integral(impl_->canvas.width);
        hash.add_integral(impl_->canvas.height);
        hash.add_integral(mipCount);
        return CompiledTextureArtifactIdentity{
            .key = hash.finish(),
            .source_revision = impl_->revision,
            .profile = profile,
            .width = impl_->canvas.width,
            .height = impl_->canvas.height,
            .mip_count = mipCount,
            .estimated_artifact_bytes = bytes,
            .compilation_required = true
        };
    }

    PhysicalResidencyPlan plan_physical_residency(
        const CompiledTextureArtifactIdentity& artifact,
        const PhysicalResidencyCapabilities& capabilities,
        const PhysicalResidencyPolicy& policy) noexcept
    {
        PhysicalResidencyPlan result{};
        result.artifact = artifact;
        if (!artifact
            || artifact.profile.compiler_schema_version == 0
            || artifact.estimated_artifact_bytes == 0)
        {
            result.status = ResidencyPlanStatus::invalid_artifact;
            return result;
        }
        if (capabilities.maximum_texture_dimension == 0
            || capabilities.maximum_resident_bytes == 0
            || (capabilities.sparse_images
                && capabilities.sparse_page_extent == 0))
        {
            result.status = ResidencyPlanStatus::invalid_capabilities;
            return result;
        }
        if (artifact.width > capabilities.maximum_texture_dimension
            || artifact.height > capabilities.maximum_texture_dimension)
        {
            result.status = ResidencyPlanStatus::no_compatible_strategy;
            return result;
        }

        std::uint64_t sparsePagePixels{};
        std::uint64_t sparsePageBytes{};
        if (capabilities.sparse_images
            && (!checked_multiply(
                    capabilities.sparse_page_extent,
                    capabilities.sparse_page_extent,
                    sparsePagePixels)
                || !checked_multiply(
                    sparsePagePixels,
                    4,
                    sparsePageBytes)))
        {
            result.status = ResidencyPlanStatus::arithmetic_overflow;
            return result;
        }

        std::vector<std::uint64_t> mipSizes{};
        if (!artifact_mip_sizes(artifact, mipSizes))
        {
            result.status = ResidencyPlanStatus::arithmetic_overflow;
            return result;
        }
        const std::uint64_t calculatedArtifactBytes =
            range_sum(mipSizes, 0, artifact.mip_count);
        if (calculatedArtifactBytes != artifact.estimated_artifact_bytes)
        {
            result.status = ResidencyPlanStatus::invalid_artifact;
            return result;
        }
        const std::uint64_t hardBudget =
            (std::min)(
                capabilities.maximum_resident_bytes,
                policy.target_resident_bytes == 0
                    ? capabilities.maximum_resident_bytes
                    : policy.target_resident_bytes);

        std::array<PhysicalResidencyKind, 7> candidates{
            policy.preferred,
            PhysicalResidencyKind::hybrid_sparse_tail,
            PhysicalResidencyKind::sparse_pages,
            PhysicalResidencyKind::bindless_image,
            PhysicalResidencyKind::atlas_region,
            PhysicalResidencyKind::standalone_image,
            policy.preferred
        };
        if (policy.allow_atlas_for_small_textures
            && artifact.width <= policy.atlas_small_texture_limit
            && artifact.height <= policy.atlas_small_texture_limit)
        {
            candidates[1] = PhysicalResidencyKind::atlas_region;
        }

        bool compatibleButOverBudget{};
        for (std::size_t candidateIndex = 0;
             candidateIndex < candidates.size();
             ++candidateIndex)
        {
            if (candidateIndex != 0 && !policy.allow_fallback)
                break;
            const PhysicalResidencyKind kind = candidates[candidateIndex];
            if (std::ranges::find(
                    candidates.begin(),
                    candidates.begin()
                        + static_cast<std::ptrdiff_t>(candidateIndex),
                    kind)
                != candidates.begin()
                    + static_cast<std::ptrdiff_t>(candidateIndex))
            {
                continue;
            }

            std::vector<ResidencyTier> tiers{};
            std::uint64_t residentBytes{};
            std::uint32_t descriptorCount{};
            bool compatible{};
            if (kind == PhysicalResidencyKind::standalone_image)
            {
                compatible = capabilities.standalone_images;
                residentBytes = artifact.estimated_artifact_bytes;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else if (kind == PhysicalResidencyKind::atlas_region)
            {
                compatible = capabilities.atlas_regions
                    && policy.allow_atlas_for_small_textures
                    && artifact.width <= capabilities.maximum_atlas_dimension
                    && artifact.height <= capabilities.maximum_atlas_dimension
                    && artifact.width <= policy.atlas_small_texture_limit
                    && artifact.height <= policy.atlas_small_texture_limit;
                residentBytes = artifact.estimated_artifact_bytes;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else if (kind == PhysicalResidencyKind::bindless_image)
            {
                compatible = capabilities.bindless_images
                    && capabilities.available_bindless_descriptors != 0;
                residentBytes = artifact.estimated_artifact_bytes;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else if (kind == PhysicalResidencyKind::sparse_pages)
            {
                compatible = capabilities.sparse_images;
                const std::uint64_t logicalPages = compatible
                    ? logical_page_count(
                        artifact,
                        capabilities.sparse_page_extent,
                        0,
                        artifact.mip_count)
                    : 0;
                residentBytes = compatible
                    ? (std::min)(
                        artifact.estimated_artifact_bytes,
                        (std::max)(sparsePageBytes, hardBudget))
                    : 0;
                descriptorCount = 1;
                if (compatible)
                {
                    tiers.push_back(ResidencyTier{
                        .kind = kind,
                        .first_mip = 0,
                        .mip_count = artifact.mip_count,
                        .logical_tile_count = logicalPages,
                        .estimated_resident_bytes = residentBytes,
                        .descriptor_count = 1
                    });
                }
            }
            else
            {
                const std::uint8_t streamedMips = (std::min)(
                    policy.minimum_streamed_mip_count,
                    static_cast<std::uint8_t>(
                        artifact.mip_count > 1
                            ? artifact.mip_count - 1
                            : 0));
                compatible = capabilities.sparse_images
                    && capabilities.standalone_images
                    && streamedMips != 0;
                if (compatible)
                {
                    const std::uint8_t tailMips =
                        artifact.mip_count - streamedMips;
                    const std::uint64_t sparseLogicalBytes =
                        range_sum(mipSizes, 0, streamedMips);
                    const std::uint64_t tailBytes =
                        range_sum(
                            mipSizes,
                            streamedMips,
                            tailMips);

                    const std::uint64_t sparseBudget =
                        hardBudget > tailBytes
                            ? hardBudget - tailBytes
                            : 0;
                    const std::uint64_t sparseResident =
                        (std::min)(
                            sparseLogicalBytes,
                            (std::max)(sparsePageBytes, sparseBudget));
                    residentBytes = tailBytes + sparseResident;
                    descriptorCount = 2;
                    tiers.push_back(ResidencyTier{
                        .kind = PhysicalResidencyKind::sparse_pages,
                        .first_mip = 0,
                        .mip_count = streamedMips,
                        .logical_tile_count = logical_page_count(
                            artifact,
                            capabilities.sparse_page_extent,
                            0,
                            streamedMips),
                        .estimated_resident_bytes = sparseResident,
                        .descriptor_count = 1
                    });
                    tiers.push_back(ResidencyTier{
                        .kind = PhysicalResidencyKind::standalone_image,
                        .first_mip = streamedMips,
                        .mip_count = tailMips,
                        .logical_tile_count = 1,
                        .estimated_resident_bytes = tailBytes,
                        .descriptor_count = 1
                    });
                }
            }

            if (!compatible)
                continue;
            if (residentBytes > hardBudget
                || residentBytes > capabilities.maximum_resident_bytes)
            {
                compatibleButOverBudget = true;
                continue;
            }

            StableHash cacheHash{};
            cacheHash.add_string("epoch.texture.residency.v1");
            cacheHash.add_hash(artifact.key);
            cacheHash.add_integral(kind);
            cacheHash.add_integral(residentBytes);
            cacheHash.add_integral(descriptorCount);
            cacheHash.add_integral(capabilities.sparse_page_extent);
            cacheHash.add_integral(policy.target_resident_bytes);
            for (const ResidencyTier& tier : tiers)
            {
                cacheHash.add_integral(tier.kind);
                cacheHash.add_integral(tier.first_mip);
                cacheHash.add_integral(tier.mip_count);
                cacheHash.add_integral(tier.logical_tile_count);
                cacheHash.add_integral(tier.estimated_resident_bytes);
                cacheHash.add_integral(tier.descriptor_count);
            }
            result.status = ResidencyPlanStatus::ready_for_allocation;
            result.cache_key = cacheHash.finish();
            result.tiers = std::move(tiers);
            result.estimated_resident_bytes = residentBytes;
            result.descriptor_count = descriptorCount;
            return result;
        }

        result.status = compatibleButOverBudget
            ? ResidencyPlanStatus::budget_exceeded
            : ResidencyPlanStatus::no_compatible_strategy;
        return result;
    }

    const char* result_code_name(ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::unchanged: return "unchanged";
        case ResultCode::invalid_document: return "invalid_document";
        case ResultCode::invalid_descriptor: return "invalid_descriptor";
        case ResultCode::invalid_handle: return "invalid_handle";
        case ResultCode::stale_handle: return "stale_handle";
        case ResultCode::invalid_temporal_point:
            return "invalid_temporal_point";
        case ResultCode::temporal_stream_mismatch:
            return "temporal_stream_mismatch";
        case ResultCode::temporal_regression: return "temporal_regression";
        case ResultCode::invalid_operation: return "invalid_operation";
        case ResultCode::unsupported_brush: return "unsupported_brush";
        case ResultCode::layer_limit_exceeded:
            return "layer_limit_exceeded";
        case ResultCode::tile_limit_exceeded: return "tile_limit_exceeded";
        case ResultCode::canonical_memory_budget_exceeded:
            return "canonical_memory_budget_exceeded";
        case ResultCode::history_operation_limit_exceeded:
            return "history_operation_limit_exceeded";
        case ResultCode::history_memory_budget_exceeded:
            return "history_memory_budget_exceeded";
        case ResultCode::checkpoint_limit_exceeded:
            return "checkpoint_limit_exceeded";
        case ResultCode::checkpoint_memory_budget_exceeded:
            return "checkpoint_memory_budget_exceeded";
        case ResultCode::stroke_sample_limit_exceeded:
            return "stroke_sample_limit_exceeded";
        case ResultCode::stroke_tile_limit_exceeded:
            return "stroke_tile_limit_exceeded";
        case ResultCode::nothing_to_undo: return "nothing_to_undo";
        case ResultCode::nothing_to_redo: return "nothing_to_redo";
        case ResultCode::checkpoint_not_found:
            return "checkpoint_not_found";
        case ResultCode::invalid_compile_profile:
            return "invalid_compile_profile";
        case ResultCode::residency_unavailable:
            return "residency_unavailable";
        case ResultCode::arithmetic_overflow: return "arithmetic_overflow";
        }
        return "unknown";
    }

    const char* residency_kind_name(
        PhysicalResidencyKind kind) noexcept
    {
        switch (kind)
        {
        case PhysicalResidencyKind::standalone_image:
            return "standalone_image";
        case PhysicalResidencyKind::atlas_region: return "atlas_region";
        case PhysicalResidencyKind::bindless_image:
            return "bindless_image";
        case PhysicalResidencyKind::sparse_pages: return "sparse_pages";
        case PhysicalResidencyKind::hybrid_sparse_tail:
            return "hybrid_sparse_tail";
        }
        return "unknown";
    }

    const char* residency_plan_status_name(
        ResidencyPlanStatus status) noexcept
    {
        switch (status)
        {
        case ResidencyPlanStatus::ready_for_allocation:
            return "ready_for_allocation";
        case ResidencyPlanStatus::invalid_artifact:
            return "invalid_artifact";
        case ResidencyPlanStatus::invalid_capabilities:
            return "invalid_capabilities";
        case ResidencyPlanStatus::no_compatible_strategy:
            return "no_compatible_strategy";
        case ResidencyPlanStatus::budget_exceeded:
            return "budget_exceeded";
        case ResidencyPlanStatus::arithmetic_overflow:
            return "arithmetic_overflow";
        }
        return "unknown";
    }
}
