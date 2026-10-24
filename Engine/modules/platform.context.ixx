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
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
// Epoch Context: window/context state, dispatch and input ownership remain scoped.
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
 /**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/core.stl_types.hpp"

export module platform.context;

import core.error;
import platform.window;
import platform.capabilities;

export namespace epochengine::platform
{
    enum class GraphicsBackend : std::uint8_t
    {
        null_backend,
        vulkan,
        d3d12,
        opengl,
    };

    struct GraphicsContextDesc
    {
        GraphicsBackend backend = GraphicsBackend::null_backend;
        bool enable_validation = false;
    };

    class IGraphicsContext
    {
    public:
        virtual ~IGraphicsContext() noexcept = default;

        [[nodiscard]] virtual core::error::result<void> create_surface(window_adapter::WindowHandle handle) noexcept = 0;
        virtual void resize_surface(window_adapter::WindowHandle handle, std::int32_t width, std::int32_t height) noexcept = 0;
        virtual void teardown() noexcept = 0;
        [[nodiscard]] virtual GraphicsBackend backend() const noexcept = 0;
        // An API name is not hardware evidence. Override only with enabled
        // device capabilities; the default is deliberately unprobed/empty.
        [[nodiscard]] virtual Capabilities capabilities() const noexcept { return {}; }
    };

    [[nodiscard]] core::error::result<std::unique_ptr<IGraphicsContext>> create_graphics_context(const GraphicsContextDesc& desc) noexcept;
} // namespace epochengine::platform
