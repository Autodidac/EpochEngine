"""Real Editor drag/selection/commit with deterministic input and real scene history."""
from pathlib import Path
import hashlib,json,re,subprocess,time,unittest
from Tests.support.cpp import configure_arguments,executable
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

class SceneObjectDrag(unittest.TestCase):
 def test_constraints_selection_and_single_history_gesture(self):
  folder=ROOT/'Engine/src/editor/application'
  source=(folder/'editor.application_document_editing.inl').read_text(encoding='utf-8')
  selection=(folder/'editor.application_scene_selection.inl').read_text(encoding='utf-8')
  selection_functions='\n'.join(definition(selection,marker) for marker in (
   '        [[nodiscard]] std::optional<std::size_t> editor_entity_index(',
   '        void clear_editor_selection(',
   '        [[nodiscard]] bool editor_entity_selected(',
   '        void add_editor_entity_selection(',
   '        void toggle_editor_entity_selection('))
  plane=source.index('        [[nodiscard]] std::optional<epochengine::previewgrid::Vec3> screen_point_to_world_plane(')
  normal=source.index('        [[nodiscard]] epochengine::previewgrid::Vec3 scene_drag_plane_normal(',plane)
  functions=source[plane:normal]+'\n'.join(definition(source,marker) for marker in (
   '        [[nodiscard]] epochengine::previewgrid::Vec3 scene_drag_plane_normal(',
   '        void anchor_scene_group_drag(',
   '        [[nodiscard]] bool commit_scene_group_drag(',
   '        void update_scene_object_interaction('))
  code=r'''
#include "core.format_text.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
import scene.document;
import scene.snapshot;
import render.math;
using namespace epochengine;
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
namespace epochengine::input {enum class MouseButton{MouseLeft,MouseRight,MouseMiddle};enum class Key{LeftAlt,RightAlt,LeftShift,RightShift};}
namespace epochengine::core {
enum class ScenePreviewMode{Editor,Other};
struct Context{int x=100,y=100;bool left{},right{},middle{},shift{},alt{};std::uint64_t picked=1;
 void get_mouse_position_safe(int& a,int& b)const{a=x;b=y;}
 bool is_mouse_button_held_safe(input::MouseButton b)const{return b==input::MouseButton::MouseLeft?left:b==input::MouseButton::MouseRight?right:middle;}
 bool is_key_held_safe(input::Key k)const{return k==input::Key::LeftShift||k==input::Key::RightShift?shift:alt;}};
}
namespace gui {struct Vec2{float x{},y{};};struct WidgetBounds{Vec2 position{},size{800,600};};}
namespace epochengine::previewgrid {
using Vec3=render_math::Float3;
using render_math::subtract;using render_math::add;using render_math::scale;using render_math::cross;using render_math::dot;
Vec3 normalize(Vec3 v){return render_math::normalize(v,{0,0,0});}
struct Camera{Vec3 eye{0,10,10},target{},up{0,1,0};};Camera camera;
Vec3 camera_forward(const Camera& value){return epochengine::previewgrid::normalize(subtract(value.target,value.eye));}
Camera camera_for(const core::Context*){return camera;}
float camera_distance_for(const core::Context*){return 10;}
}
struct EditorEntity{scene::SceneObjectId sceneObjectId{};std::array<float,3>position{},rotation{},scale{1,1,1};std::optional<std::array<double,3>> positionAnchor{};};
struct EditorState {
 core::ScenePreviewMode previewMode=core::ScenePreviewMode::Editor;
 std::vector<EditorEntity>entities;authoring::scene::SceneDocument sceneDocument;
 std::size_t selectedEntity{},sceneDragEntity{};scene::SceneObjectId selectedEntityId=1,sceneDragEntityId{};
 std::vector<scene::SceneObjectId>selectedEntityIds{1};
 bool sceneDragActive{},sceneDragMoved{},sceneDragPendingToggle{},sceneDragVertical{},sceneLeftWasHeld{},sceneDragHasPlaneHit{},sceneMarqueeActive{},sceneMarqueeAdditive{},sceneLightingDirty{},sceneInteractionDirty{},textureBrushActive{};
 unsigned sceneDragHorizontalAxis{},guiSelectedWidget{},guiContentEditorWidget{};
 gui::Vec2 sceneDragStartMouse{},sceneMarqueeStart{},sceneMarqueeCurrent{};
 std::array<float,3>sceneDragStartPosition{},sceneDragStartHit{};
 std::vector<std::pair<scene::SceneObjectId,std::array<float,3>>>sceneDragStartPositions;
 unsigned commits{};
};
struct EditorFrameResult{bool scene_input_captured{};gui::WidgetBounds scene_viewport;};
void apply_editor_primary_selection_side_effects(EditorState& e)noexcept;
SELECTION
void apply_editor_primary_selection_side_effects(EditorState& e)noexcept{if(auto i=editor_entity_index(e,e.selectedEntityId))e.selectedEntity=*i;}
void select_editor_entity(EditorState&e,std::size_t i)noexcept{e.selectedEntityIds={e.entities[i].sceneObjectId};e.selectedEntityId=e.selectedEntityIds[0];e.selectedEntity=i;}
void synchronize_editor_selection(EditorState&e){apply_editor_primary_selection_side_effects(e);}
void push_editor_log(EditorState&,const std::string&){}
void apply_scene_marquee_selection(const core::Context*,EditorState&,const gui::WidgetBounds&){} // Unchanged marquee owner, not this gesture.
std::optional<scene::SceneObjectId>pick_editor_scene_entity(const core::Context*c,EditorState&,const gui::WidgetBounds&,const gui::Vec2&){return c->picked?std::optional<scene::SceneObjectId>{c->picked}:std::nullopt;}
struct RayPort{render_math::Float3 origin,direction;float minimumDistance=0,maximumDistance=10000;};
bool parallelRay{};
std::optional<RayPort>screen_point_to_world_ray(const core::Context*,const gui::WidgetBounds&,const gui::Vec2&m,const std::array<double,3>&){
 return RayPort{{m.x*.01f,10-m.y*.01f,10},parallelRay?render_math::Float3{0,-1,0}:render_math::Float3{0,-1,-1}};
}
bool apply_editor_scene_transaction(EditorState&e,std::span<const authoring::scene::SceneOperation>ops,std::string_view label,scene::SceneObjectId){
 ++e.commits;return bool(e.sceneDocument.apply_transaction(ops,label)); // Real authoring history; publication side effects are outside this CPU seam.
}
FUNCTIONS
EditorState fresh(){EditorState e;e.entities={{1},{2,{2,0,0}}};scene::SceneSnapshot s;s.scene_id="drag-test";s.world_name="drag-test";
 for(const auto&v:e.entities){scene::SceneObjectSnapshot o;o.id=v.sceneObjectId;o.name="Cube"+std::to_string(o.id);o.type="Cube";o.category="World";o.position=v.position;o.scale={1,1,1};s.objects.push_back(o);}
 auto built=authoring::scene::SceneDocument::from_snapshot(s);if(built)e.sceneDocument=std::move(built.value);return e;}
void tick(std::shared_ptr<core::Context>c,EditorState&e,bool captured=false){EditorFrameResult r;r.scene_input_captured=captured;update_scene_object_interaction(c,e,r);}
int main(){
 auto c=std::make_shared<core::Context>();
 {auto e=fresh();CHECK(e.sceneDocument.initialized());c->left=true;tick(c,e);c->x+=2;tick(c,e);c->left=false;tick(c,e);CHECK(e.commits==0&&e.entities[0].position[0]==0);}
 c=std::make_shared<core::Context>();
 {auto e=fresh();e.selectedEntityIds={1,2};c->left=true;tick(c,e);c->x+=20;c->y+=4;tick(c,e);
  CHECK(e.sceneDragHorizontalAxis==1&&e.entities[0].position[0]>.19f&&e.entities[0].position[1]==0&&e.entities[0].position[2]==0);
  c->y+=100;tick(c,e);CHECK(e.entities[0].position[2]==0&&e.commits==0);c->left=false;tick(c,e);
  CHECK(e.commits==1&&e.sceneDocument.history().size()==1&&e.selectedEntityIds.size()==2);
  auto h=e.sceneDocument.find({1});CHECK(h);auto before=e.sceneDocument.object(*h);CHECK(before&&before->descriptor.transform.position.x>.19f);
  CHECK(e.sceneDocument.undo());CHECK(e.sceneDocument.object(*h)->descriptor.transform.position.x==0);
  CHECK(e.sceneDocument.redo());CHECK(e.sceneDocument.object(*h)->descriptor.transform.position.x>.19f);tick(c,e);CHECK(e.commits==1);
 }
 c=std::make_shared<core::Context>();
 {auto e=fresh();e.selectedEntityIds={1,2};c->shift=true;c->left=true;tick(c,e);CHECK(e.selectedEntityIds.size()==2);c->left=false;tick(c,e);CHECK(e.selectedEntityIds.size()==1&&e.selectedEntityIds[0]==2&&e.commits==0);
  c->picked=1;c->left=true;tick(c,e);c->left=false;tick(c,e);CHECK(e.selectedEntityIds.size()==2&&e.commits==0);}
 c=std::make_shared<core::Context>();
 {auto e=fresh();e.selectedEntityIds={1,2};c->shift=true;c->left=true;tick(c,e);c->y-=20;tick(c,e);
  CHECK(e.entities[0].position[1]>.19f&&e.entities[0].position[0]==0&&e.entities[0].position[2]==0);
  CHECK(e.entities[1].position[1]==e.entities[0].position[1]&&e.entities[1].position[0]==2);c->left=false;tick(c,e);CHECK(e.commits==1&&e.selectedEntityIds.size()==2);}
 c=std::make_shared<core::Context>();
 {auto e=fresh();c->left=true;tick(c,e);c->x+=20;tick(c,e);const auto horizontal=e.entities[0].position;
  c->shift=true;tick(c,e);CHECK(e.entities[0].position==horizontal);c->y-=20;tick(c,e);const auto vertical=e.entities[0].position;CHECK(vertical[0]==horizontal[0]&&vertical[1]>.19f);
  c->shift=false;tick(c,e);CHECK(e.entities[0].position==vertical);c->x+=20;tick(c,e);CHECK(e.entities[0].position[0]>horizontal[0]&&e.entities[0].position[1]==vertical[1]);c->left=false;tick(c,e);CHECK(e.commits==1&&e.sceneDocument.history().size()==1);CHECK(e.sceneDocument.undo());CHECK(e.sceneDocument.object(*e.sceneDocument.find({1}))->descriptor.transform.position.y==0);}
 c=std::make_shared<core::Context>();parallelRay=true;
 {auto e=fresh();c->shift=true;c->left=true;tick(c,e);c->y-=20;tick(c,e);CHECK(e.entities[0].position[1]>0&&e.entities[0].position[0]==0);c->left=false;tick(c,e);CHECK(e.commits==1);}
 parallelRay=false;c=std::make_shared<core::Context>();
 {auto e=fresh();c->left=true;tick(c,e);c->x+=20;tick(c,e);const auto moved=e.entities[0].position;
  c->x+=30;tick(c,e,true);CHECK(!e.sceneDragActive&&e.commits==1&&e.entities[0].position==moved);
  CHECK(e.sceneDocument.history().size()==1&&e.sceneDocument.undo());CHECK(e.sceneDocument.object(*e.sceneDocument.find({1}))->descriptor.transform.position.x==0);c->left=false;tick(c,e);CHECK(e.commits==1);}
 c=std::make_shared<core::Context>();
 {auto e=fresh();c->picked=2;c->shift=true;c->left=true;tick(c,e);CHECK(e.selectedEntityIds.size()==1);c->y-=20;tick(c,e);CHECK(e.selectedEntityIds.size()==2&&e.entities[0].position[1]>0&&e.entities[0].position[1]==e.entities[1].position[1]);c->left=false;tick(c,e);CHECK(e.commits==1);}
 c=std::make_shared<core::Context>();
 for(unsigned blocked=0;blocked<4;++blocked){auto e=fresh();c->left=true;c->right=blocked==1;c->middle=blocked==2;c->alt=blocked==3;tick(c,e,blocked==0);c->x+=30;tick(c,e,blocked==0);c->left=false;tick(c,e);CHECK(e.commits==0&&!e.sceneDragActive);}
 return 0;
}
'''.replace('SELECTION',selection_functions).replace('FUNCTIONS',functions)
  report=ROOT/'Artifacts/Reports/scene-object-drag-oct29'/str(time.time_ns());report.mkdir(parents=True)
  (report/'consumer.cpp').write_text(code,encoding='utf-8')
  module_map={}
  for p in (ROOT/'Engine/modules').glob('*.ixx'):
   match=re.search(r'^export module\s+([\w.]+);',p.read_text(encoding='utf-8-sig'),re.M)
   if match:module_map[match[1]]=p
  pending=['scene.document','scene.snapshot','render.math'];selected={}
  while pending:
   name=pending.pop()
   if name in selected:continue
   self.assertIn(name,module_map);p=module_map[name];selected[name]=p
   pending+=re.findall(r'^(?:export )?import\s+([\w.]+);',p.read_text(encoding='utf-8-sig'),re.M)
  modules=' '.join('"'+p.as_posix()+'"' for p in selected.values())
  (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(SceneObjectDrag LANGUAGES CXX)
add_executable(drag consumer.cpp)
target_compile_features(drag PRIVATE cxx_std_23)
target_include_directories(drag PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(drag PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {modules})
if(MSVC)
 target_compile_options(drag PRIVATE /utf-8 /W3 /UNDEBUG)
else()
 target_compile_options(drag PRIVATE -UNDEBUG)
endif()
''',encoding='utf-8')
  build=report/'build';commands=[configure_arguments(report,build)]
  for config in ('Debug','Release'):commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],[str(executable(build,config,'drag'))]]
  records=[]
  for i,command in enumerate(commands):
   ran=subprocess.run(command,cwd=report,capture_output=True,text=True,timeout=120)
   (report/f'lane-{i}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8');records.append({'command':command,'exit':ran.returncode})
   (report/'result.json').write_text(json.dumps({'passed':i==len(commands)-1 and ran.returncode==0,'commands':records,'modules':len(selected),'scope':'Real interaction, selection and group commit; real scene history and plane math; deterministic input/ray/camera/publication ports. No native GUI/model.'},indent=2),encoding='utf-8')
   self.assertEqual(ran.returncode,0,ran.stdout[-5000:]+ran.stderr+f'; report={report}')
