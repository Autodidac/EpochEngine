/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

import authoring.texture;

namespace epochengine::authoring::texture::contract
{
    namespace
    {
        template <typename LeftBytes, typename RightBytes>
        [[nodiscard]] bool equal_bytes(
            const LeftBytes& left,
            const RightBytes& right) noexcept
        {
            return left.size() == right.size()
                && (left.empty()
                    || std::memcmp(left.data(), right.data(), left.size()) == 0);
        }

        [[nodiscard]] ContentHash legacy_empty_document_content(
            const DocumentSnapshot& snapshot,
            std::uint32_t sourceSchemaVersion) noexcept
        {
            if (!snapshot.layers.empty()
                || (sourceSchemaVersion != 1u
                    && sourceSchemaVersion != 2u))
            {
                return {};
            }
            constexpr std::uint64_t fnvPrime = 1'099'511'628'211ull;
            std::array<std::uint64_t, 4> words{
                0xcbf29ce484222325ull,
                0x9e3779b97f4a7c15ull,
                0x6a09e667f3bcc909ull,
                0xbb67ae8584caa73bull
            };
            const auto addByte = [&](std::uint8_t value) noexcept
            {
                words[0] ^= value;
                words[0] *= fnvPrime;
                words[1] ^= static_cast<std::uint64_t>(value)
                    + 0x9e3779b97f4a7c15ull
                    + (words[1] << 6)
                    + (words[1] >> 2);
                words[1] = std::rotl(words[1], 17)
                    * 0xbf58476d1ce4e5b9ull;
                words[2] += static_cast<std::uint64_t>(value)
                    + (words[0] ^ std::rotl(words[1], 11));
                words[2] ^= words[2] >> 29;
                words[2] *= 0x94d049bb133111ebull;
                words[3] ^= static_cast<std::uint64_t>(value)
                    + std::rotl(words[0], 7)
                    + std::rotl(words[2], 31);
                words[3] *= 0x9e3779b185ebca87ull;
            };
            const auto addIntegral = [&]<typename Value>(Value value) noexcept
            {
                using Raw = typename std::conditional_t<
                    std::is_enum_v<Value>,
                    std::underlying_type<Value>,
                    std::type_identity<Value>>::type;
                if constexpr (std::is_same_v<std::remove_cv_t<Raw>, bool>)
                {
                    addByte(value ? 1u : 0u);
                }
                else
                {
                    using Unsigned = std::make_unsigned_t<Raw>;
                    Unsigned bits = static_cast<Unsigned>(value);
                    for (std::size_t index = 0u;
                         index < sizeof(Unsigned);
                         ++index)
                    {
                        addByte(static_cast<std::uint8_t>(bits & 0xffu));
                        if constexpr (sizeof(Unsigned) > 1u)
                            bits = static_cast<Unsigned>(bits >> 8u);
                    }
                }
            };
            const auto addString = [&](std::string_view value) noexcept
            {
                addIntegral(value.size());
                for (const char character : value)
                    addByte(static_cast<std::uint8_t>(character));
            };

            addString(sourceSchemaVersion == 1u
                ? "epoch.texture.document.v1"
                : "epoch.texture.document.v2");
            addIntegral(snapshot.descriptor.width);
            addIntegral(snapshot.descriptor.height);
            addIntegral(snapshot.descriptor.tile_extent);
            addIntegral(snapshot.descriptor.mip_count);
            addIntegral(snapshot.descriptor.format);
            addIntegral(snapshot.descriptor.color_space);
            addIntegral(snapshot.descriptor.schema_version);
            addIntegral(snapshot.layers.size());

            ContentHash result{ words };
            for (std::size_t index = 0u;
                 index < result.words.size();
                 ++index)
            {
                std::uint64_t value = result.words[index]
                    ^ std::rotl(
                        result.words[(index + 1u) % result.words.size()],
                        static_cast<int>(13u + index * 7u));
                value ^= value >> 30;
                value *= 0xbf58476d1ce4e5b9ull;
                value ^= value >> 27;
                value *= 0x94d049bb133111ebull;
                value ^= value >> 31;
                result.words[index] = value;
            }
            if (result.empty())
                result.words[0] = 1u;
            return result;
        }

        [[nodiscard]] bool overwrite_serialized_content_hash(
            std::vector<std::byte>& bytes,
            const ContentHash& hash) noexcept
        {
            constexpr std::size_t hashOffset = 12u;
            constexpr std::size_t hashBytes =
                4u * sizeof(std::uint64_t);
            if (bytes.size() < hashOffset + hashBytes)
                return false;
            for (std::size_t wordIndex = 0u;
                 wordIndex < hash.words.size();
                 ++wordIndex)
            {
                for (std::size_t byteIndex = 0u;
                     byteIndex < sizeof(std::uint64_t);
                     ++byteIndex)
                {
                    bytes[hashOffset
                        + wordIndex * sizeof(std::uint64_t)
                        + byteIndex] = static_cast<std::byte>(
                            (hash.words[wordIndex] >> (byteIndex * 8u))
                            & 0xffu);
                }
            }
            return true;
        }

        [[nodiscard]] TextureDocument make_document(
            DocumentHandle handle = { 7, 1 },
            HistoryPolicy history = {},
            DocumentLimits limits = {},
            PixelFormat format = PixelFormat::rgba8_srgb,
            ColorSpace color_space = ColorSpace::srgb)
        {
            CanvasDescriptor canvas{};
            canvas.width = 64;
            canvas.height = 64;
            canvas.tile_extent = 16;
            canvas.mip_count = 3;
            canvas.format = format;
            canvas.color_space = color_space;
            return TextureDocument{
                handle,
                BranchIdentity{ 9, 4 },
                canvas,
                history,
                limits
            };
        }

        [[nodiscard]] TemporalPoint at(std::int64_t tick) noexcept
        {
            return TemporalPoint{ BranchIdentity{ 9, 4 }, tick };
        }

        [[nodiscard]] StrokeDescriptor red_stamp(
            LayerHandle layer,
            std::int64_t x,
            std::int64_t y)
        {
            StrokeDescriptor stroke{};
            stroke.target = layer;
            stroke.color = PixelRgba8{ 255, 32, 16, 255 };
            stroke.radius_subpixels = 512;
            stroke.seed = 42;
            stroke.samples.push_back(StrokeSample{
                .x_subpixels = x * 256 + 128,
                .y_subpixels = y * 256 + 128
            });
            return stroke;
        }
    }

