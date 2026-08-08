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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <format>
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
            GuiFontCache font{};
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
            std::string scrollId{};
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
        };

        struct EditBoxMenuState
        {
            bool open = false;
            Vec2 pos{};
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

        struct WindowFrameState
        {
            Vec2 cursor{};
            Vec2 origin{};
            Vec2 windowSize{};
            Vec2 contentMin{};
            Vec2 contentMax{};
            std::string windowKey{};
            std::uint64_t widgetSerial = 0;
            bool insideWindow = false;
        };

        static thread_local std::unordered_map<std::string, ScrollTextState> g_scrollTextStates{};
        static thread_local std::unordered_map<std::string, ScrollAreaState> g_scrollAreaStates{};
        static thread_local std::unordered_map<std::string, SelectBoxState> g_selectBoxStates{};
        static thread_local std::unordered_map<std::string, SourceEditorState> g_sourceEditorStates{};
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
            bool modalInputCapture = false;
            bool inputBlockedUntilClear = false;
            Vec2 modalInputMin{};
            Vec2 modalInputMax{};

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
            SpriteHandle handle = try_add_sprite(atlas, name, pixels, w, h, false);
            if (!handle.is_valid())
                throw std::runtime_error("[agui] Failed to register GUI sprite: " + name);
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

            auto atlasVec = epochengine::atlasmanager::get_atlas_vector_snapshot(); // by value snapshot
            if (g_resources.font.asset->atlas_index >= 0 &&
                static_cast<std::size_t>(g_resources.font.asset->atlas_index) < atlasVec.size())
            {
                g_resources.font.atlas = atlasVec[static_cast<std::size_t>(g_resources.font.asset->atlas_index)];
            }

            if (!g_resources.font.atlas)
            {
                if (auto it = epochengine::atlasmanager::atlas_map.find("font_atlas");
                    it != epochengine::atlasmanager::atlas_map.end())
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
                    make_solid_pixels(0x68, 0x6F, 0x78, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.buttonNormal = add_sprite(atlas, "__agui_light/button_normal",
                    make_solid_pixels(0x78, 0x80, 0x8B, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.buttonHover = add_sprite(atlas, "__agui_light/button_hover",
                    make_solid_pixels(0x8B, 0x95, 0xA1, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.buttonActive = add_sprite(atlas, "__agui_light/button_active",
                    make_solid_pixels(0x5E, 0x75, 0x94, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.textField = add_sprite(atlas, "__agui_light/text_field",
                    make_solid_pixels(0x61, 0x67, 0x70, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.textFieldActive = add_sprite(atlas, "__agui_light/text_field_active",
                    make_solid_pixels(0x53, 0x68, 0x82, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.panelBackground = add_sprite(atlas, "__agui_light/panel_bg",
                    make_solid_pixels(0x70, 0x78, 0x82, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.consoleBackground = add_sprite(atlas, "__agui_light/console_bg",
                    make_solid_pixels(0x55, 0x5B, 0x64, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.titleBar = add_sprite(atlas, "__agui_light/title_bar",
                    make_solid_pixels(0x50, 0x5C, 0x68, 0xFF, 8, 8), 8, 8);
                g_resources.defaultLight.modalScrim = add_sprite(atlas, "__agui_light/modal_scrim",
                    make_solid_pixels(0x22, 0x26, 0x2D, 0x98, 8, 8), 8, 8);

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
                    { 0x78, 0x80, 0x8B }, { 0x8B, 0x95, 0xA1 }, { 0x5E, 0x75, 0x94 },
                    { 0x61, 0x67, 0x70 }, { 0x53, 0x68, 0x82 }
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

            draw_sprite_raw(handle, x + radius, y, w - radius * 2.0f, h);
            draw_sprite_raw(handle, x, y + radius, radius, h - radius * 2.0f);
            draw_sprite_raw(handle, x + w - radius, y + radius, radius, h - radius * 2.0f);
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
            if (g_frame.inputBlockedUntilClear)
                return false;
            if (!g_frame.modalInputCapture)
                return true;

            return point_in_rect(
                p,
                g_frame.modalInputMin.x,
                g_frame.modalInputMin.y,
                (std::max)(0.0f, g_frame.modalInputMax.x - g_frame.modalInputMin.x),
                (std::max)(0.0f, g_frame.modalInputMax.y - g_frame.modalInputMin.y));
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
            g_frame.modalInputCapture = false;
            g_frame.inputBlockedUntilClear = false;
            g_frame.modalInputMin = {};
            g_frame.modalInputMax = {};
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
        {
            std::scoped_lock lock(g_deferredBatchMutex);
            const auto it = g_topLayerDrawBatches.find(ctx);
            if (it == g_topLayerDrawBatches.end())
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
        g_frame.inputBlockedUntilClear = false;
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

    void begin_modal_input_capture(Vec2 position, Vec2 size) noexcept
    {
        g_frame.inputBlockedUntilClear = false;
        g_frame.modalInputCapture = size.x > 0.0f && size.y > 0.0f;
        g_frame.modalInputMin = position;
        g_frame.modalInputMax = {
            position.x + (std::max)(0.0f, size.x),
            position.y + (std::max)(0.0f, size.y)
        };
    }

    void block_input_until_clear() noexcept
    {
        g_frame.inputBlockedUntilClear = true;
        g_frame.modalInputCapture = false;
        g_frame.modalInputMin = {};
        g_frame.modalInputMax = {};
    }

    void clear_modal_input_capture() noexcept
    {
        g_frame.modalInputCapture = false;
        g_frame.inputBlockedUntilClear = false;
        g_frame.modalInputMin = {};
        g_frame.modalInputMax = {};
    }

    std::span<const ThemePreferenceChoice> theme_preference_choices() noexcept
    {
        static constexpr std::array<ThemePreferenceChoice, 3> kChoices{ {
            { "System Light/Dark", ThemePreference::FollowSystemDark },
            { "Light", ThemePreference::Light },
            { "Dark", ThemePreference::Dark }
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

        auto& scrollState = g_scrollAreaStates[scroll_panel_key(popup.scrollId)];
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

        if (options.capture_input && !g_frame.modalInputCapture)
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
            selected || pressed ? palette.buttonActive
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

    float measure_wrapped_label_height(std::string_view text, float width) noexcept
    {
        try { ensure_resources(); }
        catch (...) { return 0.0f; }

        const float wrapWidth = width > 0.0f
            ? (std::max)(space_advance(kFontScale), width)
            : 1.0f;
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
        const void* id = widget_focus_key("edit-box", static_cast<const void*>(&text), pos, { width, height });
        const void* ctxKey = static_cast<const void*>(g_frame.ctx);
        const void* activeWidget = ctxKey ? g_contextActiveWidgets[ctxKey] : nullptr;
        bool& wholeFieldSelected = g_textFieldSelectAllStates[id];
        auto& menuState = g_editBoxMenuStates[id];
        const auto& palette = active_palette();

        if (g_frame.justPressed || g_frame.rightJustPressed)
        {
            if (hovered)
            {
                activeWidget = id;
                g_frame.caretTimer = 0.0f;
                g_frame.caretVisible = true;
                if (g_frame.justPressed)
                    wholeFieldSelected = false;
            }
            else if (activeWidget == id)
            {
                activeWidget = nullptr;
                wholeFieldSelected = false;
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
            const auto replace_selection_if_needed = [&]() noexcept
            {
                if (!wholeFieldSelected)
                    return;
                if (!text.empty())
                {
                    text.clear();
                    result.changed = true;
                }
                wholeFieldSelected = false;
            };

            for (const auto& evt : g_frame.events)
            {
                switch (evt.type)
                {
                case EventType::TextInput:
                    replace_selection_if_needed();
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
                        wholeFieldSelected = false;
                    }
                    else if (evt.ctrl_down && (evt.key == 'V' || evt.key == 'v'))
                    {
                        replace_selection_if_needed();
                        append_text_limited(text, clipboard_read_text(), limit, multiline, result.changed);
                    }
                    else if (evt.ctrl_down && (evt.key == 'A' || evt.key == 'a'))
                    {
                        wholeFieldSelected = true;
                    }
                    else if (evt.key == 8 || evt.key == 127) // backspace/del-ish
                    {
                        if (wholeFieldSelected)
                        {
                            if (!text.empty())
                            {
                                text.clear();
                                result.changed = true;
                            }
                            wholeFieldSelected = false;
                        }
                        else if (!text.empty())
                        {
                            text.pop_back();
                            result.changed = true;
                        }
                    }
                    else if (evt.key == 27) // ESC
                    {
                        activeWidget = nullptr;
                        if (ctxKey)
                            g_contextActiveWidgets[ctxKey] = nullptr;
                        result.active = false;
                        wholeFieldSelected = false;
                    }
                    else if (evt.key == 13) // ENTER
                    {
                        if (multiline)
                        {
                            replace_selection_if_needed();
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

        bool openedContextMenuThisFrame = false;
        if (g_frame.rightJustPressed && hovered)
        {
            menuState.open = true;
            menuState.pos = g_frame.mousePos;
            openedContextMenuThisFrame = true;
        }

        // Create view after edits.
        const std::string_view sv{ text };

        {
            ContentClipScope clip(
                { pos.x + 1.0f, pos.y + 1.0f },
                { pos.x + width - 1.0f, pos.y + height - 1.0f });

            if (active && wholeFieldSelected && !text.empty())
                draw_sprite(palette.buttonActive, textX, textY, contentWidth, contentHeight);

            if (multiline) draw_wrapped_text(sv, textX, textY, contentWidth, kFontScale);
            else           draw_text_line(sv, textX, textY, kFontScale);
        }

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
                g_frame.windowKey = std::string("edit-box-context-menu-") + std::to_string(reinterpret_cast<std::uintptr_t>(id));
                g_frame.widgetSerial = 0;
                g_frame.origin = menuPos;
                g_frame.windowSize = { menuWidth, menuHeight };
                g_frame.contentMin = { menuPos.x + menuPadding, menuPos.y + menuPadding };
                g_frame.contentMax = { menuPos.x + menuWidth - menuPadding, menuPos.y + menuHeight - menuPadding };
                set_cursor(g_frame.contentMin);

                if (button("Select All", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    wholeFieldSelected = !text.empty();
                    menuState.open = false;
                }
                if (button("Copy", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)clipboard_write_text(text);
                    menuState.open = false;
                }
                if (button("Cut", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    (void)clipboard_write_text(text);
                    if (!text.empty())
                    {
                        text.clear();
                        result.changed = true;
                    }
                    wholeFieldSelected = false;
                    menuState.open = false;
                }
                if (button("Paste", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    if (wholeFieldSelected)
                    {
                        text.clear();
                        wholeFieldSelected = false;
                    }
                    append_text_limited(text, clipboard_read_text(), limit, multiline, result.changed);
                    menuState.open = false;
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

        advance_cursor({ 0.0f, height + kContentPadding });
        return result;
    }

    void select_all_text_in_edit_box(std::string& text) noexcept
    {
        const void* id = static_cast<const void*>(&text);
        g_textFieldSelectAllStates[id] = true;
        if (g_frame.ctx)
            g_contextActiveWidgets[static_cast<const void*>(g_frame.ctx)] = id;
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
            const unsigned char raw = static_cast<unsigned char>(text[i]);
            if (is_utf8_continuation_byte(raw))
                continue;

            const unsigned char ch = safe_draw_char(raw);
            const float advance = glyph_advance_with_kerning(ch, next_drawable_char(text, i), scale);
            if (localX <= penX + advance * 0.5f)
                return i;
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
        return true;
    }

    static void source_editor_insert_text_limited(
        std::string& text,
        SourceEditorState& state,
        std::string_view incoming,
        std::size_t limit,
        bool& changed)
    {
        source_editor_delete_selection(text, state);

        const std::size_t cursor = (std::min)(state.cursorIndex, text.size());
        const std::size_t available = limit > text.size() ? limit - text.size() : 0u;
        if (available == 0u)
            return;

        std::string filtered{};
        filtered.reserve((std::min)(incoming.size(), available));
        for (char ch : incoming)
        {
            if (filtered.size() >= available)
                break;
            if (ch == '\r')
                continue;
            if (ch == '\n')
            {
                filtered.push_back('\n');
                continue;
            }
            if (static_cast<unsigned char>(ch) < 32u)
                continue;
            filtered.push_back(ch);
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
                    source_editor_insert_text_limited(text, state, evt.text, limit, result.edit.changed);
                    break;

                case EventType::KeyDown:
                    if (evt.ctrl_down && (evt.key == 'C' || evt.key == 'c'))
                    {
                        result.copied = clipboard_write_text(source_editor_selected_text(text, state));
                    }
                    else if (evt.ctrl_down && (evt.key == 'X' || evt.key == 'x'))
                    {
                        result.cut = clipboard_write_text(source_editor_selected_text(text, state));
                        if (source_editor_has_selection(state, text.size()))
                        {
                            replace_selection_if_needed();
                        }
                    }
                    else if (evt.ctrl_down && (evt.key == 'V' || evt.key == 'v'))
                    {
                        source_editor_insert_text_limited(text, state, clipboard_read_text(), limit, result.edit.changed);
                        result.pasted = true;
                    }
                    else if (evt.ctrl_down && (evt.key == 'A' || evt.key == 'a'))
                    {
                        state.selectionAnchor = 0u;
                        state.cursorIndex = text.size();
                        state.hasSelection = !text.empty();
                        result.selected_all = state.hasSelection;
                    }
                    else if (evt.key == 8 || evt.key == 127)
                    {
                        if (source_editor_has_selection(state, text.size()))
                        {
                            replace_selection_if_needed();
                        }
                        else if (evt.key == 8 && state.cursorIndex > 0u)
                        {
                            const std::size_t eraseIndex = state.cursorIndex - 1u;
                            text.erase(eraseIndex, 1u);
                            state.cursorIndex = eraseIndex;
                            state.selectionAnchor = eraseIndex;
                            result.edit.changed = true;
                        }
                        else if (evt.key == 127 && state.cursorIndex < text.size())
                        {
                            text.erase(state.cursorIndex, 1u);
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
                        source_editor_insert_text_limited(text, state, "\n", limit, result.edit.changed);
                    }
                    else if (evt.key == 37) // VK_LEFT
                    {
                        const std::size_t next = state.cursorIndex > 0u ? state.cursorIndex - 1u : 0u;
                        source_editor_set_cursor(state, next, evt.shift_down, text.size());
                    }
                    else if (evt.key == 39) // VK_RIGHT
                    {
                        const std::size_t next = (std::min)(state.cursorIndex + 1u, text.size());
                        source_editor_set_cursor(state, next, evt.shift_down, text.size());
                    }
                    else if (evt.key == 36) // VK_HOME
                    {
                        std::size_t lineStart = 0u;
                        for (std::size_t i = 0u; i < (std::min)(state.cursorIndex, text.size()); ++i)
                            if (text[i] == '\n')
                                lineStart = i + 1u;
                        source_editor_set_cursor(state, lineStart, evt.shift_down, text.size());
                    }
                    else if (evt.key == 35) // VK_END
                    {
                        std::size_t currentLineStart = 0u;
                        for (std::size_t i = 0u; i < (std::min)(state.cursorIndex, text.size()); ++i)
                            if (text[i] == '\n')
                                currentLineStart = i + 1u;
                        source_editor_set_cursor(state, source_editor_line_end_for_start(text, currentLineStart), evt.shift_down, text.size());
                    }
                    break;

                default:
                    break;
                }
            }
        }

        draw_sprite(active ? palette.textFieldActive : palette.textField, pos.x, pos.y, editorWidth, height);
        const std::string scrollId = id + "-scroll";

        const auto scroll = begin_scroll_area(ScrollAreaOptions{
            .id = scrollId,
            .size = { width, height },
            .content_height = editorContentHeight,
            .draw_background = false,
            .show_scrollbar = true
        });

        const float textX = g_frame.cursor.x + kBoxInnerPadding;
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
                firstTextY,
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
                firstTextY,
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

        if (active && g_frame.caretVisible && !source_editor_has_selection(state, text.size()))
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

            const float caretX = std::clamp(
                textX + source_editor_x_for_index(text, caretLineStart, cursor, kFontScale),
                textX,
                textX + (std::max)(1.0f, contentWidth) - 1.0f);
            const float caretY = firstTextY + static_cast<float>(caretLineIndex) * lineAdvance;
            if (caretY + baseHeight >= pos.y && caretY <= pos.y + height)
                draw_caret(caretX, caretY, baseHeight);
        }

        g_frame.cursor = { pos.x, pos.y - scroll.scroll_y + editorContentHeight };
        end_scroll_area();

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
                    if (result.cut && source_editor_has_selection(state, text.size()))
                    {
                        (void)source_editor_delete_selection(text, state);
                        result.edit.changed = true;
                    }
                    state.contextMenuOpen = false;
                }
                if (button("Paste", { menuWidth - 2.0f * menuPadding, rowHeight }))
                {
                    source_editor_insert_text_limited(text, state, clipboard_read_text(), options.max_chars, result.edit.changed);
                    result.pasted = true;
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
            popup.scrollId = listId;
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
        const std::size_t lineCount = options.lines.size();

        const auto line_view = [&](std::size_t lineIndex) noexcept -> std::string_view
        {
            std::string_view line{ options.lines[lineIndex] };
            if (options.max_line_chars > 0 && line.size() > options.max_line_chars)
                line = line.substr(0, options.max_line_chars);
            return line;
        };

        std::vector<float> lineHeights{};
        lineHeights.reserve(lineCount);

        float totalTextHeight = 0.0f;
        for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
        {
            const std::string_view line = line_view(lineIndex);
            const float textHeight = (options.wrap_lines && !line.empty())
                ? measure_wrapped_text_height(line, contentWidth, kFontScale)
                : linePitch;
            const float rowHeight = (std::max)(linePitch, textHeight) + rowGap;
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
        if (options.stick_to_bottom && lineCount != state.lastLineCount && wasAtBottom)
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
            float rowY = contentY - state.scrollY;
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

                const auto [firstSelected, lastSelected] = selected_line_range();
                if (options.selectable && state.hasSelection && lineIndex >= firstSelected && lineIndex <= lastSelected)
                    draw_sprite(palette.buttonActive, contentX - 2.0f, rowY - 1.0f, contentWidth + 4.0f, rowHeight);
                else if (lineHovered)
                    draw_sprite(palette.buttonHover, contentX - 2.0f, rowY - 1.0f, contentWidth + 4.0f, rowHeight);

                const std::string_view line = line_view(lineIndex);
                if (options.wrap_lines)
                    draw_wrapped_text(line, contentX, rowY, contentWidth, kFontScale);
                else
                {
                    const std::string fitted = fit_text_to_width(line, contentWidth, kFontScale);
                    draw_text_line(fitted.empty() ? line : std::string_view{ fitted }, contentX, rowY, kFontScale);
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
