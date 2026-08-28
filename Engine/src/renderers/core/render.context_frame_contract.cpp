import render.context_frame;

#include <climits>

namespace
{
    using epochengine::rendercontext::FrameRequest;
    using epochengine::rendercontext::Viewport;
    using epochengine::rendercontext::WindowActivity;
    using epochengine::rendercontext::WindowObservation;

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

        const auto foreground = epochengine::rendercontext::resolve_window_state(
            WindowObservation{
                1280, 720, 1920, 1080, 1500u, 7u,
                true, true, false, false,
                true, true, true, true, true});
        const auto background = epochengine::rendercontext::resolve_window_state(
            WindowObservation{
                1280, 720, 1920, 1080, 0u, 8u,
                true, false, false, false,
                true, true, true, false, false});
        const auto minimized = epochengine::rendercontext::resolve_window_state(
            WindowObservation{
                1280, 720, 0, 0, 1500u, 9u,
                true, true, true, false,
                true, true, true, true, true});
        const auto occluded = epochengine::rendercontext::resolve_window_state(
            WindowObservation{
                1280, 720, 1920, 1080, 1500u, 10u,
                true, true, false, true,
                true, true, true, true, true});
        const auto unknown = epochengine::rendercontext::resolve_window_state(
            WindowObservation{1280, 720, 1280, 720});
        if (foreground.activity != WindowActivity::foreground
            || !foreground.presentable || !foreground.dpi_known
            || foreground.dpi_milli != 1500u
            || background.activity != WindowActivity::background
            || !background.presentable || background.dpi_known
            || minimized.activity != WindowActivity::minimized
            || minimized.presentable
            || occluded.activity != WindowActivity::occluded
            || occluded.presentable
            || unknown.activity != WindowActivity::foreground
            || !unknown.presentable || unknown.focus_known
            || unknown.visibility_known || unknown.dpi_known
            || unknown.dpi_milli != 1000u)
        {
            return false;
        }

        const auto suspended = resolve_frame_plan(FrameRequest{
            1920, 1080, {100, 50, 800, 600}, true, false, minimized});
        const auto observed = resolve_frame_plan(FrameRequest{
            1920, 1080, {100, 50, 800, 600}, true, false, foreground});
        auto dpiChanged = foreground;
        dpiChanged.dpi_milli = 2000u;
        dpiChanged.resize_generation = 11u;
        const auto changedWindow = resolve_frame_plan(FrameRequest{
            1920, 1080, {100, 50, 800, 600}, true, false, dpiChanged});
        if (suspended.scene_visible || suspended.surface_presentable
            || !observed.scene_visible || !observed.surface_presentable
            || observed.window != foreground
            || observed.semantic_signature == changedWindow.semantic_signature)
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