    [[nodiscard]] int run_texture_document_contract()
    {
        if (static_cast<bool>(DocumentHandle{}))
            return 53;
        TextureDocument invalid{
            DocumentHandle{},
            BranchIdentity{ 9, 4 }
        };
        if (invalid.handle())
            return 54;
        if (invalid.valid())
            return 1;
        if (invalid.create_layer({}, 0, at(1)).code
                != ResultCode::invalid_document)
        {
            return 52;
        }

        TextureDocument first = make_document();
        TextureDocument second = make_document();
        if (!first.valid()
            || !second.valid()
            || first.revision().content.empty()
            || content_hash_hex(first.revision().content).size() != 64)
        {
            return 2;
        }

        LayerDescriptor baseDescriptor{};
        baseDescriptor.name = "Base Color";
        const MutationResult firstLayer =
            first.create_layer(baseDescriptor, 0, at(1));
        const MutationResult secondLayer =
            second.create_layer(baseDescriptor, 0, at(1));
        if (!firstLayer
            || !secondLayer
            || !firstLayer.layer
            || firstLayer.layer != secondLayer.layer
            || first.revision().content != second.revision().content)
        {
            return 3;
        }

        const ContentHash clearLayerRevision = first.revision().content;
        const StrokeDescriptor stroke = red_stamp(
            firstLayer.layer,
            15,
            15);
        const MutationResult painted =
            first.apply_stroke(stroke, at(2));
        const MutationResult paintedSecond =
            second.apply_stroke(stroke, at(2));
        if (!painted
            || !paintedSecond
            || painted.affected_tiles != 4
            || first.revision().content != second.revision().content
            || first.pixel(firstLayer.layer, 0, 15, 15).r == 0)
        {
            return 4;
        }

        TextureDocument continuousFirst = make_document({ 16, 1 });
        TextureDocument continuousSecond = make_document({ 16, 1 });
        const LayerHandle continuousLayer =
            continuousFirst.create_layer(
                LayerDescriptor{ .name = "Continuous" },
                0,
                at(1)).layer;
        const LayerHandle continuousLayerSecond =
            continuousSecond.create_layer(
                LayerDescriptor{ .name = "Continuous" },
                0,
                at(1)).layer;
        StrokeDescriptor continuousStroke{};
        continuousStroke.target = continuousLayer;
        continuousStroke.program = BrushProgram::round_path_v2;
        continuousStroke.algorithm_version = 2;
        continuousStroke.color = PixelRgba8{ 80, 160, 240, 255 };
        continuousStroke.radius_subpixels = 256;
        continuousStroke.samples = {
            StrokeSample{
                .x_subpixels = 4 * 256 + 128,
                .y_subpixels = 8 * 256 + 128,
                .pressure = 65'535,
                .tilt_x = -1'024,
                .tilt_y = 512
            },
            StrokeSample{
                .x_subpixels = 12 * 256 + 128,
                .y_subpixels = 8 * 256 + 128,
                .pressure = 32'768,
                .tilt_x = 1'024,
                .tilt_y = -512
            }
        };
        StrokeDescriptor continuousStrokeSecond = continuousStroke;
        continuousStrokeSecond.target = continuousLayerSecond;
        const MutationResult continuousPainted =
            continuousFirst.apply_stroke(continuousStroke, at(2));
        const MutationResult continuousPaintedSecond =
            continuousSecond.apply_stroke(
                continuousStrokeSecond,
                at(2));
        const auto continuousRecords =
            continuousFirst.operation_records();
        const auto* storedContinuous =
            continuousRecords.size() == 2u
            ? std::get_if<TextureStrokeAppliedOperation>(
                &continuousRecords[1].payload)
            : nullptr;
        if (!continuousPainted
            || !continuousPaintedSecond
            || continuousFirst.revision().content
                != continuousSecond.revision().content
            || continuousFirst.pixel(
                    continuousLayer,
                    0,
                    8,
                    8).b == 0u
            || !storedContinuous
            || storedContinuous->stroke.program
                != BrushProgram::round_path_v2
            || storedContinuous->stroke.algorithm_version != 2u
            || storedContinuous->stroke.samples
                != continuousStroke.samples)
        {
            return 39;
        }

        TextureDocument separatedStamps = make_document({ 17, 1 });
        const LayerHandle separatedLayer =
            separatedStamps.create_layer(
                LayerDescriptor{ .name = "Separated" },
                0,
                at(1)).layer;
        StrokeDescriptor separatedStroke = continuousStroke;
        separatedStroke.target = separatedLayer;
        separatedStroke.program = BrushProgram::round_stamp_v1;
        separatedStroke.algorithm_version = 1;
        if (!separatedStamps.apply_stroke(separatedStroke, at(2))
            || separatedStamps.pixel(
                    separatedLayer,
                    0,
                    8,
                    8).b != 0u)
        {
            return 40;
        }

        DocumentLimits pathLimits{};
        pathLimits.maximum_stroke_samples = 4u;
        TextureDocument boundedPath = make_document(
            { 18, 1 },
            HistoryPolicy{},
            pathLimits);
        const LayerHandle boundedPathLayer =
            boundedPath.create_layer(
                LayerDescriptor{ .name = "Bounded Path" },
                0,
                at(1)).layer;
        StrokeDescriptor overBudgetPath = continuousStroke;
        overBudgetPath.target = boundedPathLayer;
        const DocumentRevision beforeRejectedPath =
            boundedPath.revision();
        if (boundedPath.apply_stroke(overBudgetPath, at(2)).code
                != ResultCode::stroke_sample_limit_exceeded
            || boundedPath.revision() != beforeRejectedPath
            || boundedPath.metrics().sparse_tile_count != 0u)
        {
            return 41;
        }

        TextureDocument softBrush = make_document({ 19, 1 });
        TextureDocument hardBrush = make_document({ 20, 1 });
        const LayerHandle softLayer = softBrush.create_layer(
            LayerDescriptor{ .name = "Soft Brush" },
            0,
            at(1)).layer;
        const LayerHandle hardLayer = hardBrush.create_layer(
            LayerDescriptor{ .name = "Hard Brush" },
            0,
            at(1)).layer;
        StrokeDescriptor softStroke = red_stamp(softLayer, 16, 16);
        softStroke.color = PixelRgba8{ 200, 120, 80, 255 };
        softStroke.opacity = 32'768u;
        softStroke.hardness = 0u;
        softStroke.channel_mask = 0x05u;
        StrokeDescriptor hardStroke = softStroke;
        hardStroke.target = hardLayer;
        hardStroke.hardness = 65'535u;
        const MutationResult softPainted =
            softBrush.apply_stroke(softStroke, at(2));
        const MutationResult hardPainted =
            hardBrush.apply_stroke(hardStroke, at(2));
        const PixelRgba8 softEdge = softBrush.pixel(
            softLayer, 0, 17, 16);
        const PixelRgba8 hardEdge = hardBrush.pixel(
            hardLayer, 0, 17, 16);
        const auto softRecords = softBrush.operation_records();
        const auto* storedSoftStroke = softRecords.size() == 2u
            ? std::get_if<TextureStrokeAppliedOperation>(
                &softRecords[1].payload)
            : nullptr;
        if (!softPainted
            || !hardPainted
            || softEdge.r == 0u
            || softEdge.r >= hardEdge.r
            || softEdge.b == 0u
            || softEdge.g != 0u
            || softEdge.a != 0u
            || !storedSoftStroke
            || storedSoftStroke->stroke.opacity != 32'768u
            || storedSoftStroke->stroke.hardness != 0u
            || storedSoftStroke->stroke.channel_mask != 0x05u)
        {
            return 42;
        }

        TextureDocument invalidMask = make_document({ 21, 1 });
        const LayerHandle invalidMaskLayer = invalidMask.create_layer(
            LayerDescriptor{ .name = "Invalid Mask" },
            0,
            at(1)).layer;
        StrokeDescriptor noChannels = red_stamp(
            invalidMaskLayer,
            16,
            16);
        noChannels.channel_mask = 0u;
        const DocumentRevision beforeInvalidMask = invalidMask.revision();
        if (invalidMask.apply_stroke(noChannels, at(2)).code
                != ResultCode::invalid_descriptor
            || invalidMask.revision() != beforeInvalidMask
            || invalidMask.metrics().sparse_tile_count != 0u)
        {
            return 43;
        }

        TextureDocument layerEffects = make_document({ 22, 1 });
        const LayerHandle effectLayer = layerEffects.create_layer(
            LayerDescriptor{ .name = "Effects" },
            0,
            at(1)).layer;
        const StrokeDescriptor effectStroke = red_stamp(
            effectLayer,
            10,
            12);
        if (!effectLayer
            || !layerEffects.apply_stroke(effectStroke, at(2)))
        {
            return 44;
        }
        const PixelRgba8 canonicalBeforeEffects =
            layerEffects.pixel(effectLayer, 0, 10, 12);
        LayerDescriptor effectDescriptor =
            layerEffects.layer(effectLayer)->descriptor;
        effectDescriptor.transform.offset_x_pixels = 3;
        effectDescriptor.transform.offset_y_pixels = -2;
        effectDescriptor.transform.mirror_x = true;
        effectDescriptor.filter.brightness = 10;
        effectDescriptor.filter.grayscale = true;
        effectDescriptor.filter.invert = true;
        const MutationResult effectsChanged =
            layerEffects.set_layer_properties(
                effectLayer,
                effectDescriptor,
                at(3));
        const auto effectRecords = layerEffects.operation_records();
        const auto* effectProperties = effectRecords.size() == 3u
            ? std::get_if<LayerPropertiesChangedOperation>(
                &effectRecords.back().payload)
            : nullptr;
        if (!effectsChanged
            || layerEffects.pixel(effectLayer, 0, 10, 12)
                != canonicalBeforeEffects
            || !effectProperties
            || effectProperties->after != effectDescriptor
            || effectProperties->before.transform
                != LayerTransformDescriptor{}
            || effectProperties->before.filter
                != LayerFilterDescriptor{})
        {
            return 44;
        }

        TextureCompileProfile effectProfile{};
        const CompiledTextureArtifact compiledEffects =
            layerEffects.compile_artifact(effectProfile);
        if (!compiledEffects || compiledEffects.mips.empty())
            return 45;
        const std::size_t transformedOffset =
            static_cast<std::size_t>(10u
                * compiledEffects.mips[0].row_pitch_bytes)
            + 56u * 4u;
        const std::size_t originalOffset =
            static_cast<std::size_t>(12u
                * compiledEffects.mips[0].row_pitch_bytes)
            + 10u * 4u;
        if (transformedOffset + 3u
                >= compiledEffects.mips[0].texels.size()
            || originalOffset + 3u
                >= compiledEffects.mips[0].texels.size()
            || std::to_integer<std::uint8_t>(
                compiledEffects.mips[0].texels[transformedOffset]) != 168u
            || std::to_integer<std::uint8_t>(
                compiledEffects.mips[0].texels[transformedOffset + 1u])
                != 168u
            || std::to_integer<std::uint8_t>(
                compiledEffects.mips[0].texels[transformedOffset + 2u])
                != 168u
            || std::to_integer<std::uint8_t>(
                compiledEffects.mips[0].texels[transformedOffset + 3u])
                != 255u
            || std::to_integer<std::uint8_t>(
                compiledEffects.mips[0].texels[originalOffset + 3u]) != 0u)
        {
            return 45;
        }

        const DocumentRevision effectsRevision = layerEffects.revision();
        if (!layerEffects.undo(at(4)))
            return 46;
        const CompiledTextureArtifact effectsUndone =
            layerEffects.compile_artifact(effectProfile);
        if (!effectsUndone
            || effectsUndone.identity.key == compiledEffects.identity.key
            || std::to_integer<std::uint8_t>(
                effectsUndone.mips[0].texels[originalOffset + 3u]) == 0u
            || !layerEffects.redo(at(5)))
        {
            return 46;
        }
        const CompiledTextureArtifact effectsRedone =
            layerEffects.compile_artifact(effectProfile);
        if (!effectsRedone
            || layerEffects.revision().content
                != effectsRevision.content
            || effectsRedone.identity.key != compiledEffects.identity.key
            || effectsRedone.payload_content
                != compiledEffects.payload_content
            || !equal_bytes(
                effectsRedone.mips[0].texels,
                compiledEffects.mips[0].texels))
        {
            return 46;
        }

        LayerDescriptor invalidEffectDescriptor = effectDescriptor;
        invalidEffectDescriptor.filter.brightness = 256;
        const DocumentRevision beforeInvalidEffect =
            layerEffects.revision();
        if (layerEffects.set_layer_properties(
                effectLayer,
                invalidEffectDescriptor,
                at(6)).code != ResultCode::invalid_descriptor
            || layerEffects.revision() != beforeInvalidEffect)
        {
            return 47;
        }

        const SnapshotSerializationResult serializedEffects =
            layerEffects.serialize();
        const RestoreResult reopenedEffects = serializedEffects
            ? TextureDocument::deserialize(serializedEffects.bytes)
            : RestoreResult{};
        if (!serializedEffects
            || !reopenedEffects
            || reopenedEffects.document->snapshot()
                != layerEffects.snapshot()
            || reopenedEffects.document->layer(effectLayer)->descriptor
                != effectDescriptor
            || reopenedEffects.document->compile_artifact(
                    effectProfile).payload_content
                != compiledEffects.payload_content)
        {
            return 48;
        }

        TextureDocument maskedLayers = make_document({ 24, 1 });
        const LayerHandle maskedContent =
            maskedLayers.create_layer(
                LayerDescriptor{ .name = "Masked Content" },
                0,
                at(1)).layer;
        StrokeDescriptor maskedContentStroke = red_stamp(
            maskedContent,
            20,
            20);
        maskedContentStroke.radius_subpixels = 128u;
        const MutationResult paintedMaskedContent =
            maskedLayers.apply_stroke(maskedContentStroke, at(2));
        TextureCompileProfile maskProfile{};
        const CompiledTextureArtifact unmaskedArtifact =
            maskedLayers.compile_artifact(maskProfile);
        const std::size_t maskedPixelOffset =
            unmaskedArtifact.mips.empty()
                ? 0u
                : static_cast<std::size_t>(20u
                    * unmaskedArtifact.mips[0].row_pitch_bytes)
                    + 20u * 4u;
        if (!maskedContent
            || !paintedMaskedContent
            || paintedMaskedContent.affected_tiles != 1u
            || !unmaskedArtifact
            || unmaskedArtifact.mips.empty()
            || maskedPixelOffset + 3u
                >= unmaskedArtifact.mips[0].texels.size()
            || std::to_integer<std::uint8_t>(
                unmaskedArtifact.mips[0].texels[
                    maskedPixelOffset + 3u]) != 255u)
        {
            return 60;
        }

        LayerDescriptor maskDescriptor{};
        maskDescriptor.name = "Spatial Mask";
        maskDescriptor.role = LayerRole::mask;
        const MutationResult createdMask =
            maskedLayers.create_layer(maskDescriptor, 1, at(3));
        const auto emptyMaskInfo =
            maskedLayers.layer(createdMask.layer);
        if (!createdMask
            || !createdMask.layer
            || !emptyMaskInfo
            || emptyMaskInfo->descriptor != maskDescriptor
            || emptyMaskInfo->stored_tile_count != 0u
            || emptyMaskInfo->stored_bytes != 0u
            || maskedLayers.pixel(
                createdMask.layer,
                0,
                20,
                20) != PixelRgba8{})
        {
            return 61;
        }

        LayerDescriptor maskedContentDescriptor =
            maskedLayers.layer(maskedContent)->descriptor;
        maskedContentDescriptor.mask.source = createdMask.layer;
        if (!maskedLayers.set_layer_properties(
                maskedContent,
                maskedContentDescriptor,
                at(4)))
        {
            return 62;
        }

        StrokeDescriptor maskStroke{};
        maskStroke.target = createdMask.layer;
        maskStroke.color = PixelRgba8{ 0, 0, 0, 255 };
        maskStroke.radius_subpixels = 128u;
        maskStroke.channel_mask = 0x08u;
        maskStroke.samples.push_back(StrokeSample{
            .x_subpixels = 20 * 256 + 128,
            .y_subpixels = 20 * 256 + 128
        });
        StrokeDescriptor invalidMaskStroke = maskStroke;
        invalidMaskStroke.channel_mask = 0x0fu;
        const DocumentRevision beforeInvalidMaskStroke =
            maskedLayers.revision();
        const DocumentMetrics beforeMaskStroke =
            maskedLayers.metrics();
        if (maskedLayers.apply_stroke(
                invalidMaskStroke,
                at(5)).code != ResultCode::invalid_descriptor
            || maskedLayers.revision() != beforeInvalidMaskStroke
            || maskedLayers.metrics().canonical_tile_bytes
                != beforeMaskStroke.canonical_tile_bytes)
        {
            return 63;
        }

        const MutationResult paintedMask =
            maskedLayers.apply_stroke(maskStroke, at(5));
        const auto maskInfo = maskedLayers.layer(createdMask.layer);
        const auto maskTile = maskedLayers.tile(
            createdMask.layer,
            TileCoordinate{ 0, 1, 1 });
        const auto maskRecords = maskedLayers.operation_records();
        const auto* storedMaskStroke = maskRecords.empty()
            ? nullptr
            : std::get_if<TextureStrokeAppliedOperation>(
                &maskRecords.back().payload);
        if (!paintedMask
            || paintedMask.affected_tiles != 1u
            || !maskInfo
            || maskInfo->descriptor.role != LayerRole::mask
            || maskInfo->stored_tile_count != 1u
            || maskInfo->stored_bytes != 16u * 16u
            || !maskTile
            || maskTile->texels.size() != 16u * 16u
            || maskTile->content.empty()
            || maskedLayers.pixel(
                createdMask.layer,
                0,
                20,
                20) != PixelRgba8{ 255, 255, 255, 255 }
            || maskedLayers.metrics().canonical_tile_bytes
                != beforeMaskStroke.canonical_tile_bytes + 16u * 16u
            || !storedMaskStroke
            || storedMaskStroke->stroke.channel_mask != 0x08u
            || storedMaskStroke->stroke.color.a != 255u)
        {
            return 64;
        }

        const CompiledTextureArtifact concealedArtifact =
            maskedLayers.compile_artifact(maskProfile);
        if (!concealedArtifact
            || concealedArtifact.mips.empty()
            || std::to_integer<std::uint8_t>(
                concealedArtifact.mips[0].texels[
                    maskedPixelOffset + 3u]) != 0u
            || maskedLayers.pixel(
                maskedContent,
                0,
                20,
                20).a != 255u)
        {
            return 65;
        }

        const DocumentRevision concealedRevision =
            maskedLayers.revision();
        if (!maskedLayers.undo(at(6))
            || maskedLayers.pixel(
                createdMask.layer,
                0,
                20,
                20) != PixelRgba8{}
            || maskedLayers.metrics().canonical_tile_bytes
                != beforeMaskStroke.canonical_tile_bytes
            || !maskedLayers.can_redo())
        {
            return 66;
        }
        const CompiledTextureArtifact revealedAfterUndo =
            maskedLayers.compile_artifact(maskProfile);
        if (!revealedAfterUndo
            || std::to_integer<std::uint8_t>(
                revealedAfterUndo.mips[0].texels[
                    maskedPixelOffset + 3u]) != 255u
            || !equal_bytes(
                revealedAfterUndo.mips[0].texels,
                unmaskedArtifact.mips[0].texels))
        {
            return 67;
        }

        if (!maskedLayers.redo(at(7))
            || maskedLayers.revision().content
                != concealedRevision.content)
        {
            return 68;
        }
        const CompiledTextureArtifact concealedAfterRedo =
            maskedLayers.compile_artifact(maskProfile);
        if (!concealedAfterRedo
            || concealedAfterRedo.identity.key
                != concealedArtifact.identity.key
            || concealedAfterRedo.payload_content
                != concealedArtifact.payload_content
            || !equal_bytes(
                concealedAfterRedo.mips[0].texels,
                concealedArtifact.mips[0].texels))
        {
            return 69;
        }

        maskedContentDescriptor =
            maskedLayers.layer(maskedContent)->descriptor;
        maskedContentDescriptor.mask.strength = 32'768u;
        if (!maskedLayers.set_layer_properties(
                maskedContent,
                maskedContentDescriptor,
                at(8)))
        {
            return 70;
        }
        const CompiledTextureArtifact partialMaskArtifact =
            maskedLayers.compile_artifact(maskProfile);
        if (!partialMaskArtifact
            || std::to_integer<std::uint8_t>(
                partialMaskArtifact.mips[0].texels[
                    maskedPixelOffset + 3u]) != 127u)
        {
            return 71;
        }

        maskedContentDescriptor.mask.invert = true;
        if (!maskedLayers.set_layer_properties(
                maskedContent,
                maskedContentDescriptor,
                at(9)))
        {
            return 72;
        }
        const CompiledTextureArtifact invertedMaskArtifact =
            maskedLayers.compile_artifact(maskProfile);
        if (!invertedMaskArtifact
            || std::to_integer<std::uint8_t>(
                invertedMaskArtifact.mips[0].texels[
                    maskedPixelOffset + 3u]) != 255u)
        {
            return 73;
        }

        const DocumentRevision beforeInvalidMaskBindings =
            maskedLayers.revision();
        const DocumentMetrics beforeInvalidMaskBindingsMetrics =
            maskedLayers.metrics();
        LayerDescriptor changedMaskRole =
            maskedLayers.layer(createdMask.layer)->descriptor;
        changedMaskRole.role = LayerRole::content;
        LayerDescriptor invalidMaskProperties =
            maskedLayers.layer(createdMask.layer)->descriptor;
        invalidMaskProperties.opacity = 65'534u;
        LayerDescriptor selfMask = maskedContentDescriptor;
        selfMask.mask.source = maskedContent;
        LayerDescriptor staleMask = maskedContentDescriptor;
        ++staleMask.mask.source.generation;
        if (staleMask.mask.source.generation == 0u)
            staleMask.mask.source.generation = 1u;
        if (maskedLayers.set_layer_properties(
                createdMask.layer,
                changedMaskRole,
                at(10)).code != ResultCode::invalid_descriptor
            || maskedLayers.set_layer_properties(
                createdMask.layer,
                invalidMaskProperties,
                at(10)).code != ResultCode::invalid_descriptor
            || maskedLayers.set_layer_properties(
                maskedContent,
                selfMask,
                at(10)).code != ResultCode::invalid_descriptor
            || maskedLayers.set_layer_properties(
                maskedContent,
                staleMask,
                at(10)).code != ResultCode::invalid_descriptor
            || maskedLayers.revision() != beforeInvalidMaskBindings
            || maskedLayers.metrics().canonical_tile_bytes
                != beforeInvalidMaskBindingsMetrics.canonical_tile_bytes)
        {
            return 74;
        }

        if (maskedLayers.remove_layer(
                createdMask.layer,
                at(10)).code != ResultCode::invalid_operation
            || maskedLayers.revision() != beforeInvalidMaskBindings)
        {
            return 75;
        }

        const SnapshotSerializationResult serializedMask =
            maskedLayers.serialize();
        const RestoreResult reopenedMask = serializedMask
            ? TextureDocument::deserialize(serializedMask.bytes)
            : RestoreResult{};
        const CompiledTextureArtifact reopenedMaskArtifact = reopenedMask
            ? reopenedMask.document->compile_artifact(maskProfile)
            : CompiledTextureArtifact{};
        if (!serializedMask
            || serializedMask.bytes.size() <= 8u
            || serializedMask.bytes[8] != std::byte{ 3 }
            || !reopenedMask
            || reopenedMask.document->snapshot()
                != maskedLayers.snapshot()
            || reopenedMask.document->layer(
                createdMask.layer)->descriptor.role != LayerRole::mask
            || reopenedMask.document->layer(
                maskedContent)->descriptor.mask.source
                != createdMask.layer
            || reopenedMask.document->pixel(
                createdMask.layer,
                0,
                20,
                20) != PixelRgba8{ 255, 255, 255, 255 }
            || !reopenedMaskArtifact
            || reopenedMaskArtifact.identity.key
                != invertedMaskArtifact.identity.key
            || reopenedMaskArtifact.payload_content
                != invertedMaskArtifact.payload_content
            || !equal_bytes(
                reopenedMaskArtifact.mips[0].texels,
                invertedMaskArtifact.mips[0].texels))
        {
            return 76;
        }

        DocumentSnapshot staleMaskSnapshot = maskedLayers.snapshot();
        bool changedStaleBinding{};
        for (LayerSnapshot& layer : staleMaskSnapshot.layers)
        {
            if (layer.handle != maskedContent)
                continue;
            ++layer.descriptor.mask.source.generation;
            if (layer.descriptor.mask.source.generation == 0u)
                layer.descriptor.mask.source.generation = 1u;
            changedStaleBinding = true;
            break;
        }
        if (!changedStaleBinding
            || TextureDocument::restore(staleMaskSnapshot).code
                != ResultCode::invalid_descriptor)
        {
            return 77;
        }

        DocumentSnapshot corruptMaskSnapshot = maskedLayers.snapshot();
        bool corruptedMaskTile{};
        for (LayerSnapshot& layer : corruptMaskSnapshot.layers)
        {
            if (layer.handle != createdMask.layer
                || layer.tiles.empty()
                || layer.tiles.front().texels.empty())
            {
                continue;
            }
            layer.tiles.front().texels.pop_back();
            corruptedMaskTile = true;
            break;
        }
        if (!corruptedMaskTile
            || TextureDocument::restore(corruptMaskSnapshot).code
                != ResultCode::integrity_failure)
        {
            return 78;
        }

        std::vector<std::byte> corruptSerializedMask =
            serializedMask.bytes;
        corruptSerializedMask[12] ^= std::byte{ 0x1 };
        if (TextureDocument::deserialize(
                corruptSerializedMask).code
                != ResultCode::integrity_failure)
        {
            return 79;
        }

        const DocumentMetrics paintedMetrics = first.metrics();
        if (paintedMetrics.sparse_tile_count != 4
            || paintedMetrics.canonical_tile_bytes
                != 4ull * 16ull * 16ull * 4ull
            || paintedMetrics.operation_count != 2
            || paintedMetrics.applied_operation_count != 2)
        {
            return 5;
        }

        const auto tile = first.tile(
            firstLayer.layer,
            TileCoordinate{ 0, 0, 0 });
        const auto implicitTile = first.tile(
            firstLayer.layer,
            TileCoordinate{ 1, 0, 0 });
        if (!tile
            || tile->implicit_clear()
            || tile->content.empty()
            || !implicitTile
            || !implicitTile->implicit_clear()
            || implicitTile->content.empty())
        {
            return 6;
        }

        const ContentHash paintedRevision = first.revision().content;
        const MutationResult undone = first.undo(at(3));
        if (!undone
            || first.revision().content != clearLayerRevision
            || first.metrics().sparse_tile_count != 0
            || !first.can_redo())
        {
            return 7;
        }
        const MutationResult redone = first.redo(at(4));
        if (!redone
            || first.revision().content != paintedRevision
            || first.metrics().sparse_tile_count != 4)
        {
            return 8;
        }

        const CheckpointResult checkpoint =
            first.capture_checkpoint(at(4));
        if (!checkpoint
            || !checkpoint.checkpoint.handle
            || checkpoint.checkpoint.retained_bytes == 0)
        {
            return 9;
        }

        LayerDescriptor changed = baseDescriptor;
        changed.name = "Paint";
        changed.opacity = 32'768;
        const MutationResult changedProperties =
            first.set_layer_properties(firstLayer.layer, changed, at(5));
        if (!changedProperties
            || first.revision().content == paintedRevision)
        {
            return 10;
        }
        const MutationResult restored = first.restore_checkpoint(
            checkpoint.checkpoint.handle,
            at(6));
        if (!restored
            || first.revision().content != paintedRevision
            || !first.layer(firstLayer.layer)
            || first.layer(firstLayer.layer)->descriptor.name
                != baseDescriptor.name
            || first.layer(firstLayer.layer)->descriptor.opacity
                != baseDescriptor.opacity)
        {
            return 11;
        }

        const auto summaries = first.operation_summaries();
        const auto records = first.operation_records();
        if (summaries.size() != records.size()
            || summaries.size() != 3
            || summaries[0].kind != std::string_view{ "layer_created" }
            || summaries[1].kind
                != std::string_view{ "texture_stroke_applied" }
            || records[1].retained_bytes == 0)
        {
            return 12;
        }

        TextureDocument layerOrder = make_document({ 8, 1 });
        const LayerHandle bottom =
            layerOrder.create_layer(
                LayerDescriptor{ .name = "Bottom" },
                0,
                at(1)).layer;
        const LayerHandle top =
            layerOrder.create_layer(
                LayerDescriptor{ .name = "Top" },
                1,
                at(2)).layer;
        if (!bottom
            || !top
            || !layerOrder.move_layer(top, 0, at(3))
            || layerOrder.layers()[0].handle != top
            || !layerOrder.undo(at(4))
            || layerOrder.layers()[0].handle != bottom)
        {
            return 13;
        }

        const MutationResult removed =
            layerOrder.remove_layer(top, at(5));
        if (!removed
            || layerOrder.layer(top)
            || layerOrder.set_layer_properties(
                    top,
                    LayerDescriptor{ .name = "Stale" },
                    at(6)).code != ResultCode::stale_handle)
        {
            return 14;
        }
        if (!layerOrder.undo(at(6)) || !layerOrder.layer(top))
            return 15;

        HistoryPolicy shortHistory{};
        shortHistory.mode = HistoryMode::semantic_operations;
        shortHistory.maximum_operations = 1;
        TextureDocument boundedHistory =
            make_document({ 9, 1 }, shortHistory);
        const MutationResult onlyOperation =
            boundedHistory.create_layer(
                LayerDescriptor{ .name = "Only" },
                0,
                at(1));
        if (!onlyOperation
            || boundedHistory.set_layer_properties(
                    onlyOperation.layer,
                    LayerDescriptor{ .name = "Too Many" },
                    at(2)).code
                != ResultCode::history_operation_limit_exceeded)
        {
            return 16;
        }

        DocumentLimits oneTileLimits{};
        oneTileLimits.maximum_sparse_tiles = 1;
        oneTileLimits.maximum_tiles_per_stroke = 1;
        TextureDocument boundedTiles = make_document(
            { 10, 1 },
            HistoryPolicy{},
            oneTileLimits);
        const LayerHandle boundedLayer =
            boundedTiles.create_layer(
                LayerDescriptor{ .name = "Bounded" },
                0,
                at(1)).layer;
        StrokeDescriptor crossing = red_stamp(boundedLayer, 15, 15);
        if (boundedTiles.apply_stroke(crossing, at(2)).code
            != ResultCode::stroke_tile_limit_exceeded)
        {
            return 17;
        }
        if (boundedTiles.metrics().sparse_tile_count != 0)
            return 18;

        TextureCompileProfile compileProfile{};
        compileProfile.format = ArtifactFormat::bc7_rgba;
        compileProfile.mipmaps = MipmapPolicy::generate_box_filter;
        compileProfile.compiler_schema_version = 3;
        const DocumentRevision beforeCompile = first.revision();
        const CompiledTextureArtifactIdentity artifact =
            first.compiled_artifact_identity(compileProfile);
        const CompiledTextureArtifactIdentity repeatedArtifact =
            first.compiled_artifact_identity(compileProfile);
        if (!artifact
            || artifact.key != repeatedArtifact.key
            || artifact.mip_count != 7
            || artifact.estimated_artifact_bytes == 0
            || first.revision() != beforeCompile)
        {
            return 19;
        }

        PhysicalResidencyCapabilities atlasCapabilities{};
        atlasCapabilities.atlas_regions = true;
        PhysicalResidencyPolicy atlasPolicy{};
        atlasPolicy.preferred = PhysicalResidencyKind::atlas_region;
        atlasPolicy.allow_atlas_for_small_textures = true;
        const PhysicalResidencyPlan atlasPlan =
            plan_physical_residency(
                artifact,
                atlasCapabilities,
                atlasPolicy);
        if (!atlasPlan
            || atlasPlan.tiers.size() != 1
            || atlasPlan.tiers[0].kind
                != PhysicalResidencyKind::atlas_region
            || !atlasPlan.disposable
            || atlasPlan.cache_key.empty()
            || first.revision() != beforeCompile)
        {
            return 20;
        }

        PhysicalResidencyCapabilities sparseCapabilities{};
        sparseCapabilities.bindless_images = true;
        sparseCapabilities.sparse_images = true;
        sparseCapabilities.available_bindless_descriptors = 128;
        sparseCapabilities.maximum_resident_bytes = 64 * 1024;
        PhysicalResidencyPolicy sparsePolicy{};
        sparsePolicy.preferred =
            PhysicalResidencyKind::hybrid_sparse_tail;
        sparsePolicy.allow_atlas_for_small_textures = false;
        sparsePolicy.target_resident_bytes = 64 * 1024;
        sparsePolicy.minimum_streamed_mip_count = 2;
        const PhysicalResidencyPlan sparsePlan =
            plan_physical_residency(
                artifact,
                sparseCapabilities,
                sparsePolicy);
        if (!sparsePlan
            || sparsePlan.tiers.size() != 2
            || sparsePlan.tiers[0].kind
                != PhysicalResidencyKind::sparse_pages
            || sparsePlan.tiers[1].kind
                != PhysicalResidencyKind::standalone_image
            || sparsePlan.estimated_resident_bytes
                > sparsePolicy.target_resident_bytes)
        {
            return 21;
        }

        if (std::string_view{
                result_code_name(ResultCode::stale_handle) }
                != "stale_handle"
            || std::string_view{
                residency_kind_name(
                    PhysicalResidencyKind::hybrid_sparse_tail) }
                != "hybrid_sparse_tail"
            || std::string_view{
                residency_plan_status_name(
                    ResidencyPlanStatus::ready_for_allocation) }
                != "ready_for_allocation")
        {
            return 22;
        }

        HistoryPolicy automaticPolicy{};
        automaticPolicy.checkpoint_interval_operations = 1;
        automaticPolicy.maximum_checkpoints = 1;
        TextureDocument automaticCheckpoints = make_document(
            { 11, 1 },
            automaticPolicy);
        const MutationResult automaticLayer =
            automaticCheckpoints.create_layer(
                LayerDescriptor{ .name = "Automatic" },
                0,
                at(1));
        LayerDescriptor automaticChanged{};
        automaticChanged.name = "Automatic Changed";
        const MutationResult skippedAutomatic =
            automaticCheckpoints.set_layer_properties(
                automaticLayer.layer,
                automaticChanged,
                at(2));
        if (!automaticLayer.automatic_checkpoint_captured
            || skippedAutomatic.automatic_checkpoint_captured
            || automaticCheckpoints.metrics().checkpoint_count != 1
            || automaticCheckpoints.metrics().automatic_checkpoints_skipped
                != 1)
        {
            return 23;
        }

        TextureDocument checkpointTime = make_document({ 12, 1 });
        const LayerHandle checkpointLayer =
            checkpointTime.create_layer(
                LayerDescriptor{ .name = "Temporal" },
                0,
                at(1)).layer;
        if (!checkpointTime.capture_checkpoint(at(10))
            || checkpointTime.current_time().tick != 10
            || checkpointTime.set_layer_properties(
                    checkpointLayer,
                    LayerDescriptor{ .name = "Regression" },
                    at(9)).code != ResultCode::temporal_regression)
        {
            return 24;
        }

        HistoryPolicy disabledPolicy{};
        disabledPolicy.mode = HistoryMode::disabled;
        TextureDocument noHistory = make_document(
            { 13, 1 },
            disabledPolicy);
        if (!noHistory.create_layer(
                LayerDescriptor{ .name = "No History" },
                0,
                at(1))
            || noHistory.metrics().operation_count != 0
            || noHistory.can_undo()
            || noHistory.undo(at(2)).code != ResultCode::nothing_to_undo)
        {
            return 25;
        }

        CompiledTextureArtifactIdentity inconsistentArtifact = artifact;
        ++inconsistentArtifact.estimated_artifact_bytes;
        if (plan_physical_residency(
                inconsistentArtifact,
                atlasCapabilities,
                atlasPolicy).status != ResidencyPlanStatus::invalid_artifact)
        {
            return 26;
        }

        TextureCompileProfile rasterProfile{};
        rasterProfile.compiler_schema_version = 4;
        const CompiledTextureArtifact compiled =
            first.compile_artifact(rasterProfile);
        const CompiledTextureArtifact repeatedCompiled =
            first.compile_artifact(rasterProfile);
        if (!compiled
            || !repeatedCompiled
            || compiled.mips.empty()
            || repeatedCompiled.mips.empty())
        {
            return 27;
        }
        const std::size_t paintedOffset =
            static_cast<std::size_t>(15u * compiled.mips[0].row_pitch_bytes)
            + 15u * 4u;
        if (!validate_compiled_artifact(compiled)
            || !validate_compiled_artifact(repeatedCompiled)
            || compiled.identity.compilation_required
            || repeatedCompiled.identity.compilation_required
            || compiled.identity.key != repeatedCompiled.identity.key
            || compiled.payload_content != repeatedCompiled.payload_content
            || compiled.mips.size() != 3u
            || !equal_bytes(
                compiled.mips[0].texels, repeatedCompiled.mips[0].texels)
            || compiled.identity.estimated_artifact_bytes != 21'504u
            || paintedOffset + 3u >= compiled.mips[0].texels.size()
            || std::to_integer<std::uint8_t>(
                compiled.mips[0].texels[paintedOffset + 3u]) == 0u
            || first.revision() != beforeCompile)
        {
            return 27;
        }

        TextureDocument linearDocument = make_document(
            { 11, 1 },
            {},
            {},
            PixelFormat::rgba8_unorm,
            ColorSpace::linear);
        TextureCompileProfile generatedProfile{};
        generatedProfile.format = ArtifactFormat::rgba8_unorm;
        generatedProfile.color_space = ColorSpace::linear;
        generatedProfile.mipmaps = MipmapPolicy::generate_box_filter;
        const CompiledTextureArtifact generated =
            linearDocument.compile_artifact(generatedProfile);
        if (!generated
            || generated.mips.size() != 7u
            || generated.mips.back().width != 1u
            || generated.mips.back().height != 1u
            || !generated.mips.back().valid())
        {
            return 28;
        }

        ArtifactCompilationLimits tinyCompileBudget{};
        tinyCompileBudget.maximum_output_bytes = 64u;
        if (first.compile_artifact(
                rasterProfile,
                tinyCompileBudget).status
                != ArtifactCompilationStatus::output_budget_exceeded)
        {
            return 29;
        }
        if (first.compile_artifact(compileProfile).status
                != ArtifactCompilationStatus::unsupported_format)
        {
            return 30;
        }
        TextureCompileProfile normalProfile = rasterProfile;
        normalProfile.mipmaps = MipmapPolicy::generate_normal_renormalized;
        if (first.compile_artifact(normalProfile).status
                != ArtifactCompilationStatus::unsupported_mipmap_policy)
        {
            return 31;
        }
        TextureCompileProfile srgbGeneratedProfile = rasterProfile;
        srgbGeneratedProfile.mipmaps = MipmapPolicy::generate_box_filter;
        if (first.compile_artifact(srgbGeneratedProfile).status
                != ArtifactCompilationStatus::unsupported_mipmap_policy)
        {
            return 31;
        }
        TextureCompileProfile conversionProfile = rasterProfile;
        conversionProfile.format = ArtifactFormat::rgba8_unorm;
        conversionProfile.color_space = ColorSpace::linear;
        if (first.compile_artifact(conversionProfile).status
                != ArtifactCompilationStatus::unsupported_color_conversion
            || std::string_view{ artifact_compilation_status_name(
                    ArtifactCompilationStatus::ready) } != "ready")
        {
            return 32;
        }

        CompiledTextureArtifact corruptedCompiled = compiled;
        corruptedCompiled.mips[0].texels[paintedOffset] ^= std::byte{ 0x1 };
        if (validate_compiled_artifact(corruptedCompiled)
            || !validate_compiled_artifact(compiled))
        {
            return 33;
        }

        TextureDocument mismatchedStorage = make_document(
            { 12, 1 },
            {},
            {},
            PixelFormat::rgba8_srgb,
            ColorSpace::linear);
        if (mismatchedStorage.compile_artifact(generatedProfile).status
                != ArtifactCompilationStatus::unsupported_color_conversion)
        {
            return 34;
        }
        TextureCompileProfile invalidMipmap = generatedProfile;
        invalidMipmap.mipmaps = static_cast<MipmapPolicy>(255);
        if (linearDocument.compile_artifact(invalidMipmap).status
                != ArtifactCompilationStatus::unsupported_mipmap_policy)
        {
            return 35;
        }

        const SnapshotSerializationResult serialized = first.serialize();
        const RestoreResult reopened = serialized
            ? TextureDocument::deserialize(serialized.bytes)
            : RestoreResult{};
        if (!serialized
            || !reopened
            || reopened.document->snapshot() != first.snapshot()
            || reopened.document->serialize().bytes != serialized.bytes)
        {
            return 36;
        }

        const CompiledTextureArtifact reopenedArtifact =
            reopened.document->compile_artifact(rasterProfile);
        if (!reopenedArtifact
            || reopenedArtifact.identity.key != compiled.identity.key
            || reopenedArtifact.identity.source_revision
                != compiled.identity.source_revision
            || reopenedArtifact.payload_content != compiled.payload_content
            || reopenedArtifact.mips.size() != compiled.mips.size()
            || !equal_bytes(
                reopenedArtifact.mips.front().texels,
                compiled.mips.front().texels))
        {
            return 37;
        }

        std::vector<std::byte> corruptedSource = serialized.bytes;
        corruptedSource.back() ^= std::byte{0x1};
        std::vector<std::byte> truncatedSource = serialized.bytes;
        truncatedSource.pop_back();
        std::vector<std::byte> futureSource = serialized.bytes;
        futureSource[8] = std::byte{4};
        std::vector<std::byte> mismatchedExtension = serialized.bytes;
        if (mismatchedExtension.size() <= 20u)
            return 49;
        mismatchedExtension[mismatchedExtension.size() - 20u]
            ^= std::byte{0x1};
        if (TextureDocument::deserialize(corruptedSource).code
                != ResultCode::integrity_failure
            || TextureDocument::deserialize(truncatedSource).code
                != ResultCode::malformed_payload
            || TextureDocument::deserialize(futureSource).code
                != ResultCode::unsupported_schema
            || TextureDocument::deserialize(mismatchedExtension).code
                != ResultCode::invalid_descriptor
            || first.serialize(32u).code
                != ResultCode::serialized_budget_exceeded)
        {
            return 38;
        }

        TextureDocument legacyOriginal = make_document({ 23, 1 });
        const DocumentSnapshot currentEmptySnapshot =
            legacyOriginal.snapshot();
        const SnapshotSerializationResult currentEmptySource =
            legacyOriginal.serialize();
        if (!currentEmptySource
            || currentEmptySource.bytes.size() < 20u
            || currentEmptySource.bytes[8] != std::byte{ 3 })
        {
            return 80;
        }

        std::vector<std::byte> schema1Source =
            currentEmptySource.bytes;
        schema1Source.resize(schema1Source.size() - 8u);
        schema1Source[8] = std::byte{ 1 };
        const ContentHash schema1Content =
            legacy_empty_document_content(currentEmptySnapshot, 1u);
        if (schema1Content.empty()
            || !overwrite_serialized_content_hash(
                schema1Source,
                schema1Content))
        {
            return 81;
        }
        const RestoreResult migratedSchema1 =
            TextureDocument::deserialize(schema1Source);
        const SnapshotSerializationResult schema1AsCurrent =
            migratedSchema1
                ? migratedSchema1.document->serialize()
                : SnapshotSerializationResult{};
        if (!migratedSchema1
            || migratedSchema1.document->snapshot()
                != currentEmptySnapshot
            || !schema1AsCurrent
            || schema1AsCurrent.bytes != currentEmptySource.bytes
            || schema1AsCurrent.bytes[8] != std::byte{ 3 })
        {
            return 82;
        }
        schema1Source[12] ^= std::byte{ 0x1 };
        if (TextureDocument::deserialize(schema1Source).code
                != ResultCode::integrity_failure)
        {
            return 83;
        }

        std::vector<std::byte> schema2Source =
            currentEmptySource.bytes;
        schema2Source.resize(schema2Source.size() - 4u);
        schema2Source[8] = std::byte{ 2 };
        const ContentHash schema2Content =
            legacy_empty_document_content(currentEmptySnapshot, 2u);
        if (schema2Content.empty()
            || !overwrite_serialized_content_hash(
                schema2Source,
                schema2Content))
        {
            return 84;
        }
        const RestoreResult migratedSchema2 =
            TextureDocument::deserialize(schema2Source);
        const SnapshotSerializationResult schema2AsCurrent =
            migratedSchema2
                ? migratedSchema2.document->serialize()
                : SnapshotSerializationResult{};
        if (!migratedSchema2
            || migratedSchema2.document->snapshot()
                != currentEmptySnapshot
            || !schema2AsCurrent
            || schema2AsCurrent.bytes != currentEmptySource.bytes
            || schema2AsCurrent.bytes[8] != std::byte{ 3 })
        {
            return 85;
        }
        schema2Source[12] ^= std::byte{ 0x1 };
        if (TextureDocument::deserialize(schema2Source).code
                != ResultCode::integrity_failure)
        {
            return 86;
        }

        return 0;
    }
}

#if defined(EPOCH_AUTHORING_TEXTURE_CONTRACT_MAIN)
int main()
{
    return
        epochengine::authoring::texture::contract::
            run_texture_document_contract();
}
#endif
