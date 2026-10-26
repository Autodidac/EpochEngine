"""Execute real pane routing/storage and 2D document selection without a window."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
class WorkspaceCompanionRouting(unittest.TestCase):
 def test_transitions_preserve_docking_and_gui_document_identity(self):
  base=ROOT/'Engine/src/editor/application'
  state=(base/'editor.application_state.inl').read_text(encoding='utf-8')
  bootstrap=(base/'editor.frame_bootstrap.inl').read_text(encoding='utf-8')
  workspace=(base/'editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
  project=(base/'editor.application_project_documents.inl').read_text(encoding='utf-8')
  app=(ROOT/'Engine/modules/editor.application.ixx').read_text(encoding='utf-8')
  types='\n'.join(definition(state,marker,True) for marker in ('        enum class EditorToolDockRegion','        enum class OutlinerToolTab','        enum class EditorToolPane'))
  types+='\nconstexpr std::size_t kEditorToolPaneCount=static_cast<std::size_t>(EditorToolPane::Count);\n'
  types+='\n'.join(definition(state,marker) for marker in ('        [[nodiscard]] constexpr std::size_t tool_pane_index(', '        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>\n            editor_tool_pane_routes()', '        [[nodiscard]] constexpr std::array<EditorToolDockRegion, kEditorToolPaneCount>\n            default_tool_pane_regions()', '        [[nodiscard]] constexpr std::optional<EditorToolPane>\n            editor_tool_pane_from_route(', '        [[nodiscard]] constexpr bool is_outliner_tool_pane(', '        [[nodiscard]] constexpr bool is_inspector_tool_pane(', '        [[nodiscard]] constexpr bool is_console_tool_pane('))
  app_types='\n'.join(definition(app,marker,True) for marker in ('    enum class EditorApplicationKind','    enum class EditorApplicationSurface','    struct EditorApplicationPanes'))
  core=(ROOT/'Engine/modules/editor.core.ixx').read_text(encoding='utf-8')
  frame_types='\n'.join(definition(core,marker,True).replace('export ','',1) for marker in ('    export enum class EditorCommand','    export struct EditorFrameResult'))
  dedicated=definition(app,'    [[nodiscard]] constexpr bool\n        editor_application_owns_dedicated_gui_workspace(')
  pane='\n'.join(definition(bootstrap,marker,True) for marker in ('        auto pane_supported =', '        auto set_pane_shown =', '        auto pane_dock_region =', '        auto active_pane_storage =', '        auto set_active_pane ='))
  companion=definition(workspace,'        const auto activate_workspace_companions =',True)
  pending=definition(workspace,'        if (editor.workspaceCompanionsPending)')
  active_type=definition(project,'        enum class ActiveEditDocument',True)
  active=definition(project,'        [[nodiscard]] ActiveEditDocument active_edit_document(')
  viewport=(base/'editor.frame_viewport.inl').read_text(encoding='utf-8');start=viewport.index('        bool guiDocumentWorkspace =');view=viewport[start:viewport.index(';',start)+1]
  snapshot=(base/'editor.application_context_snapshot.inl').read_text(encoding='utf-8')
  capture='snapshot.game2d_gui_workspace = editor.game2DGuiWorkspace;'
  start=snapshot.index('        editor.game2DGuiWorkspace =');restore=snapshot[start:snapshot.index(';',start)+1]
  self.assertIn(capture,snapshot)
  self.assertIn('editor.workspaceCompanionsPending = false;',snapshot)
  code=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#define EPOCH_ENABLE_AUTHORING_PLATFORM 1
#define EPOCH_ENABLE_TILEMAP_EDITOR 1
namespace gui {struct WidgetBounds {};}
namespace core {enum class ContextType {None,OpenGL};}
FRAME_TYPES
APP_TYPES
using EditorMainSurface=EditorApplicationSurface;
TYPES
DEDICATED
struct EditorState {
 EditorMainSurface mainSurface{EditorMainSurface::Scene};EditorApplicationKind applicationKind{EditorApplicationKind::Standard};
 bool game2DGuiWorkspace{},workspaceCompanionsPending{},showOutliner{},showInspector{},showConsoleDock{},showAiChat{};
 OutlinerToolTab outlinerToolTab{OutlinerToolTab::World};
 std::array<bool,kEditorToolPaneCount> toolPaneOpen{};
 std::array<EditorToolDockRegion,kEditorToolPaneCount> toolPaneDockRegions{default_tool_pane_regions()};
 std::string activeLeftPaneRoute{"pane.world_outliner"},activeRightPaneRoute{"pane.ai_chat"},activeBottomLeftPaneRoute{"pane.output"},activeBottomRightPaneRoute{"pane.ai_development"};
};
ACTIVE_TYPE
ACTIVE_FUNCTION
int main(){
 EditorState editor;struct {EditorApplicationPanes panes;} application;
 EditorFrameResult result;
 std::set<std::string,std::less<>> detached;
 const auto pane_route_is_detached=[&](std::string_view route){return detached.contains(route);};
 PANES
 COMPANION
 const auto drain=[&]{result={};PENDING};
 const auto gui_view=[&]{VIEW return guiDocumentWorkspace;};
 const auto originalRegions=editor.toolPaneDockRegions;
 activate_workspace_companions(EditorMainSurface::Scene);
 assert(editor.activeLeftPaneRoute=="pane.world_outliner"&&editor.activeRightPaneRoute=="pane.properties");
 assert(editor.showOutliner&&editor.showInspector&&editor.toolPaneOpen[tool_pane_index(EditorToolPane::WorldOutliner)]);
 activate_workspace_companions(EditorMainSurface::Assets);
 assert(editor.activeLeftPaneRoute=="pane.asset_browser"&&editor.activeRightPaneRoute=="pane.properties");
 activate_workspace_companions(EditorMainSurface::PlantLab);
 assert(editor.activeLeftPaneRoute=="pane.world_outliner");
 editor.mainSurface=EditorMainSurface::Game2D;editor.game2DGuiWorkspace=true;editor.workspaceCompanionsPending=true;
 drain();assert(!editor.workspaceCompanionsPending&&editor.activeLeftPaneRoute=="pane.gui_hierarchy");
 // Rendering a different tool pane cannot change the active GUI document.
 editor.outlinerToolTab=OutlinerToolTab::World;
 assert(gui_view()&&active_edit_document(editor)==ActiveEditDocument::gui);
 struct {bool game2d_gui_workspace{};} snapshot;
 CAPTURE
 editor.game2DGuiWorkspace=false;
 RESTORE
 assert(editor.game2DGuiWorkspace&&gui_view());
 // Closing a companion again is respected on idle frames.
 set_pane_shown("pane.gui_hierarchy",false);set_pane_shown("pane.asset_browser",true);
 set_active_pane(EditorToolDockRegion::Left,"pane.asset_browser");
 for(int frame=0;frame<100;++frame)drain();
 assert((result.companion_pane_activations==std::array<std::string,2>{}));
 assert(!editor.toolPaneOpen[tool_pane_index(EditorToolPane::GuiHierarchy)]&&editor.activeLeftPaneRoute=="pane.asset_browser");
 // An explicit subview selection activates only its companions once.
 editor.game2DGuiWorkspace=false;activate_workspace_companions(EditorMainSurface::Game2D);
 assert(editor.activeLeftPaneRoute=="pane.tile_map"&&!gui_view());
 editor.outlinerToolTab=OutlinerToolTab::Gui;
 assert(active_edit_document(editor)==ActiveEditDocument::tilemap);
 assert(editor.toolPaneDockRegions==originalRegions);
 // Custom co-location is preserved; primary hierarchy wins its group.
 editor.toolPaneDockRegions[tool_pane_index(EditorToolPane::GuiHierarchy)]=EditorToolDockRegion::Right;
 const auto customRegions=editor.toolPaneDockRegions;
 editor.game2DGuiWorkspace=true;activate_workspace_companions(EditorMainSurface::Game2D);
 assert(editor.activeRightPaneRoute=="pane.gui_hierarchy"&&editor.toolPaneDockRegions==customRegions);
 // A detached hierarchy stays detached, with no attempt to reparent it.
 detached.insert("pane.gui_hierarchy");editor.activeRightPaneRoute="pane.ai_chat";
 activate_workspace_companions(EditorMainSurface::Game2D);
 assert(detached.contains("pane.gui_hierarchy")&&editor.activeRightPaneRoute=="pane.properties");
 assert(result.companion_pane_activations[0]=="pane.gui_hierarchy"&&result.companion_pane_activations[1].empty());
 assert(editor.toolPaneDockRegions==customRegions);
 // Two floating/native companions are revealed once, with the primary last.
 detached.insert("pane.properties");
 for(const auto command:{EditorCommand::RunGame,EditorCommand::RunScript,EditorCommand::Exit}){
  result.command=command;result.command_argument="keep-real-command";
  result.close_current_context_after_command=true;
  activate_workspace_companions(EditorMainSurface::Game2D);
  assert(result.companion_pane_activations[0]=="pane.properties"&&result.companion_pane_activations[1]=="pane.gui_hierarchy");
  assert(result.command==command&&result.command_argument=="keep-real-command"&&result.close_current_context_after_command);
 }
 // A second explicit choice supersedes the first; unsupported views queue none.
 activate_workspace_companions(EditorMainSurface::Assets);
 assert(result.companion_pane_activations[0]=="pane.properties"&&result.companion_pane_activations[1].empty());
 activate_workspace_companions(EditorMainSurface::Systems);
 assert((result.companion_pane_activations==std::array<std::string,2>{}));
 for(int frame=0;frame<100;++frame)drain();
 assert((result.companion_pane_activations==std::array<std::string,2>{}));
 application.panes.outliner=false;set_pane_shown("pane.gui_hierarchy",false);
 activate_workspace_companions(EditorMainSurface::Game2D);
 assert(!editor.toolPaneOpen[tool_pane_index(EditorToolPane::GuiHierarchy)]);
 // Dedicated GUI applications remain GUI when a tool tab has another identity.
 editor.applicationKind=EditorApplicationKind::GuiEditor;editor.game2DGuiWorkspace=false;assert(gui_view());
}
'''
  for token,value in [('FRAME_TYPES',frame_types),('APP_TYPES',app_types),('TYPES',types),('DEDICATED',dedicated),('PANES',pane),('COMPANION',companion),('PENDING',pending),('ACTIVE_TYPE',active_type),('ACTIVE_FUNCTION',active),('VIEW',view),('CAPTURE',capture),('RESTORE',restore)]:code=code.replace(token,value)
  parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True);scratch=Path(tempfile.mkdtemp(prefix='workspace-companion-',dir=parent))
  try:
   cpp=scratch/'contract.cpp';binary=scratch/'contract';cpp.write_text(code,encoding='utf-8')
   if os.name=='nt':
    tools=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'));sdk=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((sdk/'Include').glob('*'))
    if not tools or not kits:self.skipTest('MSVC/SDK required')
    tool,kit=tools[-1],kits[-1];binary=scratch/'contract.exe';args=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX','/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(scratch/'contract.obj'),'/Fe'+str(binary),str(cpp),'/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
   else:
    compiler=shutil.which('clang++') or shutil.which('g++')
    if not compiler:self.skipTest('C++23 compiler required')
    args=[compiler,'-std=c++23',str(cpp),'-o',str(binary)]
   built=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=45);self.assertEqual(built.returncode,0,built.stdout[-6000:]+built.stderr)
   result=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  finally:
   self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()));shutil.rmtree(scratch)
