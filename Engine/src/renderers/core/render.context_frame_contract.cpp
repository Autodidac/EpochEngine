import render.context_frame;

#include <climits>

namespace
{
    using epochengine::rendercontext::FrameRequest;
    using epochengine::rendercontext::Viewport;

    [[nodiscard]] bool frame_contract() noexcept
    {
        using epochengine::rendercontext::resolve_frame_plan;

        const auto full = resolve_frame_plan(FrameRequest{ 1920, 1080, { 100, 50, 800, 600 }, true, false });
        if (!full.scene_visible || full.scene_clipped
            || full.scene != Viewport{ 100, 50, 800, 600 }
            || full.bottom_left_y != 430
            || full.projection_aspect != (800.0f / 600.0f)
            || !full.deferred_before_scene || !full.deferred_after_scene
            || !full.top_layer_after_scene || !full.capture_after_composition)
        {
            return false;
        }

        const auto clipped = resolve_frame_plan(FrameRequest{ 640, 480, { -40, 440, 900, 100 }, true, true });
        if (!clipped.scene_visible || !clipped.scene_clipped
            || clipped.scene != Viewport{ 0, 440, 640, 40 }
            || clipped.source_offset_x != 40 || clipped.source_offset_y != 0
            || clipped.bottom_left_y != 0
            || clipped.deferred_before_scene)
        {
            return false;
        }

        const auto outside = resolve_frame_plan(FrameRequest{ 640, 480, { 700, 10, 20, 20 }, true, false });
        const auto disabled = resolve_frame_plan(FrameRequest{ 640, 480, { 0, 0, 640, 480 }, false, false });
        const auto overflow = resolve_frame_plan(FrameRequest{ 640, 480, { INT_MAX - 4, 0, INT_MAX, 64 }, true, false });
        if (outside.scene_visible || disabled.scene_visible || overflow.scene_visible)
            return false;

        const auto repeated = resolve_frame_plan(FrameRequest{ 1920, 1080, { 100, 50, 800, 600 }, true, false });
        const auto changed = resolve_frame_plan(FrameRequest{ 1920, 1080, { 100, 50, 801, 600 }, true, false });
        return repeated == full
            && repeated.semantic_signature == full.semantic_signature
            && changed.semantic_signature != full.semantic_signature;
    }
}

int main()
{
    return frame_contract() ? 0 : 1;
}
