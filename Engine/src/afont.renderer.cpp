module;

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION

// If stb_truetype.h uses STBTT_STATIC, define it in the header or here (optional).
// #define STBTT_STATIC

module afont.renderer;

import <algorithm>;
import <array>;
import <cstdint>;
import <fstream>;
import <iostream>;
import <mutex>;
import <optional>;
import <string>;
import <unordered_map>;
import <utility>;
import <vector>;

import afont.renderer;
import aatlas.manager;
import aatlas.texture;

namespace almondnamespace::font
{
    namespace
    {
        [[nodiscard]] std::vector<unsigned char> read_file_binary(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return {};

            file.seekg(0, std::ios::end);
            const std::streamsize size = file.tellg();
            if (size <= 0)
                return {};
            file.seekg(0, std::ios::beg);

            std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
            if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
                return {};

            return buffer;
        }
    }

    almondnamespace::font::FontRenderer::FontRenderer(logger::Logger* log)
        : logger_(log)
    {
    }

    bool FontRenderer::load_font(const std::string& name,
        const std::string& path,
        float size_pt)
    {
        if (loaded_fonts_.contains(name))
            return false;

        FontAsset asset{};
        asset.name = name;
        asset.path = path;
        asset.size_pt = size_pt;

        std::vector<std::pair<char32_t, BakedGlyph>> baked_glyphs{};
        Texture raw_texture{};
        FontMetrics metrics{};
        std::unordered_map<std::uint64_t, float> kerning_pairs{};

        if (!load_and_bake_font(path, size_pt, baked_glyphs, metrics, kerning_pairs, raw_texture))
        {
            std::cerr << "[FontRenderer] Failed to bake font '" << name << "' from '" << path << "'\n";
            return false;
        }

        if (raw_texture.empty())
        {
            std::cerr << "[FontRenderer] Baked texture for font '" << name << "' is empty\n";
            return false;
        }

        raw_texture.name = name;

        static std::mutex atlas_mutex;
        const std::string atlas_name = "font_atlas";
        if (!atlasmanager::get_registrar(atlas_name))
        {
            atlasmanager::create_atlas({
                .name = atlas_name,
                .width = 2048,
                .height = 2048,
                .generate_mipmaps = false
                });
        }

        auto* registrar = atlasmanager::get_registrar(atlas_name);
        if (!registrar)
        {
            std::cerr << "[FontRenderer] Missing registrar for atlas '" << atlas_name << "'\n";
            return false;
        }

        TextureAtlas& atlas = registrar->atlas;

        std::optional<AtlasEntry> maybe_entry;
        {
            std::lock_guard<std::mutex> lock(atlas_mutex);
            maybe_entry = atlas.add_entry(name, raw_texture);
        }

        if (!maybe_entry)
        {
            std::cerr << "[FontRenderer] Failed to pack font '" << name << "' into shared atlas\n";
            return false;
        }

        const AtlasEntry& atlas_entry = *maybe_entry;

        float total_advance = 0.0f;
        std::size_t advance_count = 0;
        float max_advance = 0.0f;

        for (auto& [codepoint, baked] : baked_glyphs)
        {
            Glyph glyph = std::move(baked.glyph);

            const int glyph_width = baked.x1 - baked.x0;
            const int glyph_height = baked.y1 - baked.y0;

            if (glyph_width > 0 && glyph_height > 0)
            {
                const int slice_x = static_cast<int>(atlas_entry.region.x) + baked.x0;
                const int slice_y = static_cast<int>(atlas_entry.region.y) + baked.y0;
                const std::string glyph_name =
                    name + "_pt" + std::to_string(size_pt) +
                    "_cp" + std::to_string(static_cast<std::uint32_t>(codepoint));

                std::optional<AtlasEntry> glyph_entry;
                {
                    std::lock_guard<std::mutex> lock(atlas_mutex);
                    glyph_entry = atlas.add_slice_entry(glyph_name, slice_x, slice_y, glyph_width, glyph_height);
                }

                if (glyph_entry)
                {
                    glyph.handle = SpriteHandle{
                        static_cast<std::uint32_t>(glyph_entry->index),
                        0u,
                        static_cast<std::uint32_t>(atlas.get_index()),
                        static_cast<std::uint32_t>(glyph_entry->index)
                    };
                }
            }

            asset.glyphs.emplace(codepoint, std::move(glyph));

            const Glyph& stored_glyph = asset.glyphs.at(codepoint);
            total_advance += stored_glyph.advance;
            max_advance = (std::max)(max_advance, stored_glyph.advance);
            ++advance_count;

            if (codepoint == U' ')
                metrics.spaceAdvance = stored_glyph.advance;
        }

        if (advance_count > 0)
            metrics.averageAdvance = total_advance / static_cast<float>(advance_count);

        metrics.maxAdvance = (std::max)(metrics.maxAdvance, max_advance);

        if (metrics.spaceAdvance <= 0.0f)
            metrics.spaceAdvance = metrics.averageAdvance;

        asset.atlas_index = atlas.get_index();
        asset.metrics = metrics;
        asset.kerning_pairs = std::move(kerning_pairs);

        almondnamespace::atlasmanager::ensure_uploaded(atlas);

        loaded_fonts_.emplace(name, std::move(asset));
        return true;
    }

