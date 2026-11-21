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
  api=(ROOT/'Engine/modules/gui/gui.engine.ixx').read_text(encoding='utf-8')
  preview=(ROOT/'Engine/modules/render/render.preview_grid.ixx').read_text(encoding='utf-8')
  fields=state[state.index('            platform::child_process::MeshDrawMode viewportMeshDrawMode'):state.index('            input::ProfilePreset inputProfilePreset',state.index('            platform::child_process::MeshDrawMode viewportMeshDrawMode'))]
  layout=viewport[viewport.index('            bool showMeshDrawMode ='):viewport.index('            if (showSceneRun)',viewport.index('            bool showMeshDrawMode ='))]
  caption_reservation=viewport[viewport.index('            const float runControlOffset ='):viewport.index('            bool showSceneRun =')]
  controls=viewport[viewport.index('            if (showMeshDrawMode)'):viewport.index('            ctx->set_scene_preview_mode(',viewport.index('            if (showMeshDrawMode)'))]
  begin=scene.index('        void publish_editor_preview_markers(')
  request=scene[scene.index('            // A mode change',begin):scene.index('            rebuild_editor_lighting(state);',begin)]
  material_mode=definition(scene,'        [[nodiscard]] constexpr bool editor_mesh_mode_uses_base_color(')
  marker_begin=scene.index('                const bool useBaseColor',scene.index('const auto meshScale',begin))
  marker=scene[marker_begin:scene.index('});',marker_begin)+3]
  neutral_begin=scene.index('            for (auto& marker : markers)',marker_begin)
  neutral=scene[neutral_begin:scene.index('            const bool planetaryFrame=',neutral_begin)]
  publish_begin=scene.index('                published=epochengine::previewgrid::set_object_markers(',begin)
  publish_end=scene.index(');',publish_begin)+2
  commit_begin=scene.index('            if (!published)',publish_end)
  commit_end=scene.index('            if (state.planetaryInitialFramePending',commit_begin)
  publish='            bool published{};\n'+scene[publish_begin:publish_end]+'\n'+scene[commit_begin:commit_end]
  gui_types='\n'.join(definition(api,m,True).replace('export ','',1) for m in (
   '    export enum class ButtonIcon','    export struct InlineButtonSpec','    export struct SegmentedButtonSpec',
   '    export enum class InlineButtonSizing','    export enum class InlineButtonAlignment',
   '    export struct InlineButtonRowOptions','    export struct SelectBoxOptions','    export struct SelectBoxResult'))
  marker_type='\n'.join(definition(preview, '    export struct '+name, True).replace('export ','',1) for name in ('PlanetAtmosphere', 'SphericalWaterView', 'ObjectMarker'))
  chrome=(ROOT/'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
  height_begin=chrome.index('        const bool hasText=')
  height_metric=chrome[height_begin:chrome.index('        std::vector<float> widths',height_begin)]
  select=(ROOT/'Engine/src/epochgui/controls/gui.scroll_controls.inl').read_text(encoding='utf-8')
  select_width=select[select.index('        const float requestedWidth ='):select.index('        const auto displayLabel =')]
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
namespace input{enum class Key{F2};}
bool cycleMeshMode{};
namespace core{enum class ScenePreviewMode{None,Editor,Play};struct Context{
 bool is_key_down_safe(input::Key)const{return cycleMeshMode;}
};}
namespace epochengine::asset::mesh{struct Artifact{}; struct SphericalWaterAppearance;}
namespace epochengine::visuals{constexpr int object_default(){return 0;}}
namespace epochengine::previewgrid{
struct Vec3{float x{},y{},z{};};constexpr Vec3 visual_rgb(int){return{};}
enum class ObjectPreviewPrimitive{Cube,IndexedMesh,Light};struct MeshBaseColorBinding{};struct PreciseMeshTransform{};struct MeshPaintView;
MARKER_TYPE
struct MeshFrameLimits{unsigned frame_indices{},frame_bytes{};};
enum class EditorView{Perspective,FreeOrthographic,Front,Back,Left,Right,Top,Bottom};
EditorView editor_view_for(const core::Context*){return EditorView::Perspective;}
std::string_view editor_view_name(EditorView){return "Perspective";}
std::optional<EditorView> appliedView;
bool set_editor_view(const core::Context*,EditorView view){appliedView=view;return true;}
void reset_camera(const core::Context*){}
bool admit=true;std::vector<ObjectMarker> published;unsigned publications{};
bool set_object_markers(const core::Context*,std::vector<ObjectMarker> value,MeshFrameLimits){++publications;if(!admit)return false;published=std::move(value);return true;}
void clear_object_markers(const core::Context*){}
void clear_lighting_frame(const core::Context*){}
}
namespace previewgrid=epochengine::previewgrid;
namespace platform::child_process {
enum class RuntimeControlStatus{unavailable,idle,pending,accepted,rejected,disconnected};
enum class MeshDrawMode{solid,wireframe,shaded};
struct RuntimeControlSnapshot{RuntimeControlStatus status{RuntimeControlStatus::unavailable};MeshDrawMode acknowledged_mode{MeshDrawMode::shaded};};
struct ProcessSnapshot{RuntimeControlSnapshot runtime_control;};}
MATERIAL_MODE
enum class EditorMainSurface{Scene,Game2D,ForestFactory};
enum class ScenePointerMode{select,move_free,move_x,move_y,move_z};
struct EditorState{bool forestTerrainPainting{};FIELDS
EditorMainSurface mainSurface{EditorMainSurface::Scene};core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};std::string projectStatus;bool projectBuildPending{},covered{},reserved{};
ScenePointerMode scenePointerMode{ScenePointerMode::select};std::optional<int> lodInspectionReturnCamera;
bool planetaryInitialFramePending{};unsigned planetaryInitialFrameGeneration{};
struct Lifecycle{struct Evidence{struct{struct{unsigned generation;}selected_project;}generations;};
 Evidence evidence()const{return {{{3}}};}}projectLifecycle;
std::optional<platform::child_process::ProcessSnapshot>projectRuntimeSnapshot;unsigned runtimeRequests{};platform::child_process::MeshDrawMode lastRuntimeMode{};};
bool begin_editor_lod_inspection(EditorState&s,const core::Context*){s.lodInspectionReturnCamera=1;return true;}
void end_editor_lod_inspection(EditorState&s,const core::Context*){s.lodInspectionReturnCamera.reset();}
bool editor_project_viewport_input_reserved(const EditorState&s){return s.reserved;}
bool editor_project_viewport_covered(const EditorState&s){return s.covered;}
bool request_project_runtime_mesh_mode(EditorState&s,platform::child_process::MeshDrawMode mode){++s.runtimeRequests;s.lastRuntimeMode=mode;return true;}
namespace scene{enum class SceneTextureAlphaMode{opaque,cutout};}
struct TextureMaterial{scene::SceneTextureAlphaMode alpha;};
struct Entity{std::array<float,3>rotation{};bool editorOnly{};std::string category,name,type;bool meshAsset{};std::shared_ptr<const epochengine::asset::mesh::Artifact>meshSnapshot;std::shared_ptr<const previewgrid::MeshBaseColorBinding>meshBaseColorTexture;std::optional<TextureMaterial> textureMaterial;};
unsigned materialLookups{};bool admitMaterial=true;
bool restore_editor_mesh_base_color(const core::Context*,EditorState&,Entity& entity){++materialLookups;return !entity.textureMaterial||admitMaterial;}
previewgrid::Vec3 selection_color_for_entity(const Entity&,bool){return{};}float marker_radius_for_entity(const Entity&){return 1;}
bool marker_selected_for_entity(const Entity&,bool selected){return selected;}bool is_plant_lab_preview_entity(const Entity&){return false;}
previewgrid::ObjectPreviewPrimitive preview_primitive_for_entity(const Entity&){return previewgrid::ObjectPreviewPrimitive::Cube;}
namespace epochengine::render_arcade{struct Node{std::string_view name;};constexpr Node kScreenSceneNode{"Screen"};}
std::vector<std::string>logs;void push_editor_log(EditorState&,std::string text){logs.push_back(std::move(text));}
void publish_mode(EditorState&state,bool available=true,std::optional<TextureMaterial> material={}){if(editor_project_viewport_covered(state))return;
 core::Context context;const auto*ctx=available?&context:nullptr;REQUEST
 const std::array<float,3>previewPosition{2,3,4};const auto meshScale=std::optional{std::array<float,3>{1,1,1}};
 const bool selected=true,forestPainting=false;Entity entity;entity.meshSnapshot=std::make_shared<const epochengine::asset::mesh::Artifact>();
 entity.textureMaterial=material;if(material)entity.meshBaseColorTexture=std::make_shared<const previewgrid::MeshBaseColorBinding>();
 struct{std::shared_ptr<const epochengine::asset::mesh::Artifact>mesh;explicit operator bool()const{return false;}}strike;const bool lightning=false;const std::array<float,3>tint{};
 std::vector<previewgrid::ObjectMarker>markers;
 struct{unsigned frame_indices=123,frame_bytes=456;}meshBudget;
 for(unsigned item=0;item<1;++item){MARKER}
 NEUTRAL_MODE
 PUBLISH
}
namespace gui{struct Vec2{float x{},y{};};TYPES
bool keyboard_input_captured(){return false;}
struct Rect{Vec2 position,size;std::string label;};std::vector<Rect>draws;
float fontHeight=22;constexpr float kFontScale=1,kBoxInnerPadding=5;float base_line_height(float){return fontHeight;}
float inline_button_row_height(std::span<const InlineButtonSpec>items,float height,float,const InlineButtonRowOptions&){HEIGHT_METRIC return height;}
Vec2 cursor;unsigned mode_calls{},camera_calls{};bool choose{},chooseView{},exitInspect{};std::size_t choice{},viewChoice{};bool illegal{};std::string disabled_reason,lastModeLabel,pointerSymbol;bool modeEnabled{};
float segmented_button_row_width(std::span<const SegmentedButtonSpec> items){float w=0;for(const auto&i:items)w+=float(i.label.size())*8+12;return w;}
float titled_window_caption_width(std::string_view title){return float(title.size())*8+12;}
void text(std::string_view){}
enum class SemanticTone{muted};struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};void status_label(std::string_view label,SemanticTone,Vec2 position){draws.push_back({position,{float(label.size())*8,fontHeight},std::string(label)});}
void set_cursor(Vec2 value){cursor=value;}
void panel_rect(Vec2,Vec2){}
std::string lastViewCaption;
SelectBoxResult select_box(const SelectBoxOptions&o){const auto&options=o;const float availableWidth=10000.f;
 SELECT_WIDTH
 draws.push_back({cursor,{rowWidth,(std::max)(o.size.y,fontHeight+10)},std::string(o.id)});
 if(o.id=="scene-viewport-pointer-mode")pointerSymbol=o.closed_label;
 if(o.id=="scene-viewport-view-select"){lastViewCaption=o.closed_label;if(chooseView&&o.enabled)return {false,true,viewChoice};}
 const bool mode=o.id=="scene-mesh-draw-mode";if(mode){++mode_calls;lastModeLabel=o.selected;modeEnabled=o.enabled;}else++camera_calls;
 return {mode&&choose,mode&&choose&&(o.enabled||illegal),mode&&choose?std::optional<std::size_t>{choice}:std::nullopt};}
