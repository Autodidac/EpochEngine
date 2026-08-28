/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

module render.canvas2d_scene;

import render.canvas2d_cpu;
import render.device;

namespace epochengine::canvas2d::scene_content
{
    namespace
    {
        constexpr std::uint64_t kHashOffset = 14695981039346656037ull;
        constexpr std::uint64_t kHashPrime = 1099511628211ull;

        struct Slot final
        {
            std::shared_ptr<const SceneContent> content{};
            std::uint64_t generation{};
            std::uint64_t frame_sequence{};
            std::uint64_t content_hash{};
        };

        struct Storage final
        {
            std::mutex mutex{};
            std::unordered_map<const void*, Slot> slots{};
            SceneMetrics metrics{};
            std::uint64_t next_generation{1};
            std::uint64_t next_frame_sequence{1};
        };

        [[nodiscard]] Storage& storage() noexcept
        {
            static Storage value{};
            return value;
        }

        [[nodiscard]] constexpr std::uint64_t append_byte(
            std::uint64_t hash,
            std::uint8_t value) noexcept
        {
            return (hash ^ value) * kHashPrime;
        }

        template <typename Integer>
        [[nodiscard]] constexpr std::uint64_t append_integer(
            std::uint64_t hash,
            Integer value) noexcept
        {
            using Unsigned = std::make_unsigned_t<Integer>;
            const Unsigned bits = static_cast<Unsigned>(value);
            for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte)
            {
                hash = append_byte(
                    hash,
                    static_cast<std::uint8_t>(bits >> (byte * 8u)));
            }
            return hash;
        }

        [[nodiscard]] constexpr std::uint64_t append_float(
            std::uint64_t hash,
            float value) noexcept
        {
            return append_integer(hash, std::bit_cast<std::uint32_t>(value));
        }

        [[nodiscard]] constexpr std::uint64_t append_bool(
            std::uint64_t hash,
            bool value) noexcept
        {
            return append_byte(hash, value ? 1u : 0u);
        }

        [[nodiscard]] constexpr std::uint64_t append_color(
            std::uint64_t hash,
            LinearColor value) noexcept
        {
            hash = append_float(hash, value.r);
            hash = append_float(hash, value.g);
            hash = append_float(hash, value.b);
            return append_float(hash, value.a);
        }

        [[nodiscard]] std::uint64_t next_nonzero(std::uint64_t& value) noexcept
        {
            const std::uint64_t result = value == 0 ? 1 : value;
            ++value;
            if (value == 0)
                value = 1;
            return result;
        }

        [[nodiscard]] bool valid_texture_view(const cpu::TextureView& view) noexcept
        {
            if (!view.has_identity() || view.extent.empty()
                || view.row_stride_pixels < view.extent.width)
            {
                return false;
            }
            const std::uint64_t required = static_cast<std::uint64_t>(
                view.row_stride_pixels) * view.extent.height;
            return required <= view.pixels.size()
                && (view.color_space == SpriteColorSpace::linear
                    || view.color_space == SpriteColorSpace::srgb)
                && (view.alpha_encoding == cpu::AlphaEncoding::straight
                    || view.alpha_encoding == cpu::AlphaEncoding::premultiplied);
        }

        [[nodiscard]] bool valid_clip(const cpu::ClipRect& clip) noexcept
        {
            return clip.key != 0 && !clip.rectangle.empty();
        }

