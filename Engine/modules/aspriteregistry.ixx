module;

export module aspriteregistry;

// ────────────────────────────────────────────────────────────
// Standard library
// ────────────────────────────────────────────────────────────

import <atomic>;
import <iostream>;
import <optional>;
import <shared_mutex>;
import <string>;
import <string_view>;
import <tuple>;
import <unordered_map>;

// ────────────────────────────────────────────────────────────
// Engine modules
// ────────────────────────────────────────────────────────────

import aspritehandle;
import asprite.pool;

// ────────────────────────────────────────────────────────────

export namespace almondnamespace
{
    // Forward declaration only — definition lives in atlas module
    struct TextureAtlas;

    // ────────────────────────────────────────────────────────
    // Transparent lookup helpers
    // ────────────────────────────────────────────────────────

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

    // ────────────────────────────────────────────────────────
    // SpriteRegistry
    // ────────────────────────────────────────────────────────

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
            return sprites.erase(name) > 0;
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

        void set_atlas(const TextureAtlas* atlas) noexcept
        {
            atlas_ptr.store(atlas, std::memory_order_release);
        }

        [[nodiscard]]
        const TextureAtlas* get_atlas() const noexcept
        {
            return atlas_ptr.load(std::memory_order_acquire);
        }
    };
}