std::optional<std::size_t>inline_button_row(std::span<const InlineButtonSpec>items,float height,float,const InlineButtonRowOptions&){
 for(const auto&i:items){draws.push_back({cursor,{i.width,height},std::string(i.label)});if(!i.enabled)disabled_reason=i.tooltip;}return{};}
bool button(std::string_view label,Vec2 size){draws.push_back({cursor,{size.x,(std::max)(size.y,fontHeight+10)},std::string(label)});return exitInspect&&label=="Exit Inspect";}
}
std::string preview_zoom_text(const std::shared_ptr<core::Context>&){return "Distance 1";}
void focus_forest_factory_preview(core::Context*,EditorState&){}void handle_scene_tool(EditorState&,core::Context*,std::string_view){}
bool same_rect_overlap(const gui::Rect&a,const gui::Rect&b){return a.position.x<b.position.x+b.size.x-.01f&&b.position.x<a.position.x+a.size.x-.01f&&a.position.y<b.position.y+b.size.y-.01f&&b.position.y<a.position.y+a.size.y-.01f;}
int draw(EditorState&editor,float width,bool running,bool modal,bool run,bool maximize=false){
 editor.covered=running;editor.reserved=running;
 const auto ctx=std::make_shared<core::Context>();const gui::Vec2 scene_pos{20,40},scene_size{width,120};
 const bool sceneFits=width>=80,projectViewportMaximized=maximize,compiledProjectRunning=running;
 const bool menusBlockedByModal=modal,toolbarControlsBlockedByMenu=false,projectChildActionsAvailable=run;const float runButtonWidth=112;
 const bool workspaceShortcutInputAvailable=!modal;
 constexpr std::string_view sceneTitle{"Scene Preview"};
 CAPTION_RESERVATION
 CHECK(projectViewportMaximized ? runControlOffset==8 : runControlOffset>=gui::titled_window_caption_width(sceneTitle));
 const std::array heightMetric{gui::InlineButtonSpec{.label="Viewport",.width=100.0f}};
 const float headerControlHeight=gui::inline_button_row_height(heightMetric,20.0f,0.0f,{.sizing=gui::InlineButtonSizing::FitContent});
 // Run eligibility matches the production threshold; its dispatch is separately tested.
 bool showSceneRun=run&&width>=runControlOffset+runButtonWidth+runModeSpace+12;
 struct{bool scene_input_captured{};struct{gui::Vec2 position,size;}scene_viewport;}result;
 result.scene_viewport={{scene_pos.x+2,scene_pos.y+30},{width-4,88}};
 LAYOUT
 gui::draws.clear();gui::disabled_reason.clear();
 if(showSceneRun)gui::draws.push_back({{scene_pos.x+runControlOffset,scene_pos.y+2},{runButtonWidth+4.0f+runModeSpace,20},"Run controls port"});
 CONTROLS
 const auto view=std::find_if(gui::draws.begin(),gui::draws.end(),[](const auto&rect){return rect.label=="scene-viewport-view-select";});
 const auto pointer=std::find_if(gui::draws.begin(),gui::draws.end(),[](const auto&rect){return rect.label=="scene-viewport-pointer-mode";});
 const auto reset=std::find_if(gui::draws.begin(),gui::draws.end(),[](const auto&rect){return rect.label=="Reset"||rect.label=="Exit Inspect";});
 if(view!=gui::draws.end()){
  const auto next=pointer==gui::draws.end()?reset:pointer;
  CHECK(next!=gui::draws.end()&&view->size.x>=96.0f&&std::abs(next->position.x-(view->position.x+view->size.x)-6.0f)<.01f);
 }
 if(pointer!=gui::draws.end()){
  CHECK(reset!=gui::draws.end()&&pointer->size.x>=96.0f&&std::abs(reset->position.x-(pointer->position.x+pointer->size.x)-6.0f)<.01f);
 }
 const auto distance=std::find_if(gui::draws.begin(),gui::draws.end(),[](const auto&rect){return rect.label=="Distance 1";});
 if(distance!=gui::draws.end()){
  CHECK(view!=gui::draws.end());
  CHECK(distance->position.y==scene_pos.y+4);
  if(!toolsWrap)CHECK(distance->position.x<view->position.x);
 }
 if(view!=gui::draws.end())CHECK(view->position.y==scene_pos.y+(toolsWrap?wrappedToolsY:2.0f));
 CHECK(meshDrawWidth==(std::max)(96.0f,9.0f*8.0f+12.0f+24.0f));
 CHECK(meshDrawWidth<18.0f*8.0f+12.0f+20.0f);
 for(std::size_t i=0;i<gui::draws.size();++i){const auto&a=gui::draws[i];
  CHECK(a.position.x>=scene_pos.x&&a.position.y>=scene_pos.y&&a.position.x+a.size.x<=scene_pos.x+width+.01f&&a.position.y+a.size.y<=scene_pos.y+scene_size.y);
  for(std::size_t j=i+1;j<gui::draws.size();++j)CHECK(!same_rect_overlap(a,gui::draws[j]));}
 if(showMeshDrawMode){CHECK(gui::draws.size()>=1);if(running&&!editor.projectRuntimeSnapshot)CHECK(!gui::modeEnabled);
  const auto mode=std::find_if(gui::draws.begin(),gui::draws.end(),[](const auto&rect){return rect.label=="scene-mesh-draw-mode";});
  CHECK(mode!=gui::draws.end()&&mode->position.y+headerControlHeight+6<=result.scene_viewport.position.y);
  CHECK(result.scene_viewport.position.y+result.scene_viewport.size.y==scene_pos.y+118);}
 if(!showMeshDrawMode)CHECK(gui::mode_calls==0);
 return 0;
}
int main(){using Mode=platform::child_process::MeshDrawMode;EditorState e,other;CHECK(e.viewportMeshDrawMode==Mode::shaded&&!e.viewportMeshDrawModeRequest);
 gui::chooseView=true;gui::viewChoice=1;
 CHECK(draw(e,320,false,false,false)==0&&gui::lastViewCaption=="View");
 CHECK(previewgrid::appliedView==previewgrid::EditorView::FreeOrthographic);
 CHECK(std::any_of(gui::draws.begin(),gui::draws.end(),[](const auto& rect){return rect.label=="Reset";}));
 CHECK(draw(e,336,false,false,false)==0&&gui::lastViewCaption=="View");
 CHECK(std::any_of(gui::draws.begin(),gui::draws.end(),[](const auto& rect){return rect.label=="scene-viewport-pointer-mode";}));
 gui::fontHeight=58;gui::viewChoice=2;gui::lastViewCaption.clear();
 CHECK(draw(e,900,false,false,true)==0&&gui::lastViewCaption=="View");
 CHECK(previewgrid::appliedView==previewgrid::EditorView::Front);
 CHECK(std::any_of(gui::draws.begin(),gui::draws.end(),[](const auto& rect){return rect.label=="Reset";}));
 CHECK(std::none_of(gui::draws.begin(),gui::draws.end(),[](const auto& rect){return rect.label=="Distance 1";}));
 gui::fontHeight=22;
 gui::chooseView=false;
 gui::chooseView=true;gui::viewChoice=8;CHECK(draw(e,900,false,false,false)==0&&e.lodInspectionReturnCamera);
 gui::chooseView=false;gui::exitInspect=true;CHECK(draw(e,900,false,false,false)==0&&!e.lodInspectionReturnCamera);
 gui::exitInspect=false;e.scenePointerMode=ScenePointerMode::move_z;
 CHECK(draw(e,900,false,false,false)==0&&gui::pointerSymbol=="Move: Z");
 gui::choose=true;gui::choice=1;CHECK(draw(e,900,false,false,false)==0);CHECK(e.viewportMeshDrawModeRequest&&*e.viewportMeshDrawModeRequest==Mode::wireframe&&e.viewportMeshDrawMode==Mode::shaded);
 publish_mode(e);CHECK(e.viewportMeshDrawMode==Mode::wireframe&&!e.viewportMeshDrawModeRequest&&previewgrid::published.size()==1&&previewgrid::published[0].wireframe);
 CHECK(other.viewportMeshDrawMode==Mode::shaded);
 e.viewportMeshDrawModeRequest=Mode::solid;previewgrid::admit=false;const auto retained=previewgrid::published[0].mesh;
 publish_mode(e);CHECK(e.viewportMeshDrawMode==Mode::wireframe&&!e.viewportMeshDrawModeRequest&&previewgrid::published[0].wireframe&&previewgrid::published[0].mesh==retained&&!logs.empty());
 e.viewportMeshDrawModeRequest=Mode::solid;const auto count=previewgrid::publications;publish_mode(e,false);
 CHECK(e.viewportMeshDrawMode==Mode::wireframe&&e.viewportMeshDrawModeRequest==Mode::solid&&previewgrid::publications==count);
 previewgrid::admit=true;publish_mode(e);CHECK(e.viewportMeshDrawMode==Mode::solid&&!previewgrid::published[0].wireframe&&previewgrid::published[0].neutralSurface);
 e.viewportMeshDrawModeRequest=Mode::shaded;publish_mode(e);CHECK(e.viewportMeshDrawMode==Mode::shaded&&!previewgrid::published[0].wireframe&&!previewgrid::published[0].neutralSurface);
 const TextureMaterial opaque{scene::SceneTextureAlphaMode::opaque},cutout{scene::SceneTextureAlphaMode::cutout};
 admitMaterial=false;materialLookups=0;
 e.viewportMeshDrawModeRequest=Mode::wireframe;publish_mode(e,true,opaque);
 CHECK(materialLookups==0&&previewgrid::published.size()==1&&previewgrid::published[0].wireframe&&!previewgrid::published[0].baseColorTexture);
 e.viewportMeshDrawModeRequest=Mode::solid;publish_mode(e,true,opaque);
 CHECK(materialLookups==0&&previewgrid::published.size()==1&&previewgrid::published[0].neutralSurface&&!previewgrid::published[0].baseColorTexture);
 publish_mode(e,true,cutout);CHECK(materialLookups==1&&previewgrid::published.empty());
 admitMaterial=true;publish_mode(e,true,cutout);CHECK(materialLookups==2&&previewgrid::published.size()==1&&previewgrid::published[0].neutralSurface&&previewgrid::published[0].baseColorTexture);
 e.viewportMeshDrawModeRequest=Mode::shaded;publish_mode(e,true,opaque);
 CHECK(materialLookups==3&&previewgrid::published.size()==1&&!previewgrid::published[0].neutralSurface&&previewgrid::published[0].baseColorTexture);
 admitMaterial=false;publish_mode(e,true,opaque);CHECK(materialLookups==4&&previewgrid::published.empty());
 admitMaterial=true;
 e.viewportMeshDrawModeRequest=Mode::solid;publish_mode(e);
 gui::illegal=true;CHECK(draw(e,900,false,true,false)==0);CHECK(!e.viewportMeshDrawModeRequest);
 e.viewportMeshDrawModeRequest=Mode::wireframe;CHECK(draw(e,900,true,false,false)==0);CHECK(!e.viewportMeshDrawModeRequest&&e.viewportMeshDrawMode==Mode::solid);
 // Runtime dropdown must describe the last published acknowledgement,
 // never claim the selected mode before the child applies it.
 gui::illegal=false;using Status=platform::child_process::RuntimeControlStatus;
 e.projectRuntimeSnapshot=platform::child_process::ProcessSnapshot{{Status::accepted,Mode::solid}};
 CHECK(draw(e,900,true,false,false)==0);CHECK(e.runtimeRequests==1&&e.lastRuntimeMode==Mode::wireframe&&e.viewportMeshDrawMode==Mode::solid&&gui::lastModeLabel=="Solid");
 e.projectRuntimeSnapshot->runtime_control={Status::pending,Mode::solid};
 CHECK(draw(e,900,true,false,false)==0);CHECK(e.runtimeRequests==1&&!gui::modeEnabled&&gui::lastModeLabel=="Solid (wait)");
 gui::choose=false;e.projectRuntimeSnapshot->runtime_control={Status::accepted,Mode::wireframe};
 CHECK(draw(e,900,true,false,false)==0);CHECK(gui::modeEnabled&&gui::lastModeLabel=="Wireframe");
 e.projectRuntimeSnapshot->runtime_control={Status::rejected,Mode::wireframe};
 CHECK(draw(e,900,true,false,false)==0);CHECK(gui::lastModeLabel=="Wireframe (refused)");
 e.projectRuntimeSnapshot->runtime_control={Status::disconnected,Mode::wireframe};
 CHECK(draw(e,900,true,false,false)==0);CHECK(!gui::modeEnabled&&gui::lastModeLabel=="Disconnected");
 e.projectRuntimeSnapshot->runtime_control={Status::accepted,Mode::shaded};
 CHECK(draw(e,900,true,false,false)==0&&gui::lastModeLabel=="Materials");
 gui::choose=true;gui::choice=2;CHECK(draw(e,900,true,false,false)==0&&e.lastRuntimeMode==Mode::shaded);
 e.projectRuntimeSnapshot.reset();gui::choose=false;gui::illegal=false;
 cycleMeshMode=true;e.viewportMeshDrawMode=Mode::solid;
 CHECK(draw(e,900,false,false,false)==0&&e.viewportMeshDrawModeRequest==Mode::wireframe);
 CHECK(draw(e,900,false,false,false)==0&&e.viewportMeshDrawModeRequest==Mode::shaded);
 CHECK(draw(e,900,false,false,false)==0&&e.viewportMeshDrawModeRequest==Mode::solid);
 e.projectRuntimeSnapshot=platform::child_process::ProcessSnapshot{{Status::accepted,Mode::solid}};
 CHECK(draw(e,900,true,false,false)==0&&e.lastRuntimeMode==Mode::wireframe);
 const auto requests=e.runtimeRequests;e.projectRuntimeSnapshot->runtime_control={Status::pending,Mode::solid};
 CHECK(draw(e,900,true,false,false)==0&&e.runtimeRequests==requests);
 cycleMeshMode=false;e.projectRuntimeSnapshot.reset();
 for(float fontHeight:{16.f,22.f,38.f,58.f})for(float width:{0.f,79.f,80.f,120.f,200.f,319.f,320.f,335.f,336.f,337.f,400.f,440.f,650.f,900.f})for(bool running:{false,true})for(bool run:{false,true})for(auto surface:{EditorMainSurface::Scene,EditorMainSurface::Game2D,EditorMainSurface::ForestFactory}){
  gui::fontHeight=fontHeight;e.mainSurface=surface;gui::mode_calls=0;CHECK(draw(e,width,running,false,run)==0);}
 return 0;}
'''
  for name,value in {'SELECT_WIDTH':select_width,'HEIGHT_METRIC':height_metric,'FIELDS':fields,'MARKER_TYPE':marker_type,'REQUEST':request,'MATERIAL_MODE':material_mode,'MARKER':marker,'NEUTRAL_MODE':neutral,'PUBLISH':publish,'TYPES':gui_types,'LAYOUT':layout,'CONTROLS':controls,'CAPTION_RESERVATION':caption_reservation}.items():cpp=cpp.replace(name,value)
  lane=ROOT/'Artifacts/Reports/preview-contracts-oct31/mesh'/hashlib.sha256(cpp.encode()).hexdigest()[:12];lane.mkdir(parents=True,exist_ok=True)
  source=lane/'editor.mesh_draw_contract.cpp';source.write_text(cpp,encoding='utf-8')
  results=compile_and_run(self,source,lane)
  (lane/'result.json').write_text(json.dumps({'passed':True,'cases':results,'scope':'Actual header layout/draw and marker publication/request/commit phases with explicit GUI and renderer-admission CPU ports; no native/backend acceptance.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
