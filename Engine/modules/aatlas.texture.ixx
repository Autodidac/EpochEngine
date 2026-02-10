/**************************************************************
 *   AlmondShell - Modular C++ Framework
 **************************************************************/

module;

export module aatlas.texture;

// ────────────────────────────────────────────────────────────
// STANDARD LIBRARY IMPORTS
// ────────────────────────────────────────────────────────────

import <string>;
import <vector>;
import <cstdint>;
import <optional>;
import <iostream>;
import <algorithm>;
import <mutex>;
import <shared_mutex>; // legacy include; this module now uses recursive_mutex for atlas entry protection
import <unordered_map>;
import <memory>;    // std::unique_ptr
import <utility>;   // std::pair

// ────────────────────────────────────────────────────────────
// ENGINE DEPENDENCIES
// ────────────────────────────────────────────────────────────

import atexture;        // provides Texture

// ────────────────────────────────────────────────────────────
// MODULE EXPORTS
// ────────────────────────────────────────────────────────────

export namespace almondnamespace
{
    using u8 = std::uint8_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    // ────────────────────────────────────────────────────────
    // ATLAS REGION
    // ────────────────────────────────────────────────────────

    struct AtlasRegion
    {
        float u1{}, v1{};
        float u2{}, v2{};
        u32   x{}, y{};
        u32   width{}, height{};

        [[nodiscard]] float uv_width()  const noexcept { return u2 - u1; }
        [[nodiscard]] float uv_height() const noexcept { return v2 - v1; }
    };

    // ────────────────────────────────────────────────────────
    // ATLAS ENTRY
    // ────────────────────────────────────────────────────────

    struct AtlasEntry
    {
        int                 index{ -1 };
        std::string         name;
        AtlasRegion         region;
        std::vector<u8>     pixels;
        u32                 texWidth{ 0 };
        u32                 texHeight{ 0 };

        AtlasEntry() = default;

        AtlasEntry(
            int idx,
            std::string name_,
            AtlasRegion region_,
            std::vector<u8> pixels_,
            u32 w,
            u32 h)
            : index(idx),
            name(std::move(name_)),
            region(region_),
            pixels(std::move(pixels_)),
            texWidth(w),
            texHeight(h)
        {
        }
    };

    // ────────────────────────────────────────────────────────
    // ATLAS CONFIG
    // ────────────────────────────────────────────────────────

    struct AtlasConfig
    {
        std::string name;
        u32 width{ 2048 };
        u32 height{ 2048 };
        bool generate_mipmaps{ false };
        int index{ 0 };
    };

    // ────────────────────────────────────────────────────────
    // TEXTURE ATLAS
    // ────────────────────────────────────────────────────────

    struct TextureAtlas
    {
        std::string name;
        int index{ -1 };
        u32 width{ 0 };
        u32 height{ 0 };
        bool has_mipmaps{ false };

        mutable u64 version{ 0 };
        mutable std::vector<u8> pixel_data;

        std::vector<AtlasEntry> entries;

        TextureAtlas() = default;

        TextureAtlas(std::string n, int w, int h)
            : name(std::move(n)), width(static_cast<u32>(w)), height(static_cast<u32>(h))
        {
        }

        TextureAtlas(const TextureAtlas&) = delete;
        TextureAtlas& operator=(const TextureAtlas&) = delete;
        TextureAtlas(TextureAtlas&&) = delete;
        TextureAtlas& operator=(TextureAtlas&&) = delete;

        bool init(const AtlasConfig& config)
        {
            name = config.name;
            index = config.index;
            width = config.width;
            height = config.height;
            has_mipmaps = config.generate_mipmaps;

            pixel_data.clear();
            pixel_data.resize(
                static_cast<std::size_t>(width) * height * 4, 0
            );

            occupancy.clear();
            occupancy.assign(height, std::vector<bool>(width, false));

            {
                std::unique_lock lock(entriesMutex);
                entries.clear();
                lookup.clear();
            }

            version = 0;
            return true;
        }

        [[nodiscard]] size_t entry_count() const noexcept
        {
            std::lock_guard lock(entriesMutex);
            return entries.size();
        }

