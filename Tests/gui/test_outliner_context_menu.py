"""Actual Outliner popup integration + compiled portable menu; recording draw ports."""
from pathlib import Path
import json, os, subprocess, time, unittest
from Tests.platform.test_child_close_router import definition

ROOT=Path(__file__).resolve().parents[2]

class OutlinerContextMenu(unittest.TestCase):
 def test_actual_popup_actions_capture_and_dismissal_debug_release(self):
  if os.name!='nt':self.skipTest('Installed MSVC CPU consumer')
  source=(ROOT/'Engine/src/editor/application/editor.frame_outliner.inl').read_text(encoding='utf-8')
  state=(ROOT/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
  api=(ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
  buttons=(ROOT/'Engine/src/epochgui/controls/gui.buttons_images.inl').read_text(encoding='utf-8')
  selection=(ROOT/'Engine/src/editor/application/editor.application_scene_selection.inl').read_text(encoding='utf-8')
  bootstrap=(ROOT/'Engine/src/editor/application/editor.frame_bootstrap.inl').read_text(encoding='utf-8')
  commands=(ROOT/'Engine/src/editor/application/editor.frame_command_menus.inl').read_text(encoding='utf-8')
  app_options=definition(commands,'gui::CommandMenuOptions{')
  start=commands.index('            const auto activated=')
  app_dispatch=commands[start:commands.index('            else if (menuResult.owner_navigation',start)]
  app_action=definition(commands,'        enum class CommandMenuGroup',True)+definition(commands,'        struct CommandMenuAction',True)
  latch_begin=bootstrap.index('        if (editor.outlinerContextMenu.menu.popup.open')
  latch=bootstrap[latch_begin:bootstrap.index('        if (modalVisible || pointerGrabActive)',latch_begin)]
  prepare=definition(source,'        const auto prepare_outliner_context_menu =')+';'
  command_ids=definition(source,'        enum class OutlinerCommand',True)
  route=definition(source,'        const auto route_selected_hierarchy_action =')+';'
  toolbar=source[source.index('        const gui::Vec2 outlinerToolbarOrigin ='):source.index('        std::vector<editor_hierarchy::EntityRecord> hierarchyEntities{};')]

  render=definition(source,'        const auto render_outliner_context_menu =')+';'
  viewport=(ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
  preview=definition(viewport,'            const auto render_preview_context_menu =')+';' 
  adapter=definition(buttons,'    CommandMenuResult command_menu(')
  declarations='\n'.join(definition(api,marker,True).replace('export ','',1) for marker in
    ('    export enum class MenuItemIndicator','    export struct MenuItemOptions','    export struct CommandMenuOptions','    export struct CommandMenuResult','    export enum class ButtonIcon','    export struct InlineButtonSpec',
     '    export enum class InlineButtonSizing','    export enum class InlineButtonAlignment','    export struct InlineButtonRowOptions'))
  code=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <string>
#include <string_view>
#include <vector>
import epoch.gui.command_menu;
#define EPOCHGUI_TEST_MODULE_IMPORT
#define EPOCHGUI_TEST_NO_MAIN
#include "POPUP_TEST_PATH"
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
namespace gui_lib=epochengine::gui_lib;
namespace input{enum class MouseButton{MouseLeft,MouseMiddle,MouseRight};enum class Key{LeftAlt,RightAlt};enum class Action{MoveForward,MoveBackward,MoveLeft,MoveRight,MoveUp,MoveDown,RollLeft,RollRight,LookLeft,LookRight,LookUp,LookDown};}
namespace core{enum class ScenePreviewMode{Editor,None};}
struct Context {bool focus{true},right{},middle{},left{},alt{},navigation{};void*windowData{};void*get_hwnd(){return this;}bool has_input_focus_safe(){return focus;}
 bool is_mouse_button_held_safe(input::MouseButton b){return b==input::MouseButton::MouseRight?right:b==input::MouseButton::MouseLeft?left:middle;}
 bool is_key_held_safe(input::Key){return alt;}bool action_held_safe(input::Action){return navigation;}};
namespace gui{
using Vec2=gui_lib::Vec2;using SemanticTone=gui_lib::SemanticTone;using CommandMenuState=gui_lib::CommandMenuState;
struct WidgetBounds{Vec2 position{},size{};};DECLARATIONS
enum class EventType{KeyDown};struct InputEvent{EventType type{EventType::KeyDown};bool ctrl_down{},alt_down{};int key{};};
struct Frame {Context*ctx{};Vec2 mousePos{};bool mouseDown{},pointerInputAllowed{true},menuPopupOwnsInput{},blocked{},press{},release{},right{};float mouseWheelDelta{},deltaTime{1.0f/60.0f};std::vector<InputEvent>events;}g_frame;
constexpr float kFontScale=1;float line_advance_amount(float){return 18;}float menu_item_row_height(){return 24;}
float menu_item_preferred_width(const MenuItemOptions&i){return i.label.size()*8.f+24;}
bool point_in_rect(Vec2 p,float x,float y,float w,float h){return p.x>=x&&p.y>=y&&p.x<x+w&&p.y<y+h;}
bool left_press_available(){return g_frame.press&&(!g_frame.blocked||g_frame.menuPopupOwnsInput);}
bool left_release_available(){return g_frame.release&&(!g_frame.blocked||g_frame.menuPopupOwnsInput);}
std::optional<Vec2>mouse_press_position(){return g_frame.mousePos;}
void consume_left_press(){g_frame.press=false;}void consume_left_release(){g_frame.release=false;}
void begin_application_popup_input(){g_frame.menuPopupOwnsInput=true;}void block_background_for_popup(){g_frame.blocked=true;}
void clear_modal_input_capture(){} Vec2 mouse_position(){return g_frame.mousePos;}bool was_mouse_right_pressed(){return g_frame.right&&!g_frame.blocked;}
void close_command_menu(CommandMenuState&s){gui_lib::close_command_menu(s);}
unsigned layers{},draws{};bool drawnOnTop{},enabled{};WidgetBounds lastBounds{};std::string drawnLabel;
void begin_top_layer(){++layers;}void end_top_layer(){--layers;}void begin_window(std::string_view,Vec2 p,Vec2 s){lastBounds={p,s};}void end_window(){}
struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};
Vec2 cursor{10,40};std::string toolbar_clicked,filter_placeholder;std::optional<std::string>filter_typed;
Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 p){cursor=p;}
enum class TextInputKeyboardPolicy{focused};
void edit_box(std::string& text,Vec2,std::size_t,bool,TextInputKeyboardPolicy,bool,std::string_view placeholder){filter_placeholder=placeholder;if(filter_typed&&!g_frame.blocked){text=*filter_typed;filter_typed.reset();}}
std::optional<std::size_t>inline_button_row(std::span<const InlineButtonSpec> items,float,float,const InlineButtonRowOptions&){
 if(toolbar_clicked==items[0].label&&items[0].enabled&&!g_frame.blocked){toolbar_clicked.clear();return 0u;}return {};}
bool menu_item_control(const MenuItemOptions&i,bool,bool,bool){++draws;drawnOnTop=layers>0;drawnLabel=i.label;enabled=i.enabled;return false;}
struct Palette{int textField{},buttonActive{};};Palette active_palette(){return {};}
void draw_sprite(int,float,float,float,float){}
ADAPTER
}
enum class TopMenu{None,File};enum class OutlinerToolTab{World,Gui,Assets};
namespace editor_workspace_commands{enum class Surface{world,gui};}
namespace editor_hierarchy{
using NodeId=std::uint64_t;enum class Action{select,focus,duplicate_entity,delete_entity,deselect};
struct Route{bool ok=true;int code{};explicit operator bool()const{return ok;}};
std::string_view code_name(int){return "rejected";}
}
struct Hierarchy{bool allowed=true;unsigned routes{};editor_hierarchy::NodeId node_for_entity(std::uint64_t id){return id;}
 editor_hierarchy::Route route(editor_hierarchy::Action,std::uint64_t){++routes;return {allowed};}};

enum class EditorMainSurface{Scene,Project};
struct EditorState{
 EditorMainSurface mainSurface{EditorMainSurface::Scene};core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};
 OUTLINER_STATE;
 struct{std::uint64_t frame_index{};}timeSnapshot;
 struct Lifecycle{std::uint64_t generation=1;struct Stamp{std::uint64_t generation{1};};struct Evidence{struct{Stamp selected_project;}generations;};Evidence evidence(){return {{{generation}}};}}projectLifecycle;
 Hierarchy worldHierarchy;std::string worldHierarchyStatus,worldHierarchyFilter;
 struct Document{bool undo=true,redo=true;bool can_undo()const{return undo;}bool can_redo()const{return redo;}}sceneDocument;
 TopMenu openMenu{},commandMenuOwner{},suppressedMenuHover{};gui::CommandMenuState commandMenuState;std::string activeRuntimeScene{"scene-a"};bool selected{true};unsigned deselections{};
 unsigned selectedEntity{4};std::uint64_t selectedEntityId{7};std::vector<int>selectedEntityIds{7};bool sceneDragActive{},sceneDragHasPlaneHit{},sceneMarqueeActive{},textureBrushActive{};
 unsigned sceneDragEntity{};std::uint64_t sceneDragEntityId{};std::vector<int>sceneDragStartPositions;int guiSelectedWidget{},guiContentEditorWidget{};
 int guiDocumentSelection{42},tileMapSelection{27};
}editor;
namespace scene{constexpr std::uint64_t kInvalidSceneObjectId=0;}
CLEAR_SELECTION
bool editor_has_selection(const EditorState&e){return e.selectedEntityId!=0;}
std::string_view editor_scene_identity(const EditorState&e){return e.activeRuntimeScene;}
void push_editor_log(EditorState&e,std::string_view){++e.deselections;}
std::shared_ptr<Context>ctx=std::make_shared<Context>();Context&context=*ctx;OutlinerToolTab pane_tab{OutlinerToolTab::World};
struct{bool allow_entity_authoring=true;}application;
editor_workspace_commands::Surface surface=editor_workspace_commands::Surface::world;
auto workspace_command_surface(const EditorState&){return surface;}
enum class SurfaceEditAction{undo,redo,duplicate_selection,delete_selection,focus_selection};
std::vector<std::string>created;std::vector<SurfaceEditAction>edits;unsigned cube_batch{},view_opens{},focus_calls{},ambient_opens{};
void add_entity(EditorState&,std::string_view archetype){created.emplace_back(archetype);}
bool add_cube_batch(EditorState&,unsigned count){cube_batch=count;return true;}
void open_editor_surface(EditorMainSurface,std::string_view){++view_opens;surface=editor_workspace_commands::Surface::world;}
bool execute_surface_edit(EditorState&,Context*,SurfaceEditAction action){edits.push_back(action);return true;}
void handle_scene_tool(EditorState&,Context*,std::string_view){++focus_calls;}
void set_pane_shown(std::string_view,bool){++ambient_opens;}bool pane_route_is_detached(std::string_view){return false;}
int pane_dock_region(std::string_view){return 0;}void set_active_pane(int,std::string_view){}
COMMAND_IDS

