"""Actual transient camera module consumer; no renderer, window or model starts."""
from pathlib import Path
import json
import os
import re
import subprocess
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    editor = ROOT / 'Engine/src/editor/application'
    documents = (editor / 'editor.application_project_documents.inl').read_text(encoding='utf-8')
    scene = (editor / 'editor.application_scene_preview.inl').read_text(encoding='utf-8')
    owners = '\n'.join((definition(documents, '        [[nodiscard]] bool begin_editor_lod_inspection('),
                       definition(documents, '        void end_editor_lod_inspection('),
                       definition(scene, '        void append_editor_translation_gizmo(')))
    return r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <vector>
import render.preview_grid;
import asset.mesh_artifact;
namespace preview=epochengine::previewgrid;
namespace previewgrid=preview;
namespace camera=epochengine::render_camera;
enum class ScenePointerMode{select,move_free,move_x,move_y,move_z};
struct EditorState{
 std::optional<preview::CameraRigSnapshot> lodInspectionReturnCamera;
 preview::CameraMode viewportCameraMode{preview::CameraMode::Editor};
 bool planetaryStreamContinue{},sceneDragActive{};std::uint64_t planetaryStreamNextTick{999};
 ScenePointerMode scenePointerMode{ScenePointerMode::select},sceneDragMode{ScenePointerMode::select};
};
struct Body{preview::Vec3 target;float radius;};
struct EditorEntity{std::optional<std::array<double,3>> positionAnchor;std::optional<Body> body;};
std::optional<Body> editor_planetary_body_focus(const EditorState&,const EditorEntity& entity){return entity.body;}
EDITOR_OWNERS
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
bool same_vec(preview::Vec3 a,preview::Vec3 b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same(const preview::Camera&a,const preview::Camera&b) {
 return same_vec(a.eye,b.eye)&&same_vec(a.target,b.target)&&same_vec(a.up,b.up)
  &&a.fovRadians==b.fovRadians&&a.nearPlane==b.nearPlane&&a.farPlane==b.farPlane
  &&a.projection==b.projection&&a.orientation==b.orientation
  &&a.orthographicVerticalSize==b.orthographicVerticalSize;
}
int main() {
 int inspection{};EditorState editor;
 preview::set_camera_mode(&inspection,preview::CameraMode::Editor);
 CHECK(preview::set_editor_view(&inspection,preview::EditorView::Front));
 const auto saved=preview::capture_camera_rig_snapshot(&inspection);
 CHECK(begin_editor_lod_inspection(editor,&inspection));
 const auto utility=preview::capture_camera_rig_snapshot(&inspection);
 CHECK(utility.logical_view_id!=saved.logical_view_id&&preview::editor_view_for(&inspection)==preview::EditorView::Perspective);
 CHECK(preview::focus_camera(&inspection,{80,90,100},4));
 CHECK(begin_editor_lod_inspection(editor,&inspection)&&editor.lodInspectionReturnCamera->logical_view_id==saved.logical_view_id);
 end_editor_lod_inspection(editor,&inspection);
 const auto restored=preview::capture_camera_rig_snapshot(&inspection);
 CHECK(!editor.lodInspectionReturnCamera&&editor.planetaryStreamContinue&&editor.planetaryStreamNextTick==0);
 CHECK(restored.logical_view_id==saved.logical_view_id&&same_vec(restored.focus,saved.focus)&&restored.orientation==saved.orientation);
 preview::set_camera_mode(&inspection,preview::CameraMode::FPS);
 const auto previousEye=preview::camera_world_eye(preview::camera_for(&inspection));
 CHECK(begin_editor_lod_inspection(editor,&inspection)&&preview::camera_mode_for(&inspection)==preview::CameraMode::Editor);
 end_editor_lod_inspection(editor,&inspection);
 CHECK(editor.viewportCameraMode==preview::CameraMode::FPS&&preview::camera_world_eye(preview::camera_for(&inspection))==previousEye);
 preview::cleanup_context(&inspection);
 int gizmo{};EditorEntity entity;editor.scenePointerMode=ScenePointerMode::move_z;
 const preview::ObjectMarker object{.position={2,4,8},.scale={4,2,6}};
 std::vector<preview::ObjectMarker> arrows;
 append_editor_translation_gizmo(arrows,editor,entity,object,{});
 CHECK(arrows.size()==3&&arrows[2].color.z>arrows[0].color.x&&arrows[2].scale.x>std::sqrt(14.0f));
 CHECK(preview::set_object_markers(&gizmo,arrows));
 const auto lines=preview::object_marker_geometry_for(&gizmo);
 CHECK(lines&&lines->size()==30);
 for(unsigned axis=0;axis<3;++axis){
  CHECK(same_vec((*lines)[axis*10].position,object.position));
  auto tip=object.position;
  (axis==0?tip.x:axis==1?tip.y:tip.z)+=arrows[axis].scale.x;
  const auto error=preview::subtract((*lines)[axis*10+1].position,tip);
  CHECK(preview::dot(error,error)<1e-10f);
 }
 arrows.clear();entity.positionAnchor=std::array<double,3>{1e12,2e12,3e12};entity.body=Body{{10,20,30},6371000};
 append_editor_translation_gizmo(arrows,editor,entity,object,{1e12+9,2e12+19,3e12+29});
 CHECK(arrows.size()==3&&same_vec(arrows[0].position,{1,1,1})&&arrows[0].scale.x>6371000);
 preview::cleanup_context(&gizmo);
 // Switching navigation styles must not teleport a planetary editor view
 // to the world origin. A large FPS eye still needs a representable target.
 int switching{};
 preview::set_camera_mode(&switching,preview::CameraMode::Editor);
 CHECK(preview::focus_camera(&switching,{6371000,0,0},6371000));
 const auto editorPose=preview::camera_for(&switching);
 const auto editorRig=preview::capture_camera_rig_snapshot(&switching);
 preview::set_camera_mode(&switching,preview::CameraMode::FPS);
 const auto fpsPose=preview::camera_for(&switching);
 CHECK(same_vec(fpsPose.eye,editorPose.eye));
 const auto editorDirection=preview::normalize(preview::subtract(editorPose.target,editorPose.eye));
 const auto fpsDirection=preview::normalize(preview::subtract(fpsPose.target,fpsPose.eye));
 CHECK(preview::dot(editorDirection,fpsDirection)>.999f);
 CHECK(camera::resolve(preview::camera_descriptor_for(&switching)).valid);
 CHECK(preview::capture_camera_rig_snapshot(&switching).scene_extent==editorRig.scene_extent);
 preview::set_camera_mode(&switching,preview::CameraMode::Editor);
 const auto returned=preview::camera_for(&switching);
 CHECK(preview::dot(preview::subtract(returned.eye,editorPose.eye),preview::subtract(returned.eye,editorPose.eye))<16.0f);
 CHECK(preview::dot(preview::normalize(preview::subtract(returned.target,returned.eye)),editorDirection)>.999f);
 CHECK(preview::capture_camera_rig_snapshot(&switching).logical_view_id==editorRig.logical_view_id);
 preview::cleanup_context(&switching);
 int first{},second{},unused{};const void* a=&first;const void* b=&second;
 preview::cleanup_context(a);preview::cleanup_context(b);
 preview::set_camera_mode(a,preview::CameraMode::FPS);
 preview::set_camera_mode(b,preview::CameraMode::FPS);
 const auto legacy=preview::camera_for(a),other=preview::camera_for(b);
 const auto snapshot=preview::capture_camera_rig_snapshot(a);
 const auto initialRevision=preview::camera_revision_for(a);
 preview::Camera radial{.eye={17,22,31},.target={17,22,30},.up={2,0,0},
  .fovRadians=1.05f,.nearPlane=0.05f,.farPlane=4096};
 CHECK(preview::set_external_camera_frame(a,radial));
 const auto accepted=preview::camera_for(a);
 CHECK(same_vec(accepted.eye,radial.eye)&&same_vec(accepted.target,radial.target));
 CHECK(accepted.up.x==1&&accepted.up.y==0&&accepted.up.z==0);
 CHECK(preview::camera_revision_for(a)==initialRevision+1);
 CHECK(same(preview::camera_for(b),other));
 const auto descriptor=preview::camera_descriptor_for(a);
 const auto resolved=camera::resolve(descriptor);
 CHECK(resolved.valid&&resolved.up.x==1&&resolved.up.y==0&&resolved.up.z==0);
 const auto revision=preview::camera_revision_for(a);
 CHECK(preview::set_external_camera_frame(a,radial));
 CHECK(preview::set_external_camera_frame(a,accepted));
 CHECK(preview::camera_revision_for(a)==revision);
 // Invalid work must not mutate the active frame or allocate a new context rig.
 const auto rejected=[&](preview::Camera invalid) {
  return !preview::set_external_camera_frame(a,invalid)
   &&same(preview::camera_for(a),accepted)&&preview::camera_revision_for(a)==revision;
 };
 auto invalid=radial;invalid.eye.x=std::numeric_limits<float>::quiet_NaN();CHECK(rejected(invalid));
 invalid=radial;invalid.target.z=std::numeric_limits<float>::infinity();CHECK(rejected(invalid));
 invalid=radial;invalid.up={};CHECK(rejected(invalid));
 invalid=radial;invalid.target=invalid.eye;CHECK(rejected(invalid));
 invalid=radial;invalid.up={0,0,-2};CHECK(rejected(invalid));
 invalid=radial;invalid.up.x=std::numeric_limits<float>::max();CHECK(rejected(invalid));
 invalid=radial;invalid.nearPlane=0;CHECK(rejected(invalid));
 invalid=radial;invalid.farPlane=invalid.nearPlane;CHECK(rejected(invalid));
 invalid=radial;invalid.fovRadians=0;CHECK(rejected(invalid));
 invalid=radial;invalid.fovRadians=3.1415927f;CHECK(rejected(invalid));
 invalid=radial;invalid.orthographicVerticalSize=std::numeric_limits<float>::quiet_NaN();CHECK(rejected(invalid));
 invalid=radial;invalid.projection=static_cast<camera::ProjectionKind>(9);CHECK(rejected(invalid));
 invalid=radial;invalid.orientation=static_cast<camera::ViewOrientation>(9);CHECK(rejected(invalid));
 CHECK(!preview::set_external_camera_frame(nullptr,radial));
 CHECK(!preview::set_external_camera_frame(&unused,invalid)&&preview::camera_revision_for(&unused)==0);
 // An arbitrary rolled, nonorthogonal input up is resolved using existing camera math.
 auto rolled=radial;rolled.up={2,2,1};
 CHECK(preview::set_external_camera_frame(b,rolled));
 const auto roll=preview::camera_for(b);
 CHECK(std::abs(roll.up.x-0.70710678f)<1e-6f&&std::abs(roll.up.y-0.70710678f)<1e-6f
  &&std::abs(roll.up.z)<1e-6f);
 CHECK(same(preview::camera_for(a),accepted));
 // Snapshots carry the ordinary navigation rig, not the transient external pose.
 const auto whileExternal=preview::capture_camera_rig_snapshot(a);
 CHECK(same_vec(whileExternal.position,snapshot.position)
  &&same_vec(whileExternal.focus,snapshot.focus)&&whileExternal.yaw_degrees==snapshot.yaw_degrees);
 CHECK(preview::clear_external_camera_frame(a));
 CHECK(same(preview::camera_for(a),legacy));
 const auto cleared=preview::camera_revision_for(a);
 CHECK(cleared==revision+1&&!preview::clear_external_camera_frame(a));
 CHECK(preview::camera_revision_for(a)==cleared);
 CHECK(preview::set_external_camera_frame(a,radial));
 preview::set_camera_mode(a,preview::CameraMode::FPS); // Even the same mode releases it.
 CHECK(same(preview::camera_for(a),legacy));
 CHECK(preview::set_external_camera_frame(a,radial));preview::reset_camera(a);
 CHECK(same(preview::camera_for(a),legacy));
 CHECK(preview::set_external_camera_frame(a,radial));
 CHECK(preview::restore_camera_rig_snapshot(a,whileExternal));
 CHECK(same(preview::camera_for(a),legacy));
 CHECK(preview::set_external_camera_frame(a,radial));
 preview::set_camera_mode(a,preview::CameraMode::Canvas2D);
 CHECK(preview::camera_for(a).projection==camera::ProjectionKind::orthographic);
 // Orthographic frames use the existing projection contract too.
 auto orthographic=radial;orthographic.projection=camera::ProjectionKind::orthographic;
 orthographic.nearPlane=0;orthographic.orthographicVerticalSize=32;
 CHECK(preview::set_external_camera_frame(a,orthographic));
 CHECK(camera::resolve(preview::camera_descriptor_for(a)).orthographic_vertical_size==32);
 invalid=orthographic;invalid.orthographicVerticalSize=0;
 CHECK(!preview::set_external_camera_frame(a,invalid));
 invalid=orthographic;invalid.nearPlane=-1;
 CHECK(!preview::set_external_camera_frame(a,invalid));
 preview::cleanup_context(a);CHECK(preview::camera_revision_for(a)==0);
 CHECK(same(preview::camera_for(b),roll));
 preview::set_camera_mode(a,preview::CameraMode::FPS);CHECK(same(preview::camera_for(a),legacy));
 // After release, ordinary navigation remains live and independent.
 const auto old=preview::camera_for(a);
 preview::step_camera(a,0.1f,1,0,0,0,0,1);
 CHECK(!same_vec(preview::camera_for(a).eye,old.eye));
 preview::cleanup_context(a);preview::cleanup_context(b);
 CHECK(!preview::clear_external_camera_frame(a)&&!preview::clear_external_camera_frame(nullptr));

 // Planet overview: fit the depth interval to visible opaque geometry.
 const float radius=6371000.0f;
 preview::Camera orbit{.eye={0,0,-3.25f*radius},.target={},.up={0,1,0},
  .fovRadians=.90f,.nearPlane=3.25f*radius*128*std::numeric_limits<float>::epsilon(),.farPlane=8*radius};
 const preview::ObjectMarker planet{.position={},.scale={2*radius,2*radius,2*radius},
  .primitive=preview::ObjectPreviewPrimitive::Cube};
 const std::array planetFrame{planet};
 const auto fitted=preview::fit_visible_depth_range(orbit,planetFrame);
 CHECK(fitted.nearPlane>1000000.0f&&fitted.nearPlane<2.25f*radius);
 const auto depth_code=[&](const preview::Camera& value,float distance){
  const auto projection=preview::projection_for(a,1,value);
  const auto clip=preview::transform_point(projection,{0,0,-distance});
  return std::llround((double(clip.z)/clip.w*.5+.5)*16777215.0);
 };
 const auto surface=2.25f*radius;
 CHECK(depth_code(orbit,surface)==depth_code(orbit,surface+100));
 CHECK(std::abs(depth_code(fitted,surface)-depth_code(fitted,surface+100))>8);
 // Spawn is opaque scene geometry too. A distant spawn must not disable
 // the planetary fit; its small proxy is not an unknown primitive.
 preview::ObjectMarker spawn{.position={},.scale={1,10000,2},.rotationDegrees={90,0,0},
  .primitive=preview::ObjectPreviewPrimitive::Spawn};
 const std::array planetAndSpawn{planet,spawn};
 const auto withSpawn=preview::fit_visible_depth_range(orbit,planetAndSpawn);
 CHECK(withSpawn.nearPlane==fitted.nearPlane&&withSpawn.farPlane==fitted.farPlane);
 CHECK(std::abs(depth_code(withSpawn,surface)-depth_code(withSpawn,surface+100))>8);
 spawn.position={0,0,orbit.eye.z+4};
 const std::array nearSpawnFrame{planet,spawn};
 const auto nearSpawn=preview::fit_visible_depth_range(orbit,nearSpawnFrame);
 CHECK(nearSpawn.nearPlane>.05f&&nearSpawn.nearPlane<=1.0f&&nearSpawn.farPlane==fitted.farPlane);
 // Match the actual solid publication, including the deliberate axis-aligned
 // Spawn proxy and the rotated Cube/Level/Canvas2D geometry. Large authored
 // Spawn Y scale is replaced by its radius-driven thin half-height.
 int primitiveContext{};
 const preview::Camera axial{.eye={},.target={0,0,1}};
 for(const auto primitive:std::array{preview::ObjectPreviewPrimitive::Spawn,
   preview::ObjectPreviewPrimitive::Cube,preview::ObjectPreviewPrimitive::Level,
   preview::ObjectPreviewPrimitive::Canvas2D}){
  preview::ObjectMarker marker{.position={0,0,20},.scale={2,10,4},.rotationDegrees={90,0,0},
   .radius=.8f,.primitive=primitive};
  CHECK(preview::set_object_markers(&primitiveContext,std::span{&marker,1u}));
  const auto solids=preview::object_solid_geometry_for(&primitiveContext);CHECK(solids&&!solids->empty());
  double minimum=1e30,maximum=-1e30;
  for(const auto& vertex:*solids){minimum=std::min(minimum,double(vertex.position.z));maximum=std::max(maximum,double(vertex.position.z));}
  const auto bounds=preview::marker_depth_bounds(marker,axial);CHECK(bounds);
  CHECK(std::abs((*bounds)[0]-minimum)<.00001&&std::abs((*bounds)[1]-maximum)<.00001);
  if(primitive==preview::ObjectPreviewPrimitive::Spawn){
   CHECK(minimum==18&&maximum==22);
   auto vertical=axial;vertical.target={0,1,0};
   const auto thin=preview::marker_depth_bounds(marker,vertical);CHECK(thin);
   CHECK(std::abs((*thin)[0]+.096)<.00001&&std::abs((*thin)[1]-.096)<.00001);
  }
 }
 preview::cleanup_context(&primitiveContext);
 auto foreground=planet;foreground.scale={.5f,.5f,.5f};
 foreground.position={0,0,orbit.eye.z+4};
 const std::array withForeground{planet,foreground};
 const auto nearObject=preview::fit_visible_depth_range(orbit,withForeground);
 CHECK(nearObject.nearPlane<2&&nearObject.nearPlane>0);
 auto enclosing=planet;enclosing.position=orbit.eye;
 const std::array inside{enclosing};
 CHECK(preview::fit_visible_depth_range(orbit,inside).nearPlane==.05f);
 auto behind=foreground;behind.position.z=orbit.eye.z-20;
 const std::array behindFrame{planet,behind};
 CHECK(preview::fit_visible_depth_range(orbit,behindFrame).nearPlane==fitted.nearPlane);
 // A diagonal view outside a closed mesh can be inside its loose AABB.
 // Only referenced vertex depths bound the rendered triangles in that case.
 namespace mesh=epochengine::asset::mesh;
 std::array<mesh::Vertex,6> octaVertices{};
 const std::array<std::array<float,3>,6> octaPoints{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
 for(unsigned i=0;i<octaVertices.size();++i){octaVertices[i].position=octaPoints[i];octaVertices[i].normal=octaPoints[i];}
 const std::array<std::uint32_t,24> octaIndices{0,2,4,4,2,1,1,2,5,5,2,0,0,4,3,4,1,3,1,5,3,5,0,3};
 const auto octa=mesh::build_artifact(octaVertices,octaIndices);CHECK(octa);
 auto diagonal=orbit;diagonal.eye={radius*.75f,radius*.75f,radius*.75f};
 preview::ObjectMarker actual{.position={},.scale={radius,radius,radius},
  .primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=octa.artifact};
 const std::array actualFrame{actual};
 const auto actualBounds=preview::marker_depth_bounds(actual,diagonal);CHECK(actualBounds);
 CHECK((*actualBounds)[0]>radius*.70&&(*actualBounds)[1]<radius*1.90);
 const auto exactFit=preview::fit_visible_depth_range(diagonal,actualFrame);
 CHECK(exactFit.nearPlane>radius*.17&&exactFit.nearPlane<radius*.19);
 CHECK(exactFit.farPlane>(*actualBounds)[1]);
 const auto looseBox=preview::marker_depth_bounds(planet,diagonal);CHECK(looseBox&&(*looseBox)[0]<0);
 auto precise=actual;precise.preciseTransform=preview::make_precise_mesh_transform({0,0,0},{radius,radius,radius},{});
 CHECK(precise.preciseTransform);
 const auto preciseBounds=preview::marker_depth_bounds(precise,diagonal);CHECK(preciseBounds);
 CHECK(std::abs((*preciseBounds)[0]-(*actualBounds)[0])<1.0);
 const auto translated=preview::make_precise_mesh_transform({90000000.25,0,0},{radius,radius,radius},{});
 precise.preciseTransform=translated;
 auto translatedView=diagonal;translatedView.eye.x+=90000000.0f;translatedView.target.x+=90000000.0f;
 const auto translatedBounds=preview::marker_depth_bounds(precise,translatedView);CHECK(translatedBounds&&(*translatedBounds)[0]>0);
 diagonal.eye={radius*.25f,radius*.25f,radius*.25f};
 const auto crossing=preview::marker_depth_bounds(actual,diagonal);CHECK(crossing&&(*crossing)[0]<0&&(*crossing)[1]>0);
 CHECK(preview::fit_visible_depth_range(diagonal,actualFrame).nearPlane==.05f);
 // Disconnected triangles on opposite sides of the eye do not cross its
 // plane. Only a triangle with forward geometry can constrain the fit.
 std::array<mesh::Vertex,10> separatedVertices{};
 const std::array<std::array<float,3>,10> separatedPoints{{
  {-1,-1,1000},{1,-1,1000},{0,1,1000},
  {-1,-1,-10000000},{1,-1,-10000000},{0,1,-10000000},
  {-1,-1,1000000000},{1,-1,1000000000},{0,1,1000000000},{0,0,.125f}}};
 for(unsigned i=0;i<separatedVertices.size();++i){separatedVertices[i].position=separatedPoints[i];separatedVertices[i].normal={0,0,1};}
 const std::array<std::uint32_t,9> separatedIndices{3,4,5,0,1,2,6,7,8};
 const auto separated=mesh::build_artifact(separatedVertices,separatedIndices);CHECK(separated);
 preview::ObjectMarker disconnected{.primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=separated.artifact};
 const std::array disconnectedFrame{disconnected};
 const auto disconnectedBounds=preview::marker_depth_bounds(disconnected,axial);CHECK(disconnectedBounds);
 CHECK((*disconnectedBounds)[0]==1000&&(*disconnectedBounds)[1]==1000000000);
 const auto disconnectedFit=preview::fit_visible_depth_range(axial,disconnectedFrame);
 CHECK(disconnectedFit.nearPlane==250&&disconnectedFit.farPlane>=1000000000);
 // An unused forward vertex must not make a behind-only indexed mesh visible.
 const std::array<std::uint32_t,3> behindIndices{3,4,5};
 const auto onlyBehind=mesh::build_artifact(separatedVertices,behindIndices);CHECK(onlyBehind);
 disconnected.mesh=onlyBehind.artifact;
 CHECK(same(preview::fit_visible_depth_range(axial,std::span{&disconnected,1u}),axial));
 // A triangle touching the eye plane still requires the conservative floor.
 separatedVertices[0].position[2]=0;
 const auto touching=mesh::build_artifact(separatedVertices,separatedIndices);CHECK(touching);
 disconnected.mesh=touching.artifact;
 CHECK(preview::fit_visible_depth_range(axial,std::span{&disconnected,1u}).nearPlane==.05f);
 auto ortho=orbit;ortho.projection=camera::ProjectionKind::orthographic;
 CHECK(same(preview::fit_visible_depth_range(ortho,planetFrame),ortho));
 CHECK(same(preview::fit_visible_depth_range(orbit,{}),orbit));
 return 0;
}
'''.replace('EDITOR_OWNERS', owners)


class ExternalCameraFrame(unittest.TestCase):
    def test_actual_module_lifecycle_and_radial_basis(self):
        proof = ROOT / "Artifacts/Reports/editor-planet-depth-oct31/module-proof"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "camera.frame_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        module_map = {}
        for owner in (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"):
            for path in owner.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    self.assertNotIn(match.group(1), module_map, "Duplicate module owner")
                    module_map[match.group(1)] = path
        pending, closure = ["render.preview_grid", "authoring.texture_detail"], set()
        while pending:
            name = pending.pop()
            if name in closure:
                continue
            self.assertIn(name, module_map)
            closure.add(name)
            pending.extend(re.findall(r"^(?:export\s+)?import\s+([\w.:]+);",
                module_map[name].read_text(encoding="utf-8-sig"), re.M))
        modules = " ".join('"' + module_map[name].as_posix() + '"' for name in sorted(closure))
        roots = '"' + (ROOT / "Engine/modules").as_posix() + '" "' + (ROOT / "Engine/dep/EpochPlatformEngine/modules").as_posix() + '"'
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ExternalCamera LANGUAGES CXX)
add_executable(camera_contract "{source.as_posix()}" "{(ROOT / 'Engine/src/render/render.preview_camera.cpp').as_posix()}" "{(ROOT / 'Engine/src/render/render.preview_lighting.cpp').as_posix()}" "{(ROOT / 'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}")
target_compile_features(camera_contract PRIVATE cxx_std_23)
target_sources(camera_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {roots} FILES {modules})
target_include_directories(camera_contract PRIVATE "{(ROOT / 'Engine').as_posix()}" "{(ROOT / 'Engine/include').as_posix()}" "{(ROOT / 'Engine/src').as_posix()}")
if(MSVC)
 target_compile_options(camera_contract PRIVATE /utf-8 /EHsc)
endif()
enable_testing()
add_test(NAME external_camera_frame COMMAND camera_contract)
set_tests_properties(external_camera_frame PROPERTIES TIMEOUT 30)
''', encoding="utf-8")
        configure = ["cmake", "-S", str(proof), "-B", str(proof / "cmake")]
        if os.name == "nt":
            configure += ["-G", "Visual Studio 17 2022", "-A", "x64"]
        else:
            configure += ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
        commands = [("configure", configure)]
        for config in ("Debug", "Release") if os.name == "nt" else ("Release",):
            build = ["cmake", "--build", str(proof / "cmake"), "--config", config, "--parallel", "1"]
            if os.name == "nt":
                build += ["--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]
            commands += [(config + "-build", build), (config + "-contract", ["ctest", "--test-dir",
                str(proof / "cmake"), "-C", config, "--output-on-failure"])]
        completed = []
        for label, command in commands:
            env = dict(os.environ, MSBUILDDISABLENODEREUSE="1")
            result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
            (proof / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            completed.append({"phase": label, "exit": result.returncode, "command": command})
            (proof / "result.json").write_text(json.dumps({"module_count": len(closure), "completed": completed,
                "native_integration": False}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-12000:])


if __name__ == "__main__":
    unittest.main()
