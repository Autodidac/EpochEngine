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

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <include/engine.config.hpp> // for EPOCH_USING Macros

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <Windows.h>
#   include <wingdi.h>
#endif

export module raylib.textures;

import core.context;
import context.type;
import atlas.manager;
import atlas.texture;
import image.loader;
import texture.core;
import core.logger;
import raylib.api;
import raylib.state;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

namespace epochengine::raylibtextures
{
    using Handle = std::uint32_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    struct AtlasGPU
    {
        epochengine::raylib_api::Texture2D texture{};
        u64 version = static_cast<u64>(-1);
        u64 uploadingVersion = static_cast<u64>(-1);
        u64 failedVersion = static_cast<u64>(-1);
        bool uploading = false;
        std::uint8_t failedAttempts = 0;
        u32 width = 0;
        u32 height = 0;
    };

    constexpr std::uint8_t kMaxUploadAttemptsPerVersion = 3;

    inline std::atomic_uint32_t s_diagnosticUploadRejects{ 0 };
    inline std::atomic_uint32_t s_diagnosticUploadCommits{ 0 };

    struct TextureAtlasPtrHash
    {
        size_t operator()(const TextureAtlas* atlas) const noexcept
        {
            return std::hash<const TextureAtlas*>{}(atlas);
        }
    };

    struct TextureAtlasPtrEqual
    {
        bool operator()(const TextureAtlas* lhs, const TextureAtlas* rhs) const noexcept
        {
            return lhs == rhs;
        }
    };

    struct BackendData
    {
        std::unordered_map<const TextureAtlas*, AtlasGPU, TextureAtlasPtrHash, TextureAtlasPtrEqual> gpu_atlases;
        std::mutex gpuMutex;
    };

    struct UploadedTexture
    {
        epochengine::raylib_api::Texture2D texture{};
        u64 version{ 0 };
        u32 width{ 0 };
        u32 height{ 0 };
    };

    inline BackendData& raylib_backend_storage() noexcept
    {
        static BackendData backend{};
        return backend;
    }

    inline BackendData& get_raylib_backend()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx || ctx->type != epochengine::core::ContextType::RayLib)
            throw std::runtime_error("[RaylibTextures] No current render context");

        return raylib_backend_storage();
    }

    export inline bool backend_storage_is_separate_from_context_native_drawable() noexcept
    {
        try
        {
            auto ctx = epochengine::core::get_current_render_context();
            if (!ctx || ctx->type != epochengine::core::ContextType::RayLib)
                return false;

            auto& backend = get_raylib_backend();
            return static_cast<const void*>(&backend) != ctx->native_drawable;
        }
        catch (...)
        {
            return false;
        }
    }

    export inline void shutdown_current_context_backend() noexcept
    {
        try
        {
            auto& backend = raylib_backend_storage();
            const auto& state = epochengine::raylibstate::s_raylibstate;
            const bool canUnload =
                (state.running || state.renderingActive || state.frameActive)
                && epochengine::raylib_api::is_window_ready();

            // Move textures out under lock, destroy them unlocked.
            std::vector<epochengine::raylib_api::Texture2D> to_free;
            {
                std::scoped_lock lock(backend.gpuMutex);
                if (canUnload)
                {
                    to_free.reserve(backend.gpu_atlases.size());
                    for (auto& [_, gpu] : backend.gpu_atlases)
                    {
                        if (gpu.texture.id != 0)
                            to_free.push_back(gpu.texture);
                    }
                }
                backend.gpu_atlases.clear();
            }

            for (auto& t : to_free)
                epochengine::raylib_api::unload_texture(t);
        }
        catch (...) {}
    }

    [[nodiscard]]
    inline ImageData ensure_rgba(const ImageData& img)
    {
        const std::size_t pixelCount =
            static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height);

        if (pixelCount == 0)
            return img;

        const std::size_t channels = img.pixels.size() / pixelCount;

        if (channels == 4)
            return img;

        if (channels != 3)
            throw std::runtime_error("ensure_rgba(): Unsupported channel count: " + std::to_string(channels));

        std::vector<std::uint8_t> rgba(pixelCount * 4);
        const std::uint8_t* src = img.pixels.data();

        for (std::size_t i = 0; i < pixelCount; ++i)
        {
            rgba[4 * i + 0] = src[3 * i + 0];
            rgba[4 * i + 1] = src[3 * i + 1];
            rgba[4 * i + 2] = src[3 * i + 2];
            rgba[4 * i + 3] = 255;
        }

        return { std::move(rgba), img.width, img.height, 4 };
    }

    // ---- Core rule: no raylib calls while holding gpuMutex ----
    inline UploadedTexture upload_texture_raylib(const TextureAtlas& atlas)
    {
        const AtlasPixelSnapshot snapshot = atlas.snapshot_pixels();
        if (snapshot.width == 0 || snapshot.height == 0 || snapshot.pixels.empty())
            throw std::runtime_error("[RaylibTextures] Atlas '" + atlas.name + "' has no pixel snapshot to upload");

        epochengine::raylib_api::Image img{};
        img.data = const_cast<unsigned char*>(snapshot.pixels.data());
        img.width = snapshot.width;
        img.height = snapshot.height;
        img.mipmaps = 1;
        img.format = epochengine::raylib_api::pixelformat_rgba8;

        return UploadedTexture{
            .texture = epochengine::raylib_api::load_texture_from_image(img),
            .version = snapshot.version,
            .width = snapshot.width,
            .height = snapshot.height
        };
    }

    // Fast check: only attempt upload when the current context is a raylib context.
    // This avoids Ã¢â‚¬Å“helpfullyÃ¢â‚¬Â uploading while some other backend (OpenGL/SDL) is current.
    [[nodiscard]] inline bool is_raylib_backend_current() noexcept
    {
        try
        {
            auto ctx = epochengine::core::get_current_render_context();
            if (!ctx) return false;
            return ctx->type == epochengine::core::ContextType::RayLib;
        }
        catch (...) { return false; }
    }

