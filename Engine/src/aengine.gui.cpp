// Engine/src/aengine.gui.cpp
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
 **************************************************************/

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

import aengine.core.context;
import aengine.context.multiplexer;
import aengine.context.window;
import aengine.context.commandqueue;

import aatlas.manager;
import aatlas.texture;
import afont.renderer;
import asprite.pool;
import aspriteregistry;
import aspritehandle;
import atexture;

import aengine.gui;

namespace almondnamespace::gui
{
    using Context = almondnamespace::core::Context;

    constexpr const char* kAtlasName = "__agui_builtin";
    constexpr float       kContentPadding = 1.0f;
    constexpr float       kDefaultFontSizePt = 18.0f;
    constexpr float       kFontScale = 1.0f;
    constexpr float       kTitleScale = 1.1f;
    constexpr float       kLineSpacingFactor = 0.15f;
    constexpr float       kLetterSpacingFactor = 1.0f;
    constexpr float       kBoxInnerPadding = 6.0f;
    constexpr float       kTitleBarPadding = 6.0f;
    constexpr float       kCaretBlinkPeriod = 1.0f;
    constexpr int         kTabSpaces = 4;
    constexpr const char* kDefaultFontName = "__agui_default_font";
    constexpr const char* kDefaultFontFile = "Roboto-Regular.ttf";

    namespace
    {
        [[nodiscard]] static core::RenderPath render_path_for_context(const core::Context* ctx) noexcept
        {
            if (!ctx)
                return core::RenderPath::Unknown;

            switch (ctx->type)
            {
            case core::ContextType::OpenGL:
                return core::RenderPath::OpenGL;
            case core::ContextType::SFML:
                return core::RenderPath::SFML;
            case core::ContextType::Vulkan:
                return core::RenderPath::Vulkan;
            default:
                return core::RenderPath::Unknown;
            }
        }

        struct GuiFontCache
        {
            std::string fontName = kDefaultFontName;
            float fontSizePt = kDefaultFontSizePt;
            const font::FontAsset* asset = nullptr;
            const TextureAtlas* atlas = nullptr;
            font::FontMetrics metrics{};
            std::array<const font::Glyph*, 256> glyphLookup{};
            const font::Glyph* fallbackGlyph = nullptr;
        };

        struct GuiResources
        {
            bool atlasBuilt = false;
            TextureAtlas* atlas = nullptr;

            SpriteHandle windowBackground{};
            SpriteHandle buttonNormal{};
            SpriteHandle buttonHover{};
            SpriteHandle buttonActive{};
            SpriteHandle textField{};
            SpriteHandle textFieldActive{};
            SpriteHandle panelBackground{};
            SpriteHandle consoleBackground{};
            SpriteHandle titleBar{};
            GuiFontCache font{};
            font::FontRenderer fontRenderer{};
        };

        static GuiResources g_resources{};
        static std::mutex g_resourceMutex{};

        struct PtrHash { std::size_t operator()(const void* p) const noexcept { return std::hash<const void*>{}(p); } };

        struct UploadState
        {
            bool guiAtlasUploaded = false;
            bool fontAtlasUploaded = false;
        };

        static std::unordered_map<const void*, UploadState, PtrHash> g_uploadedContexts{};
        static std::mutex g_uploadMutex{};

        struct FrameState
        {
            std::shared_ptr<core::Context> ctxShared{};
            core::Context* ctx = nullptr;

            Vec2 cursor{};
            Vec2 origin{};
            Vec2 windowSize{};
            Vec2 mousePos{};

            bool mouseDown = false;
            bool prevMouseDown = false;
            bool justReleased = false;
            bool insideWindow = false;
            bool justPressed = false;

            std::optional<WidgetBounds> lastButtonBounds{};

            float deltaTime = 0.0f;
            float caretTimer = 0.0f;
            bool caretVisible = true;

            std::vector<InputEvent> events{};
        };

        static thread_local FrameState g_frame{};
        static thread_local std::vector<InputEvent> g_pendingEvents{};
        static thread_local const void* g_activeWidget = nullptr;

        [[nodiscard]] static std::vector<std::uint8_t> make_solid_pixels(
            std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a,
            std::uint32_t w, std::uint32_t h)
        {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4, 0);
            for (std::size_t idx = 0; idx < pixels.size(); idx += 4)
            {
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = a;
            }
            return pixels;
        }

