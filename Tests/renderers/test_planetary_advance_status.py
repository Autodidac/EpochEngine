"""Actual ProjectPlayScene advance ownership; recorded camera/provider ports, no window."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition
import hashlib
import json
import os
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"


def actual_advance():
    source = OWNER.read_text(encoding="utf-8-sig")
    start = source.index("            void advance_planetary_exploration(")
    end = source.index("            [[nodiscard]] std::vector<previewgrid::ObjectMarker>", start)
    return source[start:end]


def fixture(body):
    module=(ROOT/'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
    camera_source=(ROOT/'Engine/src/render/render.preview_camera.cpp').read_text(encoding='utf-8')
    precise=definition(module,'    export struct PreciseMeshTransform final')+';\n'+definition(camera_source,'    [[nodiscard]] bool valid_precise_mesh_transform(')+'\n'+definition(camera_source,'    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(')
    world_position=definition((ROOT/'Engine/modules/scene.snapshot.ixx').read_text(encoding='utf-8').replace('anchor = {}','anchor'),
        '    [[nodiscard]] inline std::optional<std::array<double, 3>> world_position(')
    return r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace gui {struct Vec2{float x{},y{};};bool captured{};bool keyboard_input_captured(){return captured;}}
namespace input {
enum class Action{RollLeft,RollRight,MoveForward,MoveBackward,MoveRight,MoveLeft,MoveUp,MoveDown,LookRight,LookLeft,LookUp,LookDown};
enum class Key{LeftShift,RightShift};struct InputProfile{float mouse_look_sensitivity{.15f};};
}
namespace project_input {enum class ActionSemantic{move_x,move_y};}
namespace core {struct Context {
 bool focused=true,rolling{};
 bool has_input_focus_safe()const{return focused;}
 bool action_held_safe(input::Action action,const input::InputProfile&)const{return rolling&&action==input::Action::RollRight;}
 bool is_key_held_safe(input::Key)const{return false;}
};}
namespace render_camera {enum class ProjectionKind{perspective};}
namespace project_planetary_runtime {
enum class Command{none,land};
enum class Code:std::uint8_t{ready,cancelled,invalid_input,invalid_frame,unavailable,provider_failure};
struct Input{double elapsed_seconds{},forward{},right{},rise{},yaw{},pitch{},roll{};bool boost{};Command command{};};
struct Frame{std::uint32_t anchor{};std::optional<int> sunlight{};bool valid{true};};
struct Result{Code code{Code::ready};Frame frame{};};
struct PreviewFrame{std::array<double,3> camera{},forward{0,0,-1},up{0,1,0};double meters_to_preview{1},far_distance{100},eye_clearance{1.8};};
std::optional<PreviewFrame> preview_frame(const Frame& frame){return frame.valid?std::optional{PreviewFrame{}}:std::nullopt;}
struct Controller{Result result{};unsigned calls{};Input last{};Result advance(Input input){++calls;last=input;return result;}};
}
namespace previewgrid {
struct Vec3{float x{},y{},z{};};
 ACTUAL_PRECISE_TYPES
 struct Camera{Vec3 eye{},target{},up{};float farPlane{},nearPlane{};render_camera::ProjectionKind projection{};std::array<double,3> navigation{};bool navigationValid{};};
unsigned validations{};bool acceptCamera{true};
Camera camera_for(core::Context*){Camera result;result.navigation={42,43,44};result.navigationValid=true;return result;}
Vec3 normalize(Vec3 value){return value;}Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
std::optional<Camera> validate_camera_frame(Camera value){++validations;return acceptCamera?std::optional{value}:std::nullopt;}
}
struct Settings{bool original_sunlight{};};struct Mesh{std::optional<int> coordinates{};};
namespace scene {using Vec3=std::array<float,3>;ACTUAL_WORLD_POSITION}
struct Entity{std::optional<Mesh> mesh_asset{Mesh{}};std::optional<std::array<double,3>>position_anchor{};struct Transform{std::array<float,3> scale{1,1,1},position{},rotation{};}transform;};
struct Scheduler{void cancel(int){}};
struct Probe {
 std::unique_ptr<project_planetary_runtime::Controller> m_explorationController{std::make_unique<project_planetary_runtime::Controller>()};
 std::optional<Settings> m_planetarySettings{Settings{}},m_meshPlanetarySettings{Settings{}};
 project_planetary_runtime::Input m_explorationInput{};
 project_planetary_runtime::Frame m_explorationFrame{};
 std::optional<project_planetary_runtime::PreviewFrame> m_explorationPreview{};
 std::array<double,3>m_explorationRenderOrigin{};std::optional<previewgrid::Camera>m_explorationRenderCamera;
 gui::Vec2 m_explorationLastMouse{};bool m_explorationMouseHeld{},validTransform{true};
 std::string m_explorationStatus{"Saved exploration successfully."},m_explorationAdvanceStatus{};
 std::optional<int> m_planetaryPending{};Scheduler* m_planetaryScheduler{};int m_planetaryCancellation{};
 int m_entities{},m_meshes{},m_planetaryEntity{};Entity entity{};unsigned reconfigures{};
 bool runtime_panel_captures_pointer(gui::Vec2)const{return false;}
 // No project input artifact in this provider-status lane; the dedicated
 // runtime-input consumer covers admission, rebinding and refusal behavior.
 input::InputProfile m_runtimeInputProfile{};
 bool runtime_input_ready()const{return true;}
 float runtime_planar_axis(project_input::ActionSemantic,float value)const{return value;}
 const Entity* planetary_entity()const{return &entity;}
 std::optional<previewgrid::Vec3> planetary_preview_vector(std::array<double,3> value,const Entity&,bool,double=1)const {
  if(!validTransform)return {};return previewgrid::Vec3{float(value[0]),float(value[1]),float(value[2])};}
 float project_play_adaptive_near_plane(previewgrid::Camera,double,double,int,int,int,std::array<double,3>,bool)const{return .18f;}
 std::array<double,3> planetary_scene_origin_offset(const project_planetary_runtime::Frame*)const{return {};}
 void configure_exploration_stream(std::uint32_t){++reconfigures;}
 ACTUAL_ADVANCE
};
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 core::Context context;Probe probe;auto& controller=*probe.m_explorationController;
 using Code=project_planetary_runtime::Code;
 const auto advance=[&]{probe.advance_planetary_exploration(&context,1.0f/60.0f,{},false,false,false);};
 // A refused operation keeps the current frame, exposes the actual reason,
 // and does not overwrite an independent save/startup status.
 for(const auto pair:std::array{std::pair{Code::invalid_input,"input was rejected"},std::pair{Code::cancelled,"was cancelled"},
  std::pair{Code::unavailable,"provider is unavailable"},std::pair{Code::provider_failure,"provider failed"},
  std::pair{Code::invalid_frame,"invalid camera frame"}}){
  controller.result.code=pair.first;const auto published=previewgrid::validations;advance();
  CHECK(previewgrid::validations==published&&!probe.m_explorationPreview);
  CHECK(probe.m_explorationAdvanceStatus.find(pair.second)!=std::string::npos);
  CHECK(probe.m_explorationStatus=="Saved exploration successfully.");
 }
 controller.result.code=Code::ready;controller.result.frame.valid=false;advance();
 CHECK(probe.m_explorationAdvanceStatus.find("invalid camera frame")!=std::string::npos);
 controller.result.frame.valid=true;probe.m_meshPlanetarySettings->original_sunlight=true;advance();
 CHECK(probe.m_explorationAdvanceStatus.find("required sunlight")!=std::string::npos);
 controller.result.frame.sunlight=1;probe.validTransform=false;advance();
 CHECK(probe.m_explorationAdvanceStatus.find("authored transform")!=std::string::npos);
 probe.validTransform=true;previewgrid::acceptCamera=false;advance();
 CHECK(probe.m_explorationAdvanceStatus.find("camera was rejected")!=std::string::npos);
 previewgrid::acceptCamera=true;controller.result.frame.anchor=5;advance();
 CHECK(probe.m_explorationAdvanceStatus.empty()&&probe.m_explorationStatus=="Saved exploration successfully.");
 CHECK(probe.m_explorationPreview&&probe.m_explorationFrame.anchor==5&&probe.reconfigures==1);
 CHECK((probe.m_explorationRenderCamera&&!probe.m_explorationRenderCamera->navigationValid
    &&probe.m_explorationRenderCamera->navigation==std::array<double,3>{}));
 context.rolling=true;advance();CHECK(controller.last.roll>0);
 gui::captured=true;advance();CHECK(controller.last.roll==0);gui::captured=false;
 probe.advance_planetary_exploration(&context,1.0f/60.0f,{},false,false,true);CHECK(controller.last.roll==0);
 context.focused=false;advance();CHECK(controller.last.roll==0);context.focused=true;context.rolling=false;
 const auto oldFrame=probe.m_explorationFrame;
 controller.result.code=Code::invalid_input;advance();
 CHECK(probe.m_explorationFrame.anchor==oldFrame.anchor&&!probe.m_explorationAdvanceStatus.empty());
 controller.result.code=Code::ready;advance();
 CHECK(probe.m_explorationAdvanceStatus.empty()&&probe.m_explorationFrame.anchor==oldFrame.anchor);
 return 0;
}
'''.replace("ACTUAL_ADVANCE", body).replace("ACTUAL_PRECISE_TYPES",precise.replace('export ','')).replace("ACTUAL_WORLD_POSITION",world_position)


class PlanetaryAdvanceStatus(unittest.TestCase):
    def test_actual_advance_failure_and_recovery(self):
        digest = hashlib.sha256(OWNER.read_bytes()).hexdigest()
        report = ROOT / "Artifacts/Reports/planetary-advance-status-oct29" / digest[:12]
        report.mkdir(parents=True, exist_ok=True)
        source = report / "runtime.advance_status.cpp"
        source.write_text(fixture(actual_advance()), encoding="utf-8")
        phases=compile_and_run(self,source,report,windows_flags=['/W4'])
        (report/'result.json').write_text(json.dumps(dict(source_sha256=digest,phases=phases,
            actual_advance_owner=True,provider_camera_recording_ports=True,no_native_camera_or_model=True),indent=2)+'\n',encoding='utf-8')
        self.assertEqual(hashlib.sha256(OWNER.read_bytes()).hexdigest(), digest)


if __name__ == "__main__":
    unittest.main()
