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
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module epoch.render.device.null;

import epoch.render.device;


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

        BufferHandle create_buffer(const BufferDesc&) override { return BufferHandle{ ++m_buf }; }
        TextureHandle create_texture(const TextureDesc&) override { return TextureHandle{ ++m_tex }; }

        void destroy(BufferHandle) noexcept override {}
        void destroy(TextureHandle) noexcept override {}

        ICommandContext& acquire_graphics_context() override { return m_ctx; }
        void present(ISwapchain&) override {}

    private:
        NullCommandContext m_ctx{};
        std::atomic<u32> m_buf{0};
        std::atomic<u32> m_tex{0};
    };
} // namespace epoch