        [[nodiscard]] static SpriteHandle add_sprite(
            TextureAtlas& atlas,
            const std::string& name,
            const std::vector<std::uint8_t>& pixels,
            std::uint32_t w, std::uint32_t h)
        {
            Texture texture{};
            texture.name = name;
            texture.width = w;
            texture.height = h;
            texture.channels = 4;
            texture.pixels = pixels;

            auto entry = atlas.add_entry(name, texture);
            if (!entry)
                throw std::runtime_error("[agui] Failed to add atlas entry: " + name);

            if (almondnamespace::spritepool::capacity == 0)
                almondnamespace::spritepool::initialize(2048);

            SpriteHandle handle = almondnamespace::spritepool::allocate();
            if (!handle.is_valid())
                throw std::runtime_error("[agui] Sprite pool exhausted while registering GUI sprite");

            handle.atlasIndex = static_cast<std::uint32_t>(atlas.get_index());
            handle.localIndex = static_cast<std::uint32_t>(entry->index);

            almondnamespace::atlasmanager::registry.add(
                name, handle,
                entry->region.u1,
                entry->region.v1,
                entry->region.u2 - entry->region.u1,
                entry->region.v2 - entry->region.v1);

            return handle;
        }

        [[nodiscard]] static std::filesystem::path find_default_font_path()
        {
            auto resolve_env_font_path = []() -> std::filesystem::path
                {
#if defined(_WIN32)
                    char* envBuf = nullptr;
                    size_t len = 0;

                    if (_dupenv_s(&envBuf, &len, "ALMOND_GUI_FONT_PATH") == 0 && envBuf)
                    {
                        std::filesystem::path envPath{ envBuf };
                        free(envBuf);
                        return envPath;
                    }
                    return {};
#else
                    if (const char* envValue = std::getenv("ALMOND_GUI_FONT_PATH"))
                        return std::filesystem::path{ envValue };
                    return {};
#endif
                };

            if (std::filesystem::path envPath = resolve_env_font_path(); !envPath.empty())
            {
                std::error_code ec;
                if (std::filesystem::exists(envPath, ec))
                    return envPath;
            }

            const std::array<std::filesystem::path, 5> relativeCandidates{
                std::filesystem::path{kDefaultFontFile},
                std::filesystem::path{"assets/fonts"} / kDefaultFontFile,
                std::filesystem::path{"Fonts"} / kDefaultFontFile,
                std::filesystem::path{"AlmondShell/assets/fonts"} / kDefaultFontFile,
                std::filesystem::path{"../AlmondShell/assets/fonts"} / kDefaultFontFile,
            };

            const auto try_with_root = [&](const std::filesystem::path& root) -> std::filesystem::path
                {
                    for (const auto& rel : relativeCandidates)
                    {
                        std::filesystem::path candidate = root.empty() ? rel : (root / rel);
                        std::error_code ec;
                        if (!candidate.empty() && std::filesystem::exists(candidate, ec))
                            return candidate;
                    }
                    return {};
                };

            if (auto path = try_with_root({}); !path.empty())
                return path;

            const std::filesystem::path cwd = std::filesystem::current_path();
            if (auto path = try_with_root(cwd); !path.empty())
                return path;

            const std::filesystem::path parent = cwd.parent_path();
            if (!parent.empty())
            {
                if (auto path = try_with_root(parent); !path.empty())
                    return path;
            }

            return {};
        }

        static void populate_font_lookup(GuiFontCache& fontCache)
        {
            fontCache.glyphLookup.fill(nullptr);
            fontCache.fallbackGlyph = nullptr;

            if (!fontCache.asset)
                return;

            for (const auto& [codepoint, glyph] : fontCache.asset->glyphs)
            {
                if (codepoint < static_cast<char32_t>(fontCache.glyphLookup.size()))
                    fontCache.glyphLookup[static_cast<std::size_t>(codepoint)] = &glyph;
            }

            auto setFallback = [&](unsigned char ch)
                {
                    if (ch < fontCache.glyphLookup.size() && fontCache.glyphLookup[ch])
                        fontCache.fallbackGlyph = fontCache.glyphLookup[ch];
                };

            setFallback(static_cast<unsigned char>('?'));
            if (!fontCache.fallbackGlyph)
                setFallback(static_cast<unsigned char>(' '));
            if (!fontCache.fallbackGlyph && !fontCache.asset->glyphs.empty())
                fontCache.fallbackGlyph = &fontCache.asset->glyphs.begin()->second;
        }

