"""Actual Editor header and marker-publication phases with explicit CPU ports.

The native geometry admission/drawing implementation is not mocked as proof of
rendering: a refusing publication port verifies the Editor's accepted-mode rule.
"""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]

class EditorMeshDrawMode(unittest.TestCase):
 def test_actual_draw_and_publication_debug_release(self):
  folder=ROOT/'Engine/src/editor/application'
  viewport=(folder/'editor.frame_viewport.inl').read_text(encoding='utf-8')
  scene=(folder/'editor.application_scene_preview.inl').read_text(encoding='utf-8')
  state=(folder/'editor.application_state.inl').read_text(encoding='utf-8')
  api=(ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
  preview=(ROOT/'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
  fields=state[state.index('            bool viewportMeshWireframe{};'):state.index('            input::ProfilePreset inputProfilePreset',state.index('            bool viewportMeshWireframe{};'))]
  layout=viewport[viewport.index('            const bool showMeshDrawMode ='):viewport.index('            if (showSceneRun)',viewport.index('            const bool showMeshDrawMode ='))]
  controls=viewport[viewport.index('            if (showMeshDrawMode)'):viewport.index('            ctx->set_scene_preview_mode(',viewport.index('            if (showMeshDrawMode)'))]
  begin=scene.index('        void publish_editor_preview_markers(')
  request=scene[scene.index('            // A mode change',begin):scene.index('            rebuild_editor_lighting(state);',begin)]
  marker_begin=scene.index('                markers.push_back(',begin)
  marker=scene[marker_begin:scene.index('});',marker_begin)+3]
  publish_begin=scene.index('                published=epochengine::previewgrid::set_object_markers(',begin)
  publish_end=scene.index(');',publish_begin)+2
  commit_begin=scene.index('            if (!published)',publish_end)
  commit_end=scene.index('            if(state.planetaryInitialFramePending',commit_begin)
  publish='            bool published{};\n'+scene[publish_begin:publish_end]+'\n'+scene[commit_begin:commit_end]
  gui_types='\n'.join(definition(api,m,True).replace('export ','',1) for m in (
   '    export enum class ButtonIcon','    export struct InlineButtonSpec',
   '    export enum class InlineButtonSizing','    export enum class InlineButtonAlignment',
   '    export struct InlineButtonRowOptions','    export struct SelectBoxOptions','    export struct SelectBoxResult'))
  marker_type=definition(preview,'    export struct ObjectMarker',True).replace('export ','',1)
  cpp=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
namespace core{enum class ScenePreviewMode{None,Editor,Play};struct Context{};}
namespace epochengine::asset::mesh{struct Artifact{};}
namespace epochengine::visuals{constexpr int object_default(){return 0;}}
namespace epochengine::previewgrid{
struct Vec3{float x{},y{},z{};};constexpr Vec3 visual_rgb(int){return{};}
enum class ObjectPreviewPrimitive{Cube,IndexedMesh};struct MeshBaseColorBinding{};struct PreciseMeshTransform{};struct MeshPaintView;
MARKER_TYPE
struct MeshFrameLimits{unsigned frame_indices{},frame_bytes{};};
enum class EditorView{Perspective,FreeOrthographic,Front,Back,Left,Right,Top,Bottom};
EditorView editor_view_for(const core::Context*){return EditorView::Perspective;}
std::string_view editor_view_name(EditorView){return "Perspective";}
bool set_editor_view(const core::Context*,EditorView){return true;}
bool admit=true;std::vector<ObjectMarker> published;unsigned publications{};
bool set_object_markers(const core::Context*,std::vector<ObjectMarker> value,MeshFrameLimits){++publications;if(!admit)return false;published=std::move(value);return true;}
void clear_object_markers(const core::Context*){}
void clear_lighting_frame(const core::Context*){}
}
namespace previewgrid=epochengine::previewgrid;
namespace platform::child_process {
enum class RuntimeControlStatus{unavailable,idle,pending,accepted,rejected,disconnected};
enum class MeshDrawMode{solid,wireframe};
struct RuntimeControlSnapshot{RuntimeControlStatus status{RuntimeControlStatus::unavailable};MeshDrawMode acknowledged_mode{MeshDrawMode::solid};};
struct ProcessSnapshot{RuntimeControlSnapshot runtime_control;};}
enum class EditorMainSurface{Scene,Game2D,ForestFactory};
struct EditorState{bool forestTerrainPainting{};FIELDS
EditorMainSurface mainSurface{EditorMainSurface::Scene};core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};std::string projectStatus;bool projectBuildPending{},covered{},reserved{};
std::optional<platform::child_process::ProcessSnapshot>projectRuntimeSnapshot;unsigned runtimeRequests{};bool lastRuntimeWire{};};
bool editor_project_viewport_input_reserved(const EditorState&s){return s.reserved;}
bool editor_project_viewport_covered(const EditorState&s){return s.covered;}
bool request_project_runtime_mesh_mode(EditorState&s,bool wire){++s.runtimeRequests;s.lastRuntimeWire=wire;return true;}
struct Entity{std::array<float,3>rotation{};bool editorOnly{};std::string category,name;bool meshAsset{};std::shared_ptr<const epochengine::asset::mesh::Artifact>meshSnapshot;std::shared_ptr<const previewgrid::MeshBaseColorBinding>meshBaseColorTexture;};
previewgrid::Vec3 selection_color_for_entity(const Entity&,bool){return{};}float marker_radius_for_entity(const Entity&){return 1;}
bool marker_selected_for_entity(const Entity&,bool selected){return selected;}bool is_plant_lab_preview_entity(const Entity&){return false;}
previewgrid::ObjectPreviewPrimitive preview_primitive_for_entity(const Entity&){return previewgrid::ObjectPreviewPrimitive::Cube;}
namespace epochengine::render_arcade{struct Node{std::string_view name;};constexpr Node kScreenSceneNode{"Screen"};}
std::vector<std::string>logs;void push_editor_log(EditorState&,std::string text){logs.push_back(std::move(text));}
void publish_mode(EditorState&state,bool available=true){if(editor_project_viewport_covered(state))return;
 core::Context context;const auto*ctx=available?&context:nullptr;REQUEST
 const std::array<float,3>previewPosition{2,3,4};const auto meshScale=std::optional{std::array<float,3>{1,1,1}};
 const bool selected=true,forestPainting=false;Entity entity;entity.meshSnapshot=std::make_shared<const epochengine::asset::mesh::Artifact>();
 std::vector<previewgrid::ObjectMarker>markers;
 struct{unsigned frame_indices=123,frame_bytes=456;}meshBudget;
 MARKER
 PUBLISH
}
namespace gui{struct Vec2{float x{},y{};};TYPES
struct Rect{Vec2 position,size;std::string label;};std::vector<Rect>draws;
Vec2 cursor;unsigned mode_calls{},camera_calls{};bool choose{};std::size_t choice{};bool illegal{};std::string disabled_reason,lastModeLabel;bool modeEnabled{};
void set_cursor(Vec2 value){cursor=value;}
void panel_rect(Vec2,Vec2){}
SelectBoxResult select_box(const SelectBoxOptions&o){draws.push_back({cursor,o.size,std::string(o.id)});
 const bool mode=o.id=="scene-mesh-draw-mode";if(mode){++mode_calls;lastModeLabel=o.selected;modeEnabled=o.enabled;}else++camera_calls;
 return {mode&&choose,mode&&choose&&(o.enabled||illegal),mode&&choose?std::optional<std::size_t>{choice}:std::nullopt};}
std::optional<std::size_t>inline_button_row(std::span<const InlineButtonSpec>items,float height,float,const InlineButtonRowOptions&){
 for(const auto&i:items){draws.push_back({cursor,{i.width,height},std::string(i.label)});if(!i.enabled)disabled_reason=i.tooltip;}return{};}
bool button(std::string_view label,Vec2 size){draws.push_back({cursor,size,std::string(label)});return false;}
}
void focus_forest_factory_preview(core::Context*,EditorState&){}void handle_scene_tool(EditorState&,core::Context*,std::string_view){}
bool same_rect_overlap(const gui::Rect&a,const gui::Rect&b){return a.position.x<b.position.x+b.size.x-.01f&&b.position.x<a.position.x+a.size.x-.01f&&a.position.y<b.position.y+b.size.y-.01f&&b.position.y<a.position.y+a.size.y-.01f;}
int draw(EditorState&editor,float width,bool running,bool modal,bool run,bool maximize=false){
 editor.covered=running;editor.reserved=running;
 const auto ctx=std::make_shared<core::Context>();const gui::Vec2 scene_pos{20,40},scene_size{width,120};
 const bool sceneFits=width>=80,projectViewportMaximized=maximize,compiledProjectRunning=running;
 const bool menusBlockedByModal=modal,toolbarControlsBlockedByMenu=false;const float runControlOffset=96,runButtonWidth=112,runModeWidth=100;
 // Run eligibility matches the production threshold; its dispatch is separately tested.
 const bool showSceneRun=run&&width>=320;
 struct{bool scene_input_captured{};struct{gui::Vec2 position,size;}scene_viewport;}result;
 result.scene_viewport={{scene_pos.x+2,scene_pos.y+30},{width-4,88}};
 LAYOUT
 gui::draws.clear();gui::disabled_reason.clear();
 if(showSceneRun)gui::draws.push_back({{scene_pos.x+runControlOffset,scene_pos.y+2},{216,20},"Run controls port"});
 CONTROLS
 for(std::size_t i=0;i<gui::draws.size();++i){const auto&a=gui::draws[i];
  CHECK(a.position.x>=scene_pos.x&&a.position.y>=scene_pos.y&&a.position.x+a.size.x<=scene_pos.x+width+.01f&&a.position.y+a.size.y<=scene_pos.y+scene_size.y);
  for(std::size_t j=i+1;j<gui::draws.size();++j)CHECK(!same_rect_overlap(a,gui::draws[j]));}
 if(showMeshDrawMode){CHECK(gui::draws.size()>=1);if(running&&!editor.projectRuntimeSnapshot)CHECK(!gui::modeEnabled);
  const auto mode=std::find_if(gui::draws.begin(),gui::draws.end(),[](const auto&rect){return rect.label=="scene-mesh-draw-mode";});
  CHECK(mode!=gui::draws.end()&&mode->position.y+26<=result.scene_viewport.position.y);
  CHECK(result.scene_viewport.position.y+result.scene_viewport.size.y==scene_pos.y+118);}
 if(!showMeshDrawMode)CHECK(gui::mode_calls==0);
 return 0;
}
int main(){EditorState e,other;CHECK(!e.viewportMeshWireframe&&!e.viewportMeshWireframeRequest);
 gui::choose=true;gui::choice=1;CHECK(draw(e,900,false,false,false)==0);CHECK(e.viewportMeshWireframeRequest&&*e.viewportMeshWireframeRequest&&!e.viewportMeshWireframe);
 publish_mode(e);CHECK(e.viewportMeshWireframe&&!e.viewportMeshWireframeRequest&&previewgrid::published.size()==1&&previewgrid::published[0].wireframe);
 CHECK(!other.viewportMeshWireframe);
 e.viewportMeshWireframeRequest=false;previewgrid::admit=false;const auto retained=previewgrid::published[0].mesh;
 publish_mode(e);CHECK(e.viewportMeshWireframe&&!e.viewportMeshWireframeRequest&&previewgrid::published[0].wireframe&&previewgrid::published[0].mesh==retained&&!logs.empty());
 e.viewportMeshWireframeRequest=false;const auto count=previewgrid::publications;publish_mode(e,false);
 CHECK(e.viewportMeshWireframe&&e.viewportMeshWireframeRequest==false&&previewgrid::publications==count);
 previewgrid::admit=true;publish_mode(e);CHECK(!e.viewportMeshWireframe&&!previewgrid::published[0].wireframe);
 gui::illegal=true;CHECK(draw(e,900,false,true,false)==0);CHECK(!e.viewportMeshWireframeRequest);
 e.viewportMeshWireframeRequest=true;CHECK(draw(e,900,true,false,false)==0);CHECK(!e.viewportMeshWireframeRequest&&!e.viewportMeshWireframe);
 // Runtime dropdown must describe the last published acknowledgement,
 // never claim the selected mode before the child applies it.
 gui::illegal=false;using Status=platform::child_process::RuntimeControlStatus;using Mode=platform::child_process::MeshDrawMode;
 e.projectRuntimeSnapshot=platform::child_process::ProcessSnapshot{{Status::accepted,Mode::solid}};
 CHECK(draw(e,900,true,false,false)==0);CHECK(e.runtimeRequests==1&&e.lastRuntimeWire&&!e.viewportMeshWireframe&&gui::lastModeLabel=="Solid");
 e.projectRuntimeSnapshot->runtime_control={Status::pending,Mode::solid};
 CHECK(draw(e,900,true,false,false)==0);CHECK(e.runtimeRequests==1&&!gui::modeEnabled&&gui::lastModeLabel=="Solid (wait)");
 gui::choose=false;e.projectRuntimeSnapshot->runtime_control={Status::accepted,Mode::wireframe};
 CHECK(draw(e,900,true,false,false)==0);CHECK(gui::modeEnabled&&gui::lastModeLabel=="Wireframe");
 e.projectRuntimeSnapshot->runtime_control={Status::rejected,Mode::wireframe};
 CHECK(draw(e,900,true,false,false)==0);CHECK(gui::lastModeLabel=="Wire (refused)");
 e.projectRuntimeSnapshot->runtime_control={Status::disconnected,Mode::wireframe};
 CHECK(draw(e,900,true,false,false)==0);CHECK(!gui::modeEnabled&&gui::lastModeLabel=="Disconnected");
 e.projectRuntimeSnapshot.reset();gui::choose=false;gui::illegal=false;
 for(float width:{0.f,79.f,80.f,120.f,200.f,319.f,320.f,400.f,440.f,650.f,900.f})for(bool running:{false,true})for(bool run:{false,true})for(auto surface:{EditorMainSurface::Scene,EditorMainSurface::Game2D,EditorMainSurface::ForestFactory}){
  e.mainSurface=surface;gui::mode_calls=0;CHECK(draw(e,width,running,false,run)==0);}
 return 0;}
'''
  for name,value in {'FIELDS':fields,'MARKER_TYPE':marker_type,'REQUEST':request,'MARKER':marker,'PUBLISH':publish,'TYPES':gui_types,'LAYOUT':layout,'CONTROLS':controls}.items():cpp=cpp.replace(name,value)
  lane=ROOT/'Artifacts/Reports/preview-contracts-oct31/mesh'/hashlib.sha256(cpp.encode()).hexdigest()[:12];lane.mkdir(parents=True,exist_ok=True)
  source=lane/'editor.mesh_draw_contract.cpp';source.write_text(cpp,encoding='utf-8')
  results=compile_and_run(self,source,lane)
  (lane/'result.json').write_text(json.dumps({'passed':True,'cases':results,'scope':'Actual header layout/draw and marker publication/request/commit phases with explicit GUI and renderer-admission CPU ports; no native/backend acceptance.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
