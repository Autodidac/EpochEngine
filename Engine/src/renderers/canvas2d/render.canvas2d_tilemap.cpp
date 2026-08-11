/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <tuple>
#include <utility>

module render.canvas2d_tilemap;

namespace epochengine::canvas2d::tilemap_runtime
{
    namespace detail
    {
        [[nodiscard]] bool finite(Float2 value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        [[nodiscard]] bool finite(RectF value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y)
                && std::isfinite(value.width) && std::isfinite(value.height);
        }

        [[nodiscard]] constexpr bool intersects(RectF left, RectF right) noexcept
        {
            if (left.empty() || right.empty())
                return false;
            return left.x < right.x + right.width
                && left.x + left.width > right.x
                && left.y < right.y + right.height
                && left.y + left.height > right.y;
        }

        [[nodiscard]] constexpr RectF expanded(
            RectF bounds,
            Float2 margin) noexcept
        {
            return {
                bounds.x - margin.x,
                bounds.y - margin.y,
                bounds.width + margin.x * 2.0f,
                bounds.height + margin.y * 2.0f};
        }

        [[nodiscard]] constexpr SpritePhase sprite_phase(
            asset::tilemap::LayerPhase value) noexcept
        {
            switch (value)
            {
            case asset::tilemap::LayerPhase::background:
                return SpritePhase::background;
            case asset::tilemap::LayerPhase::foreground:
                return SpritePhase::foreground;
            case asset::tilemap::LayerPhase::overlay:
                return SpritePhase::overlay;
            case asset::tilemap::LayerPhase::world:
            default:
                return SpritePhase::world;
            }
        }

        [[nodiscard]] constexpr FilterMode filter_mode(
            asset::tilemap::FilterMode value) noexcept
        {
            return value == asset::tilemap::FilterMode::linear
                ? FilterMode::linear : FilterMode::nearest;
        }

        [[nodiscard]] constexpr AddressMode address_mode(
            asset::tilemap::AddressMode value) noexcept
        {
            switch (value)
            {
            case asset::tilemap::AddressMode::repeat:
                return AddressMode::repeat;
            case asset::tilemap::AddressMode::mirrored_repeat:
                return AddressMode::mirrored_repeat;
            case asset::tilemap::AddressMode::clamp_to_edge:
            default:
                return AddressMode::clamp_to_edge;
            }
        }

        [[nodiscard]] constexpr SpriteAlphaMode alpha_mode(
            asset::tilemap::AlphaMode value) noexcept
        {
            switch (value)
            {
            case asset::tilemap::AlphaMode::opaque:
                return SpriteAlphaMode::opaque;
            case asset::tilemap::AlphaMode::mask:
                return SpriteAlphaMode::mask;
            case asset::tilemap::AlphaMode::straight:
                return SpriteAlphaMode::straight;
            case asset::tilemap::AlphaMode::additive:
                return SpriteAlphaMode::additive;
            case asset::tilemap::AlphaMode::premultiplied:
            default:
                return SpriteAlphaMode::premultiplied;
            }
        }

        [[nodiscard]] constexpr SpriteColorSpace color_space(
            asset::tilemap::ColorSpace value) noexcept
        {
            return value == asset::tilemap::ColorSpace::linear
                ? SpriteColorSpace::linear : SpriteColorSpace::srgb;
        }

        [[nodiscard]] constexpr SpriteMaterialDeclaration material(
            const asset::tilemap::TextureDependency& dependency) noexcept
        {
            return {
                .stable_key = dependency.stable_material_key,
                .source = SpriteSourceKind::texture,
                .logical_texture = {
                    dependency.asset_key,
                    dependency.artifact_revision},
                .sampler = {
                    .min_filter = filter_mode(dependency.min_filter),
                    .mag_filter = filter_mode(dependency.mag_filter),
                    .address_u = address_mode(dependency.address_u),
                    .address_v = address_mode(dependency.address_v),
                    .maximum_anisotropy = 1.0f,
                    .mipmapped = false},
                .alpha = alpha_mode(dependency.alpha),
                .color_space = color_space(dependency.color_space),
                .alpha_cutoff = dependency.alpha_cutoff,
                .writes_color = true};
        }

