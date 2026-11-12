"""Compile Forest Factory's actual action block; no window or renderer starts."""
from pathlib import Path
import shutil
import tempfile
import unittest
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class ForestPreviewActions(unittest.TestCase):
    def test_controls_keep_a_bounded_independent_tree_viewport(self):
        from Tests.build.test_opengl_sprite_stream import definition
        viewport = (ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        actual = definition(viewport, '            const auto forestPanelBounds =', True)
        self.assertIn('gui::begin_scroll_area({.id="forest-authoring-controls"', viewport)
        self.assertIn('gui::set_cursor(panels[1].position)', viewport)
        source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
namespace gui {struct Vec2 {float x{},y{};};struct WidgetBounds {Vec2 position,size;};}
LAYOUT
int main(){
 for(float width:{0.f,80.f,320.f,599.f,600.f,800.f,1600.f})
  for(float height:{0.f,80.f,200.f,600.f,1000.f}){
   const gui::Vec2 origin{17,29};const auto panels=forestPanelBounds(origin,{width,height});
   for(const auto& panel:panels){
    assert(panel.size.x>=0&&panel.size.y>=0);
    assert(panel.position.x>=origin.x&&panel.position.y>=origin.y);
    assert(panel.position.x+panel.size.x<=origin.x+width+.001f);
    assert(panel.position.y+panel.size.y<=origin.y+height+.001f);
   }
   const auto& controls=panels[0];const auto& scene=panels[1];
   assert(controls.position.x==origin.x&&controls.position.y==origin.y);
   assert(controls.position.x+controls.size.x<=scene.position.x
       ||controls.position.y+controls.size.y<=scene.position.y);
   if(width>=600){assert(scene.size.y==height&&scene.size.x>=width*.38f);}
   else {assert(scene.size.x==width&&scene.size.y>=std::max(0.f,height-8.f)*.59f);}
  }
}
'''.replace('LAYOUT',actual)
        self._compile_and_run(source)

    def test_scene_header_backing_covers_previous_workspace_without_covering_scene(self):
        from Tests.build.test_opengl_sprite_stream import definition
        viewport = (ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        actual = definition(viewport, '            if (editor.mainSurface == EditorMainSurface::ForestFactory && !editor.forestTerrainPainting)')
        forest = (ROOT/'Engine/modules/forest/forest.factory.ixx').read_text(encoding='utf-8')
        forest_types = '\n'.join(definition(forest, marker, True) for marker in (
            '    enum class ForestProfileProperty', '    struct ForestProfileEdit', '    struct ForestAssetDocument',
            '    struct ForestCanopySettings final'))
        forest_functions = '\n'.join(definition(forest, marker) for marker in (
            '    [[nodiscard]] inline float forest_profile_value(', '    inline void set_forest_profile_value(',
            '    [[nodiscard]] inline bool apply_forest_profile_edit(',
            '    [[nodiscard]] inline bool undo_forest_profile_edit(',
            '    [[nodiscard]] inline bool redo_forest_profile_edit(',
            '    [[nodiscard]] inline ForestCanopySettings forest_canopy_settings('))
        for name in ('apply', 'undo', 'redo'):
            forest_functions = forest_functions.replace(name+'_forest_profile_edit', name+'_actual_forest_profile_edit')
        state = (ROOT/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
        draft = definition(state, '            struct ForestProfileSliderDraft final', True)
        workspace = (ROOT/'Engine/src/editor/application/editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
        leaving = definition(workspace, '            if (changedSurface && surface != EditorMainSurface::ForestFactory)')
        source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "core.format_text.hpp"
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
enum class EditorMainSurface {ForestFactory,Scene,Project};
enum class EditorCommand {None,OpenContextWindow};
struct Context {int type=7;};
struct Result {EditorCommand command{};std::string command_argument;int requested_context_type{};};
unsigned previewCalls{},previewBuilds{},editCalls{},undoCalls{},redoCalls{};
namespace forest {
enum class ForestPreviewMode {Mode2D,Mode3D};
struct ForestTextureReference {std::string logicalPath;};
struct ForestGenomeId {std::uint64_t stableHash{};};
struct ForestFactoryProfile {
 struct {float targetHeightMeters=3,trunkRadiusMeters=.1f;}config;
 struct {unsigned levels=2,childrenPerNode=2;float startHeightMeters{},branchLengthMeters=1,
  nodeStepMeters=.25f,lengthReferenceMeters{},angleDegrees=30,spreadDegrees=45;}branch;
 struct {float durationSeconds=1,timeSeconds=1;}temporal;
 float leafSizeMeters=.28f;unsigned leafSitesPerBranch=6,barkTileIndex{};
 ForestPreviewMode previewMode=ForestPreviewMode::Mode3D;
 ForestTextureReference barkTexture,foliageTexture;
};
FOREST_TYPES
inline constexpr std::size_t kForestMaximumProfileEdits=512;
// Hashing is a recording port; the actual profile setters/journal/Undo/Redo
// below execute unchanged against these dependency-light profile inputs.
std::uint64_t forest_profile_content_hash(const ForestFactoryProfile& p){
 std::uint64_t h=p.branch.levels;
 for(float v:{p.config.targetHeightMeters,p.branch.branchLengthMeters,p.leafSizeMeters,
     float(p.leafSitesPerBranch),float(p.previewMode),float(p.barkTileIndex)})h=h*1000003+std::bit_cast<std::uint32_t>(v);
 return h;
}
FOREST_FUNCTIONS
bool apply_forest_profile_edit(ForestAssetDocument& d,ForestProfileProperty p,float v){++editCalls;return apply_actual_forest_profile_edit(d,p,v);}
bool undo_forest_profile_edit(ForestAssetDocument& d){++undoCalls;return undo_actual_forest_profile_edit(d);}
bool redo_forest_profile_edit(ForestAssetDocument& d){++redoCalls;return redo_actual_forest_profile_edit(d);}
namespace preview_mesh {constexpr unsigned kBarkTileCount=16;}
template<class T>std::size_t forest_area_candidate_count(const T&){return 5;}
}
namespace previewgrid {int capture_camera_rig_snapshot(Context*){return 1;}bool restore_camera_rig_snapshot(Context*,int){return true;}}
struct Editor {EditorMainSurface mainSurface=EditorMainSurface::ForestFactory;
 bool project=true,forestTerrainPainting=false,plantLabDocumentInitialized=true;
 forest::ForestAssetDocument plantLabDocument;
 DRAFT
 std::optional<ForestProfileSliderDraft> forestProfileSliderDraft;
 bool forestExtendedRange{};std::string projectId="project",projectRoot="checkout/project";
 std::uint64_t compiledRevision{},compiledHash{};
 int forestDesignCamera{};std::optional<int> forestProjectCamera;
 std::string plantLabStatus;
 std::optional<bool> forestPlacementSaved;
 struct {double radiusMeters=12,densityPerSquareMeter=.01;unsigned long long seed=1337;}forestArea;
 std::optional<int> forestSurfacePending,forestSurfacePlacement;bool projectBuildPending{},planetaryGeneratePending{};
 struct {bool valid()const{return false;}}projectRuntimeProcess;
 struct Scheduler {bool cancel(int){return true;}};Scheduler* taskScheduler{};int forestSurfaceCancellation{};
};
bool paint_forest_factory_area(Editor&){assert(false);return false;}
bool erase_selected_forest_area(Editor&){assert(false);return false;}
bool has_open_project(const Editor& editor){return editor.project;}
bool ensure_forest_factory_preview(Editor& editor){
 ++previewCalls;if(editor.compiledRevision!=editor.plantLabDocument.revision||editor.compiledHash!=editor.plantLabDocument.contentHash){
  ++previewBuilds;editor.compiledRevision=editor.plantLabDocument.revision;editor.compiledHash=editor.plantLabDocument.contentHash;}
 return editor.plantLabDocumentInitialized&&editor.plantLabDocument.profile.branch.levels<=7;
}
bool refresh_forest_factory_instances(Editor&){assert(false);return false;}
bool begin_forest_surface_refresh(Editor&){assert(false);return false;}
bool begin_forest_project_creation(Editor&){assert(false);return false;}
namespace gui {
struct Vec2 {float x{},y{};};
struct InlineButtonSpec {std::string_view label;float width;bool enabled=true;};
enum class InlineButtonSizing {Fill,FitContent};
enum class InlineButtonAlignment {Start,End,Center};
struct InlineButtonRowOptions {InlineButtonSizing sizing{};InlineButtonAlignment alignment{};
 bool wrap{};float available_width{};};
struct Rect {Vec2 position,size;};
inline Vec2 cursor;inline float available,headingWidth;inline std::vector<Rect> panels;
inline std::string_view requested;inline std::vector<InlineButtonSpec> seen;
struct SliderOptions {std::string_view id,label;float minimum{},maximum{},value{},step{};Vec2 size;bool enabled=true;};
struct SliderResult {float value{};bool hovered{},changed{};};
inline std::vector<SliderOptions> sliders;
inline std::vector<std::string> labels;
inline bool down{},pressed{},released{},sliderHovered=true;
inline std::string_view sliderRequested,captured;
inline std::optional<float> sliderValue;
bool is_mouse_down(){return down;}
bool was_mouse_pressed(){return pressed;}
bool was_mouse_released(){return released;}
Vec2 cursor_position(){return cursor;}
void set_cursor(Vec2 position){cursor=position;}
float line_height(){return 18;}
float measure_wrapped_label_height(std::string_view text,float width){
 return 18*std::max(1.0f,std::ceil(float(text.size())*7/width));
}
void panel_rect(Vec2 p,Vec2 size){assert(size.x==available&&size.y>=0);panels.push_back({p,size});}
void foreground(float height,float width=-1){
 if(width<0)width=available;
 // Recording renderer: every old Project pixel behind this foreground must
 // already have an opaque panel; no frame/global clear is being assumed.
 for(float y=cursor.y+0.25f;y<cursor.y+height;y+=1)
  for(float x=cursor.x+0.25f;x<cursor.x+width;x+=1){
   bool covered=false;for(const auto& r:panels)covered|=x>=r.position.x&&x<r.position.x+r.size.x
      &&y>=r.position.y&&y<r.position.y+r.size.y;assert(covered);
  }
 cursor.y+=height;
}
void label(std::string_view){foreground(line_height());}
void wrapped_label(std::string_view text,float width){labels.emplace_back(text);foreground(measure_wrapped_label_height(text,width),width);}
SliderResult slider(const SliderOptions& options){
 assert(options.maximum>options.minimum&&options.step>0&&options.size.x<=available);
 sliders.push_back(options);foreground(options.size.y+6,options.size.x);
 SliderResult result{options.value,options.enabled&&sliderHovered&&sliderRequested==options.id};
 if(result.hovered&&pressed){captured=options.id;pressed=false;}
 if(options.enabled&&captured==options.id&&(down||(released&&result.hovered))&&sliderValue)
  result.value=std::clamp(*sliderValue,options.minimum,options.maximum);
 result.changed=result.value!=options.value;
 if(released&&captured==options.id){if(result.hovered)released=false;captured={};}
 return result;
}
float measure_section_heading_height(std::string_view text,float width){headingWidth=width;return measure_wrapped_label_height(text,width)*1.22f+8;}
enum class SemanticTone{neutral,success,warning};
SemanticTone placementTone{};
void section_heading(std::string_view text,SemanticTone tone=SemanticTone::neutral){
 if(text.starts_with("Last tree placement"))placementTone=tone;
 foreground(measure_wrapped_label_height(text,headingWidth)*1.22f+8);
}
float inline_button_row_height(std::span<const InlineButtonSpec> items,float height,float gap,const InlineButtonRowOptions& options){
 assert(options.sizing==InlineButtonSizing::FitContent&&options.alignment==InlineButtonAlignment::Center&&options.wrap);
 assert(options.available_width>0&&options.available_width<=available);
 float x=0,rows=1;for(const auto& item:items){assert(item.width==0);
  const float w=std::min(float(item.label.size())*7+24,options.available_width);
  if(x>0&&x+w>options.available_width){++rows;x=0;}x+=w+gap;}return rows*height+(rows-1)*gap;
}
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> items,float height,float gap,const InlineButtonRowOptions& options){
 foreground(inline_button_row_height(items,height,gap,options)+8);
 seen.insert(seen.end(),items.begin(),items.end());
 for(std::size_t i=0;i<items.size();++i)if(items[i].label==requested&&items[i].enabled){requested={};return i;}
 return {};
}
bool enabled(std::string_view label){for(const auto& spec:seen)if(spec.label==label)return spec.enabled;assert(false);return false;}
const SliderOptions& control(std::string_view id){for(const auto& spec:sliders)if(spec.id==id)return spec;assert(false);return sliders[0];}
}
struct Input {bool down{},pressed{},released{},hovered=true;std::string_view slider;std::optional<float> value;};
void paint(Editor& editor,float width,std::string_view action={},Input input={}){
 Result result;auto ctx=std::make_unique<Context>();
 const gui::Vec2 viewport_pos{31,71},viewport_size{width,1600};
 const auto forestControlPosition=viewport_pos,forestControlSize=viewport_size;
 gui::cursor=viewport_pos;gui::available=width;gui::panels.clear();gui::seen.clear();gui::sliders.clear();gui::labels.clear();gui::requested=action;
 gui::placementTone=gui::SemanticTone::neutral;
 gui::down=input.down;gui::pressed=input.pressed;gui::released=input.released;gui::sliderHovered=input.hovered;
 gui::sliderRequested=input.slider;gui::sliderValue=input.value;
 if(!input.down&&!input.released)gui::captured={}; // Native focus/ownership cancellation.
 auto selected_forest_species_target=[](const Editor&)->const Editor*{return nullptr;};
 auto refresh_selected_forest_material=[](Editor&){assert(false);return false;};
 auto place_forest_factory_asset=[](Editor&){assert(false);return std::size_t{};};
 auto save_editor_scene_snapshot=[](Editor&){assert(false);return false;};
 auto apply_editor_surface=[](EditorMainSurface,std::string_view){assert(false);};
 auto focus_forest_factory_placement=[](Context*,Editor&){assert(false);return false;};
 const auto paint=[&]{
ACTUAL
 };
 paint();
 assert(gui::panels.size()==10+std::size_t(editor.forestPlacementSaved.has_value()));
 assert(gui::placementTone==(editor.forestPlacementSaved
    ?(*editor.forestPlacementSaved?gui::SemanticTone::success:gui::SemanticTone::warning):gui::SemanticTone::neutral));
 float end=viewport_pos.y;
 for(const auto& rect:gui::panels){assert(rect.position.x==viewport_pos.x&&std::abs(rect.position.y-end)<.001f);
  end+=rect.size.y;assert(end<=gui::cursor.y+.001f);}
 assert(std::abs(end-gui::cursor.y)<.001f); // The scene starts here; none of its pixels are covered.
 assert(gui::sliders.size()==5);
}
void reset_counters(){previewCalls=previewBuilds=editCalls=undoCalls=redoCalls=0;gui::captured={};}
Editor seeded(unsigned levels=2,bool initialized=true){
 Editor editor;editor.plantLabDocumentInitialized=initialized;
 editor.plantLabDocument.profile.branch.levels=levels==8?7:levels;
 editor.plantLabDocument.contentHash=forest::forest_profile_content_hash(editor.plantLabDocument.profile);
 if(levels==8)assert(forest::apply_actual_forest_profile_edit(editor.plantLabDocument,forest::ForestProfileProperty::BranchLevels,8));
 return editor;
}
void run(float width,std::string status,bool project,std::string_view action={},unsigned levels=2,bool initialized=true){
 auto editor=seeded(levels,initialized);editor.project=project;editor.plantLabStatus=std::move(status);reset_counters();
 paint(editor,width,action,action=="Reduce branches"?Input{false,true,true,true,"forest.branch_levels",7.f}:Input{});
 if(levels==8||!initialized){
  // Rejecting a preview must not disable the actions that repair the document.
  for(auto label:{"2D Plant","3D Plant",
      "Previous bark","Next bark","Undo","Redo","Advanced Plant Lab"})assert(gui::enabled(label)==initialized);
  for(const auto& control:gui::sliders)assert(control.enabled==initialized);
  const bool repaired=initialized&&(action=="Reduce branches"||action=="Undo");
  assert(gui::enabled(project?"Add to Scene":"Create Project & Add Plant...")==bool(initialized&&action=="Reduce branches"));
  assert(gui::enabled("Update Painted Trees")==bool(repaired&&project));
  assert(gui::enabled("Paint Forest on Terrain")==bool(repaired&&project));
  assert(gui::enabled("Back to Scene"));
 }
 if(!initialized){assert(editCalls==0&&undoCalls==0&&redoCalls==0);return;}
 if(action=="Reduce branches"){assert(editCalls==1&&editor.plantLabDocument.profile.branch.levels==7&&previewCalls==2);}
 if(action=="Undo"){assert(undoCalls==1&&editor.plantLabDocument.profile.branch.levels==7&&previewCalls==2);}
 if(action=="Reduce branches"||action=="Undo"){
  paint(editor,width);
  assert(gui::enabled(project?"Add to Scene":"Create Project & Add Plant..."));
  assert(gui::enabled("Update Painted Trees")==project);
  assert(gui::enabled("Paint Forest on Terrain")==project);
 }
}
void gestures(float width){
 using P=forest::ForestProfileProperty;
 auto editor=seeded();reset_counters();paint(editor,width);
 const auto before=editor.plantLabDocument;
 // Pressing the current value still binds the document identity immediately.
 paint(editor,width,{},Input{true,true,false,true,"forest.height",3.f});
 assert(editor.forestProfileSliderDraft&&editor.forestProfileSliderDraft->revision==before.revision);
 for(float value:{4.f,5.f,6.f})paint(editor,width,{},Input{true,false,false,true,"forest.height",value});
 assert(editCalls==0&&previewBuilds==1&&editor.plantLabDocument.journal.empty());
 assert(editor.plantLabDocument.profile.config.targetHeightMeters==3.f);
 paint(editor,width,{},Input{false,false,true,true,"forest.height",6.5f});
 assert(!gui::released); // The actual action samples release before the slider consumes it.
 assert(editCalls==1&&previewBuilds==2&&editor.plantLabDocument.journal.size()==1);
 assert(editor.plantLabDocument.profile.config.targetHeightMeters==6.5f&&!editor.forestProfileSliderDraft);
 paint(editor,width,"Undo");assert(undoCalls==1&&editor.plantLabDocument.profile.config.targetHeightMeters==3.f);
 paint(editor,width,"Redo");assert(redoCalls==1&&editor.plantLabDocument.profile.config.targetHeightMeters==6.5f);
 // Release outside retains the last dragged value, and commits only once.
 paint(editor,width,{},Input{true,true,false,true,"forest.branch_length",2.f});
 paint(editor,width,{},Input{true,false,false,false,"forest.branch_length",2.5f});
 paint(editor,width,{},Input{false,false,true,false,"forest.branch_length",7.f});
 assert(editCalls==2&&editor.plantLabDocument.profile.branch.branchLengthMeters==2.5f);
 paint(editor,width);assert(editCalls==2);
 // Focus loss/ownership cancellation is not a release and never writes history.
 auto journalSize=editor.plantLabDocument.journal.size();
 paint(editor,width,{},Input{true,true,false,true,"forest.height",10.f});paint(editor,width);
 assert(!editor.forestProfileSliderDraft&&editor.plantLabDocument.journal.size()==journalSize&&editCalls==2);
 // A changed source, Undo, or project identity invalidates the whole gesture;
 // holding the pointer must not start a fresh draft against its replacement.
 for(int stale:{0,1,2,3}){
  paint(editor,width,{},Input{true,true,false,true,"forest.height",11.f});
  if(stale==0)++editor.plantLabDocument.revision;
  if(stale==1)++editor.plantLabDocument.contentHash;
  if(stale==2)assert(forest::undo_actual_forest_profile_edit(editor.plantLabDocument));
  if(stale==3)editor.projectRoot+="/other";
  const auto height=editor.plantLabDocument.profile.config.targetHeightMeters;
  paint(editor,width,{},Input{true,false,false,true,"forest.height",12.f});
  assert(editor.forestProfileSliderDraft&&editor.forestProfileSliderDraft->cancelled);
  paint(editor,width,{},Input{false,false,true,true,"forest.height",13.f});
  assert(!editor.forestProfileSliderDraft&&editor.plantLabDocument.profile.config.targetHeightMeters==height&&editCalls==2);
 }
 // A click/tap can press and release in the same frame.
 paint(editor,width,{},Input{false,true,true,true,"forest.leaf_size",.5f});
 assert(editCalls==3&&editor.plantLabDocument.profile.leafSizeMeters==.5f);
 // The actual surface transition discards a live draft.
 paint(editor,width,{},Input{true,true,false,true,"forest.height",20.f});
 const bool changedSurface=true;const auto surface=EditorMainSurface::Scene;
 LEAVING
 assert(!editor.forestProfileSliderDraft);
 // Standard ranges, full validated ranges, and legacy effective foliage values.
 editor=seeded();reset_counters();paint(editor,width);
 assert(gui::control("forest.height").minimum==.1f&&gui::control("forest.height").maximum==32.f);
 assert(gui::control("forest.branch_levels").step==1.f&&gui::control("forest.branch_levels").maximum==8.f);
 assert(gui::control("forest.branch_length").minimum==.01f&&gui::control("forest.branch_length").maximum==8.f);
 paint(editor,width,"Standard range");paint(editor,width);
 assert(editor.forestExtendedRange&&gui::control("forest.height").maximum==256.f
  &&gui::control("forest.branch_levels").maximum==32.f&&gui::control("forest.branch_length").maximum==128.f);
 paint(editor,width,"Extended range");paint(editor,width);assert(!editor.forestExtendedRange);
 editor.plantLabDocument.profile.config.targetHeightMeters=128;
 editor.plantLabDocument.profile.branch.levels=31;editor.plantLabDocument.profile.branch.branchLengthMeters=64;
 paint(editor,width);assert(editor.forestExtendedRange&&!gui::enabled("Extended range"));
 assert(gui::control("forest.height").value==128&&gui::control("forest.branch_levels").value==31
  &&gui::control("forest.branch_length").value==64&&editCalls==0);
 editor=seeded();editor.plantLabDocument.profile.leafSizeMeters=0;editor.plantLabDocument.profile.leafSitesPerBranch=0;
 paint(editor,width);assert(gui::control("forest.leaf_size").value==.28f&&gui::control("forest.leaf_sites").value==6.f);
 assert(gui::control("forest.leaf_size").minimum==.02f&&gui::control("forest.leaf_sites").minimum==1.f);
 assert(std::ranges::find(gui::labels,"Height: 3.00 m")!=gui::labels.end());
 assert(std::ranges::find(gui::labels,"Leaf groups per branch: 6")!=gui::labels.end());
 assert(std::ranges::any_of(gui::labels,[](const auto& label){return label.starts_with("Tip-only leaves.");}));
 paint(editor,width,{},Input{false,true,true,true,"forest.leaf_sites",8.f});
 assert(editor.plantLabDocument.profile.leafSitesPerBranch==8&&forest::forest_canopy_settings(editor.plantLabDocument.profile).enabled);
}
int main(){for(float width:{160.0f,320.0f,900.0f})for(bool project:{false,true}){
 run(width,"Compiled revision 7 into 64 segments and 30 leaves.",project);
 run(width,std::string(240,'S'),project);run(width,"Ready.",project);
 for(auto action:{"Reduce branches","Undo","Add to Scene","Create Project & Add Plant...","Update Painted Trees","Paint Forest on Terrain"}){
  run(width,"Preview geometry refused; previous preview retained.",project,action,8);
  run(width,"Source reopen refused.",project,action,8,false);
 }}for(float width:{160.f,320.f,900.f}){
  gestures(width);
  auto editor=seeded();editor.plantLabStatus="A tree was added.";
  editor.forestPlacementSaved=true;paint(editor,width);
  editor.forestPlacementSaved=false;paint(editor,width);
 }}
'''.replace('ACTUAL', actual).replace('FOREST_TYPES', forest_types).replace('FOREST_FUNCTIONS', forest_functions).replace('DRAFT', draft).replace('LEAVING', leaving)
        self._compile_and_run(source)

    def test_inline_actions_require_project_and_advanced_lab_is_explicit(self):
        viewport = (ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        start = viewport.index('                const std::array<gui::InlineButtonSpec, 5> actions')
        end = viewport.index('                const std::array paintModeActions', start)
        layout_start = viewport.index('                const gui::InlineButtonRowOptions forestButtons')
        layout_end = viewport.index('};', layout_start) + 2
        actual = viewport[layout_start:layout_end] + '\n' + viewport[start:end]
        source = r'''
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
enum class EditorMainSurface { Scene,PlantLab,Project };
enum class EditorCommand { None,OpenContextWindow };
struct Context { int type=7; };
struct Result { EditorCommand command{};std::string command_argument;int requested_context_type{}; };
struct Editor { bool project;int plantLabDocument{};
 std::optional<int> forestSurfacePending;bool projectBuildPending{},planetaryGeneratePending{};
 struct {bool running{};bool valid()const{return running;}}projectRuntimeProcess;
};
bool has_open_project(const Editor& value) { return value.project; }
namespace forest {
inline unsigned undoCalls{},redoCalls{};
bool undo_forest_profile_edit(int&) {++undoCalls;return true;}
bool redo_forest_profile_edit(int&) {++redoCalls;return true;}
}
namespace gui {
struct InlineButtonSpec { std::string_view label;float width;bool enabled=true; };
enum class InlineButtonSizing {Fill,FitContent};
enum class InlineButtonAlignment {Start,End,Center};
struct InlineButtonRowOptions {InlineButtonSizing sizing{};InlineButtonAlignment alignment{};
 bool wrap{};float available_width{};};
inline std::size_t requested;
inline std::array<InlineButtonSpec,5> seen;inline bool refreshEnabled,placementRefreshEnabled,materialRefreshEnabled;
template<std::size_t N>float inline_button_row_height(const std::array<InlineButtonSpec,N>& specs,float,float,const InlineButtonRowOptions& options){
 assert(options.sizing==InlineButtonSizing::FitContent&&options.alignment==InlineButtonAlignment::Center&&options.wrap&&options.available_width==320);
 for(const auto& spec:specs)assert(spec.width==0);return 28;}
template<std::size_t N>std::optional<std::size_t> inline_button_row(const std::array<InlineButtonSpec,N>& specs,float height,float gap,const InlineButtonRowOptions& options) {
 (void)inline_button_row_height(specs,height,gap,options);
 if constexpr(N==5){seen=specs;return requested<5&&specs[requested].enabled ? std::optional{requested}:std::nullopt;}
 else {static_assert(N==3);assert(specs[0].label=="Update Painted Trees");refreshEnabled=specs[0].enabled;
 assert(specs[1].label=="Refresh Forest Placement");placementRefreshEnabled=specs[1].enabled;
 assert(specs[2].label=="Refresh Selected Tree Material");materialRefreshEnabled=specs[2].enabled;
 return requested>=5&&requested<8&&specs[requested-5].enabled?std::optional<std::size_t>{requested-5}:std::nullopt;}
}
}
void run(bool project,bool ready,std::size_t action,int busy=0,bool editable=true) {
 const bool initialReady=ready;
 Editor editor{project};if(busy==1)editor.forestSurfacePending=1;editor.projectBuildPending=busy==2;editor.projectRuntimeProcess.running=busy==3;editor.planetaryGeneratePending=busy==4;auto ctx=std::make_unique<Context>();Result result;
 unsigned placementCalls{},saveCalls{},focusCalls{},surfaceCalls{},refreshCalls{},creationCalls{},paintedRefreshCalls{},placementRefreshCalls{},materialRefreshCalls{};
 forest::undoCalls=forest::redoCalls=0;
 EditorMainSurface surface=EditorMainSurface::Scene;
 gui::requested=action;
 auto place_forest_factory_asset=[&](Editor&) { ++placementCalls;return std::size_t{1}; };
 auto save_editor_scene_snapshot=[&](Editor&) { ++saveCalls;return true; };
 auto ensure_forest_factory_preview=[&](Editor&) {++refreshCalls;return true;};
 auto refresh_forest_factory_instances=[&](Editor&) {++paintedRefreshCalls;return true;};
 auto begin_forest_surface_refresh=[&](Editor&) {++placementRefreshCalls;return true;};
 auto selected_forest_species_target=[&](const Editor& candidate) {return editable ? &candidate : nullptr;};
 auto refresh_selected_forest_material=[&](Editor&) {++materialRefreshCalls;return true;};
 auto begin_forest_project_creation=[&](Editor&) {++creationCalls;return true;};
 auto apply_editor_surface=[&](EditorMainSurface value,std::string_view) { ++surfaceCalls;surface=value; };
 auto focus_forest_factory_placement=[&](Context*,Editor&) { assert(surfaceCalls==1&&surface==EditorMainSurface::Scene);++focusCalls;return true; };
 auto paintForestHeader=[](float){};
 const float forestTextWidth=320;
ACTUAL
 assert(gui::seen[0].enabled==editable&&gui::seen[1].enabled==editable);
 assert(gui::seen[2].enabled==initialReady);
 assert(gui::seen[2].label==(project?"Add to Scene":"Create Project & Add Plant..."));
 assert(gui::seen[3].enabled&&gui::seen[4].enabled==editable);
 assert(gui::refreshEnabled==(ready&&project&&(busy==0||busy==4)));
 assert(gui::placementRefreshEnabled==(project&&busy==0));
 assert(gui::materialRefreshEnabled==(project&&busy==0&&editable));
 if(action>=5){assert(paintedRefreshCalls==(action==5&&ready&&project&&(busy==0||busy==4)?1u:0u));
  assert(placementRefreshCalls==(action==6&&project&&busy==0?1u:0u));
  assert(materialRefreshCalls==(action==7&&project&&busy==0&&editable?1u:0u));
  assert(placementCalls==0&&saveCalls==0&&surfaceCalls==0&&focusCalls==0&&refreshCalls==0);return;}
 assert(paintedRefreshCalls==0);
 if(!gui::seen[action].enabled) {
  assert(placementCalls==0&&saveCalls==0&&surfaceCalls==0&&focusCalls==0);return;
 }
 if(action==0)assert(forest::undoCalls==1&&refreshCalls==1&&placementCalls==0&&surfaceCalls==0);
 if(action==1)assert(forest::redoCalls==1&&refreshCalls==1&&placementCalls==0&&surfaceCalls==0);
 if(action==2&&project)assert(placementCalls==1&&saveCalls==1&&surfaceCalls==1&&surface==EditorMainSurface::Scene&&focusCalls==1&&creationCalls==0);
 if(action==2&&!project)assert(placementCalls==0&&saveCalls==0&&surfaceCalls==1&&surface==EditorMainSurface::Project&&focusCalls==0&&creationCalls==1);
 if(action==3)assert(placementCalls==0&&saveCalls==0&&surfaceCalls==1&&surface==EditorMainSurface::Scene&&focusCalls==0);
 if(action==4)assert(placementCalls==0&&saveCalls==0&&surfaceCalls==0&&focusCalls==0
  &&result.command==EditorCommand::OpenContextWindow&&result.command_argument=="application.plant_lab"&&result.requested_context_type==7);
}
int main() {
 for(bool project:{false,true})for(bool ready:{false,true})
  for(std::size_t action:{0u,1u,2u,3u,4u,5u,6u,7u})for(int busy:{0,1,2,3,4})for(bool editable:{false,true})run(project,ready,action,busy,editable);
}
'''.replace('ACTUAL', actual)
        self._compile_and_run(source)

    def _compile_and_run(self, source):
        # A failing CPU contract must return its generated line, not open a CRT
        # assertion dialog over the operator's running Editor.
        source = source.replace('#include <cassert>', '#include <cassert>\n#include <cstdlib>\n#undef assert\n#define assert(condition) do { if(!(condition)) std::_Exit(__LINE__); } while(false)')
        parent = ROOT/'Artifacts/Intermediate/test-fixtures'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='forest-preview-actions-', dir=parent))
        try:
            cpp = scratch/'contract.cpp'
            cpp.write_text(source, encoding='utf-8')
            compile_and_run(self, cpp, scratch, includes=[ROOT / 'Engine/include'], compile_timeout=45, run_timeout=10)
        except BaseException:
            # Keep the complete generated source and earliest build/run logs.
            print(f'Forest action fixture retained: {scratch}', flush=True)
            raise
        else:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
