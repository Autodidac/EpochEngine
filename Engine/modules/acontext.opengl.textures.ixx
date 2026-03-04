/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   epochengine - Modular C++ Framework                      *
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
import <memory>;
import <span>;
import <string>;
import <unordered_map>;
import <vector>;

import aengine.platform;

#ifdef ALMOND_USING_OPENGL

import aengine.cli;
import aengine.core.context;
import aengine.context.commandqueue;
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

export namespace epochnamespace::opengltextures
{
    namespace detail
    {
        inline epochnamespace::openglcontext::PlatformGL::PlatformGLContext
            to_platform_context(const epochnamespace::openglstate::OpenGL4State& state) noexcept
        {
            epochnamespace::openglcontext::PlatformGL::PlatformGLContext ctx{};
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

        inline epochnamespace::openglcontext::PlatformGL::PlatformGLContext
            context_to_platform_context(const core::Context* ctx) noexcept
        {
            epochnamespace::openglcontext::PlatformGL::PlatformGLContext result{};
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

    struct ContextPtrHash {
        size_t operator()(const core::Context* ctx) const noexcept {
            return std::hash<const core::Context*>{}(ctx);
        }
    };

    struct ContextPtrEqual {
        bool operator()(const core::Context* lhs, const core::Context* rhs) const noexcept {
            return lhs == rhs;
        }
    };

    using AtlasGPUMap = std::unordered_map<const TextureAtlas*, AtlasGPU,
        TextureAtlasPtrHash, TextureAtlasPtrEqual>;

    struct BackendData {
        std::unordered_map<const core::Context*, AtlasGPUMap,
            ContextPtrHash, ContextPtrEqual> gpu_atlases;
        std::unordered_map<const core::Context*, epochnamespace::openglstate::OpenGL4State> contextStates;
        std::mutex gpuMutex;
        std::mutex stateMutex;
    };

    inline BackendData& get_opengl_backend() {
        BackendData* data = nullptr;
        {
            std::unique_lock lock(epochnamespace::core::g_backendsMutex);
            auto& backend = epochnamespace::core::g_backends[epochnamespace::core::ContextType::OpenGL];
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

    inline epochnamespace::openglstate::OpenGL4State* find_state_for_context(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return nullptr;

        auto& backend = get_opengl_backend();
        std::lock_guard<std::mutex> stateLock(backend.stateMutex);
        auto it = backend.contextStates.find(ctx);
        if (it == backend.contextStates.end())
            return nullptr;
        return &it->second;
    }

    inline epochnamespace::openglstate::OpenGL4State* ensure_state_for_context(const core::Context* ctx)
    {
        if (!ctx)
            return nullptr;

        auto& backend = get_opengl_backend();
        std::lock_guard<std::mutex> stateLock(backend.stateMutex);
        return &backend.contextStates[ctx];
    }

    inline bool remove_state_for_context(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return false;

        auto& backend = get_opengl_backend();
        std::lock_guard<std::mutex> stateLock(backend.stateMutex);
        return backend.contextStates.erase(ctx) > 0;
    }

    inline void purge_stale_context_entries(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return;

        auto& backend = get_opengl_backend();
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            backend.gpu_atlases.erase(ctx);
        }
        {
            std::lock_guard<std::mutex> stateLock(backend.stateMutex);
            backend.contextStates.erase(ctx);
        }
    }

    inline bool is_context_alive_for_draw(const core::Context* ctx) noexcept
    {
        if (!ctx || !ctx->windowData || !ctx->windowData->running)
            return false;

#if defined(_WIN32)
        if (!ctx->windowData->hwnd || !ctx->native_drawable || !ctx->native_gl_context)
            return false;
#elif defined(__linux__)
        if (!ctx->native_window || !ctx->native_drawable || !ctx->native_gl_context)
            return false;
#endif

        return true;
    }

    inline void log_draw_sprite_warning_rate_limited(const std::string& message) noexcept
    {
        static std::atomic_uint32_t s_warningTick{ 0 };
        const std::uint32_t tick = s_warningTick.fetch_add(1, std::memory_order_relaxed);
        if ((tick % 60u) == 0u)
            std::cerr << message << '\n';
    }

    inline void schedule_draw_sprite_retry(
        const core::Context* owner,
        SpriteHandle handle,
        std::vector<const TextureAtlas*> atlases,
        float x,
        float y,
        float width,
        float height,
        std::uint8_t retryCount) noexcept;

    inline const core::Context* resolve_context_for_platform(
        const epochnamespace::openglcontext::PlatformGL::PlatformGLContext& platformCtx) noexcept
    {
        if (!platformCtx.valid())
            return nullptr;

        auto& backend = get_opengl_backend();
        std::lock_guard<std::mutex> stateLock(backend.stateMutex);
        for (const auto& [ctx, state] : backend.contextStates) {
            const auto statePlatformCtx = detail::to_platform_context(state);
            if (statePlatformCtx == platformCtx)
                return ctx;
        }
        return nullptr;
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
        auto& backend = get_opengl_backend();
        auto* activeCtx = core::MultiContextManager::GetCurrent().get();
        if (!activeCtx) {
            std::cerr << "[UploadAtlas] No active context for upload\n";
            return;
        }
        auto* glState = find_state_for_context(activeCtx);
        if (!glState) {
            std::cerr << "[UploadAtlas] No OpenGL context state registered for upload\n";
            return;
        }

        if (atlas.pixel_data.empty()) {
            std::cerr << "[UploadAtlas] Pixel data empty for '" << atlas.name
                << "', rebuilding...\n";
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();
        }

        const auto platformCtx = detail::to_platform_context(*glState);
        epochnamespace::openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!contextGuard.set(platformCtx)) {
            std::cerr << "[UploadAtlas] Failed to activate GL context for upload\n";
            return;
        }

        AtlasGPU snapshot{};
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto& gpuByContext = backend.gpu_atlases[activeCtx];
            snapshot = gpuByContext[&atlas];
        }

        if (snapshot.version == atlas.version && snapshot.textureHandle != 0) {
            std::cerr << "[UploadAtlas] SKIPPING upload for '" << atlas.name
                << "' version = " << atlas.version << "\n";
            return;
        }

        if (!snapshot.textureHandle) {
            glGenTextures(1, &snapshot.textureHandle);
            if (!snapshot.textureHandle) {
                std::cerr << "[OpenGL] Failed to generate texture for atlas: "
                    << atlas.name << "\n";
                return;
            }
        }

        glBindTexture(GL_TEXTURE_2D, snapshot.textureHandle);

        if (snapshot.width != atlas.width || snapshot.height != atlas.height) {
#ifdef GL_ARB_texture_storage
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, atlas.width, atlas.height);
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                atlas.width, atlas.height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#endif
            snapshot.width = atlas.width;
            snapshot.height = atlas.height;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            atlas.width, atlas.height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            atlas.pixel_data.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        snapshot.version = atlas.version;

        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto& gpu = backend.gpu_atlases[activeCtx][&atlas];
            gpu = snapshot;
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        std::cerr << "[OpenGL] Uploaded atlas '" << atlas.name
            << "' (tex id " << snapshot.textureHandle << ")\n";
    }

    inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        auto& backend = get_opengl_backend();
        auto* activeCtx = core::MultiContextManager::GetCurrent().get();
        if (!activeCtx) {
            std::cerr << "[EnsureUploaded] No active context for atlas upload\n";
            return;
        }

        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto ctxIt = backend.gpu_atlases.find(activeCtx);
            if (ctxIt != backend.gpu_atlases.end()) {
                auto atlasIt = ctxIt->second.find(&atlas);
                if (atlasIt != ctxIt->second.end()) {
                    if (atlasIt->second.version == atlas.version && atlasIt->second.textureHandle != 0)
                        return;
                }
            }
        }
        upload_atlas_to_gpu(atlas);
    }

