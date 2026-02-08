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
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
//afontrenderer.hpp

//#include "aplatform.hpp"
//#include "acontext.hpp"
//#include "aatlastexture.hpp"
//#include "aatlasmanager.hpp"
//#include "alogger.hpp"
//
//#include <string>
//#include <unordered_map>
//#include <cstdint>
//#include <memory>
module;

export module afont.renderer;

import <string>;
import <unordered_map>;
import <cstdint>;
import <vector>;

import aspritehandle;

// import the modules that define these types
import aengine.core.logger;
import aatlas.texture;
import atexture;

export namespace almondnamespace::font
{
    export struct Vec2
    {
        float x{};
        float y{};
    };

    export struct FontMetrics
    {
        float ascent = 0.0f;
        float descent = 0.0f;
        float lineGap = 0.0f;
        float lineHeight = 0.0f;
        float spaceAdvance = 0.0f;
        float averageAdvance = 0.0f;
        float maxAdvance = 0.0f;
    };

    export struct Glyph
    {
        Vec2 size_px{};
        Vec2 offset_px{};
        float advance{};
        SpriteHandle handle{};
    };

    export struct FontAsset
    {
        std::string name;
        std::string path;
        float size_pt{};
        std::unordered_map<char32_t, Glyph> glyphs{};
        int atlas_index = -1;
        FontMetrics metrics{};
        std::unordered_map<std::uint64_t, float> kerning_pairs{};

        [[nodiscard]] float get_kerning(char32_t left, char32_t right) const noexcept
        {
            if (kerning_pairs.empty())
                return 0.0f;

            const std::uint64_t key = (static_cast<std::uint64_t>(left) << 32)
                | static_cast<std::uint64_t>(right);
            const auto it = kerning_pairs.find(key);
            if (it == kerning_pairs.end())
                return 0.0f;
            return it->second;
        }
    };

   export class FontRenderer
    {
    public:
        explicit FontRenderer(logger::Logger* log = nullptr);

        bool load_font(const std::string& name,
            const std::string& path,
            float size_pt);

        [[nodiscard]] const FontAsset* get_font(const std::string& name) const noexcept;

        bool render_text_to_atlas(const std::string& font_name,
            const std::string& text,
            TextureAtlas& atlas,
            int x,
            int y);

        void unload_font(const std::string& name);

    private:
        struct BakedGlyph
        {
            Glyph glyph{};
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
        };

        bool load_and_bake_font(const std::string& ttf_path,
            float size_pt,
            std::vector<std::pair<char32_t, BakedGlyph>>& out_glyphs,
            FontMetrics& out_metrics,
            std::unordered_map<std::uint64_t, float>& out_kerning,
            Texture& out_texture);

        logger::Logger* logger_{};
        std::unordered_map<std::string, FontAsset> loaded_fonts_;
    };
}