        static void ensure_font_loaded_locked()
        {
            if (g_resources.font.asset)
                return;

            const std::filesystem::path fontPath = find_default_font_path();
            if (fontPath.empty())
            {
                std::cerr << "[agui] Unable to locate GUI font '" << kDefaultFontFile << "'\n";
                std::cerr << "[agui] Place '" << kDefaultFontFile
                    << "' in 'assets/fonts' (relative to the working directory) or set ALMOND_GUI_FONT_PATH.\n";
                return;
            }

            if (!g_resources.fontRenderer.load_font(g_resources.font.fontName, fontPath.string(), g_resources.font.fontSizePt))
            {
                std::cerr << "[agui] Failed to load GUI font from '" << fontPath.string() << "'\n";
                return;
            }

            g_resources.font.asset = g_resources.fontRenderer.get_font(g_resources.font.fontName);
            if (!g_resources.font.asset)
            {
                std::cerr << "[agui] Font renderer returned no asset for '" << g_resources.font.fontName << "'\n";
                return;
            }

            g_resources.font.metrics = g_resources.font.asset->metrics;
            populate_font_lookup(g_resources.font);

            auto atlasVec = almondnamespace::atlasmanager::get_atlas_vector_snapshot(); // by value snapshot
            if (g_resources.font.asset->atlas_index >= 0 &&
                static_cast<std::size_t>(g_resources.font.asset->atlas_index) < atlasVec.size())
            {
                g_resources.font.atlas = atlasVec[static_cast<std::size_t>(g_resources.font.asset->atlas_index)];
            }

            if (!g_resources.font.atlas)
            {
                if (auto it = almondnamespace::atlasmanager::atlas_map.find("font_atlas");
                    it != almondnamespace::atlasmanager::atlas_map.end())
                {
                    g_resources.font.atlas = it->second.get();
                }
            }
        }

        static void ensure_resources()
        {
            std::scoped_lock lock(g_resourceMutex);

            if (!g_resources.atlasBuilt)
            {
                auto atlasIt = almondnamespace::atlasmanager::atlas_map.find(kAtlasName);
                if (atlasIt == almondnamespace::atlasmanager::atlas_map.end())
                {
                    almondnamespace::atlasmanager::create_atlas({
                        .name = kAtlasName,
                        .width = 512,
                        .height = 512,
                        .generate_mipmaps = false
                        });

                    atlasIt = almondnamespace::atlasmanager::atlas_map.find(kAtlasName);
                    if (atlasIt == almondnamespace::atlasmanager::atlas_map.end())
                        throw std::runtime_error("[agui] Unable to create GUI atlas");
                }

                TextureAtlas& atlas = *atlasIt->second;
                g_resources.atlas = &atlas;

                g_resources.windowBackground = add_sprite(atlas, "__agui/window_bg",
                    make_solid_pixels(0x33, 0x35, 0x38, 0xFF, 8, 8), 8, 8);
                g_resources.buttonNormal = add_sprite(atlas, "__agui/button_normal",
                    make_solid_pixels(0x5B, 0x5F, 0x66, 0xFF, 8, 8), 8, 8);
                g_resources.buttonHover = add_sprite(atlas, "__agui/button_hover",
                    make_solid_pixels(0x76, 0x7C, 0x85, 0xFF, 8, 8), 8, 8);
                g_resources.buttonActive = add_sprite(atlas, "__agui/button_active",
                    make_solid_pixels(0x94, 0x9A, 0xA3, 0xFF, 8, 8), 8, 8);
                g_resources.textField = add_sprite(atlas, "__agui/text_field",
                    make_solid_pixels(0x2B, 0x2E, 0x33, 0xFF, 8, 8), 8, 8);
                g_resources.textFieldActive = add_sprite(atlas, "__agui/text_field_active",
                    make_solid_pixels(0x3A, 0x3E, 0x45, 0xFF, 8, 8), 8, 8);
                g_resources.panelBackground = add_sprite(atlas, "__agui/panel_bg",
                    make_solid_pixels(0x27, 0x29, 0x2E, 0xFF, 8, 8), 8, 8);
                g_resources.consoleBackground = add_sprite(atlas, "__agui/console_bg",
                    make_solid_pixels(0x1F, 0x21, 0x26, 0xFF, 8, 8), 8, 8);
                g_resources.titleBar = add_sprite(atlas, "__agui/title_bar",
                    make_solid_pixels(0x22, 0x24, 0x28, 0xFF, 8, 8), 8, 8);

                g_resources.atlasBuilt = true;
            }

            ensure_font_loaded_locked();
        }

        static void perform_backend_upload(Context& ctx)
        {
            ensure_resources();

            std::scoped_lock lock(g_uploadMutex);
            auto& state = g_uploadedContexts[&ctx];

            if (!state.guiAtlasUploaded && g_resources.atlas)
            {
                const std::uint32_t handle = ctx.add_atlas_safe(*g_resources.atlas);
                if (handle == 0)
                    throw std::runtime_error("[agui] Failed to upload GUI atlas for this context");
                state.guiAtlasUploaded = true;
            }

            if (!state.fontAtlasUploaded && g_resources.font.atlas)
            {
                const std::uint32_t handle = ctx.add_atlas_safe(*g_resources.font.atlas);
                if (handle != 0)
                    state.fontAtlasUploaded = true;
            }
        }

