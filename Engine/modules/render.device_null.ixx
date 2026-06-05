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

#include <atomic>
#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module render.device_null;

import render.device;


export namespace epoch
{
    class NullCommandContext final : public ICommandContext
    {
    public:
        void begin(const char*) override {}
        void end() override {}
        void debug_marker(const char*) override {}
        void barrier() override {}
    };

    class NullRenderDevice final : public IRenderDevice
    {
    public:
        std::string backend_name() const override { return "null"; }
        RendererCapabilities capabilities() const noexcept override { return renderer_capabilities_for(RendererBackendKind::null); }

        BufferHandle create_buffer(const BufferDesc&) override { return BufferHandle{ ++m_buf }; }
        TextureHandle create_texture(const TextureDesc&) override { return TextureHandle{ ++m_tex }; }
        SamplerHandle create_sampler(const SamplerDesc&) override { return SamplerHandle{ ++m_sampler }; }
        ShaderHandle create_shader(const ShaderDesc&) override { return ShaderHandle{ ++m_shader }; }
        PipelineHandle create_pipeline(const PipelineDesc&) override { return PipelineHandle{ ++m_pipeline }; }
        MaterialHandle create_material(const MaterialDesc&) override { return MaterialHandle{ ++m_material }; }
        RenderTargetHandle create_render_target(const RenderTargetDesc&) override { return RenderTargetHandle{ ++m_render_target }; }

        void destroy(BufferHandle) noexcept override {}
        void destroy(TextureHandle) noexcept override {}
        void destroy(SamplerHandle) noexcept override {}
        void destroy(ShaderHandle) noexcept override {}
        void destroy(PipelineHandle) noexcept override {}
        void destroy(MaterialHandle) noexcept override {}
        void destroy(RenderTargetHandle) noexcept override {}

        ICommandContext& acquire_graphics_context() override { return m_ctx; }
        CommandListHandle begin_command_list(const char*) override { return CommandListHandle{ ++m_command_list }; }
        void end_command_list(CommandListHandle) override {}
        void present(ISwapchain&) override {}

    private:
        NullCommandContext m_ctx{};
        std::atomic<u32> m_buf{0};
        std::atomic<u32> m_tex{0};
        std::atomic<u32> m_sampler{0};
        std::atomic<u32> m_shader{0};
        std::atomic<u32> m_pipeline{0};
        std::atomic<u32> m_material{0};
        std::atomic<u32> m_render_target{0};
        std::atomic<u32> m_command_list{0};
    };
} // namespace epoch
