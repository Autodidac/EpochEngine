"""Run the shared surface paint owners with a recording sprite consumer."""
import hashlib
import json
from pathlib import Path
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]


class GuiWindowShadow(unittest.TestCase):
    def test_shadow_survives_content_clip_and_precedes_the_surface(self):
        gui = (ROOT / 'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        windows = (ROOT / 'Engine/src/epochgui/controls/gui.window_controls.inl').read_text(encoding='utf-8')
        constants = '\n'.join(line for line in gui.splitlines()
                              if 'constexpr std::uint32_t kWindowShadow' in line)
        types = '\n'.join(definition(gui, '        struct ' + name, True)
                          for name in ('QueuedSpriteDraw', 'WindowFrameState', 'PendingSelectPopup'))
        paint = '\n'.join(definition(gui, marker) for marker in (
            '        static void register_window_shadow(',
            '        [[nodiscard]] static bool has_content_clip(',
            '        static void draw_sprite_raw(',
            '        static void draw_window_shadow('))
        begin_window = '\n'.join(definition(windows, marker) for marker in (
            '    static void begin_window_impl(',
            '    void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background, bool show_title_bar)',
            '    void begin_overlay_window('))
        caption = '\n'.join(definition(gui, marker) for marker in (
            '        [[nodiscard]] static float centered_text_top(',
            '        [[nodiscard]] static const GuiFontCache* caption_text_font(',
            '        static void draw_window_caption('))
        popup = definition(windows, '    static void render_select_popup(')
        chrome = definition(windows, '    static void paint_floating_window_chrome(')
        pending = '\n'.join(definition(windows, marker) for marker in (
            '    void begin_top_layer()', '    void end_top_layer()',
            '    static void render_pending_select_popups()'))
        clear_clip = definition(gui, '        struct ContentClipClearScope', True)
        fixture = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#define CHECK(value) do { if (!(value)) std::exit(__LINE__); } while (false)
struct Vec2 { float x{}, y{}; };
struct SpriteHandle { std::size_t id{}; bool is_valid() const { return id != 0; } };
struct TextureAtlas {};
WINDOW_TYPES
SHADOW_CONSTANTS

struct Sprite { std::uint32_t width{}, height{}; std::vector<std::uint8_t> rgba; };
std::vector<Sprite> sprites(1);
SpriteHandle add_sprite(TextureAtlas&, const std::string&, const std::vector<std::uint8_t>& pixels,
    std::uint32_t width, std::uint32_t height)
{
    sprites.push_back({width, height, pixels});
    return {sprites.size() - 1};
}
struct GuiFontCache { bool asset{}; };
const GuiFontCache* g_temporaryTextFont{};
struct Resources { std::array<std::array<SpriteHandle, 3>, 3> windowShadow{}; GuiFontCache font; } g_resources;
namespace epochengine::atlasmanager {
    std::vector<const TextureAtlas*> get_atlas_vector_snapshot() { return {}; }
}
struct Context {
    bool windowData{true};
    void draw_sprite_safe(SpriteHandle, std::span<const TextureAtlas* const>, float, float, float, float, bool)
    { std::exit(201); } // The fixture must exercise the queued path.
};
struct Frame {
    Context* ctx{};
    bool ctxShared{true}, insideWindow{true};
    int topLayerDepth{};
    Vec2 cursor{}, origin{}, windowSize{}, contentMin{}, contentMax{};
    std::string windowKey;
    std::uint64_t widgetSerial{};
    std::vector<QueuedSpriteDraw> queuedDraws, topLayerDraws;
    std::vector<PendingSelectPopup> pendingSelectPopups;
    bool tracePopupPublication{};
} g_frame;
struct ScrollAreaState { float contentHeight{}, scrollY{}; };
std::unordered_map<std::string, ScrollAreaState> g_scrollAreaStates;
std::vector<int> g_scrollAreaStack;
std::vector<bool> g_floatingWindowTopLayerStack;
std::vector<WindowFrameState> g_windowFrameStack;
constexpr float kMinQueuedSpriteExtent = 0.5f, kMaxQueuedSpriteExtent = 65536.0f;
constexpr float kFontScale = 1.0f, kTitleScale = 1.0f, kTitleBarPadding = 2.0f, kContentPadding = 4.0f;
struct Palette {
    SpriteHandle windowBackground{100}, titleBar{101}, consoleBackground{102}, overlayBackground{103},
        buttonActive{103}, buttonNormal{104}, textFieldActive{105}, textField{106}, buttonHover{107};
    bool captionUsesLightText{};
} palette;
const Palette& active_palette() { return palette; }
void ensure_resources() {}
void set_cursor(Vec2 position) { g_frame.cursor = position; }
float line_advance_amount(float) { return 16.0f; }
float base_line_height(float) { return g_temporaryTextFont ? 14.0f : 12.0f; }
float baseline_offset(float) { return g_temporaryTextFont ? 10.5f : 9.0f; }
float captionY{};
void draw_text_line(std::string_view, float, float y, float scale) { captionY = y + baseline_offset(scale); }
std::string fit_text_to_width(std::string_view text, float, float) { return std::string{text}; }
ACTUAL_CAPTION
struct ContentClipScope {
    Vec2 oldMin = g_frame.contentMin, oldMax = g_frame.contentMax;
    ContentClipScope(Vec2 min, Vec2 max) { g_frame.contentMin = min; g_frame.contentMax = max; }
    ~ContentClipScope() { g_frame.contentMin = oldMin; g_frame.contentMax = oldMax; }
};
namespace epochengine {template<class... T> std::string format_text(const char*,T...){return {};}}
namespace logger {unsigned calls{};void info(const char*,const std::string&){++calls;}}
WINDOW_PAINT
void draw_sprite(SpriteHandle handle, float x, float y, float w, float h)
{ draw_sprite_raw(handle, x, y, w, h); }
BEGIN_WINDOW
POPUP_PAINT
CLEAR_CLIP
PENDING_POPUP
struct FloatingWindowState{bool dragging{},resizing{},close_pressed{};};
struct FloatingWindowOptions{bool draw_background{true},closable{},resizable{};};
struct Rect{Vec2 position,size;};
struct FloatingWindowResult{Rect window,close_button,resize_handle;bool close_hovered{},resize_hovered{};};
void draw_close_mark(Vec2,Vec2,bool,bool,bool){}
FLOATING_CHROME

int shadow_alpha_at(float x, float y)
{
    int alpha = 0;
    unsigned hits = 0;
    for (const auto& draw : g_frame.topLayerDraws)
    {
        if (draw.handle.id >= sprites.size() || x < draw.x || x >= draw.x + draw.w
            || y < draw.y || y >= draw.y + draw.h)
            continue;
        ++hits;
        const auto& texture = sprites[draw.handle.id];
        const auto px = static_cast<std::size_t>((x - draw.x) / draw.w * texture.width);
        const auto py = static_cast<std::size_t>((y - draw.y) / draw.h * texture.height);
        const auto pixel = (py * texture.width + px) * 4u;
        CHECK(pixel + 3u < texture.rgba.size());
        CHECK(texture.rgba[pixel] == 0 && texture.rgba[pixel + 1u] == 0 && texture.rgba[pixel + 2u] == 0);
        alpha += texture.rgba[pixel + 3u];
    }
    CHECK(hits <= 1); // No dark seams from overlapping slices.
    return alpha;
}

int main()
{
    TextureAtlas atlas;
    register_window_shadow(atlas);
    Context context;
    g_frame.ctx = &context;
    g_frame.contentMax = {240.0f, 180.0f};

    // An ordinary docked pane must not darken its neighbors.
    begin_window({}, {40, 30}, {80, 50}, true, false);
    CHECK(g_frame.topLayerDraws.empty());
    CHECK(g_frame.queuedDraws.size() == 1 && g_frame.queuedDraws.front().handle.id == palette.windowBackground.id);
    g_frame.queuedDraws.clear();
    begin_overlay_window("Flight status", {40, 30}, {80, 50}, false);
    CHECK(g_frame.topLayerDraws.empty());
    CHECK(g_frame.queuedDraws.size() == 1 && g_frame.queuedDraws.front().handle.id == palette.overlayBackground.id);

    // Modal/undocked windows have a visible, soft shadow beyond the body clip.
    g_frame.topLayerDepth = 1;
    begin_window({}, {40, 30}, {80, 50}, true, false);
    CHECK(g_frame.topLayerDraws.size() == 9);
    const auto& body = g_frame.topLayerDraws.back();
    CHECK(body.handle.id == palette.windowBackground.id && body.x == 40 && body.y == 30 && body.w == 80 && body.h == 50);
    CHECK(g_frame.contentMin.x == 44 && g_frame.contentMax.x == 116 && g_frame.topLayerDepth == 1);
    const int contact = shadow_alpha_at(70.5f, 81.5f);
    const int mid = shadow_alpha_at(70.5f, 86.5f);
    const int far = shadow_alpha_at(70.5f, 91.5f);
    CHECK(contact > mid && mid > far && far > 0 && shadow_alpha_at(70.5f, 94.5f) == 0);
    CHECK(shadow_alpha_at(121.5f, 50.5f) > shadow_alpha_at(130.5f, 50.5f));
    CHECK(shadow_alpha_at(124.5f, 87.5f) > 0 && shadow_alpha_at(131.5f, 94.5f) == 0);
    for (float y = 20.5f; y < 95.0f; y += 1.0f)
        for (float x = 28.5f; x < 132.0f; x += 1.0f)
            (void)shadow_alpha_at(x, y);

    g_frame.topLayerDraws.clear();
    begin_window({}, {40, 30}, {80, 50}, false, false);
    CHECK(g_frame.topLayerDraws.empty());

    g_frame.contentMin = {0, 0};
    g_frame.contentMax = {240, 180};
    render_select_popup({.position={40, 30}, .width=80, .height=50,
        .rowHeight=20, .optionPitch=20, .contentHeight=20, .options={"One"}});
    CHECK(g_frame.topLayerDraws.size() >= 10);
    CHECK(g_frame.topLayerDraws[8].handle.id == palette.consoleBackground.id);
    CHECK(shadow_alpha_at(70.5f, 81.5f) == contact && g_frame.topLayerDepth == 1);
    // A dropdown selected inside a scrolling modal must replay after all later
    // modal action buttons, with its full surface outside the body's old clip.
    g_frame.topLayerDraws.clear();
    g_frame.pendingSelectPopups.push_back({.traceOpen=true, .position={40, 30}, .width=80, .height=70,
        .rowHeight=20, .optionPitch=20, .contentHeight=80,
        .options={"One", "Two", "Three", "Four"}});
    g_scrollAreaStates[""].scrollY = 10;
    g_frame.origin={0,0}; g_frame.windowSize={240,180};
    g_frame.contentMin={40,30}; g_frame.contentMax={120,60};
    draw_sprite_raw(palette.titleBar,40,50,80,10); // following action row
    const auto actionCount=g_frame.topLayerDraws.size();
    CHECK(actionCount==1);
    g_frame.topLayerDepth=0; // modal closed before shared end-frame replay
    render_pending_select_popups();
    CHECK(g_frame.pendingSelectPopups.empty() && g_frame.topLayerDepth==0);
    CHECK(logger::calls==1 && g_frame.tracePopupPublication);
    render_pending_select_popups();
    CHECK(logger::calls==1);
    CHECK(g_frame.contentMax.y==60); // inherited body clip restored
    CHECK(g_frame.topLayerDraws[actionCount+8].handle.id==palette.consoleBackground.id);
    CHECK(g_frame.topLayerDraws[actionCount+8].h==70);
    CHECK(g_frame.topLayerDraws.back().y>=80); // later options weren't body-clipped
    CHECK(g_frame.topLayerDraws.front().handle.id==palette.titleBar.id);
    g_frame.topLayerDepth=1;
    // Large floating surfaces use a quiet single-pixel themed boundary, never
    // a thick heading accent. Paint does not change the resize hit geometry.
    g_frame.topLayerDraws.clear();
    FloatingWindowState floating;FloatingWindowOptions options;
    FloatingWindowResult bounds{.window={{10,10},{200,150}},.resize_handle={{190,140},{20,20}}};
    paint_floating_window_chrome(floating,options,bounds);
    CHECK(g_frame.topLayerDraws.size()==4);
    for(const auto&draw:g_frame.topLayerDraws){CHECK(draw.handle.id==palette.buttonHover.id);CHECK(draw.w==1||draw.h==1);}
    CHECK(bounds.resize_handle.size.x==20&&bounds.resize_handle.position.x==190);
    floating.resizing=true;g_frame.topLayerDraws.clear();paint_floating_window_chrome(floating,options,bounds);
    CHECK(g_frame.topLayerDraws.front().handle.id==palette.buttonActive.id);
    options.draw_background=false;g_frame.topLayerDraws.clear();paint_floating_window_chrome(floating,options,bounds);
    CHECK(g_frame.topLayerDraws.empty());
    // Single-line captions center their font box, not multiline leading.
    begin_window("Inspector", {40, 30}, {80, 50}, true, true);
    CHECK(captionY == 43.0f);
    CHECK(g_frame.contentMin.y == 54.0f); // Existing title/content geometry retained.
    // Caption fitting, centering and drawing all use the selected contrast font.
    palette.captionUsesLightText = g_resources.font.asset = true;
    draw_window_caption("Scene Preview", 40.0f, 30.4f, 20.0f, 160.0f, 1.0f);
    CHECK(captionY == 44.0f && g_temporaryTextFont == nullptr);
}
'''
        for marker, source in (
            ('WINDOW_TYPES', types), ('SHADOW_CONSTANTS', constants),
            ('WINDOW_PAINT', paint), ('BEGIN_WINDOW', begin_window), ('POPUP_PAINT', popup),
            ('ACTUAL_CAPTION', caption),
            ('CLEAR_CLIP', clear_clip), ('PENDING_POPUP', pending), ('FLOATING_CHROME', chrome),
        ):
            fixture = fixture.replace(marker, source)
        report = ROOT / 'Artifacts/Reports/gui-window-shadow-20261116' / hashlib.sha256(fixture.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        source = report / 'actual_owners.cpp'
        source.write_text(fixture, encoding='utf-8')
        results = compile_and_run(self, source, report)
        (report / 'result.json').write_text(json.dumps({
            'scope': 'actual shared shadow atlas generation and queued GUI surface paint with a recording renderer',
            'results': results,
        }, indent=2), encoding='utf-8')
