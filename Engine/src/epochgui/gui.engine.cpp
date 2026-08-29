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
// Engine/src/epochgui/gui.engine.cpp
module;

#include "core.format_text.hpp"
#include <gui/node_graph_workspace.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <iterator>
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
#include "../platform.framework.hpp"
#endif

module gui.engine;

import epoch.gui;
import epoch.gui.input;
import epoch.gui.image;
import epoch.gui.rounded_rect;

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
import sprite.registry;
import sprite.handle;
import texture.core;

namespace epochengine::gui
{
    using Context = epochengine::core::Context;

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
    constexpr float       kGlyphRasterPadding = 2.0f;
    constexpr float       kCaretBlinkPeriod = 1.0f;
    constexpr float       kMinQueuedSpriteExtent = 0.5f;
    constexpr float       kMaxQueuedSpriteExtent = 65536.0f;
    constexpr int         kTabSpaces = 4;
    constexpr const char* kDefaultFontName = "__agui_default_font";
    constexpr const char* kDarkTextFontName = "__agui_dark_text_font";
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

        struct RoundedCorners
        {
            SpriteHandle topLeft{};
            SpriteHandle topRight{};
            SpriteHandle bottomRight{};
            SpriteHandle bottomLeft{};
            float sourceRadius{ 6.0f };

            [[nodiscard]] bool valid() const noexcept
            {
                return topLeft.is_valid()
                    && topRight.is_valid()
                    && bottomRight.is_valid()
                    && bottomLeft.is_valid();
            }
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
            PaletteSprites defaultLight{};
            PaletteSprites classicLauncher{};
            PaletteSprites midnightBlue{};
            PaletteSprites emberForge{};
            PaletteSprites forestTerminal{};
            PaletteSprites auroraSteel{};
            SpriteHandle dockGuide{};
            SpriteHandle dockGuideHover{};
            SpriteHandle dockGuidePreview{};
            SpriteHandle dockContextGuide{};
            SpriteHandle dockContextHover{};
            GuiFontCache font{};
            GuiFontCache darkTextFont{
                .fontName = kDarkTextFontName
            };
            font::FontRenderer fontRenderer{};
            std::unordered_map<SpriteHandle, RoundedCorners, SpriteHandleHash> roundedControls{};
        };

        struct CachedRuntimeSurface
        {
            SpriteHandle handle{};
            std::uint64_t contentHash = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t version = 0;
            std::string spriteName{};
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
            bool uploadQueued = false;
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
            std::uint64_t replayedGeneration = 0;
        };

        static std::unordered_map<const void*, UploadState, PtrHash> g_uploadedContexts{};
        static std::mutex g_uploadMutex{};
        static std::unordered_map<const void*, DeferredDrawBatch, PtrHash> g_deferredDrawBatches{};
        static std::unordered_map<const void*, DeferredDrawBatch, PtrHash> g_topLayerDrawBatches{};
        static std::mutex g_deferredBatchMutex{};
        static std::unordered_map<const void*, std::vector<InputEvent>, PtrHash> g_contextPendingEvents{};
        static std::mutex g_contextPendingEventsMutex{};

        constexpr std::size_t kMaxPendingInputEvents = 512;

        [[nodiscard]] constexpr bool is_coalescible_input(EventType type) noexcept
        {
            return type == EventType::MouseMove || type == EventType::MouseWheel;
        }

        void queue_input_event(
            std::vector<InputEvent>& events,
            const InputEvent& event) noexcept
        {
            if (event.type == EventType::None)
                return;

            try
            {
                if (!events.empty()
                    && event.type == EventType::MouseMove
                    && events.back().type == EventType::MouseMove
                    && events.back().mouse_button == event.mouse_button)
                {
                    events.back() = event;
                    return;
                }

                if (!events.empty()
                    && event.type == EventType::MouseWheel
                    && events.back().type == EventType::MouseWheel)
                {
                    const auto accumulated = static_cast<long long>(events.back().wheel_delta)
                        + static_cast<long long>(event.wheel_delta);
                    events.back() = event;
                    events.back().wheel_delta = static_cast<int>((std::clamp)(
                        accumulated,
                        static_cast<long long>((std::numeric_limits<int>::min)()),
                        static_cast<long long>((std::numeric_limits<int>::max)())));
                    return;
                }

                if (events.size() >= kMaxPendingInputEvents)
                {
                    const auto stale = std::find_if(
                        events.begin(),
                        events.end(),
                        [](const InputEvent& queued) noexcept
                        {
                            return is_coalescible_input(queued.type);
                        });
                    events.erase(stale != events.end() ? stale : events.begin());
                }

                events.push_back(event);
            }
            catch (...)
            {
                // Input is transient. A failed allocation must not terminate a
                // renderer or native message thread.
            }
        }

        static thread_local std::unordered_map<const void*, bool, PtrHash> g_contextMouseDownStates{};
        static thread_local std::unordered_map<const void*, bool, PtrHash> g_contextRightMouseDownStates{};
        static thread_local std::unordered_map<const void*, const void*, PtrHash> g_contextActiveWidgets{};
        static thread_local std::unordered_map<const void*, std::size_t, PtrHash> g_contextPressedButtonKeys{};
        struct EditBoxTextState
        {
            gui_lib::TextControlState control{};
            bool initialized{};
            bool draggingSelection{};
        };

        static thread_local std::unordered_map<const void*, EditBoxTextState, PtrHash> g_editBoxTextStates{};
        static thread_local std::unordered_map<const void*, bool, PtrHash> g_textFieldSelectAllStates{};

        struct ScrollTextState
        {
            std::size_t firstLine = 0;
            std::size_t selectedLine = 0;
            std::size_t selectionAnchorLine = 0;
            bool hasSelection = false;
            std::size_t lastLineCount = 0;
            float scrollY = 0.0f;
            float lastContentPixelHeight = 0.0f;
            float lastViewportHeight = 0.0f;
            std::uint64_t scrollToEndGeneration = 0u;
            bool draggingScrollbar = false;
            bool draggingSelection = false;
            float dragGrabOffset = 0.0f;
            bool contextMenuOpen = false;
            Vec2 contextMenuPos{};
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
            bool alignSelectedOnOpen = false;
        };

        struct PendingSelectPopup
        {
            std::string scrollKey{};
            Vec2 position{};
            float width = 0.0f;
            float height = 0.0f;
            float rowHeight = 0.0f;
            float optionPitch = 0.0f;
            float contentHeight = 0.0f;
            bool showScrollbar = false;
            std::string selected{};
            std::vector<std::string> options{};
        };

        struct SourceEditorState
        {
            bool contextMenuOpen = false;
            Vec2 contextMenuPos{};
            std::size_t cursorIndex = 0;
            std::size_t selectionAnchor = 0;
            bool hasSelection = false;
            bool draggingSelection = false;
            float scrollX = 0.0f;
            std::size_t preferredColumn{};
            bool verticalColumnActive{};
            std::uint64_t lastGotoGeneration{};
        };

        struct AssetGridContextMenuState
        {
            Vec2 position{};
            float scrollOffset{};
            std::uint64_t targetId{};
            bool open{};
        };

        struct EditBoxMenuState
        {
            bool open = false;
            Vec2 pos{};
        };

        struct NodeGraphCanvasState
        {
            gui_lib::node_graph_workspace::Controller controller{};
            std::unordered_map<std::uint64_t,
                gui_lib::node_graph_workspace::Vec2> nodeOffsets{};
            std::uint64_t externalSelectionSignature{};
            bool initialized{};
        };

        struct ScrollAreaFrame
        {
            std::string key{};
            std::string ownerWindowKey{};
            std::size_t ownerWindowDepth{};
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

        struct WindowFrameState
        {
            Vec2 cursor{};
            Vec2 origin{};
            Vec2 windowSize{};
            Vec2 contentMin{};
            Vec2 contentMax{};
            std::string windowKey{};
            std::uint64_t widgetSerial = 0;
            std::size_t scrollAreaDepth = 0;
            bool insideWindow = false;
        };

        static thread_local std::unordered_map<std::string, ScrollTextState> g_scrollTextStates{};
        static thread_local std::unordered_map<std::string, ScrollAreaState> g_scrollAreaStates{};
        static thread_local std::unordered_map<std::string, SelectBoxState> g_selectBoxStates{};
        static thread_local std::unordered_map<std::string, SourceEditorState> g_sourceEditorStates{};
        static thread_local std::unordered_map<std::string, AssetGridContextMenuState>
            g_assetGridContextMenuStates{};
        static thread_local std::unordered_map<std::string, NodeGraphCanvasState> g_nodeGraphCanvasStates{};
        static thread_local std::unordered_map<const void*, EditBoxMenuState, PtrHash> g_editBoxMenuStates{};
        static thread_local std::vector<ScrollAreaFrame> g_scrollAreaStack{};
        static thread_local std::vector<bool> g_floatingWindowTopLayerStack{};
        static thread_local std::vector<WindowFrameState> g_windowFrameStack{};

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
            bool mouseRightDown = false;
            bool prevMouseRightDown = false;
            bool justReleased = false;
            bool rightJustReleased = false;
            bool insideWindow = false;
            bool justPressed = false;
            bool rightJustPressed = false;
            bool mousePressConsumed = false;
            bool mouseReleaseConsumed = false;
            bool rightPressConsumed = false;
            bool rightReleaseConsumed = false;
            int mouseWheelDelta = 0;
            gui_lib::input::ModalInputArbiter modalInput{};

            std::optional<WidgetBounds> lastButtonBounds{};

            float deltaTime = 0.0f;
            float caretTimer = 0.0f;
            bool caretVisible = true;
            ThemeVariant activeTheme = ThemeVariant::DefaultDark;
            gui_lib::rounded_rect::RoundedRectStyle roundedRectStyle{};

            std::vector<InputEvent> events{};
            std::vector<QueuedSpriteDraw> queuedDraws{};
            std::vector<QueuedSpriteDraw> topLayerDraws{};
            std::vector<PendingSelectPopup> pendingSelectPopups{};
            std::vector<ThemeVariant> themeStack{};
            std::vector<gui_lib::rounded_rect::RoundedRectStyle> roundedRectStyleStack{};
            int topLayerDepth = 0;
        };

        static thread_local FrameState g_frame{};
        static thread_local std::vector<InputEvent> g_pendingEvents{};

        [[nodiscard]] static const GuiResources::PaletteSprites& active_palette() noexcept
        {
            switch (g_frame.activeTheme)
            {
            case ThemeVariant::DefaultLight:
                return g_resources.defaultLight;
            case ThemeVariant::ClassicLauncher:
                return g_resources.classicLauncher;
            case ThemeVariant::MidnightBlue:
                return g_resources.midnightBlue;
            case ThemeVariant::EmberForge:
                return g_resources.emberForge;
            case ThemeVariant::ForestTerminal:
                return g_resources.forestTerminal;
            case ThemeVariant::AuroraSteel:
                return g_resources.auroraSteel;
            case ThemeVariant::DefaultDark:
            default:
                return g_resources.defaultDark;
            }
        }

        [[nodiscard]] static const GuiFontCache& active_font_cache() noexcept
        {
            if (g_frame.activeTheme == ThemeVariant::DefaultLight
                && g_resources.darkTextFont.asset)
                return g_resources.darkTextFont;
            return g_resources.font;
        }

        [[nodiscard]] static bool system_prefers_dark_palette() noexcept
        {
#if defined(_WIN32)
            DWORD appsUseLightTheme = 1;
            DWORD valueSize = sizeof(appsUseLightTheme);
            const LSTATUS status = ::RegGetValueW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                L"AppsUseLightTheme",
                RRF_RT_REG_DWORD,
                nullptr,
                &appsUseLightTheme,
                &valueSize);
            if (status == ERROR_SUCCESS)
                return appsUseLightTheme == 0;
#endif
            return true;
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

        struct ContentClipClearScope
        {
            Vec2 previousMin{};
            Vec2 previousMax{};

            ContentClipClearScope() noexcept
            {
                previousMin = g_frame.contentMin;
                previousMax = g_frame.contentMax;
                g_frame.contentMin = g_frame.origin;
                g_frame.contentMax = {
                    g_frame.origin.x + g_frame.windowSize.x,
                    g_frame.origin.y + g_frame.windowSize.y
                };
            }

            ~ContentClipClearScope()
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

        [[nodiscard]] static std::string window_scoped_state_key(std::string_view id)
        {
            const auto ctxValue = reinterpret_cast<std::uintptr_t>(g_frame.ctx);
            return gui_lib::scoped_control_key(
                std::to_string(ctxValue),
                g_frame.windowKey,
                id);
        }

        [[nodiscard]] static std::string scroll_panel_key(std::string_view id)
        {
            return window_scoped_state_key(id);
        }

        [[nodiscard]] static std::string widget_state_key(std::string_view id)
        {
            return window_scoped_state_key(id);
        }

        [[nodiscard]] static std::string asset_grid_state_key(std::string_view id)
        {
            return window_scoped_state_key(id);
        }

        [[nodiscard]] static gui_lib::Vec2 to_lib(Vec2 v) noexcept
        {
            return { v.x, v.y };
        }

        [[nodiscard]] static Vec2 from_lib(gui_lib::Vec2 v) noexcept
        {
            return { v.x, v.y };
        }

        [[nodiscard]] static gui_lib::FloatingWindowState to_lib(const FloatingWindowState& state) noexcept
        {
            return gui_lib::FloatingWindowState{
                .position = to_lib(state.position),
                .size = to_lib(state.size),
                .drag_offset = to_lib(state.drag_offset),
                .resize_origin_mouse = to_lib(state.resize_origin_mouse),
                .resize_origin_size = to_lib(state.resize_origin_size),
                .open = state.open,
                .initialized = state.initialized,
                .dragging = state.dragging,
                .resizing = state.resizing,
                .close_pressed = state.close_pressed,
                .focus_order = state.focus_order
            };
        }

        static void from_lib(const gui_lib::FloatingWindowState& source, FloatingWindowState& state) noexcept
        {
            state.position = from_lib(source.position);
            state.size = from_lib(source.size);
            state.drag_offset = from_lib(source.drag_offset);
            state.resize_origin_mouse = from_lib(source.resize_origin_mouse);
            state.resize_origin_size = from_lib(source.resize_origin_size);
            state.open = source.open;
            state.initialized = source.initialized;
            state.dragging = source.dragging;
            state.resizing = source.resizing;
            state.close_pressed = source.close_pressed;
            state.focus_order = source.focus_order;
        }

        [[nodiscard]] static WidgetBounds from_lib(gui_lib::Rect rect) noexcept
        {
            return WidgetBounds{
                .position = from_lib(rect.position),
                .size = from_lib(rect.size)
            };
        }

        [[nodiscard]] static gui_lib::Rect to_lib(WidgetBounds bounds) noexcept
        {
            return gui_lib::Rect{
                .position = to_lib(bounds.position),
                .size = to_lib(bounds.size)
            };
        }

        [[nodiscard]] static gui_lib::DockSlot to_lib(DockSlot slot) noexcept
        {
            switch (slot)
            {
            case DockSlot::left: return gui_lib::DockSlot::left;
            case DockSlot::right: return gui_lib::DockSlot::right;
            case DockSlot::top: return gui_lib::DockSlot::top;
            case DockSlot::bottom: return gui_lib::DockSlot::bottom;
            case DockSlot::center: return gui_lib::DockSlot::center;
            case DockSlot::none:
            default: return gui_lib::DockSlot::none;
            }
        }

        [[nodiscard]] static DockSlot from_lib(gui_lib::DockSlot slot) noexcept
        {
            switch (slot)
            {
            case gui_lib::DockSlot::left: return DockSlot::left;
            case gui_lib::DockSlot::right: return DockSlot::right;
            case gui_lib::DockSlot::top: return DockSlot::top;
            case gui_lib::DockSlot::bottom: return DockSlot::bottom;
            case gui_lib::DockSlot::center: return DockSlot::center;
            case gui_lib::DockSlot::none:
            default: return DockSlot::none;
            }
        }

        [[nodiscard]] static gui_lib::DockGuideTarget to_lib(DockGuideTarget target) noexcept
        {
            switch (target)
            {
            case DockGuideTarget::left_tabs: return gui_lib::DockGuideTarget::left_tabs;
            case DockGuideTarget::right_tabs: return gui_lib::DockGuideTarget::right_tabs;
            case DockGuideTarget::bottom_left_tabs: return gui_lib::DockGuideTarget::bottom_left_tabs;
            case DockGuideTarget::bottom_right_tabs: return gui_lib::DockGuideTarget::bottom_right_tabs;
            case DockGuideTarget::left_context: return gui_lib::DockGuideTarget::left_context;
            case DockGuideTarget::right_context: return gui_lib::DockGuideTarget::right_context;
            case DockGuideTarget::float_window: return gui_lib::DockGuideTarget::float_window;
            case DockGuideTarget::none:
            default: return gui_lib::DockGuideTarget::none;
            }
        }

        [[nodiscard]] static DockGuideTarget from_lib(gui_lib::DockGuideTarget target) noexcept
        {
            switch (target)
            {
            case gui_lib::DockGuideTarget::left_tabs: return DockGuideTarget::left_tabs;
            case gui_lib::DockGuideTarget::right_tabs: return DockGuideTarget::right_tabs;
            case gui_lib::DockGuideTarget::bottom_left_tabs: return DockGuideTarget::bottom_left_tabs;
            case gui_lib::DockGuideTarget::bottom_right_tabs: return DockGuideTarget::bottom_right_tabs;
            case gui_lib::DockGuideTarget::left_context: return DockGuideTarget::left_context;
            case gui_lib::DockGuideTarget::right_context: return DockGuideTarget::right_context;
            case gui_lib::DockGuideTarget::float_window: return DockGuideTarget::float_window;
            case gui_lib::DockGuideTarget::none:
            default: return DockGuideTarget::none;
            }
        }

        [[nodiscard]] static gui_lib::DockGuideOptions to_lib(const DockGuideOptions& options) noexcept
        {
            return gui_lib::DockGuideOptions{
                .guide_bounds = to_lib(options.guide_bounds),
                .left_tabs_preview = to_lib(options.left_tabs_preview),
                .right_tabs_preview = to_lib(options.right_tabs_preview),
                .bottom_left_tabs_preview = to_lib(options.bottom_left_tabs_preview),
                .bottom_right_tabs_preview = to_lib(options.bottom_right_tabs_preview),
                .left_context_preview = to_lib(options.left_context_preview),
                .right_context_preview = to_lib(options.right_context_preview),
                .floating_preview = to_lib(options.floating_preview),
                .pointer = to_lib(options.pointer),
                .guide_extent = options.guide_extent,
                .guide_gap = options.guide_gap,
                .allow_side_tabs = options.allow_side_tabs,
                .allow_bottom_tabs = options.allow_bottom_tabs,
                .allow_contexts = options.allow_contexts,
                .allow_float = options.allow_float,
                .center_context_guides_in_previews =
                    options.center_context_guides_in_previews
            };
        }

        [[nodiscard]] static DockGuideLayout from_lib(const gui_lib::DockGuideLayout& source) noexcept
        {
            DockGuideLayout result{};
            result.count = (std::min)(source.count, std::uint32_t{7});
            result.hovered_target = from_lib(source.hovered_target);
            result.hovered_preview = from_lib(source.hovered_preview);
            for (std::uint32_t i = 0; i < result.count; ++i)
            {
                result.guides[i] = DockGuide{
                    .target = from_lib(source.guides[i].target),
                    .target_bounds = from_lib(source.guides[i].target_bounds),
                    .preview_bounds = from_lib(source.guides[i].preview_bounds),
                    .hovered = source.guides[i].hovered
                };
            }
            return result;
        }

        [[nodiscard]] static gui_lib::DockableWindowMode to_lib(DockableWindowMode mode) noexcept
        {
            switch (mode)
            {
            case DockableWindowMode::docked: return gui_lib::DockableWindowMode::docked;
            case DockableWindowMode::detached: return gui_lib::DockableWindowMode::detached;
            case DockableWindowMode::floating:
            default: return gui_lib::DockableWindowMode::floating;
            }
        }

        [[nodiscard]] static DockableWindowMode from_lib(gui_lib::DockableWindowMode mode) noexcept
        {
            switch (mode)
            {
            case gui_lib::DockableWindowMode::docked: return DockableWindowMode::docked;
            case gui_lib::DockableWindowMode::detached: return DockableWindowMode::detached;
            case gui_lib::DockableWindowMode::floating:
            default: return DockableWindowMode::floating;
            }
        }

        [[nodiscard]] static gui_lib::DockableWindowAction to_lib(DockableWindowAction action) noexcept
        {
            switch (action)
            {
            case DockableWindowAction::focus: return gui_lib::DockableWindowAction::focus;
            case DockableWindowAction::dock: return gui_lib::DockableWindowAction::dock;
            case DockableWindowAction::float_window: return gui_lib::DockableWindowAction::float_window;
            case DockableWindowAction::detach: return gui_lib::DockableWindowAction::detach;
            case DockableWindowAction::close: return gui_lib::DockableWindowAction::close;
            case DockableWindowAction::none:
            default: return gui_lib::DockableWindowAction::none;
            }
        }

        [[nodiscard]] static DockableWindowAction from_lib(gui_lib::DockableWindowAction action) noexcept
        {
            switch (action)
            {
            case gui_lib::DockableWindowAction::focus: return DockableWindowAction::focus;
            case gui_lib::DockableWindowAction::dock: return DockableWindowAction::dock;
            case gui_lib::DockableWindowAction::float_window: return DockableWindowAction::float_window;
            case gui_lib::DockableWindowAction::detach: return DockableWindowAction::detach;
            case gui_lib::DockableWindowAction::close: return DockableWindowAction::close;
            case gui_lib::DockableWindowAction::none:
            default: return DockableWindowAction::none;
            }
        }

        [[nodiscard]] static gui_lib::DockableWindowHostState to_lib(const DockableWindowHostState& host) noexcept
        {
            return gui_lib::DockableWindowHostState{
                .active_window_id = host.active_window_id,
                .next_focus_order = host.next_focus_order,
                .changed_this_frame = host.changed_this_frame
            };
        }

        static void from_lib(const gui_lib::DockableWindowHostState& source, DockableWindowHostState& host) noexcept
        {
            host.active_window_id = source.active_window_id;
            host.next_focus_order = source.next_focus_order;
            host.changed_this_frame = source.changed_this_frame;
        }

        [[nodiscard]] static gui_lib::DockableWindowState to_lib(const DockableWindowState& state) noexcept
        {
            return gui_lib::DockableWindowState{
                .id = state.id,
                .mode = to_lib(state.mode),
                .dock_slot = to_lib(state.dock_slot),
                .floating = to_lib(state.floating),
                .visible = state.visible,
                .initialized = state.initialized,
                .active = state.active,
                .detach_requested = state.detach_requested,
                .close_requested = state.close_requested,
                .focus_order = state.focus_order
            };
        }

        static void from_lib(const gui_lib::DockableWindowState& source, DockableWindowState& state) noexcept
        {
            state.id = source.id;
            state.mode = from_lib(source.mode);
            state.dock_slot = from_lib(source.dock_slot);
            from_lib(source.floating, state.floating);
            state.visible = source.visible;
            state.initialized = source.initialized;
            state.active = source.active;
            state.detach_requested = source.detach_requested;
            state.close_requested = source.close_requested;
            state.focus_order = source.focus_order;
        }

        [[nodiscard]] static gui_lib::DockableWindowOptions to_lib(const DockableWindowOptions& options) noexcept
        {
            return gui_lib::DockableWindowOptions{
                .title = options.title,
                .docked_frame = to_lib(options.docked_frame),
                .floating = gui_lib::FloatingWindowOptions{
                    .default_position = to_lib(options.floating.default_position),
                    .default_size = to_lib(options.floating.default_size),
                    .min_size = to_lib(options.floating.min_size),
                    .viewport_size = to_lib(options.floating.viewport_size),
                    .movable = options.floating.movable,
                    .resizable = options.floating.resizable,
                    .closable = options.floating.closable
                },
                .viewport_size = to_lib(options.viewport_size),
                .title_bar_height = options.title_bar_height,
                .content_padding = options.content_padding,
                .action_button_width = options.action_button_width,
                .action_button_gap = options.action_button_gap,
                .allow_dock = options.allow_dock,
                .allow_float = options.allow_float,
                .allow_detach = options.allow_detach,
                .allow_close = options.allow_close,
                .fallback_dock_slot = to_lib(options.fallback_dock_slot)
            };
        }

        [[nodiscard]] static gui_lib::DockableWindowInput to_lib(const DockableWindowInput& input) noexcept
        {
            return gui_lib::DockableWindowInput{
                .mouse_position = to_lib(input.mouse_position),
                .mouse_down = input.mouse_down,
                .mouse_pressed = input.mouse_pressed,
                .mouse_released = input.mouse_released,
                .requested_action = to_lib(input.requested_action),
                .requested_dock_slot = to_lib(input.requested_dock_slot)
            };
        }

        [[nodiscard]] static DockableWindowChrome from_lib(const gui_lib::DockableWindowChrome& chrome) noexcept
        {
            return DockableWindowChrome{
                .frame = from_lib(chrome.frame),
                .title_bar = from_lib(chrome.title_bar),
                .content = from_lib(chrome.content),
                .dock_button = from_lib(chrome.dock_button),
                .float_button = from_lib(chrome.float_button),
                .detach_button = from_lib(chrome.detach_button),
                .close_button = from_lib(chrome.close_button),
                .visible = chrome.visible,
                .hovered = chrome.hovered,
                .title_hovered = chrome.title_hovered,
                .dock_hovered = chrome.dock_hovered,
                .float_hovered = chrome.float_hovered,
                .detach_hovered = chrome.detach_hovered,
                .close_hovered = chrome.close_hovered,
                .active = chrome.active
            };
        }

        [[nodiscard]] static DockableWindowResult from_lib(const gui_lib::DockableWindowResult& result) noexcept
        {
            return DockableWindowResult{
                .chrome = from_lib(result.chrome),
                .mode = from_lib(result.mode),
                .action = from_lib(result.action),
                .dock_slot = from_lib(result.dock_slot),
                .changed = result.changed,
                .focused = result.focused,
                .dock_requested = result.dock_requested,
                .float_requested = result.float_requested,
                .detach_requested = result.detach_requested,
                .close_requested = result.close_requested
            };
        }

        [[nodiscard]] static bool uses_deferred_gui_batch(const core::Context* ctx) noexcept
        {
            return ctx
                && (ctx->type == core::ContextType::Software
                    || ctx->type == core::ContextType::OpenGL
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

                if (epochengine::spritepool::capacity == 0)
                    epochengine::spritepool::initialize(2048);

                SpriteHandle handle = epochengine::spritepool::allocate();
                if (!handle.is_valid())
                    return {};

                handle.atlasIndex = static_cast<std::uint32_t>(atlas.get_index());
                handle.localIndex = static_cast<std::uint32_t>(entry->index);

                epochengine::atlasmanager::registry.add(
                    name, handle,
                    entry->region.u1,
                    entry->region.v1,
                    entry->region.u2 - entry->region.u1,
                    entry->region.v2 - entry->region.v1);

                if (queueUpload)
                    epochengine::atlasmanager::ensure_uploaded(atlas);

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
            constexpr std::uint32_t gutter = 1U;
            if (w == 0U || h == 0U
                || pixels.size() != static_cast<std::size_t>(w) * h * 4U)
            {
                throw std::runtime_error("[agui] Invalid GUI sprite: " + name);
            }

            gui_lib::image::Image source{};
            source.width = w;
            source.height = h;
            source.pixels.resize(static_cast<std::size_t>(w) * h);
            for (std::size_t index = 0; index < source.pixels.size(); ++index)
            {
                const std::size_t byte = index * 4U;
                source.pixels[index] = {
                    pixels[byte],
                    pixels[byte + 1U],
                    pixels[byte + 2U],
                    pixels[byte + 3U]
                };
            }

            const gui_lib::image::ImageResult padded =
                gui_lib::image::extrude_edge_gutter(source, gutter);
            if (!padded)
                throw std::runtime_error("[agui] Failed to pad GUI sprite: " + name);

            std::vector<std::uint8_t> padded_pixels;
            padded_pixels.reserve(padded.image.pixels.size() * 4U);
            for (const gui_lib::image::Rgba8 pixel : padded.image.pixels)
            {
                padded_pixels.push_back(pixel.r);
                padded_pixels.push_back(pixel.g);
                padded_pixels.push_back(pixel.b);
                padded_pixels.push_back(pixel.a);
            }

            Texture texture{};
            texture.name = name + "/gutter";
            texture.width = padded.image.width;
            texture.height = padded.image.height;
            texture.channels = 4;
            texture.pixels = std::move(padded_pixels);

            const auto padded_entry = atlas.add_entry(texture.name, texture);
            if (!padded_entry)
                throw std::runtime_error("[agui] Failed to register GUI sprite: " + name);

            const auto entry = atlas.add_slice_entry(
                name,
                static_cast<int>(padded_entry->region.x + gutter),
                static_cast<int>(padded_entry->region.y + gutter),
                static_cast<int>(w),
                static_cast<int>(h));
            if (!entry)
                throw std::runtime_error("[agui] Failed to register GUI sprite slice: " + name);

            if (epochengine::spritepool::capacity == 0)
                epochengine::spritepool::initialize(2048);

            SpriteHandle handle = epochengine::spritepool::allocate();
            if (!handle.is_valid())
                throw std::runtime_error("[agui] Failed to allocate GUI sprite: " + name);

            handle.atlasIndex = static_cast<std::uint32_t>(atlas.get_index());
            handle.localIndex = static_cast<std::uint32_t>(entry->index);
            epochengine::atlasmanager::registry.add(
                name,
                handle,
                entry->region.u1,
                entry->region.v1,
                entry->region.u2 - entry->region.u1,
                entry->region.v2 - entry->region.v1);
            return handle;
        }
        [[nodiscard]] static bool point_in_triangle(
            gui_lib::Vec2 point,
            gui_lib::Vec2 a,
            gui_lib::Vec2 b,
            gui_lib::Vec2 c) noexcept
        {
            const auto edge = [](gui_lib::Vec2 first, gui_lib::Vec2 second, gui_lib::Vec2 sample) noexcept
            {
                return (sample.x - second.x) * (first.y - second.y)
                    - (first.x - second.x) * (sample.y - second.y);
            };

            const float ab = edge(a, b, point);
            const float bc = edge(b, c, point);
            const float ca = edge(c, a, point);
            const bool hasNegative = ab < 0.0f || bc < 0.0f || ca < 0.0f;
            const bool hasPositive = ab > 0.0f || bc > 0.0f || ca > 0.0f;
            return !(hasNegative && hasPositive);
        }

        [[nodiscard]] static std::vector<std::uint8_t> make_rounded_rect_pixels(
            Color color,
            std::uint32_t radius)
        {
            const std::uint32_t extent = radius * 2U;
            std::vector<std::uint8_t> pixels(
                static_cast<std::size_t>(extent) * extent * 4U,
                0U);
            if (radius == 0U)
                return pixels;

            const auto mesh = gui_lib::rounded_rect::make_rounded_rect_mesh({
                .bounds = { { 0.0f, 0.0f }, { static_cast<float>(extent), static_cast<float>(extent) } },
                .radii = {
                    static_cast<float>(radius),
                    static_cast<float>(radius),
                    static_cast<float>(radius),
                    static_cast<float>(radius)
                },
                .segments_per_corner = 8U
            });
            if (!mesh.valid)
                return pixels;

            for (std::uint32_t y = 0; y < extent; ++y)
            {
                for (std::uint32_t x = 0; x < extent; ++x)
                {
                    const gui_lib::Vec2 sample{
                        static_cast<float>(x) + 0.5f,
                        static_cast<float>(y) + 0.5f
                    };
                    bool covered = false;
                    for (std::size_t index = 0; index + 2U < mesh.fill_indices.size(); index += 3U)
                    {
                        if (point_in_triangle(
                            sample,
                            mesh.vertices[mesh.fill_indices[index]],
                            mesh.vertices[mesh.fill_indices[index + 1U]],
                            mesh.vertices[mesh.fill_indices[index + 2U]]))
                        {
                            covered = true;
                            break;
                        }
                    }
                    if (!covered)
                        continue;

                    const std::size_t pixel = (static_cast<std::size_t>(y) * extent + x) * 4U;
                    pixels[pixel + 0U] = color.r;
                    pixels[pixel + 1U] = color.g;
                    pixels[pixel + 2U] = color.b;
                    pixels[pixel + 3U] = color.a;
                }
            }
            return pixels;
        }

        [[nodiscard]] static std::vector<std::uint8_t> extract_corner_pixels(
            std::span<const std::uint8_t> source,
            std::uint32_t radius,
            std::uint32_t sourceX,
            std::uint32_t sourceY)
        {
            const std::uint32_t sourceExtent = radius * 2U;
            std::vector<std::uint8_t> corner(
                static_cast<std::size_t>(radius) * radius * 4U,
                0U);
            for (std::uint32_t y = 0; y < radius; ++y)
            {
                for (std::uint32_t x = 0; x < radius; ++x)
                {
                    const std::size_t sourcePixel =
                        (static_cast<std::size_t>(sourceY + y) * sourceExtent + sourceX + x) * 4U;
                    const std::size_t targetPixel =
                        (static_cast<std::size_t>(y) * radius + x) * 4U;
                    std::copy_n(source.data() + sourcePixel, 4U, corner.data() + targetPixel);
                }
            }
            return corner;
        }

        static void register_rounded_control(
            TextureAtlas& atlas,
            SpriteHandle fill,
            std::string_view name,
            Color color)
        {
            constexpr std::uint32_t radius = 6U;
            const auto pixels = make_rounded_rect_pixels(color, radius);
            const std::string base{ name };
            RoundedCorners corners{
                .topLeft = add_sprite(
                    atlas,
                    base + "/top_left",
                    extract_corner_pixels(pixels, radius, 0U, 0U),
                    radius,
                    radius),
                .topRight = add_sprite(
                    atlas,
                    base + "/top_right",
                    extract_corner_pixels(pixels, radius, radius, 0U),
                    radius,
                    radius),
                .bottomRight = add_sprite(
                    atlas,
                    base + "/bottom_right",
                    extract_corner_pixels(pixels, radius, radius, radius),
                    radius,
                    radius),
                .bottomLeft = add_sprite(
                    atlas,
                    base + "/bottom_left",
                    extract_corner_pixels(pixels, radius, 0U, radius),
                    radius,
                    radius),
                .sourceRadius = static_cast<float>(radius)
            };
            if (fill.is_valid() && corners.valid())
                g_resources.roundedControls.insert_or_assign(fill, std::move(corners));
        }

        [[nodiscard]] static TextureAtlas* ensure_runtime_surface_atlas_locked()
        {
            if (g_resources.runtimeSurfaceAtlas)
                return g_resources.runtimeSurfaceAtlas;

            auto atlasIt = epochengine::atlasmanager::atlas_map.find(kRuntimeSurfaceAtlasName);
            if (atlasIt == epochengine::atlasmanager::atlas_map.end())
            {
                (void)epochengine::atlasmanager::create_atlas({
                    .name = kRuntimeSurfaceAtlasName,
                    .width = kRuntimeSurfaceAtlasSize,
                    .height = kRuntimeSurfaceAtlasSize,
                    .generate_mipmaps = false
                    });
                atlasIt = epochengine::atlasmanager::atlas_map.find(kRuntimeSurfaceAtlasName);
            }

            if (atlasIt == epochengine::atlasmanager::atlas_map.end())
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

            if (const auto exampleAssets = epochengine::core::path::example_asset_dir(); !exampleAssets.empty())
            {
                if (auto path = try_with_root(exampleAssets / "fonts"); !path.empty())
                    return path;
                if (auto path = try_with_root(exampleAssets); !path.empty())
                    return path;
            }

            if (const auto engineAssets = epochengine::core::path::engine_asset_dir(); !engineAssets.empty())
            {
                if (auto path = try_with_root(engineAssets / "fonts"); !path.empty())
                    return path;
                if (auto path = try_with_root(engineAssets); !path.empty())
                    return path;
            }

            if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir(); !runtimeRoot.empty())
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
                    if (fontCache.glyphLookup[ch])
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
            if (g_resources.font.asset && g_resources.darkTextFont.asset)
                return;

            const std::filesystem::path fontPath = find_default_font_path();
            if (fontPath.empty())
            {
                if (!g_missingFontPathWarningLogged)
                {
                    logger::warn(
                        "Epoch.GUI",
                        epochengine::format_text(
                            "Unable to locate GUI font '{}'. Place it in the resolved example/runtime assets/fonts path or set EPOCH_GUI_FONT_PATH.",
                            kDefaultFontFile));
                    g_missingFontPathWarningLogged = true;
                }
                return;
            }

            const auto loadCache = [&](GuiFontCache& fontCache, font::FontColor color)
                {
                    if (fontCache.asset)
                        return true;

                    fontCache.asset = g_resources.fontRenderer.get_font(fontCache.fontName);
                    if (!fontCache.asset
                        && !g_resources.fontRenderer.load_font(
                            fontCache.fontName,
                            fontPath.string(),
                            fontCache.fontSizePt,
                            color))
                    {
                        return false;
                    }

                    fontCache.asset = g_resources.fontRenderer.get_font(fontCache.fontName);
                    if (!fontCache.asset)
                        return false;

                    fontCache.metrics = fontCache.asset->metrics;
                    populate_font_lookup(fontCache);

                    const auto atlasVec = epochengine::atlasmanager::get_atlas_vector_snapshot();
                    if (fontCache.asset->atlas_index >= 0
                        && static_cast<std::size_t>(fontCache.asset->atlas_index) < atlasVec.size())
                    {
                        fontCache.atlas = atlasVec[static_cast<std::size_t>(fontCache.asset->atlas_index)];
                    }

                    if (!fontCache.atlas)
                    {
                        if (auto it = epochengine::atlasmanager::atlas_map.find("font_atlas");
                            it != epochengine::atlasmanager::atlas_map.end())
                        {
                            fontCache.atlas = it->second.get();
                        }
                    }
                    return fontCache.atlas != nullptr;
                };

            const bool lightTextLoaded = loadCache(
                g_resources.font,
                font::FontColor{255, 255, 255, 255});
            const bool darkTextLoaded = loadCache(
                g_resources.darkTextFont,
                font::FontColor{32, 39, 48, 255});
            if (!lightTextLoaded || !darkTextLoaded)
            {
                if (!g_failedFontLoadWarningLogged)
                {
                    logger::error(
                        "Epoch.GUI",
                        epochengine::format_text(
                            "Failed to load the complete GUI font palette from '{}'",
                            fontPath.string()));
                    g_failedFontLoadWarningLogged = true;
                }
                if ((!g_resources.font.asset || !g_resources.darkTextFont.asset)
                    && !g_missingFontAssetWarningLogged)
                {
                    logger::error(
                        "Epoch.GUI",
                        "Font renderer returned an incomplete GUI font palette");
                    g_missingFontAssetWarningLogged = true;
                }
            }
        }