        [[nodiscard]] bool try_get_entry_info(
            int index,
            AtlasRegion& outRegion,
            std::string* outName = nullptr) const
        {
            std::lock_guard lock(entriesMutex);

            if (index < 0 || static_cast<size_t>(index) >= entries.size())
                return false;

            const auto& entry = entries[static_cast<size_t>(index)];
            if (entry.region.width == 0 || entry.region.height == 0)
                return false;

            outRegion = entry.region;
            if (outName)
                *outName = entry.name;

            return true;
        }

        static std::unique_ptr<TextureAtlas> create(const AtlasConfig& config)
        {
            auto atlas = std::make_unique<TextureAtlas>();
            atlas->name = config.name;
            atlas->index = config.index;
            atlas->width = config.width;
            atlas->height = config.height;
            atlas->has_mipmaps = config.generate_mipmaps;
            atlas->pixel_data.resize(
                static_cast<size_t>(atlas->width) * atlas->height * 4, 0);
            atlas->occupancy.assign(
                atlas->height, std::vector<bool>(atlas->width, false));

            return atlas;
        }

        [[nodiscard]] int get_index() const noexcept { return index; }

        std::optional<AtlasEntry> add_entry(const std::string& id, const Texture& tex);
        std::optional<AtlasEntry> add_slice_entry(const std::string& id, int x, int y, int w, int h);
        std::optional<AtlasRegion> get_region(const std::string& id) const;
        void rebuild_pixels() const;

    private:
        // IMPORTANT:
        // This atlas is accessed by both upload/build paths and GUI query paths.
        // In practice, some call chains re-enter atlas reads while holding the write lock
        // on the SAME THREAD (e.g. build/add -> code that queries regions for debug/GUI).
        // std::shared_mutex is not re-entrant; that pattern deadlocks (and MSVC will often
        // trip a debug check).
        //
        // We use std::recursive_mutex as a pragmatic correctness fix. If you want the
        // shared-read perf back later, switch to a snapshot/RCU-style structure.
        mutable std::recursive_mutex entriesMutex;
        std::unordered_map<std::string, AtlasRegion> lookup;
        std::vector<std::vector<bool>> occupancy;

        std::optional<std::pair<u32, u32>> try_pack(u32 w, u32 h);
        bool can_place(u32 x, u32 y, u32 w, u32 h) const;
        void mark_used(u32 x, u32 y, u32 w, u32 h);
    };
}

namespace almondnamespace
{
    inline std::optional<AtlasEntry> TextureAtlas::add_entry(const std::string& id, const Texture& tex)
    {
        if (tex.width == 0 || tex.height == 0 || tex.pixels.empty()) {
            std::cerr << "[Atlas] Rejected empty texture '" << id << "'\n";
            return std::nullopt;
        }

        std::unique_lock<std::recursive_mutex> lock(entriesMutex);

        if (lookup.contains(id)) {
            std::cerr << "[Atlas] Duplicate ID: '" << id << "'\n";
            return std::nullopt;
        }

        auto pos = try_pack(tex.width, tex.height);
        if (!pos) {
            std::cerr << "[Atlas] Failed to pack '" << id << "'\n";
            return std::nullopt;
        }

        auto [x, y] = *pos;
        const u32 stride = width * 4;

        for (u32 row = 0; row < tex.height; ++row) {
            u8* dst = pixel_data.data() + ((y + row) * stride) + (x * 4);
            const u8* src = tex.pixels.data() + (row * tex.width * 4);
            std::copy_n(src, tex.width * 4, dst);
        }

        AtlasRegion region{
            .u1 = static_cast<float>(x) / width,
            .v1 = static_cast<float>(height - (y + tex.height)) / height,
            .u2 = static_cast<float>(x + tex.width) / width,
            .v2 = static_cast<float>(height - y) / height,
            .x = x,
            .y = y,
            .width = tex.width,
            .height = tex.height
        };

        int entryIndex = static_cast<int>(entries.size());
        AtlasEntry entry{ entryIndex, id, region, tex.pixels, tex.width, tex.height };
        entries.push_back(entry);
        lookup.emplace(id, region);
        ++version;
#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
        std::cerr << "[Atlas] Added '" << id << "' at (" << x << ", " << y
            << ") EntryIndex=" << entryIndex << "\n";
#endif
        return entry;
    }