#if defined(_WIN32)
    [[nodiscard]] inline bool ensure_raylib_context_current() noexcept
    {
        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.hdc || !st.hglrc)
            return false;
        if (::wglGetCurrentDC() == st.hdc && ::wglGetCurrentContext() == st.hglrc)
            return true;
        return ::wglMakeCurrent(st.hdc, st.hglrc) != FALSE;
    }
#else
    [[nodiscard]] inline bool ensure_raylib_context_current() noexcept { return true; }
#endif

    export inline bool ensure_uploaded(const TextureAtlas& atlas) noexcept
    {
        const bool backendCurrent = is_raylib_backend_current();
        const bool nativeContextCurrent = backendCurrent && ensure_raylib_context_current();
        if (!backendCurrent || !nativeContextCurrent)
        {
            if (s_diagnosticUploadRejects.fetch_add(1, std::memory_order_relaxed) < 8u)
            {
                logger::warnf_loc(
                    "Raylib.Diagnostics",
                    std::source_location::current(),
                    "Raylib3 atlas upload rejected: atlas='{}' backendCurrent={} nativeContextCurrent={}",
                    atlas.name,
                    backendCurrent,
                    nativeContextCurrent);
            }
            return false;
        }

        try
        {
            auto& backend = get_raylib_backend();
            const u64 requestedVersion = atlas.current_version();

            const auto recordFailure = [&]() noexcept
            {
                try
                {
                    std::scoped_lock lock(backend.gpuMutex);
                    AtlasGPU& gpu = backend.gpu_atlases[&atlas];
                    gpu.uploading = false;
                    gpu.uploadingVersion = static_cast<u64>(-1);
                    if (gpu.failedVersion != requestedVersion)
                    {
                        gpu.failedVersion = requestedVersion;
                        gpu.failedAttempts = 0;
                    }
                    if (gpu.failedAttempts < kMaxUploadAttemptsPerVersion)
                        ++gpu.failedAttempts;
                }
                catch (...)
                {
                }
            };

            {
                std::scoped_lock lock(backend.gpuMutex);
                AtlasGPU& gpu = backend.gpu_atlases[&atlas];
                if (gpu.version >= requestedVersion && gpu.texture.id != 0)
                    return true;
                if (gpu.uploading && gpu.uploadingVersion >= requestedVersion)
                    return false;
                if (gpu.failedVersion != requestedVersion)
                {
                    gpu.failedVersion = requestedVersion;
                    gpu.failedAttempts = 0;
                }
                if (gpu.failedAttempts >= kMaxUploadAttemptsPerVersion)
                    return false;

                gpu.uploading = true;
                gpu.uploadingVersion = requestedVersion;
            }

            UploadedTexture uploaded{};
            try
            {
                uploaded = upload_texture_raylib(atlas);
            }
            catch (const std::exception& e)
            {
                logger::errorf_loc(
                    "Raylib",
                    std::source_location::current(),
                    "Upload failed for atlas '{}' (version {}): {}",
                    atlas.name,
                    requestedVersion,
                    e.what());
                recordFailure();
                return false;
            }
            catch (...)
            {
                logger::errorf_loc(
                    "Raylib",
                    std::source_location::current(),
                    "Upload failed for atlas '{}' (version {})",
                    atlas.name,
                    requestedVersion);
                recordFailure();
                return false;
            }

            epochengine::raylib_api::Texture2D newTex = uploaded.texture;
            if (newTex.id == 0)
            {
                logger::errorf_loc(
                    "Raylib",
                    std::source_location::current(),
                    "Upload returned no texture for atlas '{}' (version {})",
                    atlas.name,
                    uploaded.version);
                recordFailure();
                return false;
            }

            epochengine::raylib_api::Texture2D oldTex{};
            bool freeOld = false;
            bool committedUpload = false;
            unsigned int committedTextureId = 0;

            {
                std::scoped_lock lock(backend.gpuMutex);
                AtlasGPU& gpu = backend.gpu_atlases[&atlas];
                gpu.uploading = false;
                gpu.uploadingVersion = static_cast<u64>(-1);

                if (gpu.texture.id != 0 && gpu.version >= uploaded.version)
                {
                    oldTex = newTex;
                    freeOld = true;
                }
                else
                {
                    if (gpu.texture.id != 0)
                    {
                        oldTex = gpu.texture;
                        freeOld = true;
                    }

                    gpu.texture = newTex;
                    gpu.version = uploaded.version;
                    gpu.width = uploaded.width;
                    gpu.height = uploaded.height;
                    gpu.failedVersion = static_cast<u64>(-1);
                    gpu.failedAttempts = 0;
                    committedUpload = true;
                    committedTextureId = gpu.texture.id;
                }
            }

            if (freeOld && oldTex.id != 0)
                epochengine::raylib_api::unload_texture(oldTex);

            if (committedUpload
                && s_diagnosticUploadCommits.fetch_add(1, std::memory_order_relaxed) < 8u)
            {
                logger::infof_loc(
                    "Raylib.Diagnostics",
                    std::source_location::current(),
                    "Raylib3 atlas upload committed: atlas='{}' texture={} version={} size={}x{}",
                    atlas.name,
                    committedTextureId,
                    uploaded.version,
                    uploaded.width,
                    uploaded.height);
            }

#if EPOCH_ENABLE_BACKEND_UPLOAD_CONFIRMATION_LOGS && EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS
            if (committedUpload)
            {
                logger::infof_loc(
                    "Raylib",
                    std::source_location::current(),
                    "Uploaded atlas '{}' (tex id {}, version {})",
                    atlas.name,
                    committedTextureId,
                    uploaded.version);
            }
#else
            (void)committedUpload;
            (void)committedTextureId;
#endif

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    export inline bool try_copy_texture(
        const TextureAtlas& atlas,
        epochengine::raylib_api::Texture2D& texture) noexcept
    {
        try
        {
            auto& backend = get_raylib_backend();
            std::scoped_lock lock(backend.gpuMutex);

            auto it = backend.gpu_atlases.find(&atlas);
            if (it == backend.gpu_atlases.end())
                return false;

            if (it->second.texture.id == 0)
                return false;

            texture = it->second.texture;
            return true;
        }
        catch (...)
        {
            texture = {};
            return false;
        }
    }

    export inline void clear_gpu_atlases() noexcept
    {
        try
        {
            auto& backend = raylib_backend_storage();
            const auto& state = epochengine::raylibstate::s_raylibstate;
            const bool canUnload =
                (state.running || state.renderingActive || state.frameActive)
                && epochengine::raylib_api::is_window_ready();

            std::vector<epochengine::raylib_api::Texture2D> to_free;
            {
                std::scoped_lock lock(backend.gpuMutex);
                if (canUnload)
                {
                    to_free.reserve(backend.gpu_atlases.size());

                    for (auto& [_, gpu] : backend.gpu_atlases)
                    {
                        if (gpu.texture.id != 0)
                            to_free.push_back(gpu.texture);
                    }
                }

                backend.gpu_atlases.clear();
            }

            for (auto& t : to_free)
                epochengine::raylib_api::unload_texture(t);
        }
        catch (...) {}
    }

    inline std::atomic_uint8_t s_generation{ 1 };

    [[nodiscard]]
    inline Handle make_handle(int atlasIdx, int localIdx) noexcept
    {
        return (Handle(s_generation.load(std::memory_order_relaxed)) << 24)
            | ((Handle(atlasIdx) & 0xFFFu) << 12)
            | (Handle(localIdx) & 0xFFFu);
    }

    export inline Handle load_atlas(const TextureAtlas& atlas, int atlasIndex = -1)
    {
        if (!ensure_uploaded(atlas))
            return 0;
        const int resolvedIndex = (atlasIndex >= 0) ? atlasIndex : atlas.get_index();
        return make_handle(resolvedIndex, 0);
    }

    export inline Handle atlas_add_texture(TextureAtlas& atlas, const std::string& id, const ImageData& img)
    {
        auto rgba = ensure_rgba(img);

        Texture texture{
            .width = static_cast<std::uint32_t>(rgba.width),
            .height = static_cast<std::uint32_t>(rgba.height),
            .pixels = std::move(rgba.pixels)
        };

        auto addedOpt = atlas.add_entry(id, texture);
        if (!addedOpt)
            throw std::runtime_error("atlas_add_texture: Failed to add texture: " + id);

        if (!ensure_uploaded(atlas))
            return 0;
        return make_handle(atlas.get_index(), addedOpt->index);
    }
}

#endif // EPOCH_USING_RAYLIB
