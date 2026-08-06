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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// -----------------------------------------------------------------------------
// Global module fragment: macros + platform / C headers live here.
// -----------------------------------------------------------------------------

#include "../include/engine.config.hpp"

#if defined(__linux__)
#include <X11/X.h>
#endif

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)

// Make sure GL loaders see any platform defines they need.

// Prefer GLAD (what youÃ¢â‚¬â„¢re already using elsewhere). This provides GLuint,
// GLenum and all gl* function prototypes.
#if defined(__has_include)
#  if __has_include(<glad/glad.h>)
#    include <glad/glad.h>
#  else
     // Fallback (not ideal, but better than nothing)
#    if defined(_WIN32)
#      include <GL/gl.h>
#    elif defined(__linux__)
#      include <GL/gl.h>
#    endif
#  endif
#else
#  include <glad/glad.h>
#endif

#endif // EPOCH_USING_OPENGL

export module opengl.textures;

import engine.platform;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1) && (EPOCH_USING_OPENGL == 1)

import engine.cli;
import context.type;
import core.context;
import context.multiplexer;

import opengl.platform;
import opengl.state;
import opengl.quad;
import atlas.manager;
import atlas.texture;
import texture;
import image.loader;
import spritehandle;
import core.logger;
import render.device;
import render.device_opengl_family;

// If u32/u64 are yours and not from <cstdint>, you must import the module that
// defines them. Uncomment the correct one in your project.
// import atypes;

export namespace epochengine::opengltextures
{
    namespace detail
    {
        inline epochengine::openglcontext::PlatformGL::PlatformGLContext
            to_platform_context(const epochengine::openglstate::OpenGL4State& state) noexcept
        {
            epochengine::openglcontext::PlatformGL::PlatformGLContext ctx{};
#if defined(_WIN32)
            ctx.device = static_cast<decltype(ctx.device)>(state.hdc);
            ctx.context = static_cast<decltype(ctx.context)>(state.hglrc);
#elif defined(__linux__)
            ctx.display = static_cast<decltype(ctx.display)>(state.display);
            ctx.drawable = state.drawable ? state.drawable : state.window;
            ctx.context = static_cast<decltype(ctx.context)>(state.glxContext);
#endif
            return ctx;
        }

        inline epochengine::openglcontext::PlatformGL::PlatformGLContext
            context_to_platform_context(const core::Context* ctx) noexcept
        {
            epochengine::openglcontext::PlatformGL::PlatformGLContext result{};
            if (!ctx) return result;

#if defined(_WIN32)
            result.device = static_cast<decltype(result.device)>(ctx->native_drawable);
            result.context = static_cast<decltype(result.context)>(ctx->native_gl_context);
#elif defined(__linux__)
            result.display = static_cast<decltype(result.display)>(ctx->native_drawable);
            result.drawable = static_cast<decltype(result.drawable)>(reinterpret_cast<std::uintptr_t>(ctx->native_window));
            result.context = static_cast<decltype(result.context)>(ctx->native_gl_context);
#endif
            return result;
        }
    }

    struct AtlasGPU
    {
        GLuint textureHandle = 0;
        u64 version = static_cast<u64>(-1);  // force mismatch on first compare
        u32 width = 0;
        u32 height = 0;
    };

    struct NativeRenderTextureGPU
    {
        GLuint framebuffer = 0;
        GLuint color = 0;
        GLuint depth = 0;
        GLuint sampler = 0;
        u32 width = 0;
        u32 height = 0;
        bool has_depth = false;
        bool active = false;
    };
    struct NativeTextureGPU
    {
        GLuint texture = 0;
        const void* context_key = nullptr;
        epochengine::TextureDesc desc{};
        bool active = false;
    };

    struct NativeTextureKey final
    {
        const void* context_key{};
        GLuint texture{};

        friend bool operator==(NativeTextureKey, NativeTextureKey) noexcept = default;
    };

    struct NativeTextureKeyHash final
    {
        [[nodiscard]] std::size_t operator()(NativeTextureKey key) const noexcept
        {
            const std::size_t context = std::hash<const void*>{}(key.context_key);
            const std::size_t texture = std::hash<GLuint>{}(key.texture);
            return context ^ (texture + static_cast<std::size_t>(0x9e3779b9u)
                + (context << 6) + (context >> 2));
        }
    };


    struct TextureAtlasPtrHash {
        size_t operator()(const TextureAtlas* atlas) const noexcept {
            return std::hash<const TextureAtlas*>{}(atlas);
        }
    };