        static void ensure_resources()
        {
            std::scoped_lock lock(g_resourceMutex);

            if (!g_resources.atlasBuilt)
            {
                auto atlasIt = epochengine::atlasmanager::atlas_map.find(kAtlasName);
                if (atlasIt == epochengine::atlasmanager::atlas_map.end())
                {
                    epochengine::atlasmanager::create_atlas({
                        .name = kAtlasName,
                        .width = 512,
                        .height = 512,
                        .generate_mipmaps = false
                        });

                    atlasIt = epochengine::atlasmanager::atlas_map.find(kAtlasName);
                    if (atlasIt == epochengine::atlasmanager::atlas_map.end())
                        throw std::runtime_error("[agui] Unable to create GUI atlas");
                }

                TextureAtlas& atlas = *atlasIt->second;
                g_resources.atlas = &atlas;
                g_resources.dockGuide = add_sprite(atlas, "__agui/dock_guide",
                    make_solid_pixels(0xF0, 0x82, 0x24, 0x68, 8, 8), 8, 8);
                g_resources.dockGuideHover = add_sprite(atlas, "__agui/dock_guide_hover",
                    make_solid_pixels(0xFF, 0xA1, 0x38, 0xC0, 8, 8), 8, 8);
                g_resources.dockGuidePreview = add_sprite(atlas, "__agui/dock_guide_preview",
                    make_solid_pixels(0xF0, 0x82, 0x24, 0x34, 8, 8), 8, 8);
                g_resources.dockContextGuide = add_sprite(atlas, "__agui/dock_context_guide",
                    make_solid_pixels(0xFF, 0xA1, 0x38, 0x78, 8, 8), 8, 8);
                g_resources.dockContextHover = add_sprite(atlas, "__agui/dock_context_hover",
                    make_solid_pixels(0xFF, 0xB4, 0x55, 0xD0, 8, 8), 8, 8);

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

                g_resources.defaultLight.windowBackground = add_sprite(atlas, "__agui_light/window_bg",
                    make_solid_pixels(0xED, 0xF1, 0xF5, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.buttonNormal = add_sprite(atlas, "__agui_light/button_normal",
                    make_solid_pixels(0xDD, 0xE3, 0xEA, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.buttonHover = add_sprite(atlas, "__agui_light/button_hover",
                    make_solid_pixels(0xCE, 0xD8, 0xE4, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.buttonActive = add_sprite(atlas, "__agui_light/button_active",
                    make_solid_pixels(0xB8, 0xCB, 0xE0, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.textField = add_sprite(atlas, "__agui_light/text_field",
                    make_solid_pixels(0xFF, 0xFF, 0xFF, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.textFieldActive = add_sprite(atlas, "__agui_light/text_field_active",
                    make_solid_pixels(0xD9, 0xE6, 0xF2, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.panelBackground = add_sprite(atlas, "__agui_light/panel_bg",
                    make_solid_pixels(0xF6, 0xF8, 0xFA, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.consoleBackground = add_sprite(atlas, "__agui_light/console_bg",
                    make_solid_pixels(0xFF, 0xFF, 0xFF, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.titleBar = add_sprite(atlas, "__agui_light/title_bar",
                    make_solid_pixels(0xD7, 0xDE, 0xE7, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.modalScrim = add_sprite(atlas, "__agui_light/modal_scrim",
                    make_solid_pixels(0x20, 0x27, 0x30, 0x78, 8, 8), 8, 8);

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

                g_resources.midnightBlue.windowBackground = add_sprite(atlas, "__agui_midnight/window_bg",
                    make_solid_pixels(0x10, 0x19, 0x2A, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.buttonNormal = add_sprite(atlas, "__agui_midnight/button_normal",
                    make_solid_pixels(0x1D, 0x2E, 0x4A, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.buttonHover = add_sprite(atlas, "__agui_midnight/button_hover",
                    make_solid_pixels(0x28, 0x42, 0x66, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.buttonActive = add_sprite(atlas, "__agui_midnight/button_active",
                    make_solid_pixels(0x36, 0x5F, 0x91, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.textField = add_sprite(atlas, "__agui_midnight/text_field",
                    make_solid_pixels(0x0D, 0x14, 0x22, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.textFieldActive = add_sprite(atlas, "__agui_midnight/text_field_active",
                    make_solid_pixels(0x16, 0x25, 0x3D, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.panelBackground = add_sprite(atlas, "__agui_midnight/panel_bg",
                    make_solid_pixels(0x14, 0x21, 0x36, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.consoleBackground = add_sprite(atlas, "__agui_midnight/console_bg",
                    make_solid_pixels(0x08, 0x0F, 0x19, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.titleBar = add_sprite(atlas, "__agui_midnight/title_bar",
                    make_solid_pixels(0x19, 0x2B, 0x46, 0xFF, 8, 8), 8, 8);
                g_resources.midnightBlue.modalScrim = add_sprite(atlas, "__agui_midnight/modal_scrim",
                    make_solid_pixels(0x02, 0x05, 0x0B, 0xC4, 8, 8), 8, 8);

                g_resources.emberForge.windowBackground = add_sprite(atlas, "__agui_ember/window_bg",
                    make_solid_pixels(0x24, 0x1C, 0x19, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.buttonNormal = add_sprite(atlas, "__agui_ember/button_normal",
                    make_solid_pixels(0x3E, 0x2A, 0x22, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.buttonHover = add_sprite(atlas, "__agui_ember/button_hover",
                    make_solid_pixels(0x55, 0x37, 0x28, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.buttonActive = add_sprite(atlas, "__agui_ember/button_active",
                    make_solid_pixels(0x70, 0x45, 0x2D, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.textField = add_sprite(atlas, "__agui_ember/text_field",
                    make_solid_pixels(0x1B, 0x14, 0x12, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.textFieldActive = add_sprite(atlas, "__agui_ember/text_field_active",
                    make_solid_pixels(0x32, 0x22, 0x1C, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.panelBackground = add_sprite(atlas, "__agui_ember/panel_bg",
                    make_solid_pixels(0x2C, 0x20, 0x1A, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.consoleBackground = add_sprite(atlas, "__agui_ember/console_bg",
                    make_solid_pixels(0x14, 0x0F, 0x0D, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.titleBar = add_sprite(atlas, "__agui_ember/title_bar",
                    make_solid_pixels(0x42, 0x28, 0x1E, 0xFF, 8, 8), 8, 8);
                g_resources.emberForge.modalScrim = add_sprite(atlas, "__agui_ember/modal_scrim",
                    make_solid_pixels(0x08, 0x04, 0x03, 0xC0, 8, 8), 8, 8);

                g_resources.forestTerminal.windowBackground = add_sprite(atlas, "__agui_forest/window_bg",
                    make_solid_pixels(0x13, 0x22, 0x1D, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.buttonNormal = add_sprite(atlas, "__agui_forest/button_normal",
                    make_solid_pixels(0x24, 0x3D, 0x31, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.buttonHover = add_sprite(atlas, "__agui_forest/button_hover",
                    make_solid_pixels(0x30, 0x55, 0x40, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.buttonActive = add_sprite(atlas, "__agui_forest/button_active",
                    make_solid_pixels(0x3E, 0x72, 0x52, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.textField = add_sprite(atlas, "__agui_forest/text_field",
                    make_solid_pixels(0x0D, 0x17, 0x14, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.textFieldActive = add_sprite(atlas, "__agui_forest/text_field_active",
                    make_solid_pixels(0x1D, 0x34, 0x29, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.panelBackground = add_sprite(atlas, "__agui_forest/panel_bg",
                    make_solid_pixels(0x18, 0x2D, 0x24, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.consoleBackground = add_sprite(atlas, "__agui_forest/console_bg",
                    make_solid_pixels(0x08, 0x10, 0x0D, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.titleBar = add_sprite(atlas, "__agui_forest/title_bar",
                    make_solid_pixels(0x20, 0x3A, 0x2D, 0xFF, 8, 8), 8, 8);
                g_resources.forestTerminal.modalScrim = add_sprite(atlas, "__agui_forest/modal_scrim",
                    make_solid_pixels(0x02, 0x07, 0x05, 0xC0, 8, 8), 8, 8);

                g_resources.auroraSteel.windowBackground = add_sprite(atlas, "__agui_aurora/window_bg",
                    make_solid_pixels(0x20, 0x26, 0x2E, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.buttonNormal = add_sprite(atlas, "__agui_aurora/button_normal",
                    make_solid_pixels(0x34, 0x3F, 0x4A, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.buttonHover = add_sprite(atlas, "__agui_aurora/button_hover",
                    make_solid_pixels(0x41, 0x55, 0x61, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.buttonActive = add_sprite(atlas, "__agui_aurora/button_active",
                    make_solid_pixels(0x53, 0x72, 0x7B, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.textField = add_sprite(atlas, "__agui_aurora/text_field",
                    make_solid_pixels(0x18, 0x1E, 0x24, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.textFieldActive = add_sprite(atlas, "__agui_aurora/text_field_active",
                    make_solid_pixels(0x2A, 0x36, 0x3F, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.panelBackground = add_sprite(atlas, "__agui_aurora/panel_bg",
                    make_solid_pixels(0x26, 0x2E, 0x36, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.consoleBackground = add_sprite(atlas, "__agui_aurora/console_bg",
                    make_solid_pixels(0x14, 0x18, 0x1F, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.titleBar = add_sprite(atlas, "__agui_aurora/title_bar",
                    make_solid_pixels(0x2F, 0x3B, 0x45, 0xFF, 8, 8), 8, 8);
                g_resources.auroraSteel.modalScrim = add_sprite(atlas, "__agui_aurora/modal_scrim",
                    make_solid_pixels(0x04, 0x06, 0x08, 0xBE, 8, 8), 8, 8);

                const auto registerPaletteControls = [&atlas](
                    GuiResources::PaletteSprites& palette,
                    std::string_view prefix,
                    const std::array<Color, 5>& colors)
                {
                    register_rounded_control(atlas, palette.buttonNormal, std::string(prefix) + "/button_normal", colors[0]);
                    register_rounded_control(atlas, palette.buttonHover, std::string(prefix) + "/button_hover", colors[1]);
                    register_rounded_control(atlas, palette.buttonActive, std::string(prefix) + "/button_active", colors[2]);
                    register_rounded_control(atlas, palette.textField, std::string(prefix) + "/text_field", colors[3]);
                    register_rounded_control(atlas, palette.textFieldActive, std::string(prefix) + "/text_field_active", colors[4]);
                };

                registerPaletteControls(g_resources.defaultDark, "__agui_round/dark", {{
                    { 0x31, 0x36, 0x3F }, { 0x3C, 0x43, 0x4E }, { 0x48, 0x52, 0x60 },
                    { 0x21, 0x26, 0x2E }, { 0x2A, 0x31, 0x3B }
                }});
                registerPaletteControls(g_resources.defaultLight, "__agui_round/light", {{
                    { 0xDD, 0xE3, 0xEA }, { 0xCE, 0xD8, 0xE4 }, { 0xB8, 0xCB, 0xE0 },
                    { 0xFF, 0xFF, 0xFF }, { 0xD9, 0xE6, 0xF2 }
                }});
                registerPaletteControls(g_resources.classicLauncher, "__agui_round/classic", {{
                    { 0x5B, 0x5F, 0x66 }, { 0x6C, 0x71, 0x7A }, { 0x4C, 0x52, 0x5C },
                    { 0x2B, 0x2E, 0x33 }, { 0x3A, 0x3E, 0x45 }
                }});
                registerPaletteControls(g_resources.midnightBlue, "__agui_round/midnight", {{
                    { 0x1D, 0x2E, 0x4A }, { 0x28, 0x42, 0x66 }, { 0x36, 0x5F, 0x91 },
                    { 0x0D, 0x14, 0x22 }, { 0x16, 0x25, 0x3D }
                }});
                registerPaletteControls(g_resources.emberForge, "__agui_round/ember", {{
                    { 0x3E, 0x2A, 0x22 }, { 0x55, 0x37, 0x28 }, { 0x70, 0x45, 0x2D },
                    { 0x1B, 0x14, 0x12 }, { 0x32, 0x22, 0x1C }
                }});
                registerPaletteControls(g_resources.forestTerminal, "__agui_round/forest", {{
                    { 0x24, 0x3D, 0x31 }, { 0x30, 0x55, 0x40 }, { 0x3E, 0x72, 0x52 },
                    { 0x0D, 0x17, 0x14 }, { 0x1D, 0x34, 0x29 }
                }});
                registerPaletteControls(g_resources.auroraSteel, "__agui_round/aurora", {{
                    { 0x34, 0x3F, 0x4A }, { 0x41, 0x55, 0x61 }, { 0x53, 0x72, 0x7B },
                    { 0x18, 0x1E, 0x24 }, { 0x2A, 0x36, 0x3F }
                }});

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

        [[nodiscard]] static bool backend_upload_complete(
            const UploadState& state) noexcept
        {
            return (!g_resources.atlas || state.guiAtlasUploaded)
                && (!g_resources.font.atlas || state.fontAtlasUploaded);
        }

        static void ensure_backend_upload(Context& ctx)
        {
            ensure_resources();

            if (auto current = core::get_current_render_context(); current && current.get() == &ctx)
            {
                perform_backend_upload(ctx);
                return;
            }

            if (ctx.windowData)
            {
                {
                    std::scoped_lock lock(g_uploadMutex);
                    auto& state = g_uploadedContexts[&ctx];
                    if (backend_upload_complete(state) || state.uploadQueued)
                        return;
                    state.uploadQueued = true;
                }

                auto shared = ctx.windowData->context
                    ? std::reinterpret_pointer_cast<Context>(ctx.windowData->context)
                    : std::shared_ptr<Context>{};
                if (!shared)
                {
                    std::scoped_lock lock(g_uploadMutex);
                    if (auto it = g_uploadedContexts.find(&ctx); it != g_uploadedContexts.end())
                        it->second.uploadQueued = false;
                    return;
                }

                const core::RenderPath renderPath = render_path_for_context(&ctx);
                try
                {
                    ctx.windowData->commandQueue.enqueue([shared = std::move(shared)]()
                        {
                            try { perform_backend_upload(*shared); }
                            catch (...) { /* GUI optional */ }

                            std::scoped_lock lock(g_uploadMutex);
                            if (auto it = g_uploadedContexts.find(shared.get());
                                it != g_uploadedContexts.end())
                            {
                                it->second.uploadQueued = false;
                            }
                        }, renderPath);
                }
                catch (...)
                {
                    std::scoped_lock lock(g_uploadMutex);
                    if (auto it = g_uploadedContexts.find(&ctx); it != g_uploadedContexts.end())
                        it->second.uploadQueued = false;
                    throw;
                }
                return;
            }

            perform_backend_upload(ctx);
        }

        static void draw_sprite_raw(const SpriteHandle& handle, float x, float y, float w, float h)
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

            auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
            std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
            ctx->draw_sprite_safe(handle, span, x, y, w, h);
        }

        static void draw_rounded_sprite(
            const SpriteHandle& handle,
            float x,
            float y,
            float w,
            float h,
            const gui_lib::rounded_rect::RoundedRectStyle& requestedStyle)
        {
            const auto style = gui_lib::rounded_rect::normalize_rounded_rect_style(requestedStyle);
            const auto cornersIt = g_resources.roundedControls.find(handle);
            if (!style.enabled
                || cornersIt == g_resources.roundedControls.end()
                || !cornersIt->second.valid()
                || (std::min)(w, h) < 8.0f)
            {
                draw_sprite_raw(handle, x, y, w, h);
                return;
            }

            const RoundedCorners& corners = cornersIt->second;
            const float radius = (std::min)({
                style.control_radius,
                w * 0.5f,
                h * 0.5f
            });
            if (radius < 1.0f)
            {
                draw_sprite_raw(handle, x, y, w, h);
                return;
            }

            // Opaque slices overlap their neighboring corner by one physical
            // pixel. Fractional scaling otherwise exposes atlas texels as thin
            // seams along rounded button edges.
            const float overlap = (std::min)(1.0f, radius * 0.25f);
            draw_sprite_raw(
                handle,
                x + radius - overlap,
                y,
                w - radius * 2.0f + overlap * 2.0f,
                h);
            draw_sprite_raw(
                handle,
                x,
                y + radius - overlap,
                radius + overlap,
                h - radius * 2.0f + overlap * 2.0f);
            draw_sprite_raw(
                handle,
                x + w - radius - overlap,
                y + radius - overlap,
                radius + overlap,
                h - radius * 2.0f + overlap * 2.0f);
            draw_sprite_raw(corners.topLeft, x, y, radius, radius);
            draw_sprite_raw(corners.topRight, x + w - radius, y, radius, radius);
            draw_sprite_raw(corners.bottomRight, x + w - radius, y + h - radius, radius, radius);
            draw_sprite_raw(corners.bottomLeft, x, y + h - radius, radius, radius);
        }

        static void draw_sprite(const SpriteHandle& handle, float x, float y, float w, float h)
        {
            draw_rounded_sprite(handle, x, y, w, h, g_frame.roundedRectStyle);
        }

        [[nodiscard]] static bool point_in_rect(Vec2 p, float x, float y, float w, float h) noexcept
        {
            return (p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h);
        }

        [[nodiscard]] static bool point_in_modal_input_capture(Vec2 p) noexcept
        {
            return g_frame.modalInput.pointer_allowed({ p.x, p.y });
        }

        [[nodiscard]] static bool point_in_active_clip(Vec2 p) noexcept
        {
            if (!point_in_modal_input_capture(p))
                return false;

            return !has_content_clip()
                || point_in_rect(
                    p,
                    g_frame.contentMin.x,
                    g_frame.contentMin.y,
                    g_frame.contentMax.x - g_frame.contentMin.x,
                    g_frame.contentMax.y - g_frame.contentMin.y);
        }

        [[nodiscard]] static bool left_press_available() noexcept
        {
            return g_frame.justPressed
                && !g_frame.mousePressConsumed
                && point_in_modal_input_capture(g_frame.mousePos);
        }

        [[nodiscard]] static bool left_release_available() noexcept
        {
            return g_frame.justReleased
                && !g_frame.mouseReleaseConsumed
                && point_in_modal_input_capture(g_frame.mousePos);
        }

        [[nodiscard]] static bool right_press_available() noexcept
        {
            return g_frame.rightJustPressed
                && !g_frame.rightPressConsumed
                && point_in_modal_input_capture(g_frame.mousePos);
        }

        [[nodiscard]] static bool right_release_available() noexcept
        {
            return g_frame.rightJustReleased
                && !g_frame.rightReleaseConsumed
                && point_in_modal_input_capture(g_frame.mousePos);
        }

        static void consume_left_press() noexcept
        {
            if (g_frame.justPressed)
                g_frame.mousePressConsumed = true;
        }

        static void consume_left_release() noexcept
        {
            if (g_frame.justReleased)
                g_frame.mouseReleaseConsumed = true;
        }

        static void consume_right_press() noexcept
        {
            if (g_frame.rightJustPressed)
                g_frame.rightPressConsumed = true;
        }

        static void consume_right_release() noexcept
        {
            if (g_frame.rightJustReleased)
                g_frame.rightReleaseConsumed = true;
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

        [[nodiscard]] static const void* widget_focus_key(
            std::string_view label,
            const void* owner,
            Vec2 pos,
            Vec2 size) noexcept
        {
            std::size_t h = widget_press_key(label, pos, size);
            const auto mix = [&h](std::uint64_t value) noexcept
            {
                h ^= static_cast<std::size_t>(value);
                h *= static_cast<std::size_t>(1099511628211ull);
            };
            mix(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(owner)));
            return reinterpret_cast<const void*>(h == 0 ? 1 : h);
        }

        [[nodiscard]] static float base_line_height(float scale) noexcept
        {
            const auto& metrics = active_font_cache().metrics;
            if (metrics.ascent > 0.0f || metrics.descent > 0.0f)
                return (metrics.ascent + metrics.descent) * scale + kGlyphRasterPadding;
            return 8.0f * scale + kGlyphRasterPadding;
        }

        [[nodiscard]] static float line_advance_amount(float scale) noexcept
        {
            const auto& metrics = active_font_cache().metrics;
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
            const auto& metrics = active_font_cache().metrics;
            if (metrics.ascent > 0.0f)
                return metrics.ascent * scale;
            return 8.0f * scale;
        }

        [[nodiscard]] static float space_advance(float scale) noexcept
        {
            const auto& fontCache = active_font_cache();
            if (fontCache.metrics.spaceAdvance > 0.0f)
                return fontCache.metrics.spaceAdvance * scale;
            if (const auto* glyph = fontCache.glyphLookup[static_cast<unsigned char>(' ')])
                return glyph->advance * scale;
            if (fontCache.metrics.averageAdvance > 0.0f)
                return fontCache.metrics.averageAdvance * scale;
            return 8.0f * scale;
        }

        [[nodiscard]] static float letter_spacing(float scale) noexcept
        {
            const float average = active_font_cache().metrics.averageAdvance;
            if (average <= 0.0f || kLetterSpacingFactor <= 0.0f)
                return 0.0f;
            return average * scale * kLetterSpacingFactor;
        }

        [[nodiscard]] static const font::Glyph* lookup_glyph(char32_t codepoint) noexcept
        {
            const auto& fontCache = active_font_cache();
            if (!fontCache.asset)
                return nullptr;

            const auto glyphIndex = static_cast<std::size_t>(codepoint);
            if (glyphIndex < fontCache.glyphLookup.size())
            {
                if (const auto* glyph = fontCache.glyphLookup[glyphIndex])
                    return glyph;
            }

            const auto found = fontCache.asset->glyphs.find(codepoint);
            if (found != fontCache.asset->glyphs.end())
                return &found->second;
            return fontCache.fallbackGlyph;
        }

        [[nodiscard]] static char32_t safe_draw_codepoint(char32_t codepoint) noexcept
        {
            if (codepoint == U'\t' || codepoint == U'\n')
                return codepoint;
            if (codepoint < 32u
                || (codepoint >= 0x7Fu && codepoint < 0xA0u))
            {
                return U' ';
            }

            if (codepoint > 0x10FFFFu
                || (codepoint >= 0xD800u && codepoint <= 0xDFFFu))
            {
                return U'?';
            }
            return codepoint;
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

        struct DecodedUtf8Codepoint
        {
            char32_t codepoint{U'?'};
            std::size_t byte_count{1u};
        };

        [[nodiscard]] static DecodedUtf8Codepoint decode_utf8_codepoint(
            std::string_view text,
            std::size_t index) noexcept
        {
            if (index >= text.size())
                return DecodedUtf8Codepoint{U'\0', 0u};

            const auto byte = [&](std::size_t offset) noexcept
                {
                    return static_cast<unsigned char>(text[index + offset]);
                };
            const unsigned char first = byte(0u);
            if (first < 0x80u)
                return DecodedUtf8Codepoint{
                    static_cast<char32_t>(first), 1u};

            if (first >= 0xC2u && first <= 0xDFu
                && index + 1u < text.size()
                && is_utf8_continuation_byte(byte(1u)))
            {
                return DecodedUtf8Codepoint{
                    static_cast<char32_t>(
                        ((first & 0x1Fu) << 6u)
                        | (byte(1u) & 0x3Fu)),
                    2u};
            }

            if (first >= 0xE0u && first <= 0xEFu
                && index + 2u < text.size()
                && is_utf8_continuation_byte(byte(1u))
                && is_utf8_continuation_byte(byte(2u))
                && (first != 0xE0u || byte(1u) >= 0xA0u)
                && (first != 0xEDu || byte(1u) <= 0x9Fu))
            {
                return DecodedUtf8Codepoint{
                    static_cast<char32_t>(
                        ((first & 0x0Fu) << 12u)
                        | ((byte(1u) & 0x3Fu) << 6u)
                        | (byte(2u) & 0x3Fu)),
                    3u};
            }

            if (first >= 0xF0u && first <= 0xF4u
                && index + 3u < text.size()
                && is_utf8_continuation_byte(byte(1u))
                && is_utf8_continuation_byte(byte(2u))
                && is_utf8_continuation_byte(byte(3u))
                && (first != 0xF0u || byte(1u) >= 0x90u)
                && (first != 0xF4u || byte(1u) <= 0x8Fu))
            {
                return DecodedUtf8Codepoint{
                    static_cast<char32_t>(
                        ((first & 0x07u) << 18u)
                        | ((byte(1u) & 0x3Fu) << 12u)
                        | ((byte(2u) & 0x3Fu) << 6u)
                        | (byte(3u) & 0x3Fu)),
                    4u};
            }

            return DecodedUtf8Codepoint{};
        }

        [[nodiscard]] static std::size_t previous_utf8_codepoint_start(
            std::string_view text,
            std::size_t index) noexcept
        {
            index = (std::min)(index, text.size());
            if (index == 0u)
                return 0u;

            --index;
            while (index > 0u
                && is_utf8_continuation_byte(
                    static_cast<unsigned char>(text[index])))
            {
                --index;
            }
            return index;
        }

        [[nodiscard]] static std::size_t next_utf8_codepoint_start(
            std::string_view text,
            std::size_t index) noexcept
        {
            index = (std::min)(index, text.size());
            if (index >= text.size())
                return text.size();

            const DecodedUtf8Codepoint decoded =
                decode_utf8_codepoint(text, index);
            return (std::min)(
                index + (std::max)(
                    std::size_t{1u}, decoded.byte_count),
                text.size());
        }
        [[nodiscard]] static std::optional<char32_t> next_drawable_char(
            std::string_view text,
            std::size_t index) noexcept
        {
            if (index >= text.size())
                return std::nullopt;

            const DecodedUtf8Codepoint current =
                decode_utf8_codepoint(text, index);
            const std::size_t nextIndex =
                index + (std::max)(std::size_t{1u}, current.byte_count);
            if (nextIndex >= text.size())
                return std::nullopt;

            const char32_t next = safe_draw_codepoint(
                decode_utf8_codepoint(text, nextIndex).codepoint);
            if (next == U'\n')
                return std::nullopt;
            return next;
        }

        [[nodiscard]] static float kerning_adjust(
            char32_t left,
            char32_t right,
            float scale) noexcept
        {
            const auto& fontCache = active_font_cache();
            if (!fontCache.asset)
                return 0.0f;

            const float kern = fontCache.asset->get_kerning(left, right);
            if (kern == 0.0f)
                return 0.0f;
            return kern * scale;
        }

        [[nodiscard]] static float glyph_advance(
            char32_t codepoint,
            float scale) noexcept
        {
            if (codepoint == U'\t')
                return space_advance(scale) * static_cast<float>(kTabSpaces);

            if (const auto* glyph = lookup_glyph(codepoint))
                return glyph->advance * scale;

            if (active_font_cache().metrics.averageAdvance > 0.0f)
                return active_font_cache().metrics.averageAdvance * scale;

            return 8.0f * scale;
        }

        [[nodiscard]] static float glyph_advance_with_kerning(
            char32_t codepoint,
            std::optional<char32_t> next,
            float scale) noexcept
        {
            codepoint = safe_draw_codepoint(codepoint);
            if (next)
                next = safe_draw_codepoint(*next);

            float advance = glyph_advance(codepoint, scale);
            if (next)
                advance += kerning_adjust(codepoint, *next, scale);
            if (codepoint != U' ' && codepoint != U'\t')
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

        [[nodiscard]] static bool glyph_intersects_clip(
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

            constexpr float intersectionEpsilon = 0.01f;
            return x + w > clipLeft - intersectionEpsilon
                && y + h > clipTop - intersectionEpsilon
                && x < clipRight + intersectionEpsilon
                && y < clipBottom + intersectionEpsilon;
        }

        [[nodiscard]] static constexpr float align_leading_glyph_to_clip(
            float penX,
            float lineStartX,
            float glyphX,
            float clipLeft) noexcept
        {
            constexpr float lineStartEpsilon = 0.01f;
            if (penX > lineStartX + lineStartEpsilon || glyphX >= clipLeft)
                return penX;

            return penX + (clipLeft - glyphX);
        }

        [[nodiscard]] static float measure_text_width(std::string_view text, float scale) noexcept
        {
            float current = 0.0f;
            float maxWidth = 0.0f;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const std::size_t byteIndex = i;
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, byteIndex);
                const char32_t ch =
                    safe_draw_codepoint(decoded.codepoint);
                i += (std::max)(std::size_t{1u}, decoded.byte_count) - 1u;
                if (ch == '\n')
                {
                    maxWidth = (std::max)(maxWidth, current);
                    current = 0.0f;
                    continue;
                }
                const auto next = next_drawable_char(text, byteIndex);
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
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, i);
                const std::size_t byteCount =
                    (std::max)(std::size_t{1u}, decoded.byte_count);
                const std::string_view codepointBytes =
                    text.substr(i, byteCount);
                const std::string candidate =
                    trimmed + std::string(codepointBytes)
                    + std::string(kEllipsis);
                if (measure_text_width(candidate, scale) > maxWidth)
                    break;
                trimmed.append(codepointBytes);
                i += byteCount - 1u;
            }

            if (trimmed.empty())
                return std::string(kEllipsis);

            trimmed += kEllipsis;
            return trimmed;
        }

        [[nodiscard]] static bool is_wrap_space(char32_t codepoint) noexcept
        {
            return codepoint == U' ' || codepoint == U'\t';
        }

        [[nodiscard]] static float measure_word_advance(std::string_view text, std::size_t start, float scale) noexcept
        {
            float advance = 0.0f;

            for (std::size_t i = start; i < text.size(); ++i)
            {
                const std::size_t byteIndex = i;
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, byteIndex);
                const char32_t ch =
                    safe_draw_codepoint(decoded.codepoint);
                i += (std::max)(std::size_t{1u}, decoded.byte_count) - 1u;
                if (ch == '\n' || is_wrap_space(ch))
                    break;

                const auto next = next_drawable_char(text, byteIndex);
                advance += glyph_advance_with_kerning(ch, next, scale);
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
                const std::size_t byteIndex = i;
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, byteIndex);
                const char32_t ch =
                    safe_draw_codepoint(decoded.codepoint);
                i += (std::max)(std::size_t{1u}, decoded.byte_count) - 1u;
                if (ch == '\n')
                {
                    ++lines;
                    penX = 0.0f;
                    continue;
                }

                if (is_wrap_space(ch))
                {
                    std::size_t runEnd = byteIndex;
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

                if ((byteIndex == 0 || text[byteIndex - 1] == '\n' || is_wrap_space(text[byteIndex - 1])) && penX > 0.0f)
                {
                    const float wordAdvance = measure_word_advance(text, byteIndex, scale);
                    if (wordAdvance > 0.0f && penX + wordAdvance > effectiveWidth + 0.001f)
                    {
                        ++lines;
                        penX = 0.0f;
                    }
                }

                const auto next = next_drawable_char(text, byteIndex);
                const float advance = glyph_advance_with_kerning(ch, next, scale);
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
                const std::size_t byteIndex = i;
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, byteIndex);
                const char32_t ch =
                    safe_draw_codepoint(decoded.codepoint);
                i += (std::max)(std::size_t{1u}, decoded.byte_count) - 1u;
                if (ch == '\n')
                {
                    penX = x;
                    baseline += lineAdvance;
                    continue;
                }

                if (is_wrap_space(ch))
                {
                    std::size_t runEnd = byteIndex;
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

                if ((byteIndex == 0 || text[byteIndex - 1] == '\n' || is_wrap_space(text[byteIndex - 1])) && penX > x)
                {
                    const float wordAdvance = measure_word_advance(text, byteIndex, scale);
                    if (wordAdvance > 0.0f && penX - x + wordAdvance > effectiveWidth + 0.001f)
                    {
                        penX = x;
                        baseline += lineAdvance;
                    }
                }

                const auto next = next_drawable_char(text, byteIndex);
                const float advance = glyph_advance_with_kerning(ch, next, scale);
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
            if (!g_frame.ctx || !active_font_cache().asset)
                return 0.0f;

            const float clipLeft = has_content_clip() ? g_frame.contentMin.x : x;
            const float clipTop = has_content_clip() ? g_frame.contentMin.y : y;
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
                const std::size_t byteIndex = i;
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, byteIndex);
                const char32_t ch =
                    safe_draw_codepoint(decoded.codepoint);
                i += (std::max)(std::size_t{1u}, decoded.byte_count) - 1u;
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
                    std::size_t runEnd = byteIndex;
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

                if ((byteIndex == 0 || text[byteIndex - 1] == '\n' || is_wrap_space(text[byteIndex - 1])) && penX > x)
                {
                    const float wordAdvance = measure_word_advance(text, byteIndex, scale);
                    if (wordAdvance > 0.0f && penX - x + wordAdvance > effectiveWidth + 0.001f)
                    {
                        penX = x;
                        baseline += lineAdvance;
                        ++lines;
                        if (baseline - ascent > clipBottom)
                            break;
                    }
                }

                const auto next = next_drawable_char(text, byteIndex);
                const float advance = glyph_advance_with_kerning(ch, next, scale);
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
                    if (const auto* glyph = lookup_glyph(ch))
                    {
                        const float drawW = glyph->size_px.x * scale;
                        const float drawH = glyph->size_px.y * scale;
                        if (glyph->handle.is_valid() && glyph_rect_is_reasonable(drawW, drawH, scale))
                        {
                            const float offsetX = glyph->offset_px.x * scale;
                            const float offsetY = glyph->offset_px.y * scale;
                            penX = align_leading_glyph_to_clip(
                                penX,
                                x,
                                penX + offsetX,
                                clipLeft);
                            const float drawX = penX + offsetX;
                            const float drawY = baseline + offsetY;
                            if (glyph_intersects_clip(drawX, drawY, drawW, drawH, clipLeft, clipTop, clipRight, clipBottom))
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
            if (!g_frame.ctx || !active_font_cache().asset)
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
                const std::size_t byteIndex = i;
                const DecodedUtf8Codepoint decoded =
                    decode_utf8_codepoint(text, byteIndex);
                const char32_t ch =
                    safe_draw_codepoint(decoded.codepoint);
                i += (std::max)(std::size_t{1u}, decoded.byte_count) - 1u;
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
                        penX = align_leading_glyph_to_clip(
                            penX,
                            anchorX,
                            penX + offsetX,
                            clipLeft);
                        const float drawX = penX + offsetX;
                        const float drawY = baseline + offsetY;
                        const bool glyphVisible = glyph_intersects_clip(
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

                const auto next = next_drawable_char(text, byteIndex);
                penX += glyph_advance_with_kerning(ch, next, scale);
                if (penX > clipRight)
                    return;
            }
        }

        static void draw_caret(float x, float y, float height)
        {
            if (!g_frame.caretVisible || height <= 0.0f)
                return;

            const float caretWidth = (std::max)(2.0f, space_advance(kFontScale) * 0.12f);
            draw_sprite_raw(active_palette().buttonActive, x, y, caretWidth, height);
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
            g_frame.modalInput.clear();
            g_frame.mousePressConsumed = false;
            g_frame.mouseReleaseConsumed = false;
            g_frame.rightPressConsumed = false;
            g_frame.rightReleaseConsumed = false;
            g_frame.activeTheme = ThemeVariant::DefaultDark;
            g_frame.roundedRectStyle = {};
            g_frame.themeStack.clear();
            g_frame.roundedRectStyleStack.clear();
            g_frame.pendingSelectPopups.clear();
            g_scrollAreaStack.clear();
            g_floatingWindowTopLayerStack.clear();
            g_windowFrameStack.clear();
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
                auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
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

                    auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
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
        queue_input_event(g_pendingEvents, e);
    }

    void push_input_for_context(const core::Context* ctx, const InputEvent& e) noexcept
    {
        if (!ctx)
        {
            push_input(e);
            return;
        }

        std::scoped_lock lock(g_contextPendingEventsMutex);
        try
        {
            queue_input_event(g_contextPendingEvents[ctx], e);
        }
        catch (...)
        {
            // Creating the per-context bucket may allocate. Dropping one
            // transient event is safer than terminating the native pump.
        }
    }

    int consume_mouse_wheel_delta() noexcept
    {
        if (!point_in_modal_input_capture(g_frame.mousePos))
        {
            g_frame.mouseWheelDelta = 0;
            return 0;
        }

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
        g_contextRightMouseDownStates.erase(ctx);
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
        for (auto it = g_sourceEditorStates.begin(); it != g_sourceEditorStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_sourceEditorStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_assetGridContextMenuStates.begin();
             it != g_assetGridContextMenuStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_assetGridContextMenuStates.erase(it);
            else
                ++it;
        }
        for (auto it = g_nodeGraphCanvasStates.begin();
             it != g_nodeGraphCanvasStates.end();)
        {
            if (it->first.starts_with(scrollPrefix))
                it = g_nodeGraphCanvasStates.erase(it);
            else
                ++it;
        }
    }

    void refresh_context_resources(const core::Context* ctx) noexcept
    {
        forget_upload_state(ctx);
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

    std::uint64_t top_layer_batch_generation(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return 0;

        std::scoped_lock lock(g_deferredBatchMutex);
        const auto it = g_topLayerDrawBatches.find(ctx);
        if (it == g_topLayerDrawBatches.end())
            return 0;

        return it->second.generation;
    }

    std::uint64_t replayed_top_layer_batch_generation(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return 0;

        std::scoped_lock lock(g_deferredBatchMutex);
        const auto it = g_topLayerDrawBatches.find(ctx);
        if (it == g_topLayerDrawBatches.end())
            return 0;

        return it->second.replayedGeneration;
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

        auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
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
        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_topLayerDrawBatches.find(ctx);
            if (it == g_topLayerDrawBatches.end())
                return false;
            draws = it->second.draws;
            generation = it->second.generation;
        }

        if (!draws || draws->empty())
            return false;

        auto atlases = epochengine::atlasmanager::get_atlas_vector_snapshot();
        std::span<const TextureAtlas* const> span(atlases.data(), atlases.size());
        for (const auto& draw : *draws)
            ctx->draw_sprite_safe(draw.handle, span, draw.x, draw.y, draw.w, draw.h);
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_topLayerDrawBatches.find(ctx);
            if (it != g_topLayerDrawBatches.end())
            {
                it->second.replayedGeneration = (std::max)(it->second.replayedGeneration, generation);
            }
        }
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
        bool prevMouseRightDown = g_frame.mouseRightDown;
        if (rawCtx)
        {
            if (const auto it = g_contextMouseDownStates.find(rawCtx); it != g_contextMouseDownStates.end())
                prevMouseDown = it->second;
            if (const auto it = g_contextRightMouseDownStates.find(rawCtx); it != g_contextRightMouseDownStates.end())
                prevMouseRightDown = it->second;
        }
        bool currentMouseDown = mouse_down;
        bool currentMouseRightDown = prevMouseRightDown;
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
            case EventType::MouseDown:
                if (evt.mouse_button == 1)
                    currentMouseRightDown = true;
                else
                    currentMouseDown = true;
                currentMousePos = evt.mouse_pos;
                break;
            case EventType::MouseUp:
                if (evt.mouse_button == 1)
                    currentMouseRightDown = false;
                else
                    currentMouseDown = false;
                currentMousePos = evt.mouse_pos;
                break;
            case EventType::MouseWheel:
                currentMousePos = evt.mouse_pos;
                g_frame.mouseWheelDelta += evt.wheel_delta;
                break;
            default: break;
            }
        }

        g_frame.prevMouseDown = prevMouseDown;
        g_frame.mouseDown = currentMouseDown;
        g_frame.prevMouseRightDown = prevMouseRightDown;
        g_frame.mouseRightDown = currentMouseRightDown;
        g_frame.mousePos = currentMousePos;
        g_frame.justPressed = (!prevMouseDown && currentMouseDown);
        g_frame.justReleased = (prevMouseDown && !currentMouseDown);
        g_frame.rightJustPressed = (!prevMouseRightDown && currentMouseRightDown);
        g_frame.rightJustReleased = (prevMouseRightDown && !currentMouseRightDown);
        g_frame.queuedDraws.clear();
        g_frame.topLayerDraws.clear();
        g_frame.pendingSelectPopups.clear();
        g_frame.topLayerDepth = 0;

        if (rawCtx)
        {
            if (g_frame.justPressed)
                g_contextPressedButtonKeys[rawCtx] = 0;
            g_contextMouseDownStates[rawCtx] = currentMouseDown;
            g_contextRightMouseDownStates[rawCtx] = currentMouseRightDown;
        }

        reset_frame();
    }

    static void render_pending_select_popups() noexcept;

    void end_frame() noexcept
    {
        render_pending_select_popups();
        flush_queued_draws();
        g_frame.ctxShared.reset();
        g_frame.ctx = nullptr;
        g_frame.insideWindow = false;
        g_frame.events.clear();
        g_frame.justPressed = false;
        g_frame.justReleased = false;
        g_frame.rightJustPressed = false;
        g_frame.rightJustReleased = false;
        g_frame.mousePressConsumed = false;
        g_frame.mouseReleaseConsumed = false;
        g_frame.rightPressConsumed = false;
        g_frame.rightReleaseConsumed = false;
        g_frame.modalInput.clear();
        g_frame.mouseWheelDelta = 0;
        g_frame.pendingSelectPopups.clear();
        g_frame.topLayerDepth = 0;
    }

    Vec2 mouse_position() noexcept
    {
        return g_frame.mousePos;
    }

    bool is_mouse_down() noexcept
    {
        return g_frame.mouseDown && point_in_modal_input_capture(g_frame.mousePos);
    }

    DragSurfaceResult drag_surface(
        DragSurfaceState& state,
        const DragSurfaceOptions& options) noexcept
    {
        DragSurfaceResult result{};
        if (!g_frame.ctx || !options.enabled
            || options.size.x <= 0.0f || options.size.y <= 0.0f)
        {
            if (!g_frame.mouseDown)
                state = {};
            return result;
        }

        const float padding = (std::max)(0.0f, options.hit_padding);
        const float minimumExtent = (std::max)(1.0f, options.minimum_hit_extent);
        const float hitWidth = (std::max)(
            options.size.x + padding * 2.0f,
            minimumExtent);
        const float hitHeight = (std::max)(
            options.size.y + padding * 2.0f,
            minimumExtent);
        const Vec2 hitPosition{
            options.position.x - (hitWidth - options.size.x) * 0.5f,
            options.position.y - (hitHeight - options.size.y) * 0.5f};
        result.hovered = point_in_rect(
                g_frame.mousePos,
                hitPosition.x,
                hitPosition.y,
                hitWidth,
                hitHeight)
            && point_in_active_clip(g_frame.mousePos);

        if (!state.pending && result.hovered && left_press_available())
        {
            state.press_position = g_frame.mousePos;
            state.pending = true;
            state.dragging = false;
            result.pressed = true;
        }

        if (!state.pending)
            return result;

        result.delta = {
            g_frame.mousePos.x - state.press_position.x,
            g_frame.mousePos.y - state.press_position.y};
        const float threshold = (std::max)(0.0f, options.drag_threshold);
        if (g_frame.mouseDown && !state.dragging
            && result.delta.x * result.delta.x
                + result.delta.y * result.delta.y
                >= threshold * threshold)
        {
            state.dragging = true;
        }
        result.dragging = state.dragging && g_frame.mouseDown;
        if (result.dragging)
            g_frame.mousePressConsumed = true;

        if (g_frame.justReleased)
        {
            result.released = state.dragging;
            if (result.released && left_release_available())
                consume_left_release();
            state = {};
        }
        else if (!g_frame.mouseDown)
        {
            state = {};
        }
        return result;
    }

    bool was_mouse_pressed() noexcept
    {
        return g_frame.justPressed && point_in_modal_input_capture(g_frame.mousePos);
    }

    bool was_mouse_released() noexcept
    {
        return g_frame.justReleased && point_in_modal_input_capture(g_frame.mousePos);
    }

    bool is_mouse_right_down() noexcept
    {
        return g_frame.mouseRightDown && point_in_modal_input_capture(g_frame.mousePos);
    }

    bool was_mouse_right_pressed() noexcept
    {
        return g_frame.rightJustPressed && point_in_modal_input_capture(g_frame.mousePos);
    }

    bool was_mouse_right_released() noexcept
    {
        return g_frame.rightJustReleased && point_in_modal_input_capture(g_frame.mousePos);
    }
    bool keyboard_input_captured() noexcept
    {
        // A modal owns the complete interaction channel, not only pointer
        // coordinates inside its panel. Editor camera/navigation shortcuts
        // must not leak through while a modal text field is idle.
        if (g_frame.modalInput.keyboard_captured())
            return true;
        if (!g_frame.ctx)
            return any_select_box_open();
        const void* contextKey = static_cast<const void*>(g_frame.ctx);
        const auto active = g_contextActiveWidgets.find(contextKey);
        return (active != g_contextActiveWidgets.end() && active->second != nullptr)
            || any_select_box_open();
    }

    void begin_modal_input_capture(Vec2 position, Vec2 size) noexcept
    {
        g_frame.modalInput.begin({
            { position.x, position.y },
            { size.x, size.y }
        });
    }

    void block_input_until_clear() noexcept
    {
        g_frame.modalInput.block_until_clear();
    }

    void clear_modal_input_capture() noexcept
    {
        g_frame.modalInput.clear();
    }

    std::span<const ThemePreferenceChoice> theme_preference_choices() noexcept
    {
        static constexpr std::array<ThemePreferenceChoice, 8> kChoices{ {
            { "System Light/Dark", ThemePreference::FollowSystemDark },
            { "Light", ThemePreference::Light },
            { "Dark", ThemePreference::Dark },
            { "Classic Launcher", ThemePreference::ClassicLauncher },
            { "Midnight Blue", ThemePreference::MidnightBlue },
            { "Ember Forge", ThemePreference::EmberForge },
            { "Forest Terminal", ThemePreference::ForestTerminal },
            { "Aurora Steel", ThemePreference::AuroraSteel }
        } };
        return { kChoices.data(), kChoices.size() };
    }

    std::string_view theme_preference_label(ThemePreference preference) noexcept
    {
        for (const auto& choice : theme_preference_choices())
        {
            if (choice.preference == preference)
                return choice.label;
        }
        if (preference == ThemePreference::ProfessionalDark)
            return "Dark";
        return "System Light/Dark";
    }

    ThemeVariant resolve_theme_preference(ThemePreference preference) noexcept
    {
        switch (preference)
        {
        case ThemePreference::Light:
            return ThemeVariant::DefaultLight;
        case ThemePreference::Dark:
        case ThemePreference::ProfessionalDark:
            return ThemeVariant::DefaultDark;
        case ThemePreference::ClassicLauncher:
            return ThemeVariant::ClassicLauncher;
        case ThemePreference::MidnightBlue:
            return ThemeVariant::MidnightBlue;
        case ThemePreference::EmberForge:
            return ThemeVariant::EmberForge;
        case ThemePreference::ForestTerminal:
            return ThemeVariant::ForestTerminal;
        case ThemePreference::AuroraSteel:
            return ThemeVariant::AuroraSteel;
        case ThemePreference::FollowSystemDark:
            return system_prefers_dark_palette()
                ? ThemeVariant::DefaultDark
                : ThemeVariant::DefaultLight;
        default:
            return ThemeVariant::DefaultDark;
        }
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

    bool rounded_rectangles_enabled() noexcept
    {
        return g_frame.roundedRectStyle.enabled;
    }

    void push_rounded_rectangles(bool enabled) noexcept
    {
        g_frame.roundedRectStyleStack.push_back(g_frame.roundedRectStyle);
        g_frame.roundedRectStyle = gui_lib::rounded_rect::normalize_rounded_rect_style({
            .enabled = enabled,
            .control_radius = 6.0f,
            .segments_per_corner = 4U
        });
    }

    void pop_rounded_rectangles() noexcept
    {
        if (!g_frame.roundedRectStyleStack.empty())
        {
            g_frame.roundedRectStyle = g_frame.roundedRectStyleStack.back();
            g_frame.roundedRectStyleStack.pop_back();
        }
        else
        {
            g_frame.roundedRectStyle = {};
        }
    }

    ScopedRoundedRectangles::ScopedRoundedRectangles(bool enabled) noexcept
    {
        push_rounded_rectangles(enabled);
    }

    ScopedRoundedRectangles::~ScopedRoundedRectangles() noexcept
    {
        if (active_)
            pop_rounded_rectangles();
    }

    ScopedTheme::ScopedTheme(ThemeVariant theme) noexcept
    {
        push_theme(theme);
    }

    ScopedTheme::ScopedTheme(ThemePreference preference) noexcept
        : ScopedTheme(resolve_theme_preference(preference))
    {
    }

    ScopedTheme::~ScopedTheme() noexcept
    {
        if (active_)
            pop_theme();
    }

    static void render_select_popup(const PendingSelectPopup& popup) noexcept
    {
        if (!g_frame.ctx || popup.options.empty())
            return;
        if (!std::isfinite(popup.position.x) || !std::isfinite(popup.position.y)
            || !std::isfinite(popup.width) || !std::isfinite(popup.height)
            || popup.width <= 0.0f || popup.height <= 0.0f)
        {
            return;
        }

        auto& scrollState = g_scrollAreaStates[popup.scrollKey];
        scrollState.contentHeight = (std::max)(popup.height, popup.contentHeight);
        const float maxScroll = (std::max)(0.0f, scrollState.contentHeight - popup.height);
        if (!std::isfinite(scrollState.scrollY))
            scrollState.scrollY = 0.0f;
        scrollState.scrollY = (std::clamp)(scrollState.scrollY, 0.0f, maxScroll);

        const bool showScrollbar = popup.showScrollbar && scrollState.contentHeight > popup.height + 1.0f;
        const float scrollbarWidth = showScrollbar ? 10.0f : 0.0f;
        const float contentWidth = (std::max)(1.0f, popup.width - scrollbarWidth - 2.0f);
        const auto& palette = active_palette();

        draw_sprite(palette.consoleBackground, popup.position.x, popup.position.y, popup.width, popup.height);

        {
            ContentClipScope clip(
                popup.position,
                { popup.position.x + contentWidth, popup.position.y + popup.height });

            for (std::size_t i = 0; i < popup.options.size(); ++i)
            {
                const float rowY = popup.position.y - scrollState.scrollY + static_cast<float>(i) * popup.optionPitch;
                if (rowY + popup.rowHeight < popup.position.y || rowY > popup.position.y + popup.height)
                    continue;

                const bool active = popup.options[i] == popup.selected;
                const SpriteHandle background = active ? palette.buttonActive : palette.buttonNormal;
                draw_sprite(background, popup.position.x, rowY, contentWidth, popup.rowHeight);
                if (active)
                {
                    draw_sprite(palette.textFieldActive, popup.position.x, rowY, 2.0f, popup.rowHeight);
                    draw_sprite(palette.textFieldActive, popup.position.x, rowY, contentWidth, 2.0f);
                }

                const std::string fitted = fit_text_to_width(
                    popup.options[i],
                    (std::max)(1.0f, contentWidth - 20.0f),
                    kFontScale);
                const std::string_view displayLabel = fitted.empty()
                    ? std::string_view{ popup.options[i] }
                    : std::string_view{ fitted };
                const float textY = rowY + std::floor((std::max)(0.0f, (popup.rowHeight - base_line_height(kFontScale)) * 0.5f)) + 1.0f;
                draw_text_line(displayLabel, popup.position.x + kContentPadding, textY, kFontScale);
            }
        }

        if (showScrollbar)
        {
            const float trackX = popup.position.x + popup.width - scrollbarWidth;
            const float trackY = popup.position.y;
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, popup.height);

            const float visibleRatio = popup.height / scrollState.contentHeight;
            const float thumbHeight = (std::min)(
                popup.height,
                (std::max)(18.0f, popup.height * visibleRatio));
            const float scrollRatio = maxScroll > 0.0f ? scrollState.scrollY / maxScroll : 0.0f;
            const float thumbY = trackY + (popup.height - thumbHeight) * scrollRatio;
            draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
        }
    }

    static void render_pending_select_popups() noexcept
    {
        if (g_frame.pendingSelectPopups.empty())
            return;

        begin_top_layer();
        {
            ContentClipClearScope clearClip;
            for (const PendingSelectPopup& popup : g_frame.pendingSelectPopups)
                render_select_popup(popup);
        }
        end_top_layer();

        g_frame.pendingSelectPopups.clear();
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

        g_windowFrameStack.push_back(WindowFrameState{
            .cursor = g_frame.cursor,
            .origin = g_frame.origin,
            .windowSize = g_frame.windowSize,
            .contentMin = g_frame.contentMin,
            .contentMax = g_frame.contentMax,
            .windowKey = g_frame.windowKey,
            .widgetSerial = g_frame.widgetSerial,
            .scrollAreaDepth = g_scrollAreaStack.size(),
            .insideWindow = g_frame.insideWindow
        });

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
        if (g_windowFrameStack.empty())
        {
            g_frame.insideWindow = false;
            g_frame.contentMin = {};
            g_frame.contentMax = {};
            g_frame.windowKey.clear();
            g_frame.widgetSerial = 0;
            return;
        }

        const WindowFrameState previous = std::move(g_windowFrameStack.back());
        g_windowFrameStack.pop_back();
        while (g_scrollAreaStack.size() > previous.scrollAreaDepth)
        {
            const std::string leakedKey =
                std::move(g_scrollAreaStack.back().key);
            g_scrollAreaStack.pop_back();
            if (auto state = g_scrollAreaStates.find(leakedKey);
                state != g_scrollAreaStates.end())
            {
                state->second.draggingScrollbar = false;
            }
        }
        g_frame.cursor = previous.cursor;
        g_frame.origin = previous.origin;
        g_frame.windowSize = previous.windowSize;
        g_frame.contentMin = previous.contentMin;
        g_frame.contentMax = previous.contentMax;
        g_frame.windowKey = previous.windowKey;
        g_frame.widgetSerial = previous.widgetSerial;
        g_frame.insideWindow = previous.insideWindow;
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

        begin_modal_input_capture(options.position, options.size);
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

    FloatingWindowResult begin_floating_window(
        FloatingWindowState& state,
        const FloatingWindowOptions& options) noexcept
    {
        FloatingWindowResult result{};
        if (!g_frame.ctx || !state.open)
            return result;

        try { ensure_resources(); }
        catch (...) { return result; }

        const float titleHeight = line_advance_amount(kTitleScale) + 2.0f * kTitleBarPadding;
        gui_lib::FloatingWindowState coreState = to_lib(state);
        const bool wasInteracting = coreState.dragging || coreState.resizing || coreState.close_pressed;
        const gui_lib::FloatingWindowOptions coreOptions{
            .default_position = to_lib(options.default_position),
            .default_size = to_lib(options.default_size),
            .min_size = to_lib(options.min_size),
            .viewport_size = to_lib(options.viewport_size),
            .title_bar_height = titleHeight,
            .content_padding = kContentPadding,
            .movable = options.movable,
            .resizable = options.resizable,
            .closable = options.closable
        };
        const gui_lib::FloatingWindowInput coreInput{
            .mouse_position = to_lib(g_frame.mousePos),
            .mouse_down = g_frame.mouseDown,
            .mouse_pressed = left_press_available(),
            .mouse_released = left_release_available()
        };

        const gui_lib::FloatingWindowLayout layout =
            gui_lib::update_floating_window(coreState, coreOptions, coreInput);
        from_lib(coreState, state);

        result.focused = layout.focused;
        result.hovered = layout.hovered;
        result.moved = layout.moved;
        result.resized = layout.resized;
        result.close_requested = layout.close_requested;
        result.title_hovered = layout.title_hovered;
        result.close_hovered = layout.close_hovered;
        result.resize_hovered = layout.resize_hovered;
        result.window = from_lib(layout.window);
        result.title_bar = from_lib(layout.title_bar);
        result.content = from_lib(layout.content);
        result.close_button = from_lib(layout.close_button);
        result.resize_handle = from_lib(layout.resize_handle);

        if (coreInput.mouse_pressed && layout.hovered)
            consume_left_press();
        if (coreInput.mouse_released && (wasInteracting || layout.hovered || layout.close_requested))
            consume_left_release();
        if (layout.close_requested || !state.open)
            return result;

        if (options.capture_input && !g_frame.modalInput.active())
            begin_modal_input_capture(state.position, state.size);

        if (options.top_layer)
            begin_top_layer();
        g_floatingWindowTopLayerStack.push_back(options.top_layer);

        begin_window(options.title, state.position, state.size, options.draw_background);
        if (!options.id.empty())
            g_frame.windowKey.assign(options.id.begin(), options.id.end());

        const auto& palette = active_palette();
        if (options.closable)
        {
            const SpriteHandle closeBackground =
                state.close_pressed ? palette.buttonActive
                : result.close_hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_sprite(
                closeBackground,
                result.close_button.position.x,
                result.close_button.position.y,
                result.close_button.size.x,
                result.close_button.size.y);
            if (result.close_hovered || state.close_pressed)
            {
                draw_sprite(
                    palette.textFieldActive,
                    result.close_button.position.x,
                    result.close_button.position.y,
                    result.close_button.size.x,
                    2.0f);
            }

            const float textWidth = measure_text_width("X", kFontScale);
            const float textHeight = base_line_height(kFontScale);
            draw_text_line(
                "X",
                result.close_button.position.x + std::floor((std::max)(0.0f, result.close_button.size.x - textWidth) * 0.5f),
                result.close_button.position.y + std::floor((std::max)(0.0f, result.close_button.size.y - textHeight) * 0.5f) + 1.0f,
                kFontScale);
        }

        if (options.resizable)
        {
            const SpriteHandle handleSprite =
                state.resizing || result.resize_hovered ? palette.textFieldActive : palette.buttonHover;
            const float x = result.resize_handle.position.x;
            const float y = result.resize_handle.position.y;
            const float w = result.resize_handle.size.x;
            const float h = result.resize_handle.size.y;
            draw_sprite(handleSprite, x + w - 10.0f, y + h - 3.0f, 8.0f, 2.0f);
            draw_sprite(handleSprite, x + w - 6.0f, y + h - 7.0f, 4.0f, 2.0f);
        }

        set_cursor(result.content.position);
        result.begun = true;
        return result;
    }

    void end_floating_window() noexcept
    {
        if (g_floatingWindowTopLayerStack.empty())
            return;

        const bool topLayer = g_floatingWindowTopLayerStack.back();
        g_floatingWindowTopLayerStack.pop_back();
        end_window();
        if (topLayer)
            end_top_layer();
    }

    DockGuideLayout make_dock_guide_layout(const DockGuideOptions& options) noexcept
    {
        return from_lib(gui_lib::make_dock_guide_layout(to_lib(options)));
    }

    void render_dock_guide_overlay(
        const DockGuideLayout& layout,
        const DockGuideOverlayOptions& options) noexcept
    {
        if (!g_frame.ctx || layout.count == 0U)
            return;

        ensure_resources();
        begin_top_layer();
        ContentClipClearScope clearClip;

        const auto drawPreview = [&](const WidgetBounds& preview)
        {
            if (preview.size.x <= 1.0f || preview.size.y <= 1.0f)
                return;

            draw_sprite(
                g_resources.dockGuidePreview,
                preview.position.x,
                preview.position.y,
                preview.size.x,
                preview.size.y);
            draw_sprite(
                g_resources.dockGuide,
                preview.position.x,
                preview.position.y,
                preview.size.x,
                (std::min)(28.0f, preview.size.y));
            if (!options.moving_label.empty())
            {
                draw_text_line(
                    options.moving_label,
                    preview.position.x + 10.0f,
                    preview.position.y + 5.0f,
                    0.86f);
            }
        };

        if (layout.hovered_target != DockGuideTarget::none)
            drawPreview(layout.hovered_preview);
        else if (options.show_floating_preview)
            drawPreview(options.floating_preview);

        for (std::uint32_t index = 0U; index < layout.count; ++index)
        {
            const DockGuide& guide = layout.guides[index];
            if (guide.target == DockGuideTarget::none)
                continue;

            const bool contextTarget =
                guide.target == DockGuideTarget::left_context
                || guide.target == DockGuideTarget::right_context;
            const SpriteHandle fill = contextTarget
                ? (guide.hovered ? g_resources.dockContextHover : g_resources.dockContextGuide)
                : (guide.hovered ? g_resources.dockGuideHover : g_resources.dockGuide);
            draw_sprite(
                fill,
                guide.target_bounds.position.x,
                guide.target_bounds.position.y,
                guide.target_bounds.size.x,
                guide.target_bounds.size.y);

            if (contextTarget)
            {
                draw_sprite(
                    guide.hovered ? g_resources.dockContextHover : g_resources.dockGuide,
                    guide.target_bounds.position.x + 10.0f,
                    guide.target_bounds.position.y + guide.target_bounds.size.y - 10.0f,
                    (std::max)(0.0f, guide.target_bounds.size.x - 20.0f),
                    4.0f);
            }

            std::string_view label{};
            switch (guide.target)
            {
            case DockGuideTarget::left_tabs: label = options.left_tabs_label; break;
            case DockGuideTarget::right_tabs: label = options.right_tabs_label; break;
            case DockGuideTarget::bottom_left_tabs: label = options.bottom_left_tabs_label; break;
            case DockGuideTarget::bottom_right_tabs: label = options.bottom_right_tabs_label; break;
            case DockGuideTarget::left_context: label = options.left_context_label; break;
            case DockGuideTarget::right_context: label = options.right_context_label; break;
            case DockGuideTarget::float_window: label = options.floating_label; break;
            default: break;
            }

            if (!label.empty())
            {
                const float scale = contextTarget ? 0.86f : 0.80f;
                const float labelWidth = measure_text_width(label, scale);
                const float labelHeight = line_advance_amount(scale);
                draw_text_line(
                    label,
                    guide.target_bounds.position.x
                        + (std::max)(6.0f, (guide.target_bounds.size.x - labelWidth) * 0.5f),
                    guide.target_bounds.position.y
                        + (std::max)(6.0f, (guide.target_bounds.size.y - labelHeight) * 0.5f),
                    scale);
            }
        }

        end_top_layer();
    }

    DockableWindowResult update_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state,
        const DockableWindowOptions& options,
        const DockableWindowInput& input) noexcept
    {
        gui_lib::DockableWindowHostState coreHost = to_lib(host);
        gui_lib::DockableWindowState coreState = to_lib(state);
        const gui_lib::DockableWindowResult coreResult = gui_lib::update_dockable_window(
            coreHost,
            coreState,
            to_lib(options),
            to_lib(input));

        from_lib(coreHost, host);
        from_lib(coreState, state);
        return from_lib(coreResult);
    }

    void focus_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state) noexcept
    {
        gui_lib::DockableWindowHostState coreHost = to_lib(host);
        gui_lib::DockableWindowState coreState = to_lib(state);
        gui_lib::focus_dockable_window(coreHost, coreState);
        from_lib(coreHost, host);
        from_lib(coreState, state);
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

    void selection_outline(Vec2 position, Vec2 size, float thickness) noexcept
    {
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        try { ensure_resources(); }
        catch (...) { return; }

        const float border = (std::clamp)(
            thickness,
            1.0f,
            (std::max)(1.0f, (std::min)(size.x, size.y) * 0.5f));
        const SpriteHandle accent = active_palette().buttonActive;
        draw_sprite(accent,
            position.x, position.y, size.x, border);
        draw_sprite(accent,
            position.x, position.y + size.y - border, size.x, border);
        draw_sprite(accent,
            position.x, position.y, border, size.y);
        draw_sprite(accent,
            position.x + size.x - border, position.y, border, size.y);
    }

    void panel_rect(Vec2 position, Vec2 size) noexcept
    {
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        try { ensure_resources(); }
        catch (...) { return; }

        draw_sprite(active_palette().panelBackground, position.x, position.y, size.x, size.y);
    }

    void titlebar_rect(Vec2 position, Vec2 size) noexcept
    {
        if (!g_frame.ctx || size.x <= 0.0f || size.y <= 0.0f)
            return;

        try { ensure_resources(); }
        catch (...) { return; }

        draw_sprite(active_palette().titleBar, position.x, position.y, size.x, size.y);
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
    static bool button_with_state(
        std::string_view label,
        Vec2 size,
        bool selected,
        bool enabled = true) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kBoxInnerPadding);

        const bool hovered = enabled
            && point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();

        const SpriteHandle background = !enabled
            ? palette.panelBackground
            : selected || pressed ? palette.buttonActive
            : hovered ? palette.buttonHover
            : palette.buttonNormal;

        draw_sprite(background, pos.x, pos.y, width, height);

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

    bool toggle_switch(std::string_view label, bool& value, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float width = (std::max)(size.x, 96.0f);
        const float height = (std::max)(size.y, 28.0f);
        const float trackWidth = (std::min)(48.0f, height * 1.9f);
        const float trackHeight = (std::min)(24.0f, height - 4.0f);
        const float trackX = pos.x + width - trackWidth;
        const float trackY = pos.y + (height - trackHeight) * 0.5f;
        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }
        if (clicked)
            value = !value;

        const auto layout = gui_lib::make_toggle_switch_layout({
            .bounds = {
                { trackX, trackY },
                { trackWidth, trackHeight }
            },
            .value = value,
            .padding = 3.0f
        });
        const auto& palette = active_palette();
        const auto pillStyle = gui_lib::rounded_rect::RoundedRectStyle{
            .enabled = true,
            .control_radius = trackHeight * 0.5f,
            .segments_per_corner = 8U
        };
        const SpriteHandle track = value
            ? palette.buttonActive
            : hovered ? palette.buttonHover : palette.buttonNormal;
        draw_rounded_sprite(
            track,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            layout.track.size.y,
            pillStyle);
        draw_rounded_sprite(
            value ? palette.buttonHover : palette.textFieldActive,
            layout.thumb.position.x,
            layout.thumb.position.y,
            layout.thumb.size.x,
            layout.thumb.size.y,
            gui_lib::rounded_rect::RoundedRectStyle{
                .enabled = true,
                .control_radius = layout.thumb.size.y * 0.5f,
                .segments_per_corner = 8U
            });

        const float labelWidth = (std::max)(1.0f, width - trackWidth - 10.0f);
        const std::string fittedLabel = fit_text_to_width(label, labelWidth, kFontScale);
        const std::string_view displayLabel = fittedLabel.empty()
            ? label
            : std::string_view{ fittedLabel };
        const float textY = pos.y + std::floor((std::max)(0.0f, (height - baseHeight) * 0.5f)) + 1.0f;
        draw_text_line(displayLabel, pos.x, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });
        return clicked;
    }

    bool text_link(std::string_view label, Vec2 size, bool selected) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return false;

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) + 2.0f * kContentPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), baseHeight + 2.0f * kBoxInnerPadding);

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key(label, pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        if (selected)
        {
            draw_sprite(palette.panelBackground, pos.x, pos.y, width, height);
            draw_sprite(palette.textFieldActive, pos.x, pos.y, 3.0f, height);
        }
        else if (hovered || pressed)
        {
            draw_sprite(palette.buttonHover, pos.x, pos.y, width, height);
        }

        const SpriteHandle accent = selected
            ? palette.textFieldActive
            : hovered || pressed ? palette.buttonHover
            : palette.titleBar;
        draw_sprite(accent, pos.x, pos.y + height - 2.0f, width, 2.0f);

        const std::string fittedLabel = fit_text_to_width(
            label,
            (std::max)(1.0f, width - 2.0f * kContentPadding - 2.0f),
            kFontScale);
        const std::string_view displayLabel = fittedLabel.empty()
            ? label
            : std::string_view{ fittedLabel };
        const float textY = pos.y + std::floor((std::max)(0.0f, (height - baseHeight) * 0.5f)) + 1.0f;
        draw_text_line(displayLabel, pos.x + kContentPadding, textY, kFontScale);

        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        advance_cursor({ 0.0f, height + kContentPadding });
        return clicked;
    }

    bool titlebar_close_button(Vec2 window_position, Vec2 window_size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx || window_size.x < 44.0f || window_size.y < 24.0f)
            return false;

        const float width = 24.0f;
        const float height = 22.0f;
        const Vec2 pos{
            window_position.x + (std::max)(0.0f, window_size.x - width - kContentPadding),
            window_position.y + 5.0f
        };
        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_modal_input_capture(g_frame.mousePos);
        const std::size_t pressKey = widget_press_key("window-close", pos, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
        const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (hovered)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        const SpriteHandle background =
            pressed ? palette.buttonActive
            : hovered ? palette.buttonHover
            : palette.buttonNormal;
        draw_sprite(background, pos.x, pos.y, width, height);
        if (hovered || pressed)
        {
            draw_sprite(palette.textFieldActive, pos.x, pos.y, width, 2.0f);
            draw_sprite(palette.textFieldActive, pos.x, pos.y, 2.0f, height);
        }

        const float textWidth = measure_text_width("X", kFontScale);
        const float textX = pos.x + std::floor((std::max)(0.0f, width - textWidth) * 0.5f);
        const float textY = pos.y + std::floor((std::max)(0.0f, height - base_line_height(kFontScale)) * 0.5f) + 1.0f;
        draw_text_line("X", textX, textY, kFontScale);
        g_frame.lastButtonBounds = WidgetBounds{ .position = pos, .size = { width, height } };
        return clicked;
    }

    ImageBoxResult image_box(const ImageBoxOptions& options) noexcept
    {
        ImageBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        const Vec2 position = g_frame.cursor;
        const float width = (std::max)(options.size.x, 1.0f);
        const float height = (std::max)(options.size.y, 1.0f);
        const float captionHeight = options.caption.empty()
            ? 0.0f
            : base_line_height(kFontScale) + kBoxInnerPadding;
        const gui_lib::ImageBoxLayout layout = gui_lib::make_image_box_layout({
            .bounds = { to_lib(position), { width, height } },
            .source_extent = {
                options.source_size.x > 0.0f ? options.source_size.x : width,
                options.source_size.y > 0.0f ? options.source_size.y : height },
            .fit = options.fit == ImageFit::Stretch
                ? gui_lib::ImageFitMode::stretch
                : gui_lib::ImageFitMode::contain,
            .padding = kBoxInnerPadding,
            .caption_height = captionHeight
        });

        const bool hovered = options.enabled
            && point_in_rect(g_frame.mousePos, position.x, position.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const std::string_view identity = !options.id.empty()
            ? options.id
            : (!options.caption.empty() ? options.caption : std::string_view{ "<image-box>" });
        const std::size_t pressKey = widget_press_key(identity, position, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (options.interactive && hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }

        const bool pressed = options.interactive
            && (g_frame.mouseDown || g_frame.justReleased)
            && pressedKey == pressKey;
        const bool clicked = options.interactive
            && left_release_available()
            && hovered
            && pressedKey == pressKey;
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (clicked)
                consume_left_release();
            pressedKey = 0;
        }

        const auto& palette = active_palette();
        const SpriteHandle background = !options.enabled
            ? palette.panelBackground
            : pressed ? palette.buttonActive
            : hovered ? palette.buttonHover
            : palette.buttonNormal;
        draw_sprite(background, position.x, position.y, width, height);

        if (layout.valid && options.sprite.is_valid())
        {
            draw_sprite(
                options.sprite,
                layout.content.position.x,
                layout.content.position.y,
                layout.content.size.x,
                layout.content.size.y);
        }

        if (options.selected)
        {
            draw_sprite(palette.textFieldActive, position.x, position.y, width, 2.0f);
            draw_sprite(palette.textFieldActive, position.x, position.y + height - 2.0f, width, 2.0f);
            draw_sprite(palette.textFieldActive, position.x, position.y, 2.0f, height);
            draw_sprite(palette.textFieldActive, position.x + width - 2.0f, position.y, 2.0f, height);
        }

        if (!options.caption.empty() && layout.caption.size.y > 0.0f)
        {
            const std::string fitted = fit_text_to_width(
                options.caption,
                (std::max)(1.0f, layout.caption.size.x - 2.0f * kButtonTextClipInset),
                kFontScale);
            const std::string_view display = fitted.empty()
                ? options.caption
                : std::string_view{ fitted };
            const float textY = layout.caption.position.y
                + std::floor((std::max)(
                    0.0f,
                    (layout.caption.size.y - base_line_height(kFontScale)) * 0.5f));
            draw_text_line(display, layout.caption.position.x, textY, kFontScale);
        }

        result.bounds = { .position = position, .size = { width, height } };
        result.image_bounds = {
            .position = from_lib(layout.content.position),
            .size = from_lib(layout.content.size) };
        result.hovered = hovered;
        result.clicked = clicked;
        result.valid = layout.valid && options.sprite.is_valid();
        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    bool image_button(const ImageButtonOptions& options) noexcept
    {
        if (options.id.empty())
            return false;
        return image_box({
            .id = options.id,
            .sprite = options.sprite,
            .size = options.size,
            .source_size = options.source_size,
            .fit = options.fit,
            .interactive = true,
            .selected = options.selected,
            .enabled = options.enabled
        }).clicked;
    }

    bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept
    {
        return image_box({
            .id = "<image-button>",
            .sprite = sprite,
            .size = size,
            .source_size = size,
            .fit = ImageFit::Contain,
            .interactive = true
        }).clicked;
    }

    AssetGridResult asset_grid(const AssetGridOptions& options) noexcept
    {
        AssetGridResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx || options.id.empty()
            || options.size.x < 1.0f || options.size.y < 1.0f
            || options.items.size() > gui_lib::asset_grid_maximum_items)
        {
            return result;
        }

        const Vec2 origin = g_frame.cursor;
        const float width = (std::max)(1.0f, options.size.x);
        const float height = (std::max)(48.0f, options.size.y);
        const bool hovered = options.enabled
            && point_in_rect(g_frame.mousePos, origin.x, origin.y, width, height)
            && point_in_active_clip(g_frame.mousePos);

        std::vector<const ContextMenuActionSpec*> contextActions{};
        const std::size_t contextActionInspectCount = (std::min)(
            options.context_actions.size(),
            asset_grid_maximum_context_actions);
        contextActions.reserve(contextActionInspectCount);
        result.context_actions_truncated =
            options.context_actions.size() > contextActionInspectCount;
        for (std::size_t index = 0u;
             index < contextActionInspectCount;
             ++index)
        {
            const ContextMenuActionSpec& action = options.context_actions[index];
            if (action.id.empty() || action.label.empty())
                continue;
            const bool duplicate = std::any_of(
                contextActions.begin(),
                contextActions.end(),
                [&](const ContextMenuActionSpec* accepted) noexcept
                {
                    return accepted->id == action.id;
                });
            if (duplicate)
                continue;
            contextActions.push_back(&action);
        }

        const std::string contextStateKey = asset_grid_state_key(options.id);
        auto& contextMenu = g_assetGridContextMenuStates[contextStateKey];
        if (contextActions.empty())
            contextMenu.open = false;
        const bool menuWasOpen = contextMenu.open;

        std::vector<gui_lib::AssetGridItem> libraryItems{};
        libraryItems.reserve(options.items.size());
        const auto libraryKind = [](AssetGridItemKind kind) noexcept
        {
            switch (kind)
            {
            case AssetGridItemKind::Folder: return gui_lib::AssetGridItemKind::folder;
            case AssetGridItemKind::Image: return gui_lib::AssetGridItemKind::image;
            case AssetGridItemKind::Texture: return gui_lib::AssetGridItemKind::texture;
            case AssetGridItemKind::Material: return gui_lib::AssetGridItemKind::material;
            case AssetGridItemKind::Model: return gui_lib::AssetGridItemKind::model;
            case AssetGridItemKind::Audio: return gui_lib::AssetGridItemKind::audio;
            case AssetGridItemKind::Scene: return gui_lib::AssetGridItemKind::scene;
            case AssetGridItemKind::Document: return gui_lib::AssetGridItemKind::document;
            case AssetGridItemKind::Generic:
            default: return gui_lib::AssetGridItemKind::generic;
            }
        };
        for (const AssetGridItem& item : options.items)
        {
            std::optional<gui_lib::AssetGridImageMetadata> imageMetadata{};
            if (item.image.is_valid())
            {
                imageMetadata = gui_lib::AssetGridImageMetadata{
                    .content_key = item.image.pack(),
                    .pixel_width = static_cast<std::uint32_t>((std::max)(1.0f, item.source_size.x)),
                    .pixel_height = static_cast<std::uint32_t>((std::max)(1.0f, item.source_size.y)),
                    .fit = gui_lib::ImageFitMode::contain,
                    .has_alpha = true
                };
            }
            libraryItems.push_back(gui_lib::AssetGridItem{
                .id = { item.id },
                .label = item.label,
                .detail = item.detail,
                .search_terms = item.search_terms,
                .kind = libraryKind(item.kind),
                .activation = gui_lib::AssetGridActivationRole::open_in_tab,
                .image = imageMetadata,
                .enabled = options.enabled && item.enabled
            });
        }

        float requestedScroll = std::isfinite(options.scroll_offset)
            ? (std::max)(0.0f, options.scroll_offset)
            : 0.0f;
        if (hovered && !menuWasOpen && g_frame.mouseWheelDelta != 0)
        {
            const float wheelSteps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            requestedScroll = (std::max)(0.0f, requestedScroll - wheelSteps * 108.0f);
            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
        }

        gui_lib::AssetGridState state{};
        if (options.selected_id.has_value())
            state.selected_id = gui_lib::AssetGridItemId{ *options.selected_id };
        const gui_lib::AssetGridLayoutOptions layoutOptions{
            .viewport = { to_lib(origin), { width, height } },
            .tile_extent = to_lib(options.tile_size),
            .gap = to_lib(options.gap),
            .padding = to_lib(options.padding),
            .image_height = options.image_height,
            .label_height = 24.0f,
            .detail_height = 18.0f,
            .scroll_offset = requestedScroll,
            .maximum_visible_tiles = gui_lib::asset_grid_maximum_visible_tiles,
            .clear_selection_on_empty_press = options.clear_selection_on_empty_press
        };

        const std::size_t pressKey = widget_press_key(options.id, origin, { width, height });
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        const bool pointerPressed = hovered && !menuWasOpen && left_press_available();
        if (pointerPressed)
        {
            pressedKey = pressKey;
            consume_left_press();
        }
        const bool pointerActivated = hovered
            && !menuWasOpen
            && left_release_available()
            && pressedKey == pressKey;
        const bool contextRequested = hovered
            && !contextActions.empty()
            && right_press_available();

        const gui_lib::AssetGridUpdateResult update = gui_lib::update_asset_grid(
            state,
            libraryItems,
            gui_lib::AssetGridFilterOptions{
                .query = options.query,
                .maximum_results = gui_lib::asset_grid_maximum_items,
                .include_disabled = true
            },
            layoutOptions,
            gui_lib::AssetGridInput{
                .pointer_position = to_lib(g_frame.mousePos),
                .pointer_pressed = pointerPressed,
                .pointer_activated = pointerActivated,
                .context_requested = contextRequested,
                .pointer_present = hovered
            });
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (pointerActivated)
                consume_left_release();
            pressedKey = 0;
        }

        bool menuOpenedThisFrame = false;
        if (contextRequested && update.context_request_valid
            && update.context_requested_id.has_value())
        {
            contextMenu.position = g_frame.mousePos;
            contextMenu.scrollOffset = 0.0f;
            contextMenu.targetId = update.context_requested_id->value;
            contextMenu.open = true;
            menuOpenedThisFrame = true;
            consume_right_press();
        }

        if (contextMenu.open)
        {
            const bool targetVisible = std::any_of(
                update.view.layout.visible_tiles.begin(),
                update.view.layout.visible_tiles.end(),
                [&](const gui_lib::AssetGridTileLayout& tile) noexcept
                {
                    return tile.enabled && tile.id.value == contextMenu.targetId;
                });
            if (!targetVisible)
                contextMenu.open = false;
        }

        const auto& palette = active_palette();
        draw_sprite(palette.consoleBackground, origin.x, origin.y, width, height);
        const auto kindLabel = [](AssetGridItemKind kind) noexcept -> std::string_view
        {
            switch (kind)
            {
            case AssetGridItemKind::Folder: return "FOLDER";
            case AssetGridItemKind::Image: return "IMAGE";
            case AssetGridItemKind::Texture: return "TEXTURE";
            case AssetGridItemKind::Material: return "MATERIAL";
            case AssetGridItemKind::Model: return "MODEL";
            case AssetGridItemKind::Audio: return "AUDIO";
            case AssetGridItemKind::Scene: return "SCENE";
            case AssetGridItemKind::Document: return "DOCUMENT";
            case AssetGridItemKind::Generic:
            default: return "ASSET";
            }
        };

        for (const gui_lib::AssetGridTileLayout& tile : update.view.layout.visible_tiles)
        {
            if (tile.source_index >= options.items.size())
                continue;
            const AssetGridItem& item = options.items[tile.source_index];
            const SpriteHandle tileBackground = !item.enabled
                ? palette.panelBackground
                : tile.selected ? palette.buttonActive
                : tile.hovered ? palette.buttonHover
                : palette.buttonNormal;
            draw_sprite(tileBackground, tile.tile.position.x, tile.tile.position.y,
                tile.tile.size.x, tile.tile.size.y);

            if (item.image.is_valid() && tile.image.valid)
            {
                draw_sprite(item.image, tile.image.content.position.x,
                    tile.image.content.position.y, tile.image.content.size.x,
                    tile.image.content.size.y);
            }
            else if (tile.image.frame.size.y > 0.0f)
            {
                draw_sprite(palette.panelBackground, tile.image.frame.position.x,
                    tile.image.frame.position.y, tile.image.frame.size.x,
                    tile.image.frame.size.y);
                const std::string_view placeholder = kindLabel(item.kind);
                const float placeholderWidth = measure_text_width(placeholder, kFontScale);
                draw_text_line(placeholder,
                    tile.image.frame.position.x
                        + (std::max)(0.0f, (tile.image.frame.size.x - placeholderWidth) * 0.5f),
                    tile.image.frame.position.y
                        + (std::max)(0.0f, (tile.image.frame.size.y - base_line_height(kFontScale)) * 0.5f),
                    kFontScale);
            }

            const std::string fittedLabel = fit_text_to_width(item.label,
                (std::max)(1.0f, tile.label.size.x - 2.0f * kButtonTextClipInset),
                kFontScale);
            const std::string_view displayLabel = fittedLabel.empty()
                ? item.label
                : std::string_view{ fittedLabel };
            draw_text_line(displayLabel, tile.label.position.x + kButtonTextClipInset,
                tile.label.position.y
                    + (std::max)(0.0f, (tile.label.size.y - base_line_height(kFontScale)) * 0.5f),
                kFontScale);

            if (!item.detail.empty() && tile.detail.size.y > 0.0f)
            {
                const std::string fittedDetail = fit_text_to_width(item.detail,
                    (std::max)(1.0f, tile.detail.size.x - 2.0f * kButtonTextClipInset),
                    kFontScale);
                const std::string_view displayDetail = fittedDetail.empty()
                    ? item.detail
                    : std::string_view{ fittedDetail };
                draw_text_line(displayDetail,
                    tile.detail.position.x + kButtonTextClipInset,
                    tile.detail.position.y, kFontScale);
            }
            if (tile.selected)
            {
                draw_sprite(palette.textFieldActive,
                    tile.selection_indicator.position.x,
                    tile.selection_indicator.position.y,
                    tile.selection_indicator.size.x,
                    tile.selection_indicator.size.y);
            }
        }

        if (contextMenu.open)
        {
            constexpr float menuPadding = 4.0f;
            constexpr float rowHeight = 28.0f;
            constexpr float rowGap = 2.0f;
            constexpr float minimumMenuWidth = 144.0f;
            constexpr float maximumMenuWidth = 280.0f;
            constexpr float scrollbarWidth = 10.0f;

            float measuredMenuWidth = minimumMenuWidth;
            for (const ContextMenuActionSpec* action : contextActions)
            {
                measuredMenuWidth = (std::max)(
                    measuredMenuWidth,
                    measure_text_width(action->label, kFontScale)
                        + 2.0f * kContentPadding);
            }
            measuredMenuWidth = (std::min)(measuredMenuWidth, maximumMenuWidth);

            const Vec2 hostOrigin = g_frame.origin;
            const Vec2 hostSize = g_frame.windowSize;
            const float availableHostWidth = (std::max)(
                48.0f, hostSize.x - 2.0f * menuPadding);
            const float availableHostHeight = (std::max)(
                48.0f, hostSize.y - 2.0f * menuPadding);
            const float menuWidth = (std::min)(
                measuredMenuWidth, availableHostWidth);
            const float menuContentHeight =
                static_cast<float>(contextActions.size()) * rowHeight
                + static_cast<float>(
                    contextActions.empty() ? 0u : contextActions.size() - 1u)
                    * rowGap;
            const float desiredMenuHeight =
                menuContentHeight + 2.0f * menuPadding;
            const float menuHeight = (std::min)(
                desiredMenuHeight, availableHostHeight);
            const float maximumScroll = (std::max)(
                0.0f, desiredMenuHeight - menuHeight);
            contextMenu.scrollOffset = (std::clamp)(
                contextMenu.scrollOffset, 0.0f, maximumScroll);

            Vec2 menuPosition = contextMenu.position;
            menuPosition.x = (std::clamp)(
                menuPosition.x,
                hostOrigin.x + menuPadding,
                (std::max)(
                    hostOrigin.x + menuPadding,
                    hostOrigin.x + hostSize.x - menuWidth - menuPadding));
            menuPosition.y = (std::clamp)(
                menuPosition.y,
                hostOrigin.y + menuPadding,
                (std::max)(
                    hostOrigin.y + menuPadding,
                    hostOrigin.y + hostSize.y - menuHeight - menuPadding));

            const bool hoveredMenu = point_in_rect(
                g_frame.mousePos,
                menuPosition.x,
                menuPosition.y,
                menuWidth,
                menuHeight);
            if (hoveredMenu && g_frame.mouseWheelDelta != 0)
            {
                const float wheelSteps =
                    static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
                contextMenu.scrollOffset = (std::clamp)(
                    contextMenu.scrollOffset - wheelSteps * rowHeight * 2.0f,
                    0.0f,
                    maximumScroll);
                g_frame.mouseWheelDelta = 0;
            }

            if (!menuOpenedThisFrame && !hoveredMenu
                && left_press_available())
            {
                contextMenu.open = false;
                consume_left_press();
            }
            else if (!menuOpenedThisFrame && !hoveredMenu
                && right_press_available())
            {
                contextMenu.open = false;
                consume_right_press();
            }
            if (contextMenu.open && right_release_available())
                consume_right_release();

            if (contextMenu.open)
            {
                const Vec2 savedCursor = g_frame.cursor;
                const Vec2 savedOrigin = g_frame.origin;
                const Vec2 savedWindowSize = g_frame.windowSize;
                const Vec2 savedContentMin = g_frame.contentMin;
                const Vec2 savedContentMax = g_frame.contentMax;
                const bool savedInsideWindow = g_frame.insideWindow;
                const std::string savedWindowKey = g_frame.windowKey;
                const std::uint64_t savedWidgetSerial = g_frame.widgetSerial;

                begin_top_layer();
                draw_sprite(
                    palette.windowBackground,
                    menuPosition.x,
                    menuPosition.y,
                    menuWidth,
                    menuHeight);
                draw_sprite(
                    palette.titleBar,
                    menuPosition.x,
                    menuPosition.y,
                    menuWidth,
                    2.0f);

                g_frame.insideWindow = true;
                g_frame.windowKey = contextStateKey;
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPosition;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = {
                    menuPosition.x + menuPadding,
                    menuPosition.y + menuPadding
                };
                g_frame.contentMax = {
                    menuPosition.x + menuWidth - menuPadding
                        - (maximumScroll > 0.0f ? scrollbarWidth : 0.0f),
                    menuPosition.y + menuHeight - menuPadding
                };

                const float actionWidth = (std::max)(
                    1.0f, g_frame.contentMax.x - g_frame.contentMin.x);
                for (std::size_t index = 0u;
                     index < contextActions.size();
                     ++index)
                {
                    const ContextMenuActionSpec& action =
                        *contextActions[index];
                    set_cursor({
                        g_frame.contentMin.x,
                        g_frame.contentMin.y
                            + static_cast<float>(index) * (rowHeight + rowGap)
                            - contextMenu.scrollOffset
                    });
                    if (button_with_state(
                            action.label,
                            { actionWidth, rowHeight },
                            false,
                            action.enabled))
                    {
                        result.context_target_id = contextMenu.targetId;
                        result.selected_action_id = std::string(action.id);
                        contextMenu.open = false;
                        break;
                    }
                }

                if (maximumScroll > 0.0f)
                {
                    const float trackX =
                        menuPosition.x + menuWidth - scrollbarWidth;
                    const float visibleRatio = menuHeight / desiredMenuHeight;
                    const float thumbHeight = (std::max)(
                        18.0f, menuHeight * visibleRatio);
                    const float scrollRatio = maximumScroll > 0.0f
                        ? contextMenu.scrollOffset / maximumScroll
                        : 0.0f;
                    const float thumbY = menuPosition.y
                        + (menuHeight - thumbHeight) * scrollRatio;
                    draw_sprite(
                        palette.textField,
                        trackX,
                        menuPosition.y,
                        scrollbarWidth,
                        menuHeight);
                    draw_sprite(
                        palette.buttonActive,
                        trackX,
                        thumbY,
                        scrollbarWidth,
                        thumbHeight);
                }

                if (hoveredMenu)
                {
                    if (left_press_available())
                        consume_left_press();
                    if (left_release_available())
                        consume_left_release();
                    if (right_press_available())
                        consume_right_press();
                    if (right_release_available())
                        consume_right_release();
                }

                g_frame.cursor = savedCursor;
                g_frame.origin = savedOrigin;
                g_frame.windowSize = savedWindowSize;
                g_frame.contentMin = savedContentMin;
                g_frame.contentMax = savedContentMax;
                g_frame.insideWindow = savedInsideWindow;
                g_frame.windowKey = savedWindowKey;
                g_frame.widgetSerial = savedWidgetSerial;
                end_top_layer();
            }
        }

        result.bounds = { .position = origin, .size = { width, height } };
        if (update.selected_id.has_value())
            result.selected_id = update.selected_id->value;
        if (update.activated_id.has_value())
            result.activated_id = update.activated_id->value;
        if (contextMenu.open)
        {
            result.context_target_id = contextMenu.targetId;
            result.context_menu_open = true;
        }
        result.matched_count = update.view.filter.matched_count;
        result.visible_count = update.view.layout.visible_tiles.size();
        result.visible_ids.reserve(update.view.layout.visible_tiles.size());
        for (const gui_lib::AssetGridTileLayout& tile :
             update.view.layout.visible_tiles)
        {
            result.visible_ids.push_back(tile.id.value);
        }
        result.content_height = update.view.layout.content_extent.y;
        result.scroll_offset = update.view.layout.scroll_offset;
        result.selection_changed = update.selection_changed;
        result.truncated = update.view.filter.source_truncated
            || update.view.filter.results_truncated
            || update.view.layout.items_truncated
            || update.view.layout.visible_tiles_truncated;
        result.valid = update.view.layout.valid;
        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    NodeGraphCanvasResult node_graph_canvas(
        const NodeGraphCanvasOptions& options) noexcept
    {
        NodeGraphCanvasResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx || options.id.empty()
            || options.size.x < 1.0f || options.size.y < 1.0f
            || options.nodes.size() > 4'096u
            || options.edges.size() > 16'384u)
        {
            return result;
        }

        const Vec2 origin = g_frame.cursor;
        const float width = options.size.x;
        const float height = options.size.y;
        result.bounds = {origin, options.size};
        result.hovered = options.enabled
            && point_in_rect(
                g_frame.mousePos, origin.x, origin.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        namespace graph_workspace = gui_lib::node_graph_workspace;
        auto& graphState = g_nodeGraphCanvasStates[widget_state_key(options.id)];
        std::erase_if(
            graphState.nodeOffsets,
            [&](const auto& offset)
            {
                return std::ranges::none_of(
                    options.nodes,
                    [&](const NodeGraphCanvasNode& node)
                    {
                        return node.id == offset.first;
                    });
            });

        std::vector<graph_workspace::NodeLayout> graphNodes{};
        std::vector<graph_workspace::PinLayout> graphPins{};
        std::vector<graph_workspace::EdgeLayout> graphEdges{};
        std::unordered_map<std::uint64_t, std::size_t> nodeIndices{};
        graphNodes.reserve(options.nodes.size());
        graphPins.reserve(options.nodes.size() * 2u);
        graphEdges.reserve(options.edges.size());
        nodeIndices.reserve(options.nodes.size());

        std::uint64_t sourceRevision = 1'469'598'103'934'665'603ull;
        const auto hashValue = [&](std::uint64_t value) noexcept
        {
            sourceRevision ^= value;
            sourceRevision *= 1'099'511'628'211ull;
        };
        for (std::size_t index = 0u; index < options.nodes.size(); ++index)
        {
            const NodeGraphCanvasNode& node = options.nodes[index];
            if (node.id == graph_workspace::invalid_node_id
                || nodeIndices.contains(node.id))
            {
                result.valid = false;
                return result;
            }
            nodeIndices.emplace(node.id, index);
            const auto offset = graphState.nodeOffsets.contains(node.id)
                ? graphState.nodeOffsets.at(node.id)
                : graph_workspace::Vec2{};
            const double nodeWidth = (std::max)(64.0, static_cast<double>(node.size.x));
            const double nodeHeight = (std::max)(36.0, static_cast<double>(node.size.y));
            const graph_workspace::Rect bounds{
                static_cast<double>(node.position.x) + offset.x,
                static_cast<double>(node.position.y) + offset.y,
                nodeWidth,
                nodeHeight};
            graphNodes.push_back({
                .id = node.id,
                .bounds = bounds,
                .layout_order = index,
                .selectable = node.enabled});
            const graph_workspace::PinId inputPin =
                static_cast<graph_workspace::PinId>(index * 2u + 1u);
            const graph_workspace::PinId outputPin = inputPin + 1u;
            graphPins.push_back({
                .id = inputPin,
                .node_id = node.id,
                .direction = graph_workspace::PinDirection::input,
                .position = {bounds.x, bounds.y + bounds.height * 0.5},
                .layout_order = index * 2u,
                .connectable = options.allow_connections && node.enabled});
            graphPins.push_back({
                .id = outputPin,
                .node_id = node.id,
                .direction = graph_workspace::PinDirection::output,
                .position = {bounds.x + bounds.width, bounds.y + bounds.height * 0.5},
                .layout_order = index * 2u + 1u,
                .connectable = options.allow_connections && node.enabled});
            hashValue(node.id);
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.x * 1'000.0)));
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.y * 1'000.0)));
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.width * 1'000.0)));
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.height * 1'000.0)));
        }
        for (std::size_t index = 0u; index < options.edges.size(); ++index)
        {
            const NodeGraphCanvasEdge& edge = options.edges[index];
            const auto sourceNode = nodeIndices.find(edge.source_node);
            const auto targetNode = nodeIndices.find(edge.target_node);
            if (!edge.enabled || edge.id == graph_workspace::invalid_edge_id
                || sourceNode == nodeIndices.end()
                || targetNode == nodeIndices.end())
            {
                continue;
            }
            graphEdges.push_back({
                .id = edge.id,
                .output_pin_id = static_cast<graph_workspace::PinId>(
                    sourceNode->second * 2u + 2u),
                .input_pin_id = static_cast<graph_workspace::PinId>(
                    targetNode->second * 2u + 1u),
                .layout_order = index,
                .selectable = edge.enabled});
            hashValue(edge.id);
            hashValue(edge.source_node);
            hashValue(edge.target_node);
        }
        if (sourceRevision == 0u)
            sourceRevision = 1u;

        const auto replaced = graphState.controller.replace_graph({
            .nodes = graphNodes,
            .pins = graphPins,
            .edges = graphEdges,
            .source_revision = sourceRevision});
        const auto viewportSet = graphState.controller.set_viewport({
            origin.x, origin.y, width, height});
        if (!replaced.committed || !viewportSet.accepted)
        {
            result.valid = false;
            return result;
        }

        std::uint64_t externalSelectionSignature = 1u;
        for (const NodeGraphCanvasNode& node : options.nodes)
        {
            if (node.selected)
            {
                externalSelectionSignature ^= node.id;
                externalSelectionSignature *= 1'099'511'628'211ull;
            }
        }
        if (externalSelectionSignature != graphState.externalSelectionSignature)
        {
            (void)graphState.controller.clear_selection();
            bool first = true;
            for (const NodeGraphCanvasNode& node : options.nodes)
            {
                if (!node.selected)
                    continue;
                (void)graphState.controller.select_node(
                    node.id,
                    first ? graph_workspace::SelectionMode::replace
                          : graph_workspace::SelectionMode::add);
                first = false;
            }
            graphState.externalSelectionSignature = externalSelectionSignature;
        }
        if (!graphState.initialized || options.reset_view)
        {
            (void)graphState.controller.set_view({});
            graphState.initialized = true;
            result.view_changed = true;
        }
        if (options.fit_to_content && !options.nodes.empty())
        {
            const auto fitted = graphState.controller.fit_to_content(28.0);
            result.view_changed = result.view_changed || fitted.changed;
        }
        if (result.hovered && g_frame.mouseWheelDelta != 0)
        {
            const double wheelSteps =
                static_cast<double>(g_frame.mouseWheelDelta) / 120.0;
            const auto zoomed = graphState.controller.zoom_at(
                {g_frame.mousePos.x, g_frame.mousePos.y},
                wheelSteps);
            result.view_changed = result.view_changed || zoomed.changed;
            g_frame.mouseWheelDelta = 0;
        }
        result.valid = true;

        const auto& palette = active_palette();
        draw_sprite(
            palette.panelBackground,
            origin.x, origin.y, width, height);
        ContentClipScope graphClip{
            origin,
            {origin.x + width, origin.y + height}};
        const auto graphView = graphState.controller.view();
        const float gridStep = (std::clamp)(
            options.grid_step * static_cast<float>(graphView.zoom),
            12.0f,
            96.0f);
        float gridOffsetX = std::fmod(
            static_cast<float>(graphView.pan.x), gridStep);
        float gridOffsetY = std::fmod(
            static_cast<float>(graphView.pan.y), gridStep);
        if (gridOffsetX < 0.0f)
            gridOffsetX += gridStep;
        if (gridOffsetY < 0.0f)
            gridOffsetY += gridStep;
        for (float x = gridOffsetX; x < width; x += gridStep)
        {
            draw_sprite(
                palette.titleBar,
                origin.x + x, origin.y, 1.0f, height);
        }
        for (float y = gridOffsetY; y < height; y += gridStep)
        {
            draw_sprite(
                palette.titleBar,
                origin.x, origin.y + y, width, 1.0f);
        }

        std::unordered_map<std::uint64_t,
            const graph_workspace::NodeProjection*> projectedNodes{};
        projectedNodes.reserve(options.nodes.size());
        for (const graph_workspace::NodeProjection& projection
             : graphState.controller.node_projections())
        {
            projectedNodes.emplace(projection.id, &projection);
        }
        std::unordered_map<std::uint64_t, Vec2> projectedPins{};
        projectedPins.reserve(graphPins.size());
        for (const graph_workspace::PinProjection& projection
             : graphState.controller.pin_projections())
        {
            projectedPins.emplace(
                projection.id,
                Vec2{
                    static_cast<float>(projection.screen_position.x),
                    static_cast<float>(projection.screen_position.y)});
        }
        const auto drawHorizontal = [&](float first, float second, float y,
            const SpriteHandle& sprite)
        {
            const float x = (std::min)(first, second);
            draw_sprite(
                sprite,
                x, y - 1.0f,
                (std::max)(2.0f, std::abs(second - first)),
                2.0f);
        };
        const auto drawVertical = [&](float x, float first, float second,
            const SpriteHandle& sprite)
        {
            const float y = (std::min)(first, second);
            draw_sprite(
                sprite,
                x - 1.0f, y,
                2.0f,
                (std::max)(2.0f, std::abs(second - first)));
        };
        const auto edgeScreenPositions = [&](
            const NodeGraphCanvasEdge& edge)
            -> std::optional<std::pair<Vec2, Vec2>>
        {
            const auto sourceNode = nodeIndices.find(edge.source_node);
            const auto targetNode = nodeIndices.find(edge.target_node);
            if (sourceNode == nodeIndices.end()
                || targetNode == nodeIndices.end())
            {
                return std::nullopt;
            }
            const std::uint64_t outputPin = sourceNode->second * 2u + 2u;
            const std::uint64_t inputPin = targetNode->second * 2u + 1u;
            const auto source = projectedPins.find(outputPin);
            const auto target = projectedPins.find(inputPin);
            if (source == projectedPins.end() || target == projectedPins.end())
                return std::nullopt;
            return std::pair{source->second, target->second};
        };
        const auto edgeHovered = [&](const NodeGraphCanvasEdge& edge)
        {
            if (!result.hovered || !edge.enabled)
                return false;
            const auto positions = edgeScreenPositions(edge);
            if (!positions)
                return false;
            const auto [source, target] = *positions;
            const float middleX = (source.x + target.x) * 0.5f;
            constexpr float tolerance = 6.0f;
            const auto nearHorizontal = [&](float first, float second, float y)
            {
                return g_frame.mousePos.x >= (std::min)(first, second) - tolerance
                    && g_frame.mousePos.x <= (std::max)(first, second) + tolerance
                    && std::abs(g_frame.mousePos.y - y) <= tolerance;
            };
            const auto nearVertical = [&](float x, float first, float second)
            {
                return g_frame.mousePos.y >= (std::min)(first, second) - tolerance
                    && g_frame.mousePos.y <= (std::max)(first, second) + tolerance
                    && std::abs(g_frame.mousePos.x - x) <= tolerance;
            };
            return nearHorizontal(source.x, middleX, source.y)
                || nearVertical(middleX, source.y, target.y)
                || nearHorizontal(middleX, target.x, target.y);
        };

        std::optional<std::uint64_t> hoveredEdge{};
        for (const NodeGraphCanvasEdge& edge : options.edges)
        {
            if (!edge.enabled)
                continue;
            const auto positions = edgeScreenPositions(edge);
            if (!positions)
                continue;
            const auto [source, target] = *positions;
            const float middleX = (source.x + target.x) * 0.5f;
            const bool hovered = edgeHovered(edge);
            const SpriteHandle lineSprite = edge.selected || hovered
                ? palette.textFieldActive
                : palette.buttonActive;
            drawHorizontal(source.x, middleX, source.y, lineSprite);
            drawVertical(middleX, source.y, target.y, lineSprite);
            drawHorizontal(middleX, target.x, target.y, lineSprite);
            draw_sprite(
                lineSprite,
                target.x - 4.0f, target.y - 4.0f,
                8.0f, 8.0f);
            if (hovered)
                hoveredEdge = edge.id;
        }

        std::optional<std::uint64_t> hoveredNode{};
        for (const NodeGraphCanvasNode& node : options.nodes)
        {
            const auto projected = projectedNodes.find(node.id);
            if (projected == projectedNodes.end())
                continue;
            const auto& projection = *projected->second;
            const Vec2 nodePosition{
                static_cast<float>(projection.screen_bounds.x),
                static_cast<float>(projection.screen_bounds.y)};
            const float nodeWidth = static_cast<float>(
                projection.screen_bounds.width);
            const float nodeHeight = static_cast<float>(
                projection.screen_bounds.height);
            const bool hovered = options.enabled && node.enabled
                && point_in_rect(
                    g_frame.mousePos,
                    nodePosition.x,
                    nodePosition.y,
                    nodeWidth,
                    nodeHeight)
                && point_in_active_clip(g_frame.mousePos);
            SpriteHandle fill = palette.textField;
            switch (node.role)
            {
            case NodeGraphNodeRole::document:
                fill = palette.textFieldActive;
                break;
            case NodeGraphNodeRole::container:
                fill = palette.buttonNormal;
                break;
            case NodeGraphNodeRole::action:
                fill = palette.buttonActive;
                break;
            case NodeGraphNodeRole::control:
            default:
                break;
            }
            if (!node.enabled)
                fill = palette.panelBackground;
            else if (hovered)
                fill = palette.buttonHover;
            draw_sprite(
                fill,
                nodePosition.x,
                nodePosition.y,
                nodeWidth,
                nodeHeight);

            const bool selected = node.selected || projection.selected;
            const SpriteHandle border = selected
                ? palette.textFieldActive
                : palette.titleBar;
            const float borderWidth = selected ? 3.0f : 2.0f;
            draw_sprite(border, nodePosition.x, nodePosition.y,
                nodeWidth, borderWidth);
            draw_sprite(border, nodePosition.x,
                nodePosition.y + nodeHeight - borderWidth,
                nodeWidth, borderWidth);
            draw_sprite(border, nodePosition.x, nodePosition.y,
                borderWidth, nodeHeight);
            draw_sprite(border,
                nodePosition.x + nodeWidth - borderWidth,
                nodePosition.y, borderWidth, nodeHeight);
            draw_sprite(
                palette.textFieldActive,
                nodePosition.x - 4.0f,
                nodePosition.y + nodeHeight * 0.5f - 4.0f,
                8.0f, 8.0f);
            draw_sprite(
                palette.buttonActive,
                nodePosition.x + nodeWidth - 4.0f,
                nodePosition.y + nodeHeight * 0.5f - 4.0f,
                8.0f, 8.0f);

            const std::string title = fit_text_to_width(
                node.title,
                (std::max)(1.0f, nodeWidth - 18.0f),
                kFontScale);
            draw_text_line(
                title.empty() ? node.title : std::string_view{title},
                nodePosition.x + 9.0f,
                nodePosition.y + 6.0f,
                kFontScale);
            if (!node.subtitle.empty() && nodeHeight >= 44.0f)
            {
                const std::string subtitle = fit_text_to_width(
                    node.subtitle,
                    (std::max)(1.0f, nodeWidth - 18.0f),
                    kFontScale * 0.82f);
                draw_text_line(
                    subtitle.empty()
                        ? node.subtitle
                        : std::string_view{subtitle},
                    nodePosition.x + 9.0f,
                    nodePosition.y + nodeHeight - 18.0f,
                    kFontScale * 0.82f);
            }
            if (hovered)
                hoveredNode = node.id;
        }

        const std::size_t pressKey = widget_press_key(
            options.id, origin, options.size);
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        const auto nodeForPin = [&](graph_workspace::PinId id)
            -> std::optional<std::uint64_t>
        {
            const auto pins = graphState.controller.pins();
            const auto found = std::ranges::find(
                pins, id, &graph_workspace::PinLayout::id);
            return found == pins.end()
                ? std::nullopt
                : std::optional<std::uint64_t>{found->node_id};
        };
        const auto commitIntent = [&](
            const graph_workspace::WorkspaceIntent& intent)
        {
            if (intent.phase != graph_workspace::IntentPhase::commit)
                return;
            if (intent.kind == graph_workspace::IntentKind::move_nodes
                && options.allow_node_movement)
            {
                for (const std::uint64_t id : intent.node_ids)
                {
                    auto& offset = graphState.nodeOffsets[id];
                    offset.x = (std::clamp)(
                        offset.x + intent.world_delta.x,
                        -1'000'000.0,
                        1'000'000.0);
                    offset.y = (std::clamp)(
                        offset.y + intent.world_delta.y,
                        -1'000'000.0,
                        1'000'000.0);
                    result.moved_nodes.push_back({
                        .id = id,
                        .delta = {
                            static_cast<float>(intent.world_delta.x),
                            static_cast<float>(intent.world_delta.y)}
                    });
                }
                result.layout_changed = !result.moved_nodes.empty();
                return;
            }
            if (intent.kind == graph_workspace::IntentKind::connect_pins
                && options.allow_connections
                && intent.valid_target)
            {
                const auto source = nodeForPin(intent.output_pin_id);
                const auto target = nodeForPin(intent.input_pin_id);
                if (source && target && *source != *target)
                {
                    result.connection = NodeGraphCanvasConnection{
                        .source_node = *source,
                        .target_node = *target};
                }
                return;
            }
            if (intent.kind
                    == graph_workspace::IntentKind::disconnect_edges
                && options.allow_disconnection)
            {
                result.disconnected_edges.assign(
                    intent.edge_ids.begin(), intent.edge_ids.end());
            }
        };

        if (options.interactive && options.allow_disconnection
            && result.hovered && right_press_available())
        {
            const auto pointer = graphState.controller.pointer_down({
                .screen_position = {g_frame.mousePos.x, g_frame.mousePos.y},
                .button = graph_workspace::PointerButton::secondary,
                .disconnect_gesture = true});
            commitIntent(pointer.intent);
            if (pointer.hit.kind == graph_workspace::HitKind::edge)
                result.clicked_edge = pointer.hit.edge_id;
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            consume_right_press();
        }
        if (options.interactive && result.hovered && left_press_available())
        {
            const auto hit = graphState.controller.hit_test(
                {g_frame.mousePos.x, g_frame.mousePos.y});
            const auto pointer = graphState.controller.pointer_down({
                .screen_position = {g_frame.mousePos.x, g_frame.mousePos.y},
                .pan_gesture = hit.kind == graph_workspace::HitKind::none});
            if (hit.kind == graph_workspace::HitKind::node)
                result.clicked_node = hit.node_id;
            else if (hit.kind == graph_workspace::HitKind::pin)
                result.clicked_node = hit.node_id;
            else if (hit.kind == graph_workspace::HitKind::edge)
                result.clicked_edge = hit.edge_id;
            else
                result.clicked_background = true;
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            result.view_changed = result.view_changed || pointer.view_changed;
            pressedKey = pressKey;
            consume_left_press();
        }
        if (options.interactive && graphState.controller.pointer_active()
            && g_frame.mouseDown && pressedKey == pressKey)
        {
            const auto pointer = graphState.controller.pointer_move(
                {g_frame.mousePos.x, g_frame.mousePos.y});
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            result.view_changed = result.view_changed || pointer.view_changed;
        }
        if (options.interactive && graphState.controller.pointer_active()
            && g_frame.justReleased && pressedKey == pressKey)
        {
            const auto pointer = graphState.controller.pointer_up(
                {g_frame.mousePos.x, g_frame.mousePos.y});
            commitIntent(pointer.intent);
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            result.view_changed = result.view_changed || pointer.view_changed;
            if (left_release_available())
                consume_left_release();
            pressedKey = 0u;
        }

        result.selected_nodes.assign(
            graphState.controller.selected_node_ids().begin(),
            graphState.controller.selected_node_ids().end());
        const auto finalView = graphState.controller.view();
        result.pan = {
            static_cast<float>(finalView.pan.x),
            static_cast<float>(finalView.pan.y)};
        result.zoom = static_cast<float>(finalView.zoom);

        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({0.0f, height + kContentPadding});
        return result;
    }

    SliderResult slider(const SliderOptions& options) noexcept
    {
        SliderResult result{};
        result.value = options.value;
        if (!g_frame.insideWindow || !g_frame.ctx || options.id.empty()
            || !std::isfinite(options.minimum)
            || !std::isfinite(options.maximum)
            || !std::isfinite(options.value)
            || !std::isfinite(options.step)
            || options.maximum <= options.minimum || options.step <= 0.0f)
        {
            return result;
        }

        const Vec2 position = g_frame.cursor;
        const float width = (std::max)(options.size.x, 40.0f);
        const float height = (std::max)(
            options.size.y, base_line_height(kFontScale) + 8.0f);
        result.bounds = {position, {width, height}};
        const gui_lib::SliderLayoutOptions layoutOptions{
            .bounds = {
                {position.x, position.y},
                {width, height}},
            .minimum = options.minimum,
            .maximum = options.maximum,
            .value = options.value,
            .step = options.step
        };
        if (!gui_lib::make_slider_layout(layoutOptions).valid)
            return result;
        result.hovered = options.enabled
            && point_in_rect(
                g_frame.mousePos, position.x, position.y, width, height)
            && point_in_active_clip(g_frame.mousePos);

        const std::size_t pressKey = widget_press_key(
            options.id, position, {width, height});
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (result.hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }
        const bool active = options.enabled && g_frame.mouseDown
            && pressedKey == pressKey;
        if (active || (result.hovered && g_frame.justReleased
                && pressedKey == pressKey))
        {
            result.value = gui_lib::slider_value_from_position(
                layoutOptions,
                g_frame.mousePos.x);
            result.changed = result.value != options.value;
        }
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (result.hovered)
                consume_left_release();
            pressedKey = 0u;
        }

        const auto& palette = active_palette();
        draw_sprite(
            options.enabled ? palette.textField : palette.panelBackground,
            position.x, position.y, width, height);
        auto renderedLayoutOptions = layoutOptions;
        renderedLayoutOptions.value = result.value;
        const gui_lib::SliderLayout layout =
            gui_lib::make_slider_layout(renderedLayoutOptions);
        draw_sprite(
            palette.titleBar,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            layout.track.size.y);
        draw_sprite(
            active || result.hovered
                ? palette.textFieldActive : palette.buttonActive,
            layout.fill.position.x,
            layout.fill.position.y,
            layout.fill.size.x,
            layout.fill.size.y);
        draw_sprite(
            palette.buttonHover,
            layout.thumb.position.x,
            layout.thumb.position.y,
            layout.thumb.size.x,
            layout.thumb.size.y);
        if (!options.label.empty())
        {
            const std::string fitted = fit_text_to_width(
                options.label,
                (std::max)(1.0f, width - 2.0f * kContentPadding),
                kFontScale);
            draw_text_line(
                fitted.empty() ? options.label : std::string_view{fitted},
                position.x + kContentPadding,
                position.y + 2.0f,
                kFontScale);
        }
        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({0.0f, height + kContentPadding});
        return result;
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

    RuntimeSurfaceAtlasResult register_runtime_surface_atlas(
        std::span<const RuntimeSurfaceDescriptor> surfaces) noexcept
    {
        RuntimeSurfaceAtlasResult result{};
        result.descriptor_count = surfaces.size();
        try
        {
            result.handles.resize(surfaces.size());
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }

        if (surfaces.empty())
            return result;
        if (surfaces.size() > runtime_surface_batch_maximum_entries)
        {
            result.status = RuntimeSurfaceBatchStatus::BatchLimitExceeded;
            return result;
        }

        struct PreparedSurface
        {
            const RuntimeSurfaceDescriptor* descriptor{};
            std::size_t inputIndex{};
            std::string key{};
            std::uint64_t contentHash{};
        };

        std::vector<PreparedSurface> prepared{};
        result.status = RuntimeSurfaceBatchStatus::Ready;
        try
        {
            prepared.reserve(surfaces.size());
            std::size_t totalBytes = 0u;
            for (std::size_t index = 0u;
                 index < surfaces.size();
                 ++index)
            {
                const RuntimeSurfaceDescriptor& surface =
                    surfaces[index];
                const auto reject = [&](RuntimeSurfaceBatchStatus status)
                {
                    if (result.status == RuntimeSurfaceBatchStatus::Ready)
                        result.status = status;
                };

                if (surface.id.empty()
                    || surface.id.size()
                        > runtime_surface_identifier_maximum_bytes
                    || surface.width == 0u
                    || surface.height == 0u
                    || surface.width > runtime_surface_maximum_extent
                    || surface.height > runtime_surface_maximum_extent)
                {
                    reject(RuntimeSurfaceBatchStatus::InvalidDescriptor);
                    continue;
                }

                const std::uint64_t expectedBytes =
                    static_cast<std::uint64_t>(surface.width)
                    * static_cast<std::uint64_t>(surface.height) * 4ull;
                if (expectedBytes != surface.rgba_pixels.size())
                {
                    reject(RuntimeSurfaceBatchStatus::InvalidDescriptor);
                    continue;
                }
                if (expectedBytes
                        > runtime_surface_batch_maximum_rgba_bytes
                    || totalBytes
                        > runtime_surface_batch_maximum_rgba_bytes
                            - static_cast<std::size_t>(expectedBytes))
                {
                    reject(RuntimeSurfaceBatchStatus::ByteBudgetExceeded);
                    continue;
                }

                const auto duplicate = std::find_if(
                    prepared.begin(),
                    prepared.end(),
                    [&](const PreparedSurface& accepted) noexcept
                    {
                        return accepted.key == surface.id;
                    });
                if (duplicate != prepared.end())
                {
                    reject(RuntimeSurfaceBatchStatus::DuplicateIdentifier);
                    continue;
                }

                totalBytes += static_cast<std::size_t>(expectedBytes);
                prepared.push_back(PreparedSurface{
                    .descriptor = &surface,
                    .inputIndex = index,
                    .key = std::string{surface.id},
                    .contentHash = hash_surface_pixels(
                        surface.rgba_pixels,
                        surface.width,
                        surface.height)
                });
            }
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }

        if (result.status != RuntimeSurfaceBatchStatus::Ready
            || prepared.empty())
            return result;

        try
        {
            ensure_resources();
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }

        try
        {
            std::scoped_lock locks(
                g_runtimeSurfaceCacheMutex,
                g_resourceMutex);
            TextureAtlas* runtimeAtlas =
                ensure_runtime_surface_atlas_locked();
            if (!runtimeAtlas)
            {
                result.status =
                    RuntimeSurfaceBatchStatus::ResourceUnavailable;
                return result;
            }

            bool mutationFailed = false;
            try
            {
                for (const PreparedSurface& surface : prepared)
                {
                    const RuntimeSurfaceDescriptor& descriptor =
                        *surface.descriptor;
                    auto entryIt =
                        g_runtimeSurfaceCache.find(surface.key);
                    if (entryIt != g_runtimeSurfaceCache.end()
                        && entryIt->second.handle.is_valid()
                        && entryIt->second.contentHash
                            == surface.contentHash
                        && entryIt->second.width == descriptor.width
                        && entryIt->second.height == descriptor.height)
                    {
                        result.handles[surface.inputIndex] =
                            entryIt->second.handle;
                        ++result.accepted_count;
                        ++result.unchanged_count;
                        continue;
                    }

                    if (entryIt != g_runtimeSurfaceCache.end()
                        && entryIt->second.handle.is_valid()
                        && entryIt->second.width == descriptor.width
                        && entryIt->second.height == descriptor.height
                        && !entryIt->second.spriteName.empty())
                    {
                        Texture texture{};
                        texture.name = entryIt->second.spriteName;
                        texture.width = descriptor.width;
                        texture.height = descriptor.height;
                        texture.channels = 4;
                        texture.pixels.assign(
                            descriptor.rgba_pixels.begin(),
                            descriptor.rgba_pixels.end());
                        if (!runtimeAtlas->replace_entry_pixels(
                                entryIt->second.spriteName,
                                texture))
                        {
                            mutationFailed = true;
                            break;
                        }

                        entryIt->second.contentHash =
                            surface.contentHash;
                        ++entryIt->second.version;
                        result.handles[surface.inputIndex] =
                            entryIt->second.handle;
                        ++result.accepted_count;
                        ++result.replaced_count;
                        continue;
                    }

                    const std::uint32_t nextVersion =
                        entryIt == g_runtimeSurfaceCache.end()
                            ? 1u
                            : entryIt->second.version + 1u;
                    std::vector<std::uint8_t> pixels(
                        descriptor.rgba_pixels.begin(),
                        descriptor.rgba_pixels.end());
                    std::string spriteName =
                        "__agui/runtime_surface/" + surface.key + "/"
                        + std::to_string(nextVersion);
                    SpriteHandle handle = try_add_sprite(
                        *runtimeAtlas,
                        spriteName,
                        pixels,
                        descriptor.width,
                        descriptor.height,
                        false);
                    if (!handle.is_valid())
                    {
                        mutationFailed = true;
                        break;
                    }

                    CachedRuntimeSurface cacheEntry{
                        .handle = handle,
                        .contentHash = surface.contentHash,
                        .width = descriptor.width,
                        .height = descriptor.height,
                        .version = nextVersion,
                        .spriteName = std::move(spriteName)
                    };
                    if (entryIt == g_runtimeSurfaceCache.end())
                    {
                        g_runtimeSurfaceCache.emplace(
                            surface.key,
                            std::move(cacheEntry));
                    }
                    else
                    {
                        entryIt->second = std::move(cacheEntry);
                    }

                    result.handles[surface.inputIndex] = handle;
                    ++result.accepted_count;
                    ++result.added_count;
                }
            }
            catch (...)
            {
                result.status =
                    RuntimeSurfaceBatchStatus::ResourceUnavailable;
            }

            if (mutationFailed)
            {
                result.status =
                    RuntimeSurfaceBatchStatus::AtlasMutationFailed;
            }

            try
            {
                epochengine::atlasmanager::ensure_uploaded(
                    *runtimeAtlas);
                result.uploaded = true;
            }
            catch (...)
            {
                result.status = RuntimeSurfaceBatchStatus::UploadFailed;
            }
            return result;
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }
    }

    [[nodiscard]] static bool source_editor_navigation_contract() noexcept;

    bool run_runtime_surface_contract() noexcept
    {
        if (!source_editor_navigation_contract())
            return false;
        constexpr std::string_view utf8Sample{
            "A\xE2\x96\x88\xF0\x9F\x9A\x80"};
        const DecodedUtf8Codepoint ascii =
            decode_utf8_codepoint(utf8Sample, 0u);
        const DecodedUtf8Codepoint block =
            decode_utf8_codepoint(utf8Sample, 1u);
        const DecodedUtf8Codepoint rocket =
            decode_utf8_codepoint(utf8Sample, 4u);
        constexpr std::string_view malformed{"\xE2\x28\xA1"};
        const DecodedUtf8Codepoint rejected =
            decode_utf8_codepoint(malformed, 0u);
        if (ascii.codepoint != U'A' || ascii.byte_count != 1u
            || block.codepoint != U'\u2588'
            || block.byte_count != 3u
            || rocket.codepoint != U'\U0001F680'
            || rocket.byte_count != 4u
            || rejected.codepoint != U'?'
            || rejected.byte_count != 1u
            || safe_draw_codepoint(block.codepoint) != U'\u2588'
            || safe_draw_codepoint(U'\u2550') != U'\u2550'
            || safe_draw_codepoint(U'\u2551') != U'\u2551'
            || safe_draw_codepoint(U'\u2557') != U'\u2557'
            || next_utf8_codepoint_start(utf8Sample, 0u) != 1u
            || next_utf8_codepoint_start(utf8Sample, 1u) != 4u
            || next_utf8_codepoint_start(utf8Sample, 4u) != 8u
            || previous_utf8_codepoint_start(utf8Sample, 8u) != 4u
            || previous_utf8_codepoint_start(utf8Sample, 4u) != 1u)
        {
            return false;
        }
        constexpr std::array<std::uint8_t, 16> firstPixels{
            255u, 0u, 0u, 255u,
            0u, 255u, 0u, 255u,
            0u, 0u, 255u, 255u,
            255u, 255u, 255u, 255u};
        constexpr std::array<std::uint8_t, 16> changedPixels{
            0u, 0u, 0u, 255u,
            255u, 255u, 0u, 255u,
            0u, 255u, 255u, 255u,
            255u, 0u, 255u, 255u};
        const RuntimeSurfaceDescriptor first{
            .id = "__agui_contract/runtime_surface",
            .rgba_pixels = firstPixels,
            .width = 2u,
            .height = 2u};
        const RuntimeSurfaceAtlasResult initial =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{&first, 1u});
        if (!initial || initial.handles.size() != 1u
            || !initial.handles.front().is_valid())
        {
            return false;
        }

        const RuntimeSurfaceAtlasResult unchanged =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{&first, 1u});
        if (!unchanged || unchanged.unchanged_count != 1u
            || unchanged.handles.front() != initial.handles.front())
        {
            return false;
        }

        const RuntimeSurfaceDescriptor changed{
            .id = first.id,
            .rgba_pixels = changedPixels,
            .width = first.width,
            .height = first.height};
        const RuntimeSurfaceAtlasResult replaced =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{&changed, 1u});
        return replaced
            && replaced.replaced_count == 1u
            && replaced.handles.front() == initial.handles.front();
    }
    SpriteHandle register_runtime_surface(
        std::string_view id,
        std::span<const std::uint8_t> rgba_pixels,
        std::uint32_t width,
        std::uint32_t height) noexcept
    {
        const RuntimeSurfaceDescriptor descriptor{
            .id = id,
            .rgba_pixels = rgba_pixels,
            .width = width,
            .height = height
        };
        RuntimeSurfaceAtlasResult result =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{
                    &descriptor,
                    1u
                });
        return result && !result.handles.empty()
            ? result.handles.front()
            : SpriteHandle{};
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

    float measure_wrapped_label_height(std::string_view text, float width) noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 0.0f; }

        const float wrapWidth = width > 0.0f
            ? (std::max)(space_advance(kFontScale), width)
            : 1.0f;
        return measure_wrapped_text_height(text, wrapWidth, kFontScale);
    }

    [[nodiscard]] static std::size_t edit_box_line_start(
        std::string_view text,
        std::size_t index) noexcept
    {
        index = (std::min)(index, text.size());
        if (index == 0u)
            return 0u;
        const std::size_t newline = text.rfind('\n', index - 1u);
        return newline == std::string_view::npos ? 0u : newline + 1u;
    }

    [[nodiscard]] static std::size_t edit_box_line_end(
        std::string_view text,
        std::size_t index) noexcept
    {
        index = (std::min)(index, text.size());
        const std::size_t newline = text.find('\n', index);
        return newline == std::string_view::npos ? text.size() : newline;
    }

    [[nodiscard]] static float edit_box_x_for_index(
        std::string_view text,
        std::size_t lineStart,
        std::size_t index,
        float scale) noexcept
    {
        lineStart = (std::min)(lineStart, text.size());
        index = (std::min)((std::max)(lineStart, index), text.size());
        return measure_text_width(text.substr(lineStart, index - lineStart), scale);
    }

    [[nodiscard]] static std::size_t edit_box_index_from_point(
        std::string_view text,
        float pointerX,
        float pointerY,
        float textX,
        float textY,
        gui_lib::Vec2 scroll,
        float lineAdvance,
        float scale,
        bool multiline) noexcept
    {
        std::size_t lineStart = 0u;
        if (multiline)
        {
            const float localY = (std::max)(0.0f, pointerY - textY + scroll.y);
            const std::size_t requestedLine =
                static_cast<std::size_t>(localY / (std::max)(1.0f, lineAdvance));
            for (std::size_t line = 0u; line < requestedLine && lineStart < text.size(); ++line)
            {
                const std::size_t lineEnd = edit_box_line_end(text, lineStart);
                lineStart = lineEnd < text.size() ? lineEnd + 1u : text.size();
            }
        }

        const std::size_t lineEnd = edit_box_line_end(text, lineStart);
        const float localX = pointerX - textX + scroll.x;
        if (localX <= 0.0f)
            return lineStart;

        float penX = 0.0f;
        for (std::size_t index = lineStart; index < lineEnd; ++index)
        {
            const std::size_t byteIndex = index;
            const DecodedUtf8Codepoint decoded =
                decode_utf8_codepoint(text, byteIndex);
            const char32_t codepoint =
                safe_draw_codepoint(decoded.codepoint);
            index += (std::max)(
                std::size_t{1u}, decoded.byte_count) - 1u;
            const float advance = glyph_advance_with_kerning(
                codepoint,
                next_drawable_char(text, byteIndex),
                scale);
            if (localX <= penX + advance * 0.5f)
                return byteIndex;
            penX += advance;
        }
        return lineEnd;
    }

    [[nodiscard]] static gui_lib::TextControlMetrics edit_box_text_metrics(
        const gui_lib::TextControlState& state,
        float scale,
        float lineAdvance,
        float caretHeight) noexcept
    {
        const std::string_view text{ state.text };
        float maximumWidth = 0.0f;
        std::size_t lineCount = 1u;
        std::size_t lineStart = 0u;
        while (lineStart <= text.size())
        {
            const std::size_t lineEnd = edit_box_line_end(text, lineStart);
            maximumWidth = (std::max)(
                maximumWidth,
                edit_box_x_for_index(text, lineStart, lineEnd, scale));
            if (lineEnd >= text.size())
                break;
            ++lineCount;
            lineStart = lineEnd + 1u;
        }

        const std::size_t caret = (std::min)(state.caret, text.size());
        const std::size_t caretLineStart = edit_box_line_start(text, caret);
        std::size_t caretLine = 0u;
        for (std::size_t index = 0u; index < caretLineStart; ++index)
            if (text[index] == '\n')
                ++caretLine;

        return gui_lib::TextControlMetrics{
            .content_size = { maximumWidth, static_cast<float>(lineCount) * lineAdvance },
            .caret_position = {
                edit_box_x_for_index(text, caretLineStart, caret, scale),
                static_cast<float>(caretLine) * lineAdvance
            },
            .caret_size = { 1.0f, caretHeight },
            .valid = true
        };
    }

    EditBoxResult edit_box(std::string& text, Vec2 size, std::size_t max_chars, bool multiline) noexcept
    {
        EditBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float lineAdvance = line_advance_amount(kFontScale);
        const float minWidth = space_advance(kFontScale) * 4.0f;
        const float minHeight = baseHeight + 2.0f * kBoxInnerPadding;
        const float width = (std::max)(static_cast<float>(size.x), minWidth);
        const float height = (std::max)(static_cast<float>(size.y), minHeight);
        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding);
        const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
        const float textX = pos.x + kBoxInnerPadding;
        const float textY = pos.y + kBoxInnerPadding;
        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);

        const void* id = widget_focus_key(
            "edit-box",
            static_cast<const void*>(&text),
            pos,
            { width, height });
        const void* textKey = static_cast<const void*>(&text);
        const void* ctxKey = static_cast<const void*>(g_frame.ctx);
        const auto& palette = active_palette();
        auto& widget = g_editBoxTextStates[id];
        auto& menuState = g_editBoxMenuStates[id];

        if (!widget.initialized)
        {
            widget.control.text = text;
            widget.control.anchor = text.size();
            widget.control.caret = text.size();
            widget.initialized = true;
        }
        else if (widget.control.text != text)
        {
            widget.control.text = text;
            gui_lib::normalize_text_control(widget.control);
        }

        const std::size_t limit = max_chars == 0u
            ? (std::numeric_limits<std::size_t>::max)()
            : max_chars;
        const gui_lib::TextControlOptions options{
            .viewport_size = { contentWidth, contentHeight },
            .content_padding = {},
            .maximum_bytes = limit,
            .multiline = multiline,
            .read_only = false,
            .accept_tab = multiline
        };

        const auto metrics = [&]()
        {
            return edit_box_text_metrics(widget.control, kFontScale, lineAdvance, baseHeight);
        };
        const auto dispatch = [&](gui_lib::TextControlCommand command,
                                  std::string_view payload = {},
                                  std::size_t requestedCaret = gui_lib::invalid_text_index,
                                  bool extendSelection = false,
                                  bool focusRequested = false,
                                  bool blurRequested = false)
        {
            gui_lib::TextControlInput input{};
            input.command = command;
            input.text = payload;
            input.requested_caret = requestedCaret;
            input.metrics = metrics();
            input.extend_selection = extendSelection;
            input.focus_requested = focusRequested;
            input.blur_requested = blurRequested;
            const auto update = gui_lib::update_text_control(widget.control, options, input);
            if (update.clipboard_write_requested)
                (void)clipboard_write_text(update.clipboard_text);
            result.changed = result.changed || update.text_changed;
            return update;
        };

        const bool leftPressed = g_frame.justPressed;
        const bool pointerPressed = leftPressed || g_frame.rightJustPressed;
        const bool shiftHeld = std::any_of(
            g_frame.events.begin(),
            g_frame.events.end(),
            [](const InputEvent& event) noexcept
            {
                return event.shift_down;
            });
        const void* activeBefore = ctxKey ? g_contextActiveWidgets[ctxKey] : nullptr;
        if (pointerPressed)
        {
            if (hovered)
            {
                if (ctxKey)
                    g_contextActiveWidgets[ctxKey] = id;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
            }
            else if (activeBefore == id && !menuState.open)
            {
                if (ctxKey)
                    g_contextActiveWidgets[ctxKey] = nullptr;
                widget.draggingSelection = false;
            }
        }

        bool active = ctxKey && g_contextActiveWidgets[ctxKey] == id;
        if (active != widget.control.focused)
            (void)dispatch(gui_lib::TextControlCommand::none, {}, gui_lib::invalid_text_index, false, active, !active);

        if (g_textFieldSelectAllStates[textKey])
        {
            if (ctxKey)
                g_contextActiveWidgets[ctxKey] = id;
            active = true;
            (void)dispatch(gui_lib::TextControlCommand::select_all, {}, gui_lib::invalid_text_index, false, true, false);
            g_textFieldSelectAllStates[textKey] = false;
            g_frame.caretTimer = 0.0f;
            g_frame.caretVisible = true;
        }

        if (active && leftPressed && hovered)
        {
            const std::size_t requested = edit_box_index_from_point(
                widget.control.text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                textY,
                widget.control.scroll,
                lineAdvance,
                kFontScale,
                multiline);
            (void)dispatch(
                gui_lib::TextControlCommand::set_caret,
                {},
                requested,
                shiftHeld);
            widget.draggingSelection = true;
        }
        else if (active && widget.draggingSelection && g_frame.mouseDown)
        {
            const std::size_t requested = edit_box_index_from_point(
                widget.control.text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                textY,
                widget.control.scroll,
                lineAdvance,
                kFontScale,
                multiline);
            (void)dispatch(
                gui_lib::TextControlCommand::set_caret,
                {},
                requested,
                true);
        }
        if (!g_frame.mouseDown)
            widget.draggingSelection = false;

        if (active)
        {
            for (const auto& event : g_frame.events)
            {
                if (event.type == EventType::TextInput)
                {
                    (void)dispatch(gui_lib::TextControlCommand::insert_text, event.text);
                    continue;
                }
                if (event.type != EventType::KeyDown)
                    continue;

                const int key = event.key;
                if (event.ctrl_down && (key == 'A' || key == 'a'))
                    (void)dispatch(gui_lib::TextControlCommand::select_all);
                else if (event.ctrl_down && (key == 'C' || key == 'c'))
                    (void)dispatch(gui_lib::TextControlCommand::copy_selection);
                else if (event.ctrl_down && (key == 'X' || key == 'x'))
                    (void)dispatch(gui_lib::TextControlCommand::cut_selection);
                else if (event.ctrl_down && (key == 'V' || key == 'v'))
                    (void)dispatch(gui_lib::TextControlCommand::paste_text, clipboard_read_text());
                else if (key == 8)
                    (void)dispatch(gui_lib::TextControlCommand::erase_backward);
                else if (key == 46 || key == 127)
                    (void)dispatch(gui_lib::TextControlCommand::erase_forward);
                else if (key == 37)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_word_left
                                        : gui_lib::TextControlCommand::move_left,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 39)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_word_right
                                        : gui_lib::TextControlCommand::move_right,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 38 && multiline)
                    (void)dispatch(gui_lib::TextControlCommand::move_up, {}, gui_lib::invalid_text_index, event.shift_down);
                else if (key == 40 && multiline)
                    (void)dispatch(gui_lib::TextControlCommand::move_down, {}, gui_lib::invalid_text_index, event.shift_down);
                else if (key == 36)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_document_start
                                        : gui_lib::TextControlCommand::move_line_start,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 35)
                    (void)dispatch(
                        event.ctrl_down ? gui_lib::TextControlCommand::move_document_end
                                        : gui_lib::TextControlCommand::move_line_end,
                        {},
                        gui_lib::invalid_text_index,
                        event.shift_down);
                else if (key == 27)
                {
                    if (ctxKey)
                        g_contextActiveWidgets[ctxKey] = nullptr;
                    active = false;
                    widget.draggingSelection = false;
                    (void)dispatch(gui_lib::TextControlCommand::none, {}, gui_lib::invalid_text_index, false, false, true);
                }
                else if (key == 13)
                {
                    if (multiline)
                        (void)dispatch(gui_lib::TextControlCommand::insert_text, "\n");
                    else
                        result.submitted = true;
                }
            }
        }

        gui_lib::update_text_control_scroll(widget.control, options, metrics());
        text = widget.control.text;
        result.active = active;

        draw_sprite(active ? palette.textFieldActive : palette.textField, pos.x, pos.y, width, height);
        {
            ContentClipScope clip(
                { pos.x + 1.0f, pos.y + 1.0f },
                { pos.x + width - 1.0f, pos.y + height - 1.0f });
            const auto selection = gui_lib::text_selection(widget.control);
            const std::string_view view{ widget.control.text };
            std::size_t lineStart = 0u;
            std::size_t lineIndex = 0u;
            while (lineStart <= view.size())
            {
                const std::size_t lineEnd = edit_box_line_end(view, lineStart);
                const float drawX = textX - widget.control.scroll.x;
                const float drawY = textY
                    + static_cast<float>(lineIndex) * lineAdvance
                    - widget.control.scroll.y;

                if (!selection.empty() && selection.past_last >= lineStart && selection.first <= lineEnd)
                {
                    const std::size_t highlightBegin = (std::max)(selection.first, lineStart);
                    const std::size_t highlightEnd = (std::min)(selection.past_last, lineEnd);
                    const float highlightX = drawX
                        + edit_box_x_for_index(view, lineStart, highlightBegin, kFontScale);
                    float highlightWidth =
                        edit_box_x_for_index(view, lineStart, highlightEnd, kFontScale)
                        - edit_box_x_for_index(view, lineStart, highlightBegin, kFontScale);
                    if (selection.past_last > lineEnd && highlightEnd == lineEnd)
                        highlightWidth += space_advance(kFontScale) * 0.75f;
                    if (highlightWidth > 0.0f)
                        draw_sprite(
                            palette.buttonActive,
                            highlightX,
                            drawY - 1.0f,
                            highlightWidth,
                            baseHeight + 2.0f);
                }

                draw_text_line(view.substr(lineStart, lineEnd - lineStart), drawX, drawY, kFontScale);
                if (!multiline || lineEnd >= view.size())
                    break;
                lineStart = lineEnd + 1u;
                ++lineIndex;
            }

            if (active)
            {
                const std::size_t caret = (std::min)(widget.control.caret, view.size());
                const std::size_t caretLineStart = edit_box_line_start(view, caret);
                std::size_t caretLine = 0u;
                for (std::size_t index = 0u; index < caretLineStart; ++index)
                    if (view[index] == '\n')
                        ++caretLine;

                const float caretX = textX - widget.control.scroll.x
                    + edit_box_x_for_index(view, caretLineStart, caret, kFontScale);
                const float caretY = textY - widget.control.scroll.y
                    + static_cast<float>(caretLine) * lineAdvance;
                draw_caret(caretX, caretY, (std::min)(contentHeight, baseHeight));
            }
        }

        bool openedContextMenuThisFrame = false;
        if (g_frame.rightJustPressed && hovered)
        {
            menuState.open = true;
            menuState.pos = g_frame.mousePos;
            openedContextMenuThisFrame = true;
        }

        if (menuState.open)
        {
            const float rowHeight = 28.0f;
            const float menuPadding = 4.0f;
            const float menuWidth = 172.0f;
            const float menuHeight = menuPadding * 2.0f + rowHeight * 4.0f + kContentPadding * 3.0f;
            Vec2 menuPos = menuState.pos;
            menuPos.x = (std::min)(menuPos.x, (std::max)(0.0f, g_frame.origin.x + g_frame.windowSize.x - menuWidth - kContentPadding));
            menuPos.y = (std::min)(menuPos.y, (std::max)(0.0f, g_frame.origin.y + g_frame.windowSize.y - menuHeight - kContentPadding));

            const bool hoveredMenu = point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y, menuWidth, menuHeight);
            if (!openedContextMenuThisFrame && (g_frame.justPressed || g_frame.rightJustPressed) && !hoveredMenu)
                menuState.open = false;

            if (menuState.open)
            {
                const Vec2 savedCursor = g_frame.cursor;
                const Vec2 savedOrigin = g_frame.origin;
                const Vec2 savedWindowSize = g_frame.windowSize;
                const Vec2 savedContentMin = g_frame.contentMin;
                const Vec2 savedContentMax = g_frame.contentMax;
                const bool savedInsideWindow = g_frame.insideWindow;
                const std::string savedWindowKey = g_frame.windowKey;
                const std::uint64_t savedWidgetSerial = g_frame.widgetSerial;

                begin_top_layer();
                const auto& menuPalette = active_palette();
                draw_sprite(menuPalette.windowBackground, menuPos.x, menuPos.y, menuWidth, menuHeight);
                draw_sprite(menuPalette.titleBar, menuPos.x, menuPos.y, menuWidth, 2.0f);
                draw_sprite(menuPalette.titleBar, menuPos.x, menuPos.y, 2.0f, menuHeight);
                draw_sprite(menuPalette.panelBackground, menuPos.x + menuWidth - 2.0f, menuPos.y, 2.0f, menuHeight);
                draw_sprite(menuPalette.panelBackground, menuPos.x, menuPos.y + menuHeight - 2.0f, menuWidth, 2.0f);

                g_frame.insideWindow = true;
                g_frame.windowKey = std::string("edit-box-context-menu-")
                    + std::to_string(reinterpret_cast<std::uintptr_t>(id));
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                set_cursor(g_frame.contentMin);

                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::select_all);
                    menuState.open = false;
                }
                if (button("Copy", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::copy_selection);
                    menuState.open = false;
                }
                if (button("Cut", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::cut_selection);
                    menuState.open = false;
                }
                if (button("Paste", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)dispatch(gui_lib::TextControlCommand::paste_text, clipboard_read_text());
                    menuState.open = false;
                }
                text = widget.control.text;

                g_frame.cursor = savedCursor;
                g_frame.origin = savedOrigin;
                g_frame.windowSize = savedWindowSize;
                g_frame.contentMin = savedContentMin;
                g_frame.contentMax = savedContentMax;
                g_frame.insideWindow = savedInsideWindow;
                g_frame.windowKey = savedWindowKey;
                g_frame.widgetSerial = savedWidgetSerial;
                end_top_layer();
            }
        }

        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    void select_all_text_in_edit_box(std::string& text) noexcept
    {
        g_textFieldSelectAllStates[static_cast<const void*>(&text)] = true;
        g_frame.caretTimer = 0.0f;
        g_frame.caretVisible = true;
    }
    [[nodiscard]] static std::size_t line_count_for_text(std::string_view text) noexcept
    {
        if (text.empty())
            return 1u;

        std::size_t count = 1u;
        for (const char ch : text)
            if (ch == '\n')
                ++count;
        return count;
    }

    [[nodiscard]] static std::size_t line_start_for_index(std::string_view text, std::size_t targetLine) noexcept
    {
        if (targetLine == 0u)
            return 0u;

        std::size_t line = 0u;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\n')
            {
                ++line;
                if (line == targetLine)
                    return (std::min)(i + 1u, text.size());
            }
        }

        return text.size();
    }

    [[nodiscard]] static std::string_view line_view_from_offset(std::string_view text, std::size_t& offset) noexcept
    {
        if (offset >= text.size())
            return {};

        const std::size_t start = offset;
        while (offset < text.size() && text[offset] != '\n')
            ++offset;

        const std::size_t end = offset;
        if (offset < text.size() && text[offset] == '\n')
            ++offset;

        return text.substr(start, end - start);
    }

    [[nodiscard]] static std::pair<std::size_t, std::size_t> source_editor_selection_range(
        const SourceEditorState& state,
        std::size_t textSize) noexcept
    {
        if (!state.hasSelection || state.cursorIndex == state.selectionAnchor)
            return { 0u, 0u };

        const std::size_t begin = (std::min)(state.cursorIndex, state.selectionAnchor);
        const std::size_t end = (std::max)(state.cursorIndex, state.selectionAnchor);
        return {
            (std::min)(begin, textSize),
            (std::min)(end, textSize)
        };
    }

    [[nodiscard]] static bool source_editor_has_selection(const SourceEditorState& state, std::size_t textSize) noexcept
    {
        const auto [begin, end] = source_editor_selection_range(state, textSize);
        return end > begin;
    }

    static void clamp_source_editor_cursor(SourceEditorState& state, std::size_t textSize) noexcept
    {
        state.cursorIndex = (std::min)(state.cursorIndex, textSize);
        state.selectionAnchor = (std::min)(state.selectionAnchor, textSize);
        state.hasSelection = state.hasSelection && state.cursorIndex != state.selectionAnchor;
    }

    [[nodiscard]] static std::size_t source_editor_line_end_for_start(std::string_view text, std::size_t lineStart) noexcept
    {
        std::size_t end = (std::min)(lineStart, text.size());
        while (end < text.size() && text[end] != '\n')
            ++end;
        return end;
    }

    [[nodiscard]] static float source_editor_x_for_index(
        std::string_view text,
        std::size_t lineStart,
        std::size_t index,
        float scale) noexcept
    {
        index = (std::min)(index, text.size());
        lineStart = (std::min)(lineStart, index);
        return measure_text_width(text.substr(lineStart, index - lineStart), scale);
    }

    [[nodiscard]] static std::size_t source_editor_index_from_point(
        std::string_view text,
        float pointerX,
        float pointerY,
        float textX,
        float firstTextY,
        float scrollY,
        float lineAdvance,
        float scale) noexcept
    {
        const float relativeY = (std::max)(0.0f, pointerY - firstTextY + scrollY);
        const std::size_t lineIndex = static_cast<std::size_t>(relativeY / (std::max)(1.0f, lineAdvance));
        const std::size_t lineStart = line_start_for_index(text, lineIndex);
        const std::size_t lineEnd = source_editor_line_end_for_start(text, lineStart);
        const float localX = pointerX - textX;

        if (localX <= 0.0f)
            return lineStart;

        float penX = 0.0f;
        for (std::size_t i = lineStart; i < lineEnd; ++i)
        {
            const std::size_t byteIndex = i;
            const DecodedUtf8Codepoint decoded =
                decode_utf8_codepoint(text, byteIndex);
            const char32_t codepoint =
                safe_draw_codepoint(decoded.codepoint);
            i += (std::max)(
                std::size_t{1u}, decoded.byte_count) - 1u;

            const float advance = glyph_advance_with_kerning(
                codepoint,
                next_drawable_char(text, byteIndex),
                scale);
            if (localX <= penX + advance * 0.5f)
                return byteIndex;
            penX += advance;
        }

        return lineEnd;
    }

    static void source_editor_set_cursor(SourceEditorState& state, std::size_t index, bool extendSelection, std::size_t textSize) noexcept
    {
        index = (std::min)(index, textSize);
        if (!extendSelection)
            state.selectionAnchor = index;
        state.cursorIndex = index;
        state.hasSelection = state.cursorIndex != state.selectionAnchor;
        state.verticalColumnActive = false;
    }

    [[nodiscard]] static std::size_t source_editor_line_for_index(
        const std::string_view text,
        const std::size_t index) noexcept
    {
        return static_cast<std::size_t>(std::count(
            text.begin(),
            text.begin() + static_cast<std::ptrdiff_t>(
                (std::min)(index, text.size())),
            '\n'));
    }

    [[nodiscard]] static std::size_t source_editor_line_start_for_cursor(
        const std::string_view text,
        const std::size_t index) noexcept
    {
        const std::size_t clamped = (std::min)(index, text.size());
        if (clamped == 0u)
            return 0u;
        const std::size_t newline = text.rfind('\n', clamped - 1u);
        return newline == std::string_view::npos ? 0u : newline + 1u;
    }

    [[nodiscard]] static std::size_t source_editor_column_for_index(
        const std::string_view text,
        const std::size_t lineStart,
        const std::size_t index) noexcept
    {
        std::size_t column{};
        for (std::size_t cursor = (std::min)(lineStart, text.size());
            cursor < (std::min)(index, text.size());)
        {
            cursor = next_utf8_codepoint_start(text, cursor);
            ++column;
        }
        return column;
    }

    [[nodiscard]] static std::size_t source_editor_index_for_column(
        const std::string_view text,
        const std::size_t lineStart,
        const std::size_t column) noexcept
    {
        const std::size_t lineEnd = source_editor_line_end_for_start(
            text, lineStart);
        std::size_t cursor = (std::min)(lineStart, lineEnd);
        for (std::size_t current{};
            current < column && cursor < lineEnd;
            ++current)
        {
            cursor = next_utf8_codepoint_start(text, cursor);
        }
        return (std::min)(cursor, lineEnd);
    }

    static void source_editor_move_vertical(
        SourceEditorState& state,
        const std::string_view text,
        const std::ptrdiff_t lineDelta,
        const bool extendSelection) noexcept
    {
        const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
        const std::size_t currentLine = source_editor_line_for_index(text, cursor);
        const std::size_t lineCount = line_count_for_text(text);
        if (!state.verticalColumnActive)
        {
            state.preferredColumn = source_editor_column_for_index(
                text,
                source_editor_line_start_for_cursor(text, cursor),
                cursor);
            state.verticalColumnActive = true;
        }

        const std::ptrdiff_t targetSigned = (std::clamp)(
            static_cast<std::ptrdiff_t>(currentLine) + lineDelta,
            std::ptrdiff_t{0},
            static_cast<std::ptrdiff_t>(lineCount - 1u));
        const std::size_t targetLine = static_cast<std::size_t>(targetSigned);
        const std::size_t target = source_editor_index_for_column(
            text,
            line_start_for_index(text, targetLine),
            state.preferredColumn);
        if (!extendSelection)
            state.selectionAnchor = target;
        state.cursorIndex = target;
        state.hasSelection = state.cursorIndex != state.selectionAnchor;
    }

    [[nodiscard]] static float source_editor_clamped_scroll(
        const float requested,
        const float contentExtent,
        const float viewportExtent) noexcept
    {
        return (std::clamp)(
            requested,
            0.0f,
            (std::max)(0.0f, contentExtent - viewportExtent));
    }

    [[nodiscard]] bool source_editor_navigation_contract() noexcept
    {
        constexpr std::string_view text{"ab\nx\n1234\n\xe7\x95\x8cz"};
        SourceEditorState state{
            .cursorIndex = 2u,
            .selectionAnchor = 2u};
        source_editor_move_vertical(state, text, 1, false);
        if (state.cursorIndex != 4u || state.preferredColumn != 2u)
            return false;
        source_editor_move_vertical(state, text, 1, false);
        if (state.cursorIndex != 7u)
            return false;
        source_editor_move_vertical(state, text, -50, true);
        if (state.cursorIndex != 2u || !state.hasSelection)
            return false;
        source_editor_move_vertical(state, text, 50, false);
        if (source_editor_line_for_index(text, state.cursorIndex) != 3u
            || source_editor_column_for_index(
                text,
                source_editor_line_start_for_cursor(text, state.cursorIndex),
                state.cursorIndex) != 2u)
        {
            return false;
        }
        return source_editor_clamped_scroll(-12.0f, 500.0f, 100.0f) == 0.0f
            && source_editor_clamped_scroll(900.0f, 500.0f, 100.0f) == 400.0f
            && source_editor_clamped_scroll(20.0f, 80.0f, 100.0f) == 0.0f;
    }

    [[nodiscard]] static std::string source_editor_selected_text(std::string_view text, const SourceEditorState& state)
    {
        const auto [begin, end] = source_editor_selection_range(state, text.size());
        if (end <= begin)
            return std::string(text);
        return std::string(text.substr(begin, end - begin));
    }

    static bool source_editor_delete_selection(std::string& text, SourceEditorState& state)
    {
        const auto [begin, end] = source_editor_selection_range(state, text.size());
        if (end <= begin)
        {
            state.hasSelection = false;
            state.selectionAnchor = state.cursorIndex;
            return false;
        }

        text.erase(begin, end - begin);
        state.cursorIndex = begin;
        state.selectionAnchor = begin;
        state.hasSelection = false;
        state.verticalColumnActive = false;
        return true;
    }

    static void source_editor_insert_text_limited(
        std::string& text,
        SourceEditorState& state,
        std::string_view incoming,
        std::size_t limit,
        bool& changed,
        std::size_t& invalidUtf8Replacements)
    {
        if (source_editor_delete_selection(text, state))
            changed = true;
        state.verticalColumnActive = false;

        const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
        const std::size_t available = limit > text.size() ? limit - text.size() : 0u;
        if (available == 0u)
            return;

        std::string filtered{};
        filtered.reserve((std::min)(incoming.size(), available));
        std::size_t incomingIndex = 0u;
        while (incomingIndex < incoming.size()
            && filtered.size() < available)
        {
            const unsigned char lead =
                static_cast<unsigned char>(incoming[incomingIndex]);
            if (lead == static_cast<unsigned char>('\r'))
            {
                ++incomingIndex;
                continue;
            }
            if (lead == static_cast<unsigned char>('\n'))
            {
                filtered.push_back('\n');
                ++incomingIndex;
                continue;
            }
            if (lead == static_cast<unsigned char>('\t'))
            {
                filtered.push_back('\t');
                ++incomingIndex;
                continue;
            }
            if (lead < 32u)
            {
                ++incomingIndex;
                continue;
            }

            const DecodedUtf8Codepoint decoded =
                decode_utf8_codepoint(incoming, incomingIndex);
            const std::size_t byteCount =
                (std::max)(std::size_t{1u}, decoded.byte_count);
            if (lead >= 0x80u && decoded.codepoint == U'?'
                && byteCount == 1u)
            {
                constexpr std::string_view replacement{"\xEF\xBF\xBD", 3u};
                if (replacement.size() > available - filtered.size())
                    break;
                filtered.append(replacement);
                ++invalidUtf8Replacements;
                ++incomingIndex;
                continue;
            }
            if (byteCount > available - filtered.size())
                break;
            filtered.append(incoming.substr(incomingIndex, byteCount));
            incomingIndex += byteCount;
        }

        if (filtered.empty())
            return;

        text.insert(cursor, filtered);
        state.cursorIndex = cursor + filtered.size();
        state.selectionAnchor = state.cursorIndex;
        state.hasSelection = false;
        changed = true;
    }

    SourceEditorResult source_editor(std::string& text, const SourceEditorOptions& options) noexcept
    {
        SourceEditorResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float availableWidth = (std::max)(1.0f, content_available_width(pos.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(96.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const float height = (std::max)(120.0f, options.size.y > 0.0f ? options.size.y : 220.0f);
        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(&text))
            : std::string(options.id);
        auto& state = g_sourceEditorStates[scroll_panel_key(id)];
        const std::string scrollId = id + "-scroll";
        auto& scrollState = g_scrollAreaStates[scroll_panel_key(scrollId)];

        const float scrollbarReserve = 12.0f;
        const float editorWidth = (std::max)(64.0f, width - scrollbarReserve);
        const float contentWidth = (std::max)(1.0f, editorWidth - 2.0f * kBoxInnerPadding);
        const float lineAdvance = line_advance_amount(kFontScale);
        const float baseHeight = base_line_height(kFontScale);
        const std::size_t lineCount = line_count_for_text(text);
        const float textHeight = static_cast<float>(lineCount) * lineAdvance
            + 2.0f * kBoxInnerPadding
            + kContentPadding;
        const float editorContentHeight = (std::max)(height - 4.0f, textHeight);
        const bool hoveredEditor = point_in_rect(g_frame.mousePos, pos.x, pos.y, editorWidth, height)
            && point_in_active_clip(g_frame.mousePos);
        const void* textId = static_cast<const void*>(&text);
        const void* ctxKey = static_cast<const void*>(g_frame.ctx);
        const auto& palette = active_palette();
        const std::size_t limit = (options.max_chars == 0)
            ? std::numeric_limits<std::size_t>::max()
            : options.max_chars;

        const bool pointerPressed = g_frame.justPressed || g_frame.rightJustPressed;
        const void* currentActiveWidget = ctxKey ? g_contextActiveWidgets[ctxKey] : nullptr;
        clamp_source_editor_cursor(state, text.size());
        if (options.goto_generation != 0u
            && options.goto_generation != state.lastGotoGeneration)
        {
            const std::size_t targetLine = (std::max)(std::size_t{ 1u }, options.goto_line);
            state.cursorIndex = line_start_for_index(text, targetLine - 1u);
            state.selectionAnchor = state.cursorIndex;
            state.hasSelection = false;
            state.verticalColumnActive = false;
            state.lastGotoGeneration = options.goto_generation;
            scrollState.scrollY = static_cast<float>(targetLine - 1u) * lineAdvance;
        }
        if (pointerPressed)
        {
            if (hoveredEditor)
            {
                g_contextActiveWidgets[ctxKey] = textId;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
            }
            else if (currentActiveWidget == textId && !state.contextMenuOpen)
            {
                g_contextActiveWidgets[ctxKey] = nullptr;
                state.draggingSelection = false;
            }
        }

        const bool active = g_contextActiveWidgets[ctxKey] == textId;
        result.edit.active = active;

        if (active)
        {
            const auto replace_selection_if_needed = [&]() noexcept
            {
                if (source_editor_delete_selection(text, state))
                    result.edit.changed = true;
            };

            for (const auto& evt : g_frame.events)
            {
                switch (evt.type)
                {
                case EventType::TextInput:
                    if (!options.read_only)
                        source_editor_insert_text_limited(
                            text, state, evt.text, limit,
                            result.edit.changed,
                            result.invalid_utf8_replacements);
                    break;

                case EventType::KeyDown:
                    if (evt.ctrl_down && (evt.key == 'C' || evt.key == 'c'))
                    {
                        result.copied = clipboard_write_text(source_editor_selected_text(text, state));
                    }
                    else if (evt.ctrl_down && (evt.key == 'X' || evt.key == 'x'))
                    {
                        result.cut = clipboard_write_text(source_editor_selected_text(text, state));
                        if (!options.read_only && source_editor_has_selection(state, text.size()))
                        {
                            replace_selection_if_needed();
                        }
                    }
                    else if (evt.ctrl_down && (evt.key == 'V' || evt.key == 'v'))
                    {
                        if (!options.read_only)
                        {
                            source_editor_insert_text_limited(
                                text, state, clipboard_read_text(), limit,
                                result.edit.changed,
                                result.invalid_utf8_replacements);
                            result.pasted = true;
                        }
                    }
                    else if (evt.ctrl_down && (evt.key == 'A' || evt.key == 'a'))
                    {
                        state.selectionAnchor = 0u;
                        state.cursorIndex = text.size();
                        state.hasSelection = !text.empty();
                        state.verticalColumnActive = false;
                        result.selected_all = state.hasSelection;
                    }
                    else if (evt.key == 8 || evt.key == 127)
                    {
                        if (!options.read_only && source_editor_has_selection(state, text.size()))
                        {
                            replace_selection_if_needed();
                        }
                        else if (!options.read_only && evt.key == 8 && state.cursorIndex > 0u)
                        {
                            const std::size_t eraseIndex =
                                previous_utf8_codepoint_start(
                                    text, state.cursorIndex);
                            text.erase(
                                eraseIndex,
                                state.cursorIndex - eraseIndex);
                            state.cursorIndex = eraseIndex;
                            state.selectionAnchor = eraseIndex;
                            state.verticalColumnActive = false;
                            result.edit.changed = true;
                        }
                        else if (!options.read_only && evt.key == 127
                            && state.cursorIndex < text.size())
                        {
                            const std::size_t eraseEnd =
                                next_utf8_codepoint_start(
                                    text, state.cursorIndex);
                            text.erase(
                                state.cursorIndex,
                                eraseEnd - state.cursorIndex);
                            state.verticalColumnActive = false;
                            result.edit.changed = true;
                        }
                    }
                    else if (evt.key == 27)
                    {
                        g_contextActiveWidgets[ctxKey] = nullptr;
                        state.draggingSelection = false;
                        state.hasSelection = false;
                        state.selectionAnchor = state.cursorIndex;
                        result.edit.active = false;
                    }
                    else if (evt.key == 13)
                    {
                        if (!options.read_only)
                            source_editor_insert_text_limited(
                                text, state, "\n", limit,
                                result.edit.changed,
                                result.invalid_utf8_replacements);
                    }
                    else if (evt.key == 37) // VK_LEFT
                    {
                        const std::size_t next =
                            previous_utf8_codepoint_start(
                                text, state.cursorIndex);
                        source_editor_set_cursor(state, next, evt.shift_down, text.size());
                    }
                    else if (evt.key == 39) // VK_RIGHT
                    {
                        const std::size_t next =
                            next_utf8_codepoint_start(
                                text, state.cursorIndex);
                        source_editor_set_cursor(state, next, evt.shift_down, text.size());
                    }
                    else if (evt.key == 38) // VK_UP
                    {
                        source_editor_move_vertical(
                            state, text, -1, evt.shift_down);
                    }
                    else if (evt.key == 40) // VK_DOWN
                    {
                        source_editor_move_vertical(
                            state, text, 1, evt.shift_down);
                    }
                    else if (evt.key == 33 || evt.key == 34) // VK_PRIOR / VK_NEXT
                    {
                        const std::size_t visibleLines = (std::max)(
                            std::size_t{1u},
                            static_cast<std::size_t>((std::max)(
                                lineAdvance,
                                height - 2.0f * kBoxInnerPadding)
                                / (std::max)(1.0f, lineAdvance)));
                        const std::ptrdiff_t pageLines = static_cast<std::ptrdiff_t>(
                            visibleLines > 1u ? visibleLines - 1u : 1u);
                        source_editor_move_vertical(
                            state,
                            text,
                            evt.key == 33 ? -pageLines : pageLines,
                            evt.shift_down);
                    }
                    else if (evt.ctrl_down && evt.key == 36) // Ctrl+Home
                    {
                        source_editor_set_cursor(
                            state, 0u, evt.shift_down, text.size());
                    }
                    else if (evt.ctrl_down && evt.key == 35) // Ctrl+End
                    {
                        source_editor_set_cursor(
                            state, text.size(), evt.shift_down, text.size());
                    }
                    else if (evt.key == 36) // VK_HOME
                    {
                        source_editor_set_cursor(
                            state,
                            source_editor_line_start_for_cursor(
                                text, state.cursorIndex),
                            evt.shift_down,
                            text.size());
                    }
                    else if (evt.key == 35) // VK_END
                    {
                        const std::size_t currentLineStart =
                            source_editor_line_start_for_cursor(
                                text, state.cursorIndex);
                        source_editor_set_cursor(
                            state,
                            source_editor_line_end_for_start(
                                text, currentLineStart),
                            evt.shift_down,
                            text.size());
                    }
                    break;

                default:
                    break;
                }
            }
        }

        const bool revealCaret = active
            && std::any_of(
                g_frame.events.begin(),
                g_frame.events.end(),
                [](const InputEvent& event)
                {
                    if (event.type == EventType::TextInput)
                        return true;
                    if (event.type != EventType::KeyDown)
                        return false;
                    return event.key == 8 || event.key == 127
                        || event.key == 13 || event.key == 33
                        || event.key == 34 || event.key == 35
                        || event.key == 36 || event.key == 37
                        || event.key == 38 || event.key == 39
                        || event.key == 40
                        || (event.ctrl_down
                            && (event.key == 'A' || event.key == 'a'
                                || event.key == 'V' || event.key == 'v'
                                || event.key == 'X' || event.key == 'x'));
                });
        if (revealCaret)
        {
            const auto cursorEnd = text.begin()
                + static_cast<std::ptrdiff_t>(
                    (std::min)(state.cursorIndex, text.size()));
            const std::size_t caretLine = static_cast<std::size_t>(
                std::count(text.begin(), cursorEnd, '\n'));
            const float caretTop =
                static_cast<float>(caretLine) * lineAdvance;
            const float caretBottom = caretTop + baseHeight
                + 2.0f * kBoxInnerPadding;
            if (caretTop < scrollState.scrollY)
                scrollState.scrollY = caretTop;
            else if (caretBottom > scrollState.scrollY + height)
                scrollState.scrollY = caretBottom - height;

            std::size_t caretLineStart{};
            for (std::size_t index = 0u; index < (std::min)(state.cursorIndex, text.size()); ++index)
                if (text[index] == '\n')
                    caretLineStart = index + 1u;
            const float caretOffsetX = source_editor_x_for_index(
                text, caretLineStart, (std::min)(state.cursorIndex, text.size()), kFontScale);
            if (caretOffsetX < state.scrollX)
                state.scrollX = caretOffsetX;
            else if (caretOffsetX > state.scrollX + contentWidth - space_advance(kFontScale))
                state.scrollX = caretOffsetX - contentWidth + space_advance(kFontScale);
        }

        scrollState.scrollY = source_editor_clamped_scroll(
            scrollState.scrollY,
            editorContentHeight,
            height);

        float maximumLineWidth{};
        std::size_t measuredOffset{};
        for (std::size_t line = 0u; line < lineCount; ++line)
        {
            const std::size_t lineStart = measuredOffset;
            const std::string_view lineText = line_view_from_offset(text, measuredOffset);
            maximumLineWidth = (std::max)(maximumLineWidth,
                source_editor_x_for_index(text, lineStart, lineStart + lineText.size(), kFontScale));
        }
        const float maximumScrollX = (std::max)(0.0f,
            maximumLineWidth - contentWidth + kBoxInnerPadding);
        state.scrollX = source_editor_clamped_scroll(
            state.scrollX,
            maximumLineWidth + kBoxInnerPadding,
            contentWidth);
        const bool horizontalWheel = std::any_of(
            g_frame.events.begin(), g_frame.events.end(),
            [](const InputEvent& event)
            {
                return event.type == EventType::MouseWheel && event.shift_down;
            });
        if (hoveredEditor && horizontalWheel && g_frame.mouseWheelDelta != 0)
        {
            const float steps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            state.scrollX = (std::clamp)(state.scrollX
                - steps * space_advance(kFontScale) * 6.0f,
                0.0f,
                maximumScrollX);
            g_frame.mouseWheelDelta = 0;
        }

        draw_sprite(active ? palette.textFieldActive : palette.textField, pos.x, pos.y, editorWidth, height);

        const auto scroll = begin_scroll_area(ScrollAreaOptions{
            .id = scrollId,
            .size = { width, height },
            .content_height = editorContentHeight,
            .draw_background = false,
            .show_scrollbar = true
        });

        const float textX = g_frame.cursor.x + kBoxInnerPadding - state.scrollX;
        const float viewportTextY = pos.y + kBoxInnerPadding;
        const float firstTextY = g_frame.cursor.y + kBoxInnerPadding;
        const std::size_t firstVisibleLine = static_cast<std::size_t>((std::max)(0.0f, scroll.scroll_y) / (std::max)(1.0f, lineAdvance));
        const std::size_t visibleLineBudget = static_cast<std::size_t>(height / (std::max)(1.0f, lineAdvance)) + 4u;

        if (active && g_frame.justPressed && hoveredEditor)
        {
            const std::size_t cursor = source_editor_index_from_point(
                text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                viewportTextY,
                scroll.scroll_y,
                lineAdvance,
                kFontScale);
            source_editor_set_cursor(state, cursor, false, text.size());
            state.draggingSelection = true;
        }
        else if (active && state.draggingSelection && g_frame.mouseDown)
        {
            const std::size_t cursor = source_editor_index_from_point(
                text,
                g_frame.mousePos.x,
                g_frame.mousePos.y,
                textX,
                viewportTextY,
                scroll.scroll_y,
                lineAdvance,
                kFontScale);
            source_editor_set_cursor(state, cursor, true, text.size());
        }

        if (!g_frame.mouseDown)
            state.draggingSelection = false;

        std::size_t offset = line_start_for_index(text, firstVisibleLine);
        float lineY = firstTextY + static_cast<float>(firstVisibleLine) * lineAdvance;
        const auto [selectionBegin, selectionEnd] = source_editor_selection_range(state, text.size());
        for (std::size_t line = 0u; line < visibleLineBudget && firstVisibleLine + line < lineCount; ++line)
        {
            const std::size_t lineStart = offset;
            const std::string_view lineText = line_view_from_offset(text, offset);
            const std::size_t lineEnd = lineStart + lineText.size();

            if (selectionEnd > selectionBegin && selectionEnd >= lineStart && selectionBegin <= lineEnd)
            {
                const std::size_t highlightBegin = (std::max)(selectionBegin, lineStart);
                const std::size_t highlightEnd = (std::min)(selectionEnd, lineEnd);
                const float highlightX = textX + source_editor_x_for_index(text, lineStart, highlightBegin, kFontScale);
                float highlightWidth = source_editor_x_for_index(text, lineStart, highlightEnd, kFontScale)
                    - source_editor_x_for_index(text, lineStart, highlightBegin, kFontScale);
                if (selectionEnd > lineEnd && highlightEnd == lineEnd)
                    highlightWidth += space_advance(kFontScale) * 0.75f;
                if (highlightWidth > 0.0f)
                    draw_sprite(palette.buttonActive, highlightX, lineY - 1.0f, highlightWidth, baseHeight + 2.0f);
            }

            draw_text_line(lineText, textX, lineY, kFontScale);
            lineY += lineAdvance;
        }

        if (active && !source_editor_has_selection(state, text.size()))
        {
            const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
            std::size_t caretLineIndex = 0u;
            std::size_t caretLineStart = 0u;
            for (std::size_t i = 0u; i < cursor; ++i)
            {
                if (text[i] == '\n')
                {
                    ++caretLineIndex;
                    caretLineStart = i + 1u;
                }
            }

            const float caretX = textX
                + source_editor_x_for_index(text, caretLineStart, cursor, kFontScale);
            const float caretY = firstTextY + static_cast<float>(caretLineIndex) * lineAdvance;
            if (caretY + baseHeight >= pos.y && caretY <= pos.y + height)
                draw_caret(caretX, caretY, baseHeight);
        }

        g_frame.cursor = { pos.x, pos.y - scroll.scroll_y + editorContentHeight };
        end_scroll_area();

        result.horizontal_scroll = state.scrollX;
        result.vertical_scroll = scrollState.scrollY;
        const std::size_t resultCursor = (std::min)(state.cursorIndex, text.size());
        std::size_t resultLineStart{};
        for (std::size_t index = 0u; index < resultCursor; ++index)
            if (text[index] == '\n')
                resultLineStart = index + 1u;
        result.cursor_line = static_cast<std::size_t>(
            std::count(text.begin(), text.begin()
                + static_cast<std::ptrdiff_t>(resultCursor), '\n')) + 1u;
        result.cursor_column = 1u;
        for (std::size_t index = resultLineStart; index < resultCursor;)
        {
            index = next_utf8_codepoint_start(text, index);
            ++result.cursor_column;
        }

        bool openedThisFrame = false;
        if (options.show_context_menu && g_frame.rightJustPressed && hoveredEditor)
        {
            state.contextMenuOpen = true;
            state.contextMenuPos = g_frame.mousePos;
            openedThisFrame = true;
        }

        if (state.contextMenuOpen)
        {
            const float rowHeight = 28.0f;
            const float menuPadding = 4.0f;
            const float menuWidth = 172.0f;
            const float menuHeight = menuPadding * 2.0f + rowHeight * 4.0f + kContentPadding * 3.0f;
            Vec2 menuPos = state.contextMenuPos;
            menuPos.x = (std::min)(menuPos.x, (std::max)(0.0f, g_frame.origin.x + g_frame.windowSize.x - menuWidth - kContentPadding));
            menuPos.y = (std::min)(menuPos.y, (std::max)(0.0f, g_frame.origin.y + g_frame.windowSize.y - menuHeight - kContentPadding));

            const bool hoveredMenu = point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y, menuWidth, menuHeight);
            if (!openedThisFrame && (g_frame.justPressed || g_frame.rightJustPressed) && !hoveredMenu)
                state.contextMenuOpen = false;

            if (state.contextMenuOpen)
            {
                const Vec2 savedCursor = g_frame.cursor;
                const Vec2 savedOrigin = g_frame.origin;
                const Vec2 savedWindowSize = g_frame.windowSize;
                const Vec2 savedContentMin = g_frame.contentMin;
                const Vec2 savedContentMax = g_frame.contentMax;
                const bool savedInsideWindow = g_frame.insideWindow;
                const std::string savedWindowKey = g_frame.windowKey;
                const std::uint64_t savedWidgetSerial = g_frame.widgetSerial;

                begin_top_layer();
                const auto& palette = active_palette();
                draw_sprite(palette.windowBackground, menuPos.x, menuPos.y, menuWidth, menuHeight);
                draw_sprite(palette.titleBar, menuPos.x, menuPos.y, menuWidth, 2.0f);
                draw_sprite(palette.titleBar, menuPos.x, menuPos.y, 2.0f, menuHeight);
                draw_sprite(palette.panelBackground, menuPos.x + menuWidth - 2.0f, menuPos.y, 2.0f, menuHeight);
                draw_sprite(palette.panelBackground, menuPos.x, menuPos.y + menuHeight - 2.0f, menuWidth, 2.0f);

                g_frame.insideWindow = true;
                g_frame.windowKey = id + "-context-menu";
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                set_cursor(g_frame.contentMin);

                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    state.selectionAnchor = 0u;
                    state.cursorIndex = text.size();
                    state.hasSelection = !text.empty();
                    result.selected_all = state.hasSelection;
                    state.contextMenuOpen = false;
                }
                if (button("Copy", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    result.copied = clipboard_write_text(source_editor_selected_text(text, state));
                    state.contextMenuOpen = false;
                }
                if (button("Cut", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    result.cut = clipboard_write_text(source_editor_selected_text(text, state));
                    if (!options.read_only && result.cut && source_editor_has_selection(state, text.size()))
                    {
                        (void)source_editor_delete_selection(text, state);
                        result.edit.changed = true;
                    }
                    state.contextMenuOpen = false;
                }
                if (button("Paste", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    if (!options.read_only)
                    {
                        source_editor_insert_text_limited(
                            text,
                            state,
                            clipboard_read_text(),
                            options.max_chars,
                            result.edit.changed,
                            result.invalid_utf8_replacements);
                        result.pasted = true;
                    }
                    state.contextMenuOpen = false;
                }

                g_frame.cursor = savedCursor;
                g_frame.origin = savedOrigin;
                g_frame.windowSize = savedWindowSize;
                g_frame.contentMin = savedContentMin;
                g_frame.contentMax = savedContentMax;
                g_frame.insideWindow = savedInsideWindow;
                g_frame.windowKey = savedWindowKey;
                g_frame.widgetSerial = savedWidgetSerial;
                end_top_layer();
            }
        }

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
        static thread_local std::vector<float> itemWidths;
        itemWidths.clear();
        itemWidths.reserve(items.size());
        for (const auto& item : items)
            itemWidths.push_back(item.width);

        const gui_lib::SegmentedControlLayoutOptions layoutOptions{
            .position = to_lib(rowStart),
            .item_widths = std::span<const float>{ itemWidths.data(), itemWidths.size() },
            .height = height,
            .gap = gap
        };
        const auto layout = gui_lib::make_segmented_control_layout(layoutOptions);

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            const auto itemLayout = gui_lib::segmented_control_item_layout(
                layoutOptions,
                static_cast<std::uint32_t>(i));
            set_cursor(from_lib(itemLayout.position));
            if (button_with_state(item.label, from_lib(itemLayout.size), item.active))
                clicked = i;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, layout.height + kContentPadding });
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
            if (hovered && left_press_available())
            {
                pressedKey = pressKey;
                consume_left_press();
            }

            const bool pressed = (g_frame.mouseDown || g_frame.justReleased) && pressedKey == pressKey;
            if (left_release_available() && hovered && pressedKey == pressKey)
            {
                clicked = i;
                consume_left_release();
            }
            if (g_frame.justReleased && pressedKey == pressKey)
                pressedKey = 0;

            const SpriteHandle background =
                tab.active || pressed ? palette.panelBackground
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

    TabBarResult tab_bar_buttons(
        std::span<const TabButtonSpec> tabs,
        float height,
        float gap,
        TabBarPresentation presentation) noexcept
    {
        TabBarResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx || tabs.empty())
            return result;

        const Vec2 rowStart = g_frame.cursor;
        const auto& palette = active_palette();
        static thread_local std::vector<float> widths;
        widths.clear();
        widths.reserve(tabs.size());
        const bool workbenchPresentation =
            presentation == TabBarPresentation::Workbench;
        for (const auto& tab : tabs)
        {
            const float measuredLabel = measure_text_width(
                tab.label, kFontScale);
            const float requestedWidth = workbenchPresentation
                ? gui_lib::resolve_responsive_tab_width({
                    .requested_width = tab.width,
                    .measured_label_width = measuredLabel,
                    .minimum_hit_width = 72.0f,
                    .horizontal_padding = 24.0f,
                    .close_extent = tab.closable ? 18.0f : 0.0f,
                    .dirty_extent = tab.dirty ? 8.0f : 0.0f})
                : tab.width > 0.0f
                    ? tab.width
                    : gui_lib::preferred_tool_tab_width(
                        measuredLabel,
                        tab.closable,
                        tab.dirty);
            widths.push_back((std::max)(1.0f, requestedWidth));
        }

        const float rowHeight = (std::max)(
            height,
            base_line_height(kFontScale) + 2.0f * kBoxInnerPadding);
        const gui_lib::TabButtonLayoutOptions layoutOptions{
            .strip = {
                .position = to_lib(rowStart),
                .item_widths = std::span<const float>{ widths.data(), widths.size() },
                .height = rowHeight,
                .gap = gap },
            .indicator_height = workbenchPresentation ? 2.0f : 3.0f,
            .close_extent = 18.0f,
            .label_padding = workbenchPresentation ? 12.0f : kContentPadding
        };
        const gui_lib::SegmentedControlLayout strip =
            gui_lib::make_segmented_control_layout(layoutOptions.strip);
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];

        if (workbenchPresentation && strip.valid)
        {
            draw_sprite(
                palette.panelBackground,
                strip.bounds.position.x,
                strip.bounds.position.y,
                strip.bounds.size.x,
                strip.bounds.size.y);
        }

        for (std::size_t index = 0; index < tabs.size(); ++index)
        {
            const TabButtonSpec& tab = tabs[index];
            const gui_lib::TabButtonLayout layout =
                gui_lib::make_tab_button_layout(
                    layoutOptions,
                    static_cast<std::uint32_t>(index),
                    tab.active,
                    tab.closable);
            if (!layout.valid)
                continue;

            const Vec2 buttonPosition = from_lib(layout.button.position);
            const Vec2 buttonSize = from_lib(layout.button.size);
            const Vec2 closePosition = from_lib(layout.close_button.position);
            const Vec2 closeSize = from_lib(layout.close_button.size);
            const bool closeHovered = tab.enabled
                && tab.closable
                && point_in_rect(
                    g_frame.mousePos,
                    closePosition.x,
                    closePosition.y,
                    closeSize.x,
                    closeSize.y)
                && point_in_active_clip(g_frame.mousePos);
            const bool buttonHovered = tab.enabled
                && point_in_rect(
                    g_frame.mousePos,
                    buttonPosition.x,
                    buttonPosition.y,
                    buttonSize.x,
                    buttonSize.y)
                && point_in_active_clip(g_frame.mousePos)
                && !closeHovered;

            const std::string_view identity = !tab.id.empty() ? tab.id : tab.label;
            const std::size_t buttonKey =
                widget_press_key(identity, buttonPosition, buttonSize);
            std::string closeIdentity{ identity };
            closeIdentity.append(".close");
            const std::size_t closeKey =
                widget_press_key(closeIdentity, closePosition, closeSize);

            if (closeHovered && left_press_available())
            {
                pressedKey = closeKey;
                consume_left_press();
            }
            else if (buttonHovered && left_press_available())
            {
                pressedKey = buttonKey;
                result.pressed_index = index;
                consume_left_press();
            }

            const bool buttonPressed =
                (g_frame.mouseDown || g_frame.justReleased)
                && pressedKey == buttonKey;
            const bool closePressed =
                (g_frame.mouseDown || g_frame.justReleased)
                && pressedKey == closeKey;
            const bool buttonClicked = left_release_available()
                && buttonHovered
                && pressedKey == buttonKey;
            const bool closeClicked = left_release_available()
                && closeHovered
                && pressedKey == closeKey;
            if (g_frame.justReleased
                && (pressedKey == buttonKey || pressedKey == closeKey))
            {
                if (buttonClicked || closeClicked)
                    consume_left_release();
                pressedKey = 0;
            }

            const SpriteHandle background = !tab.enabled
                ? palette.panelBackground
                : tab.active || buttonPressed ? palette.buttonActive
                : buttonHovered ? palette.buttonHover
                : workbenchPresentation ? palette.panelBackground
                : palette.buttonNormal;
            if (!workbenchPresentation || tab.active || buttonPressed || buttonHovered)
            {
                draw_sprite(
                    background,
                    buttonPosition.x,
                    buttonPosition.y,
                    buttonSize.x,
                    buttonSize.y);
            }
            const float dirtyReservation = tab.dirty ? 8.0f : 0.0f;
            const float availableTextWidth = (std::max)(
                1.0f,
                layout.label.size.x - dirtyReservation);
            const std::string fitted = fit_text_to_width(
                tab.label,
                availableTextWidth,
                kFontScale);
            const std::string_view display =
                fitted.empty() ? tab.label : std::string_view{ fitted };
            const float textY = layout.label.position.y
                + std::floor((std::max)(
                    0.0f,
                    (layout.label.size.y - base_line_height(kFontScale)) * 0.5f));
            const float textX = workbenchPresentation
                ? layout.label.position.x
                    + (std::max)(
                        0.0f,
                        (layout.label.size.x
                            - measure_text_width(display, kFontScale)) * 0.5f)
                : layout.label.position.x;
            draw_text_line(
                display,
                textX,
                textY,
                kFontScale);

            if (workbenchPresentation && tab.active)
            {
                draw_sprite(
                    palette.textFieldActive,
                    layout.indicator.position.x,
                    layout.indicator.position.y,
                    layout.indicator.size.x,
                    layout.indicator.size.y);
            }

            if (tab.dirty)
            {
                draw_sprite(
                    palette.textFieldActive,
                    layout.label.position.x + availableTextWidth + 2.0f,
                    layout.label.position.y + layout.label.size.y * 0.5f - 2.0f,
                    4.0f,
                    4.0f);
            }

            if (tab.closable)
            {
                const SpriteHandle closeBackground = closePressed
                    ? palette.buttonActive
                    : closeHovered ? palette.textFieldActive
                    : background;
                draw_sprite(
                    closeBackground,
                    closePosition.x,
                    closePosition.y,
                    closeSize.x,
                    closeSize.y);
                const float closeTextWidth = measure_text_width("X", kFontScale);
                draw_text_line(
                    "X",
                    closePosition.x + (std::max)(0.0f, (closeSize.x - closeTextWidth) * 0.5f),
                    closePosition.y + (std::max)(
                        0.0f,
                        (closeSize.y - base_line_height(kFontScale)) * 0.5f),
                    kFontScale);
            }

            if (buttonClicked)
                result.selected_index = index;
            if (closeClicked)
                result.closed_index = index;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, strip.height + kContentPadding });
        return result;
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
            if (button_with_state(
                    item.label,
                    { item.width, height },
                    false,
                    item.enabled))
            {
                clicked = i;
            }
            x += (std::max)(1.0f, item.width) + gap;
        }

        set_cursor(rowStart);
        advance_cursor({ 0.0f, (std::max)(1.0f, height) + kContentPadding });
        return clicked;
    }

    TabBarResult responsive_tab_bar_buttons(
        const ResponsiveTabBarOptions& options) noexcept
    {
        TabBarResult result{};
        if (options.tabs.empty())
            return result;

        static thread_local std::vector<float> widths{};
        widths.clear();
        widths.reserve(options.tabs.size());
        const bool workbenchPresentation =
            options.presentation == TabBarPresentation::Workbench;
        std::size_t activeIndex = (std::numeric_limits<std::size_t>::max)();
        float longestLabelWidth{};
        for (std::size_t index = 0; index < options.tabs.size(); ++index)
        {
            const TabButtonSpec& tab = options.tabs[index];
            if (tab.active)
                activeIndex = index;
            const float measuredLabel = measure_text_width(
                tab.label, kFontScale);
            longestLabelWidth = (std::max)(longestLabelWidth, measuredLabel);
            const float requestedWidth = workbenchPresentation
                ? gui_lib::resolve_responsive_tab_width({
                    .requested_width = tab.width,
                    .measured_label_width = measuredLabel,
                    .minimum_hit_width = 72.0f,
                    .horizontal_padding = 24.0f,
                    .close_extent = tab.closable ? 18.0f : 0.0f,
                    .dirty_extent = tab.dirty ? 8.0f : 0.0f})
                : tab.width > 0.0f
                    ? tab.width
                    : gui_lib::preferred_tool_tab_width(
                        measuredLabel,
                        tab.closable,
                        tab.dirty);
            widths.push_back((std::max)(1.0f, requestedWidth));
        }

        const auto apply_keyboard_navigation = [&]()
        {
            if (!options.keyboard_navigation || result.selected_index
                || keyboard_input_captured())
            {
                return;
            }
            std::optional<gui_lib::ResponsiveTabNavigationIntent> intent{};
            for (const InputEvent& event : g_frame.events)
            {
                if (event.type == EventType::KeyDown
                    && event.ctrl_down && event.key == 9)
                {
                    intent = event.shift_down
                        ? gui_lib::ResponsiveTabNavigationIntent::previous
                        : gui_lib::ResponsiveTabNavigationIntent::next;
                    break;
                }
            }
            if (!intent) return;

            static thread_local std::vector<std::uint8_t> enabled{};
            enabled.clear();
            enabled.reserve(options.tabs.size());
            for (const TabButtonSpec& tab : options.tabs)
                enabled.push_back(tab.enabled ? 1u : 0u);
            const auto selected = gui_lib::navigate_responsive_tab_strip({
                .enabled = enabled,
                .active_index = activeIndex,
                .intent = *intent,
                .wrap = true
            });
            if (selected && *selected != activeIndex)
                result.selected_index = *selected;
        };

        const gui_lib::ResponsiveTabStripLayout layout =
            gui_lib::make_responsive_tab_strip_layout({
                .item_widths = widths,
                .active_index = activeIndex,
                .available_width = options.available_width,
                .gap = options.gap,
                .overflow_width = (std::max)({
                    options.overflow_width,
                    measure_text_width(options.overflow_label, kFontScale)
                        + 52.0f,
                    longestLabelWidth + 32.0f})
            });
        if (!layout.valid || !layout.overflowed)
        {
            result = tab_bar_buttons(
                options.tabs,
                options.height,
                options.gap,
                options.presentation);
            apply_keyboard_navigation();
            return result;
        }

        const Vec2 rowStart = cursor_position();
        static thread_local std::vector<TabButtonSpec> visibleTabs{};
        visibleTabs.clear();
        visibleTabs.reserve(layout.visible_indices.size());
        for (const std::uint32_t index : layout.visible_indices)
            visibleTabs.push_back(options.tabs[index]);

        if (!visibleTabs.empty())
        {
            const TabBarResult visibleResult = tab_bar_buttons(
                visibleTabs,
                options.height,
                options.gap,
                options.presentation);
            if (visibleResult.pressed_index)
                result.pressed_index = layout.visible_indices[*visibleResult.pressed_index];
            if (visibleResult.selected_index)
                result.selected_index = layout.visible_indices[*visibleResult.selected_index];
            if (visibleResult.closed_index)
                result.closed_index = layout.visible_indices[*visibleResult.closed_index];
        }

        static thread_local std::vector<std::string_view> overflowLabels{};
        static thread_local std::vector<std::uint32_t> overflowSelectableIndices{};
        overflowLabels.clear();
        overflowSelectableIndices.clear();
        overflowLabels.reserve(layout.overflow_indices.size());
        overflowSelectableIndices.reserve(layout.overflow_indices.size());
        std::string_view selectedOverflow{};
        for (const std::uint32_t index : layout.overflow_indices)
        {
            if (!options.tabs[index].enabled)
                continue;
            overflowLabels.push_back(options.tabs[index].label);
            overflowSelectableIndices.push_back(index);
            if (options.tabs[index].active)
                selectedOverflow = options.tabs[index].label;
        }
        const std::string overflowLabel = std::string(options.overflow_label)
            + " (" + std::to_string(layout.overflow_indices.size()) + ")";
        set_cursor({
            rowStart.x + layout.visible_width
                + (visibleTabs.empty() ? 0.0f : options.gap),
            rowStart.y
        });
        if (overflowSelectableIndices.empty())
        {
            (void)button_with_state(
                overflowLabel,
                { layout.overflow_width, options.height },
                false,
                false);
        }
        else
        {
            const SelectBoxResult overflowResult = select_box(SelectBoxOptions{
                .id = options.overflow_id,
                .placeholder = overflowLabel,
                .selected = selectedOverflow,
                .options = overflowLabels,
                .size = { layout.overflow_width, options.height },
                .row_height = options.height,
                .max_visible_options = 10u
            });
            if (overflowResult.changed && overflowResult.selected_index)
            {
                result.selected_index =
                    overflowSelectableIndices[*overflowResult.selected_index];
            }
        }
        apply_keyboard_navigation();
        set_cursor({
            rowStart.x,
            rowStart.y + (std::max)(22.0f, options.height) + kContentPadding
        });
        return result;
    }

    ChromeBarResult chrome_bar(const ChromeBarOptions& options) noexcept
    {
        ChromeBarResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx
            || options.size.x <= 0.0f || options.size.y <= 0.0f)
        {
            return result;
        }

        const auto item_width = [&](const ChromeItemSpec& item, bool compact)
        {
            const float textWidth = measure_text_width(item.label, kFontScale);
            float padding = 20.0f;
            switch (item.role)
            {
            case ChromeItemRole::Label:
                padding = 8.0f;
                break;
            case ChromeItemRole::Menu:
                padding = 28.0f;
                break;
            case ChromeItemRole::Command:
                padding = 32.0f;
                break;
            case ChromeItemRole::Status:
                padding = 20.0f;
                break;
            }
            const float preferred = item.preferred_width > 0.0f
                ? item.preferred_width
                : textWidth + padding;
            const float minimum = item.compact_width > 0.0f
                ? item.compact_width
                : (std::max)(48.0f, textWidth * 0.72f + padding);
            return compact
                ? (std::min)(preferred, minimum)
                : preferred;
        };

        static thread_local std::vector<gui_lib::ChromeBarItemOptions> leftCore{};
        static thread_local std::vector<gui_lib::ChromeBarItemOptions> centerCore{};
        static thread_local std::vector<gui_lib::ChromeBarItemOptions> rightCore{};
        const auto prepare_zone = [&](std::span<const ChromeItemSpec> source,
            std::vector<gui_lib::ChromeBarItemOptions>& target)
        {
            target.clear();
            target.reserve(source.size());
            for (const ChromeItemSpec& item : source)
            {
                target.push_back(gui_lib::ChromeBarItemOptions{
                    .preferred_width = item_width(item, false),
                    .compact_width = item_width(item, true),
                    .priority = item.priority,
                    .pinned = item.pinned,
                    .overflowable = item.overflowable
                });
            }
        };
        prepare_zone(options.left_items, leftCore);
        prepare_zone(options.center_items, centerCore);
        prepare_zone(options.right_items, rightCore);

        const gui_lib::ChromeBarLayout layout =
            gui_lib::make_chrome_bar_layout({
                .bounds = {
                    { options.position.x, options.position.y },
                    { options.size.x, options.size.y }
                },
                .left_items = leftCore,
                .center_items = centerCore,
                .right_items = rightCore,
                .item_gap = options.item_gap,
                .zone_gap = options.zone_gap,
                .overflow_width = options.overflow_width,
                .horizontal_padding = options.horizontal_padding
            });
        if (!layout.valid)
            return result;

        result.left_bounds.resize(layout.left.item_bounds.size());
        result.center_bounds.resize(layout.center.item_bounds.size());
        result.right_bounds.resize(layout.right.item_bounds.size());
        result.left_overflow = from_lib(layout.left.overflow_button);
        result.center_overflow = from_lib(layout.center.overflow_button);
        result.right_overflow = from_lib(layout.right.overflow_button);
        result.compact = layout.density != gui_lib::ChromeDensity::full;
        result.overflowed = layout.density == gui_lib::ChromeDensity::minimal;
        result.valid = true;

        const Vec2 savedCursor = cursor_position();
        const auto render_zone = [&](const gui_lib::ChromeBarZoneLayout& zone,
            std::span<const ChromeItemSpec> items,
            std::vector<WidgetBounds>& bounds,
            std::optional<std::size_t>& hovered,
            std::optional<std::size_t>& selected,
            std::string_view suffix)
        {
            for (std::size_t index = 0; index < zone.item_bounds.size(); ++index)
                bounds[index] = from_lib(zone.item_bounds[index]);

            for (const std::uint32_t index : zone.visible_indices)
            {
                const ChromeItemSpec& item = items[index];
                const WidgetBounds itemBounds = bounds[index];
                set_cursor(itemBounds.position);
                const bool isHovered = item.enabled
                    && point_in_rect(
                        g_frame.mousePos,
                        itemBounds.position.x,
                        itemBounds.position.y,
                        itemBounds.size.x,
                        itemBounds.size.y)
                    && point_in_active_clip(g_frame.mousePos);
                if (isHovered)
                    hovered = index;

                if (item.role == ChromeItemRole::Menu
                    || item.role == ChromeItemRole::Command)
                {
                    if (button_with_state(
                            item.label,
                            itemBounds.size,
                            item.selected,
                            item.enabled))
                    {
                        selected = index;
                    }
                    continue;
                }

                if (item.role == ChromeItemRole::Status)
                {
                    const auto& palette = active_palette();
                    draw_sprite(
                        palette.titleBar,
                        itemBounds.position.x,
                        itemBounds.position.y,
                        itemBounds.size.x,
                        itemBounds.size.y);
                }
                const float inset = item.role == ChromeItemRole::Label
                    ? 2.0f
                    : kContentPadding;
                const std::string fitted = fit_text_to_width(
                    item.label,
                    (std::max)(1.0f, itemBounds.size.x - inset * 2.0f),
                    kFontScale);
                const std::string_view display = fitted.empty()
                    ? item.label
                    : std::string_view{ fitted };
                const float textY = itemBounds.position.y
                    + (std::max)(
                        0.0f,
                        (itemBounds.size.y - base_line_height(kFontScale))
                            * 0.5f)
                    + 1.0f;
                draw_text_line(
                    display,
                    itemBounds.position.x + inset,
                    textY,
                    kFontScale);
            }

            if (zone.overflow_button.size.x <= 0.0f
                || zone.overflow_indices.empty())
            {
                return;
            }

            static thread_local std::vector<std::string_view> labels{};
            static thread_local std::vector<std::uint32_t> mappings{};
            labels.clear();
            mappings.clear();
            labels.reserve(zone.overflow_indices.size());
            mappings.reserve(zone.overflow_indices.size());
            for (const std::uint32_t index : zone.overflow_indices)
            {
                if (!items[index].enabled)
                    continue;
                labels.push_back(items[index].label);
                mappings.push_back(index);
            }

            const WidgetBounds overflowBounds = from_lib(zone.overflow_button);
            const std::string overflowLabel =
                std::string("More (")
                + std::to_string(zone.overflow_indices.size()) + ")";
            set_cursor(overflowBounds.position);
            if (mappings.empty())
            {
                (void)button_with_state(
                    overflowLabel,
                    overflowBounds.size,
                    false,
                    false);
                return;
            }

            const std::string controlId =
                std::string(options.id) + "." + std::string(suffix);
            const SelectBoxResult overflowResult = select_box(SelectBoxOptions{
                .id = controlId,
                .placeholder = overflowLabel,
                .selected = {},
                .options = labels,
                .size = overflowBounds.size,
                .row_height = options.size.y,
                .max_visible_options = 10u
            });
            if (overflowResult.changed && overflowResult.selected_index
                && *overflowResult.selected_index < mappings.size())
            {
                selected = mappings[*overflowResult.selected_index];
            }
        };

        render_zone(
            layout.left,
            options.left_items,
            result.left_bounds,
            result.hovered_left,
            result.selected_left,
            "left-overflow");
        render_zone(
            layout.center,
            options.center_items,
            result.center_bounds,
            result.hovered_center,
            result.selected_center,
            "center-overflow");
        render_zone(
            layout.right,
            options.right_items,
            result.right_bounds,
            result.hovered_right,
            result.selected_right,
            "right-overflow");
        set_cursor(savedCursor);
        return result;
    }


    SelectBoxResult select_box(const SelectBoxOptions& options) noexcept
    {
        SelectBoxResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 start = g_frame.cursor;
        const float availableWidth = (std::max)(1.0f, content_available_width(start.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(96.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
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

        bool toggledThisFrame = false;
        if (button(selectedLabel, { width, closedHeight }))
        {
            const bool nextOpen = !state.open;
            for (auto& [otherKey, otherState] : g_selectBoxStates)
            {
                (void)otherKey;
                otherState.open = false;
                otherState.alignSelectedOnOpen = false;
            }
            state.open = nextOpen;
            state.alignSelectedOnOpen = nextOpen;
            toggledThisFrame = true;
        }
        const Vec2 afterClosedCursor = g_frame.cursor;

        result.opened = state.open;
        if (!state.open || options.options.empty())
        {
            state.alignSelectedOnOpen = false;
            return result;
        }

        const std::size_t visibleCount = (std::max)(
            std::size_t{ 1 },
            (std::min)(options.options.size(), options.max_visible_options == 0
                ? options.options.size()
                : options.max_visible_options));
        const float listHeight = (std::max)(rowHeight, static_cast<float>(visibleCount) * rowHeight + 2.0f);
        const float optionPitch = rowHeight + 2.0f;
        const float contentHeight = static_cast<float>(options.options.size()) * rowHeight
            + static_cast<float>(options.options.size() - 1u) * 2.0f;
        const std::string listId = key + "-list";
        const Vec2 listPos = afterClosedCursor;

        if (state.alignSelectedOnOpen)
        {
            auto& scrollState = g_scrollAreaStates[scroll_panel_key(listId)];
            const auto selectedIt = std::find(options.options.begin(), options.options.end(), options.selected);
            if (selectedIt != options.options.end())
            {
                const auto selectedIndex = static_cast<std::size_t>(std::distance(options.options.begin(), selectedIt));
                const float selectedY = static_cast<float>(selectedIndex) * optionPitch;
                const float maxScroll = (std::max)(0.0f, contentHeight - listHeight);
                scrollState.scrollY = (std::clamp)(selectedY - rowHeight, 0.0f, maxScroll);
            }
            state.alignSelectedOnOpen = false;
        }

        if (!toggledThisFrame && (left_press_available() || right_press_available()))
        {
            const bool pressedClosed = point_in_rect(g_frame.mousePos, start.x, start.y, width, closedHeight);
            const bool pressedList = point_in_rect(g_frame.mousePos, listPos.x, listPos.y, width, listHeight);
            if (!pressedClosed && !pressedList)
            {
                state.open = false;
                result.opened = false;
                if (left_press_available())
                    consume_left_press();
                if (right_press_available())
                    consume_right_press();
                return result;
            }
        }

        auto& scrollState = g_scrollAreaStates[scroll_panel_key(listId)];
        scrollState.contentHeight = (std::max)(listHeight, contentHeight);
        const float maxScroll = (std::max)(0.0f, scrollState.contentHeight - listHeight);
        if (!std::isfinite(scrollState.scrollY))
            scrollState.scrollY = 0.0f;
        scrollState.scrollY = (std::clamp)(scrollState.scrollY, 0.0f, maxScroll);

        const bool listHovered = point_in_rect(g_frame.mousePos, listPos.x, listPos.y, width, listHeight)
            && point_in_modal_input_capture(g_frame.mousePos);
        if (listHovered && g_frame.mouseWheelDelta != 0)
        {
            const float wheelSteps = static_cast<float>(g_frame.mouseWheelDelta) / 120.0f;
            const float step = line_advance_amount(kFontScale) * 3.0f;
            scrollState.scrollY = (std::clamp)(scrollState.scrollY - wheelSteps * step, 0.0f, maxScroll);
            g_frame.mouseWheelDelta = 0;
        }

        const bool showScrollbar = options.options.size() > visibleCount;
        const float scrollbarWidth = showScrollbar ? 10.0f : 0.0f;
        const float itemWidth = (std::max)(64.0f, width - scrollbarWidth - 2.0f);
        for (std::size_t i = 0; i < options.options.size(); ++i)
        {
            const float optionY = listPos.y - scrollState.scrollY + static_cast<float>(i) * optionPitch;
            if (optionY + rowHeight < listPos.y || optionY > listPos.y + listHeight)
                continue;

            const Vec2 optionPos{ listPos.x, optionY };
            const bool hovered = point_in_rect(g_frame.mousePos, optionPos.x, optionPos.y, itemWidth, rowHeight)
                && point_in_rect(g_frame.mousePos, listPos.x, listPos.y, width, listHeight)
                && point_in_modal_input_capture(g_frame.mousePos);
            const std::size_t pressKey = widget_press_key(options.options[i], optionPos, { itemWidth, rowHeight });
            auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
            if (hovered && left_press_available())
            {
                pressedKey = pressKey;
                consume_left_press();
            }

            const bool clicked = left_release_available() && hovered && pressedKey == pressKey;
            if (g_frame.justReleased && pressedKey == pressKey)
            {
                if (hovered)
                    consume_left_release();
                pressedKey = 0;
            }

            g_frame.lastButtonBounds = WidgetBounds{ .position = optionPos, .size = { itemWidth, rowHeight } };

            if (clicked)
            {
                result.changed = true;
                result.selected_index = i;
                state.open = false;
            }
        }
        if (point_in_rect(g_frame.mousePos, listPos.x, listPos.y, width, listHeight))
        {
            if (left_press_available())
                consume_left_press();
            if (left_release_available())
                consume_left_release();
            if (right_press_available())
                consume_right_press();
            if (right_release_available())
                consume_right_release();
        }

        if (state.open)
        {
            PendingSelectPopup popup{};
            popup.scrollKey = scroll_panel_key(listId);
            popup.position = listPos;
            popup.width = width;
            popup.height = listHeight;
            popup.rowHeight = rowHeight;
            popup.optionPitch = optionPitch;
            popup.contentHeight = contentHeight;
            popup.showScrollbar = showScrollbar;
            popup.selected = std::string(options.selected);
            popup.options.reserve(options.options.size());
            for (std::string_view option : options.options)
                popup.options.emplace_back(option);
            g_frame.pendingSelectPopups.push_back(std::move(popup));
        }

        set_cursor(afterClosedCursor);
        return result;
    }

    static void draw_progress_bar_layout(
        const gui_lib::ProgressBarLayout& layout,
        std::string_view label,
        std::string_view status,
        bool show_percent,
        bool activity,
        float activity_phase) noexcept
    {
        if (!g_frame.ctx)
            return;

        const auto& palette = active_palette();
        draw_sprite(palette.panelBackground,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            layout.track.size.y);
        draw_sprite(palette.consoleBackground,
            layout.inner.position.x,
            layout.inner.position.y,
            layout.inner.size.x,
            layout.inner.size.y);

        if (layout.fill.size.x > 0.0f && layout.fill.size.y > 0.0f)
        {
            draw_sprite(palette.textFieldActive,
                layout.fill.position.x,
                layout.fill.position.y,
                layout.fill.size.x,
                layout.fill.size.y);
        }

        if (activity && layout.inner.size.x > 8.0f && layout.inner.size.y > 4.0f)
        {
            const float normalizedPhase = activity_phase - std::floor(activity_phase);
            const float stripeWidth = std::clamp(layout.inner.size.x * 0.18f, 24.0f, 96.0f);
            const float travel = layout.inner.size.x + stripeWidth;
            const float stripeX = layout.inner.position.x + normalizedPhase * travel - stripeWidth;
            ContentClipScope progressClip(
                { layout.inner.position.x, layout.inner.position.y },
                { layout.inner.position.x + layout.inner.size.x, layout.inner.position.y + layout.inner.size.y });
            draw_sprite(palette.buttonHover,
                stripeX,
                layout.inner.position.y,
                stripeWidth,
                layout.inner.size.y);
        }

        draw_sprite(palette.buttonHover,
            layout.track.position.x,
            layout.track.position.y,
            layout.track.size.x,
            1.0f);
        draw_sprite(palette.buttonHover,
            layout.track.position.x,
            layout.track.position.y + layout.track.size.y - 1.0f,
            layout.track.size.x,
            1.0f);
        draw_sprite(palette.buttonHover,
            layout.track.position.x,
            layout.track.position.y,
            1.0f,
            layout.track.size.y);
        draw_sprite(palette.buttonHover,
            layout.track.position.x + layout.track.size.x - 1.0f,
            layout.track.position.y,
            1.0f,
            layout.track.size.y);

        std::string labelText;
        if (!label.empty())
            labelText = std::string(label);
        if (!status.empty())
        {
            if (!labelText.empty())
                labelText += " - ";
            labelText += status;
        }
        if (show_percent)
        {
            if (!labelText.empty())
                labelText += " ";
            labelText += std::to_string(static_cast<int>(std::round(layout.fraction * 100.0f)));
            labelText += "%";
        }

        if (!labelText.empty())
        {
            const std::string fitted = fit_text_to_width(
                labelText,
                (std::max)(1.0f, layout.track.size.x - 2.0f * kContentPadding),
                kFontScale);
            const std::string_view displayLabel = fitted.empty()
                ? std::string_view{ labelText }
                : std::string_view{ fitted };
            const float textY = layout.track.position.y
                + std::floor((std::max)(0.0f, (layout.track.size.y - base_line_height(kFontScale)) * 0.5f))
                + 1.0f;
            draw_text_line(displayLabel, layout.track.position.x + kContentPadding, textY, kFontScale);
        }
    }

    void progress_bar(const ProgressBarOptions& options) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx)
            return;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float clipLeft = has_content_clip() ? g_frame.contentMin.x : g_frame.origin.x;
        const float clipRight = content_right();
        const float drawX = (std::min)((std::max)(pos.x, clipLeft), clipRight);
        const float availableWidth = (std::max)(1.0f, clipRight - drawX);
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(1.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const Vec2 drawPos{ drawX, pos.y };
        const float height = (std::max)(14.0f, options.size.y > 0.0f ? options.size.y : 18.0f);
        const auto layout = gui_lib::make_progress_bar_layout(gui_lib::ProgressBarLayoutOptions{
            .track = gui_lib::Rect{ to_lib(drawPos), { width, height } },
            .value = std::clamp(options.value, 0.0f, 1.0f),
            .minimum = 0.0f,
            .maximum = 1.0f,
            .padding = 2.0f,
            .direction = gui_lib::ProgressBarDirection::left_to_right
        });

        ContentClipScope localClip(
            { drawPos.x, drawPos.y },
            { drawPos.x + width, drawPos.y + height });
        draw_progress_bar_layout(
            layout,
            options.label,
            options.status,
            options.show_percent,
            options.activity,
            options.activity_phase);

        advance_cursor({ 0.0f, height + kContentPadding });
    }

    LoadingScreenResult loading_screen(const LoadingScreenOptions& options) noexcept
    {
        LoadingScreenResult result{};
        if (!g_frame.ctx)
            return result;

        ensure_resources();

        const Vec2 viewportPos = options.viewport_position;
        const Vec2 viewportSize = options.viewport_size.x > 0.0f && options.viewport_size.y > 0.0f
            ? options.viewport_size
            : (g_frame.insideWindow ? g_frame.windowSize : Vec2{ 1.0f, 1.0f });
        const float titleScale = options.title_scale > 0.0f ? options.title_scale : kTitleScale;
        const float messageScale = options.message_scale > 0.0f ? options.message_scale : kFontScale;
        const float statusScale = options.status_scale > 0.0f ? options.status_scale : kFontScale;
        const auto layout = gui_lib::make_loading_screen_layout(gui_lib::LoadingScreenLayoutOptions{
            .viewport = gui_lib::Rect{ to_lib(viewportPos), to_lib(viewportSize) },
            .preferred_panel_size = to_lib(options.panel_size),
            .minimum_panel_size = { 360.0f, 220.0f },
            .margin = 32.0f,
            .padding = 28.0f,
            .gap = 14.0f,
            .title_height = (std::max)(line_advance_amount(titleScale) + 4.0f, 30.0f),
            .message_height = (std::max)(76.0f, line_advance_amount(messageScale) * 3.0f + 10.0f),
            .progress_height = 24.0f,
            .status_height = line_advance_amount(statusScale) + 4.0f,
            .action_height = options.reserve_action_row ? 64.0f : 0.0f,
            .progress_padding = 2.0f,
            .progress_value = std::clamp(options.progress, 0.0f, 1.0f)
        });

        result.visible = layout.visible;
        result.panel = from_lib(layout.panel);
        result.progress = from_lib(layout.progress.track);
        result.action = from_lib(layout.action);
        if (!layout.visible)
            return result;

        if (options.capture_input)
            begin_modal_input_capture(viewportPos, viewportSize);

        const auto& palette = active_palette();
        ContentClipClearScope clearClip;

        if (options.dim_background)
        {
            draw_sprite(palette.modalScrim,
                viewportPos.x,
                viewportPos.y,
                viewportSize.x,
                viewportSize.y);
        }

        draw_sprite(palette.windowBackground,
            layout.panel.position.x,
            layout.panel.position.y,
            layout.panel.size.x,
            layout.panel.size.y);
        draw_sprite(palette.titleBar,
            layout.panel.position.x,
            layout.panel.position.y,
            layout.panel.size.x,
            4.0f);
        draw_sprite(palette.panelBackground,
            layout.panel.position.x,
            layout.panel.position.y + layout.panel.size.y - 2.0f,
            layout.panel.size.x,
            2.0f);
        draw_sprite(palette.panelBackground,
            layout.panel.position.x,
            layout.panel.position.y,
            2.0f,
            layout.panel.size.y);
        draw_sprite(palette.panelBackground,
            layout.panel.position.x + layout.panel.size.x - 2.0f,
            layout.panel.position.y,
            2.0f,
            layout.panel.size.y);

        if (!options.title.empty())
            draw_text_line(options.title, layout.title.position.x, layout.title.position.y, titleScale);

        if (!options.message.empty())
        {
            draw_wrapped_text(
                options.message,
                layout.message.position.x,
                layout.message.position.y,
                layout.message.size.x,
                messageScale);
        }

        draw_progress_bar_layout(
            layout.progress,
            options.progress_label,
            options.progress_status,
            options.show_percent,
            options.activity,
            options.activity_phase);

        if (!options.progress_status.empty())
            draw_text_line(options.progress_status, layout.status.position.x, layout.status.position.y, statusScale);

        return result;
    }

    void text_box(std::string_view text, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        ensure_resources();

        const Vec2 pos = g_frame.cursor;
        const float baseHeight = base_line_height(kFontScale);
        const float minWidth = space_advance(kFontScale) * 4.0f;

        const float availableWidth = (std::max)(minWidth, content_available_width(pos.x));
        float width = static_cast<float>(size.x);
        if (width <= 0.0f)
        {
            const float estimated = measure_text_width(text, kFontScale) + 2.0f * kBoxInnerPadding;
            width = (std::min)((std::max)(minWidth, estimated), availableWidth);
        }
        else
        {
            width = (std::min)((std::max)(width, minWidth), availableWidth);
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

        const float availableWidth = (std::max)(1.0f, content_available_width(pos.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(48.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const float height = options.size.y > 0.0f
            ? (std::max)(48.0f, options.size.y)
            : 180.0f;
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
            return result;

        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(g_frame.ctx)) + ":scroll-area"
            : std::string(options.id);
        const std::string stateKey = scroll_panel_key(id);
        auto& state = g_scrollAreaStates[stateKey];

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
        if (std::isfinite(options.requested_scroll_y)
            && options.requested_scroll_y >= 0.0f)
        {
            state.scrollY = (std::clamp)(
                options.requested_scroll_y, 0.0f, maxScroll);
        }

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        const bool selectList = is_select_box_list_id(id);
        const bool selectBoxCapturesWheel = any_select_box_open() && !selectList;
        if (options.capture_wheel && hovered
            && g_frame.mouseWheelDelta != 0 && !selectBoxCapturesWheel)
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
            .key = stateKey,
            .ownerWindowKey = g_frame.windowKey,
            .ownerWindowDepth = g_windowFrameStack.size(),
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

        if (g_scrollAreaStack.back().ownerWindowDepth
                != g_windowFrameStack.size()
            || g_scrollAreaStack.back().ownerWindowKey
                != g_frame.windowKey)
        {
            return;
        }

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
            ContentClipScope scrollbarClip{
                frame.viewportMin,
                frame.viewportMax };
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
        const float availableWidth = (std::max)(1.0f, content_available_width(pos.x));
        const float requestedWidth = options.size.x > 0.0f
            ? (std::max)(64.0f, options.size.x)
            : availableWidth;
        const float width = (std::max)(1.0f, (std::min)(requestedWidth, availableWidth));
        const float height = options.size.y > 0.0f
            ? (std::max)(48.0f, options.size.y)
            : 160.0f;
        const auto& palette = active_palette();

        draw_sprite(palette.consoleBackground, pos.x, pos.y, width, height);

        const float scrollbarWidth = options.lines.empty() ? 0.0f : 10.0f;
        const float contentX = pos.x + kBoxInnerPadding;
        const float contentY = pos.y + kBoxInnerPadding;
        const float contentWidth = (std::max)(1.0f, width - 2.0f * kBoxInnerPadding - scrollbarWidth);
        const float contentHeight = (std::max)(1.0f, height - 2.0f * kBoxInnerPadding);
        const float linePitch = line_advance_amount(kFontScale);
        const float rowGap = 2.0f;
        const float edgePadding = kGlyphRasterPadding;
        const float messageInsetX = 6.0f;
        const float messageInsetY = 3.0f;
        const std::size_t lineCount = options.lines.size();

        const auto line_role = [&](std::size_t lineIndex) noexcept
        {
            return lineIndex < options.line_roles.size()
                ? options.line_roles[lineIndex]
                : TextMessageRole::neutral;
        };
        const auto role_is_styled = [](TextMessageRole role) noexcept
        {
            return role != TextMessageRole::neutral;
        };
        const auto line_text_width = [&](std::size_t lineIndex) noexcept
        {
            const float inset = role_is_styled(line_role(lineIndex))
                ? messageInsetX
                : 0.0f;
            return (std::max)(1.0f, contentWidth - inset * 2.0f);
        };

        const auto line_view = [&](std::size_t lineIndex) noexcept -> std::string_view
        {
            std::string_view line{ options.lines[lineIndex] };
            if (options.max_line_chars > 0 && line.size() > options.max_line_chars)
                line = line.substr(0, options.max_line_chars);
            return line;
        };

        std::vector<float> lineHeights{};
        lineHeights.reserve(lineCount);

        float totalTextHeight = edgePadding * 2.0f;
        for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
        {
            const std::string_view line = line_view(lineIndex);
            const TextMessageRole role = line_role(lineIndex);
            const float textHeight = (options.wrap_lines && !line.empty())
                ? measure_wrapped_text_height(line, line_text_width(lineIndex), kFontScale)
                : linePitch;
            const float verticalInset = role_is_styled(role) ? messageInsetY : 0.0f;
            const float rowHeight = (std::max)(linePitch, textHeight)
                + rowGap + verticalInset * 2.0f;
            lineHeights.push_back(rowHeight);
            totalTextHeight += rowHeight;
        }

        const float maxScrollY = (std::max)(0.0f, totalTextHeight - contentHeight);

        const std::string id = options.id.empty()
            ? std::to_string(reinterpret_cast<std::uintptr_t>(options.lines.data()))
            : std::string(options.id);
        auto& state = g_scrollTextStates[scroll_panel_key(id)];
        const auto selected_line_range = [&]() noexcept -> std::pair<std::size_t, std::size_t>
        {
            if (!state.hasSelection || lineCount == 0u)
                return { 0u, 0u };

            const std::size_t lastLine = lineCount - 1u;
            const std::size_t first = (std::min)(state.selectionAnchorLine, state.selectedLine);
            const std::size_t last = (std::max)(state.selectionAnchorLine, state.selectedLine);
            return {
                (std::min)(first, lastLine),
                (std::min)(last, lastLine)
            };
        };

        const auto selected_lines_text = [&]() -> std::string
        {
            if (!state.hasSelection || lineCount == 0u)
                return {};

            const auto [first, last] = selected_line_range();
            std::string text{};
            for (std::size_t lineIndex = first; lineIndex <= last && lineIndex < lineCount; ++lineIndex)
            {
                if (!text.empty())
                    text.push_back('\n');
                text.append(line_view(lineIndex));
            }
            return text;
        };

        const float previousMaxScrollY = (std::max)(0.0f, state.lastContentPixelHeight - state.lastViewportHeight);
        const bool wasAtBottom = state.lastLineCount == 0
            || state.scrollY >= previousMaxScrollY - linePitch;
        if (options.scroll_to_end_generation != 0u
            && options.scroll_to_end_generation != state.scrollToEndGeneration)
        {
            state.scrollY = maxScrollY;
            state.scrollToEndGeneration = options.scroll_to_end_generation;
        }
        else if (options.stick_to_bottom && lineCount != state.lastLineCount && wasAtBottom)
            state.scrollY = maxScrollY;
        else
            state.scrollY = (std::clamp)(state.scrollY, 0.0f, maxScrollY);
        state.lastLineCount = lineCount;
        state.lastContentPixelHeight = totalTextHeight;
        state.lastViewportHeight = contentHeight;

        const bool hovered = point_in_rect(g_frame.mousePos, pos.x, pos.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        if (hovered && g_frame.mouseWheelDelta != 0)
        {
            const int wheelSteps = (std::max)(1, std::abs(g_frame.mouseWheelDelta) / 120);
            if (g_frame.mouseWheelDelta > 0)
                state.scrollY = (std::max)(0.0f, state.scrollY - static_cast<float>(wheelSteps) * linePitch * 3.0f);
            else
                state.scrollY = (std::min)(maxScrollY, state.scrollY + static_cast<float>(wheelSteps) * linePitch * 3.0f);

            g_frame.mouseWheelDelta = 0;
            result.wheel_scrolled = true;
            result.user_scrolled = true;
        }

        const bool canScroll = maxScrollY > 0.5f;
        if (scrollbarWidth > 0.0f && canScroll)
        {
            const float trackX = pos.x + width - kBoxInnerPadding - scrollbarWidth;
            const float trackY = contentY;
            const float visibleRatio = contentHeight / (std::max)(contentHeight, totalTextHeight);
            const float thumbHeight = (std::min)(contentHeight, (std::max)(18.0f, contentHeight * visibleRatio));
            const float scrollRatio = maxScrollY > 0.0f
                ? state.scrollY / maxScrollY
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
                state.scrollY = (std::clamp)(requested, 0.0f, 1.0f) * maxScrollY;
                result.first_visible_line = state.firstLine;
                result.user_scrolled = true;
            }
        }

        result.first_visible_line = (std::min)(state.firstLine, lineCount);
        bool openedContextMenuThisFrame = false;
        {
            ContentClipScope clip{
                { contentX, contentY },
                { contentX + contentWidth, contentY + contentHeight }
            };

            bool foundFirstVisible = false;
            float rowY = contentY + edgePadding - state.scrollY;
            for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
            {
                const float rowHeight = lineHeights[lineIndex];
                const float rowBottom = rowY + rowHeight;

                if (rowBottom < contentY)
                {
                    rowY += rowHeight;
                    continue;
                }

                if (rowY > contentY + contentHeight)
                    break;

                if (!foundFirstVisible)
                {
                    state.firstLine = lineIndex;
                    result.first_visible_line = lineIndex;
                    foundFirstVisible = true;
                }

                const bool lineHovered = hovered && point_in_rect(g_frame.mousePos, contentX, rowY, contentWidth, rowHeight);

                if (options.selectable && g_frame.justPressed && lineHovered)
                {
                    state.selectionAnchorLine = lineIndex;
                    state.selectedLine = lineIndex;
                    state.hasSelection = true;
                    state.draggingSelection = true;
                    result.selected_line = lineIndex;
                }
                else if (options.selectable && state.draggingSelection && g_frame.mouseDown && lineHovered)
                {
                    state.selectedLine = lineIndex;
                    state.hasSelection = true;
                    result.selected_line = lineIndex;
                }

                if (options.selectable && g_frame.rightJustPressed && lineHovered)
                {
                    const auto [firstSelected, lastSelected] = selected_line_range();
                    if (!state.hasSelection || lineIndex < firstSelected || lineIndex > lastSelected)
                    {
                        state.selectionAnchorLine = lineIndex;
                        state.selectedLine = lineIndex;
                        state.hasSelection = true;
                        result.selected_line = lineIndex;
                    }
                    state.contextMenuOpen = true;
                    state.contextMenuPos = g_frame.mousePos;
                    openedContextMenuThisFrame = true;
                }

                const TextMessageRole role = line_role(lineIndex);
                const bool styledRole = role_is_styled(role);
                if (styledRole)
                {
                    switch (role)
                    {
                    case TextMessageRole::user:
                        draw_sprite(palette.buttonActive, contentX, rowY, contentWidth, rowHeight - rowGap);
                        break;
                    case TextMessageRole::assistant:
                        draw_sprite(palette.panelBackground, contentX, rowY, contentWidth, rowHeight - rowGap);
                        break;
                    case TextMessageRole::system:
                        draw_sprite(palette.buttonNormal, contentX, rowY, contentWidth, rowHeight - rowGap);
                        break;
                    case TextMessageRole::error:
                        draw_sprite(palette.buttonHover, contentX, rowY, contentWidth, rowHeight - rowGap);
                        break;
                    case TextMessageRole::neutral:
                    default:
                        break;
                    }
                }

                const auto [firstSelected, lastSelected] = selected_line_range();
                if (options.selectable && state.hasSelection && lineIndex >= firstSelected && lineIndex <= lastSelected)
                    draw_sprite(palette.buttonActive, contentX - 2.0f, rowY - 1.0f, contentWidth + 4.0f, rowHeight);
                else if (lineHovered)
                    draw_sprite(palette.buttonHover, contentX - 2.0f, rowY - 1.0f, contentWidth + 4.0f, rowHeight);

                const std::string_view line = line_view(lineIndex);
                const float textX = contentX + (styledRole ? messageInsetX : 0.0f);
                const float textY = rowY + (styledRole ? messageInsetY : 0.0f);
                const float textWidth = line_text_width(lineIndex);
                if (options.wrap_lines)
                    draw_wrapped_text(line, textX, textY, textWidth, kFontScale);
                else
                {
                    const std::string fitted = fit_text_to_width(line, textWidth, kFontScale);
                    draw_text_line(fitted.empty() ? line : std::string_view{ fitted }, textX, textY, kFontScale);
                }

                rowY += rowHeight;
            }
        }

        if (!g_frame.mouseDown)
            state.draggingSelection = false;

        if (scrollbarWidth > 0.0f)
        {
            const float trackX = pos.x + width - kBoxInnerPadding - scrollbarWidth;
            const float trackY = contentY;
            draw_sprite(palette.textField, trackX, trackY, scrollbarWidth, contentHeight);

            if (canScroll)
            {
                const float visibleRatio = contentHeight / (std::max)(contentHeight, totalTextHeight);
                const float thumbHeight = (std::max)(18.0f, contentHeight * visibleRatio);
                const float scrollRatio = maxScrollY > 0.0f
                    ? state.scrollY / maxScrollY
                    : 0.0f;
                const float thumbY = trackY + (contentHeight - thumbHeight) * scrollRatio;
                draw_sprite(palette.buttonActive, trackX, thumbY, scrollbarWidth, thumbHeight);
            }
        }

        if (state.hasSelection && state.selectedLine < lineCount)
            result.selected_line = state.selectedLine;

        if (state.contextMenuOpen)
        {
            const float rowHeight = 28.0f;
            const float menuPadding = 4.0f;
            const float menuWidth = 188.0f;
            const float menuHeight = menuPadding * 2.0f + rowHeight * 3.0f + kContentPadding * 2.0f;
            Vec2 menuPos = state.contextMenuPos;
            menuPos.x = (std::min)(menuPos.x, (std::max)(0.0f, g_frame.origin.x + g_frame.windowSize.x - menuWidth - kContentPadding));
            menuPos.y = (std::min)(menuPos.y, (std::max)(0.0f, g_frame.origin.y + g_frame.windowSize.y - menuHeight - kContentPadding));

            const bool hoveredMenu = point_in_rect(g_frame.mousePos, menuPos.x, menuPos.y, menuWidth, menuHeight);
            if (!openedContextMenuThisFrame && (g_frame.justPressed || g_frame.rightJustPressed) && !hoveredMenu)
                state.contextMenuOpen = false;

            if (state.contextMenuOpen)
            {
                const Vec2 savedCursor = g_frame.cursor;
                const Vec2 savedOrigin = g_frame.origin;
                const Vec2 savedWindowSize = g_frame.windowSize;
                const Vec2 savedContentMin = g_frame.contentMin;
                const Vec2 savedContentMax = g_frame.contentMax;
                const bool savedInsideWindow = g_frame.insideWindow;
                const std::string savedWindowKey = g_frame.windowKey;
                const std::uint64_t savedWidgetSerial = g_frame.widgetSerial;

                begin_top_layer();
                const auto& menuPalette = active_palette();
                draw_sprite(menuPalette.windowBackground, menuPos.x, menuPos.y, menuWidth, menuHeight);
                draw_sprite(menuPalette.titleBar, menuPos.x, menuPos.y, menuWidth, 2.0f);
                draw_sprite(menuPalette.titleBar, menuPos.x, menuPos.y, 2.0f, menuHeight);
                draw_sprite(menuPalette.panelBackground, menuPos.x + menuWidth - 2.0f, menuPos.y, 2.0f, menuHeight);
                draw_sprite(menuPalette.panelBackground, menuPos.x, menuPos.y + menuHeight - 2.0f, menuWidth, 2.0f);

                g_frame.insideWindow = true;
                g_frame.windowKey = id + "-text-context-menu";
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                set_cursor(g_frame.contentMin);

                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    state.selectionAnchorLine = 0u;
                    state.selectedLine = lineCount > 0u ? lineCount - 1u : 0u;
                    state.hasSelection = lineCount > 0u;
                    state.contextMenuOpen = false;
                }
                if (button("Copy Selection", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)clipboard_write_text(selected_lines_text());
                    state.contextMenuOpen = false;
                }
                if (button("Clear Selection", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    state.hasSelection = false;
                    state.contextMenuOpen = false;
                }

                g_frame.cursor = savedCursor;
                g_frame.origin = savedOrigin;
                g_frame.windowSize = savedWindowSize;
                g_frame.contentMin = savedContentMin;
                g_frame.contentMax = savedContentMax;
                g_frame.insideWindow = savedInsideWindow;
                g_frame.windowKey = savedWindowKey;
                g_frame.widgetSerial = savedWidgetSerial;
                end_top_layer();
            }
        }

        result.scroll_y = state.scrollY;
        result.maximum_scroll_y = maxScrollY;
        result.at_end = state.scrollY >= maxScrollY - linePitch;
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

        const float controlRowHeight = base_line_height(kFontScale)
            + 2.0f * kBoxInnerPadding;
        const bool taskVisible = options.task_editing
            || !options.task_label.empty() || !options.task_value.empty()
            || !options.task_actions.empty();
        const float taskValueHeight = taskVisible ? controlRowHeight : 0.0f;
        const float taskActionHeight = options.task_actions.empty()
            ? 0.0f : controlRowHeight;
        const float fieldHeight = options.input
            ? controlRowHeight
            : 0.0f;
        const float headerHeight = options.header_actions.empty()
            ? 0.0f
            : controlRowHeight;
        const float messageHeight = options.message_actions.empty()
            ? 0.0f
            : controlRowHeight;
        const float footerHeight = options.footer_actions.empty()
            ? 0.0f
            : controlRowHeight;
        const float interControlGap = options.input && !options.footer_actions.empty()
            ? kContentPadding
            : 0.0f;

        const float taskBlockHeight = taskValueHeight
            + (taskValueHeight > 0.0f && taskActionHeight > 0.0f
                ? kContentPadding : 0.0f)
            + taskActionHeight;
        const float contentTopY = g_frame.cursor.y;
        const float contentX = g_frame.cursor.x;
        const float contentBottomY = options.position.y + options.size.y - kContentPadding;
        const float headerY = contentTopY + taskBlockHeight
            + (taskBlockHeight > 0.0f && headerHeight > 0.0f
                ? kContentPadding : 0.0f);
        const float logTopY = headerY + headerHeight
            + (taskBlockHeight > 0.0f && headerHeight == 0.0f
                ? kContentPadding : 0.0f)
            + (headerHeight > 0.0f ? kContentPadding : 0.0f);
        const float controlHeight = fieldHeight + interControlGap + footerHeight;
        float reservedBottom = 0.0f;
        if (messageHeight > 0.0f)
            reservedBottom += messageHeight + kContentPadding;
        if (controlHeight > 0.0f)
            reservedBottom += controlHeight + kContentPadding;

        const float logHeight = (std::max)(0.0f, contentBottomY - logTopY - reservedBottom);
        const Vec2 logPos{g_frame.cursor.x, logTopY};

        const auto render_actions = [&](
            const std::span<const ConsoleWindowActionSpec> actions,
            const float actionGap,
            const float actionY,
            std::optional<std::size_t>& selectedAction)
        {
            if (actions.empty())
                return;

            const float gap = (std::max)(0.0f, actionGap);
            const float totalGap = gap
                * static_cast<float>(actions.size() - 1u);
            const float availableActionWidth = (std::max)(
                0.0f,
                availableWidth - totalGap);
            float requestedActionWidth = 0.0f;
            for (const ConsoleWindowActionSpec& action : actions)
                requestedActionWidth += (std::max)(32.0f, action.width);
            const float widthScale = requestedActionWidth > availableActionWidth
                && requestedActionWidth > 0.0f
                ? availableActionWidth / requestedActionWidth
                : 1.0f;

            float actionX = contentX;
            for (std::size_t index = 0u; index < actions.size(); ++index)
            {
                const ConsoleWindowActionSpec& action = actions[index];
                const float actionWidth = (std::max)(
                    1.0f,
                    (std::max)(32.0f, action.width) * widthScale);
                const Vec2 actionPosition{actionX, actionY};
                const Vec2 actionSize{actionWidth, controlRowHeight};
                bool activatedOnPress = false;
                if (action.enabled && action.activate_on_press
                    && point_in_rect(
                        g_frame.mousePos,
                        actionPosition.x,
                        actionPosition.y,
                        actionSize.x,
                        actionSize.y)
                    && point_in_active_clip(g_frame.mousePos)
                    && left_press_available())
                {
                    activatedOnPress = true;
                    consume_left_press();
                }
                set_cursor({actionX, actionY});
                const bool clicked = button_with_state(
                    action.label,
                    actionSize,
                    false,
                    action.enabled);
                if (activatedOnPress
                    || (!action.activate_on_press && clicked))
                {
                    selectedAction = index;
                }
                actionX += actionWidth + gap;
            }
        };

        if (taskVisible)
        {
            set_cursor({contentX, contentTopY});
            if (options.task_editing && options.task_edit_buffer)
            {
                result.task_input = edit_box(
                    *options.task_edit_buffer,
                    {availableWidth, taskValueHeight},
                    options.task_max_input_chars,
                    false);
            }
            else
            {
                std::string taskText{};
                if (!options.task_label.empty())
                    taskText = std::string{options.task_label} + ": ";
                taskText += options.task_value;
                text_box(taskText, {availableWidth, taskValueHeight});
            }
            render_actions(
                options.task_actions,
                options.task_action_gap,
                contentTopY + taskValueHeight
                    + (taskActionHeight > 0.0f ? kContentPadding : 0.0f),
                result.task_action_index);
        }

        render_actions(
            options.header_actions,
            options.header_action_gap,
            headerY,
            result.header_action_index);

        if (availableWidth > 0.0f && logHeight > 0.0f)
        {
            set_cursor(logPos);
            const std::string panelId = options.log_id.empty()
                ? std::string(options.title) + ".log"
                : std::string(options.log_id);
            const ScrollTextPanelResult log = scroll_text_panel(ScrollTextPanelOptions{
                .id = panelId,
                .size = { availableWidth, logHeight },
                .lines = options.lines,
                .line_roles = options.line_roles,
                .max_line_chars = options.max_visible_lines == 0 ? 768u : options.max_visible_lines * 16u,
                .selectable = true,
                .stick_to_bottom = options.follow_tail,
                .scroll_to_end_generation = options.scroll_to_end_generation
            });
            result.log_user_scrolled = log.user_scrolled;
            result.log_at_end = log.at_end;
            result.log_scroll_y = log.scroll_y;
            result.log_maximum_scroll_y = log.maximum_scroll_y;
        }

        const float messageY = logPos.y + logHeight
            + (messageHeight > 0.0f ? kContentPadding : 0.0f);
        render_actions(
            options.message_actions,
            options.message_action_gap,
            messageY,
            result.message_action_index);

        const float controlsY = messageY + messageHeight
            + (controlHeight > 0.0f ? kContentPadding : 0.0f);
        set_cursor({ logPos.x, controlsY });

        if (options.input)
        {
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

        if (!options.footer_actions.empty())
        {
            const float footerY = controlsY + fieldHeight + interControlGap;
            render_actions(
                options.footer_actions,
                options.footer_action_gap,
                footerY,
                result.footer_action_index);
        }

        end_window();
        return result;
    }
} // namespace epochengine::gui

extern "C" std::uint32_t epoch_gui_render_frame_batches(void* context) noexcept
{
    auto* renderContext = static_cast<epochengine::core::Context*>(context);
    if (!renderContext)
        return 0u;

    std::uint32_t rendered = 0u;
    if (epochengine::gui::render_deferred_batch(renderContext))
        rendered |= 1u;
    if (epochengine::gui::render_top_layer_batch(renderContext))
        rendered |= 2u;
    return rendered;
}
