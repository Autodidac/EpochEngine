/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

export module aspriteregistry;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Standard library
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Engine modules
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

import aspritehandle;
import asprite.pool;
import aatlas.texture;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

namespace epochnamespace
{
    // Forward declaration only â€” definition lives in atlas module

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Transparent lookup helpers
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    struct TransparentHash
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view sv) const noexcept
        {
            return std::hash<std::string_view>{}(sv);
        }

        std::size_t operator()(const std::string& s) const noexcept
        {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct TransparentEqual
    {
        using is_transparent = void;

        bool operator()(std::string_view a, std::string_view b) const noexcept
        {
            return a == b;
        }

        bool operator()(const std::string& a, std::string_view b) const noexcept
        {
            return a == b;
        }

        bool operator()(std::string_view a, const std::string& b) const noexcept
        {
            return a == b;
        }

        bool operator()(const std::string& a, const std::string& b) const noexcept
        {
            return a == b;
        }
    };

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // SpriteRegistry
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export struct SpriteRegistry
    {
        using Entry =
            std::tuple<SpriteHandle, float, float, float, float, float, float>;
        //        handle        u0     v0     u1     v1     pivotX pivotY

        std::unordered_map<
            std::string,
            Entry,
            TransparentHash,
            TransparentEqual
        > sprites;

        mutable std::shared_mutex mutex{};
        std::atomic<const TextureAtlas*> atlas_ptr{ nullptr };

        // ----------------------------------------------------
        // Add
        // ----------------------------------------------------

        void add(
            std::string_view name,
            SpriteHandle handle,
            float u0,
            float v0,
            float width,
            float height,
            float pivotX = 0.f,
            float pivotY = 0.f)
        {
            if (!handle.is_valid() || !spritepool::is_alive(handle))
            {
                std::cerr
                    << "[SpriteRegistry] Rejecting invalid handle for '"
                    << name << "'\n";
                return;
            }

            std::unique_lock lock(mutex);

            // Prevent handle aliasing
            for (const auto& [existingName, entry] : sprites)
            {
                if (std::get<0>(entry) == handle &&
                    existingName != name)
                {
                    std::cerr
                        << "[SpriteRegistry] Duplicate handle for '"
                        << name << "'\n";
                    return;
                }
            }

            const float u1 = u0 + width;
            const float v1 = v0 + height;

            sprites.emplace(
                std::string{ name },
                Entry{ handle, u0, v0, u1, v1, pivotX, pivotY });

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
            std::cout
                << "[SpriteRegistry] Added '" << name
                << "' handle=" << handle.id
                << " UV=(" << u0 << "," << v0
                << ")->(" << u1 << "," << v1 << ")\n";
#endif
        }

        // ----------------------------------------------------
        // Lookup
        // ----------------------------------------------------

        [[nodiscard]]
        std::optional<Entry>
            get(std::string_view name) const noexcept
        {
            std::shared_lock lock(mutex);
            auto it = sprites.find(name);
            return (it != sprites.end())
                ? std::optional<Entry>{ it->second }
            : std::nullopt;
        }

        // ----------------------------------------------------
        // Removal
        // ----------------------------------------------------

        bool remove(std::string_view name)
        {
            std::unique_lock lock(mutex);
            return sprites.erase(std::string{ name }) > 0;
        }

        bool remove_if_invalid(std::string_view name)
        {
            std::unique_lock lock(mutex);

            auto it = sprites.find(name);
            if (it == sprites.end())
                return false;

            if (!spritepool::is_alive(std::get<0>(it->second)))
            {
                sprites.erase(it);
                return true;
            }

            return false;
        }

        void cleanup_dead()
        {
            std::unique_lock lock(mutex);

            for (auto it = sprites.begin(); it != sprites.end();)
            {
                if (!spritepool::is_alive(std::get<0>(it->second)))
                    it = sprites.erase(it);
                else
                    ++it;
            }
        }

        void clear() noexcept
        {
            std::unique_lock lock(mutex);
            sprites.clear();
        }

        // ----------------------------------------------------
        // Atlas association
        // ----------------------------------------------------

        void set_atlas(const TextureAtlas* atlas) noexcept;

        [[nodiscard]]
        const TextureAtlas* get_atlas() const noexcept;
    };

    inline void SpriteRegistry::set_atlas(const TextureAtlas* atlas) noexcept
    {
        atlas_ptr.store(atlas, std::memory_order_release);
    }

    inline const TextureAtlas* SpriteRegistry::get_atlas() const noexcept
    {
        return atlas_ptr.load(std::memory_order_acquire);
    }
}
