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
//#include "../include/_epoch.stl_types.hpp"
#include <algorithm>
#include <string_view>

export module platform.budgets;

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

        f32 memory_pressure  = 0.0f;
        f32 thermal_pressure = 0.0f;
    };

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
