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
#pragma once

namespace epochengine::core
{
    class Context;
}

namespace epochengine::openglstate
{
    struct OpenGL4State;
}

namespace epochengine::openglcontext::PlatformGL
{
    struct PlatformGLContext;
}

namespace epochengine::openglcontext::contextdetail
{
    struct DrawableSize
    {
        int width = 0;
        int height = 0;
        [[nodiscard]] bool valid() const noexcept { return width > 0 && height > 0; }
    };

    PlatformGL::PlatformGLContext state_to_platform_context(const openglstate::OpenGL4State& state) noexcept;
    PlatformGL::PlatformGLContext context_to_platform_context(const core::Context* ctx) noexcept;
    bool drawable_alive(const core::Context* ctx, const openglstate::OpenGL4State& state) noexcept;
    DrawableSize query_drawable_size(const core::Context* ctx, const openglstate::OpenGL4State& state) noexcept;
    DrawableSize query_drawable_size(
        const PlatformGL::PlatformGLContext& active,
        const openglstate::OpenGL4State& state) noexcept;
}