        static void ensure_backend_upload(Context& ctx)
        {
            if (auto current = core::get_current_render_context(); current && current.get() == &ctx)
            {
                perform_backend_upload(ctx);
                return;
            }

            if (ctx.windowData)
            {
                std::weak_ptr<Context> weak = ctx.windowData->context;
                const core::RenderPath renderPath = render_path_for_context(&ctx);
                ctx.windowData->commandQueue.enqueue([weak]()
                    {
                        if (auto self = weak.lock())
                        {
                            try { perform_backend_upload(*self); }
                            catch (...) { /* GUI optional */ }
                        }
                    }, renderPath);
                return;
            }

            perform_backend_upload(ctx);
        }

        static void draw_sprite(const SpriteHandle& handle, float x, float y, float w, float h)
        {
            if (!handle.is_valid())
                return;

            Context* ctx = g_frame.ctx;
            if (!ctx)
                return;

            if (ctx->windowData && g_frame.ctxShared)
            {
                auto ctxShared = g_frame.ctxShared;
                const core::RenderPath renderPath = render_path_for_context(ctx);

                ctx->windowData->commandQueue.enqueue([ctxShared, handle, x, y, w, h]()
                    {
                        if (!ctxShared)
                            return;

                        auto atlases = almondnamespace::atlasmanager::get_atlas_vector_snapshot();
                        std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
                        ctxShared->draw_sprite_safe(handle, span, x, y, w, h);
                    }, renderPath);

                return;
            }

            bool onRenderThread = false;
            if (auto current = core::MultiContextManager::GetCurrent())
                onRenderThread = (current.get() == ctx);

            if (!ctx->windowData || onRenderThread)
            {
                auto atlases = almondnamespace::atlasmanager::get_atlas_vector_snapshot();
                std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
                ctx->draw_sprite_safe(handle, span, x, y, w, h);
            }
        }

        [[nodiscard]] static bool point_in_rect(Vec2 p, float x, float y, float w, float h) noexcept
        {
            return (p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h);
        }

        [[nodiscard]] static float base_line_height(float scale) noexcept
        {
            const auto& metrics = g_resources.font.metrics;
            if (metrics.ascent > 0.0f || metrics.descent > 0.0f)
                return (metrics.ascent + metrics.descent) * scale;
            return 8.0f * scale;
        }

        [[nodiscard]] static float line_advance_amount(float scale) noexcept
        {
            const auto& metrics = g_resources.font.metrics;
            const float baseHeight = base_line_height(scale);
            const bool hasMetrics = (metrics.ascent > 0.0f || metrics.descent > 0.0f);

            if (hasMetrics)
            {
                const float lineHeight = (metrics.lineHeight > 0.0f)
                    ? metrics.lineHeight
                    : (metrics.ascent + metrics.descent + metrics.lineGap);

                const float advance = lineHeight * scale;
                const float minimumAdvance = (std::max)(advance, baseHeight);

                if (metrics.lineGap > 0.0f)
                    return minimumAdvance;

                return minimumAdvance + baseHeight * kLineSpacingFactor;
            }

            return baseHeight + baseHeight * kLineSpacingFactor;
        }

        [[nodiscard]] static float baseline_offset(float scale) noexcept
        {
            const auto& metrics = g_resources.font.metrics;
            if (metrics.ascent > 0.0f)
                return metrics.ascent * scale;
            return base_line_height(scale);
        }

        [[nodiscard]] static float space_advance(float scale) noexcept
        {
            if (g_resources.font.metrics.spaceAdvance > 0.0f)
                return g_resources.font.metrics.spaceAdvance * scale;
            if (const auto* glyph = g_resources.font.glyphLookup[static_cast<unsigned char>(' ')])
                return glyph->advance * scale;
            if (g_resources.font.metrics.averageAdvance > 0.0f)
                return g_resources.font.metrics.averageAdvance * scale;
            return 8.0f * scale;
        }

        [[nodiscard]] static float letter_spacing(float scale) noexcept
        {
            const float average = g_resources.font.metrics.averageAdvance;
            if (average <= 0.0f || kLetterSpacingFactor <= 0.0f)
                return 0.0f;
            return average * scale * kLetterSpacingFactor;
        }

        [[nodiscard]] static const font::Glyph* lookup_glyph(unsigned char ch) noexcept
        {
            if (!g_resources.font.asset)
                return nullptr;

            if (ch < g_resources.font.glyphLookup.size())
            {
                if (const auto* glyph = g_resources.font.glyphLookup[ch])
                    return glyph;
            }

            return g_resources.font.fallbackGlyph;
        }

        [[nodiscard]] static std::optional<unsigned char> next_drawable_char(std::string_view text, std::size_t index) noexcept
        {
            if (index >= text.size())
                return std::nullopt;

            for (std::size_t i = index + 1; i < text.size(); ++i)
            {
                const char next = text[i];
                if (next == '\n')
                    return std::nullopt;
                return static_cast<unsigned char>(next);
            }

            return std::nullopt;
        }

