"""Real workspace inventory, tab/menu consumers and layout codec; no GUI runtime."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_theme_persistence import contract_source
from Tests.gui.test_command_menu_submenu_adapter import child_view_definition

ROOT=Path(__file__).resolve().parents[2]

class WorkspaceVisibility(unittest.TestCase):
 def test_actual_workspace_close_reopen_and_codec_debug_release(self):
  folder=ROOT/'Engine/src/editor/application'
  toolbar=(folder/'editor.frame_toolbar_dock_targets.inl').read_text(encoding='utf-8')
  workspace=(folder/'editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
  routing=(folder/'editor.application_pane_routing.inl').read_text(encoding='utf-8')
  menus=(folder/'editor.frame_command_menus.inl').read_text(encoding='utf-8')
  gui=(ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
  construction=toolbar[toolbar.index('        std::vector<gui::TabButtonSpec> documentTabs{};'):toolbar.index('        gui::set_cursor({ 16.0f, tab_y });')]
  dispatch=toolbar[toolbar.index('        if (documentTabResult.closed_index'):toolbar.index('        gui::end_window();',toolbar.index('        if (documentTabResult.closed_index'))]
  close=definition(workspace,'        const auto close_editor_workspace_tab =',True)
  selector=definition(routing,'        [[nodiscard]] static bool select_editor_2d_workspace(')
  window=definition(menus,'                for(std::size_t index=0;index<kEditorWorkspaceSpecs.size();++index)')
  types='\n'.join(definition(gui,marker,True).replace('export ','',1) for marker in (
   '    export enum class SemanticTone','    export enum class MenuItemIndicator','    export struct MenuItemOptions','    export struct TabButtonSpec'))
  state=(folder/'editor.application_state.inl').read_text(encoding='utf-8')
  pane_types=state[state.index('        enum class EditorToolPane :'):state.index('        [[nodiscard]] constexpr std::size_t tool_pane_index(')]
  pane_labels='\n'.join(definition(state,marker) for marker in (
   '        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>\n            editor_tool_pane_routes()',
   '        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>\n            editor_tool_pane_labels()'))
  surface_title=definition(routing,'        [[nodiscard]] static std::string_view main_surface_title(')
  code=contract_source().split('int main() {',1)[0]
  code=code.replace('struct EditorState {','enum class OutlinerToolTab{World,Gui,TileMap};\nstruct EditorState {\n int surfaceSettleFrames{};OutlinerToolTab outlinerToolTab{};')
  code=code.replace('#include <vector>','#include <vector>\n#include <functional>')
  code+=r'''
namespace gui_lib{VIEW}
namespace gui{struct Vec2{float x{},y{};};TYPES}
SELECTOR
SURFACE_TITLE
namespace pane_label_ports {PANE_INVENTORY
}
unsigned open_calls{};std::string open_source;
struct Result{std::optional<std::size_t>selected_index,closed_index;};
std::vector<gui::TabButtonSpec>tabs(EditorState&editor){const bool toolbarControlsBlockedByMenu=false;CONSTRUCTION return documentTabs;}
int act(EditorState&editor,std::string_view closeId,std::string_view selectedId={},std::string_view menuLabel={}){
 const bool toolbarControlsBlockedByMenu=false;
 CONSTRUCTION
 const auto open_editor_surface=[&](EditorMainSurface surface,std::string_view from){++open_calls;open_source=from;editor.mainSurface=surface;reveal_editor_workspace_surface(editor,surface);};
 CLOSE
 Result documentTabResult;
 for(std::size_t index=0;index<documentWorkspaces.size();++index){if(documentWorkspaces[index]->id==closeId)documentTabResult.closed_index=index;if(documentWorkspaces[index]->id==selectedId)documentTabResult.selected_index=index;}
 DISPATCH
 struct Action{std::string label;gui::MenuItemOptions options;std::function<void()>invoke;};std::vector<Action>actions;
 const auto menu_item=[&](std::string_view label,auto callback,gui::MenuItemOptions options){actions.push_back({std::string(label),options,callback});};
 WINDOW
 CHECK(actions.size()==kEditorWorkspaceSpecs.size());
 for(std::size_t index=0;index<actions.size();++index){CHECK(actions[index].options.command_id==0x30000u+index);if(actions[index].label==menuLabel)actions[index].invoke();}
 return 0;
}
int main(){EditorState editor;const auto originalRegions=editor.toolPaneDockRegions;const auto originalTheme=editor.themePreference;
 const auto defaults=tabs(editor);CHECK(defaults.size()==4);CHECK(defaults[0].id=="document.world"&&defaults[0].label=="3D Preview"&&defaults[1].id=="document.project"&&defaults[2].id=="document.assets"&&defaults[3].id=="document.ai_development");
 CHECK(main_surface_title(EditorMainSurface::Scene)=="3D Preview");
 CHECK(pane_label_ports::editor_tool_pane_routes()[0]=="pane.world_outliner"&&pane_label_ports::editor_tool_pane_labels()[0]=="3D Objects");
 EditorState renamed;CHECK(act(renamed,{}, {},"3D Preview")==0&&!editor_workspace_visible(renamed,kEditorWorkspaceSpecs[0]));
 CHECK(act(renamed,{}, {},"3D Preview")==0&&renamed.mainSurface==EditorMainSurface::Scene);open_calls=0;open_source.clear();
 for(const auto&tab:defaults)CHECK(tab.closable);
 CHECK(act(editor,{}, {},"Forest Factory")==0);CHECK(editor.mainSurface==EditorMainSurface::ForestFactory&&open_source=="Window menu");CHECK(tabs(editor).size()==5);
 const auto calls=open_calls;CHECK(act(editor,"document.assets")==0);CHECK(open_calls==calls&&tabs(editor).size()==4);
 CHECK(act(editor,"document.forest","document.forest")==0);CHECK(editor.mainSurface==EditorMainSurface::Scene&&open_calls==calls+1&&open_source=="workspace tab closed");
 CHECK(tabs(editor).size()==3);CHECK(act(editor,{}, {},"Forest Factory")==0);CHECK(tabs(editor).size()==4&&editor.mainSurface==EditorMainSurface::ForestFactory);
 CHECK(act(editor,{}, {},"UI")==0);CHECK(editor.mainSurface==EditorMainSurface::Game2D&&editor.game2DGuiWorkspace&&editor.outlinerToolTab==OutlinerToolTab::Gui);
 CHECK(act(editor,{}, {},"2D")==0);CHECK(editor.mainSurface==EditorMainSurface::Game2D&&!editor.game2DGuiWorkspace&&editor.outlinerToolTab==OutlinerToolTab::TileMap);
 CHECK(editor.toolPaneDockRegions==originalRegions&&editor.themePreference==originalTheme);
 const auto encoded=serialize_editor_layout_preferences(editor);CHECK(encoded.starts_with("EPOCH_EDITOR_LAYOUT 11\n")&&encoded.find("workspaces ")!=encoded.npos);
 EditorState reopened;
 CHECK(decode_editor_layout_preferences(reopened,encoded));
 CHECK(reopened.workspaceVisibilityCustomized);
 CHECK(std::is_permutation(reopened.visibleWorkspaceIds.begin(),reopened.visibleWorkspaceIds.end(),editor.visibleWorkspaceIds.begin(),editor.visibleWorkspaceIds.end()));
 const auto repeated=serialize_editor_layout_preferences(reopened);EditorState twice;CHECK(decode_editor_layout_preferences(twice,repeated));CHECK(twice.visibleWorkspaceIds==reopened.visibleWorkspaceIds);
 const auto retained=reopened.visibleWorkspaceIds;
 for(const auto bad:{std::string{"workspaces 2 document.world document.world"},std::string{"workspaces 1 document.missing"},std::string{"workspaces 12"}}){auto invalid=encoded;const auto begin=invalid.find("workspaces "),end=invalid.find('\n',begin);invalid.replace(begin,end-begin,bad);CHECK(!decode_editor_layout_preferences(reopened,invalid)&&reopened.visibleWorkspaceIds==retained);}
 const auto ids=editor.visibleWorkspaceIds;CHECK(!set_editor_workspace_visible(editor,"document.foreign",true)&&editor.visibleWorkspaceIds==ids);
 for(unsigned schema:{9u,10u}){auto old=encoded;const auto begin=old.find("workspaces "),end=old.find('\n',begin);old.erase(begin,end-begin+1);old.replace(0,old.find('\n'),"EPOCH_EDITOR_LAYOUT "+std::to_string(schema));EditorState legacy;CHECK(decode_editor_layout_preferences(legacy,old));CHECK(!legacy.workspaceVisibilityCustomized&&legacy.visibleWorkspaceIds.empty()&&tabs(legacy).size()==4);}
 // A restored active optional surface remains reachable even if it was not in
 // an older preference set. Closing the final tab keeps a supported fallback.
 EditorState final;final.workspaceVisibilityCustomized=true;final.visibleWorkspaceIds={"document.world"};CHECK(act(final,"document.world")==0);CHECK(final.mainSurface==EditorMainSurface::Scene&&tabs(final).size()==1);
 final.mainSurface=EditorMainSurface::ForestFactory;CHECK(tabs(final).size()==2);
 return 0;}
'''
  for key,value in {'VIEW':child_view_definition(),'TYPES':types,'SELECTOR':selector,'CONSTRUCTION':construction,'CLOSE':close,'DISPATCH':dispatch,'WINDOW':window,'SURFACE_TITLE':surface_title,'PANE_INVENTORY':pane_types+pane_labels}.items():code=code.replace(key,value)
  lane=ROOT/'Artifacts/Reports/workspace-visibility-oct29'/hashlib.sha256(code.encode()).hexdigest()[:12];lane.mkdir(parents=True,exist_ok=True)
  cpp=lane/'editor.workspace_visibility_contract.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,lane,windows_flags=["/W4","/WX"])
  (lane/'result.json').write_text(json.dumps({'passed':True,'cases':results,'scope':'Actual canonical inventory, visibility helpers, toolbar builder/closed-first dispatch, Window workspace commands and schema11 codec. Application profile, open-route side effects and GUI input are explicit CPU ports; no native GUI proof.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