    inline std::optional<AtlasEntry> TextureAtlas::add_slice_entry(
        const std::string& id,
        int x,
        int y,
        int w,
        int h)
    {
        std::unique_lock<std::recursive_mutex> lock(entriesMutex);

        if (w <= 0 || h <= 0) {
            std::cerr << "[Atlas] Invalid slice size for '" << id << "'\n";
            return std::nullopt;
        }

        if (lookup.contains(id)) {
            std::cerr << "[Atlas] Duplicate ID: '" << id << "'\n";
            return std::nullopt;
        }

        AtlasRegion region{
            .u1 = static_cast<float>(x) / static_cast<float>(width),
            .v1 = static_cast<float>(height - (y + h)) / static_cast<float>(height),
            .u2 = static_cast<float>(x + w) / static_cast<float>(width),
            .v2 = static_cast<float>(height - y) / static_cast<float>(height),
            .x = static_cast<u32>(x),
            .y = static_cast<u32>(y),
            .width = static_cast<u32>(w),
            .height = static_cast<u32>(h)
        };

        const int entryIndex = static_cast<int>(entries.size());
        AtlasEntry entry{
            entryIndex,
            id,
            region,
            {},
            static_cast<u32>(w),
            static_cast<u32>(h)
        };

        entries.emplace_back(entry);
        lookup.emplace(id, region);
        ++version;

#if defined(DEBUG_TEXTURE_RENDERING_VERBOSE)
        std::cerr << "[Atlas] Added slice entry '" << id << "' at ("
            << x << ", " << y << ") size [" << w << "x" << h << "] "
            << "EntryIndex=" << entryIndex << "\n";
#endif
        return entry;
    }

    inline std::optional<AtlasRegion> TextureAtlas::get_region(const std::string& id) const
    {
        std::lock_guard lock(entriesMutex);
        auto it = lookup.find(id);
        return (it != lookup.end()) ? std::optional{ it->second } : std::nullopt;
    }

    inline void TextureAtlas::rebuild_pixels() const
    {
        std::unique_lock<std::recursive_mutex> lock(entriesMutex);
        const size_t size = static_cast<size_t>(width) * height * 4;
        if (pixel_data.size() != size) {
            pixel_data.resize(size);
        }

        std::fill(pixel_data.begin(), pixel_data.end(), 0);
        const size_t stride = static_cast<size_t>(width) * 4;

        for (const auto& entry : entries) {
            if (entry.pixels.empty()) {
                continue;
            }

            const size_t requiredBytes = static_cast<size_t>(entry.texWidth)
                * static_cast<size_t>(entry.texHeight) * 4;
            if (entry.pixels.size() < requiredBytes) {
                std::cerr << "[Atlas] Skipping rebuild for entry '" << entry.name
                    << "' due to insufficient pixel data (have "
                    << entry.pixels.size() << ", need " << requiredBytes << ")\n";
                continue;
            }

            for (u32 row = 0; row < entry.texHeight; ++row) {
                auto dst = pixel_data.data()
                    + ((entry.region.y + row) * stride)
                    + (entry.region.x * 4);
                auto src = entry.pixels.data() + (static_cast<size_t>(row) * entry.texWidth * 4);
                std::copy_n(src, static_cast<size_t>(entry.texWidth) * 4, dst);
            }
        }

        ++version;
    }

    inline std::optional<std::pair<u32, u32>> TextureAtlas::try_pack(u32 w, u32 h)
    {
        for (u32 y = 0; y + h <= height; ++y) {
            for (u32 x = 0; x + w <= width; ++x) {
                if (can_place(x, y, w, h)) {
                    mark_used(x, y, w, h);
                    return { std::pair{ x, y } };
                }
            }
        }

        return std::nullopt;
    }

    inline bool TextureAtlas::can_place(u32 x, u32 y, u32 w, u32 h) const
    {
        for (u32 dy = 0; dy < h; ++dy) {
            for (u32 dx = 0; dx < w; ++dx) {
                if (occupancy[y + dy][x + dx]) {
                    return false;
                }
            }
        }

        return true;
    }

    inline void TextureAtlas::mark_used(u32 x, u32 y, u32 w, u32 h)
    {
        for (u32 dy = 0; dy < h; ++dy) {
            for (u32 dx = 0; dx < w; ++dx) {
                occupancy[y + dy][x + dx] = true;
            }
        }
    }
}
