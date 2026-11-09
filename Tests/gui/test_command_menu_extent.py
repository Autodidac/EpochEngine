"""Compile the actual Asset descriptor inventory with the real EpochGui menu owner.

Renderer/font ports supply metrics only. Popup layout, clipping, scrolling,
selection and dismissal come from the production portable implementation.
"""
from pathlib import Path
import json
import os
import subprocess
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.gui import test_forest_preview_actions as forest_fixture

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / "Engine/dep/EpochGui"


class CommandMenuExtent(unittest.TestCase):
    def test_actual_chrome_dismissal_does_not_reopen_hovered_header(self):
        toolbar = (ROOT / "Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl").read_text(encoding="utf-8")
        start = toolbar.index("        const auto chrome_menu_route =")
        end = toolbar.index("        for (std::size_t index = 0; index < topMenus.size(); ++index)", start)
        owner = toolbar[start:end]
        code = r'''
#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
enum class TopMenu {None,File,Edit,Asset};
struct Editor {TopMenu openMenu{},suppressedMenuHover{};}editor;
struct TopMenuButton {TopMenu menu;};
const std::array topMenus{TopMenuButton{TopMenu::File},TopMenuButton{TopMenu::Edit},TopMenuButton{TopMenu::Asset}};
void frame(std::optional<std::size_t> hover,std::optional<std::size_t> press={},bool menusBlockedByModal=false){
 struct {std::optional<std::size_t> hovered_left,selected_left;}chromeResult{hover,press};
 OWNER
}
int main(){
 // Escape or a command closes File while its header remains under the mouse.
 editor={TopMenu::None,TopMenu::File};frame(1);assert(editor.openMenu==TopMenu::None);
 frame(1);assert(editor.openMenu==TopMenu::None);
 // Leaving a dismissed header never rearms automatic opening.
 frame({});assert(editor.suppressedMenuHover==TopMenu::None);
 frame(1);assert(editor.openMenu==TopMenu::None);
 frame(2);frame(3);assert(editor.openMenu==TopMenu::None);
 // An explicit click opens; subsequent hover switches while the bar is active.
 frame(1,1);assert(editor.openMenu==TopMenu::File);
 frame(2);assert(editor.openMenu==TopMenu::Edit);
 frame({});assert(editor.openMenu==TopMenu::Edit);
 editor={TopMenu::None,TopMenu::File};frame(1,1);assert(editor.openMenu==TopMenu::File);
 // Keyboard navigation has already moved to Edit; a stationary pointer over
 // File cannot immediately steal the menu back. A different header can.
 editor={TopMenu::Edit,TopMenu::File};frame(1);assert(editor.openMenu==TopMenu::Edit);
 frame(3);assert(editor.openMenu==TopMenu::Asset&&editor.suppressedMenuHover==TopMenu::None);
 editor={};frame(1,1,true);assert(editor.openMenu==TopMenu::None);
 frame(0,0);assert(editor.openMenu==TopMenu::None);
}
'''.replace("OWNER", owner)
        forest_fixture.ForestPreviewActions()._compile_and_run(code)

    def test_actual_hover_exit_delay_resets_on_menu_return(self):
        owner = (GUI / "src/epochgui/gui.command_menu.cpp").read_text(encoding="utf-8")
        actual = definition(owner, "    bool command_menu_hover_exit_due(")
        code = r'''#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
struct CommandMenuState {struct {bool open=true;}popup;float hover_exit_seconds{};};
ACTUAL
int main(){
 CommandMenuState state;
 auto tick=[&](bool inside,float dt){return command_menu_hover_exit_due(state,inside,dt,.75f);};
 assert(!tick(false,.25f));assert(!tick(false,.25f));
 // Returning to parent, header, gap or child pane restarts the same timer.
 for(int surface=0;surface<4;++surface){assert(!tick(true,.1f));assert(state.hover_exit_seconds==0);
  assert(!tick(false,.5f));}
 assert(!tick(false,std::numeric_limits<float>::quiet_NaN()));assert(!tick(false,-1));
 assert(tick(false,.25f));
 state.popup.open=false;assert(!tick(false,1));assert(state.hover_exit_seconds==0);
 state.popup.open=true;assert(!tick(false,.5f));assert(!tick(true,0));assert(!tick(false,.5f));
 assert(tick(false,.25f));
}
'''.replace("ACTUAL",actual)
        forest_fixture.ForestPreviewActions()._compile_and_run(code)

    def test_actual_asset_descriptors_drive_portable_menu_and_dispatch(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU consumer")
        tools = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = sorted((sdk / "Include").glob("*"))
        if not tools or not kits:
            self.skipTest("Installed MSVC/SDK unavailable")
        commands = (ROOT / "Engine/src/editor/application/editor.frame_command_menus.inl").read_text(encoding="utf-8")
        api = (ROOT / "Engine/modules/gui.engine.ixx").read_text(encoding="utf-8")
        asset = definition(commands, '        open_dropdown("Asset", TopMenu::Asset, [&]()') + ");"
        collector = definition(commands,"        enum class CommandMenuGroup",True)
        collector += '\nCommandMenuGroup commandMenuGroup{CommandMenuGroup::root};\n'
        collector += definition(commands, "        struct CommandMenuAction", True)
        collector += "\nstd::vector<CommandMenuAction> commandMenuActions{};\n"
        for marker in ("        auto menu_item =", "        auto open_dropdown ="):
            start = commands.index(marker)
            brace = commands.index("\n        {", start) + 9
            collector += commands[start:brace] + definition(commands[brace:], "{") + ";\n"
        start=commands.index('            const auto activated=')
        dispatch=commands[start:commands.index('            else if (menuResult.owner_navigation',start)]
        options = definition(api,"    export enum class SemanticTone",True)
        options += definition(api, "    export enum class MenuItemIndicator", True)
        options += definition(api, "    export struct MenuItemOptions", True)
        options = options.replace("export ", "")
        scratch = ROOT / "Artifacts/Intermediate/test-fixtures/command-menu-adoption"
        proof = ROOT / "Artifacts/Reports/workspace-submenus-oct29/editor-adoption"
        portable_build = ROOT / "Artifacts/Intermediate/cmake/epochgui-submenu-oct29"
        scratch.mkdir(parents=True, exist_ok=True)
        proof.mkdir(parents=True, exist_ok=True)
        configured = subprocess.run(["cmake", "-S", str(GUI), "-B", str(portable_build),
            "-G", "Visual Studio 17 2022", "-A", "x64", "-DEPOCHGUI_BUILD_TESTS=ON"],
            cwd=ROOT, capture_output=True, text=True, timeout=90)
        (proof / "portable-configure.log").write_text(configured.stdout+configured.stderr, encoding="utf-8")
        self.assertEqual(configured.returncode, 0, configured.stdout+configured.stderr)
        code = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
import epoch.gui.command_menu;
namespace gui_lib=epochengine::gui_lib;
namespace gui {
using Vec2=gui_lib::Vec2;
OPTIONS
using CommandMenuState=gui_lib::CommandMenuState;
void close_command_menu(CommandMenuState& state){gui_lib::close_command_menu(state);}
float preferred_width(const MenuItemOptions& o){return o.label=="Import Mesh..."?470.f:
 o.label=="Reload Meshes"?460.f:static_cast<float>(o.label.size()+o.shortcut.size())*8.f+38.f;}
}
enum class TopMenu {None,File,Edit,Asset,Window,Tools,Tasks,Help};
enum class EditorWorkspaceTab {Assets};
enum class EditorMainSurface {Assets,Scene};
enum class SurfaceEditAction {duplicate_selection,delete_selection};
struct EditorEntity {std::optional<int> meshAsset;bool meshLoadAttempted{};std::optional<int> meshSnapshot,meshSnapshotFrame;};
struct Editor {TopMenu openMenu{},commandMenuOwner{},suppressedMenuHover{};
 gui::CommandMenuState commandMenuState;EditorWorkspaceTab workspaceTab{};std::string projectRoot,projectStatus;
 bool showPackageManagerModal{},meshImportOpen{},sceneInteractionDirty{};unsigned assetWorkspaceSection{};
 std::vector<EditorEntity> entities;}editor;
struct {void* get(){return nullptr;}}ctx;
struct {bool allow_package_management{},allow_entity_authoring{};}application;
struct Availability {bool enabled{true};};
Availability resolve_surface_edit(Editor&,SurfaceEditAction){return {};}
bool execute_surface_edit(Editor&,void*,SurfaceEditAction){return true;}
unsigned added{},surfaceCalls{};
void open_editor_surface(EditorMainSurface,std::string_view){assert(editor.openMenu==TopMenu::None&&!editor.commandMenuState.popup.open);++surfaceCalls;}
void add_entity(Editor&,std::string_view kind){assert(kind=="cube"&&editor.openMenu==TopMenu::None&&!editor.commandMenuState.popup.open);++added;}
void push_editor_log(Editor&,std::string){}
bool modal_visible_now(){return false;}
int main(){
 for(float rowHeight:{24.f,42.f,64.f})for(bool package:{false,true})for(bool authoring:{false,true}){
  editor={};editor.openMenu=TopMenu::Asset;application={package,authoring};added=surfaceCalls=0;
  COLLECTOR
  ASSET
  assert(commandMenuActions.size()==1u+(package?1u:0u)+(authoring?7u:0u));
  std::vector<gui_lib::CommandMenuItemSpec> specs;
  for(const auto& command:commandMenuActions){auto o=command.options;o.label=command.label;o.shortcut=command.shortcut;
   assert(o.command_id!=0);specs.push_back({.id=o.command_id,.label=o.label,.shortcut=o.shortcut,
    .preferred_width=gui::preferred_width(o),.enabled=o.enabled,.separator_before=o.separator_before});}
  // The actual collector owns its temporary labels, including the dynamic text.
  assert(commandMenuActions.front().label=="Open Asset Browser");
  gui_lib::CommandMenuOptions config{.owner={{40,40},{80,30}},.viewport_size={1200,1500},.row_height=rowHeight};
  const auto open=[] {gui_lib::CommandMenuInput input;input.popup.open_requested=true;return input;};
  auto plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,open());
  assert(plan.valid&&plan.popup.visible&&plan.rows.size()==specs.size()&&!plan.scrollbar);
  assert(std::abs(plan.content_height-specs.size()*rowHeight)<.01f);
  for(const auto& spec:specs)assert(plan.content_clip.size.x+.01f>=spec.preferred_width);
  // Small viewports really overflow; successive wheels retain range and clamp.
  gui_lib::close_command_menu(editor.commandMenuState);config.viewport_size.y=150;
  plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,open());
  if(plan.maximum_scroll_offset>0){assert(plan.scrollbar);
   auto pointer=gui_lib::Vec2{plan.content_clip.position.x+2,plan.content_clip.position.y+2};
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{.popup={.mouse_position=pointer},.wheel_delta=-20});
   assert(plan.scroll_offset>0);const auto prior=plan.scroll_offset;
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{.popup={.mouse_position=pointer},.wheel_delta=-20});
   assert(plan.scroll_offset>=prior);
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{.popup={.mouse_position=pointer},.wheel_delta=-100000});
   assert(std::abs(plan.scroll_offset-plan.maximum_scroll_offset)<.01f);
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{.popup={.mouse_position=pointer},.wheel_delta=100000});assert(plan.scroll_offset==0);
  }
  if(authoring){
   gui_lib::close_command_menu(editor.commandMenuState);config.viewport_size.y=1500;
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,open());
   const auto disabled=std::find_if(specs.begin(),specs.end(),[](const auto& item){return item.label=="Import Mesh...";});
   assert(disabled!=specs.end()&&!disabled->enabled);
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{.shortcut_id=disabled->id});assert(!plan.activated_index&&plan.popup.visible);
   const auto target=std::find_if(specs.begin(),specs.end(),[](const auto& item){return item.label=="Add Static Mesh";});assert(target!=specs.end());
   auto menuResult=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{.shortcut_id=target->id});
   assert(menuResult.activated_index&&!editor.commandMenuState.popup.open);const auto hoveredMenu=TopMenu::None;
   DISPATCH
   assert(added==1&&surfaceCalls==1&&editor.openMenu==TopMenu::None);
   plan=gui_lib::update_command_menu(editor.commandMenuState,specs,config,{});assert(!plan.activated_index);
  }
 }
}
'''
        for key, value in (("OPTIONS", options), ("COLLECTOR", collector), ("ASSET", asset), ("DISPATCH", dispatch)):
            code = code.replace(key, value)
        cpp = scratch / "consumer.cpp"
        cpp.write_text(code, encoding="utf-8")
        tool, kit = tools[-1], kits[-1]
        completed = []
        for configuration in ("Debug", "Release"):
            portable = subprocess.run(["cmake", "--build", str(portable_build), "--config", configuration,
                "--target", "EpochGui", "--parallel", "1"], cwd=ROOT, capture_output=True, text=True, timeout=180)
            (proof / (configuration+"-portable-build.log")).write_text(portable.stdout+portable.stderr, encoding="utf-8")
            self.assertEqual(portable.returncode, 0, portable.stdout+portable.stderr)
            module_dir = portable_build / "EpochGui.dir" / configuration
            library = portable_build / configuration / "EpochGui.lib"
            binary = scratch / (configuration + ".exe")
            args = [str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/permissive-", "/EHsc", "/UNDEBUG",
                    "/MDd" if configuration == "Debug" else "/MD", "/D_DEBUG" if configuration == "Debug" else "/UNDEBUG",
                    "/Od" if configuration == "Debug" else "/O2", "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"),
                    "/reference", "epoch.gui="+str(module_dir/"epoch.gui.ixx.ifc"),
                    "/reference", "epoch.gui.command_menu="+str(module_dir/"epoch.gui.command_menu.ixx.ifc"),
                    "/Fo" + str(scratch / (configuration+".obj")), "/Fe" + str(binary), str(cpp), str(library),
                    "/link", "/LIBPATH:" + str(tool / "lib/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"),
                    "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")]
            built = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=90)
            (proof / (configuration + "-compile.log")).write_text(built.stdout + built.stderr, encoding="utf-8")
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            completed.append({"configuration": configuration, "compile_exit": built.returncode, "run_exit": ran.returncode})
            (proof / "result.json").write_text(json.dumps({"completed": completed,
                "scope": "Actual Asset descriptor/callback inventory + real imported epoch.gui.command_menu module/library + actual close-before-dispatch block; no renderer/native claim"}, indent=2)+"\n", encoding="utf-8")
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