    struct TextureAtlasPtrEqual {
        bool operator()(const TextureAtlas* lhs, const TextureAtlas* rhs) const noexcept {
            return lhs == rhs;
        }
    };

    using AtlasGpuMap = std::unordered_map<const TextureAtlas*, AtlasGPU,
        TextureAtlasPtrHash, TextureAtlasPtrEqual>;

    inline AtlasGpuMap opengl_gpu_atlases;

    struct BackendData {
        AtlasGpuMap gpu_atlases;
        std::unordered_map<const void*, AtlasGpuMap> context_gpu_atlases;
        std::unordered_map<const void*, std::unique_ptr<epochengine::openglstate::OpenGL4State>> context_gl_states;
        std::unordered_map<u32, NativeRenderTextureGPU> native_render_textures;
        std::unordered_map<NativeTextureKey, NativeTextureGPU,
            NativeTextureKeyHash> native_textures;
        std::mutex gpuMutex;
        epochengine::openglstate::OpenGL4State glState{};
    };

    [[nodiscard]] inline const void* platform_context_key(
        const epochengine::openglcontext::PlatformGL::PlatformGLContext& ctx) noexcept
    {
#if defined(_WIN32) || defined(__linux__)
        return ctx.context ? static_cast<const void*>(ctx.context) : nullptr;
#else
        (void)ctx;
        return nullptr;
#endif
    }

    inline void bind_platform_state(
        epochengine::openglstate::OpenGL4State& state,
        const epochengine::openglcontext::PlatformGL::PlatformGLContext& platformCtx,
        const core::Context* ctx) noexcept
    {
#if defined(_WIN32)
        state.hdc = platformCtx.device;
        state.hglrc = platformCtx.context;
        if (ctx && ctx->native_window)
            state.hwnd = static_cast<HWND>(ctx->native_window);
#elif defined(__linux__)
        state.display = platformCtx.display;
        state.drawable = platformCtx.drawable;
        state.glxContext = platformCtx.context;
        if (platformCtx.drawable)
            state.window = static_cast<::Window>(platformCtx.drawable);
#endif

        int width = static_cast<int>(state.width);
        int height = static_cast<int>(state.height);
        if (ctx)
        {
            if (ctx->framebufferWidth > 0 && ctx->framebufferHeight > 0)
            {
                width = ctx->framebufferWidth;
                height = ctx->framebufferHeight;
            }
            else
            {
                width = (std::max)(width, ctx->get_width_safe());
                height = (std::max)(height, ctx->get_height_safe());
            }
        }

        state.width = static_cast<unsigned int>((std::max)(1, width));
        state.height = static_cast<unsigned int>((std::max)(1, height));
    }

    [[nodiscard]] inline epochengine::openglstate::OpenGL4State& state_for_platform_context(
        BackendData& backend,
        const epochengine::openglcontext::PlatformGL::PlatformGLContext& platformCtx,
        const core::Context* ctx)
    {
        const void* key = platform_context_key(platformCtx);
        if (!key)
        {
            bind_platform_state(backend.glState, platformCtx, ctx);
            return backend.glState;
        }

        epochengine::openglstate::OpenGL4State* state = nullptr;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto& statePtr = backend.context_gl_states[key];
            if (!statePtr)
                statePtr = std::make_unique<epochengine::openglstate::OpenGL4State>();
            state = statePtr.get();
        }

