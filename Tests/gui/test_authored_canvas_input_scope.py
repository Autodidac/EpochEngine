"""Real authored-widget and scoped-input owners; recording paint ports, real modules."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "Engine/src/editor/application/editor.application_gui_authoring.inl"
API = ROOT / "Engine/modules/gui.engine.ixx"
FRAME = ROOT / "Engine/src/epochgui/controls/gui.frame_lifecycle.inl"


def fixture(owner_text=None):
    api, frame = API.read_text(encoding="utf-8"), FRAME.read_text(encoding="utf-8")
    text = (ROOT / "Engine/src/epochgui/controls/gui.text_controls.inl").read_text(encoding="utf-8")
    owner = definition(owner_text or APP.read_text(encoding="utf-8"),
        "        [[nodiscard]] bool render_gui_document_widget(")
    declarations = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export enum class EventType", "    export struct InputEvent",
        "    export class ScopedInputSuppression", "    export struct DragSurfaceState",
        "    export struct DragSurfaceOptions", "    export struct DragSurfaceResult"))
    # Braced member initializers precede the constructor body; retain this exact
    # adjacent owner region instead of treating its first brace as that body.
    scope = frame[frame.index("    ScopedInputSuppression::ScopedInputSuppression("):
        frame.index("    Vec2 mouse_position()")]
    # The actual pending-programmatic-selection gate must not consume a future
    # request while a render-only surface paints the same focused text object.
    select_all = definition(text, "        if (!g_frame.keyboardFocusCancelled && g_frame.modalInput.owner_input_allowed()")
    return r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
import authoring.gui_document;
import epoch.gui.input;
namespace authoring=epochengine::authoring;
namespace gui_lib=epochengine::gui_lib;
struct SpriteHandle{int id{};bool is_valid()const{return id!=0;}};
void check(bool value,int line){if(!value){std::fprintf(stderr,"Canvas input contract failed line %d\n",line);std::exit(81);}}
#define CHECK(value) check(static_cast<bool>(value),__LINE__)
namespace gui {
struct Vec2{float x{},y{};};
DECLARATIONS
struct Frame{
 void* ctx=reinterpret_cast<void*>(1);bool insideWindow=true;
 bool mouseDown{},prevMouseDown{},mouseRightDown{},prevMouseRightDown{};
 bool justPressed{},justReleased{},rightJustPressed{},rightJustReleased{};
 bool mousePressConsumed{},mouseReleaseConsumed{},rightPressConsumed{},rightReleaseConsumed{};
 bool keyboardFocusCancelled{};
 int mouseWheelDelta{};Vec2 mousePos{},cursor{};float caretTimer{};bool caretVisible{};
 gui_lib::input::ModalInputArbiter modalInput;
 std::vector<InputEvent> events;
} g_frame;
SCOPE
struct ScopedTextScale{explicit ScopedTextScale(float){}};
struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};
struct Paint{std::string kind;bool selected{},enabled=true;std::size_t count{};friend bool operator==(const Paint&,const Paint&)=default;};
std::vector<Paint> paints;
bool pointer(){return g_frame.modalInput.owner_input_allowed();}
Vec2 mouse_position(){return g_frame.mousePos;}
bool is_mouse_over_content_clip(){return pointer();}
bool was_mouse_pressed(){return pointer()&&g_frame.justPressed&&!g_frame.mousePressConsumed;}
std::optional<Vec2> mouse_press_position(){return was_mouse_pressed()?std::optional{g_frame.mousePos}:std::nullopt;}
bool drag_surface_hit_test(Vec2 p,const DragSurfaceOptions& options){return pointer()&&p.x>=options.position.x&&p.y>=options.position.y&&p.x<=options.position.x+options.size.x&&p.y<=options.position.y+options.size.y;}
void set_cursor(Vec2 p){g_frame.cursor=p;}
void panel_rect(Vec2,Vec2){paints.push_back({"panel"});}
void label(std::string_view){paints.push_back({"label"});}
void wrapped_label(std::string_view,float){paints.push_back({"wrapped_label"});}
void selection_outline(Vec2,Vec2,float){paints.push_back({"selection"});}
bool button_selected(std::string_view,Vec2,bool selected){paints.push_back({"button",selected});return pointer()&&g_frame.justReleased;}
DragSurfaceResult drag_surface(DragSurfaceState&,const DragSurfaceOptions&){return {};}
struct EditBoxResult{bool active{},changed{};};
inline const void* focused=reinterpret_cast<void*>(7);
EditBoxResult edit_box(std::string& value,Vec2,std::size_t,bool){
 paints.push_back({"edit"});
 const bool changed=pointer()&&!g_frame.events.empty();
 if(changed)value+="BAD";
 return {focused!=nullptr,changed};
}
struct SliderOptions{std::string_view id,label;float minimum{},maximum{},value{},step{};Vec2 size;bool enabled=true;};
struct SliderResult{float value{};bool changed{};};
SliderResult slider(const SliderOptions& options){
 paints.push_back({"slider",false,options.enabled});
 const bool changed=options.enabled&&pointer()&&g_frame.mouseDown;
 return {changed?options.maximum:options.value,changed};
}
enum class ImageFit{Contain};
struct ImageBoxOptions{std::string_view id;::SpriteHandle sprite;Vec2 size,source_size;std::string_view caption;ImageFit fit{};bool interactive{},selected{};};
struct ImageBoxResult{bool clicked{};};
ImageBoxResult image_box(const ImageBoxOptions& options){paints.push_back({"image",options.selected});return {options.interactive&&pointer()&&g_frame.justReleased};}
struct TabButtonSpec{std::string_view id,label;float width{};bool active{},closable{};};
struct TabBarResult{std::optional<std::size_t> selected_index;};
TabBarResult tab_bar_buttons(std::span<const TabButtonSpec> tabs,float,float){
 paints.push_back({"tabs",false,true,tabs.size()});
 return {pointer()&&!g_frame.events.empty()?std::optional<std::size_t>{1}:std::nullopt};
}
}
namespace editor_project_textures{struct TextureCatalogEntry{std::string logical_path;};}
struct Textures{std::vector<editor_project_textures::TextureCatalogEntry> catalog(){return {{"image"}};}};
struct EditorState{
 gui::DragSurfaceState guiCanvasWidgetDrag{};authoring::gui::WidgetHandle guiCanvasWidgetDragHandle{},selected{};
 float guiCanvasWidgetDragOriginX{},guiCanvasWidgetDragOriginY{},guiCanvasWidgetDragPreviewX{},guiCanvasWidgetDragPreviewY{};
 std::string guiDocumentStatus,guiContentPrimary{"Text"};
};
Textures* project_texture_controller(EditorState&){static Textures value;return &value;}
std::string_view gui_widget_asset_path(const authoring::gui::WidgetView&){return "image";}
SpriteHandle project_texture_thumbnail(EditorState&,Textures&,const editor_project_textures::TextureCatalogEntry&){return {1};}
std::string gui_widget_display_text(const authoring::gui::WidgetView& widget){return widget.descriptor.name;}
bool gui_widget_selected(const EditorState& state,authoring::gui::WidgetHandle widget){return state.selected==widget;}
void select_gui_widget(EditorState& state,authoring::gui::WidgetHandle widget){state.selected=widget;}
void hydrate_gui_content_editor(EditorState&,const authoring::gui::WidgetView&){}
bool project_gui_document_layout(EditorState&){return true;}
// This paint/input fixture never releases a widget drag. Paired journals are
// exercised separately by test_grouped_gui_world_history.
bool commit_gui_layout_edit(EditorState&,authoring::gui::WidgetHandle,authoring::gui::LayoutDescriptor){return false;}
bool editor_point_in_rect(gui::Vec2 p,gui::Vec2 o,gui::Vec2 s){return p.x>=o.x&&p.y>=o.y&&p.x<=o.x+s.x&&p.y<=o.y+s.y;}
OWNER
void selection_request_gate(){
 std::map<const void*,bool> g_textFieldSelectAllStates;
 const void* textKey=reinterpret_cast<void*>(9);const void* id=textKey;const void* ctxKey=reinterpret_cast<void*>(10);
 std::map<const void*,const void*> g_contextActiveWidgets;bool active=false;
 namespace gui_lib=epochengine::gui_lib;
 const auto dispatch=[](gui_lib::TextControlCommand,std::string_view,std::size_t,bool,bool,bool){return 0;};
 auto& g_frame=gui::g_frame;g_textFieldSelectAllStates[textKey]=true;
 {gui::ScopedInputSuppression blocked;
 SELECT_ALL
 CHECK(g_textFieldSelectAllStates[textKey]&&!active&&g_contextActiveWidgets.empty());}
 g_frame.keyboardFocusCancelled=true;
 SELECT_ALL
 CHECK(g_textFieldSelectAllStates[textKey]&&!active&&g_contextActiveWidgets.empty());
 g_frame.keyboardFocusCancelled=false;
 SELECT_ALL
 CHECK(!g_textFieldSelectAllStates[textKey]&&active&&g_contextActiveWidgets[ctxKey]==id);
}
void scope_contract(){using namespace gui;
 g_frame={};g_frame.mouseDown=g_frame.prevMouseDown=g_frame.mouseRightDown=g_frame.prevMouseRightDown=true;
 g_frame.justPressed=g_frame.justReleased=g_frame.rightJustPressed=g_frame.rightJustReleased=true;
 g_frame.mousePressConsumed=true;g_frame.rightReleaseConsumed=true;g_frame.mouseWheelDelta=120;
 g_frame.events={{.type=EventType::TextInput,.text="retain"},{.type=EventType::KeyDown,.key=9},
  {.type=EventType::FileDrop,.files={"image.png"}}};
 const auto* eventBytes=g_frame.events.data();const auto focus=focused;
 g_frame.modalInput.begin({{1,2},{300,200}});g_frame.modalInput.require_owner_scope();g_frame.modalInput.enter_owner_scope();
 {ScopedInputSuppression blocked;
  CHECK(!pointer()&&g_frame.events.empty()&&g_frame.mouseWheelDelta==0&&!g_frame.mouseDown&&!g_frame.prevMouseDown);
  CHECK(!g_frame.mouseRightDown&&!g_frame.prevMouseRightDown&&!g_frame.justPressed&&!g_frame.justReleased&&!g_frame.rightJustPressed&&!g_frame.rightJustReleased);
  CHECK(g_frame.modalInput.keyboard_captured()&&focused==focus);
  {ScopedInputSuppression nested;CHECK(!pointer());}
  {ScopedInputSuppression noChange{false};CHECK(!pointer());}
  CHECK(!pointer()&&g_frame.mousePressConsumed&&g_frame.rightReleaseConsumed);
 }
 CHECK(pointer()&&g_frame.events.data()==eventBytes&&g_frame.events.size()==3&&g_frame.events[0].text=="retain");
 CHECK(g_frame.events[2].files==std::vector<std::string>{"image.png"}&&g_frame.mouseWheelDelta==120);
 CHECK(g_frame.mouseDown&&g_frame.prevMouseDown&&g_frame.mouseRightDown&&g_frame.prevMouseRightDown);
 CHECK(g_frame.justPressed&&g_frame.justReleased&&g_frame.rightJustPressed&&g_frame.rightJustReleased);
 CHECK(g_frame.modalInput.mode()==gui_lib::input::ModalInputMode::bounded&&g_frame.modalInput.bounds().position.x==1&&focused==focus);
 g_frame.modalInput.leave_owner_scope();{ScopedInputSuppression noChange{false};CHECK(!pointer());}CHECK(!pointer());
 g_frame.modalInput.clear();selection_request_gate();
}
void paint_contract(){using namespace authoring::gui;
 GuiDocument document{DocumentHandle{7,1},BranchIdentity{9,1}};WidgetDescriptor root;root.kind=WidgetKind::canvas;root.name="Canvas";root.content=CanvasContent{};
 const auto canvas=document.create_widget(root).widget;
 std::vector<WidgetHandle> handles;
 for(auto kind:{WidgetKind::button,WidgetKind::text_input,WidgetKind::slider,WidgetKind::tab_set,WidgetKind::image_button}){
  WidgetDescriptor d;d.kind=kind;d.name=std::to_string(handles.size());
  switch(kind){case WidgetKind::button:d.content=ButtonContent{"Button"};break;
  case WidgetKind::text_input:d.content=TextInputContent{"Text","Placeholder",256,false,false};break;
  case WidgetKind::slider:d.content=SliderContent{"Slider",0,100,25,1};break;
  case WidgetKind::tab_set:d.content=TabSetContent{};break;
  default:d.content=ImageButtonContent{"image","Image"};break;}
  const auto created=document.create_widget(d,canvas);CHECK(created);handles.push_back(created.widget);
 }
 WidgetDescriptor page;page.kind=WidgetKind::tab_page;page.name="Page0";page.content=TabPageContent{"First",false};
 const auto first=document.create_widget(page,handles[3]).widget;page.name="Page1";page.content=TabPageContent{"Second",false};
 CHECK(document.create_widget(page,handles[3]));CHECK(document.update_content(handles[3],TabSetContent{first}));
 const auto revision=document.revision();EditorState state;
 for(auto handle:handles){
  state.selected=handle;auto widget=document.widget(handle);CHECK(widget);
  gui::g_frame={};gui::paints.clear();gui::Vec2 position{50,50};
  (void)render_gui_document_widget(state,document,*widget,position,{180,36},{1000,1000},LayoutFlow::absolute,1,true);
  const auto normal=gui::paints;const auto selected=state.selected;const auto text=state.guiContentPrimary;
  gui::paints.clear();gui::g_frame.mouseDown=gui::g_frame.justPressed=gui::g_frame.justReleased=true;
  gui::g_frame.events={{.type=gui::EventType::TextInput,.text="BAD"},{.type=gui::EventType::KeyDown,.key=9}};
  const auto* events=gui::g_frame.events.data();
  CHECK(!render_gui_document_widget(state,document,*widget,position,{180,36},{1000,1000},LayoutFlow::absolute,1,false));
  CHECK(gui::paints==normal&&document.revision()==revision&&state.selected==selected&&state.guiContentPrimary==text);
  CHECK(gui::g_frame.events.data()==events&&gui::g_frame.events.size()==2&&gui::g_frame.mouseDown&&gui::g_frame.justPressed&&gui::g_frame.justReleased);
 }
}
int main(){scope_contract();paint_contract();}
'''.replace("DECLARATIONS", declarations).replace("SCOPE", scope).replace("OWNER", owner).replace("SELECT_ALL", select_all)


class AuthoredCanvasInputScope(unittest.TestCase):
    def test_actual_input_scope_and_widget_paint_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        proof = ROOT / "build/validation/authored-canvas-input-scope-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.canvas_input_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [ROOT / "Engine/modules/authoring.document.ixx", ROOT / "Engine/modules/authoring.gui_document.ixx",
            portable / "modules/epoch.gui.ixx", portable / "modules/epoch.gui.input.ixx"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(AuthoredCanvasInput LANGUAGES CXX)
add_executable(canvas_input "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(canvas_input PRIVATE cxx_std_23)
target_compile_options(canvas_input PRIVATE /W3 /WX /utf-8)
target_sources(canvas_input PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_canvas_input COMMAND canvas_input)
set_tests_properties(actual_canvas_input PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(proof / "msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(proof / "msvc"), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(proof / "msvc"), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual portable input arbiter + Engine scope/widget owner; recording paint ports; real GUI documents; Debug/Release CPU",
            "passed": True, "nativeAcceptance": False,
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                [APP, API, FRAME, *modules, portable / "src/epochgui/input.cpp"]}, "commands": results,
            "cases": ["normal button/selected-text/slider/tabs/image painting retained while noninteractive",
                "pointer/key/text input cannot mutate/select/activate authored widgets", "event vector bytes/order and raw edge/hold/wheel restored",
                "nested/inactive scopes preserve modal mode/bounds/owner/focus and consumed bits", "pending programmatic SelectAll retained for real owner"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
