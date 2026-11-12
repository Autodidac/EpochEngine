"""Exercise the real edit box, text controller, event queue and scroll parent."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_queued_drag import fixture as queue_fixture

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / "Engine/src/epochgui"
PORTABLE = ROOT / "Engine/dep/EpochGui"


def fixture():
    owner = (GUI / "gui.engine.cpp").read_text(encoding="utf-8")
    frame = (GUI / "controls/gui.frame_lifecycle.inl").read_text(encoding="utf-8")
    text = (GUI / "controls/gui.text_controls.inl").read_text(encoding="utf-8")
    scroll = (GUI / "controls/gui.scroll_controls.inl").read_text(encoding="utf-8")
    buttons = (GUI / "controls/gui.buttons_images.inl").read_text(encoding="utf-8")
    api = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
    base = queue_fixture().split("void fast_sequences(", 1)[0]
    base = base.replace("#include <algorithm>", "#include <functional>\n#include <unordered_map>\n#include <algorithm>")
    base = base.replace("import epoch.gui.input;", '#include "gui/gui.chat_context_popup.hpp"\nimport epoch.gui.input;\nimport epoch.gui;\nimport epoch.gui.rounded_rect;')
    base = base.replace("Vec2 contentMin{},contentMax{};bool insideWindow{};", "Vec2 contentMin{},contentMax{},cursor{},origin{},windowSize{};bool insideWindow{};\n"
        "std::string windowKey;std::uint64_t widgetSerial{};\n"
        "std::vector<InputEvent>modalHeldEvents;int modalHeldWheel{};\n"
        "bool modalInputSuspended{},modalOwnerDrawn{},modalOwnerDrawing{};\n"
        "std::optional<WidgetBounds>lastButtonBounds;")
    base = base.replace("struct Frame{", definition(api, "    export struct WidgetBounds", True).replace("export ", "") + "\nstruct Frame{")
    base = base.replace("bool visited_this_frame{};", "bool visited_this_frame{},open{},alignSelectedOnOpen{};")
    base = base.replace("void reset_edit_box_visits(const void*){}", "static void reset_edit_box_visits(const void*) noexcept;")
    extra_api = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export struct KeyboardInputTarget", "    export struct KeyboardInputOptions",
        "    export struct EditBoxResult", "    export enum class TextInputKeyboardPolicy",
        "    export class ContentClipScope", "    export enum class ButtonIcon",
        "    export struct ScrollAreaOptions", "    export struct ScrollAreaResult"))
    extra_state = "\n".join(definition(owner, marker, True) for marker in (
        "        struct EditBoxTextState", "        struct EditBoxMenuState",
        "        struct ScrollAreaState", "        struct ScrollAreaFrame"))
    helpers = "\n".join(definition(owner, marker) for marker in (
        "        static void reset_edit_box_visits(", "        static void retire_hidden_edit_boxes(",
        "        [[nodiscard]] static bool right_press_available()",
        "        static void consume_left_press()", "        static void consume_right_press()",
        "        static void consume_right_release()", "        [[nodiscard]] static std::size_t widget_press_key(",
        "        [[nodiscard]] static const void* widget_focus_key(",
        "        [[nodiscard]] static float content_right()", "        [[nodiscard]] static float content_available_width(",
        "        [[nodiscard]] static std::string window_scoped_state_key(",
        "        [[nodiscard]] static std::string scroll_panel_key(",
        "        [[nodiscard]] static bool is_select_box_list_id(",
        "    ContentClipScope::ContentClipScope(", "    ContentClipScope::~ContentClipScope()"))
    helpers += "\n" + "\n".join(definition(frame, marker) for marker in (
        "    void set_cursor(", "    void advance_cursor(", "    bool keyboard_input_captured(",
        "    void end_frame()"))
    key_helpers = text[text.index("    [[nodiscard]] static std::size_t edit_box_line_start("):text.index("    EditBoxResult edit_box(")]
    # The default empty string_view contains braces before the function body.
    button_start = buttons.index("    static bool button_with_state(")
    button_body = buttons.index("\n    {", button_start) + 1
    button_owner = buttons[button_start:button_body] + definition(buttons[button_body:], "    {")
    owners = "\n".join((
        button_owner,
        definition(buttons, "    bool button("), key_helpers,
        definition(scroll, "    [[nodiscard]] static bool update_scrollbar_drag("),
        definition(scroll, "    ScrollAreaResult begin_scroll_area("),
        definition(scroll, "    void end_scroll_area()"),
        definition(text, "    EditBoxResult edit_box("),
        definition(text, "    static bool edit_box_insert_text("),
        definition(text, "    bool paste_text_in_edit_box("),
        definition(text, "    bool replace_text_in_edit_box(")))
    # The callback boundary is only a font/paint/clipboard port. Text storage,
    # wrapping, caret mapping, history, scrollbar mapping and all input admission
    # below execute their production owners without replacement algorithms.
    return base + r'''
namespace gui {
using SemanticTone=gui_lib::SemanticTone;
EXTRA_API
EXTRA_STATE
std::map<const void*,const void*> g_contextActiveWidgets;
std::map<const void*,EditBoxTextState> g_editBoxTextStates;
std::map<const void*,EditBoxMenuState> g_editBoxMenuStates;
std::map<const void*,bool> g_textFieldSelectAllStates;
std::map<std::string,ScrollAreaState> g_scrollAreaStates;
std::vector<ScrollAreaFrame> g_scrollAreaStack;
std::vector<int> g_windowFrameStack;
constexpr float kFontScale=1,kBoxInnerPadding=4,kContentPadding=4,kButtonTextClipInset=0;
struct SpriteHandle {int value{};};
struct GuiFontCache {bool asset{};};
const GuiFontCache* g_temporaryTextFont{};
struct {GuiFontCache halfTextLightFont,halfTextFont;}g_resources;
struct Palette {
 SpriteHandle textField{1},textFieldActive{2},buttonActive{3},windowBackground{4},
  consoleBackground{5},buttonNormal{6},buttonHover{7},panelBackground{8};
 bool isLight{},beveledControls{};
}palette;
struct Paint {std::string label;Vec2 position,size;int sprite{};};
std::vector<Paint> paints,buttonBounds;
struct {SpriteHandle accent{9},background{10};}semantic;
const Palette& active_palette(){return palette;}
bool active_palette_is_light(){return palette.isLight;}
const auto& active_semantic_pair(SemanticTone){return semantic;}
void ensure_resources(){}
void render_pending_select_popups() noexcept{}
void flush_queued_draws() noexcept{}
void begin_top_layer(){++g_frame.topLayerDepth;}
void end_top_layer(){--g_frame.topLayerDepth;}
bool draw_beveled_control(const Palette&,SpriteHandle,float,float,float,float,bool){return false;}
bool draw_high_contrast_control(float,float,float,float,bool){return false;}
void draw_sprite(SpriteHandle sprite,float x,float y,float width,float height){
 paints.push_back({{}, {x,y},{width,height},sprite.value});
}
void draw_rounded_sprite(SpriteHandle sprite,float x,float y,float width,float height,
 const gui_lib::rounded_rect::RoundedRectStyle&){draw_sprite(sprite,x,y,width,height);}
float base_line_height(float scale){return 12*scale;}
float line_advance_amount(float scale){return 14*scale;}
float space_advance(float scale){return 8*scale;}
struct DecodedUtf8Codepoint {char32_t codepoint{};std::size_t byte_count=1;};
DecodedUtf8Codepoint decode_utf8_codepoint(std::string_view value,std::size_t index){
 if(index>=value.size())return {};
 const auto byte=static_cast<unsigned char>(value[index]);
 const std::size_t length=byte<0x80?1:byte<0xE0?2:byte<0xF0?3:4;
 return {byte<0x80?char32_t(byte):char32_t(0xE9),(std::min)(length,value.size()-index)};
}
char32_t safe_draw_codepoint(char32_t codepoint){return codepoint;}
char32_t next_drawable_char(std::string_view,std::size_t){return {};}
bool is_wrap_space(char32_t c){return c==' '||c=='\t';}
float glyph_advance_with_kerning(char32_t c,char32_t,float scale){return c=='\t'?32*scale:8*scale;}
float measure_text_width(std::string_view text,float scale){float result{};
 for(std::size_t i{};i<text.size();){const auto c=decode_utf8_codepoint(text,i);i+=c.byte_count;
  result+=glyph_advance_with_kerning(c.codepoint,0,scale);}return result;
}
std::string fit_text_to_width(std::string_view text,float width,float scale){
 return std::string(text.substr(0,(std::min)(text.size(),std::size_t((std::max)(0.0f,width)/(8*scale)))));
}
void draw_text_line(std::string_view text,float x,float y,float scale){
 paints.push_back({std::string(text),{x,y},{measure_text_width(text,scale),12*scale},0});
 if(g_frame.windowKey.starts_with("edit-box-context-menu-"))
  buttonBounds.push_back({std::string(text),g_frame.cursor,{172-8,28},0});
}
void draw_contrast_text(std::string_view text,float x,float y,float scale){draw_text_line(text,x,y,scale);}
void draw_caret(float x,float y,float height){paints.push_back({"caret",{x,y},{1,height},0});}
void draw_button_icon(ButtonIcon,Vec2,Vec2){}
void draw_button_tooltip(std::string_view,Vec2){}
std::string clipboard;
bool clipboard_write_text(std::string_view value){clipboard=value;return true;}
std::string clipboard_read_text(){return clipboard;}
HELPERS
OWNERS
}
using namespace gui;
CASES
'''.replace("EXTRA_API", extra_api).replace("EXTRA_STATE", extra_state).replace("HELPERS", helpers).replace("OWNERS", owners).replace("CASES", CASES)


CASES = r'''
InputEvent text(std::string value){return {.type=EventType::TextInput,.text=std::move(value)};}
InputEvent key(int code,bool ctrl=false,bool shift=false){return {.type=EventType::KeyDown,.key=code,.ctrl_down=ctrl,.shift_down=shift};}
InputEvent wheel(float x,float y,int delta){return {.type=EventType::MouseWheel,.mouse_pos={x,y},.wheel_delta=delta};}
void layout(Vec2 position={20,20},Vec2 size={760,560},std::string window="test"){
 g_frame.insideWindow=true;g_frame.origin={0,0};g_frame.windowSize={800,600};
 g_frame.cursor=position;g_frame.contentMin={0,0};g_frame.contentMax=size;g_frame.windowKey=std::move(window);
 paints.clear();buttonBounds.clear();
}
void next(const std::shared_ptr<core::Context>& context,std::initializer_list<InputEvent> events={},Vec2 pointer={30,30},bool down=false){
 tick(context,events,down,pointer);layout();
}
auto draw(std::string& value,Vec2 size={150,50}){return edit_box(value,size,8192,true,
 TextInputKeyboardPolicy::focused,false,"placeholder",true,false,{});}
auto& state(std::string& text){return g_editBoxTextStates.at(&text);}
bool near(float a,float b){return std::abs(a-b)<.01f;}
void close_frame(){end_frame();}
void stable_focus_and_history(const std::shared_ptr<core::Context>& context){
 std::string value="first";next(context,{event(EventType::MouseDown,30,30),event(EventType::MouseUp,30,30)});
 CHECK(draw(value).active);close_frame();
 next(context,{key(35,true),text(" one")});CHECK(draw(value).changed&&value=="first one");close_frame();
 next(context,{text(" two")});CHECK(draw(value).changed&&value=="first one two");
 const auto caret=state(value).control.caret,history=state(value).control.undo_history.size();close_frame();
 // The same std::string object retains focus/history as the outer scroll,
 // window movement and wrapping width all change its rectangle.
 for(const auto position:{Vec2{50,60},Vec2{80,-15},Vec2{10,100}}){
  next(context);layout(position);CHECK(draw(value,{110,80}).active);
  CHECK(g_contextActiveWidgets[context.get()]==&value&&state(value).control.caret==caret
   &&state(value).control.undo_history.size()==history);close_frame();
 }
 next(context,{key('Z',true)});layout({40,70});CHECK(draw(value,{240,70}).changed&&value=="first one");close_frame();
 next(context,{key('Y',true)});CHECK(draw(value).changed&&value=="first one two");close_frame();
 next(context,{key('Z',true,true)});CHECK(!draw(value).changed&&value=="first one two");close_frame();
 // Right-click real six-row edit menu; native queued press/release is routed
 // through the actual shared button owner, not an injected selected action.
 next(context,{event(EventType::MouseDown,40,40,1),event(EventType::MouseUp,40,40,1)});
 draw(value);CHECK(buttonBounds.size()==6);const auto undo=buttonBounds[0],redo=buttonBounds[1];
 CHECK(undo.label=="Undo"&&redo.label=="Redo"&&buttonBounds[5].label=="Paste");
 CHECK(buttonBounds[5].position.y+buttonBounds[5].size.y<=g_frame.contentMax.y);close_frame();
 const Vec2 point{undo.position.x+10,undo.position.y+10};
 next(context,{event(EventType::MouseDown,point.x,point.y),event(EventType::MouseUp,point.x,point.y)},point);
 CHECK(draw(value).changed&&value=="first one"&&!g_editBoxMenuStates.at(&value).open);close_frame();
 next(context,{key('Y',true)});draw(value);CHECK(value=="first one two");close_frame();
}
std::string long_text(){std::string result;for(int i=0;i<20;++i)result+="line "+std::to_string(i)+" words wrap here\n";return result;}
void overflow_and_scroll(const std::shared_ptr<core::Context>& context){
 std::string value=long_text();next(context);draw(value);auto& control=state(value).control;
 CHECK(state(value).wheelVisible&&control.scroll.y>0);const auto bottom=control.scroll.y;close_frame();
 next(context,{wheel(30,30,120)});draw(value);const auto manuallyScrolled=control.scroll.y;
 CHECK(manuallyScrolled<bottom&&state(value).scrolling&&g_frame.mouseWheelDelta==0);close_frame();
 for(int frame=0;frame<8;++frame){next(context);draw(value);CHECK(near(control.scroll.y,manuallyScrolled));close_frame();}
 // Resizing rewraps through TextControl's real visual-line policy while
 // preserving a user's manual scroll, rather than following its hidden caret.
 next(context);layout({40,35});draw(value,{110,60});CHECK(near(control.scroll.y,manuallyScrolled));close_frame();
 next(context,{event(EventType::MouseDown,50,40),event(EventType::MouseUp,50,40)});layout({40,35});draw(value,{110,60});
 CHECK(control.focused);close_frame();
 next(context,{key(35,true)});draw(value);CHECK(control.caret==value.size()&&control.scroll.y>=manuallyScrolled);close_frame();
 // The visible internal scrollbar uses the exact shared drag and portable
 // mapping owners, and keeps its offset on following idle frames.
 next(context);draw(value);const auto geometry=gui_lib::make_scrollbar_layout({
  .track={{160,20},{10,50}},.content_extent=edit_box_text_metrics(control,1,14,12,132,true).content_size.y+8,
  .scroll_offset=control.scroll.y});close_frame();
 const Vec2 thumb{geometry.thumb.position.x+5,geometry.thumb.position.y+3};
 next(context,{event(EventType::MouseDown,thumb.x,thumb.y)},thumb,true);draw(value);close_frame();
 next(context,{event(EventType::MouseMove,thumb.x,25),event(EventType::MouseUp,thumb.x,25)}, {thumb.x,25});
 draw(value);const auto dragged=control.scroll.y;CHECK(dragged<bottom&&state(value).scrolling);close_frame();
 next(context);draw(value);CHECK(near(control.scroll.y,dragged));close_frame();
}
void nested_wheel_ownership(const std::shared_ptr<core::Context>& context){
 std::string value=long_text();const ScrollAreaOptions parent{.id="parent",.size={400,260},.content_height=600,.show_scrollbar=false};
 next(context);begin_scroll_area(parent);draw(value);g_frame.cursor.y=620;end_scroll_area();close_frame();
 const auto previous=state(value).control.scroll.y;
 next(context,{wheel(30,30,120)});const auto parentScroll=begin_scroll_area(parent);
 CHECK(!parentScroll.wheel_scrolled&&near(parentScroll.scroll_y,0)&&g_frame.mouseWheelDelta==120);
 draw(value);CHECK(state(value).control.scroll.y<previous&&g_frame.mouseWheelDelta==0);
 g_frame.cursor.y=620;end_scroll_area();close_frame();
 // Outside the child, its scroll parent still owns wheel input normally.
 next(context,{wheel(300,180,-120)}, {300,180});auto moved=begin_scroll_area(parent);
 CHECK(moved.wheel_scrolled&&moved.scroll_y>0&&g_frame.mouseWheelDelta==0);
 draw(value);g_frame.cursor.y=620;end_scroll_area();close_frame();
 // Hidden controls retire only this context's wheel claim after end_frame.
 next(context);close_frame();CHECK(!state(value).wheelVisible);
 next(context,{wheel(30,30,-120)});CHECK(!edit_box_captures_wheel());
 CHECK(begin_scroll_area(parent).wheel_scrolled);g_frame.cursor.y=620;end_scroll_area();close_frame();
 // Retained bounds are clipped, window-scoped, and context-scoped.
 next(context);g_frame.contentMin={50,25};g_frame.contentMax={100,55};draw(value);close_frame();
 next(context,{wheel(30,30,-120)});CHECK(!edit_box_captures_wheel()&&g_frame.mouseWheelDelta==-120);
 draw(value);close_frame();
 next(context,{wheel(60,30,-120)},{60,30});layout({20,20},{760,560},"another-window");
 CHECK(!edit_box_captures_wheel());close_frame();
 auto other=std::make_shared<core::Context>();fresh(other);next(other,{wheel(60,30,-120)},{60,30});
 CHECK(!edit_box_captures_wheel());close_frame();
 next(context,{wheel(30,30,-120)});g_frame.modalInput.begin({{0,0},{800,600}});
 g_frame.modalInput.require_owner_scope();draw(value);CHECK(g_frame.mouseWheelDelta==-120);close_frame();
}
void hidden_focus_and_ordered_dispatch(const std::shared_ptr<core::Context>& context){
 std::string value="A";next(context);g_contextActiveWidgets[context.get()]=&value;draw(value);close_frame();
 next(context);close_frame();CHECK(g_contextActiveWidgets[context.get()]==nullptr);
 next(context,{text("B"),key('Z',true),key('Y',true)});
 const KeyboardInputOptions options{.owner=&value,.owns_text=[&](const void* id){return id==&value;},
  .target=[&]{return KeyboardInputTarget{.identity=&value,.text=&value,.size={150,50},.maximum_bytes=8192,.multiline=true};},
  .command=[](const InputEvent&){return false;}};
 CHECK(dispatch_keyboard_input(options)&&value=="AB"&&g_frame.events.empty());
 draw(value);CHECK(value=="AB");close_frame();
 next(context,{{.type=EventType::FocusLost},text("invisible")});draw(value);CHECK(value=="AB");close_frame();
 next(context,{{.type=EventType::FocusLost},text("invisible")});g_textFieldSelectAllStates[&value]=true;
 CHECK(!draw(value).active&&value=="AB"&&g_contextActiveWidgets[context.get()]==nullptr);close_frame();
}
void programmatic_replace(const std::shared_ptr<core::Context>& context){
 std::string value="prior prompt";next(context);g_contextActiveWidgets[context.get()]=&value;draw(value);close_frame();
 const auto before=state(value).control.undo_history.size();
 CHECK(replace_text_in_edit_box(value,"generated prompt\nsecond line",8192));
 CHECK(value=="generated prompt\nsecond line"&&state(value).control.undo_history.size()==before+1);
 next(context,{key('Z',true)});draw(value);CHECK(value=="prior prompt");close_frame();
 next(context,{key('Y',true)});draw(value);CHECK(value=="generated prompt\nsecond line");close_frame();
 CHECK(replace_text_in_edit_box(value,"",8192)&&value.empty());
 next(context,{key('Z',true)});draw(value);CHECK(value=="generated prompt\nsecond line");close_frame();
 next(context,{key('Y',true)});draw(value);CHECK(value.empty());close_frame();
 CHECK(!replace_text_in_edit_box(value,"",8192));
 CHECK(!paste_text_in_edit_box(value,"",8192));
 CHECK(paste_text_in_edit_box(value,"short",8192)&&value=="short");
 CHECK(replace_text_in_edit_box(value,"too large",3)&&value=="too");
 next(context,{key('Z',true)});draw(value);CHECK(value=="short");close_frame();
}
int main(){auto context=std::make_shared<core::Context>();fresh(context);
 stable_focus_and_history(context);overflow_and_scroll(context);nested_wheel_ownership(context);
 hidden_focus_and_ordered_dispatch(context);programmatic_replace(context);return 0;}
'''


class EditBoxScrolling(unittest.TestCase):
    def test_actual_edit_box_and_portable_history(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        code = fixture()
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/edit-box-scrolling-nov06" / identity
        report.mkdir(parents=True, exist_ok=True)
        (report / "consumer.cpp").write_text(code, encoding="utf-8")
        modules = [PORTABLE / "modules" / name for name in (
            "epoch.gui.ixx", "epoch.gui.input.ixx", "epoch.gui.rounded_rect.ixx")]
        sources = [PORTABLE / "src/epochgui" / name for name in (
            "text_control.cpp", "gui.text_layout.cpp", "input.cpp", "gui.scrollbar_layout.cpp")]
        (report / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.30)\nproject(EditBoxScrolling LANGUAGES CXX)\n'
            + 'add_executable(edit_box consumer.cpp ' + " ".join('"' + p.as_posix() + '"' for p in sources) + ')\n'
            + 'target_compile_features(edit_box PRIVATE cxx_std_23)\n'
            + 'set_property(TARGET edit_box PROPERTY MSVC_DEBUG_INFORMATION_FORMAT Embedded)\n'
            + 'target_compile_options(edit_box PRIVATE /W3 /WX /utf-8 /EHsc)\n'
            + 'target_sources(edit_box PRIVATE FILE_SET CXX_MODULES BASE_DIRS "' + ROOT.as_posix() + '" FILES '
            + " ".join('"' + p.as_posix() + '"' for p in modules) + ')\n'
            + 'target_include_directories(edit_box PRIVATE "' + ROOT.as_posix() + '/Engine/include")\n', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/cmake/edit-box-scrolling" / identity
        commands = [["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         [str(build / config / "edit_box.exe")]]
        runs = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=report, capture_output=True, text=True, timeout=120)
            log = report / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": completed.returncode, "log": str(log)})
            (report / "result.json").write_text(json.dumps({
                "passed": completed.returncode == 0 and index == len(commands) - 1,
                "scope": "Actual queued input/begin_frame/end_frame, real edit_box and shared buttons, portable TextControl and scrollbar layout/mapping, parent scroll-area deferral. Recording font/draw/clipboard ports; no native input/renderer/model.",
                "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (
                    GUI / "gui.engine.cpp", GUI / "controls/gui.text_controls.inl",
                    GUI / "controls/gui.scroll_controls.inl", GUI / "controls/gui.frame_lifecycle.inl",
                    GUI / "controls/gui.buttons_images.inl", Path(__file__), *modules, *sources)},
                "runs": runs}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(completed.returncode, 0, completed.stdout[-7000:] + completed.stderr[-1500:])


if __name__ == "__main__":
    unittest.main()
