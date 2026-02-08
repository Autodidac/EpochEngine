module;

#include <include/aengine.config.hpp>


#if defined(ALMOND_USING_SFML)
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#endif

export module acontext.sfml.textures;

import aengine.platform;

#if defined(ALMOND_USING_SFML)

import aatlas.manager;
import aatlas.texture;
import aimage.loader;
import atexture;
import aspritehandle;

import acontext.sfml.state;

import <algorithm>;
import <atomic>;
import <cstdint>;
import <filesystem>;
import <format>;
import <fstream>;
import <iostream>;
import <span>;
import <stdexcept>;
import <string>;
import <string_view>;
import <unordered_map>;
import <vector>;

export namespace almondnamespace::sfmlcontext
{
    using Handle = uint32_t;

    struct AtlasGPU
    {
        sf::Texture texture{};
        u64 version = static_cast<u64>(-1);
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

    inline std::unordered_map<const TextureAtlas*, AtlasGPU, TextureAtlasPtrHash, TextureAtlasPtrEqual> sfml_gpu_atlases;

    inline std::atomic_uint8_t s_generation{ 1 };
    inline std::atomic_uint32_t s_dumpSerial{ 0 };

    [[nodiscard]]
    inline Handle make_handle(int atlasIdx, int localIdx) noexcept
    {
        return (Handle(s_generation.load(std::memory_order_relaxed)) << 24)
            | ((atlasIdx & 0xFFF) << 12)
            | (localIdx & 0xFFF);
    }

    [[nodiscard]]
    inline ImageData ensure_rgba(const ImageData& img)
    {
        const size_t pixelCount = static_cast<size_t>(img.width) * img.height;
        const size_t channels = img.pixels.size() / pixelCount;

        if (channels == 4) return img;

        if (channels != 3)
            throw std::runtime_error("ensure_rgba(): Unsupported channel count: " + std::to_string(channels));

        std::vector<uint8_t> rgba(pixelCount * 4);
        const uint8_t* src = img.pixels.data();
        uint8_t* dst = rgba.data();

        for (size_t i = 0; i < pixelCount; ++i)
        {
            dst[4 * i + 0] = src[3 * i + 0];
            dst[4 * i + 1] = src[3 * i + 1];
            dst[4 * i + 2] = src[3 * i + 2];
            dst[4 * i + 3] = 255;
        }

        return { std::move(rgba), img.width, img.height, 4 };
    }

    inline std::string make_dump_name(int atlasIdx, std::string_view tag)
    {
        std::filesystem::create_directories("atlases");
        return std::format("atlases/{}_{}_{}.ppm", tag, atlasIdx, s_dumpSerial.fetch_add(1, std::memory_order_relaxed));
    }

    inline void dump_atlas(const TextureAtlas& atlas, int atlasIdx)
    {
        const std::string filename = make_dump_name(atlasIdx, atlas.name);
        std::ofstream out(filename, std::ios::binary);
        if (!out)
        {
            std::cerr << "[Dump] Failed to open: " << filename << "\n";
            return;
        }

        out << "P6\n" << atlas.width << " " << atlas.height << "\n255\n";
        for (size_t i = 0; i < atlas.pixel_data.size(); i += 4)
        {
            out.put(static_cast<char>(atlas.pixel_data[i]));
            out.put(static_cast<char>(atlas.pixel_data[i + 1]));
            out.put(static_cast<char>(atlas.pixel_data[i + 2]));
        }
        std::cerr << "[Dump] Wrote: " << filename << "\n";
    }

