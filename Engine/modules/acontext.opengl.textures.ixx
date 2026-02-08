/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
module;

// -----------------------------------------------------------------------------
// Global module fragment: macros + platform / C headers live here.
// -----------------------------------------------------------------------------

#include "../include/aengine.config.hpp"

#if defined(ALMOND_USING_OPENGL)

// Make sure GL loaders see any platform defines they need.
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

// Prefer GLAD (what you’re already using elsewhere). This provides GLuint,
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

#endif // ALMOND_USING_OPENGL

export module acontext.opengl.textures;

import <algorithm>;
import <atomic>;
import <cstdint>;
import <filesystem>;
import <format>;
import <fstream>;
import <iostream>;
import <mutex>;
import <span>;
import <string>;
import <unordered_map>;
import <vector>;

import aengine.platform;

#ifdef ALMOND_USING_OPENGL

import aengine.cli;
import aengine.core.context;
import aengine.context.multiplexer;

import acontext.opengl.platform;
import acontext.opengl.state;
import acontext.opengl.quad;
import aatlas.manager;
import aatlas.texture;
import atexture;
import aimage.loader;
import aspritehandle;

// If u32/u64 are yours and not from <cstdint>, you must import the module that
// defines them. Uncomment the correct one in your project.
// import atypes;

export namespace almondnamespace::opengltextures
{
    namespace detail
    {
        inline almondnamespace::openglcontext::PlatformGL::PlatformGLContext
            to_platform_context(const almondnamespace::openglstate::OpenGL4State& state) noexcept
        {
            almondnamespace::openglcontext::PlatformGL::PlatformGLContext ctx{};
#if defined(_WIN32)
            ctx.device = state.hdc;
            ctx.context = state.hglrc;
#elif defined(__linux__)
            ctx.display = state.display;
            ctx.drawable = state.drawable ? state.drawable : state.window;
            ctx.context = state.glxContext;
#endif
            return ctx;
        }

