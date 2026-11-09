"""Execute real pane routing/storage and 2D document selection without a window."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest,json,time
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes
ROOT=Path(__file__).resolve().parents[2]
class WorkspaceCompanionRouting(unittest.TestCase):
 def test_scene_camera_metadata_matches_existing_rig(self):
  from Tests.support.cpp import compile_and_run
  source=(ROOT/'Engine/src/editor/application/editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
  a=source.index('                editor.workspaceTab =',source.index('            case EditorMainSurface::Scene:'))
  body=source[a:source.index('                push_editor_log',a)]
  code=r'''
#include <memory>
#include <cstdio>
namespace epochengine{namespace previewgrid{enum class CameraMode{Editor,FPS,Canvas2D};CameraMode active=CameraMode::Editor;int switches{};CameraMode camera_mode_for(const void*){return active;}void set_camera_mode(const void*,CameraMode mode){active=mode;++switches;}}}
namespace previewgrid=epochengine::previewgrid;
namespace core{enum class ScenePreviewMode{Editor};}
enum class EditorWorkspaceTab{Output};
struct Editor{EditorWorkspaceTab workspaceTab{};core::ScenePreviewMode previewMode{};previewgrid::CameraMode viewportCameraMode{};};
void actual(Editor& editor,std::shared_ptr<int> ctx){BODY}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d\n",__LINE__);return 91;}}while(false)
int main(){Editor editor;auto ctx=std::make_shared<int>();
 for(auto mode:{previewgrid::CameraMode::Editor,previewgrid::CameraMode::FPS}){previewgrid::active=mode;actual(editor,ctx);CHECK(editor.viewportCameraMode==mode&&previewgrid::active==mode&&previewgrid::switches==0);}
 previewgrid::active=previewgrid::CameraMode::Canvas2D;actual(editor,ctx);CHECK(editor.viewportCameraMode==previewgrid::CameraMode::Editor&&previewgrid::active==previewgrid::CameraMode::Editor&&previewgrid::switches==1);
 actual(editor,{});CHECK(editor.viewportCameraMode==previewgrid::CameraMode::Editor&&previewgrid::switches==1);return 0;}
'''.replace('BODY',body)
  report=ROOT/'Artifacts/Reports/editor-camera-switch-oct31'/('scene-return-'+str(time.time_ns()));report.mkdir(parents=True)
  unit=report/'contract.cpp';unit.write_text(code,encoding='utf-8')
  records=compile_and_run(self,unit,report)
  (report/'result.json').write_text(json.dumps({'scope':'Actual Scene workspace camera handoff, Editor/FPS/Canvas2D/no context','records':records},indent=2),encoding='utf-8')

 def test_transitions_preserve_docking_and_gui_document_identity(self):
  base=ROOT/'Engine/src/editor/application'
  state=(base/'editor.application_state.inl').read_text(encoding='utf-8')
  bootstrap=(base/'editor.frame_bootstrap.inl').read_text(encoding='utf-8')
  workspace=(base/'editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
  routing=(base/'editor.application_pane_routing.inl').read_text(encoding='utf-8')
  selector=definition(routing,'        [[nodiscard]] static bool select_editor_2d_workspace(')
  inventory=state[state.index('        struct EditorWorkspaceSpec final'):state.index('        struct EditorState',state.index('        struct EditorWorkspaceSpec final'))]
  visibility='\n'.join(definition(routing,marker) for marker in (
   '        [[nodiscard]] static bool editor_workspace_supported(',
   '        [[nodiscard]] static bool editor_workspace_active(',
   '        [[nodiscard]] static bool editor_workspace_visible(',
   '        static bool set_editor_workspace_visible(',
   '        static void reveal_editor_workspace_surface('))
  toolbar=(base/'editor.frame_toolbar_dock_targets.inl').read_text(encoding='utf-8')
  start=toolbar.index('        std::vector<gui::TabButtonSpec> documentTabs{};')
  tabs=toolbar[start:toolbar.index('        gui::set_cursor({ 16.0f, tab_y });',start)]
  start=toolbar.index('            const auto& workspace = *documentWorkspaces[*documentTabResult.selected_index];')
  dispatch=toolbar[start:toolbar.index('        gui::end_window();',start)]
  dispatch=dispatch[:dispatch.rfind('        }')]

  project=(base/'editor.application_project_documents.inl').read_text(encoding='utf-8')
  app=(ROOT/'Engine/modules/editor.application.ixx').read_text(encoding='utf-8')
  types='\n'.join(definition(state,marker,True) for marker in ('        enum class EditorToolDockRegion','        enum class OutlinerToolTab','        enum class EditorToolPane'))
  types+='\nconstexpr std::size_t kEditorToolPaneCount=static_cast<std::size_t>(EditorToolPane::Count);\n'
  types+='\n'.join(definition(state,marker) for marker in ('        [[nodiscard]] constexpr std::size_t tool_pane_index(', '        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>\n            editor_tool_pane_routes()', '        [[nodiscard]] constexpr std::array<EditorToolDockRegion, kEditorToolPaneCount>\n            default_tool_pane_regions()', '        [[nodiscard]] constexpr std::optional<EditorToolPane>\n            editor_tool_pane_from_route(', '        [[nodiscard]] constexpr bool is_outliner_tool_pane(', '        [[nodiscard]] constexpr bool is_inspector_tool_pane(', '        [[nodiscard]] constexpr bool is_console_tool_pane('))
  app_types='\n'.join(definition(app,marker,True) for marker in ('    enum class EditorApplicationKind','    enum class EditorApplicationSurface','    struct EditorApplicationPanes'))
  core=(ROOT/'Engine/modules/editor.core.ixx').read_text(encoding='utf-8')
  frame_types='\n'.join(definition(core,marker,True).replace('export ','',1) for marker in ('    export enum class EditorCommand','    export struct EditorFrameResult'))
  dedicated=definition(app,'    [[nodiscard]] constexpr bool\n        editor_application_owns_dedicated_gui_workspace(')
  pane='\n'.join(definition(bootstrap,marker,True) for marker in ('        auto pane_supported =', '        auto pane_family_enabled =', '        auto pane_is_shown =', '        auto set_pane_shown =', '        auto pane_dock_region =', '        auto active_pane_storage =', '        auto set_active_pane ='))
  companion=definition(workspace,'        const auto activate_workspace_companions =',True)
  focused=definition(workspace,'        if (focusedCompanionOwnerChanged)')
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
#include <vector>
#define EPOCH_ENABLE_AUTHORING_PLATFORM 1
#define EPOCH_ENABLE_TILEMAP_EDITOR 1
namespace gui {struct WidgetBounds {};
 struct TabButtonSpec {std::string_view id,label;float width{};bool active{},closable{},dirty{},enabled{};};
}
namespace core {enum class ContextType {None,OpenGL};}
FRAME_TYPES
APP_TYPES
using EditorMainSurface=EditorApplicationSurface;
TYPES
DEDICATED
struct ApplicationProfilePort{};
ApplicationProfilePort editor_application_profile(EditorApplicationKind){return {};}
bool editor_application_supports_surface(ApplicationProfilePort,EditorMainSurface){return true;}
INVENTORY
struct EditorState {
 bool workspaceVisibilityCustomized{};std::vector<std::string>visibleWorkspaceIds;
 EditorMainSurface mainSurface{EditorMainSurface::Scene};EditorApplicationKind applicationKind{EditorApplicationKind::Standard};
 int surfaceSettleFrames{};
 bool game2DGuiWorkspace{},workspaceCompanionsPending{},showOutliner{},showInspector{},showConsoleDock{},showAiChat{};
 OutlinerToolTab outlinerToolTab{OutlinerToolTab::World};
 std::array<bool,kEditorToolPaneCount> toolPaneOpen{};
 std::array<EditorToolDockRegion,kEditorToolPaneCount> toolPaneDockRegions{default_tool_pane_regions()};
 std::string activeLeftPaneRoute{"pane.world_outliner"},activeRightPaneRoute{"pane.ai_chat"},activeBottomLeftPaneRoute{"pane.output"},activeBottomRightPaneRoute{"pane.ai_development"};
};
VISIBILITY
SELECT_WORKSPACE
ACTIVE_TYPE
ACTIVE_FUNCTION
int main(){
 EditorState editor;struct {EditorApplicationPanes panes;} application;
 EditorFrameResult result;
 std::set<std::string,std::less<>> detached;
 const auto pane_route_is_detached=[&](std::string_view route){return detached.contains(route);};
 const bool projectViewportMaximized=false;
 PANES
 COMPANION
 bool focusedCompanionOwnerChanged{};
 const auto focus_companions=[&]{FOCUSED};
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
 // Automatic companions preserve a selected visible conversation in any
 // custom dock group. Manual tab selection remains a distinct owner action.
 const auto beforeChatProtection=editor;
 application.panes.ai_chat=true;
 for(const auto region:{EditorToolDockRegion::Left,EditorToolDockRegion::Right,
     EditorToolDockRegion::BottomLeft,EditorToolDockRegion::BottomRight}){
  editor=beforeChatProtection;detached.clear();result={};
  editor.toolPaneDockRegions[tool_pane_index(EditorToolPane::AiChat)]=region;
  editor.toolPaneDockRegions[tool_pane_index(EditorToolPane::Properties)]=region;
  editor.toolPaneDockRegions[tool_pane_index(EditorToolPane::GuiHierarchy)]=region;
  const auto regions=editor.toolPaneDockRegions;
  editor.game2DGuiWorkspace=true;set_pane_shown("pane.ai_chat",true);
  set_active_pane(region,"pane.ai_chat");
  activate_workspace_companions(EditorMainSurface::Game2D);
  assert(active_pane_storage(region)=="pane.ai_chat"&&pane_is_shown("pane.ai_chat"));
  assert(editor.toolPaneDockRegions==regions);
  assert((result.companion_pane_activations==std::array<std::string,2>{}));
  // Snapshot migration accepts the old persisted identity without replacing it.
  set_active_pane(region,"pane.ai_raw");
  activate_workspace_companions(EditorMainSurface::Game2D);
  assert(active_pane_storage(region)=="pane.ai_raw");
  // A direct user selection is allowed. Merely having Chat open does not pin it.
  set_active_pane(region,"pane.properties");
  assert(active_pane_storage(region)=="pane.properties");
  activate_workspace_companions(EditorMainSurface::Game2D);
  assert(active_pane_storage(region)=="pane.gui_hierarchy");
  // A closed/hidden conversation is never reopened or made active implicitly.
  set_active_pane(region,"pane.ai_chat");set_pane_shown("pane.ai_chat",false);
  activate_workspace_companions(EditorMainSurface::Game2D);
  assert(active_pane_storage(region)=="pane.gui_hierarchy"&&!pane_is_shown("pane.ai_chat"));
  assert(!editor.toolPaneOpen[tool_pane_index(EditorToolPane::AiChat)]);
  // A detached conversation does not occupy its remembered dock group.
  set_pane_shown("pane.ai_chat",true);set_active_pane(region,"pane.ai_chat");
  detached.insert("pane.ai_chat");
  activate_workspace_companions(EditorMainSurface::Game2D);
  assert(active_pane_storage(region)=="pane.gui_hierarchy"&&detached.contains("pane.ai_chat"));
 }
 editor=beforeChatProtection;detached.clear();result={};
 // The usual split keeps Chat selected while the other group's companion opens.
 set_pane_shown("pane.ai_chat",true);set_active_pane(EditorToolDockRegion::Right,"pane.ai_chat");
 activate_workspace_companions(EditorMainSurface::Assets);
 assert(editor.activeLeftPaneRoute=="pane.asset_browser"&&editor.activeRightPaneRoute=="pane.ai_chat");
 editor=beforeChatProtection;result={};application.panes.ai_chat=false;
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
 set_pane_shown("pane.properties",false);
 focusedCompanionOwnerChanged=true;focus_companions();
 if(editor.activeRightPaneRoute!="pane.ai_chat"||pane_is_shown("pane.properties"))return 98;
 assert(result.companion_pane_activations[0]=="pane.gui_hierarchy"&&result.companion_pane_activations[1].empty());
 set_pane_shown("pane.gui_hierarchy",false);
 focus_companions();
 assert((result.companion_pane_activations==std::array<std::string,2>{}));
 set_pane_shown("pane.gui_hierarchy",true);
 focusedCompanionOwnerChanged=false;
 for(int frame=0;frame<100;++frame)focus_companions();
 assert((result.companion_pane_activations==std::array<std::string,2>{}));
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
 // The actual top-tab builder has distinct 2D and UI identities, routes,
 // and active states while sharing the persisted authoring-mode bit.
 const bool toolbarControlsBlockedByMenu=false;
 const auto verify_tabs=[&](bool guiMode){
  editor.applicationKind=EditorApplicationKind::Standard;
  editor.mainSurface=EditorMainSurface::Game2D;(void)select_editor_2d_workspace(editor,guiMode);
  (void)set_editor_workspace_visible(editor,"document.game2d",true);
  (void)set_editor_workspace_visible(editor,"document.gui",true);
  DOCUMENT_TABS
  std::optional<std::size_t> tile,gui;
  for(std::size_t i=0;i<documentTabs.size();++i){
   if(documentTabs[i].id=="document.game2d"){tile=i;assert(documentTabs[i].label=="2D"&&documentTabs[i].active==!guiMode);}
   if(documentTabs[i].id=="document.gui"){gui=i;assert(documentTabs[i].label=="UI"&&documentTabs[i].active==guiMode);}
  }
  assert(tile&&gui&&*tile!=*gui);
  struct {std::optional<std::size_t> selected_index;} documentTabResult{guiMode?tile:gui};
  EditorMainSurface opened=EditorMainSurface::Scene;
  const auto open_editor_surface=[&](EditorMainSurface surface,std::string_view){opened=surface;activate_workspace_companions(surface);};
  DOCUMENT_DISPATCH
  assert(opened==EditorMainSurface::Game2D&&editor.game2DGuiWorkspace==!guiMode);
  assert(editor.outlinerToolTab==(!guiMode?OutlinerToolTab::Gui:OutlinerToolTab::TileMap));
  assert(editor.surfaceSettleFrames==1);
 };
 application.panes.outliner=true;detached.clear();verify_tabs(false);verify_tabs(true);
 // The real mode selection obeys a dedicated GUI application's ownership.
 editor.applicationKind=EditorApplicationKind::GuiEditor;
 editor.game2DGuiWorkspace=false;assert(select_editor_2d_workspace(editor,false)&&gui_view());
 {
  DOCUMENT_TABS
  for(const auto& tab:documentTabs)assert(tab.id!="document.game2d");
  assert(std::count_if(documentTabs.begin(),documentTabs.end(),[](const auto& tab){return tab.id=="document.gui"&&tab.active;})==1);
 }

}
'''
  for token,value in [('DOCUMENT_TABS',tabs),('DOCUMENT_DISPATCH',dispatch),('SELECT_WORKSPACE',selector),('FRAME_TYPES',frame_types),('APP_TYPES',app_types),('TYPES',types),('DEDICATED',dedicated),('INVENTORY',inventory),('VISIBILITY',visibility),('PANES',pane),('COMPANION',companion),('FOCUSED',focused),('PENDING',pending),('ACTIVE_TYPE',active_type),('ACTIVE_FUNCTION',active),('VIEW',view),('CAPTURE',capture),('RESTORE',restore)]:code=code.replace(token,value)
  parent=ROOT/'Artifacts/Reports/workspace-separation-oct28';parent.mkdir(parents=True,exist_ok=True)
  scratch=parent/str(time.time_ns());scratch.mkdir();cpp=scratch/'contract.cpp';cpp.write_text(code,encoding='utf-8')
  phases=[]
  for configuration in ('Debug','Release'):
   binary=scratch/('contract-'+configuration)
   if os.name=='nt':
    tools=msvc_2022_toolsets();sdk=Path('C:/Program Files (x86)/Windows Kits/10');kits=windows_sdk_includes(sdk)
    if not tools or not kits:self.skipTest('MSVC/SDK required')
    tool,kit=tools[-1],kits[-1];binary=binary.with_suffix('.exe');args=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX','/Od' if configuration=='Debug' else '/O2','/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(scratch/('contract-'+configuration+'.obj')),'/Fe'+str(binary),str(cpp),'/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
   else:
    compiler=shutil.which('clang++') or shutil.which('g++')
    if not compiler:self.skipTest('C++23 compiler required')
    args=[compiler,'-std=c++23','-O0' if configuration=='Debug' else '-O2',str(cpp),'-o',str(binary)]
   for phase,command in [(configuration+'-build',args),(configuration+'-test',[str(binary)])]:
    result=subprocess.run(command,cwd=scratch,capture_output=True,text=True,timeout=45)
    (scratch/(phase+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
    phases.append({'phase':phase,'exit':result.returncode,'command':command})
    (scratch/'result.json').write_text(json.dumps({'scope':'Actual workspace selector, top-tab builder/dispatch, companion routing and snapshot mode; selected shown Assistant Chat survives automatic companion activation in every dock group, with explicit manual/hidden/detached/legacy-route cases. Recording ports only, no GUI/native/models','phases':phases},indent=2)+'\n',encoding='utf-8')
    self.assertEqual(result.returncode,0,result.stdout[-6000:]+result.stderr)