        [[nodiscard]] PublicationResult reject(SceneCode code) noexcept
        {
            auto& state = storage();
            std::scoped_lock lock(state.mutex);
            ++state.metrics.rejected_publications;
            return {code};
        }
    }

    bool ResourceLease::valid() const noexcept
    {
        if ((!bindings.textures.empty() || !bindings.clips.empty()) && !owner)
            return false;

        std::unordered_set<
            LogicalTextureReference, LogicalTextureReferenceHash> logical_keys{};
        std::unordered_set<std::uint32_t> physical_keys{};
        std::unordered_set<std::uint32_t> clip_keys{};
        try
        {
            logical_keys.reserve(bindings.textures.size());
            physical_keys.reserve(bindings.textures.size());
            clip_keys.reserve(bindings.clips.size());
            for (const cpu::TextureView& view : bindings.textures)
            {
                if (!valid_texture_view(view))
                    return false;
                if (view.logical && !logical_keys.insert(view.logical).second)
                    return false;
                if (!view.logical
                    && (!view.physical
                        || !physical_keys.insert(view.physical.value).second))
                {
                    return false;
                }
            }
            for (const cpu::ClipRect& clip : bindings.clips)
            {
                if (!valid_clip(clip) || !clip_keys.insert(clip.key).second)
                    return false;
            }
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    bool SceneContent::valid() const noexcept
    {
        if (source_revision == 0 || canvas2d::validate(project) != ResultCode::success
            || !canvas2d::valid(camera) || !finite(clear_color)
            || !finite(letterbox_color)
            || validate_resource_closure(sprites, resources)
                != ResourceClosureCode::ready)
        {
            return false;
        }
        for (const SpriteSubmission& sprite : sprites)
        {
            if (canvas2d::validate(sprite) != ResultCode::success)
                return false;
        }
        return true;
    }

    Canvas2DSubmission SceneContent::submission(
        std::uint64_t frameSequence) const noexcept
    {
        Canvas2DSubmission result{};
        result.project = project;
        result.camera = camera;
        result.sprites = sprites;
        result.destination = ComposeTargetKind::presentation_surface;
        result.color_format = TextureFormat::rgba8_unorm;
        result.clear_color = clear_color;
        result.letterbox_color = letterbox_color;
        result.frame_sequence = frameSequence;
        return result;
    }

    std::uint64_t content_hash(const SceneContent& content) noexcept
    {
        std::uint64_t hash = kHashOffset;
        hash = append_integer(hash, content.project.logical_canvas.width);
        hash = append_integer(hash, content.project.logical_canvas.height);
        hash = append_float(hash, content.project.pixels_per_world_unit);
        hash = append_integer(hash, content.project.viewport_policy);
        hash = append_integer(hash, content.project.presentation_filter);
        hash = append_integer(hash, content.project.pixel_snap);
        hash = append_integer(hash, content.project.maximum_sprites_per_batch);
        hash = append_integer(hash, content.project.maximum_batches);
        hash = append_integer(hash, content.project.tile_chunk_extent);
        hash = append_float(hash, content.camera.center.x);
        hash = append_float(hash, content.camera.center.y);
        hash = append_float(hash, content.camera.rotation_radians);
        hash = append_float(hash, content.camera.zoom);
        hash = append_float(hash, content.camera.pixels_per_world_unit);
        hash = append_integer(hash, content.camera.pixel_snap);
        hash = append_integer(hash, content.camera.y_axis);
        hash = append_color(hash, content.clear_color);
        hash = append_color(hash, content.letterbox_color);
        hash = append_integer(hash, content.sprites.size());

        for (const SpriteSubmission& sprite : content.sprites)
        {
            hash = append_integer(hash, sprite.sprite.index);
            hash = append_integer(hash, sprite.sprite.generation);
            hash = append_integer(hash, sprite.material.stable_key);
            hash = append_integer(hash, sprite.material.source);
            hash = append_integer(hash, sprite.material.texture.value);
            hash = append_integer(hash, sprite.material.logical_texture.asset_key);
            hash = append_integer(hash, sprite.material.logical_texture.artifact_revision);
            hash = append_integer(hash, sprite.material.sampler.min_filter);
            hash = append_integer(hash, sprite.material.sampler.mag_filter);
            hash = append_integer(hash, sprite.material.sampler.address_u);
            hash = append_integer(hash, sprite.material.sampler.address_v);
            hash = append_float(hash, sprite.material.sampler.maximum_anisotropy);
            hash = append_bool(hash, sprite.material.sampler.mipmapped);
            hash = append_integer(hash, sprite.material.alpha);
            hash = append_integer(hash, sprite.material.color_space);
            hash = append_float(hash, sprite.material.alpha_cutoff);
            hash = append_bool(hash, sprite.material.writes_color);
            hash = append_float(hash, sprite.transform.position.x);
            hash = append_float(hash, sprite.transform.position.y);
            hash = append_float(hash, sprite.transform.size.x);
            hash = append_float(hash, sprite.transform.size.y);
            hash = append_float(hash, sprite.transform.pivot.x);
            hash = append_float(hash, sprite.transform.pivot.y);
            hash = append_float(hash, sprite.transform.rotation_radians);
            hash = append_float(hash, sprite.source_uv.x);
            hash = append_float(hash, sprite.source_uv.y);
            hash = append_float(hash, sprite.source_uv.width);
            hash = append_float(hash, sprite.source_uv.height);
            hash = append_color(hash, sprite.tint);
            hash = append_integer(hash, sprite.phase);
            hash = append_integer(hash, sprite.layer);
            hash = append_integer(hash, sprite.order);
            hash = append_integer(hash, sprite.depth);
            hash = append_integer(hash, sprite.stable_sequence);
            hash = append_integer(hash, sprite.clip_key);
            hash = append_bool(hash, sprite.flip_x);
            hash = append_bool(hash, sprite.flip_y);
            hash = append_bool(hash, sprite.hidden);
            hash = append_integer(hash, sprite.sort_group);
        }

        hash = append_integer(hash, content.resources.bindings.textures.size());
        for (const cpu::TextureView& view : content.resources.bindings.textures)
        {
            hash = append_integer(hash, view.logical.asset_key);
            hash = append_integer(hash, view.logical.artifact_revision);
            hash = append_integer(hash, view.physical.value);
            hash = append_integer(hash, view.extent.width);
            hash = append_integer(hash, view.extent.height);
            hash = append_integer(hash, view.row_stride_pixels);
            hash = append_integer(hash, view.color_space);
            hash = append_integer(hash, view.alpha_encoding);
            for (const cpu::Rgba8 pixel : view.pixels)
            {
                hash = append_byte(hash, pixel.r);
                hash = append_byte(hash, pixel.g);
                hash = append_byte(hash, pixel.b);
                hash = append_byte(hash, pixel.a);
            }
        }
        hash = append_integer(hash, content.resources.bindings.clips.size());
        for (const cpu::ClipRect& clip : content.resources.bindings.clips)
        {
            hash = append_integer(hash, clip.key);
            hash = append_integer(hash, clip.rectangle.x);
            hash = append_integer(hash, clip.rectangle.y);
            hash = append_integer(hash, clip.rectangle.width);
            hash = append_integer(hash, clip.rectangle.height);
        }
        return hash == 0 ? 1 : hash;
    }

    PublicationResult publish(
        const void* owner,
        SceneContent content,
        const SceneLimits& limits) noexcept
    {
        if (!owner)
            return reject(SceneCode::invalid_owner);
        if (!limits.valid() || !content.valid())
            return reject(SceneCode::invalid_content);
        if (content.sprites.size() > limits.maximum_sprites
            || content.resources.bindings.textures.size() > limits.maximum_textures
            || content.resources.bindings.clips.size() > limits.maximum_clips)
        {
            return reject(SceneCode::capacity_exceeded);
        }

        const std::uint64_t hash = content_hash(content);
        std::shared_ptr<const SceneContent> immutable{};
        try
        {
            immutable = std::make_shared<const SceneContent>(std::move(content));
        }
        catch (...)
        {
            return reject(SceneCode::allocation_failure);
        }

        auto& state = storage();
        std::scoped_lock lock(state.mutex);
        ++state.metrics.publication_requests;
        const auto existing = state.slots.find(owner);
        if (existing != state.slots.end()
            && existing->second.content_hash == hash
            && existing->second.content->source_revision
                == immutable->source_revision)
        {
            ++state.metrics.unchanged_publications;
            return {
                SceneCode::unchanged,
                existing->second.generation,
                existing->second.frame_sequence,
                existing->second.content_hash};
        }
        if (existing == state.slots.end()
            && state.slots.size() >= limits.maximum_slots)
        {
            ++state.metrics.rejected_publications;
            return {SceneCode::capacity_exceeded};
        }

        Slot slot{};
        slot.content = std::move(immutable);
        slot.generation = next_nonzero(state.next_generation);
        slot.frame_sequence = next_nonzero(state.next_frame_sequence);
        slot.content_hash = hash;
        try
        {
            state.slots.insert_or_assign(owner, slot);
        }
        catch (...)
        {
            ++state.metrics.rejected_publications;
            return {SceneCode::allocation_failure};
        }
        ++state.metrics.publications;
        state.metrics.published_sprites += slot.content->sprites.size();
        state.metrics.published_texture_views +=
            slot.content->resources.bindings.textures.size();
        state.metrics.active_slots = static_cast<std::uint32_t>(state.slots.size());
        return {
            SceneCode::ready,
            slot.generation,
            slot.frame_sequence,
            slot.content_hash};
    }

    AcquiredScene acquire(const void* owner) noexcept
    {
        if (!owner)
            return {};
        auto& state = storage();
        std::scoped_lock lock(state.mutex);
        const auto found = state.slots.find(owner);
        if (found == state.slots.end())
        {
            ++state.metrics.misses;
            return {};
        }
        ++state.metrics.acquisitions;
        return {
            found->second.content,
            found->second.generation,
            found->second.frame_sequence,
            found->second.content_hash};
    }

    SceneCode retire(const void* owner) noexcept
    {
        if (!owner)
            return SceneCode::invalid_owner;
        auto& state = storage();
        std::scoped_lock lock(state.mutex);
        if (state.slots.erase(owner) == 0)
            return SceneCode::not_found;
        ++state.metrics.retirements;
        state.metrics.active_slots = static_cast<std::uint32_t>(state.slots.size());
        return SceneCode::ready;
    }

    void retire_all() noexcept
    {
        auto& state = storage();
        std::scoped_lock lock(state.mutex);
        state.metrics.retirements += state.slots.size();
        state.slots.clear();
        state.metrics.active_slots = 0;
    }

    SceneMetrics metrics() noexcept
    {
        auto& state = storage();
        std::scoped_lock lock(state.mutex);
        SceneMetrics result = state.metrics;
        result.active_slots = static_cast<std::uint32_t>(state.slots.size());
        return result;
    }

    Canvas2DFramePlan compile(
        const AcquiredScene& scene,
        CanvasExtent outputSurface,
        const CanvasLimits& canvasLimits) noexcept
    {
        if (!scene || outputSurface.empty())
            return {};
        return compile_canvas2d_submission(
            scene.content->submission(scene.frame_sequence),
            outputSurface,
            canvasLimits);
    }
}
