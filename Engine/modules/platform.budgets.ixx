/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
 /**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
//#include "../include/core.stl_types.hpp"
#include <algorithm>
#include <string_view>

export module platform.budgets;

import perf.tier;

export namespace epochengine
{
    enum class ReconstructionMode : u8
    {
        none,
        temporal,
        ai_assisted
    };

    [[nodiscard]] constexpr std::string_view to_string(const ReconstructionMode mode) noexcept
    {
        switch (mode)
        {
        case ReconstructionMode::none:
            return "none";
        case ReconstructionMode::temporal:
            return "temporal";
        case ReconstructionMode::ai_assisted:
            return "ai_assisted";
        }

        return "temporal";
    }

    struct Canvas2DBudgets final
    {
        u64 maximum_logical_canvas_pixels{1920ull * 1080ull};
        u64 maximum_logical_texture_bytes{256ull * 1024ull * 1024ull};
        u64 maximum_visible_sprites{32'768u};
        u32 maximum_batches{1'024u};
        u32 maximum_collision_surfaces{16'384u};
        u64 maximum_resident_audio_bytes{128ull * 1024ull * 1024ull};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_logical_canvas_pixels != 0u
                && maximum_logical_texture_bytes != 0u
                && maximum_visible_sprites != 0u
                && maximum_batches != 0u
                && maximum_collision_surfaces != 0u
                && maximum_resident_audio_bytes != 0u;
        }
    };

    struct Budgets
    {
        f32 cpu_ms = 6.0f;
        f32 gpu_ms = 10.0f;

        u64 vram_budget_bytes   = 512ull * 1024ull * 1024ull;
        u64 upload_budget_bytes = 32ull  * 1024ull * 1024ull;

        u32 max_lights = 64;
        u32 shadow_cascades = 2;

        u32 min_w = 640,  min_h = 360;
        u32 max_w = 1920, max_h = 1080;

        Canvas2DBudgets canvas2d{};

        f32 memory_pressure  = 0.0f;
        f32 thermal_pressure = 0.0f;
    };

    namespace platform
    {
    [[nodiscard]] constexpr Budgets recommended_budgets_for_tier(
        const perf::tier performance_tier) noexcept
    {
        Budgets budgets{};

        switch (performance_tier)
        {
        case perf::tier::mobile_30:
            budgets.cpu_ms = 10.0f;
            budgets.gpu_ms = 20.0f;
            budgets.max_w = 1280;
            budgets.max_h = 720;
            budgets.max_lights = 32;
            budgets.shadow_cascades = 1;
            budgets.canvas2d = {
                .maximum_logical_canvas_pixels = 1280ull * 720ull,
                .maximum_logical_texture_bytes = 64ull * 1024ull * 1024ull,
                .maximum_visible_sprites = 8'192u,
                .maximum_batches = 256u,
                .maximum_collision_surfaces = 4'096u,
                .maximum_resident_audio_bytes = 32ull * 1024ull * 1024ull};
            break;
        case perf::tier::deck_40:
            budgets.cpu_ms = 8.0f;
            budgets.gpu_ms = 16.0f;
            budgets.max_w = 1600;
            budgets.max_h = 900;
            budgets.max_lights = 48;
            budgets.shadow_cascades = 2;
            budgets.canvas2d = {
                .maximum_logical_canvas_pixels = 1600ull * 900ull,
                .maximum_logical_texture_bytes = 128ull * 1024ull * 1024ull,
                .maximum_visible_sprites = 16'384u,
                .maximum_batches = 512u,
                .maximum_collision_surfaces = 8'192u,
                .maximum_resident_audio_bytes = 64ull * 1024ull * 1024ull};
            break;
        case perf::tier::desktop_60:
            budgets.cpu_ms = 6.0f;
            budgets.gpu_ms = 12.0f;
            budgets.max_w = 1920;
            budgets.max_h = 1080;
            budgets.max_lights = 64;
            budgets.shadow_cascades = 2;
            break;
        case perf::tier::editor_120:
        case perf::tier::uncapped:
        default:
            budgets.cpu_ms = 4.0f;
            budgets.gpu_ms = 8.0f;
            budgets.max_w = 2560;
            budgets.max_h = 1440;
            budgets.max_lights = 96;
            budgets.shadow_cascades = 4;
            budgets.canvas2d = {
                .maximum_logical_canvas_pixels = 2560ull * 1440ull,
                .maximum_logical_texture_bytes = 512ull * 1024ull * 1024ull,
                .maximum_visible_sprites = 65'536u,
                .maximum_batches = 2'048u,
                .maximum_collision_surfaces = 32'768u,
                .maximum_resident_audio_bytes = 256ull * 1024ull * 1024ull};
            break;
        }

        return budgets;
    }

    static_assert(
        recommended_budgets_for_tier(perf::tier::mobile_30)
            .canvas2d.maximum_logical_canvas_pixels == 1280ull * 720ull);
    static_assert(
        recommended_budgets_for_tier(perf::tier::mobile_30)
            .canvas2d.maximum_visible_sprites == 8'192u);
    }

    struct DesktopWindowPolicy
    {
        u32 client_width{1280};
        u32 client_height{800};
        perf::tier capability_tier{perf::tier::desktop_60};
        std::string_view display_class{"1080p"};
    };

    namespace platform
    {
    [[nodiscard]] constexpr DesktopWindowPolicy recommended_desktop_window(
        perf::tier requested_tier,
        const u32 work_area_width,
        const u32 work_area_height,
        const u32 hardware_threads) noexcept
    {
        perf::tier capability_tier = requested_tier;
        if (hardware_threads > 0u && hardware_threads <= 4u)
            capability_tier = perf::tier::mobile_30;
        else if (hardware_threads > 0u && hardware_threads <= 8u
            && capability_tier != perf::tier::mobile_30)
            capability_tier = perf::tier::deck_40;

        DesktopWindowPolicy policy{
            .capability_tier = capability_tier};

        if (work_area_width >= 3'200u && work_area_height >= 1'800u)
        {
            policy.client_width = 1'920u;
            policy.client_height = 1'200u;
            policy.display_class = "4k";
        }
        else if (work_area_width >= 2'300u && work_area_height >= 1'300u)
        {
            policy.client_width = 1'600u;
            policy.client_height = 1'000u;
            policy.display_class = "2k";
        }
        else if (work_area_width >= 1'700u && work_area_height >= 950u)
        {
            policy.client_width = 1'440u;
            policy.client_height = 900u;
            policy.display_class = "1080p";
        }
        else
        {
            policy.client_width = 1'024u;
            policy.client_height = 640u;
            policy.display_class = "compact";
        }

        u32 tier_scale_eighths = 8u;
        switch (capability_tier)
        {
        case perf::tier::mobile_30:
            tier_scale_eighths = 6u;
            break;
        case perf::tier::deck_40:
            tier_scale_eighths = 7u;
            break;
        case perf::tier::desktop_60:
        case perf::tier::editor_120:
        case perf::tier::uncapped:
        default:
            break;
        }

        const u32 tier_width = (std::max)(
            960u,
            policy.client_width * tier_scale_eighths / 8u);
        const u32 tier_height = (std::max)(
            600u,
            policy.client_height * tier_scale_eighths / 8u);
        const u32 available_width = work_area_width > 48u
            ? work_area_width - 48u
            : work_area_width;
        const u32 available_height = work_area_height > 64u
            ? work_area_height - 64u
            : work_area_height;
        policy.client_width = (std::max)(
            1u,
            (std::min)({
                policy.client_width,
                tier_width,
                available_width}));
        policy.client_height = (std::max)(
            1u,
            (std::min)({
                policy.client_height,
                tier_height,
                available_height}));
        return policy;
    }

    static_assert(
        recommended_desktop_window(
            perf::tier::desktop_60, 3'840u, 2'160u, 16u).client_width
        == 1'920u);
    static_assert(
        recommended_desktop_window(
            perf::tier::desktop_60, 2'560u, 1'440u, 16u).client_width
        == 1'600u);
    static_assert(
        recommended_desktop_window(
            perf::tier::desktop_60, 3'840u, 2'160u, 8u).client_width
        == 1'680u);
    static_assert(
        recommended_desktop_window(
            perf::tier::desktop_60, 2'560u, 1'440u, 8u).client_width
        == 1'400u);
    }

    struct FramePolicy
    {
        u32 render_w = 1280;
        u32 render_h = 720;

        u32 max_lights = 64;
        u32 shadow_cascades = 2;

        bool use_visibility_buffer = true;
        bool use_bindless          = false;
        bool use_sparse            = false;
        bool use_async_compute      = false;
        bool use_surface_data      = true;
        bool use_lighting          = true;
        bool use_temporal_history  = true;
        ReconstructionMode reconstruction = ReconstructionMode::temporal;
        bool expose_ai_reconstruction_inputs = false;

        f32 target_fps = 60.0f;
    };

    [[nodiscard]] inline FramePolicy compute_policy(const Budgets& b,
                                                    const bool caps_bindless,
                                                    const bool caps_sparse,
                                                    const bool caps_async,
                                                    const bool caps_visibility_buffer,
                                                    const f32 measured_gpu_ms) noexcept
    {
        FramePolicy p{};
        p.max_lights = b.max_lights;
        p.shadow_cascades = b.shadow_cascades;

        p.use_bindless = caps_bindless;
        p.use_sparse   = caps_sparse;
        p.use_async_compute = caps_async;
        p.use_surface_data = true;
        p.use_lighting = true;
        p.use_temporal_history = true;

        p.use_visibility_buffer = caps_visibility_buffer && (b.memory_pressure < 0.85f);
        if (!caps_visibility_buffer && !caps_bindless)
            p.reconstruction = ReconstructionMode::none;
        else if (caps_bindless && caps_async && caps_visibility_buffer)
            p.reconstruction = ReconstructionMode::ai_assisted;
        else
            p.reconstruction = ReconstructionMode::temporal;
        p.expose_ai_reconstruction_inputs = p.reconstruction == ReconstructionMode::ai_assisted;

        const f32 over = (measured_gpu_ms - b.gpu_ms) / (b.gpu_ms > 0.001f ? b.gpu_ms : 1.0f);
        const f32 bias_down = std::clamp(over, 0.0f, 0.5f);
        const f32 bias_up   = std::clamp(-over, 0.0f, 0.25f);

        f32 scale = 1.0f - bias_down + bias_up;
        scale *= (1.0f - 0.35f * std::clamp(b.thermal_pressure, 0.0f, 1.0f));

        const u32 w = static_cast<u32>(static_cast<f32>(b.max_w) * scale);
        const u32 h = static_cast<u32>(static_cast<f32>(b.max_h) * scale);

        p.render_w = std::clamp(w, b.min_w, b.max_w);
        p.render_h = std::clamp(h, b.min_h, b.max_h);
        return p;
    }
} // namespace epoch
