#pragma once

namespace epochnamespace::core
{
    class Context;
}

namespace epochnamespace::openglstate
{
    struct OpenGL4State;
}

namespace epochnamespace::openglcontext::PlatformGL
{
    struct PlatformGLContext;
}

namespace epochnamespace::openglcontext::contextdetail
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