        [[nodiscard]] constexpr std::uint64_t stable_sequence(
            asset::tilemap::LayerId layer,
            asset::tilemap::UInt2 coordinate) noexcept
        {
            std::uint64_t hash = 14695981039346656037ull;
            const auto mix = [&hash](std::uint64_t value)
            {
                for (std::uint32_t byte = 0u; byte < 8u; ++byte)
                {
                    hash ^= static_cast<std::uint8_t>(value >> (byte * 8u));
                    hash *= 1099511628211ull;
                }
            };
            mix(layer.value);
            mix(coordinate.x);
            mix(coordinate.y);
            return hash == 0u ? 1u : hash;
        }

        [[nodiscard]] constexpr LinearColor tint(
            asset::tilemap::Rgba8 color,
            float opacity) noexcept
        {
            constexpr float scale = 1.0f / 255.0f;
            const float alpha = static_cast<float>(color.a) * scale * opacity;
            return {
                static_cast<float>(color.r) * scale * opacity,
                static_cast<float>(color.g) * scale * opacity,
                static_cast<float>(color.b) * scale * opacity,
                alpha};
        }

        struct TransformResult final
        {
            Float2 size{};
            float rotation{};
            bool flip_x{};
            bool flip_y{};
        };

        [[nodiscard]] constexpr TransformResult transform(
            asset::tilemap::TileTransform value,
            Float2 cellSize) noexcept
        {
            constexpr float halfPi = 1.57079632679489661923f;
            switch (value)
            {
            case asset::tilemap::TileTransform::flip_x:
                return {cellSize, 0.0f, true, false};
            case asset::tilemap::TileTransform::flip_y:
                return {cellSize, 0.0f, false, true};
            case asset::tilemap::TileTransform::flip_xy:
                return {cellSize, 0.0f, true, true};
            case asset::tilemap::TileTransform::rotate_90:
                return {{cellSize.y, cellSize.x}, halfPi, false, false};
            case asset::tilemap::TileTransform::rotate_180:
                return {cellSize, halfPi * 2.0f, false, false};
            case asset::tilemap::TileTransform::rotate_270:
                return {{cellSize.y, cellSize.x}, halfPi * 3.0f, false, false};
            case asset::tilemap::TileTransform::identity:
            default:
                return {cellSize, 0.0f, false, false};
            }
        }

        [[nodiscard]] constexpr RectF source_uv(
            const asset::tilemap::CompiledTileSet& tileSet,
            const asset::tilemap::TextureDependency& dependency,
            std::uint32_t tile) noexcept
        {
            const std::uint32_t column = tile % tileSet.grid.x;
            const std::uint32_t row = tile / tileSet.grid.x;
            const std::uint32_t pixelX = tileSet.margin.x
                + column * (tileSet.tile_extent.x + tileSet.spacing.x);
            const std::uint32_t pixelY = tileSet.margin.y
                + row * (tileSet.tile_extent.y + tileSet.spacing.y);
            return {
                static_cast<float>(pixelX)
                    / static_cast<float>(dependency.texture_extent.x),
                static_cast<float>(pixelY)
                    / static_cast<float>(dependency.texture_extent.y),
                static_cast<float>(tileSet.tile_extent.x)
                    / static_cast<float>(dependency.texture_extent.x),
                static_cast<float>(tileSet.tile_extent.y)
                    / static_cast<float>(dependency.texture_extent.y)};
        }

        [[nodiscard]] constexpr std::uint16_t animation_frame(
            const asset::tilemap::CompiledPaletteEntry& palette,
            const asset::tilemap::CompiledCell& cell,
            std::uint64_t simulatedMilliseconds) noexcept
        {
            if (palette.animation_frame_count <= 1u
                || palette.animation_rate_millihertz == 0u)
            {
                return cell.animation_frame;
            }
            const std::uint64_t frame = simulatedMilliseconds
                * palette.animation_rate_millihertz / 1'000'000u;
            return static_cast<std::uint16_t>(
                (cell.animation_frame + frame) % palette.animation_frame_count);
        }