        inline almondnamespace::openglcontext::PlatformGL::PlatformGLContext
            context_to_platform_context(const core::Context* ctx) noexcept
        {
            almondnamespace::openglcontext::PlatformGL::PlatformGLContext result{};
            if (!ctx) return result;

#if defined(_WIN32)
            result.device = static_cast<HDC>(ctx->native_drawable);
            result.context = static_cast<HGLRC>(ctx->native_gl_context);
#elif defined(__linux__)
            result.display = static_cast<Display*>(ctx->native_drawable);
            result.drawable = static_cast<GLXDrawable>(reinterpret_cast<std::uintptr_t>(ctx->native_window));
            result.context = static_cast<GLXContext>(ctx->native_gl_context);
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

    inline std::unordered_map<const TextureAtlas*, AtlasGPU, TextureAtlasPtrHash, TextureAtlasPtrEqual> opengl_gpu_atlases;

    struct BackendData {
        std::unordered_map<const TextureAtlas*, AtlasGPU,
            TextureAtlasPtrHash, TextureAtlasPtrEqual> gpu_atlases;
        std::mutex gpuMutex;
        almondnamespace::openglstate::OpenGL4State glState{};
    };

    inline BackendData& get_opengl_backend() {
        BackendData* data = nullptr;
        {
            std::unique_lock lock(almondnamespace::core::g_backendsMutex);
            auto& backend = almondnamespace::core::g_backends[almondnamespace::core::ContextType::OpenGL];
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

    using Handle = uint32_t;

    inline std::atomic_uint8_t  s_generation{ 1 };
    inline std::atomic_uint32_t s_dumpSerial{ 0 };

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

    inline std::string make_dump_name(int atlasIdx, std::string_view tag) {
        std::filesystem::create_directories("atlas_dump");
        return std::format("atlas_dump/{}_{}_{}.ppm", tag, atlasIdx,
            s_dumpSerial.fetch_add(1, std::memory_order_relaxed));
    }

    inline void dump_atlas(const TextureAtlas& atlas, int atlasIdx) {
        std::string filename = make_dump_name(atlasIdx, atlas.name);
        std::ofstream out(filename, std::ios::binary);
        out << "P6\n" << atlas.width << " " << atlas.height << "\n255\n";
        for (size_t i = 0; i < atlas.pixel_data.size(); i += 4) {
            out.put(atlas.pixel_data[i]);
            out.put(atlas.pixel_data[i + 1]);
            out.put(atlas.pixel_data[i + 2]);
        }
        std::cerr << "[Dump] Wrote: " << filename << "\n";
    }

    inline void upload_atlas_to_gpu(const TextureAtlas& atlas)
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock lock(core::g_backendsMutex);
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }
        if (!oglData) {
            std::cerr << "[UploadAtlas] OpenGL backendData not initialized!\n";
            return;
        }
        auto& glState = oglData->glState;

        if (atlas.pixel_data.empty()) {
            std::cerr << "[UploadAtlas] Pixel data empty for '" << atlas.name
                << "', rebuilding...\n";
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();
        }

        const auto platformCtx = detail::to_platform_context(glState);
        almondnamespace::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!contextGuard.set(platformCtx)) {
            std::cerr << "[UploadAtlas] Failed to activate GL context for upload\n";
            return;
        }

        std::lock_guard<std::mutex> gpuLock(oglData->gpuMutex);
        auto& gpu = oglData->gpu_atlases[&atlas];

        if (!gpu.textureHandle) {
            glGenTextures(1, &gpu.textureHandle);
            if (!gpu.textureHandle) {
                std::cerr << "[OpenGL] Failed to generate texture for atlas: "
                    << atlas.name << "\n";
                return;
            }
        }

        if (gpu.version == atlas.version) {
            std::cerr << "[UploadAtlas] SKIPPING upload for '" << atlas.name
                << "' version = " << atlas.version << "\n";
            return;
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

        glBindTexture(GL_TEXTURE_2D, 0);

        std::cerr << "[OpenGL] Uploaded atlas '" << atlas.name
            << "' (tex id " << gpu.textureHandle << ")\n";
    }

    inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock lock(core::g_backendsMutex);
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }
        if (!oglData) {
            std::cerr << "[EnsureUploaded] OpenGL backendData not initialized!\n";
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

    inline bool ensure_created_pipeline(almondnamespace::openglstate::OpenGL4State& glState)
    {
        return almondnamespace::openglquad::ensure_quad_pipeline(glState);
    }

    inline void clear_gpu_atlases() noexcept
    {
        BackendData* oglData = nullptr;
        {
            std::shared_lock lock(core::g_backendsMutex);
            auto it = core::g_backends.find(core::ContextType::OpenGL);
            if (it != core::g_backends.end()) {
                oglData = static_cast<BackendData*>(it->second.data.get());
            }
        }

        if (oglData) {
            std::lock_guard<std::mutex> gpuLock(oglData->gpuMutex);
            for (auto& [_, gpu] : oglData->gpu_atlases) {
                if (gpu.textureHandle) {
                    glDeleteTextures(1, &gpu.textureHandle);
                }
            }
            oglData->gpu_atlases.clear();
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
        if (!handle.is_valid()) {
            std::cerr << "[DrawSprite] Invalid sprite handle.\n";
            return;
        }

        auto& backend = get_opengl_backend();
        almondnamespace::openglcontext::PlatformGL::ScopedContext contextGuard;
        auto desired = detail::context_to_platform_context(core::MultiContextManager::GetCurrent().get());
        if (!desired.valid()) {
            desired = detail::to_platform_context(backend.glState);
        }
        if (!desired.valid() || !contextGuard.set(desired)) {
            std::cerr << "[DrawSprite] WARNING: Unable to activate OpenGL context; skipping draw.\n";
            return;
        }

        if (!ensure_created_pipeline(backend.glState)) {
            std::cerr << "[DrawSprite] Missing quad pipeline; skipping draw\n";
            return;
        }

        GLint viewport[4] = { 0, 0, 0, 0 };
        glGetIntegerv(GL_VIEWPORT, viewport);
        int w = viewport[2];
        int h = viewport[3];

        if (w <= 0 || h <= 0) {
            w = static_cast<int>(backend.glState.width);
            h = static_cast<int>(backend.glState.height);
        }
        if (w <= 0 || h <= 0) {
            if (auto ctx = core::MultiContextManager::GetCurrent()) {
                w = (std::max)(1, ctx->get_width_safe());
                h = (std::max)(1, ctx->get_height_safe());
            }
        }
        if (w <= 0 || h <= 0) {
            w = (std::max)(1, core::cli::window_width);
            h = (std::max)(1, core::cli::window_height);
        }
        if (w <= 0 || h <= 0) {
            std::cerr << "[DrawSprite] ERROR: Unable to resolve window dimensions.\n";
            return;
        }

        backend.glState.width = static_cast<unsigned int>(w);
        backend.glState.height = static_cast<unsigned int>(h);

        const int atlasIdx = int(handle.atlasIndex);
        const int localIdx = int(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= int(atlases.size())) {
            std::cerr << "[DrawSprite] Atlas index out of bounds: " << atlasIdx << '\n';
            return;
        }
        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas) {
            std::cerr << "[DrawSprite] Null atlas pointer at index: " << atlasIdx << '\n';
            return;
        }

        AtlasRegion region{};
        std::string spriteName;
        if (!atlas->try_get_entry_info(localIdx, region, &spriteName)) {
            std::cerr << "[DrawSprite] Sprite index out of bounds: " << localIdx << '\n';
            return;
        }

        ensure_uploaded(*atlas);

        GLuint tex = 0;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto it = backend.gpu_atlases.find(atlas);
            if (it == backend.gpu_atlases.end()) {
                std::cerr << "[DrawSprite] GPU texture not found for atlas '"
                    << atlas->name << "'\n";
                return;
            }
            tex = it->second.textureHandle;
        }
        if (!tex) {
            std::cerr << "[DrawSprite] GPU texture not found for atlas '"
                << atlas->name << "'\n";
            return;
        }

        auto& pipe = almondnamespace::openglquad::quad_pipeline_state();
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

        const bool widthNormalized = width > 0.f && width <= 1.f;
        const bool heightNormalized = height > 0.f && height <= 1.f;

        float drawWidth = widthNormalized ? (std::max)(width * float(w), 1.0f) : width;
        float drawHeight = heightNormalized ? (std::max)(height * float(h), 1.0f) : height;

        float drawX = (widthNormalized && x >= 0.f && x <= 1.f) ? x * float(w) : x;
        float drawY = (heightNormalized && y >= 0.f && y <= 1.f) ? y * float(h) : y;

        const float u0 = region.u1;
        const float du = region.u2 - region.u1;
        const float v0 = region.v2;
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
            std::cerr << "[OpenGL ERROR] glDrawElements failed: " << std::hex << err << "\n";
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

} // namespace almondnamespace::opengltextures

#endif // ALMOND_USING_OPENGL