        bind_platform_state(*state, platformCtx, ctx);
        return *state;
    }

    [[nodiscard]] inline AtlasGpuMap& atlas_map_for_platform_context(
        BackendData& backend,
        const epochengine::openglcontext::PlatformGL::PlatformGLContext& platformCtx)
    {
        if (const void* key = platform_context_key(platformCtx))
            return backend.context_gpu_atlases[key];

        return backend.gpu_atlases;
    }

    inline BackendData& get_opengl_backend() {
        BackendData* data = nullptr;
        {
            std::unique_lock<std::shared_mutex> lock{ epochengine::core::g_backendsMutex };
            auto& backend = epochengine::core::g_backends[epochengine::core::ContextType::OpenGL];
            if (!backend.data) {
                backend.data = {
                    new BackendData(),
                    [](void* p) { delete static_cast<BackendData*>(p); }
                };
            }
            data = static_cast<BackendData*>(backend.data.get());
        }
        return *data;
    }

    [[nodiscard]] inline BackendData& resolve_backend_data(void* user) noexcept
    {
        if (user)
            return *static_cast<BackendData*>(user);

        return get_opengl_backend();
    }

    [[nodiscard]] inline bool activate_backend_context(
        BackendData& backend,
        epochengine::openglcontext::PlatformGL::ScopedContext& contextGuard,
        std::string_view tag) noexcept
    {
        const auto platformCtx = detail::to_platform_context(backend.glState);
        if (!platformCtx.valid())
        {
            logger::warnf_loc("OpenGL.RTT", std::source_location::current(), "{} skipped: no native GL context is registered.", tag);
            return false;
        }

        if (!contextGuard.set(platformCtx))
        {
            logger::warnf_loc("OpenGL.RTT", std::source_location::current(), "{} skipped: failed to activate GL context.", tag);
            return false;
        }

        return true;
    }

    struct NativeTextureFormat final
    {
        GLint internal_format{};
        GLenum external_format{};
        GLenum component_type{};
        u32 bytes_per_texel{};
    };

    [[nodiscard]] inline bool native_texture_format(
        epochengine::TextureFormat format,
        NativeTextureFormat& output) noexcept
    {
        switch (format)
        {
        case epochengine::TextureFormat::r8_unorm:
            output = { GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1u };
            return true;
        case epochengine::TextureFormat::r16_float:
            output = { GL_R16F, GL_RED, GL_HALF_FLOAT, 2u };
            return true;
        case epochengine::TextureFormat::r32_uint:
            output = { GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4u };
            return true;
        case epochengine::TextureFormat::rgba8_unorm:
            output = { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u };
            return true;
        case epochengine::TextureFormat::bgra8_unorm:
            output = { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4u };
            return true;
        case epochengine::TextureFormat::rgba16_float:
            output = { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8u };
            return true;
        case epochengine::TextureFormat::rgba32_float:
            output = { GL_RGBA32F, GL_RGBA, GL_FLOAT, 16u };
            return true;
        default:
            output = {};
            return false;
        }
    }

    struct ScopedNativeTextureState final
    {
        GLint texture_binding{};
        GLint unpack_alignment{4};
        GLint unpack_row_length{};
        GLint unpack_buffer{};
        GLint unpack_skip_pixels{};
        GLint unpack_skip_rows{};
        GLboolean unpack_swap_bytes{GL_FALSE};

        ScopedNativeTextureState() noexcept
        {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_length);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpack_skip_pixels);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &unpack_skip_rows);
            glGetBooleanv(GL_UNPACK_SWAP_BYTES, &unpack_swap_bytes);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
        }

        ScopedNativeTextureState(const ScopedNativeTextureState&) = delete;
        ScopedNativeTextureState& operator=(const ScopedNativeTextureState&) = delete;

        ~ScopedNativeTextureState() noexcept
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpack_buffer));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_binding));
            glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpack_skip_pixels);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, unpack_skip_rows);
            glPixelStorei(GL_UNPACK_SWAP_BYTES, unpack_swap_bytes);
        }
    };

    inline void clear_gl_errors() noexcept
    {
        while (glGetError() != GL_NO_ERROR)
        {
        }
    }

    inline void delete_native_texture_objects(NativeTextureGPU& gpu) noexcept
    {
        if (gpu.texture)
            glDeleteTextures(1, &gpu.texture);
        gpu = {};
    }

    [[nodiscard]] inline epochengine::OpenGLFamilyNativeTextureAllocation
        allocate_native_texture(
            void* user,
            epochengine::RendererBackendKind,
            const epochengine::TextureDesc& desc,
            u32 slot)
    {
        epochengine::OpenGLFamilyNativeTextureAllocation allocation{};
        NativeTextureFormat format{};
        if (desc.width == 0u || desc.height == 0u || desc.mip_levels == 0u
            || desc.mip_levels > 32u || desc.depth_stencil
            || !native_texture_format(desc.format, format))
        {
            return allocation;
        }

        BackendData& backend = resolve_backend_data(user);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "allocate texture"))
            return allocation;

        const auto platformContext = detail::to_platform_context(backend.glState);
        const void* const contextKey = platform_context_key(platformContext);
        if (!contextKey)
            return allocation;
        ScopedNativeTextureState stateGuard{};
        NativeTextureGPU gpu{};
        gpu.context_key = contextKey;
        gpu.desc = desc;
        gpu.desc.debug_name = nullptr;
        clear_gl_errors();

        glGenTextures(1, &gpu.texture);
        if (!gpu.texture)
            return allocation;
        glBindTexture(GL_TEXTURE_2D, gpu.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            desc.mip_levels > 1u ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(desc.mip_levels - 1u));

        for (u32 mip = 0; mip < desc.mip_levels; ++mip)
        {
            const u32 width = (std::max)(1u, desc.width >> mip);
            const u32 height = (std::max)(1u, desc.height >> mip);
            glTexImage2D(
                GL_TEXTURE_2D,
                static_cast<GLint>(mip),
                format.internal_format,
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height),
                0,
                format.external_format,
                format.component_type,
                nullptr);
        }

        const GLenum allocationError = glGetError();
        if (allocationError != GL_NO_ERROR || glIsTexture(gpu.texture) != GL_TRUE)
        {
            delete_native_texture_objects(gpu);
            return allocation;
        }

        gpu.active = true;
        allocation.texture_object = gpu.texture;
        allocation.ready = true;
        allocation.context_key = contextKey;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            backend.native_textures[NativeTextureKey{contextKey, gpu.texture}] = gpu;
        }
        static_cast<void>(slot);
        return allocation;
    }

    [[nodiscard]] inline bool upload_native_texture(
        void* user,
        epochengine::RendererBackendKind,
        const epochengine::OpenGLFamilyTextureRecord& record,
        const epochengine::TextureUploadDesc& upload)
    {
        if (!record.native_work_order_ready() || !epochengine::valid(upload)
            || upload.format != record.desc.format
            || upload.mip_level >= record.desc.mip_levels)
        {
            return false;
        }

        NativeTextureFormat format{};
        if (!native_texture_format(upload.format, format)
            || format.bytes_per_texel == 0u)
        {
            return false;
        }
        const u64 tightRowBytes64 = static_cast<u64>(upload.width)
            * static_cast<u64>(format.bytes_per_texel);
        if (tightRowBytes64 > static_cast<u64>(~u32{0}))
            return false;
        const u32 tightRowBytes = static_cast<u32>(tightRowBytes64);
        const u32 rowPitch = upload.row_pitch_bytes == 0u
            ? tightRowBytes
            : upload.row_pitch_bytes;
        if (rowPitch < tightRowBytes || rowPitch % format.bytes_per_texel != 0u)
            return false;

        BackendData& backend = resolve_backend_data(user);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "upload texture"))
            return false;

        const auto platformContext = detail::to_platform_context(backend.glState);
        const void* const contextKey = platform_context_key(platformContext);
        if (!contextKey || contextKey != record.native_context_key)
            return false;
        std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
        const auto stored = backend.native_textures.find(
            NativeTextureKey{contextKey, record.texture_object});
        if (stored == backend.native_textures.end()
            || !stored->second.active
            || glIsTexture(record.texture_object) != GL_TRUE)
        {
            return false;
        }

        ScopedNativeTextureState stateGuard{};
        glBindTexture(GL_TEXTURE_2D, record.texture_object);
        clear_gl_errors();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(
            GL_UNPACK_ROW_LENGTH,
            rowPitch == tightRowBytes
                ? 0
                : static_cast<GLint>(rowPitch / format.bytes_per_texel));
        glTexSubImage2D(
            GL_TEXTURE_2D,
            static_cast<GLint>(upload.mip_level),
            static_cast<GLint>(upload.x),
            static_cast<GLint>(upload.y),
            static_cast<GLsizei>(upload.width),
            static_cast<GLsizei>(upload.height),
            format.external_format,
            format.component_type,
            upload.data);
        return glGetError() == GL_NO_ERROR;
    }

    inline void destroy_native_texture(
        void* user,
        epochengine::RendererBackendKind,
        const epochengine::OpenGLFamilyTextureRecord& record)
    {
        BackendData& backend = resolve_backend_data(user);
        const NativeTextureKey key{
            record.native_context_key,
            record.texture_object};
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "destroy texture"))
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            backend.native_textures.erase(key);
            return;
        }

        const auto platformContext = detail::to_platform_context(backend.glState);
        const void* const contextKey = platform_context_key(platformContext);
        std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
        const auto it = backend.native_textures.find(key);
        if (it == backend.native_textures.end())
            return;
        if (contextKey == record.native_context_key)
        {
            delete_native_texture_objects(it->second);
        }
        backend.native_textures.erase(it);
    }

    [[nodiscard]] inline epochengine::OpenGLFamilyNativeTextureHooks
        make_native_texture_hooks() noexcept
    {
        epochengine::OpenGLFamilyNativeTextureHooks hooks{};
        hooks.user = &get_opengl_backend();
        hooks.allocate = &allocate_native_texture;
        hooks.upload = &upload_native_texture;
        hooks.destroy = &destroy_native_texture;
        return hooks;
    }

    inline void delete_native_render_texture_objects(NativeRenderTextureGPU& gpu) noexcept
    {
        if (gpu.framebuffer)
        {
            glDeleteFramebuffers(1, &gpu.framebuffer);
            gpu.framebuffer = 0;
        }

        if (gpu.color)
        {
            glDeleteTextures(1, &gpu.color);
            gpu.color = 0;
        }

        if (gpu.depth)
        {
            glDeleteRenderbuffers(1, &gpu.depth);
            gpu.depth = 0;
        }

        if (gpu.sampler)
        {
            glDeleteSamplers(1, &gpu.sampler);
            gpu.sampler = 0;
        }

        gpu.active = false;
    }

    [[nodiscard]] inline epochengine::OpenGLFamilyNativeRenderTextureAllocation allocate_native_render_texture(
        void* user,
        epochengine::RendererBackendKind,
        const epochengine::RenderTextureAssetDesc& desc,
        const epochengine::RenderTextureBackendRequirements& requirements,
        u32 slot)
    {
        epochengine::OpenGLFamilyNativeRenderTextureAllocation allocation{};
        BackendData& backend = resolve_backend_data(user);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "allocate render texture"))
            return allocation;

        NativeRenderTextureGPU gpu{};
        gpu.width = (std::max)(desc.width, 1u);
        gpu.height = (std::max)(desc.height, 1u);
        gpu.has_depth = requirements.depth_attachment;

        glGenFramebuffers(1, &gpu.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, gpu.framebuffer);

        glGenTextures(1, &gpu.color);
        glBindTexture(GL_TEXTURE_2D, gpu.color);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(gpu.width),
            static_cast<GLsizei>(gpu.height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gpu.color, 0);

        if (gpu.has_depth)
        {
            glGenRenderbuffers(1, &gpu.depth);
            glBindRenderbuffer(GL_RENDERBUFFER, gpu.depth);
            glRenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH24_STENCIL8,
                static_cast<GLsizei>(gpu.width),
                static_cast<GLsizei>(gpu.height));
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gpu.depth);
        }

        if (requirements.sampler)
        {
            glGenSamplers(1, &gpu.sampler);
            glSamplerParameteri(gpu.sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glSamplerParameteri(gpu.sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(gpu.sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(gpu.sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            logger::warnf_loc(
                "OpenGL.RTT",
                std::source_location::current(),
                "Framebuffer setup failed for render texture slot {}: status 0x{:x}.",
                slot,
                static_cast<unsigned int>(status));
            delete_native_render_texture_objects(gpu);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            return allocation;
        }

        gpu.active = true;
        allocation.framebuffer_object = gpu.framebuffer;
        allocation.color_object = gpu.color;
        allocation.depth_object = gpu.depth;
        allocation.sampler_object = gpu.sampler ? gpu.sampler : gpu.color;
        allocation.ready = true;

        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto& stored = backend.native_render_textures[slot];
            if (stored.active)
                delete_native_render_texture_objects(stored);
            stored = gpu;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        return allocation;
    }

    inline void destroy_native_render_texture(
        void* user,
        epochengine::RendererBackendKind,
        const epochengine::OpenGLFamilyRenderTextureRecord& record)
    {
        BackendData& backend = resolve_backend_data(user);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "destroy render texture"))
            return;

        std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
        for (auto it = backend.native_render_textures.begin(); it != backend.native_render_textures.end(); ++it)
        {
            NativeRenderTextureGPU& gpu = it->second;
            if (gpu.framebuffer == record.framebuffer_object || gpu.color == record.color_object)
            {
                delete_native_render_texture_objects(gpu);
                backend.native_render_textures.erase(it);
                return;
            }
        }
    }

    [[nodiscard]] inline bool begin_native_render_texture_pass(
        void* user,
        epochengine::RendererBackendKind,
        const epochengine::OpenGLFamilyRenderTextureRecord& record,
        const epochengine::RenderPassDesc& pass)
    {
        BackendData& backend = resolve_backend_data(user);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "begin render texture pass"))
            return false;

        glBindFramebuffer(GL_FRAMEBUFFER, record.framebuffer_object);
        glViewport(0, 0, static_cast<GLsizei>(record.width), static_cast<GLsizei>(record.height));
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        GLbitfield clearMask = 0;
        if (pass.clear_color)
        {
            glClearColor(pass.clear[0], pass.clear[1], pass.clear[2], pass.clear[3]);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
        if (pass.clear_depth && record.depth_object)
        {
            glClearDepth(1.0);
            clearMask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        }
        if (clearMask != 0)
            glClear(clearMask);

        return true;
    }

    inline void end_native_render_texture_pass(
        void* user,
        epochengine::RendererBackendKind,
        const epochengine::OpenGLFamilyRenderTextureRecord&)
    {
        BackendData& backend = resolve_backend_data(user);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!activate_backend_context(backend, contextGuard, "end render texture pass"))
            return;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    [[nodiscard]] inline epochengine::OpenGLFamilyNativeRenderTextureHooks make_native_render_texture_hooks() noexcept
    {
        epochengine::OpenGLFamilyNativeRenderTextureHooks hooks{};
        hooks.user = &get_opengl_backend();
        hooks.allocate = &allocate_native_render_texture;
        hooks.destroy = &destroy_native_render_texture;
        hooks.begin_pass = &begin_native_render_texture_pass;
        hooks.end_pass = &end_native_render_texture_pass;
        return hooks;
    }

    using Handle = uint32_t;

    inline std::atomic_uint8_t  s_generation{ 1 };
    [[nodiscard]] inline Handle make_handle(int atlasIdx, int localIdx) noexcept {
        return (Handle(s_generation.load(std::memory_order_relaxed)) << 24)
            | ((atlasIdx & 0xFFF) << 12)
            | (localIdx & 0xFFF);
    }

    [[nodiscard]] inline bool is_handle_live(Handle h) noexcept {
        return uint8_t(h >> 24) == s_generation.load(std::memory_order_relaxed);
    }

    [[nodiscard]] inline ImageData ensure_rgba(const ImageData& img) {
        const size_t pixelCount = static_cast<size_t>(img.width) * img.height;
        const size_t channels = img.pixels.size() / pixelCount;

        if (channels == 4) return img;

        if (channels != 3)
            throw std::runtime_error("ensure_rgba(): Unsupported channel count: " + std::to_string(channels));

        std::vector<uint8_t> rgba(pixelCount * 4);
        const uint8_t* src = img.pixels.data();
        uint8_t* dst = rgba.data();

        for (size_t i = 0; i < pixelCount; ++i) {
            dst[4 * i + 0] = src[3 * i + 0];
            dst[4 * i + 1] = src[3 * i + 1];
            dst[4 * i + 2] = src[3 * i + 2];
            dst[4 * i + 3] = 255;
        }

        return { std::move(rgba), img.width, img.height, 4 };
    }

    [[nodiscard]] inline bool upload_atlas_to_gpu_for_context(
        BackendData& backend,
        epochengine::openglstate::OpenGL4State& glState,
        const epochengine::openglcontext::PlatformGL::PlatformGLContext& requestedCtx,
        const TextureAtlas& atlas)
    {
        if (atlas.pixel_data.empty()) {
            logger::warnf_loc("OpenGL.Upload", std::source_location::current(), "Pixel data empty for '{}', rebuilding", atlas.name);
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();
        }

        auto platformCtx = requestedCtx.valid()
            ? requestedCtx
            : detail::to_platform_context(glState);
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!contextGuard.set(platformCtx)) {
            logger::error("OpenGL.Upload", "Failed to activate GL context for upload.");
            return false;
        }

        std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
        auto& gpu = atlas_map_for_platform_context(backend, platformCtx)[&atlas];

        if (!gpu.textureHandle) {
            glGenTextures(1, &gpu.textureHandle);
            if (!gpu.textureHandle) {
                logger::errorf_loc("OpenGL.Upload", std::source_location::current(), "Failed to generate texture for atlas '{}'", atlas.name);
                return false;
            }
        }

        if (gpu.version == atlas.version) {
            return true;
        }

        glBindTexture(GL_TEXTURE_2D, gpu.textureHandle);

        if (gpu.width != atlas.width || gpu.height != atlas.height) {
#ifdef GL_ARB_texture_storage
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, atlas.width, atlas.height);
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                atlas.width, atlas.height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#endif
            gpu.width = atlas.width;
            gpu.height = atlas.height;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            atlas.width, atlas.height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            atlas.pixel_data.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gpu.version = atlas.version;

