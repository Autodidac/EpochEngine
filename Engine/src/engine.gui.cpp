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

// Engine/src/engine.gui.cpp

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
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

#if defined(_WIN32)
#include "framework.hpp"
#endif

module engine.gui;

import context.type;
import core.context;
import context.multiplexer;
import context.window;
import context.commandqueue;

import atlas.manager;
import atlas.texture;
import core.path;
import core.logger;
import font.renderer;
import sprite.pool;
import spriteregistry;
import spritehandle;
import texture;

namespace epochnamespace::gui
{
    using Context = epochnamespace::core::Context;

    constexpr const char* kAtlasName = "__agui_builtin";
    constexpr float       kContentPadding = 6.0f;
    constexpr float       kDefaultFontSizePt = 16.0f;
    constexpr float       kFontScale = 1.0f;
    constexpr float       kTitleScale = 1.1f;
    constexpr float       kLineSpacingFactor = 0.15f;
    constexpr float       kLetterSpacingFactor = 0.0f;
    constexpr float       kBoxInnerPadding = 5.0f;
    constexpr float       kTitleBarPadding = 6.0f;
    constexpr float       kButtonTextClipInset = 2.0f;
    constexpr float       kTextClipSlack = 2.0f;
    constexpr float       kCaretBlinkPeriod = 1.0f;
    constexpr float       kMinQueuedSpriteExtent = 0.5f;
    constexpr float       kMaxQueuedSpriteExtent = 65536.0f;
    constexpr int         kTabSpaces = 4;
    constexpr const char* kDefaultFontName = "__agui_default_font";
    constexpr const char* kDefaultFontFile = "Roboto-Regular.ttf";
    constexpr const char* kRuntimeSurfaceAtlasName = "__agui_runtime_surfaces";
    constexpr std::uint32_t kRuntimeSurfaceAtlasSize = 4096;

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
            case core::ContextType::DirectX:
                return core::RenderPath::DirectX;
            default:
                return core::RenderPath::Unknown;
            }
        }

        [[nodiscard]] static bool backend_owns_scene_viewport(const core::Context* ctx) noexcept
        {
            if (!ctx)
                return false;

            switch (ctx->type)
            {
            case core::ContextType::OpenGL:
            case core::ContextType::Vulkan:
            case core::ContextType::RayLib:
            case core::ContextType::SFML:
            case core::ContextType::SDL:
            case core::ContextType::DirectX:
            case core::ContextType::Software:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] static const char* viewport_fallback_name(const core::Context* ctx) noexcept
        {
            if (!ctx)
                return "Scene";

            switch (ctx->type)
            {
            case core::ContextType::RayLib:
                return "RayLib";
            case core::ContextType::SDL:
                return "SDL";
            case core::ContextType::SFML:
                return "SFML";
            case core::ContextType::Software:
                return "Software";
            case core::ContextType::OpenGL:
                return "OpenGL";
            case core::ContextType::Vulkan:
                return "Vulkan";
            case core::ContextType::DirectX:
                return "DirectX";
            default:
                return "Scene";
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
            struct PaletteSprites
            {
                SpriteHandle windowBackground{};
                SpriteHandle buttonNormal{};
                SpriteHandle buttonHover{};
                SpriteHandle buttonActive{};
                SpriteHandle textField{};
                SpriteHandle textFieldActive{};
                SpriteHandle panelBackground{};
                SpriteHandle consoleBackground{};
                SpriteHandle titleBar{};
                SpriteHandle modalScrim{};
            };

            bool atlasBuilt = false;
            TextureAtlas* atlas = nullptr;
            TextureAtlas* runtimeSurfaceAtlas = nullptr;

            PaletteSprites defaultDark{};
            PaletteSprites classicLauncher{};
            GuiFontCache font{};
            font::FontRenderer fontRenderer{};
        };

        struct CachedRuntimeSurface
        {
            SpriteHandle handle{};
            std::uint64_t contentHash = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t version = 0;
        };

        static GuiResources g_resources{};
        static std::mutex g_resourceMutex{};
        static std::unordered_map<std::string, CachedRuntimeSurface> g_runtimeSurfaceCache{};
        static std::mutex g_runtimeSurfaceCacheMutex{};
        static bool g_missingFontPathWarningLogged = false;
        static bool g_failedFontLoadWarningLogged = false;
        static bool g_missingFontAssetWarningLogged = false;

        struct PtrHash { std::size_t operator()(const void* p) const noexcept { return std::hash<const void*>{}(p); } };

        struct UploadState
        {
            bool guiAtlasUploaded = false;
            bool fontAtlasUploaded = false;
        };

        struct QueuedSpriteDraw
        {
            SpriteHandle handle{};
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
        };

        struct DeferredDrawBatch
        {
            std::shared_ptr<std::vector<QueuedSpriteDraw>> draws{};
            std::uint64_t generation = 0;
        };

        static std::unordered_map<const void*, UploadState, PtrHash> g_uploadedContexts{};
        static std::mutex g_uploadMutex{};
        static std::unordered_map<const void*, DeferredDrawBatch, PtrHash> g_deferredDrawBatches{};
        static std::unordered_map<const void*, DeferredDrawBatch, PtrHash> g_topLayerDrawBatches{};
        static std::mutex g_deferredBatchMutex{};
        static std::unordered_map<const void*, std::vector<InputEvent>, PtrHash> g_contextPendingEvents{};
        static std::mutex g_contextPendingEventsMutex{};
        static thread_local std::unordered_map<const void*, bool, PtrHash> g_contextMouseDownStates{};
        static thread_local std::unordered_map<const void*, const void*, PtrHash> g_contextActiveWidgets{};
        static thread_local std::unordered_map<const void*, std::size_t, PtrHash> g_contextPressedButtonKeys{};

        struct ScrollTextState
        {
            std::size_t firstLine = 0;
            std::size_t selectedLine = 0;
            bool hasSelection = false;
            std::size_t lastLineCount = 0;
            bool draggingScrollbar = false;
            float dragGrabOffset = 0.0f;
        };

        struct ScrollAreaState
        {
            float scrollY = 0.0f;
            float contentHeight = 0.0f;
            bool draggingScrollbar = false;
            float dragGrabOffset = 0.0f;
        };

        struct SelectBoxState
        {
            bool open = false;
        };

        struct ScrollAreaFrame
        {
            std::string key{};
            Vec2 previousCursor{};
            Vec2 previousMin{};
            Vec2 previousMax{};
            Vec2 viewportMin{};
            Vec2 viewportMax{};
            float viewportHeight = 0.0f;
            float viewportWidth = 0.0f;
            float scrollY = 0.0f;
            bool showScrollbar = true;
        };

        static thread_local std::unordered_map<std::string, ScrollTextState> g_scrollTextStates{};
        static thread_local std::unordered_map<std::string, ScrollAreaState> g_scrollAreaStates{};
        static thread_local std::unordered_map<std::string, SelectBoxState> g_selectBoxStates{};
        static thread_local std::vector<ScrollAreaFrame> g_scrollAreaStack{};

        [[nodiscard]] static bool any_select_box_open() noexcept
        {
            for (const auto& [key, state] : g_selectBoxStates)
            {
                (void)key;
                if (state.open)
                    return true;
            }
            return false;
        }

        [[nodiscard]] static bool is_select_box_list_id(std::string_view id) noexcept
        {
            return id.size() >= 5u && id.substr(id.size() - 5u) == "-list";
        }

        struct FrameState
        {
            std::shared_ptr<core::Context> ctxShared{};
            core::Context* ctx = nullptr;

            Vec2 cursor{};
            Vec2 origin{};
            Vec2 windowSize{};
            Vec2 contentMin{};
            Vec2 contentMax{};
            Vec2 mousePos{};
            std::string windowKey{};
            std::uint64_t widgetSerial = 0;

            bool mouseDown = false;
            bool prevMouseDown = false;
            bool justReleased = false;
            bool insideWindow = false;
            bool justPressed = false;
            int mouseWheelDelta = 0;

            std::optional<WidgetBounds> lastButtonBounds{};

            float deltaTime = 0.0f;
            float caretTimer = 0.0f;
            bool caretVisible = true;
            ThemeVariant activeTheme = ThemeVariant::DefaultDark;

            std::vector<InputEvent> events{};
            std::vector<QueuedSpriteDraw> queuedDraws{};
            std::vector<QueuedSpriteDraw> topLayerDraws{};
            std::vector<ThemeVariant> themeStack{};
            int topLayerDepth = 0;
        };

        static thread_local FrameState g_frame{};
        static thread_local std::vector<InputEvent> g_pendingEvents{};

        [[nodiscard]] static const GuiResources::PaletteSprites& active_palette() noexcept
        {
            switch (g_frame.activeTheme)
            {
            case ThemeVariant::ClassicLauncher:
                return g_resources.classicLauncher;
            case ThemeVariant::DefaultDark:
            default:
                return g_resources.defaultDark;
            }
        }

        [[nodiscard]] static bool rects_intersect(
            float ax,
            float ay,
            float aw,
            float ah,
            float bx,
            float by,
            float bw,
            float bh) noexcept
        {
            return ax < bx + bw
                && ax + aw > bx
                && ay < by + bh
                && ay + ah > by;
        }

        [[nodiscard]] static bool has_content_clip() noexcept
        {
            return g_frame.insideWindow
                && g_frame.contentMax.x > g_frame.contentMin.x
                && g_frame.contentMax.y > g_frame.contentMin.y;
        }

        struct ContentClipScope
        {
            Vec2 previousMin{};
            Vec2 previousMax{};

            ContentClipScope(Vec2 clipMin, Vec2 clipMax) noexcept
            {
                previousMin = g_frame.contentMin;
                previousMax = g_frame.contentMax;
                g_frame.contentMin = {
                    (std::max)(previousMin.x, clipMin.x),
                    (std::max)(previousMin.y, clipMin.y)
                };
                g_frame.contentMax = {
                    (std::min)(previousMax.x, clipMax.x),
                    (std::min)(previousMax.y, clipMax.y)
                };
            }

            ~ContentClipScope()
            {
                g_frame.contentMin = previousMin;
                g_frame.contentMax = previousMax;
            }
        };

        [[nodiscard]] static float content_right() noexcept
        {
            return has_content_clip() ? g_frame.contentMax.x : (g_frame.origin.x + g_frame.windowSize.x);
        }

        [[nodiscard]] static float content_bottom() noexcept
        {
            return has_content_clip() ? g_frame.contentMax.y : (g_frame.origin.y + g_frame.windowSize.y);
        }

        [[nodiscard]] static float content_available_width(float cursorX) noexcept
        {
            return (std::max)(8.0f, content_right() - cursorX);
        }

        [[nodiscard]] static std::string scroll_panel_key(std::string_view id)
        {
            const auto ctxValue = reinterpret_cast<std::uintptr_t>(g_frame.ctx);
            return std::to_string(ctxValue) + "|" + std::string(id);
        }

        [[nodiscard]] static std::string widget_state_key(std::string_view id)
        {
            const auto ctxValue = reinterpret_cast<std::uintptr_t>(g_frame.ctx);
            return std::to_string(ctxValue) + "|" + std::string(id);
        }

        [[nodiscard]] static bool uses_deferred_gui_batch(const core::Context* ctx) noexcept
        {
            return ctx
                && (ctx->type == core::ContextType::Software
                    || ctx->type == core::ContextType::SDL
                    || ctx->type == core::ContextType::SFML
                    || ctx->type == core::ContextType::RayLib
                    || ctx->type == core::ContextType::Vulkan
                    || ctx->type == core::ContextType::DirectX);
        }

        [[nodiscard]] static bool same_queued_draw(
            const QueuedSpriteDraw& lhs,
            const QueuedSpriteDraw& rhs) noexcept
        {
            const auto same_pixel_x = static_cast<int>(std::floor(lhs.x)) == static_cast<int>(std::floor(rhs.x));
            const auto same_pixel_y = static_cast<int>(std::floor(lhs.y)) == static_cast<int>(std::floor(rhs.y));
            const auto same_pixel_w = static_cast<int>(std::lround(lhs.w)) == static_cast<int>(std::lround(rhs.w));
            const auto same_pixel_h = static_cast<int>(std::lround(lhs.h)) == static_cast<int>(std::lround(rhs.h));
            return lhs.handle == rhs.handle
                && same_pixel_x
                && same_pixel_y
                && same_pixel_w
                && same_pixel_h;
        }

        [[nodiscard]] static bool same_queued_draws(
            const std::vector<QueuedSpriteDraw>& lhs,
            const std::vector<QueuedSpriteDraw>& rhs) noexcept
        {
            if (lhs.size() != rhs.size())
                return false;

            for (std::size_t i = 0; i < lhs.size(); ++i)
            {
                if (!same_queued_draw(lhs[i], rhs[i]))
                    return false;
            }

            return true;
        }

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

        [[nodiscard]] static std::uint64_t hash_surface_pixels(
            std::span<const std::uint8_t> pixels,
            std::uint32_t width,
            std::uint32_t height) noexcept
        {
            constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
            constexpr std::uint64_t kFnvPrime = 1099511628211ull;

            std::uint64_t hash = kFnvOffset;
            auto mix = [&](std::uint64_t value) noexcept
            {
                hash ^= value;
                hash *= kFnvPrime;
            };

            mix(width);
            mix(height);
            mix(pixels.size());

            for (const auto byte : pixels)
            {
                hash ^= byte;
                hash *= kFnvPrime;
            }

            return hash;
        }

        [[nodiscard]] static SpriteHandle try_add_sprite(
            TextureAtlas& atlas,
            const std::string& name,
            const std::vector<std::uint8_t>& pixels,
            std::uint32_t w,
            std::uint32_t h,
            bool queueUpload) noexcept
        {
            try
            {
                Texture texture{};
                texture.name = name;
                texture.width = w;
                texture.height = h;
                texture.channels = 4;
                texture.pixels = pixels;

                auto entry = atlas.add_entry(name, texture);
                if (!entry)
                    return {};

                if (epochnamespace::spritepool::capacity == 0)
                    epochnamespace::spritepool::initialize(2048);

                SpriteHandle handle = epochnamespace::spritepool::allocate();
                if (!handle.is_valid())
                    return {};

                handle.atlasIndex = static_cast<std::uint32_t>(atlas.get_index());
                handle.localIndex = static_cast<std::uint32_t>(entry->index);

                epochnamespace::atlasmanager::registry.add(
                    name, handle,
                    entry->region.u1,
                    entry->region.v1,
                    entry->region.u2 - entry->region.u1,
                    entry->region.v2 - entry->region.v1);

                if (queueUpload)
                    epochnamespace::atlasmanager::ensure_uploaded(atlas);

                return handle;
            }
            catch (...)
            {
                return {};
            }
        }

        [[nodiscard]] static SpriteHandle add_sprite(
            TextureAtlas& atlas,
            const std::string& name,
            const std::vector<std::uint8_t>& pixels,
            std::uint32_t w, std::uint32_t h)
        {
            SpriteHandle handle = try_add_sprite(atlas, name, pixels, w, h, false);
            if (!handle.is_valid())
                throw std::runtime_error("[agui] Failed to register GUI sprite: " + name);
            return handle;
        }

        [[nodiscard]] static TextureAtlas* ensure_runtime_surface_atlas_locked()
        {
            if (g_resources.runtimeSurfaceAtlas)
                return g_resources.runtimeSurfaceAtlas;

            auto atlasIt = epochnamespace::atlasmanager::atlas_map.find(kRuntimeSurfaceAtlasName);
            if (atlasIt == epochnamespace::atlasmanager::atlas_map.end())
            {
                (void)epochnamespace::atlasmanager::create_atlas({
                    .name = kRuntimeSurfaceAtlasName,
                    .width = kRuntimeSurfaceAtlasSize,
                    .height = kRuntimeSurfaceAtlasSize,
                    .generate_mipmaps = false
                    });
                atlasIt = epochnamespace::atlasmanager::atlas_map.find(kRuntimeSurfaceAtlasName);
            }

            if (atlasIt == epochnamespace::atlasmanager::atlas_map.end())
                return nullptr;

            g_resources.runtimeSurfaceAtlas = atlasIt->second.get();
            return g_resources.runtimeSurfaceAtlas;
        }

        [[nodiscard]] static std::filesystem::path find_default_font_path()
        {
            auto resolve_env_font_path = []() -> std::filesystem::path
                {
#if defined(_WIN32)
                    char* envBuf = nullptr;
                    size_t len = 0;

                    if (_dupenv_s(&envBuf, &len, "EPOCH_GUI_FONT_PATH") == 0 && envBuf)
                    {
                        std::filesystem::path envPath{ envBuf };
                        free(envBuf);
                        return envPath;
                    }
                    return {};
#else
                    if (const char* envValue = std::getenv("EPOCH_GUI_FONT_PATH"))
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

            const std::array<std::filesystem::path, 3> relativeCandidates{
                std::filesystem::path{kDefaultFontFile},
                std::filesystem::path{"assets/fonts"} / kDefaultFontFile,
                std::filesystem::path{"Fonts"} / kDefaultFontFile,
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

            if (const auto exampleAssets = epoch::core::path::example_asset_dir(); !exampleAssets.empty())
            {
                if (auto path = try_with_root(exampleAssets / "fonts"); !path.empty())
                    return path;
                if (auto path = try_with_root(exampleAssets); !path.empty())
                    return path;
            }

            if (const auto engineAssets = epoch::core::path::engine_asset_dir(); !engineAssets.empty())
            {
                if (auto path = try_with_root(engineAssets / "fonts"); !path.empty())
                    return path;
                if (auto path = try_with_root(engineAssets); !path.empty())
                    return path;
            }

            if (const auto runtimeRoot = epoch::core::path::runtime_root_dir(); !runtimeRoot.empty())
            {
                if (auto path = try_with_root(runtimeRoot); !path.empty())
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
                if (!g_missingFontPathWarningLogged)
                {
                    logger::warn(
                        "Epoch.GUI",
                        std::format(
                            "Unable to locate GUI font '{}'. Place it in the resolved example/runtime assets/fonts path or set EPOCH_GUI_FONT_PATH.",
                            kDefaultFontFile));
                    g_missingFontPathWarningLogged = true;
                }
                return;
            }

            if (!g_resources.fontRenderer.load_font(g_resources.font.fontName, fontPath.string(), g_resources.font.fontSizePt))
            {
                if (!g_failedFontLoadWarningLogged)
                {
                    logger::error(
                        "Epoch.GUI",
                        std::format("Failed to load GUI font from '{}'", fontPath.string()));
                    g_failedFontLoadWarningLogged = true;
                }
                return;
            }

            g_resources.font.asset = g_resources.fontRenderer.get_font(g_resources.font.fontName);
            if (!g_resources.font.asset)
            {
                if (!g_missingFontAssetWarningLogged)
                {
                    logger::error(
                        "Epoch.GUI",
                        std::format("Font renderer returned no asset for '{}'", g_resources.font.fontName));
                    g_missingFontAssetWarningLogged = true;
                }
                return;
            }

            g_resources.font.metrics = g_resources.font.asset->metrics;
            populate_font_lookup(g_resources.font);

            auto atlasVec = epochnamespace::atlasmanager::get_atlas_vector_snapshot(); // by value snapshot
            if (g_resources.font.asset->atlas_index >= 0 &&
                static_cast<std::size_t>(g_resources.font.asset->atlas_index) < atlasVec.size())
            {
                g_resources.font.atlas = atlasVec[static_cast<std::size_t>(g_resources.font.asset->atlas_index)];
            }

            if (!g_resources.font.atlas)
            {
                if (auto it = epochnamespace::atlasmanager::atlas_map.find("font_atlas");
                    it != epochnamespace::atlasmanager::atlas_map.end())
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
                auto atlasIt = epochnamespace::atlasmanager::atlas_map.find(kAtlasName);
                if (atlasIt == epochnamespace::atlasmanager::atlas_map.end())
                {
                    epochnamespace::atlasmanager::create_atlas({
                        .name = kAtlasName,
                        .width = 512,
                        .height = 512,
                        .generate_mipmaps = false
                        });

                    atlasIt = epochnamespace::atlasmanager::atlas_map.find(kAtlasName);
                    if (atlasIt == epochnamespace::atlasmanager::atlas_map.end())
                        throw std::runtime_error("[agui] Unable to create GUI atlas");
                }

                TextureAtlas& atlas = *atlasIt->second;
                g_resources.atlas = &atlas;

                g_resources.defaultDark.windowBackground = add_sprite(atlas, "__agui/window_bg",
                    make_solid_pixels(0x1F, 0x23, 0x2A, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.buttonNormal = add_sprite(atlas, "__agui/button_normal",
                    make_solid_pixels(0x31, 0x36, 0x3F, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.buttonHover = add_sprite(atlas, "__agui/button_hover",
                    make_solid_pixels(0x3C, 0x43, 0x4E, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.buttonActive = add_sprite(atlas, "__agui/button_active",
                    make_solid_pixels(0x48, 0x52, 0x60, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.textField = add_sprite(atlas, "__agui/text_field",
                    make_solid_pixels(0x21, 0x26, 0x2E, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.textFieldActive = add_sprite(atlas, "__agui/text_field_active",
                    make_solid_pixels(0x2A, 0x31, 0x3B, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.panelBackground = add_sprite(atlas, "__agui/panel_bg",
                    make_solid_pixels(0x25, 0x2B, 0x34, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.consoleBackground = add_sprite(atlas, "__agui/console_bg",
                    make_solid_pixels(0x18, 0x1D, 0x24, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.titleBar = add_sprite(atlas, "__agui/title_bar",
                    make_solid_pixels(0x2B, 0x31, 0x3B, 0xFF, 8, 8), 8, 8);
                g_resources.defaultDark.modalScrim = add_sprite(atlas, "__agui/modal_scrim",
                    make_solid_pixels(0x05, 0x07, 0x0B, 0xB8, 8, 8), 8, 8);

                g_resources.classicLauncher.windowBackground = add_sprite(atlas, "__agui_classic/window_bg",
                    make_solid_pixels(0x33, 0x35, 0x38, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.buttonNormal = add_sprite(atlas, "__agui_classic/button_normal",
                    make_solid_pixels(0x5B, 0x5F, 0x66, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.buttonHover = add_sprite(atlas, "__agui_classic/button_hover",
                    make_solid_pixels(0x6C, 0x71, 0x7A, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.buttonActive = add_sprite(atlas, "__agui_classic/button_active",
                    make_solid_pixels(0x4C, 0x52, 0x5C, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.textField = add_sprite(atlas, "__agui_classic/text_field",
                    make_solid_pixels(0x2B, 0x2E, 0x33, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.textFieldActive = add_sprite(atlas, "__agui_classic/text_field_active",
                    make_solid_pixels(0x3A, 0x3E, 0x45, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.panelBackground = add_sprite(atlas, "__agui_classic/panel_bg",
                    make_solid_pixels(0x27, 0x29, 0x2E, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.consoleBackground = add_sprite(atlas, "__agui_classic/console_bg",
                    make_solid_pixels(0x1F, 0x21, 0x26, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.titleBar = add_sprite(atlas, "__agui_classic/title_bar",
                    make_solid_pixels(0x22, 0x24, 0x28, 0xFF, 8, 8), 8, 8);
                g_resources.classicLauncher.modalScrim = add_sprite(atlas, "__agui_classic/modal_scrim",
                    make_solid_pixels(0x04, 0x05, 0x07, 0xB8, 8, 8), 8, 8);

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
                auto shared = ctx.windowData->context
                    ? std::reinterpret_pointer_cast<Context>(ctx.windowData->context)
                    : std::shared_ptr<Context>{};
                const core::RenderPath renderPath = render_path_for_context(&ctx);
                ctx.windowData->commandQueue.enqueue([shared = std::move(shared)]()
                    {
                        if (shared)
                        {
                            try { perform_backend_upload(*shared); }
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

            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)
                || w < kMinQueuedSpriteExtent || h < kMinQueuedSpriteExtent
                || w > kMaxQueuedSpriteExtent || h > kMaxQueuedSpriteExtent)
            {
                return;
            }

            Context* ctx = g_frame.ctx;
            if (!ctx)
                return;

            if (has_content_clip())
            {
                const float left = (std::max)(x, g_frame.contentMin.x);
                const float top = (std::max)(y, g_frame.contentMin.y);
                const float right = (std::min)(x + w, g_frame.contentMax.x);
                const float bottom = (std::min)(y + h, g_frame.contentMax.y);
                if (right <= left || bottom <= top)
                {
                    return;
                }

                x = left;
                y = top;
                w = right - left;
                h = bottom - top;
                if (w < kMinQueuedSpriteExtent || h < kMinQueuedSpriteExtent)
                    return;
            }

            if (ctx->windowData && g_frame.ctxShared)
            {
                QueuedSpriteDraw draw{
                    .handle = handle,
                    .x = x,
                    .y = y,
                    .w = w,
                    .h = h
                    };
                if (g_frame.topLayerDepth > 0)
                {
                    g_frame.topLayerDraws.push_back(draw);
                    return;
                }

                g_frame.queuedDraws.push_back(draw);
                return;
            }

            auto atlases = epochnamespace::atlasmanager::get_atlas_vector_snapshot();
            std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
            ctx->draw_sprite_safe(handle, span, x, y, w, h);
        }

        [[nodiscard]] static bool point_in_rect(Vec2 p, float x, float y, float w, float h) noexcept
        {
            return (p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h);
        }

        [[nodiscard]] static bool point_in_active_clip(Vec2 p) noexcept
        {
            return !has_content_clip()
                || point_in_rect(
                    p,
                    g_frame.contentMin.x,
                    g_frame.contentMin.y,
                    g_frame.contentMax.x - g_frame.contentMin.x,
                    g_frame.contentMax.y - g_frame.contentMin.y);
        }

        [[nodiscard]] static std::size_t widget_press_key(std::string_view label, Vec2 pos, Vec2 size) noexcept
        {
            std::size_t h = static_cast<std::size_t>(1469598103934665603ull);
            const auto mix = [&h](std::uint64_t value) noexcept
            {
                h ^= static_cast<std::size_t>(value);
                h *= static_cast<std::size_t>(1099511628211ull);
            };
            for (const unsigned char ch : g_frame.windowKey)
                mix(ch);
            for (const unsigned char ch : label)
                mix(ch);

            const auto quantize = [](float value) noexcept -> std::uint64_t
            {
                return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::lround(value * 4.0f)));
            };
            mix(quantize(pos.x));
            mix(quantize(pos.y));
            mix(quantize(size.x));
            mix(quantize(size.y));
            return h == 0 ? 1 : h;
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

            if (ch >= 128u)
                ch = static_cast<unsigned char>('?');

            const auto glyphIndex = static_cast<std::size_t>(ch);
            if (glyphIndex < g_resources.font.glyphLookup.size())
            {
                if (const auto* glyph = g_resources.font.glyphLookup[glyphIndex])
                    return glyph;
            }

            return g_resources.font.fallbackGlyph;
        }

        [[nodiscard]] static unsigned char safe_draw_char(unsigned char ch) noexcept
        {
            if (ch == '\t' || ch == '\n')
                return ch;
            if (ch < 32u)
                return static_cast<unsigned char>(' ');
            if (ch >= 128u)
                return static_cast<unsigned char>('?');
            return ch;
        }

#if defined(_WIN32)
        [[nodiscard]] static std::wstring utf8_to_wide(std::string_view text)
        {
            if (text.empty())
                return {};

            const int required = ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0);

            if (required <= 0)
            {
                std::wstring fallback{};
                fallback.reserve(text.size());
                for (unsigned char ch : text)
                    fallback.push_back(ch < 128u ? static_cast<wchar_t>(ch) : L'?');
                return fallback;
            }

            std::wstring out(static_cast<std::size_t>(required), L'\0');
            ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                out.data(),
                required);
            return out;
        }

        [[nodiscard]] static std::string wide_to_utf8(std::wstring_view text)
        {
            if (text.empty())
                return {};

            const int required = ::WideCharToMultiByte(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0,
                nullptr,
                nullptr);

            if (required <= 0)
            {
                std::string fallback{};
                fallback.reserve(text.size());
                for (wchar_t ch : text)
                    fallback.push_back((ch >= 0 && ch < 128) ? static_cast<char>(ch) : '?');
                return fallback;
            }

            std::string out(static_cast<std::size_t>(required), '\0');
            ::WideCharToMultiByte(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(text.size()),
                out.data(),
                required,
                nullptr,
                nullptr);
            return out;
        }

        [[nodiscard]] static std::string clipboard_read_text()
        {
            std::string out{};
            if (!::OpenClipboard(nullptr))
                return out;

            if (HANDLE handle = ::GetClipboardData(CF_UNICODETEXT))
            {
                if (const auto* wide = static_cast<const wchar_t*>(::GlobalLock(handle)))
                {
                    out = wide_to_utf8(std::wstring_view{ wide });
                    ::GlobalUnlock(handle);
                }
            }

            ::CloseClipboard();
            return out;
        }

        [[nodiscard]] static bool clipboard_write_text(std::string_view text)
        {
            if (!::OpenClipboard(nullptr))
                return false;

            const std::wstring wide = utf8_to_wide(text);
            const SIZE_T bytes = (wide.size() + 1u) * sizeof(wchar_t);
            HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (!memory)
            {
                ::CloseClipboard();
                return false;
            }

            bool wrote = false;
            if (void* locked = ::GlobalLock(memory))
            {
                std::memcpy(locked, wide.c_str(), bytes);
                ::GlobalUnlock(memory);
                ::EmptyClipboard();
                if (!::SetClipboardData(CF_UNICODETEXT, memory))
                    ::GlobalFree(memory);
                else
                    wrote = true;
            }
            else
            {
                ::GlobalFree(memory);
            }

            ::CloseClipboard();
            return wrote;
        }
#else
        [[nodiscard]] static std::string clipboard_read_text()
        {
            return {};
        }

        [[nodiscard]] static bool clipboard_write_text(std::string_view)
        {
            return false;
        }
#endif

        static void append_text_limited(std::string& text, std::string_view incoming, std::size_t limit, bool multiline, bool& changed)
        {
            for (char ch : incoming)
            {
                if (text.size() >= limit)
                    break;
                if (ch == '\r')
                    continue;
                if (ch == '\n')
                {
                    if (multiline)
                    {
                        text.push_back('\n');
                        changed = true;
                    }
                    else if (!text.empty() && text.back() != ' ')
                    {
                        text.push_back(' ');
                        changed = true;
                    }
                    continue;
                }
                if (static_cast<unsigned char>(ch) < 32u)
                    continue;
                text.push_back(ch);
                changed = true;
            }
        }

        [[nodiscard]] static bool is_utf8_continuation_byte(unsigned char ch) noexcept
        {
            return (ch & 0xC0u) == 0x80u;
        }

        [[nodiscard]] static std::optional<unsigned char> next_drawable_char(std::string_view text, std::size_t index) noexcept
        {
            if (index >= text.size())
                return std::nullopt;

            for (std::size_t i = index + 1; i < text.size(); ++i)
            {
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char next = safe_draw_char(raw);
                if (next == '\n')
                    return std::nullopt;
                return next;
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
            ch = safe_draw_char(ch);
            if (next)
                next = safe_draw_char(*next);

            float advance = glyph_advance(ch, scale);
            if (next)
                advance += kerning_adjust(ch, *next, scale);
            if (ch != ' ' && ch != '\t')
                advance += letter_spacing(scale);
            return advance;
        }

        [[nodiscard]] static bool glyph_rect_is_reasonable(float w, float h, float scale) noexcept
        {
            const float baseHeight = (std::max)(1.0f, base_line_height(scale));
            return std::isfinite(w)
                && std::isfinite(h)
                && w > 0.0f
                && h > 0.0f
                && w <= baseHeight * 8.0f
                && h <= baseHeight * 4.0f;
        }

        [[nodiscard]] static bool glyph_fully_inside_clip(
            float x,
            float y,
            float w,
            float h,
            float clipLeft,
            float clipTop,
            float clipRight,
            float clipBottom) noexcept
        {
            if (!std::isfinite(x)
                || !std::isfinite(y)
                || !std::isfinite(w)
                || !std::isfinite(h)
                || !std::isfinite(clipLeft)
                || !std::isfinite(clipTop)
                || !std::isfinite(clipRight)
                || !std::isfinite(clipBottom)
                || w <= 0.0f
                || h <= 0.0f
                || clipRight <= clipLeft
                || clipBottom <= clipTop)
            {
                return false;
            }

            const float visibleLeft = (std::max)(x, clipLeft);
            const float visibleTop = (std::max)(y, clipTop);
            const float visibleRight = (std::min)(x + w, clipRight);
            const float visibleBottom = (std::min)(y + h, clipBottom);
            const float visibleWidth = visibleRight - visibleLeft;
            const float visibleHeight = visibleBottom - visibleTop;

            return visibleWidth >= 1.0f && visibleHeight >= 1.0f;
        }

        [[nodiscard]] static float measure_text_width(std::string_view text, float scale) noexcept
        {
            float current = 0.0f;
            float maxWidth = 0.0f;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char ch = safe_draw_char(raw);
                if (ch == '\n')
                {
                    maxWidth = (std::max)(maxWidth, current);
                    current = 0.0f;
                    continue;
                }
                const auto next = next_drawable_char(text, i);
                current += glyph_advance_with_kerning(ch, next, scale);
            }
            return (std::max)(maxWidth, current);
        }

        [[nodiscard]] static std::string fit_text_to_width(
            std::string_view text,
            float maxWidth,
            float scale) noexcept
        {
            if (text.empty() || maxWidth <= 0.0f)
                return {};

            if (measure_text_width(text, scale) <= maxWidth)
                return std::string(text);

            constexpr std::string_view kEllipsis = "...";
            const float ellipsisWidth = measure_text_width(kEllipsis, scale);
            if (ellipsisWidth >= maxWidth)
                return std::string(kEllipsis);

            std::string trimmed{};
            trimmed.reserve(text.size());
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const std::string candidate = trimmed + text[i] + std::string(kEllipsis);
                if (measure_text_width(candidate, scale) > maxWidth)
                    break;
                trimmed.push_back(text[i]);
            }

            if (trimmed.empty())
                return std::string(kEllipsis);

            trimmed += kEllipsis;
            return trimmed;
        }

        [[nodiscard]] static bool is_wrap_space(char ch) noexcept
        {
            return ch == ' ' || ch == '\t';
        }

        [[nodiscard]] static float measure_word_advance(std::string_view text, std::size_t start, float scale) noexcept
        {
            float advance = 0.0f;

            for (std::size_t i = start; i < text.size(); ++i)
            {
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char ch = safe_draw_char(raw);
                if (ch == '\n' || is_wrap_space(ch))
                    break;

                const auto next = next_drawable_char(text, i);
                advance += glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
            }

            return advance;
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
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char ch = safe_draw_char(raw);
                if (ch == '\n')
                {
                    ++lines;
                    penX = 0.0f;
                    continue;
                }

                if (is_wrap_space(ch))
                {
                    std::size_t runEnd = i;
                    float whitespaceAdvance = 0.0f;
                    while (runEnd < text.size() && is_wrap_space(text[runEnd]))
                    {
                        const auto next = next_drawable_char(text, runEnd);
                        whitespaceAdvance += glyph_advance_with_kerning(static_cast<unsigned char>(text[runEnd]), next, scale);
                        ++runEnd;
                    }

                    if (penX <= 0.0f)
                    {
                        i = runEnd - 1;
                        continue;
                    }

                    const float nextWordAdvance = measure_word_advance(text, runEnd, scale);
                    if (nextWordAdvance > 0.0f && penX + whitespaceAdvance + nextWordAdvance > effectiveWidth + 0.001f)
                    {
                        ++lines;
                        penX = 0.0f;
                        i = runEnd - 1;
                        continue;
                    }

                    penX += whitespaceAdvance;
                    i = runEnd - 1;
                    continue;
                }

                if ((i == 0 || text[i - 1] == '\n' || is_wrap_space(text[i - 1])) && penX > 0.0f)
                {
                    const float wordAdvance = measure_word_advance(text, i, scale);
                    if (wordAdvance > 0.0f && penX + wordAdvance > effectiveWidth + 0.001f)
                    {
                        ++lines;
                        penX = 0.0f;
                    }
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
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char ch = safe_draw_char(raw);
                if (ch == '\n')
                {
                    penX = x;
                    baseline += lineAdvance;
                    continue;
                }

                if (is_wrap_space(ch))
                {
                    std::size_t runEnd = i;
                    float whitespaceAdvance = 0.0f;
                    while (runEnd < text.size() && is_wrap_space(text[runEnd]))
                    {
                        const auto next = next_drawable_char(text, runEnd);
                        whitespaceAdvance += glyph_advance_with_kerning(static_cast<unsigned char>(text[runEnd]), next, scale);
                        ++runEnd;
                    }

                    if (penX <= x)
                    {
                        i = runEnd - 1;
                        continue;
                    }

                    const float nextWordAdvance = measure_word_advance(text, runEnd, scale);
                    if (nextWordAdvance > 0.0f && penX - x + whitespaceAdvance + nextWordAdvance > effectiveWidth + 0.001f)
                    {
                        penX = x;
                        baseline += lineAdvance;
                        i = runEnd - 1;
                        continue;
                    }

                    penX += whitespaceAdvance;
                    i = runEnd - 1;
                    continue;
                }

                if ((i == 0 || text[i - 1] == '\n' || is_wrap_space(text[i - 1])) && penX > x)
                {
                    const float wordAdvance = measure_word_advance(text, i, scale);
                    if (wordAdvance > 0.0f && penX - x + wordAdvance > effectiveWidth + 0.001f)
                    {
                        penX = x;
                        baseline += lineAdvance;
                    }
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

            const float clipLeft = has_content_clip() ? (g_frame.contentMin.x - kTextClipSlack) : x;
            const float clipTop = has_content_clip() ? (g_frame.contentMin.y - kTextClipSlack) : y;
            const float clipRight = has_content_clip() ? g_frame.contentMax.x : (x + width);
            const float clipBottom = has_content_clip() ? g_frame.contentMax.y : (y + 100000.0f);
            const float effectiveWidth = (std::max)(space_advance(scale), (std::min)(width, clipRight - x));
            const float lineAdvance = line_advance_amount(scale);
            const float ascent = baseline_offset(scale);
            const float baseHeight = base_line_height(scale);

            float penX = x;
            float baseline = y + ascent;
            std::size_t lines = 1;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char ch = safe_draw_char(raw);
                if (ch == '\n')
                {
                    penX = x;
                    baseline += lineAdvance;
                    ++lines;
                    if (baseline - ascent > clipBottom)
                        break;
                    continue;
                }

                if (is_wrap_space(ch))
                {
                    std::size_t runEnd = i;
                    float whitespaceAdvance = 0.0f;
                    while (runEnd < text.size() && is_wrap_space(text[runEnd]))
                    {
                        const auto next = next_drawable_char(text, runEnd);
                        whitespaceAdvance += glyph_advance_with_kerning(static_cast<unsigned char>(text[runEnd]), next, scale);
                        ++runEnd;
                    }

                    if (penX <= x)
                    {
                        i = runEnd - 1;
                        continue;
                    }

                    const float nextWordAdvance = measure_word_advance(text, runEnd, scale);
                    if (nextWordAdvance > 0.0f && penX - x + whitespaceAdvance + nextWordAdvance > effectiveWidth + 0.001f)
                    {
                        penX = x;
                        baseline += lineAdvance;
                        ++lines;
                        if (baseline - ascent > clipBottom)
                            break;
                        i = runEnd - 1;
                        continue;
                    }

                    penX += whitespaceAdvance;
                    i = runEnd - 1;
                    continue;
                }

                if ((i == 0 || text[i - 1] == '\n' || is_wrap_space(text[i - 1])) && penX > x)
                {
                    const float wordAdvance = measure_word_advance(text, i, scale);
                    if (wordAdvance > 0.0f && penX - x + wordAdvance > effectiveWidth + 0.001f)
                    {
                        penX = x;
                        baseline += lineAdvance;
                        ++lines;
                        if (baseline - ascent > clipBottom)
                            break;
                    }
                }

                const auto next = next_drawable_char(text, i);
                const float advance = glyph_advance_with_kerning(static_cast<unsigned char>(ch), next, scale);
                if (penX > x && penX + advance > x + effectiveWidth + 0.001f)
                {
                    penX = x;
                    baseline += lineAdvance;
                    ++lines;
                    if (baseline - ascent > clipBottom)
                        break;
                }

                if (ch != ' ' && ch != '\t')
                {
                    if (const auto* glyph = lookup_glyph(static_cast<unsigned char>(ch)))
                    {
                        const float drawW = glyph->size_px.x * scale;
                        const float drawH = glyph->size_px.y * scale;
                        if (glyph->handle.is_valid() && glyph_rect_is_reasonable(drawW, drawH, scale))
                        {
                            const float offsetX = glyph->offset_px.x * scale;
                            const float offsetY = glyph->offset_px.y * scale;
                            const float drawX = penX + offsetX;
                            const float drawY = baseline + offsetY;
                            if (glyph_fully_inside_clip(drawX, drawY, drawW, drawH, clipLeft, clipTop, clipRight, clipBottom))
                            {
                                draw_sprite(glyph->handle, drawX, drawY, drawW, drawH);
                            }
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

            const bool clipped = has_content_clip();
            const float clipLeft = clipped ? g_frame.contentMin.x : x;
            const float clipTop = clipped ? g_frame.contentMin.y : y;
            const float clipRight = clipped ? g_frame.contentMax.x : (x + measure_text_width(text, scale));
            const float clipBottom = clipped ? g_frame.contentMax.y : (y + line_advance_amount(scale));
            const float anchorX = indent.value_or(x);
            float penX = anchorX;
            float baseline = y + baseline_offset(scale);
            const float lineAdvance = line_advance_amount(scale);

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const unsigned char raw = static_cast<unsigned char>(text[i]);
                if (is_utf8_continuation_byte(raw))
                    continue;
                const unsigned char ch = safe_draw_char(raw);
                if (ch == '\n')
                {
                    penX = anchorX;
                    baseline += lineAdvance;
                    continue;
                }

                if (const auto* glyph = lookup_glyph(ch))
                {
                    if (glyph->handle.is_valid())
                    {
                        const float drawW = glyph->size_px.x * scale;
                        const float drawH = glyph->size_px.y * scale;
                        const float offsetX = glyph->offset_px.x * scale;
                        const float offsetY = glyph->offset_px.y * scale;
                        const float drawX = penX + offsetX;
                        const float drawY = baseline + offsetY;
                        const bool glyphVisible = glyph_fully_inside_clip(
                            drawX,
                            drawY,
                            drawW,
                            drawH,
                            clipLeft,
                            clipTop,
                            clipRight,
                            clipBottom);
                        if (glyphVisible && glyph_rect_is_reasonable(drawW, drawH, scale))
                        {
                            draw_sprite(glyph->handle, drawX, drawY, drawW, drawH);
                        }
                    }
                }

                const auto next = next_drawable_char(text, i);
                penX += glyph_advance_with_kerning(ch, next, scale);
                if (penX > clipRight)
                    return;
            }
        }

        static void draw_caret(float x, float y, float height)
        {
            const float caretWidth = (std::max)(1.0f, space_advance(kFontScale) * 0.08f);
            draw_sprite(active_palette().textFieldActive, x, y, caretWidth, height);
        }

        static void reset_frame()
        {
            g_frame.cursor = {};
            g_frame.origin = {};
            g_frame.windowSize = {};
            g_frame.contentMin = {};
            g_frame.contentMax = {};
            g_frame.windowKey.clear();
            g_frame.widgetSerial = 0;
            g_frame.insideWindow = false;
            g_frame.lastButtonBounds.reset();
            g_frame.activeTheme = ThemeVariant::DefaultDark;
            g_frame.themeStack.clear();
            g_scrollAreaStack.clear();
        }

        static void forget_upload_state(const void* ctxKey) noexcept
        {
            if (!ctxKey)
                return;

            std::scoped_lock lock(g_uploadMutex);
            g_uploadedContexts.erase(ctxKey);
        }

        static void store_top_layer_batch(Context* ctx) noexcept
        {
            if (!ctx)
            {
                g_frame.topLayerDraws.clear();
                return;
            }

            std::scoped_lock lock(g_deferredBatchMutex);
            auto& batch = g_topLayerDrawBatches[ctx];
            if (g_frame.topLayerDraws.empty())
            {
                if (batch.draws && !batch.draws->empty())
                {
                    batch.draws = std::make_shared<std::vector<QueuedSpriteDraw>>();
                    ++batch.generation;
                }
                return;
            }

            auto draws = std::make_shared<std::vector<QueuedSpriteDraw>>();
            draws->swap(g_frame.topLayerDraws);
            const std::size_t reserveCount = (std::max)(draws->size(), std::size_t{ 256 });
            g_frame.topLayerDraws.reserve(reserveCount);
            if (!batch.draws || !same_queued_draws(*batch.draws, *draws))
            {
                batch.draws = std::move(draws);
                ++batch.generation;
            }
        }

        static void flush_queued_draws() noexcept
        {
            auto ctxShared = g_frame.ctxShared;
            Context* ctx = g_frame.ctx;
            if (!ctxShared || !ctx)
            {
                g_frame.queuedDraws.clear();
                g_frame.topLayerDraws.clear();
                return;
            }

            store_top_layer_batch(ctx);

            if (uses_deferred_gui_batch(ctx))
            {
                std::scoped_lock lock(g_deferredBatchMutex);
                auto& batch = g_deferredDrawBatches[ctx];
                if (g_frame.queuedDraws.empty())
                {
                    if (batch.draws && !batch.draws->empty())
                    {
                        batch.draws = std::make_shared<std::vector<QueuedSpriteDraw>>();
                        ++batch.generation;
                    }
                    return;
                }

                auto draws = std::make_shared<std::vector<QueuedSpriteDraw>>();
                draws->swap(g_frame.queuedDraws);
                const std::size_t reserveCount = (std::max)(draws->size(), std::size_t{ 4096 });
                g_frame.queuedDraws.reserve(reserveCount);
                if (!batch.draws || !same_queued_draws(*batch.draws, *draws))
                {
                    batch.draws = std::move(draws);
                    ++batch.generation;
                }
                return;
            }

            if (g_frame.queuedDraws.empty())
                return;

            if (!ctx->windowData)
            {
                auto atlases = epochnamespace::atlasmanager::get_atlas_vector_snapshot();
                std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
                for (const auto& draw : g_frame.queuedDraws)
                    ctxShared->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
                g_frame.queuedDraws.clear();
                return;
            }

            const core::RenderPath renderPath = render_path_for_context(ctx);
            auto draws = std::make_shared<std::vector<QueuedSpriteDraw>>();
            draws->swap(g_frame.queuedDraws);
            const std::size_t reserveCount = (std::max)(draws->size(), std::size_t{ 4096 });
            g_frame.queuedDraws.reserve(reserveCount);

            ctx->windowData->commandQueue.enqueue([ctxShared, draws]()
                {
                    if (!ctxShared || !draws || draws->empty())
                        return;

                    auto atlases = epochnamespace::atlasmanager::get_atlas_vector_snapshot();
                    std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
                    for (const auto& draw : *draws)
                        ctxShared->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
                }, renderPath);
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

    void push_input_for_context(const core::Context* ctx, const InputEvent& e) noexcept
    {
        if (!ctx)
        {
            push_input(e);
            return;
        }

        std::scoped_lock lock(g_contextPendingEventsMutex);
        g_contextPendingEvents[ctx].push_back(e);
    }

    int consume_mouse_wheel_delta() noexcept
    {
        const int delta = g_frame.mouseWheelDelta;
        g_frame.mouseWheelDelta = 0;
        return delta;
    }

    void cleanup_context(const core::Context* ctx) noexcept
    {
        forget_upload_state(ctx);
        {
            std::scoped_lock lock(g_contextPendingEventsMutex);
            g_contextPendingEvents.erase(ctx);
        }
        std::scoped_lock lock(g_deferredBatchMutex);
        g_deferredDrawBatches.erase(ctx);
        g_topLayerDrawBatches.erase(ctx);
        g_contextMouseDownStates.erase(ctx);
        g_contextActiveWidgets.erase(ctx);
        g_contextPressedButtonKeys.erase(ctx);
        const std::string scrollPrefix = std::to_string(reinterpret_cast<std::uintptr_t>(ctx)) + "|";
        for (auto it = g_scrollTextStates.begin(); it != g_scrollTextStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_scrollTextStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_scrollAreaStates.begin(); it != g_scrollAreaStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_scrollAreaStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_selectBoxStates.begin(); it != g_selectBoxStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_selectBoxStates.erase(it);
            else
                ++it;
        }
    }

    std::uint64_t deferred_batch_generation(const core::Context* ctx) noexcept
    {
        if (!ctx || !uses_deferred_gui_batch(ctx))
            return 0;

        std::scoped_lock lock(g_deferredBatchMutex);
        const auto it = g_deferredDrawBatches.find(ctx);
        if (it == g_deferredDrawBatches.end())
            return 0;

        return it->second.generation;
    }

    bool render_deferred_batch(core::Context* ctx) noexcept
    {
        if (!ctx || !uses_deferred_gui_batch(ctx))
            return false;

        std::shared_ptr<std::vector<QueuedSpriteDraw>> draws;
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_deferredDrawBatches.find(ctx);
            if (it == g_deferredDrawBatches.end())
                return false;
            draws = it->second.draws;
        }

        if (!draws || draws->empty())
            return false;

        auto atlases = epochnamespace::atlasmanager::get_atlas_vector_snapshot();
        std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
        for (const auto& draw : *draws)
            ctx->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
        return true;
    }

    bool render_top_layer_batch(core::Context* ctx) noexcept
    {
        if (!ctx)
            return false;

        std::shared_ptr<std::vector<QueuedSpriteDraw>> draws;
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_topLayerDrawBatches.find(ctx);
            if (it == g_topLayerDrawBatches.end())
                return false;
            draws = it->second.draws;
        }

        if (!draws || draws->empty())
            return false;

        auto atlases = epochnamespace::atlasmanager::get_atlas_vector_snapshot();
        std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
        for (const auto& draw : *draws)
            ctx->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
        return true;
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

        g_frame.caretVisible = g_frame.caretTimer < (kCaretBlinkPeriod * 0.5f);

        bool prevMouseDown = g_frame.mouseDown;
        if (rawCtx)
        {
            if (const auto it = g_contextMouseDownStates.find(rawCtx); it != g_contextMouseDownStates.end())
                prevMouseDown = it->second;
        }
        bool currentMouseDown = mouse_down;
        Vec2 currentMousePos = mouse_pos;
        g_frame.mouseWheelDelta = 0;

        g_frame.events.clear();
        if (!g_pendingEvents.empty())
        {
            g_frame.events.insert(g_frame.events.end(), g_pendingEvents.begin(), g_pendingEvents.end());
            g_pendingEvents.clear();
        }

        if (rawCtx)
        {
            std::scoped_lock lock(g_contextPendingEventsMutex);
            if (const auto it = g_contextPendingEvents.find(rawCtx); it != g_contextPendingEvents.end() && !it->second.empty())
            {
                g_frame.events.insert(g_frame.events.end(), it->second.begin(), it->second.end());
                it->second.clear();
            }
        }

        for (const auto& evt : g_frame.events)
        {
            switch (evt.type)
            {
            case EventType::MouseMove: currentMousePos = evt.mouse_pos; break;
            case EventType::MouseDown: currentMouseDown = true;  currentMousePos = evt.mouse_pos; break;
            case EventType::MouseUp:   currentMouseDown = false; currentMousePos = evt.mouse_pos; break;
            case EventType::MouseWheel:
                currentMousePos = evt.mouse_pos;
                g_frame.mouseWheelDelta += evt.wheel_delta;
                break;
            default: break;
            }
        }

        g_frame.prevMouseDown = prevMouseDown;
        g_frame.mouseDown = currentMouseDown;
        g_frame.mousePos = currentMousePos;
        g_frame.justPressed = (!prevMouseDown && currentMouseDown);
        g_frame.justReleased = (prevMouseDown && !currentMouseDown);
        g_frame.queuedDraws.clear();
        g_frame.topLayerDraws.clear();
        g_frame.topLayerDepth = 0;

        if (rawCtx)
        {
            if (g_frame.justPressed)
                g_contextPressedButtonKeys[rawCtx] = 0;
            g_contextMouseDownStates[rawCtx] = currentMouseDown;
        }

        reset_frame();
    }

    void end_frame() noexcept
    {
        flush_queued_draws();
        g_frame.ctxShared.reset();
        g_frame.ctx = nullptr;
        g_frame.insideWindow = false;
        g_frame.events.clear();
        g_frame.justPressed = false;
        g_frame.justReleased = false;
        g_frame.mouseWheelDelta = 0;
        g_frame.topLayerDepth = 0;
    }

    Vec2 mouse_position() noexcept
    {
        return g_frame.mousePos;
    }

    bool is_mouse_down() noexcept
    {
        return g_frame.mouseDown;
    }

    bool was_mouse_pressed() noexcept
    {
        return g_frame.justPressed;
    }

    bool was_mouse_released() noexcept
    {
        return g_frame.justReleased;
    }

    void push_theme(ThemeVariant theme) noexcept
    {
        g_frame.themeStack.push_back(g_frame.activeTheme);
        g_frame.activeTheme = theme;
    }

    void pop_theme() noexcept
    {
        if (!g_frame.themeStack.empty())
        {
            g_frame.activeTheme = g_frame.themeStack.back();
            g_frame.themeStack.pop_back();
        }
        else
        {
            g_frame.activeTheme = ThemeVariant::DefaultDark;
        }
    }

    void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept
    {
        begin_window(title, position, size, true);
    }

    void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background) noexcept
    {
        if (!g_frame.ctx) return;

        try { ensure_resources(); }
        catch (...) { return; }

        g_frame.origin = position;
        g_frame.windowSize = size;
        g_frame.insideWindow = true;
        g_frame.windowKey.assign(title.begin(), title.end());
        g_frame.widgetSerial = 0;
        g_frame.contentMin = position;
        g_frame.contentMax = { position.x + size.x, position.y + size.y };
        const auto& palette = active_palette();

        if (draw_background)
            draw_sprite(palette.windowBackground, position.x, position.y, size.x, size.y);

        const bool hasTitleBar = !title.empty();
        float titleBarHeight = 0.0f;
        if (hasTitleBar)
        {
            const float titleHeight = line_advance_amount(kTitleScale);
            titleBarHeight = titleHeight + 2.0f * kTitleBarPadding;
            const float titleTextY = position.y + (titleBarHeight - titleHeight) * 0.5f;
            draw_sprite(palette.titleBar, position.x, position.y, size.x, titleBarHeight);
            const std::string fittedTitle = fit_text_to_width(
                title,
                (std::max)(1.0f, size.x - 2.0f * kContentPadding),
                kTitleScale);
            draw_text_line(
                fittedTitle.empty() ? title : std::string_view{ fittedTitle },
                position.x + kContentPadding,
                titleTextY,
                kTitleScale);
        }

        const bool fullBleedContent = !draw_background && title.empty();
        g_frame.contentMin = fullBleedContent
            ? position
            : Vec2{
                position.x + kContentPadding,
                position.y + titleBarHeight + kContentPadding
            };
        g_frame.contentMax = fullBleedContent
            ? Vec2{ position.x + (std::max)(0.0f, size.x), position.y + (std::max)(0.0f, size.y) }
            : Vec2{
                position.x + (std::max)(kContentPadding, size.x - kContentPadding),
                position.y + (std::max)(titleBarHeight + kContentPadding, size.y - kContentPadding)
            };

        set_cursor(g_frame.contentMin);
    }

    void end_window() noexcept
    {
        g_frame.insideWindow = false;
        g_frame.contentMin = {};
        g_frame.contentMax = {};
        g_frame.windowKey.clear();
        g_frame.widgetSerial = 0;
    }

    void begin_top_layer() noexcept
    {
        ++g_frame.topLayerDepth;
    }

    void end_top_layer() noexcept
    {
        if (g_frame.topLayerDepth > 0)
            --g_frame.topLayerDepth;
    }

    void begin_modal_window(const ModalWindowOptions& options) noexcept
    {
        if (!g_frame.ctx) return;

        try { ensure_resources(); }
        catch (...) { return; }

        begin_top_layer();

        if (options.dim_background && options.viewport_size.x > 0.0f && options.viewport_size.y > 0.0f)
        {
            const auto& palette = active_palette();
            draw_sprite(palette.modalScrim, 0.0f, 0.0f, options.viewport_size.x, options.viewport_size.y);
        }

        begin_window(options.title, options.position, options.size);
    }

    void end_modal_window() noexcept
    {
        end_window();
        end_top_layer();
    }

    WidgetBounds scene_viewport(std::string_view title, Vec2 position, Vec2 size) noexcept
    {
        WidgetBounds bounds{};
        if (!g_frame.ctx) return bounds;

        try { ensure_resources(); }
        catch (...) { return bounds; }

        const auto& palette = active_palette();
        const float width = (std::max)(0.0f, size.x);
        const float height = (std::max)(0.0f, size.y);
        const float border = 2.0f;

        const bool hasTitleBar = !title.empty();
        const float titleHeight = hasTitleBar ? line_advance_amount(kTitleScale) : 0.0f;
        const float titleBarHeight = hasTitleBar ? (titleHeight + 2.0f * kTitleBarPadding) : 0.0f;
        if (hasTitleBar)
        {
            const float titleTextY = position.y + (titleBarHeight - titleHeight) * 0.5f;
            draw_sprite(palette.titleBar, position.x, position.y, width, titleBarHeight);
            const std::string fittedTitle = fit_text_to_width(
                title,
                (std::max)(1.0f, width - 2.0f * kContentPadding),
                kTitleScale);
            draw_text_line(
                fittedTitle.empty() ? title : std::string_view{ fittedTitle },
                position.x + kContentPadding,
                titleTextY,
                kTitleScale);
        }

        const float topBorderHeight = hasTitleBar ? 0.0f : border;
        const float contentY = position.y + titleBarHeight + topBorderHeight;
        const float contentHeight = (std::max)(0.0f, height - titleBarHeight - topBorderHeight - border);
        const float contentWidth = (std::max)(0.0f, width - border * 2.0f);

        const core::RenderPath renderPath = render_path_for_context(g_frame.ctx);
        const bool needsViewportFallback = !backend_owns_scene_viewport(g_frame.ctx);

        if (needsViewportFallback && contentWidth > 0.0f && contentHeight > 0.0f)
        {
            draw_sprite(palette.consoleBackground, position.x + border, contentY, contentWidth, contentHeight);

            const float inset = 18.0f;
            const float cardX = position.x + border + inset;
            const float cardY = contentY + inset;
            const float cardW = (std::max)(48.0f, contentWidth - inset * 2.0f);
            const float cardH = (std::max)(48.0f, contentHeight - inset * 2.0f);
            draw_sprite(palette.panelBackground, cardX, cardY, cardW, cardH);

            const float previewScale = 1.15f;
            const float lineHeight = line_advance_amount(previewScale);
            draw_text_line(std::string(viewport_fallback_name(g_frame.ctx)) + " Preview", cardX + 16.0f, cardY + 16.0f, previewScale);
            draw_text_line("Scene output pending backend pass", cardX + 16.0f, cardY + 16.0f + lineHeight + 10.0f, kFontScale);
            draw_text_line("GUI remains live in this viewport", cardX + 16.0f, cardY + 16.0f + lineHeight * 2.0f + 18.0f, kFontScale);
        }

        if (contentHeight > 0.0f)
        {
            draw_sprite(palette.panelBackground, position.x, contentY, border, contentHeight);
            draw_sprite(palette.panelBackground,
                position.x + (std::max)(0.0f, width - border),
                contentY,
                border,
                contentHeight);
        }
        if (height > border)
        {
            if (!hasTitleBar)
            {
                draw_sprite(palette.panelBackground,
                    position.x,
                    position.y,
                    width,
                    border);
            }
            draw_sprite(palette.panelBackground,
                position.x,
                position.y + (std::max)(0.0f, height - border),
                width,
                border);
        }

        bounds.position = { position.x + border, contentY };
        bounds.size = { contentWidth, contentHeight };
        return bounds;
    }

    void splitter_bar(Vec2 position, Vec2 size, bool hovered, bool active) noexcept
    {
        ensure_resources();
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        const auto& palette = active_palette();
        draw_sprite(palette.windowBackground, position.x, position.y, size.x, size.y);
        const SpriteHandle accent =
            active ? palette.buttonActive
            : hovered ? palette.textFieldActive
            : palette.titleBar;

        const bool vertical = size.y >= size.x;
        if (vertical && size.x >= 5.0f)
        {
            const float x = position.x + std::floor(size.x * 0.5f);
            draw_sprite(accent, x, position.y, 1.0f, size.y);
        }
        else if (!vertical && size.y >= 5.0f)
        {
            const float y = position.y + std::floor(size.y * 0.5f);
            draw_sprite(accent, position.x, y, size.x, 1.0f);
        }
    }
    static bool button_with_state(std::string_view label, Vec2 size, bool selected) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kBoxInnerPadding);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && g_frame.justPressed)
            pressedKey = pressKey;

        const bool pressed = g_frame.mouseDown && pressedKey == pressKey;
        const bool clicked = g_frame.justReleased && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
            pressedKey = 0;

        const auto& palette = active_palette();

        const SpriteHandle background =
            selected ? palette.buttonActive
            : hovered ? palette.buttonHover
            : palette.buttonNormal;

        draw_sprite(background, pos.x, pos.y, width, height);
        if (hovered || pressed || selected)
        {
            const SpriteHandle accent = selected
                ? palette.textFieldActive
                : palette.buttonHover;
            draw_sprite(accent, pos.x, pos.y, width, 2.0f);
            draw_sprite(accent, pos.x, pos.y, 2.0f, height);
        }

        const std::string fittedLabel = fit_text_to_width(
            label,
            (std::max)(1.0f, width - 2.0f * kContentPadding - 2.0f),
            kFontScale);
        const std::string_view displayLabel = fittedLabel.empty()
            ? label
            : std::string_view{ fittedLabel };
        const float textWidth = measure_text_width(displayLabel, kFontScale) + 2.0f;
        const float textHeight = baseHeight;
        const float minTextX = pos.x + kContentPadding + kButtonTextClipInset;
        const float maxTextX = pos.x + width - kContentPadding - textWidth;
        const float centeredTextX = pos.x + (std::max)(0.0f, (width - textWidth) * 0.5f);
        const float textX = maxTextX > minTextX
            ? (std::clamp)(centeredTextX, minTextX, maxTextX)
            : minTextX;
        const float textY =
            pos.y + std::floor((std::max)(0.0f, (height - textHeight) * 0.5f)) + 1.0f;

        draw_text_line(displayLabel, textX, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });

        return clicked;
    }

    bool button(std::string_view label, Vec2 size) noexcept
    {
        return button_with_state(label, size, false);
    }

    bool button_selected(std::string_view label, Vec2 size, bool selected) noexcept
    {
        return button_with_state(label, size, selected);
    }

    bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return false;

        const Vec2 pos = g_frame.cursor;
        const float width = (std::max)(static_cast<float>(size.x), 1.0f);
        const float height = (std::max)(static_cast<float>(size.y), 1.0f);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key("<image-button>", pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && g_frame.justPressed)
            pressedKey = pressKey;

        const bool pressed = g_frame.mouseDown && pressedKey == pressKey;
        const bool clicked = g_frame.justReleased && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
            pressedKey = 0;

        const auto& palette = active_palette();

        const SpriteHandle background =
            hovered ? palette.buttonHover
            : palette.buttonNormal;

        draw_sprite(background, pos.x, pos.y, width, height);

        const float inset = (std::min)(kBoxInnerPadding, (std::min)(width, height) * 0.2f);
        draw_sprite(sprite, pos.x + inset, pos.y + inset, (std::max)(1.0f, width - 2.0f * inset), (std::max)(1.0f, height - 2.0f * inset));

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });

        return clicked;
    }

    void image(const SpriteHandle& sprite, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        const Vec2 pos = g_frame.cursor;
        const float width = (std::max)(static_cast<float>(size.x), 1.0f);
        const float height = (std::max)(static_cast<float>(size.y), 1.0f);

        draw_sprite(sprite, pos.x, pos.y, width, height);
        advance_cursor({ 0.0f, height + kContentPadding });
    }

    SpriteHandle register_runtime_surface(
        std::string_view id,
        std::span<const std::uint8_t> rgba_pixels,
        std::uint32_t width,
        std::uint32_t height) noexcept
    {
        if (id.empty() || rgba_pixels.empty() || width == 0 || height == 0)
            return {};

        try
        {
            ensure_resources();
        }
        catch (...)
        {
            return {};
        }

        std::lock_guard cacheLock(g_runtimeSurfaceCacheMutex);

        const std::string key{ id };
        const std::uint64_t contentHash = hash_surface_pixels(rgba_pixels, width, height);
        auto& entry = g_runtimeSurfaceCache[key];

        if (entry.handle.is_valid()
            && entry.contentHash == contentHash
            && entry.width == width
            && entry.height == height)
        {
            return entry.handle;
        }

        try
        {
            std::vector<std::uint8_t> pixels(rgba_pixels.begin(), rgba_pixels.end());
            std::string spriteName = "__agui/runtime_surface/" + key + "/" + std::to_string(entry.version + 1u);

            SpriteHandle handle{};
            {
                std::scoped_lock resourceLock(g_resourceMutex);
                TextureAtlas* runtimeAtlas = ensure_runtime_surface_atlas_locked();
                if (!runtimeAtlas)
                    return {};

                handle = try_add_sprite(*runtimeAtlas, spriteName, pixels, width, height, true);
            }

            if (!handle.is_valid())
                return {};

            entry.handle = handle;
            entry.contentHash = contentHash;
            entry.width = width;
            entry.height = height;
            entry.version += 1u;
            return entry.handle;
        }
        catch (...)
        {
            return {};
        }
    }

    std::optional<WidgetBounds> last_button_bounds() noexcept
    {
        return g_frame.lastButtonBounds;
    }

    Vec2 cursor_position() noexcept
    {
        return g_frame.cursor;
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

    float titled_window_total_height(float content_height) noexcept
    {
        try { ensure_resources(); }
        catch (...) { return content_height + 48.0f; }

        const float titleHeight = line_advance_amount(kTitleScale);
        const float titleBarHeight = titleHeight + 2.0f * kTitleBarPadding;
        return titleBarHeight + 2.0f * kContentPadding + (std::max)(0.0f, content_height);
    }

    void label(std::string_view text) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        draw_text_line(text, g_frame.cursor.x, g_frame.cursor.y, kFontScale);
        advance_cursor({ 0.0f, line_advance_amount(kFontScale) });
    }

    void wrapped_label(std::string_view text, float width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float wrapWidth = width > 0.0f
            ? (std::max)(space_advance(kFontScale), width)
            : availableWidth;

        const float drawnHeight = draw_wrapped_text(
            text,
            g_frame.cursor.x,
            g_frame.cursor.y,
            wrapWidth,
            kFontScale);
        advance_cursor({ 0.0f, drawnHeight });
    }

    void property_row(std::string_view labelText, std::string_view valueText, float label_width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float minReadableValueWidth = space_advance(kFontScale) * 14.0f;
        const float labelWidth = std::clamp(
            label_width,
            space_advance(kFontScale) * 6.0f,
            (std::max)(space_advance(kFontScale) * 6.0f, availableWidth * 0.55f));
        const float gap = 10.0f;
        const float valueX = pos.x + labelWidth + gap;
        const float valueWidth = (std::max)(space_advance(kFontScale), availableWidth - labelWidth - gap);

        if (valueWidth < minReadableValueWidth)
        {
            const std::string clippedLabel = fit_text_to_width(labelText, availableWidth, kFontScale);
            draw_text_line(clippedLabel, pos.x, pos.y, kFontScale);

            const float nextLineY = pos.y + line_advance_amount(kFontScale);
            const float valueHeight = valueText.empty()
                ? line_advance_amount(kFontScale)
                : draw_wrapped_text(valueText, pos.x, nextLineY, availableWidth, kFontScale);

            advance_cursor({
                0.0f,
                line_advance_amount(kFontScale) + (std::max)(line_advance_amount(kFontScale), valueHeight)
            });
            return;
        }

        const std::string clippedLabel = fit_text_to_width(labelText, labelWidth, kFontScale);
        draw_text_line(clippedLabel, pos.x, pos.y, kFontScale);

        const float valueHeight = valueText.empty()
            ? line_advance_amount(kFontScale)
            : draw_wrapped_text(valueText, valueX, pos.y, valueWidth, kFontScale);

        advance_cursor({ 0.0f, (std::max)(line_advance_amount(kFontScale), valueHeight) });
    }

    float wrapped_text_height(std::string_view text, float width) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return 0.0f;

        const float availableWidth = content_available_width(g_frame.cursor.x);
        const float wrapWidth = width > 0.0f
            ? (std::max)(space_advance(kFontScale), width)
            : availableWidth;

        return measure_wrapped_text_height(text, wrapWidth, kFontScale);
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

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const void* id = static_cast<const void*>(&text);
        const void* ctxKey = static_cast<const void*>(g_frame.ctx);
        const void* activeWidget = ctxKey ? g_contextActiveWidgets[ctxKey] : nullptr;
        const auto& palette = active_palette();

        if (g_frame.justPressed)
        {
            if (hovered)
            {
                activeWidget = id;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
            }
            else if (activeWidget == id)
            {
                activeWidget = nullptr;
            }
        }

        if (ctxKey)
            g_contextActiveWidgets[ctxKey] = activeWidget;

        const bool active = (activeWidget == id);
        result.active = active;

        const SpriteHandle background = active ? palette.textFieldActive : palette.textField;
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
                    append_text_limited(text, evt.text, limit, multiline, result.changed);
                    break;

                case EventType::KeyDown:
                    if (evt.ctrl_down && (evt.key == 'C' || evt.key == 'c'))
                    {
                        (void)clipboard_write_text(text);
                    }
                    else if (evt.ctrl_down && (evt.key == 'X' || evt.key == 'x'))
                    {
                        (void)clipboard_write_text(text);
                        if (!text.empty())
                        {
                            text.clear();
                            result.changed = true;
                        }
                    }
                    else if (evt.ctrl_down && (evt.key == 'V' || evt.key == 'v'))
                    {
                        append_text_limited(text, clipboard_read_text(), limit, multiline, result.changed);
                    }
                    else if (evt.ctrl_down && (evt.key == 'A' || evt.key == 'a'))
                    {
                        // Selection ranges are a follow-up; copy/cut operate on the whole focused field for now.
                        (void)clipboard_write_text(text);
                    }
                    else if (evt.key == 8 || evt.key == 127) // backspace/del-ish
                    {
                        if (!text.empty()) { text.pop_back(); result.changed = true; }
                    }
                    else if (evt.key == 27) // ESC
                    {
                        activeWidget = nullptr;
                        if (ctxKey)
                            g_contextActiveWidgets[ctxKey] = nullptr;
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

    std::string clipboard_text() noexcept
    {
        return clipboard_read_text();
    }

    bool set_clipboard_text(std::string_view text) noexcept
    {
        return clipboard_write_text(text);
    }

    std::optional<std::size_t> segmented_button_row(
        std::span<const SegmentedButtonSpec> items,
        float height,
        float gap) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || items.empty())
            return std::nullopt;

        const Vec2 rowStart = g_frame.cursor;
        std::optional<std::size_t> clicked{};
        float x = rowStart.x;

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            set_cursor({ x, rowStart.y });
            if (button_with_state(item.label, { item.width, height }, item.active))
                clicked = i;
            x += (std::max)(1.0f, item.width) + gap;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, (std::max)(1.0f, height) + kContentPadding });
        return clicked;
    }

    std::optional<std::size_t> tab_bar(
        std::span<const SegmentedButtonSpec> tabs,
        float height,
        float gap) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || tabs.empty())
            return std::nullopt;

        const Vec2 rowStart = g_frame.cursor;
        const auto& palette = active_palette();
        std::optional<std::size_t> clicked{};
        float x = rowStart.x;
        float totalWidth = 0.0f;

        for (const auto& tab : tabs)
            totalWidth += (std::max)(1.0f, tab.width) + gap;
        totalWidth = (std::max)(0.0f, totalWidth - gap);

        const float h = (std::max)(height, base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
        draw_sprite(palette.titleBar,
            rowStart.x,
            rowStart.y + h - 2.0f,
            totalWidth,
            2.0f);

        for (std::size_t i = 0; i < tabs.size(); ++i)
        {
            const auto& tab = tabs[i];
            const float width = (std::max)(1.0f, tab.width);
            const bool hovered = point_in_rect(g_frame.mousePos, x, rowStart.y, width, h)
                && point_in_active_clip(g_frame.mousePos);
            const std::size_t pressKey = widget_press_key(tab.label, { x, rowStart.y }, { width, h });
            auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
            if (hovered && g_frame.justPressed)
                pressedKey = pressKey;

            const bool pressed = g_frame.mouseDown && pressedKey == pressKey;
            if (g_frame.justReleased && hovered && pressedKey == pressKey)
                clicked = i;
            if (g_frame.justReleased && pressedKey == pressKey)
                pressedKey = 0;

            const SpriteHandle background =
                tab.active ? palette.panelBackground
                : hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_sprite(background, x, rowStart.y, width, h);
            draw_sprite(tab.active ? palette.textFieldActive : palette.titleBar, x, rowStart.y, width, tab.active ? 3.0f : 1.0f);
            draw_sprite(palette.titleBar, x, rowStart.y, 1.0f, h);
            draw_sprite(palette.titleBar, x + width - 1.0f, rowStart.y, 1.0f, h);

            if (tab.active)
            {
                draw_sprite(palette.panelBackground, x, rowStart.y + h - 2.0f, width, 3.0f);
                draw_sprite(palette.textFieldActive, x, rowStart.y + h - 3.0f, width, 2.0f);
            }

            const std::string fittedLabel = fit_text_to_width(
                tab.label,
                (std::max)(1.0f, width - 2.0f * kContentPadding - 2.0f),
                kFontScale);
            const std::string_view displayLabel = fittedLabel.empty()
                ? tab.label
                : std::string_view{ fittedLabel };
            const float textWidth = measure_text_width(displayLabel, kFontScale) + 2.0f;
            const float textHeight = base_line_height(kFontScale);
            const float minTextX = x + kContentPadding + kButtonTextClipInset;
            const float maxTextX = x + width - kContentPadding - textWidth;
            const float centeredTextX = x + (std::max)(0.0f, (width - textWidth) * 0.5f);
            const float textX = maxTextX > minTextX
                ? (std::clamp)(centeredTextX, minTextX, maxTextX)
                : minTextX;
            const float textY = rowStart.y + std::floor((std::max)(0.0f, (h - textHeight) * 0.5f)) + 1.0f;
            draw_text_line(displayLabel, textX, textY, kFontScale);

            x += width + gap;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, h + kContentPadding });
        return clicked;
    }

    std::optional<std::size_t> inline_button_row(
        std::span<const InlineButtonSpec> items,
        float height,
        float gap) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || items.empty())
            return std::nullopt;

        const Vec2 rowStart = g_frame.cursor;
        std::optional<std::size_t> clicked{};
        float x = rowStart.x;

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            set_cursor({ x, rowStart.y });
            if (button(item.label, { item.width, height }))
                clicked = i;
            x += (std::max)(1.0f, item.width) + gap;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, (std::max)(1.0f, height) + kContentPadding });
        return clicked;
    }

    SelectBoxResult select_box(const SelectBoxOptions& options) noexcept
    {
        SelectBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 start = g_frame.cursor;
        const float availableWidth = content_available_width(start.x);
        const float width = options.size.x > 0.0f
            ? (std::max)(96.0f, options.size.x)
            : availableWidth;
        const float rowHeight = (std::max)(22.0f, options.row_height);
        const float closedHeight = options.size.y > 0.0f
            ? (std::max)(rowHeight, options.size.y)
            : rowHeight;
        const std::string key = widget_state_key(
            options.id.empty() ? std::string_view{ "__select_box" } : options.id);
        auto& state = g_selectBoxStates[key];

        std::string selectedLabel = options.selected.empty()
            ? std::string(options.placeholder)
            : std::string(options.selected);
        selectedLabel += state.open ? "  ^" : "  v";

        if (button(selectedLabel, { width, closedHeight }))
        {
            const bool nextOpen = !state.open;
            for (auto& [otherKey, otherState] : g_selectBoxStates)
            {
                (void)otherKey;
                otherState.open = false;
            }
            state.open = nextOpen;
        }

        result.opened = state.open;
        if (!state.open || options.options.empty())
            return result;

        const std::size_t visibleCount = (std::max)(
            std::size_t{ 1 },
            (std::min)(options.options.size(), options.max_visible_options == 0
                ? options.options.size()
                : options.max_visible_options));
        const float listHeight = (std::max)(rowHeight, static_cast<float>(visibleCount) * rowHeight + 2.0f);
        const float contentHeight = static_cast<float>(options.options.size()) * (rowHeight + 2.0f);
        const std::string listId = key + "-list";

        (void)begin_scroll_area(ScrollAreaOptions{
            .id = listId,
            .size = { width, listHeight },
            .content_height = contentHeight,
            .draw_background = true,
            .show_scrollbar = options.options.size() > visibleCount
        });

        const auto& palette = active_palette();
        const float itemWidth = (std::max)(64.0f, width - 14.0f);
        for (std::size_t i = 0; i < options.options.size(); ++i)
        {
            const Vec2 optionPos = g_frame.cursor;
            const bool active = options.options[i] == options.selected;
            const bool hovered = point_in_rect(g_frame.mousePos, optionPos.x, optionPos.y, itemWidth, rowHeight)
                && point_in_active_clip(g_frame.mousePos);
            const std::size_t pressKey = widget_press_key(options.options[i], optionPos, { itemWidth, rowHeight });
            auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
            if (hovered && g_frame.justPressed)
                pressedKey = pressKey;

            const bool pressed = g_frame.mouseDown && pressedKey == pressKey;
            const bool clicked = g_frame.justReleased && hovered && pressedKey == pressKey;
            if (g_frame.justReleased && pressedKey == pressKey)
                pressedKey = 0;

            const SpriteHandle background =
                active ? palette.buttonActive
                : hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_sprite(background, optionPos.x, optionPos.y, itemWidth, rowHeight);
            if (active || hovered || pressed)
            {
                const SpriteHandle accent = active ? palette.textFieldActive : palette.buttonHover;
                draw_sprite(accent, optionPos.x, optionPos.y, 2.0f, rowHeight);
                draw_sprite(accent, optionPos.x, optionPos.y, itemWidth, 2.0f);
            }

            const std::string fitted = fit_text_to_width(
                options.options[i],
                (std::max)(1.0f, itemWidth - 20.0f),
                kFontScale);
            const std::string_view displayLabel = fitted.empty()
                ? std::string_view{ options.options[i] }
                : std::string_view{ fitted };
            const float textY = optionPos.y + std::floor((std::max)(0.0f, (rowHeight - base_line_height(kFontScale)) * 0.5f)) + 1.0f;
            draw_text_line(displayLabel, optionPos.x + kContentPadding, textY, kFontScale);

            g_frame.lastButtonBounds = WidgetBounds{ .position = optionPos, .size = { itemWidth, rowHeight } };
            advance_cursor({ 0.0f, rowHeight + 2.0f });

            if (clicked)
            {
                result.changed = true;
                result.selected_index = i;
                state.open = false;
            }
        }

        end_scroll_area();
        return result;
    }

    void progress_bar(const ProgressBarOptions& options) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = content_available_width(pos.x);
        const float requestedWidth = options.size.x > 0.0f ? options.size.x : availableWidth;
        const float width = std::clamp(
            requestedWidth,
            1.0f,
            (std::max)(1.0f, availableWidth));
        const float height = (std::max)(14.0f, options.size.y > 0.0f ? options.size.y : 18.0f);
        const float value = std::clamp(options.value, 0.0f, 1.0f);
        const auto& palette = active_palette();

        draw_sprite(palette.panelBackground, pos.x, pos.y, width, height);
        draw_sprite(palette.consoleBackground, pos.x + 2.0f, pos.y + 2.0f, (std::max)(1.0f, width - 4.0f), (std::max)(1.0f, height - 4.0f));

        const float fillWidth = std::floor((std::max)(0.0f, width - 4.0f) * value);
        if (fillWidth > 0.0f)
            draw_sprite(palette.textFieldActive, pos.x + 2.0f, pos.y + 2.0f, fillWidth, (std::max)(1.0f, height - 4.0f));

        draw_sprite(palette.buttonHover, pos.x, pos.y, width, 1.0f);
        draw_sprite(palette.buttonHover, pos.x, pos.y + height - 1.0f, width, 1.0f);
        draw_sprite(palette.buttonHover, pos.x, pos.y, 1.0f, height);
        draw_sprite(palette.buttonHover, pos.x + width - 1.0f, pos.y, 1.0f, height);

        std::string labelText;
        if (!options.label.empty())
            labelText = std::string(options.label);
        if (!options.status.empty())
        {
            if (!labelText.empty())
                labelText += " - ";
            labelText += options.status;
        }
        if (options.show_percent)
        {
            if (!labelText.empty())
                labelText += " ";
            labelText += std::to_string(static_cast<int>(std::round(value * 100.0f)));
            labelText += "%";
        }

        if (!labelText.empty())
        {
            const std::string fitted = fit_text_to_width(
                labelText,
                (std::max)(1.0f, width - 2.0f * kContentPadding),
                kFontScale);
            const std::string_view displayLabel = fitted.empty()
                ? std::string_view{ labelText }
                : std::string_view{ fitted };
            const float textY = pos.y + std::floor((std::max)(0.0f, (height - base_line_height(kFontScale)) * 0.5f)) + 1.0f;
            draw_text_line(displayLabel, pos.x + kContentPadding, textY, kFontScale);
        }

        advance_cursor({ 0.0f, height + kContentPadding });
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

        draw_sprite(active_palette().panelBackground, pos.x, pos.y, width, height);
        draw_wrapped_text(text, pos.x + kBoxInnerPadding, pos.y + kBoxInnerPadding, contentWidth, kFontScale);

        advance_cursor({ 0.0f, height + kContentPadding });
    }

    ScrollAreaResult begin_scroll_area(const ScrollAreaOptions& options) noexcept
    {
        ScrollAreaResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y))
            return result;

        const float availableWidth = content_available_width(pos.x);
        const float width = options.size.x > 0.0f
            ? (std::max)(48.0f, options.size.x)
            : availableWidth;
        const float height = options.size.y > 0.0f
            ? (std::max)(48.0f, options.size.y)
            : 180.0f;
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
            return result;

        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(g_frame.ctx)) + ":scroll-area"
            : std::string(options.id);
        auto& state = g_scrollAreaStates[scroll_panel_key(id)];

        float estimatedContentHeight = (std::max)(
            height,
            options.content_height > 0.0f ? options.content_height : state.contentHeight);
        if (!std::isfinite(estimatedContentHeight))
        {
            state.contentHeight = height;
            estimatedContentHeight = height;
        }
        const float maxScroll = (std::max)(0.0f, estimatedContentHeight - height);
        if (!std::isfinite(state.scrollY))
            state.scrollY = 0.0f;
        state.scrollY = (std::clamp)(state.scrollY, 0.0f, maxScroll);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const bool selectList = is_select_box_list_id(id);
        const bool selectBoxCapturesWheel = any_select_box_open() && !selectList;
        if (hovered && g_frame.mouseWheelDelta != 0 && !selectBoxCapturesWheel)
        {
            const float wheelSteps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            const float step = line_advance_amount(kFontScale) * 3.0f;
            state.scrollY = (std::clamp)(state.scrollY - wheelSteps * step, 0.0f, maxScroll);
            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
        }

        const auto& palette = active_palette();
        if (options.draw_background)
            draw_sprite(palette.consoleBackground, pos.x, pos.y, width, height);

        const float scrollbarWidth = (options.show_scrollbar && estimatedContentHeight > height + 1.0f)
            ? 10.0f
            : 0.0f;
        const float contentWidth = (std::max)(1.0f, width - scrollbarWidth - 2.0f);

        if (scrollbarWidth > 0.0f)
        {
            const float trackX = pos.x + width - scrollbarWidth;
            const float trackY = pos.y;
            const float visibleRatio = height / estimatedContentHeight;
            const float thumbHeight = (std::min)(height, (std::max)(18.0f, height * visibleRatio));
            const float scrollRatio = maxScroll > 0.0f ? state.scrollY / maxScroll : 0.0f;
            const float thumbY = trackY + (height - thumbHeight) * scrollRatio;
            const bool trackHovered = point_in_rect(g_frame.mousePos, trackX, trackY, scrollbarWidth, height)
                && point_in_active_clip(g_frame.mousePos);
            const bool thumbHovered = point_in_rect(g_frame.mousePos, trackX, thumbY, scrollbarWidth, thumbHeight)
                && point_in_active_clip(g_frame.mousePos);

            if (g_frame.justPressed && trackHovered)
            {
                state.draggingScrollbar = true;
                state.dragGrabOffset = thumbHovered
                    ? (g_frame.mousePos.y - thumbY)
                    : (thumbHeight * 0.5f);
            }

            if (!g_frame.mouseDown)
                state.draggingScrollbar = false;

            if (state.draggingScrollbar)
            {
                const float travel = (std::max)(1.0f, height - thumbHeight);
                const float requested = (g_frame.mousePos.y - trackY - state.dragGrabOffset) / travel;
                state.scrollY = (std::clamp)(requested, 0.0f, 1.0f) * maxScroll;
            }
        }

        g_scrollAreaStack.push_back(ScrollAreaFrame{
            .key = scroll_panel_key(id),
            .previousCursor = g_frame.cursor,
            .previousMin = g_frame.contentMin,
            .previousMax = g_frame.contentMax,
            .viewportMin = pos,
            .viewportMax = { pos.x + width, pos.y + height },
            .viewportHeight = height,
            .viewportWidth = width,
            .scrollY = state.scrollY,
            .showScrollbar = options.show_scrollbar
        });

        g_frame.contentMin = {
            (std::max)(g_frame.contentMin.x, pos.x),
            (std::max)(g_frame.contentMin.y, pos.y)
        };
        g_frame.contentMax = {
            (std::min)(g_frame.contentMax.x, pos.x + contentWidth),
            (std::min)(g_frame.contentMax.y, pos.y + height)
        };
        g_frame.cursor = { pos.x, pos.y - state.scrollY };

        result.scroll_y = state.scrollY;
        result.content_height = estimatedContentHeight;
        return result;
    }

    void end_scroll_area() noexcept
    {
        if (!g_frame.insideWindow || g_scrollAreaStack.empty())
            return;

        const ScrollAreaFrame frame = g_scrollAreaStack.back();
        g_scrollAreaStack.pop_back();

        auto& state = g_scrollAreaStates[frame.key];
        const float drawnContentHeight = (std::max)(
            frame.viewportHeight,
            (g_frame.cursor.y + frame.scrollY) - frame.viewportMin.y);
        state.contentHeight = std::isfinite(drawnContentHeight)
            ? drawnContentHeight
            : frame.viewportHeight;
        const float maxScroll = (std::max)(0.0f, state.contentHeight - frame.viewportHeight);
        if (!std::isfinite(state.scrollY))
            state.scrollY = 0.0f;
        state.scrollY = (std::clamp)(state.scrollY, 0.0f, maxScroll);

        g_frame.contentMin = frame.previousMin;
        g_frame.contentMax = frame.previousMax;
        g_frame.cursor = {
            frame.previousCursor.x,
            frame.viewportMax.y + kContentPadding
        };

        if (frame.showScrollbar && state.contentHeight > frame.viewportHeight + 1.0f)
        {
            const auto& palette = active_palette();
            const float scrollbarWidth = 10.0f;
            const float trackX = frame.viewportMax.x - scrollbarWidth;
            const float trackY = frame.viewportMin.y;
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, frame.viewportHeight);

            const float visibleRatio = frame.viewportHeight / state.contentHeight;
            const float thumbHeight = (std::min)(
                frame.viewportHeight,
                (std::max)(18.0f, frame.viewportHeight * visibleRatio));
            const float scrollRatio = maxScroll > 0.0f ? state.scrollY / maxScroll : 0.0f;
            const float thumbY = trackY + (frame.viewportHeight - thumbHeight) * scrollRatio;
            draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
        }
    }

    ScrollTextPanelResult scroll_text_panel(const ScrollTextPanelOptions& options) noexcept
    {
        ScrollTextPanelResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = content_available_width(pos.x);
        const float width = options.size.x > 0.0f
            ? (std::max)(64.0f, options.size.x)
            : availableWidth;
        const float height = options.size.y > 0.0f
            ? (std::max)(48.0f, options.size.y)
            : 160.0f;
        const auto& palette = active_palette();

        draw_sprite(palette.consoleBackground, pos.x, pos.y, width, height);

        const float scrollbarWidth = options.lines.size() > 1 ? 10.0f : 0.0f;
        const float contentX = pos.x + kBoxInnerPadding;
        const float contentY = pos.y + kBoxInnerPadding;
        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding - scrollbarWidth);
        const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
        const float linePitch = line_advance_amount(kFontScale);
        const std::size_t visibleLines = (std::max)(std::size_t{ 1 },
            static_cast<std::size_t>(std::floor(contentHeight / (std::max)(1.0f, linePitch))));

        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(options.lines.data()))
            : std::string(options.id);
        auto& state = g_scrollTextStates[scroll_panel_key(id)];

        const std::size_t lineCount = options.lines.size();
        const std::size_t maxFirstLine = lineCount > visibleLines ? lineCount - visibleLines : 0;
        const bool wasAtBottom = state.lastLineCount == 0
            || state.firstLine + visibleLines >= state.lastLineCount;
        if (options.stick_to_bottom && lineCount != state.lastLineCount && wasAtBottom)
            state.firstLine = maxFirstLine;
        else
            state.firstLine = (std::min)(state.firstLine, maxFirstLine);
        state.lastLineCount = lineCount;

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height);
        if (hovered && g_frame.mouseWheelDelta != 0)
        {
            const int wheelSteps = (std::max)(1, std::abs(g_frame.mouseWheelDelta) / 120);
            if (g_frame.mouseWheelDelta > 0)
                state.firstLine = state.firstLine > static_cast<std::size_t>(wheelSteps)
                    ? state.firstLine - static_cast<std::size_t>(wheelSteps)
                    : 0;
            else
                state.firstLine = (std::min)(maxFirstLine, state.firstLine + static_cast<std::size_t>(wheelSteps));

            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
        }

        if (scrollbarWidth > 0.0f && lineCount > visibleLines)
        {
            const float trackX = pos.x + width - kBoxInnerPadding - scrollbarWidth;
            const float trackY = contentY;
            const float visibleRatio = static_cast<float>(visibleLines) / static_cast<float>(lineCount);
            const float thumbHeight = (std::min)(contentHeight, (std::max)(18.0f, contentHeight * visibleRatio));
            const float scrollRatio = maxFirstLine > 0
                ? static_cast<float>(state.firstLine) / static_cast<float>(maxFirstLine)
                : 0.0f;
            const float thumbY = trackY + (contentHeight - thumbHeight) * scrollRatio;
            const bool trackHovered = point_in_rect(g_frame.mousePos, trackX, trackY, scrollbarWidth, contentHeight)
                && point_in_active_clip(g_frame.mousePos);
            const bool thumbHovered = point_in_rect(g_frame.mousePos, trackX, thumbY, scrollbarWidth, thumbHeight)
                && point_in_active_clip(g_frame.mousePos);

            if (g_frame.justPressed && trackHovered)
            {
                state.draggingScrollbar = true;
                state.dragGrabOffset = thumbHovered
                    ? (g_frame.mousePos.y - thumbY)
                    : (thumbHeight * 0.5f);
            }

            if (!g_frame.mouseDown)
                state.draggingScrollbar = false;

            if (state.draggingScrollbar)
            {
                const float travel = (std::max)(1.0f, contentHeight - thumbHeight);
                const float requested = (g_frame.mousePos.y - trackY - state.dragGrabOffset) / travel;
                state.firstLine = static_cast<std::size_t>(std::round((std::clamp)(requested, 0.0f, 1.0f) * static_cast<float>(maxFirstLine)));
                state.firstLine = (std::min)(state.firstLine, maxFirstLine);
                result.first_visible_line = state.firstLine;
            }
        }

        result.first_visible_line = state.firstLine;
        {
            ContentClipScope clip{
                { contentX, contentY },
                { contentX + contentWidth, contentY + contentHeight }
            };

            const std::size_t endLine = (std::min)(lineCount, state.firstLine + visibleLines);
            for (std::size_t lineIndex = state.firstLine; lineIndex < endLine; ++lineIndex)
            {
                const float rowY = contentY + static_cast<float>(lineIndex - state.firstLine) * linePitch;
                const bool lineHovered = hovered && point_in_rect(g_frame.mousePos, contentX, rowY, contentWidth, linePitch);

                if (options.selectable && g_frame.justPressed && lineHovered)
                {
                    state.selectedLine = lineIndex;
                    state.hasSelection = true;
                    result.selected_line = lineIndex;
                }

                if (options.selectable && state.hasSelection && state.selectedLine == lineIndex)
                    draw_sprite(palette.buttonActive, contentX - 2.0f, rowY - 1.0f, contentWidth + 4.0f, linePitch + 2.0f);
                else if (lineHovered)
                    draw_sprite(palette.buttonHover, contentX - 2.0f, rowY - 1.0f, contentWidth + 4.0f, linePitch + 2.0f);

                std::string_view line{ options.lines[lineIndex] };
                if (options.max_line_chars > 0 && line.size() > options.max_line_chars)
                    line = line.substr(0, options.max_line_chars);

                const std::string fitted = fit_text_to_width(line, contentWidth, kFontScale);
                draw_text_line(fitted.empty() ? line : std::string_view{ fitted }, contentX, rowY, kFontScale);
            }
        }

        if (scrollbarWidth > 0.0f)
        {
            const float trackX = pos.x + width - kBoxInnerPadding - scrollbarWidth;
            const float trackY = contentY;
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, contentHeight);

            if (lineCount > visibleLines)
            {
                const float visibleRatio = static_cast<float>(visibleLines) / static_cast<float>(lineCount);
                const float thumbHeight = (std::max)(18.0f, contentHeight * visibleRatio);
                const float scrollRatio = maxFirstLine > 0
                    ? static_cast<float>(state.firstLine) / static_cast<float>(maxFirstLine)
                    : 0.0f;
                const float thumbY = trackY + (contentHeight - thumbHeight) * scrollRatio;
                draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
            }
        }

        if (state.hasSelection && state.selectedLine < lineCount)
            result.selected_line = state.selectedLine;

        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept
    {
        ConsoleWindowResult result{};
        if (!g_frame.ctx) return result;

        begin_window(options.title, options.position, options.size);
        if (!g_frame.insideWindow || !g_frame.ctx) { end_window(); return result; }

        ensure_resources();
        const float availableWidth = (std::max)(0.0f, options.size.x - 2.0f * kContentPadding);

        const float fieldHeight = options.input
            ? (base_line_height(kFontScale) + 2.0f * kBoxInnerPadding)
            : 0.0f;

        const float contentTopY = g_frame.cursor.y;
        const float contentBottomY = options.position.y + options.size.y - kContentPadding;
        const float reservedBottom = options.input ? (fieldHeight + kContentPadding) : 0.0f;

        const float logHeight = (std::max)(0.0f, contentBottomY - contentTopY - reservedBottom);
        const Vec2 logPos = g_frame.cursor;

        if (availableWidth > 0.0f && logHeight > 0.0f)
        {
            set_cursor(logPos);
            const std::string panelId = std::string(options.title) + ".log";
            (void)scroll_text_panel(ScrollTextPanelOptions{
                .id = panelId,
                .size = { availableWidth, logHeight },
                .lines = options.lines,
                .max_line_chars = options.max_visible_lines == 0 ? 768u : options.max_visible_lines * 16u,
                .selectable = true,
                .stick_to_bottom = true
            });
        }

        set_cursor({ logPos.x, logPos.y + logHeight + kContentPadding });

        if (options.input)
        {
            const float fieldHeight = base_line_height(kFontScale) + 2.0f * kBoxInnerPadding;
            const float rowY = g_frame.cursor.y;

            if (options.show_send_button)
            {
                const float buttonWidth = (std::max)(32.0f, options.send_button_width);
                const float inputWidth = (std::max)(1.0f, availableWidth - buttonWidth - kContentPadding);

                set_cursor({ logPos.x, rowY });
                Vec2 inputSize{ inputWidth, fieldHeight };
                result.input = edit_box(*options.input, inputSize, options.max_input_chars, options.multiline_input);

                set_cursor({ logPos.x + inputWidth + kContentPadding, rowY });
                if (button(options.send_button_label, { buttonWidth, fieldHeight }) && options.send_button_enabled)
                    result.send_clicked = true;
            }
            else
            {
                Vec2 inputSize{ availableWidth, fieldHeight };
                result.input = edit_box(*options.input, inputSize, options.max_input_chars, options.multiline_input);
            }
        }

        end_window();
        return result;
    }
} // namespace epochnamespace::gui