        [[nodiscard]] constexpr bool object_visible(
            const asset::tilemap::CompiledMapObject& object,
            RectF view) noexcept
        {
            return intersects({
                object.position.x - object.size.x * 0.5f,
                object.position.y - object.size.y * 0.5f,
                object.size.x,
                object.size.y}, view);
        }
    }

    VisibleCompilation compile_visible(
        const asset::tilemap::CompiledTileMapArtifact& artifact,
        const ViewRequest& view,
        const CompileLimits& limits) noexcept
    {
        VisibleCompilation output{};
        try
        {
            if (!limits.valid())
            {
                output.code = CompileCode::invalid_limits;
                return output;
            }
            const auto artifactValidation = asset::tilemap::validate(artifact);
            if (!artifactValidation)
            {
                output.code = CompileCode::invalid_artifact;
                return output;
            }
            if (!detail::finite(view.world_bounds) || view.world_bounds.empty()
                || !detail::finite(view.culling_margin)
                || view.culling_margin.x < 0.0f || view.culling_margin.y < 0.0f)
            {
                output.code = CompileCode::invalid_view;
                return output;
            }
            const RectF expandedView = detail::expanded(
                view.world_bounds, view.culling_margin);
            output.metrics.submitted_layers = artifact.layers.size();
            output.metrics.submitted_chunks = artifact.chunks.size();
            output.metrics.submitted_cells = artifact.cells.size();
            output.sprites.reserve((std::min)(
                artifact.cells.size(),
                static_cast<std::size_t>(limits.maximum_visible_sprites)));

            std::uint64_t spriteOrdinal{};
            for (const asset::tilemap::CompiledLayer& layer : artifact.layers)
            {
                if (layer.visible && layer.opacity > 0.0f)
                    ++output.metrics.visible_layers;
                else
                    ++output.metrics.hidden_layers;
            }
            for (const asset::tilemap::CompiledChunk& chunk : artifact.chunks)
            {
                const asset::tilemap::CompiledLayer& layer =
                    artifact.layers[chunk.layer_index];
                if (!layer.visible || layer.opacity <= 0.0f
                    || !detail::intersects({
                        chunk.world_bounds.x,
                        chunk.world_bounds.y,
                        chunk.world_bounds.width,
                        chunk.world_bounds.height}, expandedView))
                {
                    ++output.metrics.culled_chunks;
                    continue;
                }
                if (output.metrics.visible_chunks >= limits.maximum_visible_chunks)
                {
                    ++output.metrics.capacity_rejections;
                    output.code = CompileCode::partial;
                    break;
                }
                ++output.metrics.visible_chunks;
                for (std::uint32_t offset = 0u; offset < chunk.cell_count; ++offset)
                {
                    if (output.sprites.size() >= limits.maximum_visible_sprites)
                    {
                        ++output.metrics.capacity_rejections;
                        output.code = CompileCode::partial;
                        break;
                    }
                    const asset::tilemap::CompiledCell& cell =
                        artifact.cells[chunk.first_cell + offset];
                    const asset::tilemap::CompiledPaletteEntry& palette =
                        artifact.palette[cell.palette_index];
                    const asset::tilemap::CompiledTileSet& tileSet =
                        artifact.tile_sets[palette.tile_set_index];
                    if (tileSet.dependency_index >= artifact.dependencies.size())
                    {
                        ++output.metrics.dependency_rejections;
                        output.code = CompileCode::partial;
                        continue;
                    }
                    const asset::tilemap::TextureDependency& dependency =
                        artifact.dependencies[tileSet.dependency_index];
                    const std::uint16_t frame = detail::animation_frame(
                        palette, cell, view.simulated_milliseconds);
                    const std::uint32_t tile = palette.local_tile + frame;
                    if (tile >= tileSet.tile_count)
                    {
                        ++output.metrics.dependency_rejections;
                        output.code = CompileCode::partial;
                        continue;
                    }
                    const Float2 cellSize{
                        layer.tile_world_extent.x,
                        layer.tile_world_extent.y};
                    const detail::TransformResult transformed =
                        detail::transform(cell.transform, cellSize);
                    if (spriteOrdinal > (std::numeric_limits<std::uint32_t>::max)())
                    {
                        output.code = CompileCode::arithmetic_overflow;
                        return output;
                    }
                    output.sprites.push_back({
                        .sprite = {
                            static_cast<std::uint32_t>(spriteOrdinal), 1u},
                        .material = detail::material(dependency),
                        .transform = {
                            .position = {
                                layer.world_origin.x
                                    + (static_cast<float>(cell.coordinate.x) + 0.5f)
                                        * layer.tile_world_extent.x,
                                layer.world_origin.y
                                    + (static_cast<float>(cell.coordinate.y) + 0.5f)
                                        * layer.tile_world_extent.y},
                            .size = transformed.size,
                            .pivot = {0.5f, 0.5f},
                            .rotation_radians = transformed.rotation},
                        .source_uv = detail::source_uv(tileSet, dependency, tile),
                        .tint = detail::tint(cell.tint, layer.opacity),
                        .phase = detail::sprite_phase(layer.phase),
                        .layer = layer.draw_layer,
                        .order = static_cast<std::int32_t>(cell.coordinate.y),
                        .depth = 0,
                        .stable_sequence = detail::stable_sequence(
                            layer.id, cell.coordinate),
                        .clip_key = 0u,
                        .flip_x = transformed.flip_x,
                        .flip_y = transformed.flip_y,
                        .hidden = false,
                        .sort_group = static_cast<std::int32_t>(chunk.layer_index)});
                    ++spriteOrdinal;
                    ++output.metrics.emitted_sprites;
                }
            }

            if (view.include_collision)
            {
                output.collision.reserve((std::min)(
                    artifact.collision.size(),
                    static_cast<std::size_t>(limits.maximum_visible_collision)));
                for (const auto& primitive : artifact.collision)
                {
                    if (!detail::intersects({
                            primitive.world_bounds.x,
                            primitive.world_bounds.y,
                            primitive.world_bounds.width,
                            primitive.world_bounds.height}, expandedView))
                    {
                        continue;
                    }
                    if (output.collision.size()
                        >= limits.maximum_visible_collision)
                    {
                        ++output.metrics.capacity_rejections;
                        output.code = CompileCode::partial;
                        break;
                    }
                    output.collision.push_back(primitive);
                    ++output.metrics.emitted_collision;
                }
            }

            if (view.include_objects)
            {
                output.objects.reserve((std::min)(
                    artifact.objects.size(),
                    static_cast<std::size_t>(limits.maximum_visible_objects)));
                for (const auto& object : artifact.objects)
                {
                    if (!detail::object_visible(object, expandedView))
                        continue;
                    if (output.objects.size() >= limits.maximum_visible_objects)
                    {
                        ++output.metrics.capacity_rejections;
                        output.code = CompileCode::partial;
                        break;
                    }
                    output.objects.push_back({
                        .id = object.id,
                        .name = object.name,
                        .type = object.type,
                        .position = {object.position.x, object.position.y},
                        .size = {object.size.x, object.size.y},
                        .rotation_radians = object.rotation_radians,
                        .layer_bits = object.layer_bits,
                        .user_flags = object.user_flags});
                    ++output.metrics.emitted_objects;
                }
            }
            std::stable_sort(
                output.sprites.begin(), output.sprites.end(),
                [](const SpriteSubmission& left, const SpriteSubmission& right)
                {
                    return less(sort_key(left), sort_key(right));
                });

            if (output.code != CompileCode::partial)
                output.code = CompileCode::ready;
            return output;
        }
        catch (const std::bad_alloc&)
        {
            output.sprites.clear();
            output.collision.clear();
            output.objects.clear();
            output.code = CompileCode::allocation_failure;
            return output;
        }
        catch (...)
        {
            output.sprites.clear();
            output.collision.clear();
            output.objects.clear();
            output.code = CompileCode::invalid_artifact;
            return output;
        }
    }
}
