/***********************************************
 *                                             *
 * This file is part of the Epoch Project.     *
 *                                             *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 *                                             *
 ***********************************************/

module;

#include <algorithm>
#include <cstdint>

export module render.context_frame;

export namespace epochengine::rendercontext
{
    struct Viewport final
    {
        int x{};
        int y{};
        int width{};
        int height{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return width > 0 && height > 0;
        }

        [[nodiscard]] constexpr bool operator==(const Viewport&) const noexcept = default;
    };

    struct FrameRequest final
    {
        int framebuffer_width{};
        int framebuffer_height{};
        Viewport requested_scene{};
        bool editor_preview{};
        bool gui_overlay_priority{};
    };

    struct FramePlan final
    {
        Viewport requested_scene{};
        Viewport scene{};
        int source_offset_x{};
        int source_offset_y{};
        int bottom_left_y{};
        float normalized_left{};
        float normalized_top{};
        float normalized_width{};
        float normalized_height{};
        float projection_aspect{1.0f};
        bool scene_visible{};
        bool scene_clipped{};
        bool deferred_before_scene{};
        bool deferred_after_scene{true};
        bool top_layer_after_scene{true};
        bool capture_after_composition{true};
        std::uint64_t semantic_signature{};

        [[nodiscard]] constexpr bool operator==(const FramePlan&) const noexcept = default;
    };

    namespace detail
    {
        [[nodiscard]] constexpr std::uint64_t hash_word(
            std::uint64_t hash,
            std::uint32_t word) noexcept
        {
            constexpr std::uint64_t prime = 1099511628211ull;
            for (unsigned shift = 0; shift < 32; shift += 8)
            {
                hash ^= (word >> shift) & 0xffu;
                hash *= prime;
            }
            return hash;
        }

        [[nodiscard]] constexpr std::uint64_t signature_for(const FrameRequest& request) noexcept
        {
            std::uint64_t hash = 1469598103934665603ull;
            hash = hash_word(hash, static_cast<std::uint32_t>(request.framebuffer_width));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.framebuffer_height));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.requested_scene.x));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.requested_scene.y));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.requested_scene.width));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.requested_scene.height));
            hash = hash_word(hash, request.editor_preview ? 1u : 0u);
            return hash_word(hash, request.gui_overlay_priority ? 1u : 0u);
        }
    }

    [[nodiscard]] constexpr FramePlan resolve_frame_plan(const FrameRequest& request) noexcept
    {
        FramePlan plan{};
        plan.requested_scene = request.requested_scene;
        plan.deferred_before_scene = !request.gui_overlay_priority;
        plan.semantic_signature = detail::signature_for(request);

        if (!request.editor_preview
            || request.framebuffer_width <= 0
            || request.framebuffer_height <= 0
            || !request.requested_scene.valid())
        {
            return plan;
        }

        const std::int64_t requested_left = request.requested_scene.x;
        const std::int64_t requested_top = request.requested_scene.y;
        const std::int64_t requested_right = requested_left + request.requested_scene.width;
        const std::int64_t requested_bottom = requested_top + request.requested_scene.height;
        const std::int64_t clipped_left = (std::max)(std::int64_t{0}, requested_left);
        const std::int64_t clipped_top = (std::max)(std::int64_t{0}, requested_top);
        const std::int64_t clipped_right = (std::min)(
            static_cast<std::int64_t>(request.framebuffer_width), requested_right);
        const std::int64_t clipped_bottom = (std::min)(
            static_cast<std::int64_t>(request.framebuffer_height), requested_bottom);
        if (clipped_left >= clipped_right || clipped_top >= clipped_bottom)
            return plan;

        plan.scene = Viewport{
            static_cast<int>(clipped_left),
            static_cast<int>(clipped_top),
            static_cast<int>(clipped_right - clipped_left),
            static_cast<int>(clipped_bottom - clipped_top)
        };
        plan.source_offset_x = static_cast<int>(clipped_left - requested_left);
        plan.source_offset_y = static_cast<int>(clipped_top - requested_top);
        plan.bottom_left_y = request.framebuffer_height - (plan.scene.y + plan.scene.height);
        plan.normalized_left = plan.scene.x / static_cast<float>(request.framebuffer_width);
        plan.normalized_top = plan.scene.y / static_cast<float>(request.framebuffer_height);
        plan.normalized_width = plan.scene.width / static_cast<float>(request.framebuffer_width);
        plan.normalized_height = plan.scene.height / static_cast<float>(request.framebuffer_height);
        plan.projection_aspect = plan.scene.width / static_cast<float>(plan.scene.height);
        plan.scene_visible = true;
        plan.scene_clipped = plan.scene != request.requested_scene;
        return plan;
    }
}