gui::Vec2 pane_pos{10,10},pane_size{200,300},outlinerScrollStart{10,40};float w=640,h=480;
struct{bool scene_input_captured{};std::array<std::string,2>companion_pane_activations;gui::WidgetBounds scene_viewport{{250,100},{300,200}};}result;bool modal{},floating{},reserved{},sceneFits=true,previewFrame{};
namespace previewgrid{std::uint64_t revision=1;std::uint64_t camera_revision_for(void*){return revision;}}
bool editor_point_in_rect(gui::Vec2 p,gui::Vec2 origin,gui::Vec2 size){return gui::point_in_rect(p,origin.x,origin.y,size.x,size.y);}
bool editor_project_viewport_input_reserved(const EditorState&){return reserved;}
bool modal_visible_now(){return modal;}bool floating_gui_visible(){return floating;}
APP_ACTION
std::vector<CommandMenuAction>commandMenuActions;
std::vector<gui::MenuItemOptions>commandMenuItems;
gui::CommandMenuResult app_frame(gui::Vec2 pointer,bool menuOwnerChanged=false,bool press=false,bool release=false,int key=0){
 gui::g_frame.ctx=&context;gui::g_frame.mousePos=pointer;gui::g_frame.press=press;gui::g_frame.release=release;gui::g_frame.mouseDown=press;gui::g_frame.blocked=true;gui::g_frame.events.clear();if(key)gui::g_frame.events.push_back({.key=key});
 const std::optional<gui::WidgetBounds>ownerBounds{{{20,8},{68,32}}};
 return gui::command_menu(editor.commandMenuState,commandMenuItems,APP_OPTIONS);
}
void dispatch_app(const gui::CommandMenuResult&menuResult){const auto hoveredMenu=TopMenu::None;APP_DISPATCH}
void frame(bool right=false,bool press=false,bool release=false,int key=0,bool consumedText=false){
 ++editor.timeSnapshot.frame_index;gui::g_frame.ctx=&context;gui::g_frame.right=right;gui::g_frame.press=press;gui::g_frame.release=release;gui::g_frame.mouseDown=press;gui::g_frame.blocked=false;gui::g_frame.events.clear();if(key)gui::g_frame.events.push_back({.key=key});result={};
 LATCH
 ROUTE
 PREPARE
 prepare_outliner_context_menu();
 if(pane_tab==OutlinerToolTab::World){const float outlinerWidth=188;TOOLBAR}
 if(consumedText)gui::g_frame.right=false;
 RENDER
 render_outliner_context_menu();
 PREVIEW
 if(previewFrame)render_preview_context_menu();
}
int main(){
 CHECK(popup_layout_contract()==0);
 // The actual viewport lambda works with the Outliner hidden and only opens on
 // a plain right-click release. Fly drags, navigation and stale owners refuse.
 previewFrame=true;pane_tab=OutlinerToolTab::Gui;gui::g_frame.mousePos={300,150};
 context.right=true;frame(true);CHECK(!editor.outlinerContextMenu.menu.popup.open&&!result.scene_input_captured);
 context.right=false;frame();CHECK(editor.outlinerContextMenu.menu.popup.open&&result.scene_input_captured&&gui::drawnLabel=="Deselect");
 const auto previewBounds=gui::lastBounds;gui::g_frame.mousePos={previewBounds.position.x+12,previewBounds.position.y+12};frame(false,true);frame(false,false,true);
 CHECK(editor.selectedEntityId==0&&editor.selectedEntityIds.empty()&&!editor.outlinerContextMenu.menu.popup.open&&editor.deselections==1);
 CHECK(editor.guiDocumentSelection==42&&editor.tileMapSelection==27);
 // Disabled Deselect cannot mutate empty selection.
 gui::g_frame.mousePos={300,150};context.right=true;frame(true);context.right=false;frame();CHECK(!gui::enabled);frame(false,false,false,27);
 editor.selectedEntityId=7;editor.selectedEntityIds={7};editor.deselections=0;
 const auto noMenu=[&](){CHECK(!editor.outlinerContextMenu.menu.popup.open);return 0;};
 context.right=true;frame(true);gui::g_frame.mousePos.x+=20;frame();gui::g_frame.mousePos.x-=20;context.right=false;frame();CHECK(noMenu()==0&&editor.selectedEntityId==7);
 context.right=true;frame(true);context.navigation=true;frame();context.navigation=false;context.right=false;frame();CHECK(noMenu()==0);
 context.right=true;frame(true);++previewgrid::revision;frame();context.right=false;frame();CHECK(noMenu()==0);
 context.right=true;context.alt=true;frame(true);context.alt=false;context.right=false;frame();CHECK(noMenu()==0);
 context.right=true;frame(true);context.focus=false;frame();context.focus=true;context.right=false;frame();CHECK(noMenu()==0);
 context.right=true;frame(true);++editor.projectLifecycle.generation;context.right=false;frame();CHECK(noMenu()==0);
 context.right=true;frame(true);reserved=true;context.right=false;frame();CHECK(noMenu()==0);reserved=false;
 context.right=true;frame(true);editor.mainSurface=EditorMainSurface::Project;context.right=false;frame();CHECK(noMenu()==0);editor.mainSurface=EditorMainSurface::Scene;
 context.right=true;frame(true);context.right=false;frame();CHECK(editor.outlinerContextMenu.menu.popup.open);editor.activeRuntimeScene="scene-other";frame();CHECK(noMenu()==0);editor.activeRuntimeScene="scene-a";
 previewFrame=false;pane_tab=OutlinerToolTab::World;editor.outlinerContextMenu={};

 // The real toolbar creates a menu request bound to this scene/project, without a direct mutation.
 gui::filter_typed="Water";frame();CHECK(editor.worldHierarchyFilter=="Water"&&gui::filter_placeholder=="Filter objects...");
 const auto pick=[&](std::size_t index){const auto box=gui::lastBounds;gui::g_frame.mousePos={box.position.x+12,box.position.y+12+24*static_cast<float>(index)};frame(false,true);frame(false,false,true);};
 const auto toolbar=[&](std::string_view name){gui::toolbar_clicked=name;frame();};
 toolbar("Create object");CHECK(editor.outlinerContextMenu.menu.popup.open&&created.empty()&&result.scene_input_captured);
 pick(0);CHECK(created==std::vector<std::string>{"cube"}&&!editor.outlinerContextMenu.menu.popup.open);
 for(const auto pair:std::array{std::pair{1u,"ground"},std::pair{2u,"light"},std::pair{3u,"water"},std::pair{6u,"spawn"},std::pair{7u,"camera"}}){toolbar("Create object");pick(pair.first);CHECK(created.back()==pair.second);}
 toolbar("Create object");pick(4);CHECK(cube_batch==4);toolbar("Create object");pick(5);CHECK(cube_batch==8);
 const auto created_count=created.size();surface=editor_workspace_commands::Surface::gui;
 toolbar("Create object");pick(0);CHECK(created.size()==created_count&&editor.outlinerContextMenu.menu.popup.open);
 pick(8);CHECK(view_opens==1&&surface==editor_workspace_commands::Surface::world);
 toolbar("More actions");pick(2);CHECK(edits.back()==SurfaceEditAction::undo);
 toolbar("More actions");pick(3);CHECK(edits.back()==SurfaceEditAction::redo);
 toolbar("More actions");pick(4);CHECK(edits.back()==SurfaceEditAction::duplicate_selection);
 toolbar("More actions");pick(5);CHECK(edits.back()==SurfaceEditAction::delete_selection);
 toolbar("More actions");pick(0);CHECK(edits.back()==SurfaceEditAction::focus_selection);
 toolbar("More actions");pick(6);CHECK(ambient_opens==1);
 const auto edit_count=edits.size();editor.worldHierarchy.allowed=false;
 toolbar("More actions");pick(4);CHECK(edits.size()==edit_count&&editor.worldHierarchyStatus.find("rejected")!=std::string::npos);
 editor.worldHierarchy.allowed=true;editor.sceneDocument.undo=false;
 toolbar("More actions");pick(2);CHECK(edits.size()==edit_count);frame(false,false,false,27);editor.sceneDocument.undo=true;
 application.allow_entity_authoring=false;
 toolbar("Create object");CHECK(!editor.outlinerContextMenu.menu.popup.open&&created.size()==created_count);
 gui::toolbar_clicked.clear();toolbar("More actions");pick(4);CHECK(edits.size()==edit_count);frame(false,false,false,27);
 toolbar("More actions");pick(0);CHECK(focus_calls==1&&edits.size()==edit_count);application.allow_entity_authoring=true;
 toolbar("Create object");++editor.projectLifecycle.generation;frame();CHECK(!editor.outlinerContextMenu.menu.popup.open&&created.size()==created_count);
 // Existing right-click Deselect remains independent of all creation/history commands.
 gui::g_frame.mousePos={30,70};frame(true);CHECK(editor.outlinerContextMenu.menu.popup.open&&result.scene_input_captured);
 CHECK(gui::drawnOnTop&&gui::drawnLabel=="Deselect"&&gui::enabled&&gui::layers==0);
 CHECK(gui::lastBounds.position.x>=0&&gui::lastBounds.position.y>=0&&gui::lastBounds.position.x+gui::lastBounds.size.x<=w&&gui::lastBounds.position.y+gui::lastBounds.size.y<=h);
 const auto bounds=gui::lastBounds;gui::g_frame.mousePos={bounds.position.x+12,bounds.position.y+12};frame(false,true);frame(false,false,true);
 CHECK(editor.selectedEntityId==0&&editor.selectedEntityIds.empty()&&editor.deselections==1&&!editor.outlinerContextMenu.menu.popup.open);
 CHECK(editor.guiDocumentSelection==42&&editor.tileMapSelection==27);
 frame(false,false,false,13);CHECK(editor.deselections==1); // no repeat after close.
 gui::g_frame.mousePos={30,70};frame(true);CHECK(!gui::enabled&&editor.outlinerContextMenu.menu.popup.open);frame(false,false,false,13);CHECK(editor.deselections==1);
 frame(false,false,false,27);CHECK(!editor.outlinerContextMenu.menu.popup.open);
 // Global right-press retirement happens before background capture, so a
 // different pane can own its new right click; a hidden owner cannot trap input.
 gui::g_frame.mousePos={30,70};frame(true);pane_tab=OutlinerToolTab::Gui;frame(true);CHECK(!editor.outlinerContextMenu.menu.popup.open&&!gui::g_frame.blocked);pane_tab=OutlinerToolTab::World;
 frame(true);editor.timeSnapshot.frame_index+=3;pane_tab=OutlinerToolTab::Gui;frame();CHECK(!editor.outlinerContextMenu.menu.popup.open&&!gui::g_frame.blocked);pane_tab=OutlinerToolTab::World;
 // Actual top-menu owner options and actual popup adapter: move diagonally
 // beyond the narrow header before crossing the gap into the wider File body.
 unsigned saves{};for(const auto label:{"Open Launcher","Open Project...","Close Project","Save Project","Settings","Exit"}){
  commandMenuItems.push_back({.label=label,.command_id=commandMenuItems.size()+1});
  commandMenuActions.push_back({.options={.command_id=commandMenuItems.size()},.invoke=[&saves,label](){if(std::string_view{label}=="Save Project")++saves;}});
 }
 editor.openMenu=editor.commandMenuOwner=TopMenu::File;
 auto menuResult=app_frame({54,22},true);CHECK(menuResult.visible);
 for(const auto p:std::array{gui::Vec2{100,35},gui::Vec2{120,43},gui::Vec2{130,60},gui::Vec2{130,136}}){menuResult=app_frame(p);CHECK(menuResult.visible);}
 menuResult=app_frame({130,136},false,true);CHECK(menuResult.visible);
 menuResult=app_frame({130,136},false,false,true);CHECK(menuResult.activated_index==std::optional<std::size_t>{3});dispatch_app(menuResult);
 CHECK(saves==1&&editor.openMenu==TopMenu::None&&!editor.commandMenuState.popup.open);
 app_frame({54,22},true);menuResult=app_frame({500,400},false,true);CHECK(menuResult.closed&&menuResult.captured);
 app_frame({54,22},true);menuResult=app_frame({54,22},false,false,false,27);CHECK(menuResult.closed&&!editor.commandMenuState.popup.open);
 app_frame({54,22},true);context.focus=false;menuResult=app_frame({100,80});CHECK(menuResult.closed);context.focus=true;
 // Tasks has two ordinary rows: narrow viewport constrains body and recorded
 // row geometry without introducing phantom commands or changing indices.
 commandMenuItems={{.label="Open Tasks & Threads",.command_id=10},{.label="Tasks Diagnostics",.command_id=11}};
 w=160;h=160;menuResult=app_frame({54,22},true);CHECK(menuResult.visible&&menuResult.bounds.position.x>=0&&menuResult.bounds.position.y>=0);
 CHECK(menuResult.bounds.position.x+menuResult.bounds.size.x<=w&&menuResult.bounds.position.y+menuResult.bounds.size.y<=h);
 menuResult=app_frame({40,94},false,true);menuResult=app_frame({40,94},false,false,true);CHECK(menuResult.activated_index==std::optional<std::size_t>{1});
 editor.selectedEntityId=7;frame(true);gui::g_frame.mousePos={500,400};frame(false,true);CHECK(!editor.outlinerContextMenu.menu.popup.open&&result.scene_input_captured&&editor.selectedEntityId==7);
 gui::g_frame.mousePos={30,70};frame(true);context.focus=false;frame();CHECK(!editor.outlinerContextMenu.menu.popup.open&&editor.selectedEntityId==7);context.focus=true;
 frame(true);editor.activeRuntimeScene="scene-b";frame();CHECK(!editor.outlinerContextMenu.menu.popup.open&&editor.selectedEntityId==7);
 frame(true,false,false,0,true);CHECK(!editor.outlinerContextMenu.menu.popup.open); // text popup owns its right press.
 pane_tab=OutlinerToolTab::Gui;frame(true);CHECK(!editor.outlinerContextMenu.menu.popup.open&&editor.selectedEntityId==7);pane_tab=OutlinerToolTab::World;
 modal=true;frame(true);CHECK(!editor.outlinerContextMenu.menu.popup.open);modal=false;floating=true;frame(true);CHECK(!editor.outlinerContextMenu.menu.popup.open);floating=false;
 gui::g_frame.mousePos={(std::numeric_limits<float>::quiet_NaN)(),70};frame(true);CHECK(!editor.outlinerContextMenu.menu.popup.open);
 gui::g_frame.mousePos={205,305};frame(true);CHECK(editor.outlinerContextMenu.menu.popup.open&&gui::lastBounds.position.x+gui::lastBounds.size.x<=w);
 frame(false,false,false,27);CHECK(!editor.outlinerContextMenu.menu.popup.open);
}
'''
  menu_state=definition(state,'            struct OutlinerContextMenu final')+' outlinerContextMenu{}'
  code=code.replace('DECLARATIONS',declarations).replace('COMMAND_IDS',command_ids).replace('ROUTE',route).replace('TOOLBAR',toolbar).replace('ADAPTER',adapter).replace('OUTLINER_STATE',menu_state).replace('CLEAR_SELECTION',definition(selection,'        void clear_editor_selection(')).replace('LATCH',latch).replace('PREPARE',prepare).replace('RENDER',render).replace('PREVIEW',preview).replace('APP_ACTION',app_action).replace('APP_OPTIONS',app_options).replace('APP_DISPATCH',app_dispatch)
  code=code.replace('POPUP_TEST_PATH',(ROOT/'Engine/dep/EpochGui/tests/popup_layout_tests.cpp').as_posix())
  proof=ROOT/'Artifacts/Reports/outliner-context-menu-oct29'/str(time.time_ns());build=proof/'build';proof.mkdir(parents=True,exist_ok=True)
  (proof/'consumer.cpp').write_text(code,encoding='utf-8')
  gui_root=ROOT/'Engine/dep/EpochGui'
  (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(OutlinerMenu LANGUAGES CXX)
add_executable(consumer consumer.cpp "{gui_root.as_posix()}/src/epochgui/gui.command_menu.cpp" "{gui_root.as_posix()}/src/epochgui/popup_layout.cpp" "{gui_root.as_posix()}/src/epochgui/gui.scrollbar_layout.cpp")
target_compile_features(consumer PRIVATE cxx_std_23)
target_compile_options(consumer PRIVATE /utf-8 /EHsc /Z7)
target_sources(consumer PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{gui_root.as_posix()}/modules" FILES "{gui_root.as_posix()}/modules/epoch.gui.ixx" "{gui_root.as_posix()}/modules/epoch.gui.command_menu.ixx")
''',encoding='utf-8')
  commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64','-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']]
  for config in ('Debug','Release'):commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],[str(build/config/'consumer.exe')]]
  results=[];started=time.monotonic()
  for index,command in enumerate(commands):
   run=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=120,env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'));(proof/f'phase-{index}.log').write_text(run.stdout+run.stderr,encoding='utf-8');results.append({'command':command,'exit':run.returncode});self.assertEqual(run.returncode,0,(run.stdout+run.stderr)[-6000:]+f' exit={run.returncode}')
  (proof/'result.json').write_text(json.dumps({'passed':True,'elapsed_seconds':time.monotonic()-started,'results':results,'scope':'Actual Editor opener/dispatch/clear selection, GUI adapter and compiled portable menu with recording drawing/input ports; no native pixels.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