    inline bool ensure_created_pipeline(epochnamespace::openglstate::OpenGL4State& glState)
    {
        return epochnamespace::openglquad::ensure_quad_pipeline(glState);
    }

    inline void clear_gpu_atlases() noexcept
    {
        auto* activeCtx = core::MultiContextManager::GetCurrent().get();
        if (!activeCtx)
            return;

        auto& backend = get_opengl_backend();
        std::vector<GLuint> handles;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto ctxIt = backend.gpu_atlases.find(activeCtx);
            if (ctxIt == backend.gpu_atlases.end())
                return;
            handles.reserve(ctxIt->second.size());
            for (const auto& [_, gpu] : ctxIt->second) {
                if (gpu.textureHandle)
                    handles.push_back(gpu.textureHandle);
            }
            backend.gpu_atlases.erase(ctxIt);
        }

        for (GLuint handle : handles) {
            glDeleteTextures(1, &handle);
        }

        s_generation.fetch_add(1, std::memory_order_relaxed);
    }

    inline void clear_gpu_atlases_for_context(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return;

        auto& backend = get_opengl_backend();
        std::vector<GLuint> handles;
        {
            std::lock_guard<std::mutex> gpuLock(backend.gpuMutex);
            auto ctxIt = backend.gpu_atlases.find(ctx);
            if (ctxIt == backend.gpu_atlases.end())
                return;
            handles.reserve(ctxIt->second.size());
            for (const auto& [_, gpu] : ctxIt->second) {
                if (gpu.textureHandle)
                    handles.push_back(gpu.textureHandle);
            }
            backend.gpu_atlases.erase(ctxIt);
        }

        for (GLuint handle : handles) {
            glDeleteTextures(1, &handle);
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

    inline void draw_sprite_impl(SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height,
        std::uint8_t retryCount) noexcept
    {
        if (!handle.is_valid()) {
            std::cerr << "[DrawSprite] Invalid sprite handle.\n";
            return;
        }

        auto& backend = get_opengl_backend();
        auto* requestedCtx = core::MultiContextManager::GetCurrent().get();

        if (!is_context_alive_for_draw(requestedCtx)) {
            purge_stale_context_entries(requestedCtx);
            log_draw_sprite_warning_rate_limited("[DrawSprite] Skipping draw: context/window is no longer alive; stale OpenGL entries purged.");
            return;
        }

        auto* requestedState = find_state_for_context(requestedCtx);
        epochnamespace::openglcontext::PlatformGL::ScopedContext contextGuard;
        auto desired = detail::context_to_platform_context(core::MultiContextManager::GetCurrent().get());
        if (!desired.valid()) {
            if (requestedState)
                desired = detail::to_platform_context(*requestedState);
        }
        if (!desired.valid() || !contextGuard.set(desired)) {
            std::cerr << "[DrawSprite] WARNING: Unable to activate OpenGL context; skipping draw.\n";
            return;
        }

        const core::Context* effectiveCtx = resolve_context_for_platform(desired);
        if (!effectiveCtx) {
            std::cerr << "[DrawSprite] ERROR: Active OpenGL platform context could not be mapped to a backend context; skipping draw.\n";
            return;
        }

        auto* currentState = find_state_for_context(effectiveCtx);

        if (!is_context_alive_for_draw(effectiveCtx)) {
            purge_stale_context_entries(effectiveCtx);
            log_draw_sprite_warning_rate_limited("[DrawSprite] Skipping draw: resolved context/window is no longer alive; stale OpenGL entries purged.");
            return;
        }

        if (!currentState || !ensure_created_pipeline(*currentState)) {
            schedule_draw_sprite_retry(effectiveCtx, handle, { atlases.begin(), atlases.end() }, x, y, width, height, retryCount);
            return;
        }

        GLint viewport[4] = { 0, 0, 0, 0 };
        glGetIntegerv(GL_VIEWPORT, viewport);
        int w = viewport[2];
        int h = viewport[3];

        if (w <= 0 || h <= 0) {
            if (currentState) {
                w = static_cast<int>(currentState->width);
                h = static_cast<int>(currentState->height);
            }
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

        if (currentState) {
            currentState->width = static_cast<unsigned int>(w);
            currentState->height = static_cast<unsigned int>(h);
        }

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
            auto ctxIt = backend.gpu_atlases.find(effectiveCtx);
            if (ctxIt == backend.gpu_atlases.end()) {
                std::cerr << "[DrawSprite] GPU texture not found for active context and atlas '"
                    << atlas->name << "'\n";
                return;
            }
            auto it = ctxIt->second.find(atlas);
            if (it == ctxIt->second.end()) {
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

        auto& pipe = epochnamespace::openglquad::quad_pipeline_state();
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

    inline void schedule_draw_sprite_retry(
        const core::Context* owner,
        SpriteHandle handle,
        std::vector<const TextureAtlas*> atlases,
        float x,
        float y,
        float width,
        float height,
        std::uint8_t retryCount) noexcept
    {
        constexpr std::uint8_t kMaxRetries = 3;
        if (!owner || !owner->windowData)
            return;

        if (retryCount >= kMaxRetries) {
            log_draw_sprite_warning_rate_limited("[DrawSprite] Dropping draw after retry cap due to missing OpenGL state/pipeline.");
            return;
        }

        std::weak_ptr<core::Context> weakOwner = owner->windowData->context;
        owner->windowData->commandQueue.enqueue(
            [weakOwner,
            handle,
            atlases = std::move(atlases),
            x,
            y,
            width,
            height,
            retryCount]() mutable
            {
                auto locked = weakOwner.lock();
                if (!locked)
                    return;

                core::MultiContextManager::SetCurrent(locked);
                draw_sprite_impl(
                    handle,
                    std::span<const TextureAtlas* const>(atlases.data(), atlases.size()),
                    x,
                    y,
                    width,
                    height,
                    static_cast<std::uint8_t>(retryCount + 1));
            },
            core::RenderPath::OpenGL);

        log_draw_sprite_warning_rate_limited("[DrawSprite] OpenGL state/pipeline unavailable; queued one-shot retry.");
    }

    inline void draw_sprite(SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        draw_sprite_impl(handle, atlases, x, y, width, height, 0);
    }

} // namespace epochnamespace::opengltextures

#endif // ALMOND_USING_OPENGL
