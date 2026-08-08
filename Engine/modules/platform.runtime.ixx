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
module;

#include "../include/core.stl_types.hpp"
#include <source_location>

export module platform.runtime;

import core.logger;
import platform.engine;
import core.format;
import perf.select;
import perf.tier;
import platform.budgets;
import platform.capabilities;
import platform.context;

export namespace epochengine::platform
{
    struct RuntimeFrameProfile
    {
        epochengine::string platform_key{ "unknown" };
        Capabilities capabilities{};
        Budgets recommended_budgets{};
        FramePolicy frame_policy{};
        epochengine::perf::tier perf_tier = epochengine::perf::tier::desktop_60;
        double target_fps = 60.0;
        bool supports_parented_windows = false;
        bool prefer_single_context_runtime = false;
    };

    [[nodiscard]] inline Capabilities probe_capabilities(const IGraphicsContext* context) noexcept
    {
        Capabilities caps{};
        caps.device_name = "epoch-runtime";

        if (!context)
        {
            caps.api_name = "none";
            return caps;
        }

        switch (context->backend())
        {
        case GraphicsBackend::vulkan:
            caps.api_name = "vulkan";
            caps.descriptor_indexing = true;
            caps.bindless_textures = true;
            caps.indirect_draw = true;
            caps.multi_draw_indirect = true;
            caps.subgroup_ops = true;
            caps.async_compute = true;
            caps.sparse_resources = true;
            caps.max_sampled_images = 8192;
            caps.max_samplers = 4096;
            break;
        case GraphicsBackend::d3d12:
            caps.api_name = "d3d12";
            caps.descriptor_indexing = true;
            caps.bindless_textures = true;
            caps.indirect_draw = true;
            caps.multi_draw_indirect = true;
            caps.subgroup_ops = true;
            caps.async_compute = true;
            caps.max_sampled_images = 8192;
            caps.max_samplers = 4096;
            break;
        case GraphicsBackend::opengl:
            caps.api_name = "opengl";
            caps.indirect_draw = true;
            caps.multi_draw_indirect = true;
            caps.max_sampled_images = 2048;
            caps.max_samplers = 1024;
            break;
        case GraphicsBackend::null_backend:
        default:
            caps.api_name = "null";
            caps.indirect_draw = false;
            caps.max_sampled_images = 512;
            caps.max_samplers = 256;
            caps.max_storage_buffers = 64;
            caps.max_uniform_buffers = 64;
            break;
        }

        return caps;
    }

    [[nodiscard]] inline FramePolicy build_frame_policy(
        const Capabilities& caps,
        const epochengine::perf::tier perf_tier,
        const double target_fps) noexcept
    {
        const auto budgets = recommended_budgets_for_tier(perf_tier);
        auto policy = compute_policy(
            budgets,
            caps.bindless_textures && caps.descriptor_indexing,
            caps.sparse_resources,
            caps.async_compute,
            caps.indirect_draw || caps.multi_draw_indirect,
            budgets.gpu_ms);
        policy.target_fps = target_fps > 0.0 ? static_cast<float>(target_fps) : 0.0f;
        return policy;
    }

    [[nodiscard]] inline RuntimeFrameProfile build_runtime_frame_profile(const IGraphicsContext* context) noexcept
    {
        RuntimeFrameProfile profile{};
        const auto runtime_policy = epochengine::platform::policy::current_runtime_policy();

        profile.platform_key = epochengine::string{ std::string(runtime_policy.platform_key) };
        profile.supports_parented_windows = epochengine::platform::policy::supports_parented_multiwindow();
        profile.prefer_single_context_runtime = epochengine::platform::policy::prefer_single_context_runtime();
        profile.capabilities = probe_capabilities(context);
        profile.perf_tier = epochengine::perf::select_tier(profile.capabilities);
        profile.target_fps = epochengine::perf::target_fps_for(profile.perf_tier);
        profile.recommended_budgets = recommended_budgets_for_tier(profile.perf_tier);
        profile.frame_policy = build_frame_policy(profile.capabilities, profile.perf_tier, profile.target_fps);
        return profile;
    }

    [[nodiscard]] inline epochengine::string runtime_summary(const RuntimeFrameProfile& profile)
    {
        return epochengine::core::format::str(
            "platform={}, api={}, parented_windows={}, single_context_runtime={}, perf_tier={}, target_fps={}",
            epochengine::to_std(profile.platform_key),
            profile.capabilities.api_name,
            profile.supports_parented_windows,
            profile.prefer_single_context_runtime,
            epochengine::perf::to_string(profile.perf_tier),
            profile.target_fps);
    }

    [[nodiscard]] inline epochengine::string frame_policy_summary(const RuntimeFrameProfile& profile)
    {
        return epochengine::core::format::str(
            "frame policy={}x{}, lights={}, lighting={}, temporal={}, reconstruction={}, ai_inputs={}, visibility={}, bindless={}, async={}",
            profile.frame_policy.render_w,
            profile.frame_policy.render_h,
            profile.frame_policy.max_lights,
            profile.frame_policy.use_lighting,
            profile.frame_policy.use_temporal_history,
            epochengine::to_string(profile.frame_policy.reconstruction),
            profile.frame_policy.expose_ai_reconstruction_inputs,
            profile.frame_policy.use_visibility_buffer,
            profile.frame_policy.use_bindless,
            profile.frame_policy.use_async_compute);
    }

    inline void log_runtime_profile(
        const std::string_view runtime_channel,
        const std::string_view perf_channel,
        const RuntimeFrameProfile& profile)
    {
        epochengine::logger::get(runtime_channel).log(
            epochengine::logger::LogLevel::INFO,
            runtime_summary(profile).impl,
            std::source_location::current());

        epochengine::logger::get(perf_channel).log(
            epochengine::logger::LogLevel::INFO,
            frame_policy_summary(profile).impl,
            std::source_location::current());
    }
}