    inline void upload_atlas_to_gpu(const TextureAtlas& atlas)
    {
        if (atlas.pixel_data.empty())
        {
            const_cast<TextureAtlas&>(atlas).rebuild_pixels();
        }

        auto& gpu = sfml_gpu_atlases[&atlas];

        if (gpu.version == atlas.version && gpu.texture.getSize().x > 0)
        {
            std::cerr << "[SFML] SKIPPING upload for '" << atlas.name
                << "' version = " << atlas.version << "\n";
            return;
        }

        sf::Image image{};
        image.create(
            static_cast<unsigned>(atlas.width),
            static_cast<unsigned>(atlas.height),
            reinterpret_cast<const sf::Uint8*>(atlas.pixel_data.data())
        );


        if (!gpu.texture.loadFromImage(image))
        {
            throw std::runtime_error("[SFML] Failed to load GPU texture from pixel_data for atlas: " + atlas.name);
        }

        gpu.width = atlas.width;
        gpu.height = atlas.height;
        gpu.version = atlas.version;

        dump_atlas(atlas, atlas.index);

        std::cerr << "[SFML] Uploaded atlas '" << atlas.name
            << "' (" << gpu.width << "x" << gpu.height << ")\n";
    }

    inline void ensure_uploaded(const TextureAtlas& atlas)
    {
        auto it = sfml_gpu_atlases.find(&atlas);
        if (it != sfml_gpu_atlases.end())
        {
            if (it->second.version == atlas.version && it->second.texture.getSize().x > 0)
                return;
        }
        upload_atlas_to_gpu(atlas);
    }

    inline void clear_gpu_atlases() noexcept
    {
        sfml_gpu_atlases.clear();
        s_generation.fetch_add(1, std::memory_order_relaxed);
    }

    inline Handle load_atlas(const TextureAtlas& atlas, int atlasIndex = -1)
    {
        ensure_uploaded(atlas);

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
        if (!addedOpt)
        {
            throw std::runtime_error("atlas_add_texture: Failed to add: " + id);
        }

        ensure_uploaded(atlas);

        return make_handle(atlas.get_index(), addedOpt->index);
    }

    inline void draw_sprite(SpriteHandle handle, std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        if (!handle.is_valid())
        {
            std::cerr << "[SFML_DrawSprite] Invalid sprite handle.\n";
            return;
        }

        const int atlasIdx = int(handle.atlasIndex);
        const int localIdx = int(handle.localIndex);

        if (atlasIdx < 0 || atlasIdx >= int(atlases.size()))
        {
            std::cerr << "[SFML_DrawSprite] Atlas index out of bounds: " << atlasIdx << '\n';
            return;
        }

        const TextureAtlas* atlas = atlases[atlasIdx];
        if (!atlas)
        {
            std::cerr << "[SFML_DrawSprite] Null atlas pointer at index: " << atlasIdx << '\n';
            return;
        }

        AtlasRegion region{};
        if (!atlas->try_get_entry_info(localIdx, region))
        {
            std::cerr << "[SFML_DrawSprite] Sprite index out of bounds: " << localIdx << '\n';
            return;
        }

        ensure_uploaded(*atlas);

        auto it = sfml_gpu_atlases.find(atlas);
        if (it == sfml_gpu_atlases.end())
        {
            std::cerr << "[SFML_DrawSprite] GPU texture not found for atlas '" << atlas->name << "'\n";
            return;
        }

        const auto& gpu = it->second;

        sf::Sprite sprite(gpu.texture);
        sf::IntRect rect(
            sf::Vector2i(static_cast<int>(region.x), static_cast<int>(region.y)),
            sf::Vector2i(static_cast<int>(region.width), static_cast<int>(region.height)));

        sprite.setTextureRect(rect);
        sprite.setPosition(sf::Vector2f(x, y));

        if (width > 0.f && height > 0.f)
        {
            const float scaleX = width / float(region.width);
            const float scaleY = height / float(region.height);
            sprite.setScale(sf::Vector2f(scaleX, scaleY));
        }

        if (!state::s_sfmlstate.window.sfml_window || !state::s_sfmlstate.window.sfml_window->isOpen())
        {
            std::cerr << "[SFML_DrawSprite] Render window is not open.\n";
            return;
        }

        sf::RenderStates renderStates{};
        state::s_sfmlstate.window.sfml_window->draw(sprite, renderStates);
    }
} // namespace almondnamespace::sfmlcontext

#endif // ALMOND_USING_SFML
