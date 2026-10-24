// SPDX-License-Identifier: MPL-2.0
// A standalone dependency consumer. No Engine, renderer, GUI or model service.
#include "core.stl_types.hpp"
#include <string_view>
import core.env;
import core.log;
import perf.tier;
import perf.select;
import platform.capabilities;
import platform.budgets;

#define VERIFY(expression) do { if (!(expression)) { \
    epochengine::core::log::error("Platform.TierContract", #expression); return __LINE__; \
} } while (false)

namespace
{
    struct RestoreTier final
    {
        epochengine::optional<epochengine::string> previous{
            epochengine::core::env::get("EPOCH_TIER")};
        ~RestoreTier()
        {
            if (previous)
                (void)epochengine::core::env::set("EPOCH_TIER",
                    epochengine::to_view(epochengine::to_std(*previous)));
            else
                (void)epochengine::core::env::unset("EPOCH_TIER");
        }
    };
}

int main()
{
    using namespace epochengine;
    using namespace epochengine::perf;
    const RestoreTier restore;
    VERIFY(core::env::unset("EPOCH_TIER"));
    Capabilities caps{};
    VERIFY(!caps.indirect_draw && !caps.bindless_textures && !caps.async_compute);
    VERIFY(caps.max_sampled_images == 0 && caps.max_samplers == 0);
    VERIFY(caps.max_storage_buffers == 0 && caps.max_uniform_buffers == 0);
    VERIFY(select_tier(caps) == tier::mobile_30);
    caps.subgroup_ops = true;
    VERIFY(select_tier(caps) == tier::deck_40);
    caps.ray_tracing = true;
    VERIFY(select_tier(caps) == tier::editor_120);
    VERIFY(target_fps_for_caps(caps) == 120.0);
    // Process-local environment access is the complete policy override.
    VERIFY(core::env::set("EPOCH_TIER", "uncapped"));
    VERIFY(tier_from_env() == tier::uncapped && target_fps_for_caps(caps) == 0.0);
    const auto loaded = core::env::get("EPOCH_TIER");
    VERIFY(loaded && to_std(*loaded).compare("uncapped") == 0);
    VERIFY(core::env::set("EPOCH_TIER", "30"));
    VERIFY(select_tier(caps) == tier::mobile_30);
    VERIFY(core::env::set("EPOCH_TIER", "40"));
    VERIFY(select_tier(caps) == tier::deck_40);
    VERIFY(core::env::set("EPOCH_TIER", "desktop"));
    VERIFY(select_tier(caps) == tier::desktop_60);
    VERIFY(core::env::set("EPOCH_TIER", "invalid"));
    VERIFY(tier_from_env() == tier::editor_120);

    const auto unlimited = target_hz_policy(0.0);
    const auto foreground = resolve_frame_pacing_with_capabilities(
        unlimited, frame_activity::foreground, {true, true});
    VERIFY(foreground.effective_mode == frame_pacing_mode::uncapped);
    VERIFY(foreground.effective_hz == 0.0 && !foreground.cpu_deadline_wait);
    const auto raylib = frame_pacing_capabilities_for(frame_pacing_backend::raylib);
    VERIFY(!raylib.target_hz && !raylib.vsync);
    const auto inactive = resolve_frame_pacing_with_capabilities(
        unlimited, frame_activity::background, raylib);
    VERIFY(inactive.effective_hz == 30.0 && inactive.cpu_deadline_wait);
    const auto minimized = resolve_frame_pacing_with_capabilities(
        unlimited, frame_activity::minimized, raylib);
    VERIFY(minimized.effective_hz == 10.0 && minimized.cpu_deadline_wait);
    const auto vsync = select_frame_pacing_policy(frame_pacing_mode::vsync, 120.0, true, false);
    const auto native = resolve_frame_pacing_with_capabilities(vsync, frame_activity::foreground, {true, false});
    VERIFY(native.native_vsync_requested && !native.cpu_deadline_wait);
    const auto fallback = finalize_native_frame_pacing(native, {});
    VERIFY(fallback.effective_hz == 120.0 && fallback.cpu_deadline_wait);
    const auto swap = select_native_present_mode(frame_pacing_mode::uncapped, {true, false, true});
    VERIFY(swap.mode == native_present_mode::immediate && !swap.pacing_active);
    VERIFY(next_frame_deadline(10.0, 10.1, 0.01, true) == 10.1);
    VERIFY(next_frame_deadline(0.0, 10.0, 0.01, false) == 10.01);
    frame_pacing_override selected;
    selected.publish(frame_pacing_mode::uncapped, 0.0);
    VERIFY(with_frame_pacing_override(target_hz_policy(120.0), selected).target_hz == 0.0);
    selected.clear();
    VERIFY(!selected.snapshot().explicit_selection);
    frame_limiter limiter;
    limiter.set_plan(foreground);
    limiter.wait_for_next_frame(); // Actual uncapped path returns without a deadline wait.
    VERIFY(limiter.target_dt == 0.0 && !limiter.started && limiter.next_time == 0.0);

    const auto mobile = platform::recommended_budgets_for_tier(tier::mobile_30);
    const auto desktop = platform::recommended_budgets_for_tier(tier::editor_120);
    VERIFY(mobile.canvas2d.valid() && desktop.canvas2d.valid());
    VERIFY(mobile.canvas2d.maximum_visible_sprites < desktop.canvas2d.maximum_visible_sprites);
    const auto compact = platform::recommended_desktop_window(tier::desktop_60, 800u, 560u, 4u);
    VERIFY(compact.client_width <= 752u && compact.client_height <= 496u);
    auto pressure = desktop;
    pressure.memory_pressure = 1.0f;
    pressure.thermal_pressure = 1.0f;
    const auto bounded = compute_policy(pressure, false, false, false, true, pressure.gpu_ms * 2.0f);
    VERIFY(!bounded.use_visibility_buffer && !bounded.use_bindless);
    VERIFY(bounded.render_w >= pressure.min_w && bounded.render_w <= pressure.max_w);
    VERIFY(bounded.render_h >= pressure.min_h && bounded.render_h <= pressure.max_h);
    double manual = 1500.0;
    VERIFY(parse_frame_limit_value("1500", manual) && manual == 1500.0);
    VERIFY(parse_frame_limit_value(" 5000\t", manual) && manual == 5000.0);
    VERIFY(parse_frame_limit_value("0", manual) && manual == 0.0);
    VERIFY(parse_frame_limit_value("59.94", manual) && manual == 59.94);
    for (const auto invalid : {"", "-1", "nan", "inf", "1e999", "120garbage", "1 20"})
    {
        const double before = manual;
        VERIFY(!parse_frame_limit_value(invalid, manual) && manual == before);
    }
    VERIFY(target_fps_for(frame_limit_preset::fps_30) == 30.0);
    VERIFY(std::string_view{label_for_frame_limit(30.0)} == std::string_view{"30 FPS"});
    return 0;
}
