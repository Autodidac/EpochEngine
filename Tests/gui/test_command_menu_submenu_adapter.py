"""Compile the real GUI adapter against real portable menu modules.

Native input, fonts and painting are recording ports. This verifies type/lifetime,
tree layout painting and dispatch/capture integration, not native window pixels.
"""
from pathlib import Path
from Tests.support.cpp import configure_arguments, executable
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]

def child_view_definition():
 source=(ROOT/'Engine/dep/EpochGui/modules/epoch.gui.command_menu.ixx').read_text(encoding='utf-8')
 return definition(source,'    template<typename Item>\n    struct CommandMenuItemsView',True)

class SubmenuAdapter(unittest.TestCase):
 def test_actual_adapter_debug_release(self):
  api=(ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
  owner=(ROOT/'Engine/src/epochgui/controls/gui.buttons_images.inl').read_text(encoding='utf-8')
  types='\n'.join(definition(api,marker,True).replace('export ','',1) for marker in (
   '    export enum class SemanticTone','    export enum class MenuItemIndicator','    export struct MenuItemOptions',
   '    export struct CommandMenuOptions','    export struct CommandMenuResult'))
  adapter=definition(owner,'    void close_command_menu(')+definition(owner,'    CommandMenuResult command_menu(')
  cpp=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
import epoch.gui.command_menu;
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
namespace gui_lib=epochengine::gui_lib;
namespace gui{
using Vec2=gui_lib::Vec2;struct WidgetBounds{Vec2 position,size;};
TYPES
using CommandMenuState=gui_lib::CommandMenuState;
enum class EventType{KeyDown,Other};struct InputEvent{EventType type;int key;bool ctrl_down{},alt_down{};};
struct Context{void* windowData{};std::uintptr_t get_hwnd(){return 1;}bool has_input_focus_safe(){return focused;}bool focused=true;};
Context context;
struct Frame{Context*ctx=&context;bool menuPopupOwnsInput{},mouseDown{},pointerInputAllowed=true;Vec2 mousePos{};int mouseWheelDelta{};float deltaTime{1.0f/60.0f};std::vector<InputEvent>events;}g_frame;
bool press{},release{};std::optional<Vec2>pressOrigin;unsigned row_calls{},windows{},layers{};std::vector<std::string>labels,shortcuts;
bool left_press_available(){return press;}bool left_release_available(){return release;}
std::optional<Vec2>mouse_press_position(){return pressOrigin;}
void consume_left_press(){press=false;}void consume_left_release(){release=false;}
constexpr float kFontScale=1;float line_advance_amount(float){return 20;}float menu_item_row_height(){return 28;}
float menu_item_preferred_width(const MenuItemOptions&item){return float(item.label.size()+item.shortcut.size())*8+36;}
bool point_in_rect(Vec2 p,float x,float y,float w,float h){return p.x>=x&&p.y>=y&&p.x<x+w&&p.y<y+h;}
void begin_application_popup_input(){g_frame.menuPopupOwnsInput=true;}
void begin_top_layer(){++layers;}void end_top_layer(){--layers;}
void begin_window(std::string_view,Vec2,Vec2){++windows;}void end_window(){--windows;}
struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};void set_cursor(Vec2){}
bool menu_item_control(const MenuItemOptions&item,bool,bool,bool){++row_calls;labels.emplace_back(item.label);shortcuts.emplace_back(item.shortcut);return false;}
struct Palette{int textField{},buttonActive{};};Palette active_palette(){return{};}void draw_sprite(int,float,float,float,float){}
ADAPTER
}
int main(){using namespace gui;CommandMenuState state;
 const std::array tools{MenuItemOptions{.label="World Outliner",.command_id=101},MenuItemOptions{.label="Assistant Chat",.command_id=102}};
 const std::array workspaces{MenuItemOptions{.label="World",.command_id=201},MenuItemOptions{.label="Forest Factory",.command_id=202}};
 const std::array root{MenuItemOptions{.label="Tool Windows",.command_id=1001,.children=tools},MenuItemOptions{.label="Workspace Windows",.command_id=1002,.children=workspaces}};
 CommandMenuOptions options{.owner={{20,20},{80,28}},.viewport_size={900,600},.open_requested=true,.dismiss_on_hover_exit=true};
 auto result=command_menu(state,root,options);CHECK(result.visible&&!result.activated_id&&windows==0&&layers==0&&!g_frame.menuPopupOwnsInput);
 CHECK(row_calls==2&&shortcuts[0]==">"&&shortcuts[1]==">");options.open_requested=false;
 g_frame.events={{EventType::KeyDown,39}};result=command_menu(state,root,options);
 CHECK(state.submenu.size()==1&&result.visible&&result.captured&&g_frame.events.empty());
 CHECK(row_calls==6&&labels.back()=="Assistant Chat"&&windows==0&&layers==0&&!g_frame.menuPopupOwnsInput);
 const auto child=state.submenu[0].popup.rect;g_frame.mousePos={child.position.x+12,child.position.y+12};
 press=release=true;pressOrigin=g_frame.mousePos;
 result=command_menu(state,root,options);CHECK(result.activated_id==101&&!result.activated_index&&!result.visible&&result.captured);
 CHECK(!state.popup.open&&state.submenu.empty()&&!press&&!release&&windows==0&&layers==0);
 pressOrigin.reset();g_frame.mousePos={0,0};options.open_requested=true;result=command_menu(state,root,options);options.open_requested=false;
 g_frame.events={{EventType::KeyDown,40},{EventType::KeyDown,39}};result=command_menu(state,root,options);CHECK(state.open_submenu_id==1002);
 g_frame.events={{EventType::KeyDown,40}};result=command_menu(state,root,options);
 g_frame.events={{EventType::KeyDown,13}};result=command_menu(state,root,options);CHECK(result.activated_id==202&&!state.popup.open);
 options.open_requested=true;result=command_menu(state,root,options);options.open_requested=false;
 g_frame.events={{EventType::KeyDown,39}};result=command_menu(state,root,options);
 const auto body=state.submenu[0].popup.rect;g_frame.mousePos={body.position.x+10,body.position.y+10};
 result=command_menu(state,root,options);CHECK(result.visible&&state.submenu.size()==1); // parent hover dismissal must include child body
 context.focused=false;result=command_menu(state,root,options);CHECK(!result.visible&&state.submenu.empty());context.focused=true;
 std::array<MenuItemOptions,1>cycle{{{.label="Cycle",.command_id=700}}};cycle[0].children=cycle;
 options.open_requested=true;result=command_menu(state,cycle,options);CHECK(!result.visible&&!state.popup.open&&!g_frame.menuPopupOwnsInput);
 return 0;}
