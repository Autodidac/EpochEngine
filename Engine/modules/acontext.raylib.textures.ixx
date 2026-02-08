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
 **************************************************************/
module;

#include <include/aengine.config.hpp> // for ALMOND_USING Macros

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

export module acontext.raylib.textures;

import <atomic>;
import <cstdint>;
import <filesystem>;
import <format>;
import <fstream>;
import <iostream>;
import <mutex>;
import <string>;
import <string_view>;
import <unordered_map>;
import <utility>;
import <vector>;

import aengine.core.context;
import aatlas.manager;
import aatlas.texture;
import aimage.loader;
import atexture;
import acontext.raylib.api;
import acontext.raylib.state;

#if defined(ALMOND_USING_RAYLIB)

export namespace almondnamespace::raylibtextures
{
    using Handle = std::uint32_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    struct AtlasGPU
    {
        almondnamespace::raylib_api::Texture2D texture{};
        u64 version = static_cast<u64>(-1);
        u64 uploadingVersion = static_cast<u64>(-1);
        bool uploading = false;
        u32 width = 0;
        u32 height = 0;
    };

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

    inline BackendData& get_raylib_backend()
    {
        auto ctx = almondnamespace::core::get_current_render_context();
        if (!ctx)
            throw std::runtime_error("[RaylibTextures] No current render context");

        if (!ctx->native_drawable)
            ctx->native_drawable = new BackendData();

        return *static_cast<BackendData*>(ctx->native_drawable);
    }

    export inline void shutdown_current_context_backend() noexcept
    {
        try
        {
            auto& backend = get_raylib_backend();

            // Move textures out under lock, destroy them unlocked.
            std::vector<almondnamespace::raylib_api::Texture2D> to_free;
            {
                std::scoped_lock lock(backend.gpuMutex);
                to_free.reserve(backend.gpu_atlases.size());
                for (auto& [_, gpu] : backend.gpu_atlases)
                {
                    if (gpu.texture.id != 0)
                        to_free.push_back(gpu.texture);
                }
                backend.gpu_atlases.clear();
            }

            for (auto& t : to_free)
                almondnamespace::raylib_api::unload_texture(t);

            if (auto ctx = almondnamespace::core::get_current_render_context(); ctx && ctx->native_drawable)
            {
                delete static_cast<BackendData*>(ctx->native_drawable);
                ctx->native_drawable = nullptr;
            }
        }
        catch (...) {}
    }

    inline std::atomic_uint32_t s_dumpSerial{ 0 };

    inline std::string make_dump_name(int atlasIdx, std::string_view tag)
    {
        std::filesystem::create_directories("atlas_dump");
        return std::format("atlas_dump/{}_{}_{}.ppm",
            tag,
            atlasIdx,
            s_dumpSerial.fetch_add(1, std::memory_order_relaxed));
    }

