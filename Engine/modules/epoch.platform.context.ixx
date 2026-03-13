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

#include "../include/_epoch.stl_types.hpp"

export module epoch.platform.context;

import core.error;
import epoch.platform.window;

export namespace epoch::platform
{
    enum class GraphicsBackend : std::uint8_t
    {
        null_backend,
        vulkan,
        d3d12,
        opengl,
    };

    struct ContextDesc
    {
        GraphicsBackend backend = GraphicsBackend::null_backend;
        bool enable_validation = false;
    };

    class IGraphicsContext
    {
    public:
        virtual ~IGraphicsContext() noexcept = default;

        [[nodiscard]] virtual core::error::result<void> create_surface(WindowHandle handle) noexcept = 0;
        virtual void resize_surface(WindowHandle handle, std::int32_t width, std::int32_t height) noexcept = 0;
        virtual void teardown() noexcept = 0;
        [[nodiscard]] virtual GraphicsBackend backend() const noexcept = 0;
    };

    [[nodiscard]] core::error::result<std::unique_ptr<IGraphicsContext>> create_graphics_context(const ContextDesc& desc) noexcept;
} // namespace epoch::platform