        [[nodiscard]] static float kerning_adjust(unsigned char left, unsigned char right, float scale) noexcept
        {
            if (!g_resources.font.asset)
                return 0.0f;

            const float kern = g_resources.font.asset->get_kerning(left, right);
            if (kern == 0.0f)
                return 0.0f;
            return kern * scale;
        }

        [[nodiscard]] static float glyph_advance(unsigned char ch, float scale) noexcept
        {
            if (ch == '\t')
                return space_advance(scale) * static_cast<float>(kTabSpaces);

            if (const auto* glyph = lookup_glyph(ch))
                return glyph->advance * scale;

            if (g_resources.font.metrics.averageAdvance > 0.0f)
                return g_resources.font.metrics.averageAdvance * scale;

            return 8.0f * scale;
        }

        [[nodiscard]] static float glyph_advance_with_kerning(
            unsigned char ch,
            std::optional<unsigned char> next,
            float scale) noexcept
        {
            float advance = glyph_advance(ch, scale);
            if (next)
                advance += kerning_adjust(ch, *next, scale);
            if (ch != ' ' && ch != '\t')
                advance += letter_spacing(scale);
            return advance;
        }

        [[nodiscard]] static float measure_text_width(std::string_view text, float scale) noexcept
        {
            float current = 0.0f;
            float maxWidth = 0.0f;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (ch == '\n')
                {
                    maxWidth = (std::max)(maxWidth, current);
                    current = 0.0f;
                    continue;
                }
                const auto next = next_drawable_char(text, i);
                current += glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
            }
            return (std::max)(maxWidth, current);
        }

        [[nodiscard]] static float measure_wrapped_text_height(std::string_view text, float width, float scale) noexcept
        {
            ensure_resources();
            const float effectiveWidth = (std::max)(space_advance(scale), width);
            const float lineAdvance = line_advance_amount(scale);
            const float baseHeight = base_line_height(scale);

            std::size_t lines = 1;
            float penX = 0.0f;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (ch == '\n')
                {
                    ++lines;
                    penX = 0.0f;
                    continue;
                }

                const auto next = next_drawable_char(text, i);
                const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
                if (penX > 0.0f && penX + advance > effectiveWidth + 0.001f)
                {
                    ++lines;
                    penX = 0.0f;
                }

                penX += advance;
            }

            const float totalHeight = baseHeight + static_cast<float>(lines - 1) * lineAdvance;
            return (std::max)(baseHeight, totalHeight);
        }

        [[nodiscard]] static Vec2 compute_caret_position(std::string_view text, float x, float y, float width, float scale) noexcept
        {
            ensure_resources();
            const float effectiveWidth = (std::max)(space_advance(scale), width);
            const float lineAdvance = line_advance_amount(scale);

            float penX = x;
            float baseline = y + baseline_offset(scale);

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (ch == '\n')
                {
                    penX = x;
                    baseline += lineAdvance;
                    continue;
                }

                const auto next = next_drawable_char(text, i);
                const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
                if (penX > x && penX + advance > x + effectiveWidth + 0.001f)
                {
                    penX = x;
                    baseline += lineAdvance;
                }

                penX += advance;
            }

            const float caretTop = baseline - baseline_offset(scale);
            return { penX, caretTop };
        }

        static float draw_wrapped_text(std::string_view text, float x, float y, float width, float scale)
        {
            ensure_resources();
            if (!g_frame.ctx || !g_resources.font.asset)
                return 0.0f;

            const float effectiveWidth = (std::max)(space_advance(scale), width);
            const float lineAdvance = line_advance_amount(scale);
            const float ascent = baseline_offset(scale);
            const float baseHeight = base_line_height(scale);

            float penX = x;
            float baseline = y + ascent;
            std::size_t lines = 1;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (ch == '\n')
                {
                    penX = x;
                    baseline += lineAdvance;
                    ++lines;
                    continue;
                }

                const auto next = next_drawable_char(text, i);
                const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
                if (penX > x && penX + advance > x + effectiveWidth + 0.001f)
                {
                    penX = x;
                    baseline += lineAdvance;
                    ++lines;
                }

                if (ch != ' ' && ch != '\t')
                {
                    if (const auto* glyph = lookup_glyph(static_cast<unsigned char>(ch)))
                    {
                        const float drawW = glyph->size_px.x * scale;
                        const float drawH = glyph->size_px.y * scale;
                        if (glyph->handle.is_valid() && drawW > 0.0f && drawH > 0.0f)
                        {
                            const float offsetX = glyph->offset_px.x * scale;
                            const float offsetY = glyph->offset_px.y * scale;
                            draw_sprite(glyph->handle, penX + offsetX, baseline + offsetY, drawW, drawH);
                        }
                    }
                }

                penX += advance;
            }

            return baseHeight + static_cast<float>(lines - 1) * lineAdvance;
        }

        static void draw_text_line(std::string_view text, float x, float y, float scale, std::optional<float> indent = std::nullopt)
        {
            ensure_resources();
            if (!g_frame.ctx || !g_resources.font.asset)
                return;

            const float anchorX = indent.value_or(x);
            float penX = anchorX;
            float baseline = y + baseline_offset(scale);
            const float lineAdvance = line_advance_amount(scale);

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if (ch == '\n')
                {
                    penX = anchorX;
                    baseline += lineAdvance;
                    continue;
                }

                if (const auto* glyph = lookup_glyph(static_cast<unsigned char>(ch)))
                {
                    if (glyph->handle.is_valid())
                    {
                        const float drawW = glyph->size_px.x * scale;
                        const float drawH = glyph->size_px.y * scale;
                        const float offsetX = glyph->offset_px.x * scale;
                        const float offsetY = glyph->offset_px.y * scale;
                        draw_sprite(glyph->handle, penX + offsetX, baseline + offsetY, drawW, drawH);
                    }
                }

                const auto next = next_drawable_char(text, i);
                penX += glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
            }
        }

        static void draw_caret(float x, float y, float height)
        {
            const float caretWidth = (std::max)(1.0f, space_advance(kFontScale) * 0.1f);
            draw_sprite(g_resources.buttonActive, x, y, caretWidth, height);
        }

        static void reset_frame()
        {
            g_frame.cursor = {};
            g_frame.origin = {};
            g_frame.windowSize = {};
            g_frame.insideWindow = false;
            g_frame.lastButtonBounds.reset();
        }
    } // namespace

    // -----------------------------
    // Exported API implementations
    // -----------------------------

    void set_cursor(Vec2 position) noexcept
    {
        if (!g_frame.insideWindow) return;
        g_frame.cursor = position;
    }

    void advance_cursor(Vec2 delta) noexcept
    {
        if (!g_frame.insideWindow) return;
        g_frame.cursor.x += delta.x;
        g_frame.cursor.y += delta.y;
    }

    void push_input(const InputEvent& e) noexcept
    {
        g_pendingEvents.push_back(e);
    }

    void begin_frame(const std::shared_ptr<core::Context>& ctx, float dt, Vec2 mouse_pos, bool mouse_down) noexcept
    {
        core::Context* rawCtx = ctx.get();
        if (rawCtx)
        {
            try { ensure_backend_upload(*rawCtx); }
            catch (...) { /* GUI optional */ }
        }

        g_frame.ctxShared = ctx;
        g_frame.ctx = rawCtx;
        g_frame.deltaTime = dt;

        g_frame.caretTimer += dt;
        while (g_frame.caretTimer >= kCaretBlinkPeriod)
            g_frame.caretTimer -= kCaretBlinkPeriod;

        g_frame.caretVisible = (g_frame.caretTimer < (kCaretBlinkPeriod * 0.5f));

        const bool prevMouseDown = g_frame.mouseDown;
        bool currentMouseDown = mouse_down;
        Vec2 currentMousePos = mouse_pos;

        g_frame.events.clear();
        if (!g_pendingEvents.empty())
        {
            g_frame.events.insert(g_frame.events.end(), g_pendingEvents.begin(), g_pendingEvents.end());
            g_pendingEvents.clear();
        }

        for (const auto& evt : g_frame.events)
        {
            switch (evt.type)
            {
            case EventType::MouseMove: currentMousePos = evt.mouse_pos; break;
            case EventType::MouseDown: currentMouseDown = true;  currentMousePos = evt.mouse_pos; break;
            case EventType::MouseUp:   currentMouseDown = false; currentMousePos = evt.mouse_pos; break;
            default: break;
            }
        }

        g_frame.prevMouseDown = prevMouseDown;
        g_frame.mouseDown = currentMouseDown;
        g_frame.mousePos = currentMousePos;
        g_frame.justPressed = (!prevMouseDown && currentMouseDown);
        g_frame.justReleased = (prevMouseDown && !currentMouseDown);

        reset_frame();
    }

    void end_frame() noexcept
    {
        g_frame.ctxShared.reset();
        g_frame.ctx = nullptr;
        g_frame.insideWindow = false;
        g_frame.events.clear();
        g_frame.justPressed = false;
        g_frame.justReleased = false;
    }

    void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept
    {
        if (!g_frame.ctx) return;

        try { ensure_resources(); }
        catch (...) { return; }

        g_frame.origin = position;
        g_frame.windowSize = size;
        g_frame.insideWindow = true;

        draw_sprite(g_resources.windowBackground, position.x, position.y, size.x, size.y);

        const float titleHeight = line_advance_amount(kTitleScale);
        const float titleBarHeight = titleHeight + 2.0f * kTitleBarPadding;
        const float titleTextY = position.y + (titleBarHeight - titleHeight) * 0.5f;

        draw_sprite(g_resources.titleBar, position.x, position.y, size.x, titleBarHeight);
        draw_text_line(title, position.x + kContentPadding, titleTextY, kTitleScale);

        set_cursor({ position.x + kContentPadding, position.y + titleBarHeight + kContentPadding });
    }

    void end_window() noexcept
    {
        g_frame.insideWindow = false;
    }

    bool button(std::string_view label, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float lineAdvance = line_advance_amount(kFontScale);
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kContentPadding);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);

        const SpriteHandle background =
            hovered ? (g_frame.mouseDown ? g_resources.buttonActive : g_resources.buttonHover)
            : g_resources.buttonNormal;

        draw_sprite(background, pos.x, pos.y, width, height);

        const float textWidth = measure_text_width(label, kFontScale);
        const float textHeight = lineAdvance;
        const float textX = pos.x + (std::max)(0.0f, (width - textWidth) * 0.5f);
        const float textY = pos.y + (std::max)(0.0f, (height - textHeight) * 0.5f);

        draw_text_line(label, textX, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });

        return hovered && g_frame.justPressed;
    }

    bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return false;

        const Vec2 pos = g_frame.cursor;
        const float width = (std::max)(static_cast<float>(size.x), 1.0f);
        const float height = (std::max)(static_cast<float>(size.y), 1.0f);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);

        const SpriteHandle background =
            hovered ? (g_frame.mouseDown ? g_resources.buttonActive : g_resources.buttonHover)
            : g_resources.buttonNormal;

        draw_sprite(background, pos.x, pos.y, width, height);

        const float inset = (std::min)(kBoxInnerPadding, (std::min)(width, height) * 0.2f);
        draw_sprite(sprite, pos.x + inset, pos.y + inset, (std::max)(1.0f, width - 2.0f * inset), (std::max)(1.0f, height - 2.0f * inset));

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });

        return hovered && g_frame.justPressed;
    }

    std::optional<WidgetBounds> last_button_bounds() noexcept
    {
        return g_frame.lastButtonBounds;
    }

    float line_height() noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 16.0f; }
        return line_advance_amount(kFontScale);
    }

    float glyph_width() noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 8.0f; }
        return space_advance(kFontScale);
    }

    void label(std::string_view text) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        draw_text_line(text, g_frame.cursor.x, g_frame.cursor.y, kFontScale);
        advance_cursor({ 0.0f, line_advance_amount(kFontScale) });
    }

    EditBoxResult edit_box(std::string& text, Vec2 size, std::size_t max_chars, bool multiline) noexcept
    {
        EditBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx) return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) * 4.0f;
        const float minHeight = baseHeight + 2.0f * kBoxInnerPadding;

        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), minHeight);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);
        const void* id = static_cast<const void*>(&text);

        if (g_frame.justPressed)
        {
            if (hovered)
            {
                g_activeWidget = id;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
            }
            else if (g_activeWidget == id)
            {
                g_activeWidget = nullptr;
            }
        }

        if (g_frame.justReleased && !hovered && g_activeWidget == id)
            g_activeWidget = nullptr;

        const bool active = (g_activeWidget == id);
        result.active = active;

        const SpriteHandle background = active ? g_resources.textFieldActive : g_resources.textField;
        draw_sprite(background, pos.x, pos.y, width, height);

        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);
        const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
        const float textX = pos.x + kBoxInnerPadding;
        const float textY = pos.y + kBoxInnerPadding;

        const std::size_t limit = (max_chars == 0)
            ? std::numeric_limits<std::size_t>::max()
            : max_chars;

        // Handle input first (mutates `text`)
        if (active)
        {
            for (const auto& evt : g_frame.events)
            {
                switch (evt.type)
                {
                case EventType::TextInput:
                    for (char ch : evt.text)
                    {
                        if (ch == '\r') continue;

                        if (ch == '\n')
                        {
                            if (multiline)
                            {
                                if (text.size() < limit) { text.push_back('\n'); result.changed = true; }
                            }
                            else
                            {
                                result.submitted = true;
                            }
                            continue;
                        }

                        if (static_cast<unsigned char>(ch) < 32) continue;
                        if (text.size() < limit) { text.push_back(ch); result.changed = true; }
                    }
                    break;

                case EventType::KeyDown:
                    if (evt.key == 8 || evt.key == 127) // backspace/del-ish
                    {
                        if (!text.empty()) { text.pop_back(); result.changed = true; }
                    }
                    else if (evt.key == 27) // ESC
                    {
                        g_activeWidget = nullptr;
                        result.active = false;
                    }
                    else if (evt.key == 13) // ENTER
                    {
                        if (multiline)
                        {
                            if (text.size() < limit) { text.push_back('\n'); result.changed = true; }
                        }
                        else
                        {
                            result.submitted = true;
                        }
                    }
                    break;

                default:
                    break;
                }
            }
        }

        // Create view after edits.
        const std::string_view sv{ text };

        if (multiline) draw_wrapped_text(sv, textX, textY, contentWidth, kFontScale);
        else           draw_text_line(sv, textX, textY, kFontScale);

        if (active && g_frame.caretVisible)
        {
            const Vec2 caret = multiline
                ? compute_caret_position(sv, textX, textY, contentWidth, kFontScale)
                : Vec2{ textX + measure_text_width(sv, kFontScale), textY };

            const float caretHeight = multiline ? (std::min)(contentHeight, baseHeight) : baseHeight;

            const float caretRight = textX + (std::max)(1.0f, contentWidth) - 1.0f;
            const float caretX = std::clamp(caret.x, textX, caretRight);
            const float caretY = std::clamp(caret.y, textY,
                textY + (std::max)(0.0f, contentHeight - caretHeight));

            draw_caret(caretX, caretY, caretHeight);
        }

        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    void text_box(std::string_view text, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) * 4.0f;

        float width = static_cast<float>(size.x);
        if (width <= 0.0f)
        {
            const float estimated = measure_text_width(text, kFontScale) + 2.0f * kBoxInnerPadding;
            width = (std::max)(minWidth, estimated);
        }
        else
        {
            width = (std::max)(width, minWidth);
        }

        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);

        float height = static_cast<float>(size.y);
        if (height <= 0.0f)
        {
            const float textHeight = measure_wrapped_text_height(text, contentWidth, kFontScale);
            height = textHeight + 2.0f * kBoxInnerPadding;
        }
        else
        {
            height = (std::max)(height, baseHeight + 2.0f * kBoxInnerPadding);
        }

        draw_sprite(g_resources.panelBackground, pos.x, pos.y, width, height);
        draw_wrapped_text(text, pos.x + kBoxInnerPadding, pos.y + kBoxInnerPadding, contentWidth, kFontScale);

        advance_cursor({ 0.0f, height + kContentPadding });
    }

    ConsoleWindowResult console_window(const ConsoleWindowOptions& options, std::string& input)
    {
        ConsoleWindowResult result{};
        if (!g_frame.ctx) return result;

        // Prefer the explicit parameter, but allow legacy options.input.
        std::string* inputPtr = &input;
        if (options.input && options.input != &input)
        {
            // If callers pass a different pointer, honor options.input (but this should be avoided).
            inputPtr = options.input;
        }
        else if (options.input)
        {
            inputPtr = options.input;
        }

        begin_window(options.title, options.position, options.size);
        if (!g_frame.insideWindow || !g_frame.ctx) { end_window(); return result; }

        ensure_resources();

        const float availableWidth = (std::max)(0.0f, options.size.x - 2.0f * kContentPadding);
        const float logHeight = (std::max)(0.0f, options.size.y - 3.0f * kContentPadding - base_line_height(kFontScale));
        const Vec2 logPos = g_frame.cursor;

        if (availableWidth > 0.0f && logHeight > 0.0f)
            draw_sprite(g_resources.consoleBackground, logPos.x, logPos.y, availableWidth, logHeight);

        const float contentWidth = (std::max)(1.0f, availableWidth - 2.0f * kBoxInnerPadding);
        float penY = logPos.y + kBoxInnerPadding;
        const float maxY = logPos.y + (std::max)(0.0f, logHeight - kBoxInnerPadding);

        if (!options.lines.empty() && logHeight > 0.0f)
        {
            const std::size_t count = options.lines.size();
            const std::size_t start = (count > options.max_visible_lines) ? (count - options.max_visible_lines) : 0;

            for (std::size_t i = start; i < count; ++i)
            {
                const std::string& line = options.lines[i];
                const float drawn = draw_wrapped_text(line, logPos.x + kBoxInnerPadding, penY, contentWidth, kFontScale);

                const float paragraphGap = (std::max)(0.0f, line_advance_amount(kFontScale) - base_line_height(kFontScale));
                penY += drawn + paragraphGap;
                if (penY > maxY) break;
            }
        }

        set_cursor({ logPos.x, logPos.y + logHeight + kContentPadding });

        if (inputPtr)
        {
            const float fieldHeight = base_line_height(kFontScale) + 2.0f * kBoxInnerPadding;
            Vec2 inputSize{ availableWidth, fieldHeight };
            result.input = edit_box(*inputPtr, inputSize, options.max_input_chars, options.multiline_input);
        }

        end_window();
        return result;
    }
} // namespace almondnamespace::gui