    inline void dump_atlas_rgb_ppm(const TextureAtlas& atlas, int atlasIdx)
    {
        const std::string filename = make_dump_name(atlasIdx, atlas.name);

        std::ofstream out(filename, std::ios::binary);
        if (!out)
        {
            std::cerr << "[Dump] Failed to open: " << filename << "\n";
            return;
        }

        out << "P6\n" << atlas.width << " " << atlas.height << "\n255\n";

        const auto& px = atlas.pixel_data; // RGBA
        for (std::size_t i = 0; i + 2 < px.size(); i += 4)
        {
            out.put(static_cast<char>(px[i + 0]));
            out.put(static_cast<char>(px[i + 1]));
            out.put(static_cast<char>(px[i + 2]));
        }

        std::cerr << "[Dump] Wrote: " << filename << "\n";
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
    inline almondnamespace::raylib_api::Texture2D upload_texture_raylib(const TextureAtlas& atlas)
    {
        // Ensure pixels exist (may rebuild CPU-side).
        if (atlas.pixel_data.empty())
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();

        almondnamespace::raylib_api::Image img{};
        img.data = const_cast<unsigned char*>(atlas.pixel_data.data());
        img.width = atlas.width;
        img.height = atlas.height;
        img.mipmaps = 1;
        img.format = almondnamespace::raylib_api::pixelformat_rgba8;

        return almondnamespace::raylib_api::load_texture_from_image(img);
    }

    // Fast check: only attempt upload when the current context is a raylib context.
    // This avoids “helpfully” uploading while some other backend (OpenGL/SDL) is current.
    [[nodiscard]] inline bool is_raylib_backend_current() noexcept
    {
        try
        {
            auto ctx = almondnamespace::core::get_current_render_context();
            if (!ctx) return false;
            return ctx->type == almondnamespace::core::ContextType::RayLib;
        }
        catch (...) { return false; }
    }

#if defined(_WIN32)
    inline void ensure_raylib_context_current() noexcept
    {
        auto& st = almondnamespace::raylibstate::s_raylibstate;
        if (st.hdc && st.hglrc)
            (void)::wglMakeCurrent(st.hdc, st.hglrc);
    }
#else
    inline void ensure_raylib_context_current() noexcept {}
#endif

    export inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        // If you call this while docking/multiplexer has a different backend current,
        // do NOT upload. Let the next raylib-active frame handle it.
        if (!is_raylib_backend_current())
            return;

        ensure_raylib_context_current();

        auto& backend = get_raylib_backend();

        // 1) Cheap read under lock: do we already have the right version?
        {
            std::scoped_lock lock(backend.gpuMutex);
            AtlasGPU& gpu = backend.gpu_atlases[&atlas];
            if (gpu.version == atlas.version && gpu.texture.id != 0)
                return;
            if (gpu.uploading && gpu.uploadingVersion == atlas.version)
                return;
            gpu.uploading = true;
            gpu.uploadingVersion = atlas.version;
        }

        // 2) Upload unlocked (raylib/GL work).
        almondnamespace::raylib_api::Texture2D newTex = upload_texture_raylib(atlas);

        if (newTex.id == 0)
        {
            // Raylib already printed warnings; add one line of ours.
            std::cerr << "[Raylib] Upload failed for atlas '" << atlas.name
                << "' (version " << atlas.version << ")\n";
            {
                std::scoped_lock lock(backend.gpuMutex);
                auto it = backend.gpu_atlases.find(&atlas);
                if (it != backend.gpu_atlases.end())
                {
                    it->second.uploading = false;
                    it->second.uploadingVersion = static_cast<u64>(-1);
                }
            }
            return;
        }

        // Optional: dump only when a *new* texture is successfully created.
        dump_atlas_rgb_ppm(atlas, atlas.index);

        // 3) Commit under lock; if someone else updated in the meantime, keep the newest.
        almondnamespace::raylib_api::Texture2D oldTex{};
        bool freeOld = false;

        {
            std::scoped_lock lock(backend.gpuMutex);
            AtlasGPU& gpu = backend.gpu_atlases[&atlas];
            gpu.uploading = false;
            gpu.uploadingVersion = static_cast<u64>(-1);

            // If another thread beat us to it with same/newer version, drop ours.
            if (gpu.texture.id != 0 && gpu.version >= atlas.version)
            {
                // Keep existing; delete our newly created texture.
                oldTex = newTex;
                freeOld = true;
            }
            else
            {
                // Replace existing (if any) and commit.
                if (gpu.texture.id != 0)
                {
                    oldTex = gpu.texture;
                    freeOld = true;
                }

                gpu.texture = newTex;
                gpu.version = atlas.version;
                gpu.width = static_cast<u32>(atlas.width);
                gpu.height = static_cast<u32>(atlas.height);

                std::cerr << "[Raylib] Uploaded atlas '" << atlas.name
                    << "' (tex id " << gpu.texture.id
                    << ", version " << gpu.version << ")\n";
            }
        }

        // 4) Free old texture(s) unlocked.
        if (freeOld && oldTex.id != 0)
            almondnamespace::raylib_api::unload_texture(oldTex);
    }

    export inline const almondnamespace::raylib_api::Texture2D* try_get_texture(const TextureAtlas& atlas) noexcept
    {
        try
        {
            auto& backend = get_raylib_backend();
            std::scoped_lock lock(backend.gpuMutex);

            auto it = backend.gpu_atlases.find(&atlas);
            if (it == backend.gpu_atlases.end())
                return nullptr;

            if (it->second.texture.id == 0)
                return nullptr;

            return &it->second.texture;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    export inline void clear_gpu_atlases() noexcept
    {
        try
        {
            auto& backend = get_raylib_backend();

            std::vector<almondnamespace::raylib_api::Texture2D> to_free;
            {
                std::scoped_lock lock(backend.gpuMutex);
                to_free.reserve(backend.gpu_atlases.size());

                for (auto& [_, gpu] : backend.gpu_atlases)
                {
                    if (gpu.texture.id != 0)
                        to_free.push_back(gpu.texture);
                }

                backend.gpu_atlases.clear();
            }

            for (auto& t : to_free)
                almondnamespace::raylib_api::unload_texture(t);
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
        atlasmanager::ensure_uploaded(atlas);
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

        atlasmanager::ensure_uploaded(atlas);
        return make_handle(atlas.get_index(), addedOpt->index);
    }
}

#endif // ALMOND_USING_RAYLIB
