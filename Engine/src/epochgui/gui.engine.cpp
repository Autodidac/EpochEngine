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
// Epoch system notes (auto-checked):
// Epoch Context: window/context state, dispatch and input ownership remain scoped.
// EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.
// Engine/src/epochgui/gui.engine.cpp
module;

#include "core.format_text.hpp"
#include "gui.theme_cycle.hpp"
#include "gui.theme_palette_spec.hpp"
#include "gui.popup_input_ownership.hpp"
#include "gui.floating_input_policy.hpp"
#include <gui/node_graph_workspace.hpp>
#include <gui/circle_raster.hpp>
#include <gui/rounded_corner_raster.hpp>
#include <gui/chat_composer_layout.hpp>
#include <gui/gui.chat_context_popup.hpp>
#include <gui/utf8_text_layout.hpp>
#include <gui/status_strip_layout.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <functional>
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
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include "../platform/platform.framework.hpp"
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
    // Readable desktop default at 1440p/4K. Text geometry is derived from
    // the same atlas metrics, so wrapping, controls and hit-testing agree.
    constexpr float       kDefaultFontSizePt = 21.0f;
    constexpr float       kFontScale = 1.0f;
    constexpr float       kTitleScale = 1.07f;
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
    constexpr const char* kHalfTextFontName = "__agui_placeholder_half_dark";
    constexpr const char* kHalfTextLightFontName = "__agui_placeholder_half_light";
    constexpr const char* kTextShadowFontName = "__agui_text_shadow_font";
    // Pre-rasterized semantic text colors; no per-frame recoloring or CPU pixels.
    constexpr const char* kStatusInfoFontName = "__agui_status_info_font";
    constexpr const char* kStatusInfoLightFontName = "__agui_status_info_light_font";
    constexpr const char* kStatusSuccessFontName = "__agui_status_success_font";
    constexpr const char* kStatusSuccessLightFontName = "__agui_status_success_light_font";
    constexpr const char* kStatusWarningFontName = "__agui_status_warning_font";
    constexpr const char* kStatusWarningLightFontName = "__agui_status_warning_light_font";
    constexpr const char* kStatusErrorFontName = "__agui_status_error_font";
    constexpr const char* kStatusErrorLightFontName = "__agui_status_error_light_font";
    constexpr const char* kDefaultFontFile = "Roboto-Regular.ttf";
    constexpr const char* kRuntimeSurfaceAtlasName = "__agui_runtime_surfaces";
    constexpr std::uint32_t kRuntimeSurfaceAtlasSize = 4096;

    namespace
    {
#include "controls/gui.context_roles.inl"
        struct GuiFontCache
        {
            std::string fontName = kDefaultFontName;
            float fontSizePt = kDefaultFontSizePt;
            const font::FontAsset* asset = nullptr;
            const TextureAtlas* atlas = nullptr;
            font::FontMetrics metrics{};
            std::array<const font::Glyph*, 256> glyphLookup{};
            const font::Glyph* fallbackGlyph = nullptr;
            bool loadFailed{}; // Retried only when the font file evidence changes.
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
                bool isLight{};
            };

            struct SemanticSpritePair
            {
                SpriteHandle background{};
                SpriteHandle accent{};
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
            PaletteSprites codexBlack{};
            std::array<SemanticSpritePair, gui_lib::semantic_tone_count> semanticDark{};
            std::array<SemanticSpritePair, gui_lib::semantic_tone_count> semanticLight{};
            SpriteHandle dockGuide{};
            SpriteHandle dockGuideHover{};
            SpriteHandle dockGuidePreview{};
            SpriteHandle dockContextGuide{};
            SpriteHandle dockContextHover{};
            GuiFontCache font{};
            GuiFontCache darkTextFont{
                .fontName = kDarkTextFontName
            };
            GuiFontCache halfTextFont{.fontName = kHalfTextFontName};
            GuiFontCache halfTextLightFont{.fontName = kHalfTextLightFontName};
            GuiFontCache textShadowFont{
                .fontName = kTextShadowFontName
            };
            GuiFontCache statusInfoFont{.fontName = kStatusInfoFontName};
            GuiFontCache statusInfoLightFont{.fontName = kStatusInfoLightFontName};
            GuiFontCache statusSuccessFont{.fontName = kStatusSuccessFontName};
            GuiFontCache statusSuccessLightFont{.fontName = kStatusSuccessLightFontName};
            GuiFontCache statusWarningFont{.fontName = kStatusWarningFontName};
            GuiFontCache statusWarningLightFont{.fontName = kStatusWarningLightFontName};
            GuiFontCache statusErrorFont{.fontName = kStatusErrorFontName};
            GuiFontCache statusErrorLightFont{.fontName = kStatusErrorLightFontName};
            SpriteHandle windowShadow{};
            font::FontRenderer fontRenderer{};
            std::array<const TextureAtlas*, font::kFontAtlasMaximumPages> fontAtlasPages{};
            std::unordered_map<SpriteHandle, RoundedCorners, SpriteHandleHash> roundedControls{};
            // Full antialiased silhouettes for circular controls. Never scale a 6px corner to 20px+.
            std::unordered_map<SpriteHandle, SpriteHandle, SpriteHandleHash> circleControls{};
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
            std::array<const TextureAtlas*, font::kFontAtlasMaximumPages> fontAtlasPagesUploaded{};
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
            struct LineMeasurement
            {
                const char* data{};
                std::size_t bytes{};
                float width{};
                float height{};
                TextMessageRole role{TextMessageRole::neutral};
                const font::FontAsset* font{};
            };
            std::vector<LineMeasurement> measurements{};
            struct DetailMeasurement
            {
                std::size_t run{};
                std::size_t content_hash{};
                float width{};
                float height{};
                const font::FontAsset* font{};
            };
            std::vector<DetailMeasurement> detailMeasurements{};
            // Keyed by message and immutable run evidence, not a transient
            // vector index: pruning the conversation cannot expand another run.
            std::unordered_map<std::size_t, bool> expandedRuns{};
            std::unordered_map<std::size_t, bool> expandedActivityItems{};
            std::unordered_set<std::size_t> activeStreamKeys{};
            std::size_t firstLine = 0;
            std::size_t selectedLine = 0;
            std::size_t selectionAnchorLine = 0;
            bool hasSelection = false;
            std::size_t lastLineCount = 0;
            bool followTail = true;
            float scrollY = 0.0f;
            float lastContentPixelHeight = 0.0f;
            float lastViewportHeight = 0.0f;
            std::uint64_t scrollToEndGeneration = 0u;
            bool draggingScrollbar = false;
            bool draggingSelection = false;
            float dragGrabOffset = 0.0f;
            bool contextMenuOpen = false;
            Vec2 contextMenuPos{};
            std::size_t contextLineIndex{};
            std::string contextLineText{};
        };

        struct ScrollAreaState
        {
            float scrollY = 0.0f;
            float contentHeight = 0.0f;
            float viewportHeight = 0.0f;
            bool followTail = false;
            bool draggingScrollbar = false;
            float dragGrabOffset = 0.0f;
        };

        struct SelectBoxState
        {
            const core::Context* owner = nullptr;
            bool open = false;
            bool alignSelectedOnOpen = false;
            bool visited_this_frame = false;
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
        struct ComposerMenuState final
        {
            bool open{};
            std::optional<std::size_t> group{};
        };
        static thread_local std::unordered_map<std::string, ComposerMenuState> g_consoleComposerMenus{};
        // A right-click starts a new menu transaction before any old menu paints.
        // Reuse the owning control states; the first hovered opener consumes it.
        static void retire_context_menus_for_right_press() noexcept
        {
            for (auto& [key, state] : g_editBoxMenuStates) state.open = false;
            for (auto& [key, state] : g_scrollTextStates) state.contextMenuOpen = false;
            for (auto& [key, state] : g_sourceEditorStates) state.contextMenuOpen = false;
            for (auto& [key, state] : g_assetGridContextMenuStates) state.open = false;
            for (auto& [key, state] : g_consoleComposerMenus) state.open = false;
        }

        struct ComposerFilePreview final { std::string name{}, content{}; };
        static thread_local std::unordered_map<std::string, ComposerFilePreview> g_consoleFilePreviews{};
        static thread_local std::vector<ScrollAreaFrame> g_scrollAreaStack{};
        static thread_local std::vector<bool> g_floatingWindowTopLayerStack{};
        static thread_local std::vector<bool> g_floatingWindowPointerCoverStack{};
        static thread_local std::vector<WindowFrameState> g_windowFrameStack{};

        [[nodiscard]] static bool any_select_box_open(const core::Context* owner) noexcept
        {
            for (const auto& [key, state] : g_selectBoxStates)
            {
                (void)key;
                if (state.owner == owner && state.open)
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
            bool pointerInputAllowed = true;
            bool justReleased = false;
            bool rightJustReleased = false;
            bool insideWindow = false;
            bool justPressed = false;
            bool rightJustPressed = false;
            bool mousePressConsumed = false;
            bool mouseReleaseConsumed = false;
            bool rightPressConsumed = false;
            bool rightReleaseConsumed = false;
            // Popups are painted last, but must own input from the *start* of
            // the frame. Otherwise controls behind a dropdown handle its click.
            bool selectPopupCapturedThisFrame = false;
            bool selectPopupOwnsInput = false;
            bool menuPopupBlocksBackground = false;
            bool menuPopupOwnsInput = false;
            int mouseWheelDelta = 0;
            gui_lib::input::ModalInputArbiter modalInput{};
            // Raw key/text/wheel events must not reach covered editor fields.
            std::vector<InputEvent> modalHeldEvents{};
            int modalHeldWheel{};
            bool modalInputSuspended{};
            bool modalOwnerDrawn{};
            bool modalOwnerDrawing{};

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

        // Native focus loss is cancellation, not a button release. In
        // particular, it must not activate a button when the user switches apps.
        [[nodiscard]] static bool filter_frame_input(
            bool keyboardAllowed, bool pointerAllowed, bool wheelAllowed = false) noexcept
        {
            bool cancelled = false;
            for (std::size_t index = g_frame.events.size(); index > 0; --index)
            {
                if (g_frame.events[index - 1].type != EventType::FocusLost)
                    continue;
                // A shell drop is an explicit transfer even while Explorer owns
                // focus. Focus loss cancels keys/clicks, not that transfer.
                const auto boundary = g_frame.events.begin() + index;
                g_frame.events.erase(std::remove_if(g_frame.events.begin(), boundary,
                    [](const InputEvent& event) { return event.type != EventType::FileDrop; }), boundary);
                cancelled = true;
                break;
            }
            if (!keyboardAllowed)
                std::erase_if(g_frame.events, [wheelAllowed](const InputEvent& event)
                {
                    return event.type != EventType::FileDrop
                        && (!wheelAllowed || event.type != EventType::MouseWheel);
                });
            else if (!pointerAllowed)
                std::erase_if(g_frame.events, [wheelAllowed](const InputEvent& event)
                {
                    return event.type == EventType::MouseMove
                        || event.type == EventType::MouseDown
                        || event.type == EventType::MouseUp
                        || (event.type == EventType::MouseWheel && !wheelAllowed);
                });
            g_frame.pointerInputAllowed = pointerAllowed || wheelAllowed;
            return cancelled || !pointerAllowed;
        }

        [[nodiscard]] static bool complete_palette(
            const GuiResources::PaletteSprites& palette) noexcept
        {
            return palette.windowBackground.is_valid()
                && palette.buttonNormal.is_valid()
                && palette.buttonHover.is_valid()
                && palette.buttonActive.is_valid()
                && palette.textField.is_valid()
                && palette.textFieldActive.is_valid()
                && palette.panelBackground.is_valid()
                && palette.consoleBackground.is_valid()
                && palette.titleBar.is_valid()
                && palette.modalScrim.is_valid();
        }

        [[nodiscard]] static const GuiResources::PaletteSprites& active_palette() noexcept
        {
            const GuiResources::PaletteSprites* candidate = &g_resources.defaultDark;
            switch (g_frame.activeTheme)
            {
            case ThemeVariant::DefaultLight:
                candidate = &g_resources.defaultLight;
                break;
            case ThemeVariant::ClassicLauncher:
                candidate = &g_resources.classicLauncher;
                break;
            case ThemeVariant::MidnightBlue:
                candidate = &g_resources.midnightBlue;
                break;
            case ThemeVariant::EmberForge:
                candidate = &g_resources.emberForge;
                break;
            case ThemeVariant::ForestTerminal:
                candidate = &g_resources.forestTerminal;
                break;
            case ThemeVariant::AuroraSteel:
                candidate = &g_resources.auroraSteel;
                break;
            case ThemeVariant::CodexBlack:
                candidate = &g_resources.codexBlack;
                break;
            case ThemeVariant::StudioNavy: // Legacy saved theme resolves to the updated Midnight Blue.
                candidate = &g_resources.midnightBlue;
                break;
            case ThemeVariant::DefaultDark:
            default:
                candidate = &g_resources.defaultDark;
                break;
            }
            // A newly authored palette can compile while forgetting to create
            // its atlas sprites. Never render transparent control surfaces.
            return complete_palette(*candidate) ? *candidate : g_resources.defaultDark;
        }

        [[nodiscard]] static bool active_palette_is_light() noexcept
        {
            return active_palette().isLight;
        }

        static_assert(gui_lib::semantic_tone_count
            == static_cast<std::size_t>(SemanticTone::engine) + 1u);

        [[nodiscard]] static const GuiResources::SemanticSpritePair& active_semantic_pair(
            SemanticTone tone) noexcept
        {
            const auto index = static_cast<std::size_t>(tone);
            const auto safeIndex = index < gui_lib::semantic_tone_count ? index : 0u;
            return active_palette_is_light()
                ? g_resources.semanticLight[safeIndex]
                : g_resources.semanticDark[safeIndex];
        }

        [[nodiscard]] static SemanticTone semantic_tone_for_message_role(
            TextMessageRole role) noexcept
        {
            switch (role)
            {
            case TextMessageRole::user: return SemanticTone::info;
            case TextMessageRole::assistant: return SemanticTone::assistant;
            case TextMessageRole::system: return SemanticTone::muted;
            case TextMessageRole::info: return SemanticTone::info;
            case TextMessageRole::success: return SemanticTone::success;
            case TextMessageRole::warning: return SemanticTone::warning;
            case TextMessageRole::error: return SemanticTone::error;
            case TextMessageRole::engine: return SemanticTone::engine;
            case TextMessageRole::neutral:
            default: return SemanticTone::neutral;
            }
        }

        // Used only while drawing the editor placeholder. Font alpha is
        // baked by Epoch's font atlas; typed text retains full opacity.
        const GuiFontCache* g_temporaryTextFont{};
        [[nodiscard]] static const GuiFontCache& active_font_cache() noexcept
        {
            if (g_temporaryTextFont && g_temporaryTextFont->asset)
                return *g_temporaryTextFont;
            if (active_palette_is_light()
                && g_resources.darkTextFont.asset)
                return g_resources.darkTextFont;
            return g_resources.font;
        }

        // The OS chrome policy is independent of the current per-widget theme stack.
        // Default to the OS setting before editor preferences are loaded.
        std::atomic<ThemePreference> g_nativeWindowTheme{ThemePreference::FollowSystemDark};
        std::atomic<ThemeVariant> g_publishedWindowVariant{ThemeVariant::DefaultDark};
        std::atomic<bool> g_cycleWindowDark{true};

        [[nodiscard]] static bool system_prefers_dark_palette() noexcept
        {
#if defined(_WIN32)
            // Windows broadcasts theme changes, but the host and renderer have
            // separate threads. A short per-thread cache avoids a registry read
            // on every rendered frame, including high-refresh editors.
            using Clock = std::chrono::steady_clock;
            static thread_local Clock::time_point lastCheck{};
            static thread_local bool lastDark{};
            const auto now = Clock::now();
            if (lastCheck != Clock::time_point{}
                && now - lastCheck < std::chrono::milliseconds{400})
                return lastDark;
            lastCheck = now;

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
            // Windows' default app mode is light when the optional key is absent.
            lastDark = status == ERROR_SUCCESS && appsUseLightTheme == 0;
            return lastDark;
#else
            return true;
#endif
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
            return g_frame.insideWindow;
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
        static void register_rounded_control(
            TextureAtlas& atlas,
            SpriteHandle fill,
            std::string_view name,
            Color color)
        {
            // Draw the inverse of a circle into each corner's alpha channel.
            // The old path rasterized an eight-segment, six-pixel polygon and
            // stretched it to the composer's 20px radius. That is visibly jagged.
            // Source corners are larger than every current chat radius; the
            // atlas sampler downsizes a smooth analytic curve instead.
            constexpr std::uint32_t radius = 48U;
            constexpr auto kTL = gui_lib::rounded_corner_raster::Corner::top_left;
            constexpr auto kTR = gui_lib::rounded_corner_raster::Corner::top_right;
            constexpr auto kBR = gui_lib::rounded_corner_raster::Corner::bottom_right;
            constexpr auto kBL = gui_lib::rounded_corner_raster::Corner::bottom_left;
            const std::string base{ name };
            const auto makeCorner = [&](gui_lib::rounded_corner_raster::Corner corner)
            {
                return gui_lib::rounded_corner_raster::rgba(
                    radius, corner, color.r, color.g, color.b, color.a);
            };
            RoundedCorners corners{
                .topLeft = add_sprite(atlas, base + "/top_left",
                    makeCorner(kTL), radius, radius),
                .topRight = add_sprite(atlas, base + "/top_right",
                    makeCorner(kTR), radius, radius),
                .bottomRight = add_sprite(atlas, base + "/bottom_right",
                    makeCorner(kBR), radius, radius),
                .bottomLeft = add_sprite(atlas, base + "/bottom_left",
                    makeCorner(kBL), radius, radius),
                .sourceRadius = static_cast<float>(radius)
            };
            if (fill.is_valid() && corners.valid())
                g_resources.roundedControls.insert_or_assign(fill, std::move(corners));
            if (fill.is_valid())
            {
                constexpr std::uint32_t kCircleTextureSize = 32U;
                const auto sprite = add_sprite(atlas, base + "/circle",
                    gui_lib::circle_raster::rgba(kCircleTextureSize,
                        color.r, color.g, color.b, color.a),
                    kCircleTextureSize, kCircleTextureSize);
                if (sprite.is_valid())
                    g_resources.circleControls.insert_or_assign(fill, sprite);
            }
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

#if defined(_WIN32)
            // Prefer the modern Windows UI typeface without bundling a system
            // font. User EPOCH_GUI_FONT_PATH always takes precedence; Linux
            // and portable builds still use the bundled Roboto fallback.
            {
                wchar_t windowsDirectory[MAX_PATH]{};
                const auto count = ::GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
                if (count > 0u && count < MAX_PATH)
                {
                    const auto candidate = std::filesystem::path{windowsDirectory}
                        / L"Fonts" / L"segoeui.ttf";
                    std::error_code ec;
                    if (std::filesystem::is_regular_file(candidate, ec))
                        return candidate;
                }
            }
#endif
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
            if (g_resources.font.asset
                && g_resources.darkTextFont.asset
                && g_resources.halfTextFont.asset
                && g_resources.halfTextLightFont.asset
                && g_resources.textShadowFont.asset
                && g_resources.statusInfoFont.asset
                && g_resources.statusSuccessFont.asset
                && g_resources.statusWarningFont.asset
                && g_resources.statusErrorFont.asset
                && g_resources.statusInfoLightFont.asset
                && g_resources.statusSuccessLightFont.asset
                && g_resources.statusWarningLightFont.asset
                && g_resources.statusErrorLightFont.asset)
                return;

            // Probe missing/file-change evidence at most every five seconds.
            // A failed registration must not rebake all glyphs/kerning on this
            // timer: a full atlas cannot recover merely because time elapsed.
            using FontClock = std::chrono::steady_clock;
            static FontClock::time_point nextFontAttempt{}; // protected by g_resourceMutex
            const auto now = FontClock::now();
            if (now < nextFontAttempt)
                return;
            nextFontAttempt = now + std::chrono::seconds{5};

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

            static std::filesystem::path attemptedFontPath{};
            static std::filesystem::file_time_type attemptedFontWrite{};
            static std::uintmax_t attemptedFontSize{};
            std::error_code fontEvidenceError;
            const auto fontWrite = std::filesystem::last_write_time(fontPath, fontEvidenceError);
            const auto fontSize = fontEvidenceError ? 0u : std::filesystem::file_size(fontPath, fontEvidenceError);
            if (!fontEvidenceError && (fontPath != attemptedFontPath
                || fontWrite != attemptedFontWrite || fontSize != attemptedFontSize))
            {
                attemptedFontPath = fontPath;
                attemptedFontWrite = fontWrite;
                attemptedFontSize = fontSize;
                for (auto* cache : {&g_resources.font, &g_resources.darkTextFont,
                    &g_resources.halfTextFont, &g_resources.halfTextLightFont,
                    &g_resources.textShadowFont, &g_resources.statusInfoFont,
                    &g_resources.statusInfoLightFont, &g_resources.statusSuccessFont,
                    &g_resources.statusSuccessLightFont, &g_resources.statusWarningFont,
                    &g_resources.statusWarningLightFont, &g_resources.statusErrorFont,
                    &g_resources.statusErrorLightFont})
                    cache->loadFailed = false;
                g_failedFontLoadWarningLogged = false;
                g_missingFontAssetWarningLogged = false;
            }

            const auto loadCache = [&](GuiFontCache& fontCache, font::FontColor color)
                {
                    if (fontCache.asset)
                        return true;
                    if (fontCache.loadFailed)
                        return false;

                    fontCache.asset = g_resources.fontRenderer.get_font(fontCache.fontName);
                    if (!fontCache.asset
                        && !g_resources.fontRenderer.load_font(
                            fontCache.fontName,
                            fontPath.string(),
                            fontCache.fontSizePt,
                            color))
                    {
                        fontCache.loadFailed = true;
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
                        fontCache.asset = nullptr;
                        fontCache.loadFailed = true;
                        logger::error("Epoch.GUI", "Font asset refers to an unavailable atlas page.");
                        return false;
                    }
                    if (std::ranges::find(g_resources.fontAtlasPages, fontCache.atlas)
                        == g_resources.fontAtlasPages.end())
                    {
                        const auto slot = std::ranges::find(g_resources.fontAtlasPages, nullptr);
                        if (slot == g_resources.fontAtlasPages.end())
                        {
                            fontCache.asset = nullptr;
                            fontCache.loadFailed = true;
                            logger::error("Epoch.GUI", "GUI font atlas page budget is exhausted.");
                            return false;
                        }
                        *slot = fontCache.atlas;
                    }
                    return true;
                };

            const bool lightTextLoaded = loadCache(
                g_resources.font,
                font::FontColor{255, 255, 255, 255});
            const bool darkTextLoaded = loadCache(
                g_resources.darkTextFont,
                font::FontColor{25, 39, 58, 255});
            (void)loadCache(g_resources.halfTextFont,
                font::FontColor{255, 255, 255, 128});
            (void)loadCache(g_resources.halfTextLightFont,
                font::FontColor{25, 39, 58, 128});
            const bool shadowTextLoaded = loadCache(
                g_resources.textShadowFont,
                font::FontColor{0, 0, 0, 112});
            // Status emphasis is optional: a failed semantic atlas never
            // makes the editor's normal font fail to load.
            (void)loadCache(g_resources.statusInfoFont,
                font::FontColor{93, 205, 240, 255});
            (void)loadCache(g_resources.statusSuccessFont,
                font::FontColor{132, 222, 170, 255});
            (void)loadCache(g_resources.statusWarningFont,
                font::FontColor{245, 195, 100, 255});
            (void)loadCache(g_resources.statusErrorFont,
                font::FontColor{255, 127, 127, 255});
            // Light theme uses distinct, ink-dark semantic text. Pale neon
            // colors on its bright surfaces do not meet readable contrast.
            (void)loadCache(g_resources.statusInfoLightFont,
                font::FontColor{25, 84, 135, 255});
            (void)loadCache(g_resources.statusSuccessLightFont,
                font::FontColor{25, 107, 64, 255});
            (void)loadCache(g_resources.statusWarningLightFont,
                font::FontColor{133, 82, 18, 255});
            (void)loadCache(g_resources.statusErrorLightFont,
                font::FontColor{171, 49, 62, 255});
            if (!lightTextLoaded || !darkTextLoaded || !shadowTextLoaded)
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
                if ((!g_resources.font.asset
                        || !g_resources.darkTextFont.asset
                        || !g_resources.textShadowFont.asset)
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
                        // Rounded surface registration now includes window,
                        // panel, and console sprites for every palette. With
                        // semantic tones and 48px corner masks, the previous
                        // 1024x1024 atlas overflowed before the first frame.
                        .width = 2048,
                        .height = 2048,
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
                g_resources.windowShadow = add_sprite(atlas, "__agui/window_shadow",
                    make_solid_pixels(0x00, 0x00, 0x00, 0x58, 8, 8), 8, 8);

                // Every theme is sourced from one palette specification. Both
                // square and rounded atlas controls use identical RGB values.
                const auto registerPalette = [&atlas](
                    GuiResources::PaletteSprites& target,
                    const theme_detail::Palette& spec)
                {
                    const auto surface = [&atlas, &spec](
                        std::string_view suffix, theme_detail::Rgb rgba)
                    {
                        return add_sprite(atlas,
                            std::string{spec.atlasPrefix} + "/" + std::string{suffix},
                            make_solid_pixels(rgba.r, rgba.g, rgba.b, rgba.a, 8, 8), 8, 8);
                    };
                    target.windowBackground = surface("window_bg", spec.window);
                    target.buttonNormal = surface("button_normal", spec.button);
                    target.buttonHover = surface("button_hover", spec.hover);
                    target.buttonActive = surface("button_active", spec.active);
                    target.textField = surface("text_field", spec.input);
                    target.textFieldActive = surface("text_field_active", spec.inputActive);
                    target.panelBackground = surface("panel_bg", spec.panel);
                    target.consoleBackground = surface("console_bg", spec.console);
                    target.titleBar = surface("title_bar", spec.title);
                    target.modalScrim = surface("modal_scrim", spec.scrim);
                    target.isLight = spec.isLight;

                    const auto rounded = [&atlas, &spec](
                        SpriteHandle sprite, std::string_view name, theme_detail::Rgb color)
                    {
                        register_rounded_control(atlas, sprite,
                            std::string{spec.roundedPrefix} + "/" + std::string{name},
                            {color.r, color.g, color.b, color.a});
                    };
                    rounded(target.buttonNormal, "button_normal", spec.button);
                    rounded(target.buttonHover, "button_hover", spec.hover);
                    rounded(target.buttonActive, "button_active", spec.active);
                    rounded(target.textField, "text_field", spec.input);
                    rounded(target.textFieldActive, "text_field_active", spec.inputActive);
                    // Expanded Activity cards and context-menu surfaces must
                    // have registered corner sprites, or requested radii render square.
                    rounded(target.windowBackground, "window_bg", spec.window);
                    rounded(target.panelBackground, "panel_bg", spec.panel);
                    rounded(target.consoleBackground, "console_bg", spec.console);
                };
                registerPalette(g_resources.defaultDark, theme_detail::palette_at(0));
                registerPalette(g_resources.defaultLight, theme_detail::palette_at(1));
                registerPalette(g_resources.classicLauncher, theme_detail::palette_at(2));
                registerPalette(g_resources.midnightBlue, theme_detail::palette_at(3));
                registerPalette(g_resources.emberForge, theme_detail::palette_at(4));
                registerPalette(g_resources.forestTerminal, theme_detail::palette_at(5));
                registerPalette(g_resources.auroraSteel, theme_detail::palette_at(6));
                registerPalette(g_resources.codexBlack, theme_detail::palette_at(7));

                for (std::size_t index = 0u;
                     index < gui_lib::semantic_tone_count;
                     ++index)
                {
                    const auto tone = static_cast<gui_lib::SemanticTone>(index);
                    const auto dark = gui_lib::semantic_colors(tone, false);
                    const auto light = gui_lib::semantic_colors(tone, true);
                    const std::string name{gui_lib::semantic_tone_name(tone)};
                    g_resources.semanticDark[index].background = add_sprite(
                        atlas,
                        "__agui_semantic/dark/" + name + "/background",
                        make_solid_pixels(
                            dark.background.r, dark.background.g, dark.background.b,
                            dark.background.a, 8, 8),
                        8, 8);
                    g_resources.semanticDark[index].accent = add_sprite(
                        atlas,
                        "__agui_semantic/dark/" + name + "/accent",
                        make_solid_pixels(
                            dark.accent.r, dark.accent.g, dark.accent.b,
                            dark.accent.a, 8, 8),
                        8, 8);
                    g_resources.semanticLight[index].background = add_sprite(
                        atlas,
                        "__agui_semantic/light/" + name + "/background",
                        make_solid_pixels(
                            light.background.r, light.background.g, light.background.b,
                            light.background.a, 8, 8),
                        8, 8);
                    g_resources.semanticLight[index].accent = add_sprite(
                        atlas,
                        "__agui_semantic/light/" + name + "/accent",
                        make_solid_pixels(
                            light.accent.r, light.accent.g, light.accent.b,
                            light.accent.a, 8, 8),
                        8, 8);
                    register_rounded_control(atlas, g_resources.semanticDark[index].background,
                        "__agui_round/semantic_dark/" + name,
                        {dark.background.r, dark.background.g, dark.background.b, dark.background.a});
                    register_rounded_control(atlas, g_resources.semanticLight[index].background,
                        "__agui_round/semantic_light/" + name,
                        {light.background.r, light.background.g, light.background.b, light.background.a});
                    register_rounded_control(atlas, g_resources.semanticDark[index].accent,
                        "__agui_round/semantic_dark_accent/" + name,
                        {dark.accent.r, dark.accent.g, dark.accent.b, dark.accent.a});
                    register_rounded_control(atlas, g_resources.semanticLight[index].accent,
                        "__agui_round/semantic_light_accent/" + name,
                        {light.accent.r, light.accent.g, light.accent.b, light.accent.a});
                }

                g_resources.atlasBuilt = true;
            }

            ensure_font_loaded_locked();
        }

        [[nodiscard]] static std::array<const TextureAtlas*, font::kFontAtlasMaximumPages> font_atlas_page_snapshot()
        {
            std::scoped_lock lock(g_resourceMutex);
            return g_resources.fontAtlasPages;
        }

        static void perform_backend_upload(Context& ctx)
        {
            ensure_resources();
            const auto fontPages = font_atlas_page_snapshot();

            std::scoped_lock lock(g_uploadMutex);
            auto& state = g_uploadedContexts[&ctx];

            if (!state.guiAtlasUploaded && g_resources.atlas)
            {
                const std::uint32_t handle = ctx.add_atlas_safe(*g_resources.atlas);
                if (handle == 0)
                    throw std::runtime_error("[agui] Failed to upload GUI atlas for this context");
                state.guiAtlasUploaded = true;
            }

            for (std::size_t page = 0u; page < fontPages.size(); ++page)
            {
                if (fontPages[page] && state.fontAtlasPagesUploaded[page] != fontPages[page])
                {
                    const std::uint32_t handle = ctx.add_atlas_safe(*fontPages[page]);
                    if (handle != 0)
                        state.fontAtlasPagesUploaded[page] = fontPages[page];
                }
            }
        }

        [[nodiscard]] static bool backend_upload_complete(
            const UploadState& state,
            const std::array<const TextureAtlas*, font::kFontAtlasMaximumPages>& fontPages) noexcept
        {
            if (g_resources.atlas && !state.guiAtlasUploaded) return false;
            for (std::size_t page = 0u; page < fontPages.size(); ++page)
                if (fontPages[page] && state.fontAtlasPagesUploaded[page] != fontPages[page])
                    return false;
            return true;
        }

        static void ensure_backend_upload(Context& ctx)
        {
            ensure_resources();
            const auto fontPages = font_atlas_page_snapshot();

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
                    if (backend_upload_complete(state, fontPages) || state.uploadQueued)
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
            auto style = gui_lib::rounded_rect::normalize_rounded_rect_style(requestedStyle);
            // A per-widget radius must never override the editor-level
            // square-controls preference. Geometry/hit testing are unchanged.
            style.enabled = style.enabled && g_frame.roundedRectStyle.enabled;
            // A circular square control uses an independent high-resolution
            // silhouette; the nine-slice path is reserved for rounded rectangles.
            if (style.enabled && w >= 8.0f && h >= 8.0f
                && std::abs(w - h) < 0.5f
                && style.control_radius >= (std::min)(w, h) * 0.5f - 0.01f)
            {
                const auto it = g_resources.circleControls.find(handle);
                if (it != g_resources.circleControls.end())
                {
                    draw_sprite_raw(it->second, x, y, w, h);
                    return;
                }
            }
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

            // Pixel-snap a contiguous nine-slice. The previous 1px overlap
            // drew solid fill UNDER the corners' transparent antialiased edge;
            // at fractional DPI this leaks bright/square artifacts at corners.
            // Adjacent snapped edges meet without a gap or an underlay.
            const float left = std::round(x);
            const float top = std::round(y);
            const float right = std::round(x + w);
            const float bottom = std::round(y + h);
            const float cornerRadius = (std::min)(std::round(radius),
                (std::min)(right - left, bottom - top) * 0.5f);
            if (cornerRadius < 1.0f) return;
            const float middleWidth = (std::max)(0.0f, right - left - 2.0f * cornerRadius);
            const float middleHeight = (std::max)(0.0f, bottom - top - 2.0f * cornerRadius);
            if (middleWidth > 0.0f)
                draw_sprite_raw(handle, left + cornerRadius, top,
                    middleWidth, bottom - top);
            if (middleHeight > 0.0f)
            {
                draw_sprite_raw(handle, left, top + cornerRadius,
                    cornerRadius, middleHeight);
                draw_sprite_raw(handle, right - cornerRadius, top + cornerRadius,
                    cornerRadius, middleHeight);
            }
            draw_sprite_raw(corners.topLeft, left, top,
                cornerRadius, cornerRadius);
            draw_sprite_raw(corners.topRight, right - cornerRadius, top,
                cornerRadius, cornerRadius);
            draw_sprite_raw(corners.bottomRight, right - cornerRadius,
                bottom - cornerRadius, cornerRadius, cornerRadius);
            draw_sprite_raw(corners.bottomLeft, left, bottom - cornerRadius,
                cornerRadius, cornerRadius);
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
            return g_frame.pointerInputAllowed
                && popup_input::allows_pointer({
                    .selection_open_this_frame = g_frame.selectPopupCapturedThisFrame,
                    .current_selection_owner = g_frame.selectPopupOwnsInput,
                    .application_menu_open_this_frame = g_frame.menuPopupBlocksBackground,
                    .application_menu_owner = g_frame.menuPopupOwnsInput })
                && g_frame.modalInput.pointer_allowed({ p.x, p.y });
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

        [[nodiscard]] static const font::Glyph* lookup_glyph_from_cache(
            const GuiFontCache& fontCache,
            char32_t codepoint) noexcept
        {
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

        [[nodiscard]] static const font::Glyph* lookup_glyph(char32_t codepoint) noexcept
        {
            return lookup_glyph_from_cache(active_font_cache(), codepoint);
        }

        struct StatusKeyword final
        {
            std::string_view word{};
            const GuiFontCache* font{};
        };

        // Only host/system activity is emphasized. Never interpret markdown or
        // allow model text to inject formatting commands or clickable links.
        [[nodiscard]] static StatusKeyword status_keyword_at(
            std::string_view text, std::size_t index) noexcept
        {
            if (index >= text.size() || text.size() > 2048u) return {};
            const auto isWord = [](char c) noexcept {
                const auto uc = static_cast<unsigned char>(c);
                return (uc >= '0' && uc <= '9') || (uc >= 'A' && uc <= 'Z')
                    || (uc >= 'a' && uc <= 'z') || uc == '_';
            };
            if (index != 0 && isWord(text[index - 1])) return {};
            const auto eq = [](char a, char b) noexcept {
                const auto lc = [](unsigned char c) noexcept {
                    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
                };
                return lc(static_cast<unsigned char>(a))
                    == lc(static_cast<unsigned char>(b));
            };
            const auto match = [&](std::string_view word) noexcept {
                if (word.size() > text.size() - index) return false;
                if (index + word.size() < text.size()
                    && isWord(text[index + word.size()])) return false;
                for (std::size_t i{}; i < word.size(); ++i)
                    if (!eq(text[index + i], word[i])) return false;
                return true;
            };
            const bool lightTheme = active_palette_is_light();
            const auto& info = lightTheme && g_resources.statusInfoLightFont.asset
                ? g_resources.statusInfoLightFont : g_resources.statusInfoFont;
            const auto& ok = lightTheme && g_resources.statusSuccessLightFont.asset
                ? g_resources.statusSuccessLightFont : g_resources.statusSuccessFont;
            const auto& caution = lightTheme && g_resources.statusWarningLightFont.asset
                ? g_resources.statusWarningLightFont : g_resources.statusWarningFont;
            const auto& error = lightTheme && g_resources.statusErrorLightFont.asset
                ? g_resources.statusErrorLightFont : g_resources.statusErrorFont;
            if (match("not approved")) return {"not approved", &caution};
            if (match("not verified")) return {"not verified", &caution};
            if (match("read unconfirmed")) return {"read unconfirmed", &caution};
            if (match("ready to chat")) return {"ready to chat", &ok};
            if (match("passed")) return {"passed", &ok};
            if (match("paused")) return {"paused", &caution};
            if (match("retained")) return {"retained", &info};
            if (match("rejected")) return {"rejected", &error};
            if (match("unavailable")) return {"unavailable", &caution};
            if (match("vision")) return {"vision", &info};
            if (match("image")) return {"image", &info};
            if (match("done")) return {"done", &ok};
            if (match("active")) return {"active", &info};
            if (match("next")) return {"next", &caution};
            if (match("engine work")) return {"engine work", &info};
            if (match("engine self-coding")) return {"engine self-coding", &info};
            if (match("engine coding")) return {"engine coding", &info};
            if (match("light model chosen")) return {"light model chosen", &ok};
            if (match("small model")) return {"small model", &ok};
            if (match("middle model")) return {"middle model", &info};
            if (match("foundation model")) return {"foundation model", &caution};
            if (match("model selected")) return {"model selected", &ok};
            if (match("system 1")) return {"system 1", &info};
            if (match("sandbox")) return {"sandbox", &info};
            if (match("candidate")) return {"candidate", &info};
            if (match("queued")) return {"queued", &caution};
            if (match("stopped")) return {"stopped", &caution};
            if (match("failed")) return {"failed", &error};
            if (match("error")) return {"error", &error};
            if (match("validated")) return {"validated", &ok};
            if (match("completed")) return {"completed", &ok};
            if (match("approved")) return {"approved", &ok};
            if (match("approval")) return {"approval", &caution};
            return {};
        }

        [[nodiscard]] static const font::Glyph* lookup_shadow_glyph(char32_t codepoint) noexcept
        {
            if (active_palette_is_light() || g_temporaryTextFont)
                return nullptr;
            return lookup_glyph_from_cache(g_resources.textShadowFont, codepoint);
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

        static float draw_wrapped_text(std::string_view text, float x, float y,
            float width, float scale, bool emphasizeStatus = false,
            bool emphasizeCode = false)
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
            std::size_t keywordEnd{};
            const GuiFontCache* keywordFont{};

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const std::size_t byteIndex = i;
                if (emphasizeStatus && byteIndex >= keywordEnd)
                {
                    const auto keyword = status_keyword_at(text, byteIndex);
                    keywordEnd = keyword.word.empty() ? byteIndex : byteIndex + keyword.word.size();
                    keywordFont = keyword.font && keyword.font->asset ? keyword.font : nullptr;
                }
                // Tool payloads retain their exact bytes and ordinary text
                // geometry. Color only literal strings, punctuation and values;
                // never interpret their contents as markup or commands.
                if (emphasizeCode && byteIndex >= keywordEnd)
                {
                    keywordFont = nullptr;
                    keywordEnd = byteIndex;
                    const char c = text[byteIndex];
                    const bool light = active_palette_is_light();
                    const auto* info = light ? &g_resources.statusInfoLightFont : &g_resources.statusInfoFont;
                    const auto* value = light ? &g_resources.statusSuccessLightFont : &g_resources.statusSuccessFont;
                    if (c == '"' || c == '\'')
                    {
                        std::size_t end = byteIndex + 1u;
                        for (; end < text.size(); ++end)
                        {
                            if (text[end] == '\\' && end + 1u < text.size()) { ++end; continue; }
                            if (text[end] == c) { ++end; break; }
                        }
                        keywordEnd = end;
                        auto next = end;
                        while (next < text.size()
                            && (text[next] == ' ' || text[next] == '\t'
                                || text[next] == '\r' || text[next] == '\n')) ++next;
                        keywordFont = next < text.size() && text[next] == ':' ? info : value;
                    }
                    else if (std::string_view{"{}[]():,;"}.find(c) != std::string_view::npos)
                    {
                        keywordEnd = byteIndex + 1u;
                        keywordFont = info;
                    }
                    else if (c >= '0' && c <= '9')
                    {
                        keywordEnd = byteIndex + 1u;
                        keywordFont = value;
                    }
                    else if ((c >= 'a' && c <= 'z') || c == '_')
                    {
                        const auto isWord = [](char ch) noexcept {
                            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                                || (ch >= '0' && ch <= '9') || ch == '_';
                        };
                        auto end = byteIndex + 1u;
                        while (end < text.size() && isWord(text[end])) ++end;
                        const auto word = text.substr(byteIndex, end - byteIndex);
                        static constexpr std::array<std::string_view, 18> words{
                            "auto", "bool", "class", "const", "constexpr", "enum",
                            "export", "false", "import", "namespace", "nullptr", "return",
                            "static", "struct", "true", "using", "void", "null"};
                        keywordEnd = end;
                        if ((byteIndex == 0u || !isWord(text[byteIndex - 1u]))
                            && std::ranges::find(words, word) != words.end())
                            keywordFont = word == "true" || word == "false" || word == "null"
                                || word == "nullptr" ? value : info;
                    }
                    if (keywordFont && !keywordFont->asset) keywordFont = nullptr;
                }
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
                    if (const auto* glyph = keywordFont && byteIndex < keywordEnd
                        ? lookup_glyph_from_cache(*keywordFont, ch) : lookup_glyph(ch))
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
                                if (const auto* shadowGlyph = lookup_shadow_glyph(ch);
                                    shadowGlyph && shadowGlyph->handle.is_valid())
                                {
                                    constexpr float shadowOffset = 1.0f;
                                    draw_sprite(
                                        shadowGlyph->handle,
                                        drawX + shadowOffset,
                                        drawY + shadowOffset,
                                        drawW,
                                        drawH);
                                }
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
                            if (const auto* shadowGlyph = g_temporaryTextFont
                                ? nullptr : lookup_shadow_glyph(ch);
                                shadowGlyph && shadowGlyph->handle.is_valid())
                            {
                                constexpr float shadowOffset = 1.0f;
                                draw_sprite(
                                    shadowGlyph->handle,
                                    drawX + shadowOffset,
                                    drawY + shadowOffset,
                                    drawW,
                                    drawH);
                            }
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

        static void draw_close_mark(Vec2 position, Vec2 size)
        {
            ensure_resources();
            if (!g_frame.ctx)
                return;

            const float scale = kFontScale * 0.65f;
            const auto* glyph = lookup_glyph(U'X');
            if (!glyph || !glyph->handle.is_valid())
                return;

            // Center the visible mark, not the font's ascent/descent line box.
            // Applying text baselines here puts a small X below its hit target.
            const float width = glyph->size_px.x * scale;
            const float height = glyph->size_px.y * scale;
            if (!glyph_rect_is_reasonable(width, height, scale))
                return;
            const float x = position.x + std::floor((size.x - width) * 0.5f);
            const float y = position.y + std::floor((size.y - height) * 0.5f);
            if (const auto* shadowGlyph = g_temporaryTextFont
                ? nullptr : lookup_shadow_glyph(U'X');
                shadowGlyph && shadowGlyph->handle.is_valid())
            {
                draw_sprite(shadowGlyph->handle, x + 1.0f, y + 1.0f, width, height);
            }
            draw_sprite(glyph->handle, x, y, width, height);
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
            g_frame.modalHeldEvents.clear();
            g_frame.modalHeldWheel = 0;
            g_frame.modalInputSuspended = false;
            g_frame.modalOwnerDrawn = false;
            g_frame.modalOwnerDrawing = false;
            g_frame.mousePressConsumed = false;
            g_frame.mouseReleaseConsumed = false;
            g_frame.rightPressConsumed = false;
            g_frame.rightReleaseConsumed = false;
            g_frame.selectPopupCapturedThisFrame = false;
            g_frame.selectPopupOwnsInput = false;
            g_frame.menuPopupBlocksBackground = false;
            g_frame.menuPopupOwnsInput = false;
            g_frame.activeTheme = ThemeVariant::DefaultDark;
            g_frame.roundedRectStyle = {};
            g_frame.themeStack.clear();
            g_frame.roundedRectStyleStack.clear();
            g_frame.pendingSelectPopups.clear();
            g_scrollAreaStack.clear();
            g_floatingWindowTopLayerStack.clear();
            g_floatingWindowPointerCoverStack.clear();
            g_windowFrameStack.clear();
        }

        static void forget_upload_state(const void* ctxKey) noexcept
        {
            if (!ctxKey)
                return;

            std::scoped_lock lock(g_uploadMutex);
            g_uploadedContexts.erase(ctxKey);
        }

        static void publish_deferred_draw_batch(
            Context* ctx,
            std::vector<QueuedSpriteDraw>& pending,
            std::unordered_map<const void*, DeferredDrawBatch, PtrHash>& batches,
            std::size_t minimumReserve) noexcept
        {
            std::shared_ptr<std::vector<QueuedSpriteDraw>> candidate;
            if (!pending.empty())
            {
                candidate = std::make_shared<std::vector<QueuedSpriteDraw>>();
                candidate->swap(pending);
                pending.reserve((std::max)(candidate->size(), minimumReserve));
            }

            for (;;)
            {
                std::shared_ptr<std::vector<QueuedSpriteDraw>> previous;
                {
                    std::scoped_lock lock(g_deferredBatchMutex);
                    previous = batches[ctx].draws;
                }

                // Published vectors are immutable. Keep the observed snapshot
                // alive while comparing without holding the renderer's lock.
                if (!candidate && previous && !previous->empty())
                    candidate = std::make_shared<std::vector<QueuedSpriteDraw>>();
                const bool unchanged = !candidate
                    || (previous && same_queued_draws(*previous, *candidate));
                {
                    std::scoped_lock lock(g_deferredBatchMutex);
                    auto& batch = batches[ctx];
                    if (batch.draws != previous)
                        continue;
                    if (!unchanged)
                    {
                        batch.draws.swap(candidate);
                        ++batch.generation;
                    }
                }
                // Both references, including retired vector storage, are
                // released only after the publication lock has been dropped.
                return;
            }
        }

        static void store_top_layer_batch(Context* ctx) noexcept
        {
            if (!ctx)
            {
                g_frame.topLayerDraws.clear();
                return;
            }

            publish_deferred_draw_batch(
                ctx, g_frame.topLayerDraws, g_topLayerDrawBatches, 256u);
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
                publish_deferred_draw_batch(
                    ctx, g_frame.queuedDraws, g_deferredDrawBatches, 4096u);
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

#include "controls/gui.frame_lifecycle.inl"
    std::span<const ThemePreferenceChoice> theme_preference_choices() noexcept
    {
        // Infer the list size from its entries. A newly registered preference
        // must not cause C2078 or silently be omitted due to a stale count.
        static constexpr ThemePreferenceChoice kChoices[]{
            { "System Light/Dark", ThemePreference::FollowSystemDark },
            { "Light", ThemePreference::Light },
            { "Dark", ThemePreference::Dark },
            { "Classic Launcher", ThemePreference::ClassicLauncher },
            { "Midnight Blue", ThemePreference::MidnightBlue },
            { "Ember Forge", ThemePreference::EmberForge },
            { "Forest Terminal", ThemePreference::ForestTerminal },
            { "Aurora Steel", ThemePreference::AuroraSteel },
            { "Codex", ThemePreference::CodexBlack },
            { "Theme Cycle", ThemePreference::ThemeCycle }
        };
        return std::span<const ThemePreferenceChoice>{kChoices};
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
        if (preference == ThemePreference::StudioNavy)
            return "Midnight Blue";
        return "System Light/Dark";
    }

    namespace
    {
        detail::ThemeCycleClock g_themeCycle{};
    }

    std::span<const ThemeCycleIntervalChoice> theme_cycle_interval_choices() noexcept
    {
        static constexpr std::array<ThemeCycleIntervalChoice, 10> kIntervals{{
            { "30 seconds", 30u },
            { "1 minute", 60u },
            { "3 minutes", 180u },
            { "5 minutes", 300u },
            { "8 minutes", 480u },
            { "10 minutes", 600u },
            { "15 minutes", 900u },
            { "20 minutes", 1200u },
            { "30 minutes", 1800u },
            { "1 hour", 3600u }
        }};
        return {kIntervals.data(), kIntervals.size()};
    }

    std::uint32_t normalize_theme_cycle_interval(std::uint32_t seconds) noexcept
    {
        for (const auto& choice : theme_cycle_interval_choices())
        {
            if (choice.seconds == seconds)
                return seconds;
        }
        return 30u;
    }

    std::string_view theme_cycle_interval_label(std::uint32_t seconds) noexcept
    {
        const auto normalized = normalize_theme_cycle_interval(seconds);
        for (const auto& choice : theme_cycle_interval_choices())
        {
            if (choice.seconds == normalized)
                return choice.label;
        }
        return "30 seconds";
    }

    void set_theme_cycle_interval(std::uint32_t seconds) noexcept
    {
        const auto normalized = normalize_theme_cycle_interval(seconds);
        g_themeCycle.set_interval(normalized);
    }

    ThemeVariant resolve_theme_preference(ThemePreference preference) noexcept
    {
        if (preference == ThemePreference::ThemeCycle)
        {
            using Clock = std::chrono::steady_clock;
            constexpr std::array<ThemeVariant, 8> kCycle{{
                ThemeVariant::DefaultDark,
                ThemeVariant::DefaultLight,
                ThemeVariant::ClassicLauncher,
                ThemeVariant::MidnightBlue,
                ThemeVariant::EmberForge,
                ThemeVariant::ForestTerminal,
                ThemeVariant::AuroraSteel,
                ThemeVariant::CodexBlack
            }};
            const auto index = static_cast<std::size_t>(
                g_themeCycle.slot(Clock::now()) % kCycle.size());
            return kCycle[index];
        }

        // Leaving cycle mode freezes the preference immediately; re-entering
        // starts again on Dark instead of advancing while the mode is disabled.
        g_themeCycle.deactivate();
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
        case ThemePreference::StudioNavy: // Legacy stored choice maps to the replacement palette.
            return ThemeVariant::MidnightBlue;
        case ThemePreference::EmberForge:
            return ThemeVariant::EmberForge;
        case ThemePreference::ForestTerminal:
            return ThemeVariant::ForestTerminal;
        case ThemePreference::AuroraSteel:
            return ThemeVariant::AuroraSteel;
        case ThemePreference::CodexBlack:
            return ThemeVariant::CodexBlack;
        case ThemePreference::RemovedLegacyTheme: // Stored ordinal 11: backward-compatible Dark migration.
            return ThemeVariant::DefaultDark;
        case ThemePreference::FollowSystemDark:
            return system_prefers_dark_palette()
                ? ThemeVariant::DefaultDark
                : ThemeVariant::DefaultLight;
        default:
            return ThemeVariant::DefaultDark;
        }
    }

    void publish_native_window_theme(ThemePreference preference) noexcept
    {
        // Only the editor render thread touches g_frame. Window threads consume
        // atomics and never access the GUI theme stack or the theme-cycle state.
        g_publishedWindowVariant.store(g_frame.activeTheme, std::memory_order_relaxed);
        if (preference == ThemePreference::ThemeCycle)
            g_cycleWindowDark.store(
                !active_palette_is_light(),
                std::memory_order_relaxed);
        g_nativeWindowTheme.store(preference, std::memory_order_release);
    }

    NativeWindowChromePalette native_window_chrome_palette() noexcept
    {
        // The window-owner thread never reads render-thread state directly.
        // System theme changes and Theme Cycle update the returned palette ID,
        // so the DWM cache correctly invalidates when the visual palette changes.
        const auto preference = g_nativeWindowTheme.load(std::memory_order_acquire);
        const auto variant = preference == ThemePreference::FollowSystemDark
            ? (system_prefers_dark_palette()
                ? ThemeVariant::DefaultDark : ThemeVariant::DefaultLight)
            : g_publishedWindowVariant.load(std::memory_order_acquire);
        std::size_t index = 0u;
        switch (variant)
        {
        case ThemeVariant::DefaultLight: index = 1u; break;
        case ThemeVariant::ClassicLauncher: index = 2u; break;
        case ThemeVariant::MidnightBlue:
        case ThemeVariant::StudioNavy: index = 3u; break;
        case ThemeVariant::EmberForge: index = 4u; break;
        case ThemeVariant::ForestTerminal: index = 5u; break;
        case ThemeVariant::AuroraSteel: index = 6u; break;
        case ThemeVariant::CodexBlack: index = 7u; break;
        default: break;
        }
        const auto& spec = theme_detail::palette_at(index);
        return NativeWindowChromePalette{
            .paletteId = static_cast<std::uint8_t>(index),
            .isLight = spec.isLight,
            .backgroundRgb = theme_detail::packed_rgb(spec.window),
            .captionRgb = theme_detail::packed_rgb(spec.title),
            .borderRgb = theme_detail::packed_rgb(spec.hover),
            .textRgb = spec.isLight ? 0x19273Au : 0xF6F7F8u
        };
    }

    bool native_window_prefers_dark() noexcept
    {
        switch (g_nativeWindowTheme.load(std::memory_order_acquire))
        {
        case ThemePreference::FollowSystemDark:
            return system_prefers_dark_palette();
        case ThemePreference::Light:
            return false;
        case ThemePreference::ThemeCycle:
            return g_cycleWindowDark.load(std::memory_order_relaxed);
        default:
            return !native_window_chrome_palette().isLight;
        }
    }

    ThemeVariant published_window_theme_variant() noexcept
    {
        // Detached panes use the canonical editor palette, not an independently
        // advancing theme cycle or an unpersisted local editor-state default.
        return g_publishedWindowVariant.load(std::memory_order_acquire);
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

#include "controls/gui.window_controls.inl"
#include "controls/gui.buttons_images.inl"
#include "controls/gui.node_surface_controls.inl"
#include "controls/gui.text_controls.inl"
#include "controls/gui.navigation_chrome.inl"
#include "controls/gui.scroll_controls.inl"
#include "controls/gui.assistant_console.inl"
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