    const FontAsset* FontRenderer::get_font(const std::string& name) const noexcept
    {
        auto it = loaded_fonts_.find(name);
        if (it == loaded_fonts_.end())
            return nullptr;
        return &it->second;
    }

    bool FontRenderer::render_text_to_atlas(
        const std::string& font_name,
        const std::string&,
        TextureAtlas&,
        int,
        int)
    {
        return loaded_fonts_.contains(font_name);
    }

    void FontRenderer::unload_font(const std::string& name)
    {
        loaded_fonts_.erase(name);
    }

    bool FontRenderer::load_and_bake_font(const std::string& ttf_path,
        float size_pt,
        std::vector<std::pair<char32_t, BakedGlyph>>& out_glyphs,
        FontMetrics& out_metrics,
        std::unordered_map<std::uint64_t, float>& out_kerning,
        Texture& out_texture)
    {
        out_glyphs.clear();
        out_kerning.clear();
        out_texture.clear();
        out_texture.channels = 4;
        out_texture.name = ttf_path;
        out_metrics = FontMetrics{};

        if (size_pt <= 0.0f)
        {
            std::cerr << "[FontRenderer] Invalid font size '" << size_pt << "' requested for '" << ttf_path << "'\n";
            return false;
        }

        auto font_buffer = read_file_binary(ttf_path);
        if (font_buffer.empty())
        {
            std::cerr << "[FontRenderer] Unable to read font file '" << ttf_path << "'\n";
            return false;
        }

        const int font_offset = stbtt_GetFontOffsetForIndex(font_buffer.data(), 0);
        if (font_offset < 0)
        {
            std::cerr << "[FontRenderer] Invalid font offset for '" << ttf_path << "'\n";
            return false;
        }

        stbtt_fontinfo font{};
        if (!stbtt_InitFont(&font, font_buffer.data(), font_offset))
        {
            std::cerr << "[FontRenderer] Failed to initialise font info for '" << ttf_path << "'\n";
            return false;
        }

        int raw_ascent = 0;
        int raw_descent = 0;
        int raw_line_gap = 0;
        stbtt_GetFontVMetrics(&font, &raw_ascent, &raw_descent, &raw_line_gap);

        const float scale = stbtt_ScaleForPixelHeight(&font, size_pt);
        out_metrics.ascent = static_cast<float>(raw_ascent) * scale;
        out_metrics.descent = static_cast<float>(-raw_descent) * scale;
        out_metrics.lineGap = static_cast<float>(raw_line_gap) * scale;
        out_metrics.lineHeight = out_metrics.ascent + out_metrics.descent + out_metrics.lineGap;

        constexpr int pack_width = 1024;
        constexpr int pack_height = 1024;
        std::vector<unsigned char> mono_bitmap(static_cast<std::size_t>(pack_width) * pack_height, 0);

        stbtt_pack_context pack_context{};
        if (!stbtt_PackBegin(&pack_context, mono_bitmap.data(), pack_width, pack_height, pack_width, 1, nullptr))
        {
            std::cerr << "[FontRenderer] Failed to begin packing for font '" << ttf_path << "'\n";
            return false;
        }

        stbtt_PackSetOversampling(&pack_context, 2, 2);

        const std::array<std::pair<int, int>, 2> ranges_info{ {
            {32, 126},
            {160, 255}
        } };

        std::size_t total_chars = 0;
        for (const auto& range : ranges_info)
            total_chars += static_cast<std::size_t>(range.second - range.first + 1);

        std::vector<stbtt_packedchar> packed_chars(total_chars);
        std::vector<stbtt_pack_range> pack_ranges(ranges_info.size());

        std::size_t packed_offset = 0;
        for (std::size_t i = 0; i < ranges_info.size(); ++i)
        {
            const auto [first_codepoint, last_codepoint] = ranges_info[i];
            const int glyph_count = last_codepoint - first_codepoint + 1;

            pack_ranges[i].font_size = size_pt;
            pack_ranges[i].first_unicode_codepoint_in_range = first_codepoint;
            pack_ranges[i].array_of_unicode_codepoints = nullptr;
            pack_ranges[i].num_chars = glyph_count;
            pack_ranges[i].chardata_for_range = packed_chars.data() + packed_offset;
            pack_ranges[i].h_oversample = 2;
            pack_ranges[i].v_oversample = 2;

            packed_offset += static_cast<std::size_t>(glyph_count);
        }

        if (!stbtt_PackFontRanges(&pack_context, font_buffer.data(), 0, pack_ranges.data(), static_cast<int>(pack_ranges.size())))
        {
            stbtt_PackEnd(&pack_context);
            std::cerr << "[FontRenderer] Failed to pack glyph ranges for font '" << ttf_path << "'\n";
            return false;
        }

        stbtt_PackEnd(&pack_context);

        int max_x1 = 0;
        int max_y1 = 0;
        for (const auto& packed : packed_chars)
        {
            max_x1 = (std::max)(max_x1, static_cast<int>(packed.x1));
            max_y1 = (std::max)(max_y1, static_cast<int>(packed.y1));
        }

        if (max_x1 <= 0 || max_y1 <= 0)
        {
            std::cerr << "[FontRenderer] Packed bitmap for font '" << ttf_path << "' is empty\n";
            return false;
        }

        max_x1 = (std::min)(max_x1, pack_width);
        max_y1 = (std::min)(max_y1, pack_height);

        out_texture.width = static_cast<std::uint32_t>(max_x1);
        out_texture.height = static_cast<std::uint32_t>(max_y1);
        out_texture.channels = 4;
        out_texture.pixels.assign(static_cast<std::size_t>(out_texture.width) * out_texture.height * 4, 0);

        for (std::uint32_t y = 0; y < out_texture.height; ++y)
        {
            for (std::uint32_t x = 0; x < out_texture.width; ++x)
            {
                const unsigned char alpha = mono_bitmap[static_cast<std::size_t>(y) * pack_width + x];
                const std::size_t idx = (static_cast<std::size_t>(y) * out_texture.width + x) * 4;
                out_texture.pixels[idx + 0] = 255;
                out_texture.pixels[idx + 1] = 255;
                out_texture.pixels[idx + 2] = 255;
                out_texture.pixels[idx + 3] = alpha;
            }
        }

        out_glyphs.reserve(total_chars);
        packed_offset = 0;

        for (std::size_t range_index = 0; range_index < ranges_info.size(); ++range_index)
        {
            const auto [first_codepoint, last_codepoint] = ranges_info[range_index];
            const int glyph_count = last_codepoint - first_codepoint + 1;
            const stbtt_packedchar* range_chars = packed_chars.data() + packed_offset;

            for (int glyph_index = 0; glyph_index < glyph_count; ++glyph_index)
            {
                const stbtt_packedchar& packed = range_chars[glyph_index];

                BakedGlyph baked{};
                baked.glyph.size_px = {
                    static_cast<float>(packed.x1 - packed.x0),
                    static_cast<float>(packed.y1 - packed.y0)
                };
                baked.glyph.offset_px = { packed.xoff, packed.yoff };
                baked.glyph.advance = packed.xadvance;
                baked.x0 = packed.x0;
                baked.y0 = packed.y0;
                baked.x1 = packed.x1;
                baked.y1 = packed.y1;

                out_metrics.maxAdvance = (std::max)(out_metrics.maxAdvance, baked.glyph.advance);
                if (first_codepoint + glyph_index == 32)
                    out_metrics.spaceAdvance = baked.glyph.advance;

                out_glyphs.emplace_back(static_cast<char32_t>(first_codepoint + glyph_index), std::move(baked));
            }

            packed_offset += static_cast<std::size_t>(glyph_count);
        }

        if (!out_glyphs.empty())
        {
            out_kerning.reserve(out_glyphs.size() * 4);
            for (const auto& [left_cp, _] : out_glyphs)
            {
                for (const auto& [right_cp, __] : out_glyphs)
                {
                    const int kern = stbtt_GetCodepointKernAdvance(&font,
                        static_cast<int>(left_cp),
                        static_cast<int>(right_cp));
                    if (kern == 0)
                        continue;

                    const float kern_px = static_cast<float>(kern) * scale;
                    const std::uint64_t key =
                        (static_cast<std::uint64_t>(left_cp) << 32) |
                        static_cast<std::uint64_t>(right_cp);
                    out_kerning[key] = kern_px;
                }
            }
        }

        return true;
    }
}
