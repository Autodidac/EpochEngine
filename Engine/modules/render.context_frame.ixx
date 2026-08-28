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

    enum class WindowActivity : std::uint8_t
    {
        foreground,
        background,
        minimized,
        occluded
    };

    struct WindowObservation final
    {
        int logical_width{};
        int logical_height{};
        int framebuffer_width{};
        int framebuffer_height{};
        std::uint32_t dpi_milli{1000u};
        std::uint64_t resize_generation{};
        bool visible{true};
        bool focused{true};
        bool minimized{};
        bool occluded{};
        bool visibility_known{};
        bool focus_known{};
        bool minimized_known{};
        bool occlusion_known{};
        bool dpi_known{};
    };

    struct WindowState final
    {
        WindowActivity activity{WindowActivity::foreground};
        int logical_width{};
        int logical_height{};
        int framebuffer_width{};
        int framebuffer_height{};
        std::uint32_t dpi_milli{1000u};
        std::uint64_t resize_generation{};
        bool presentable{true};
        bool visible{true};
        bool focused{true};
        bool minimized{};
        bool occluded{};
        bool visibility_known{};
        bool focus_known{};
        bool minimized_known{};
        bool occlusion_known{};
        bool dpi_known{};

        [[nodiscard]] constexpr bool operator==(const WindowState&) const noexcept = default;
    };

    [[nodiscard]] constexpr WindowState resolve_window_state(
        const WindowObservation& observation) noexcept;

    [[nodiscard]] constexpr const char* window_activity_name(
        WindowActivity activity) noexcept
    {
        switch (activity)
        {
        case WindowActivity::foreground: return "foreground";
        case WindowActivity::background: return "background";
        case WindowActivity::minimized: return "minimized";
        case WindowActivity::occluded: return "occluded";
        }
        return "foreground";
    }

    struct FrameRequest final
    {
        int framebuffer_width{};
        int framebuffer_height{};
        Viewport requested_scene{};
        bool editor_preview{};
        bool gui_overlay_priority{};
        WindowState window{};
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
        WindowState window{};
        bool surface_presentable{true};
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
            hash = hash_word(hash, request.gui_overlay_priority ? 1u : 0u);
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.activity));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.logical_width));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.logical_height));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.framebuffer_width));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.framebuffer_height));
            hash = hash_word(hash, request.window.dpi_milli);
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.resize_generation));
            hash = hash_word(hash, static_cast<std::uint32_t>(request.window.resize_generation >> 32u));
            hash = hash_word(hash, request.window.presentable ? 1u : 0u);
            hash = hash_word(hash, request.window.visibility_known ? 1u : 0u);
            hash = hash_word(hash, request.window.focus_known ? 1u : 0u);
            hash = hash_word(hash, request.window.minimized_known ? 1u : 0u);
            hash = hash_word(hash, request.window.occlusion_known ? 1u : 0u);
            return hash_word(hash, request.window.dpi_known ? 1u : 0u);
        }
    }

    [[nodiscard]] constexpr WindowState resolve_window_state(
        const WindowObservation& observation) noexcept
    {
        WindowState state{};
        state.logical_width = (std::max)(0, observation.logical_width);
        state.logical_height = (std::max)(0, observation.logical_height);
        state.framebuffer_width = (std::max)(0, observation.framebuffer_width);
        state.framebuffer_height = (std::max)(0, observation.framebuffer_height);
        state.resize_generation = observation.resize_generation;
        state.visibility_known = observation.visibility_known;
        state.focus_known = observation.focus_known;
        state.minimized_known = observation.minimized_known;
        state.occlusion_known = observation.occlusion_known;
        state.dpi_known = observation.dpi_known
            && observation.dpi_milli >= 250u
            && observation.dpi_milli <= 8000u;
        state.dpi_milli = state.dpi_known ? observation.dpi_milli : 1000u;
        state.visible = !state.visibility_known || observation.visible;
        state.focused = !state.focus_known || observation.focused;
        state.minimized = (state.minimized_known && observation.minimized)
            || state.framebuffer_width <= 0
            || state.framebuffer_height <= 0;
        state.occluded = state.occlusion_known && observation.occluded;
        state.presentable = state.visible && !state.minimized && !state.occluded;

        if (state.minimized)
            state.activity = WindowActivity::minimized;
        else if (!state.presentable)
            state.activity = WindowActivity::occluded;
        else if (state.focus_known && !state.focused)
            state.activity = WindowActivity::background;
        else
            state.activity = WindowActivity::foreground;
        return state;
    }

    [[nodiscard]] constexpr FramePlan resolve_frame_plan(const FrameRequest& request) noexcept
    {
        FramePlan plan{};
        plan.requested_scene = request.requested_scene;
        plan.deferred_before_scene = !request.gui_overlay_priority;
        plan.window = request.window;
        plan.surface_presentable = request.window.presentable;
        plan.semantic_signature = detail::signature_for(request);

        if (!plan.surface_presentable
            || !request.editor_preview
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