#if EPOCH_ENABLE_BACKEND_UPLOAD_CONFIRMATION_LOGS && EPOCH_ENABLE_OPENGL_CONFIRMATION_LOGS
        logger::infof_loc(
            "OpenGL.Upload",
            std::source_location::current(),
            "Uploaded atlas '{}' (tex id {}, version {})",
            atlas.name,
            gpu.textureHandle,
            gpu.version);
#endif

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    inline void upload_atlas_to_gpu(const TextureAtlas& atlas)
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock{ core::g_backendsMutex };
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }
        if (!oglData) {
            logger::error("OpenGL.Upload", "OpenGL backend data not initialized.");
            return;
        }

        auto platformCtx = detail::to_platform_context(oglData->glState);
        static_cast<void>(upload_atlas_to_gpu_for_context(
            *oglData,
            oglData->glState,
            platformCtx,
            atlas));
    }

    inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock{ core::g_backendsMutex };
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }
        if (!oglData) {
            logger::error("OpenGL.Upload", "OpenGL backend data not initialized.");
            return;
        }

        {
            std::lock_guard<std::mutex> gpuLock(oglData->gpuMutex);
            auto it = oglData->gpu_atlases.find(&atlas);
            if (it != oglData->gpu_atlases.end()) {
                if (it->second.version == atlas.version && it->second.textureHandle != 0)
                    return;
            }
        }
        upload_atlas_to_gpu(atlas);
    }

    inline void ensure_uploaded_for_context(
        BackendData& backend,
        epochengine::openglstate::OpenGL4State& glState,
        const epochengine::openglcontext::PlatformGL::PlatformGLContext& platformCtx,
        const TextureAtlas& atlas)
    {
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto& atlasMap = atlas_map_for_platform_context(backend, platformCtx);
            auto it = atlasMap.find(&atlas);
            if (it != atlasMap.end()) {
                if (it->second.version == atlas.version && it->second.textureHandle != 0)
                    return;
            }
        }

        static_cast<void>(upload_atlas_to_gpu_for_context(backend, glState, platformCtx, atlas));
    }

    inline bool ensure_created_pipeline(epochengine::openglstate::OpenGL4State& glState)
    {
        return epochengine::openglquad::ensure_quad_pipeline(glState);
    }

    inline void clear_gpu_atlases() noexcept
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock{ core::g_backendsMutex };
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }

        if (oglData) {
            std::lock_guard<std::mutex> gpuLock(oglData->gpuMutex);
            auto deleteAtlasMap = [](AtlasGpuMap& atlases) noexcept {
                for (auto& [_, gpu] : atlases) {
                    if (gpu.textureHandle) {
                        glDeleteTextures(1, &gpu.textureHandle);
                    }
                }
                atlases.clear();
            };

            deleteAtlasMap(oglData->gpu_atlases);
            oglData->native_textures.clear();
            for (auto& [_, atlasMap] : oglData->context_gpu_atlases)
                deleteAtlasMap(atlasMap);
            oglData->gpu_atlases.clear();
            oglData->context_gpu_atlases.clear();
        }

        s_generation.fetch_add(1, std::memory_order_relaxed);
    }

    inline Handle load_atlas(const TextureAtlas& atlas, int atlasIndex = -1)
    {
        atlasmanager::ensure_uploaded(atlas);
        const int resolvedIndex = (atlasIndex >= 0) ? atlasIndex : atlas.get_index();
        return make_handle(resolvedIndex, 0);
    }

    inline Handle atlas_add_texture(TextureAtlas& atlas, const std::string& id, const ImageData& img)
    {
        auto rgba = ensure_rgba(img);

        Texture texture{
            .width = static_cast<uint32_t>(rgba.width),
            .height = static_cast<uint32_t>(rgba.height),
            .pixels = std::move(rgba.pixels)
        };

        auto addedOpt = atlas.add_entry(id, texture);
        if (!addedOpt) {
            throw std::runtime_error("atlas_add_texture: Failed to add texture: " + id);
        }

        atlasmanager::ensure_uploaded(atlas);

        return make_handle(atlas.get_index(), addedOpt->index);
    }

    inline uint32_t upload_texture(const uint8_t* pixels, int width, int height)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        glBindTexture(GL_TEXTURE_2D, 0);

        return static_cast<uint32_t>(tex);
    }

    inline void draw_sprite(SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        // (unchanged from your version)
        auto log_draw_skip = [](std::string_view) {};

        if (!handle.is_valid()) {
            logger::error("OpenGL.DrawSprite", "Invalid sprite handle.");
            return;
        }

        auto& backend = get_opengl_backend();
        epochengine::openglcontext::PlatformGL::ScopedContext contextGuard;

        auto currentCtx = core::MultiContextManager::GetCurrent();
        auto desired = detail::context_to_platform_context(currentCtx.get());
        const auto current = epochengine::openglcontext::PlatformGL::get_current();
        if (!desired.valid()) {
            desired = detail::to_platform_context(backend.glState);
        }
        if (!desired.valid() && current.valid()) {
            desired = current;
        }

        if (desired.valid() && current != desired) {
            if (!contextGuard.set(desired)) {
                log_draw_skip("activate_context");
                return;
            }
        } else if (!current.valid()) {
            log_draw_skip("missing_context");
            return;
        }

        auto& glState = state_for_platform_context(backend, desired, currentCtx.get());
        if (!ensure_created_pipeline(glState)) {
            log_draw_skip("missing_pipeline");
            return;
        }

        int w = static_cast<int>(glState.width);
        int h = static_cast<int>(glState.height);
        if (auto ctx = currentCtx) {
            if (ctx->framebufferWidth > 0 && ctx->framebufferHeight > 0) {
                w = ctx->framebufferWidth;
                h = ctx->framebufferHeight;
            } else {
                w = (std::max)(w, ctx->get_width_safe());
                h = (std::max)(h, ctx->get_height_safe());
            }
        }
        if (w <= 0 || h <= 0) {
            w = (std::max)(1, core::cli::window_width);
            h = (std::max)(1, core::cli::window_height);
        }
        if (w <= 0 || h <= 0) {
            log_draw_skip("invalid_dimensions");
            return;
        }

        glState.width = static_cast<unsigned int>(w);
        glState.height = static_cast<unsigned int>(h);
        glViewport(0, 0, w, h);
        glDisable(GL_SCISSOR_TEST);

        const int atlasIdx = int(handle.atlasIndex);
        const int localIdx = int(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= int(atlases.size())) {
            log_draw_skip("atlas_index_oob");
            return;
        }
        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas) {
            log_draw_skip("null_atlas");
            return;
        }

        AtlasRegion region{};
        std::string spriteName;
        if (!atlas->try_get_entry_info(localIdx, region, &spriteName)) {
            log_draw_skip("sprite_index_oob");
            return;
        }

        ensure_uploaded_for_context(backend, glState, desired, *atlas);

        GLuint tex = 0;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto& atlasMap = atlas_map_for_platform_context(backend, desired);
            auto it = atlasMap.find(atlas);
            if (it == atlasMap.end()) {
                log_draw_skip("gpu_texture_missing");
                return;
            }
            tex = it->second.textureHandle;
        }
        if (!tex) {
            log_draw_skip("gpu_texture_missing");
            return;
        }

        auto& pipe = epochengine::openglquad::quad_pipeline_state();
        glUseProgram(pipe.shader);
        glBindVertexArray(pipe.vao);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const float drawWidth = width;
        const float drawHeight = height;
        const float drawX = x;
        const float drawY = y;

        const float u0 = region.u1;
        const float du = region.u2 - region.u1;
        const float v0 = 1.0f - region.v1;
        const float dv = region.v1 - region.v2;

        if (pipe.uUVRegionLoc >= 0)
            glUniform4f(pipe.uUVRegionLoc, u0, v0, du, dv);

        float flippedY = h - (drawY + drawHeight * 0.5f);

        float ndc_x = ((drawX + drawWidth * 0.5f) / float(w)) * 2.f - 1.f;
        float ndc_y = (flippedY / float(h)) * 2.f - 1.f;
        float ndc_w = (drawWidth / float(w)) * 2.f;
        float ndc_h = (drawHeight / float(h)) * 2.f;

        if (pipe.uTransformLoc >= 0)
            glUniform4f(pipe.uTransformLoc, ndc_x, ndc_y, ndc_w, ndc_h);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        const GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            logger::errorf_loc("OpenGL.DrawSprite", std::source_location::current(), "glDrawElements failed: 0x{:X}", static_cast<unsigned int>(err));
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

} // namespace epochengine::opengltextures

#endif // EPOCH_USING_OPENGL