'''.replace('TYPES',types).replace('ADAPTER',adapter)
  lane=ROOT/'Artifacts/Reports/workspace-submenus-oct29/adapter'/hashlib.sha256(cpp.encode()).hexdigest()[:12];lane.mkdir(parents=True,exist_ok=True)
  source=lane/'gui.submenu_adapter_contract.cpp';source.write_text(cpp,encoding='utf-8')
  portable=ROOT/'Engine/dep/EpochGui';build=ROOT/'Artifacts/Intermediate/cmake/submenu-adapter'/lane.name
  (lane/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(SubmenuAdapter LANGUAGES CXX)
set(BUILD_TESTING OFF CACHE BOOL "Only the adapter CPU contract" FORCE)
add_subdirectory("{portable.as_posix()}" gui-dependency)
add_executable(adapter "{source.as_posix()}")
target_link_libraries(adapter PRIVATE EpochGui)
target_compile_features(adapter PRIVATE cxx_std_23)
if(MSVC)
 target_compile_options(EpochGui PRIVATE /utf-8 /UNDEBUG)
 target_compile_options(adapter PRIVATE /utf-8 /EHsc /Z7 /UNDEBUG)
else()
 target_compile_options(EpochGui PRIVATE -UNDEBUG)
 target_compile_options(adapter PRIVATE -UNDEBUG)
endif()
''',encoding='utf-8')
  commands=[configure_arguments(lane,build)]
  for config in ('Debug','Release'):commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],[str(executable(build,config,'adapter'))]]
  results=[]
  for index,command in enumerate(commands):
   ran=subprocess.run(command,cwd=lane,capture_output=True,text=True,timeout=120)
   (lane/f'phase-{index}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8')
   self.assertEqual(ran.returncode,0,(ran.stdout+ran.stderr)[-9000:]);results.append({'command':command,'exit':ran.returncode})
  (lane/'result.json').write_text(json.dumps({'passed':True,'cases':results,'scope':'Actual gui.engine command_menu adapter and API with real imported portable menu library. Input/font/native drawing are recording ports; tree paint/capture/leaf dispatch types are real. No native acceptance.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
