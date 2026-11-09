"""Actual preview camera supports configured world sizes without changing legacy poses."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
LEGACY_TRACE_SHA256 = "53cfdd45c179625eb2f8958c237d93da755227ce3b75906b3f2dc339858031b0"

CODE = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <vector>
#include <string_view>
import render.preview_grid;
import asset.mesh_artifact;
namespace p = epochengine::previewgrid;
namespace mesh = epochengine::asset::mesh;
namespace camera = epochengine::render_camera;
void check(bool value, int line) {
    if (!value) { std::fprintf(stderr, "World camera contract failed line %d\n", line); std::exit(92); }
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)
bool same(p::Vec3 a, p::Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
bool finite(p::Vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
void trace(std::ofstream& output, const void* key) {
    const auto frame=p::camera_for(key); const auto snapshot=p::capture_camera_rig_snapshot(key);
    const auto one=[&](float value) { output.write(reinterpret_cast<const char*>(&value),sizeof(value)); };
    const auto vec=[&](p::Vec3 value) { one(value.x); one(value.y); one(value.z); };
    vec(frame.eye);vec(frame.target);vec(frame.up);one(frame.fovRadians);one(frame.nearPlane);one(frame.farPlane);
    one(frame.orthographicVerticalSize);one(float(frame.projection));one(float(frame.orientation));
    vec(snapshot.focus);vec(snapshot.position);one(snapshot.yaw_degrees);one(snapshot.pitch_degrees);
    one(snapshot.free_yaw_degrees);one(snapshot.free_pitch_degrees);one(snapshot.distance);
    one(snapshot.orthographic_vertical_size);one(snapshot.fly_speed);
    for(auto value:snapshot.view_focuses)vec(value);
    for(auto value:snapshot.view_distances)one(value);
    for(auto value:snapshot.view_orthographic_sizes)one(value);
}
void legacy_trace(const std::filesystem::path& file) {
    std::ofstream output(file,std::ios::binary); int a{},b{};
    for(auto mode:{p::CameraMode::Editor,p::CameraMode::FPS,p::CameraMode::Canvas2D}) {
        p::cleanup_context(&a);p::cleanup_context(&b);p::set_camera_mode(&a,mode);trace(output,&a);
        CHECK(p::focus_camera(&a,{1,2,3},1.25f));trace(output,&a);
        p::zoom_camera(&a,-2);trace(output,&a);p::pan_camera_drag(&a,12,-7);trace(output,&a);
        p::dolly_camera_drag(&a,4);trace(output,&a);p::look_camera(&a,3,-2);trace(output,&a);
        p::step_camera(&a,0.016f,1,-0.5f,0.25f,0.5f,-0.25f,2);trace(output,&a);
        if(mode==p::CameraMode::Editor) {
            CHECK(p::set_editor_view(&a,p::EditorView::Front));trace(output,&a);
            p::zoom_camera(&a,-0.5f);trace(output,&a);
            CHECK(p::set_editor_view(&a,p::EditorView::Perspective));trace(output,&a);
        }
        CHECK(p::restore_camera_rig_snapshot(&b,p::capture_camera_rig_snapshot(&a)));trace(output,&b);
        p::reset_camera(&a);trace(output,&a);
    }
    p::cleanup_context(&a);p::cleanup_context(&b);CHECK(output.good());
}
std::shared_ptr<const mesh::Artifact> normalized_mesh() {
    std::array<mesh::Vertex,6> vertices;
    const std::array<std::array<float,3>,6> positions{{{10,0,0},{-10,0,0},{0,10,0},{0,-10,0},{0,0,10},{0,0,-10}}};
    for(std::size_t index=0; index<vertices.size(); ++index) {
        vertices[index].position=positions[index];
        for(unsigned axis=0; axis<3; ++axis)vertices[index].normal[axis]=positions[index][axis]/10;
    }
    const std::array<std::uint32_t,24> indices{0,2,4,4,2,1,1,2,5,5,2,0,0,4,3,4,1,3,1,5,3,5,0,3};
    const auto built=mesh::build_artifact(vertices,indices);CHECK(built);return built.artifact;
}
void check_camera(const void* key) {
    const auto frame=p::camera_for(key);CHECK(finite(frame.eye) && finite(frame.target) && finite(frame.up));
    CHECK(frame.farPlane>frame.nearPlane && std::isfinite(frame.farPlane));
    CHECK(camera::resolve(p::camera_descriptor_for(key)).valid);
    const auto matrix=p::projection_for(key,1.6f,frame);
    CHECK(std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}));
}
void check_framed_depth(const void* key, float radius) {
    const auto frame=p::camera_for(key);
    const float distance=p::camera_distance_for(key);
    const auto projection=p::projection_for(key,1.6f,frame);
    const auto front=p::transform_point(projection,{0,0,-(distance-radius)});
    const auto back=p::transform_point(projection,{0,0,-(distance+radius)});
    const float frontDepth=front.z/front.w, backDepth=back.z/back.w;
    if (!(frontDepth < backDepth && backDepth < 1.0f))
        std::fprintf(stderr,"Collapsed framed depth: radius=%g distance=%g near=%g far=%g front=%.9g back=%.9g\n",
            radius,distance,frame.nearPlane,frame.farPlane,frontDepth,backDepth);
    CHECK(frontDepth > -1.0f && frontDepth < backDepth && backDepth < 1.0f);
}

INPUT_PROFILE_OWNERS
namespace input=epochengine::input;
namespace gui {bool keyboard_input_captured(){return false;}}
namespace project_input {enum class ActionSemantic{move_x,move_y};}
struct CameraInputContext {
 std::array<bool,static_cast<std::size_t>(epochengine::input::Action::Count)> held{};
 bool action_held_safe(epochengine::input::Action action)const{return held[static_cast<std::size_t>(action)];}
 bool action_held_safe(epochengine::input::Action action,const input::InputProfile&)const{return action_held_safe(action);}
 bool has_input_focus_safe()const{return true;}
};
void editor_navigation(const std::shared_ptr<CameraInputContext>& ctx,bool flying,bool shiftHeld,float dt) {
 EDITOR_FLIGHT_AXES
 EDITOR_FLIGHT_STEP
}
void project_navigation(const std::shared_ptr<CameraInputContext>& ctx,bool flying,bool projectGuiCapturesKeyboard,bool shiftHeld,float dt) {
 // This legacy camera-math lane has no declared project artifact. The separate
 // runtime-input contract executes the real profile admission/evaluation path.
 const auto m_runtimeInputProfile=input::make_default_profile();
 const auto m_cameraMode=p::CameraMode::Editor;
 const bool navigationPointerCaptured=false;
 const auto runtime_input_ready=[] {return true;};
 const auto runtime_planar_axis=[](project_input::ActionSemantic,float value){return value;};
 namespace previewgrid=epochengine::previewgrid;
 PROJECT_FLIGHT_AXES
 PROJECT_FLIGHT_STEP
}
void flight_controls() {
    namespace input=epochengine::input;
    const auto defaults=input::make_default_profile();
    CHECK(input::validate_profile(defaults));
    CHECK(defaults.bindings[input::action_index(input::Action::MoveUp)].primary==input::Key::Space);
    CHECK(defaults.bindings[input::action_index(input::Action::MoveDown)].primary==input::Key::LeftControl);
    CHECK(defaults.bindings[input::action_index(input::Action::MoveDown)].secondary==input::Key::RightControl);
    CHECK(defaults.bindings[input::action_index(input::Action::RollLeft)].primary==input::Key::Q);
    CHECK(defaults.bindings[input::action_index(input::Action::RollRight)].primary==input::Key::E);
    auto custom=defaults;custom.bindings[input::action_index(input::Action::MoveUp)].primary=input::Key::U;
    CHECK(input::validate_profile(custom)&&custom.bindings[input::action_index(input::Action::MoveUp)].primary==input::Key::U);
    CHECK(input::make_profile(input::ProfilePreset::LeftHanded).bindings[input::action_index(input::Action::MoveUp)].primary==input::Key::U);
    for(auto mode:{p::CameraMode::Editor,p::CameraMode::FPS}) {
        int key{},restored{};p::cleanup_context(&key);p::set_camera_mode(&key,mode);
        const auto before=p::camera_for(&key);const auto forward=p::normalize(p::subtract(before.target,before.eye));
        const auto right=p::normalize(p::cross(forward,before.up));
        for(int i=0;i<20;++i)p::step_camera(&key,.05f,0,0,0,0,0,1,1);
        const auto banked=p::camera_for(&key);const auto snapshot=p::capture_camera_rig_snapshot(&key);
        CHECK(std::abs(snapshot.roll_degrees-90)<.001f&&same(banked.eye,before.eye)&&same(banked.target,before.target));
        CHECK(p::dot(banked.up,right)>.999f&&std::abs(p::dot(banked.up,forward))<.0001f);
        CHECK(p::restore_camera_rig_snapshot(&restored,snapshot)&&same(p::camera_for(&restored).up,banked.up));
        for(int i=0;i<20;++i)p::step_camera(&key,.05f,0,0,0,0,0,1,-1);
        CHECK(p::capture_camera_rig_snapshot(&key).roll_degrees==0&&same(p::camera_for(&key).up,before.up));
        const auto beforeInvalid=p::capture_camera_rig_snapshot(&key);
        p::step_camera(&key,.05f,0,0,0,0,0,1,std::numeric_limits<float>::quiet_NaN());
        CHECK(p::capture_camera_rig_snapshot(&key).revision==beforeInvalid.revision);
        auto invalid=snapshot;invalid.roll_degrees=std::numeric_limits<float>::infinity();
        CHECK(!p::restore_camera_rig_snapshot(&key,invalid)&&p::capture_camera_rig_snapshot(&key).revision==beforeInvalid.revision);
        p::step_camera(&key,.05f,0,0,1,0,0);const auto raised=p::camera_for(&key);
        CHECK(raised.eye.y>before.eye.y&&raised.eye.x==before.eye.x&&raised.eye.z==before.eye.z);
        p::step_camera(&key,.05f,0,0,-1,0,0);CHECK(same(p::camera_for(&key).eye,before.eye));
        p::reset_camera(&key);CHECK(p::capture_camera_rig_snapshot(&key).roll_degrees==0);
        if(mode==p::CameraMode::Editor) {
            CHECK(p::set_editor_view(&key,p::EditorView::Front));const auto axis=p::camera_for(&key);
            p::step_camera(&key,.05f,0,0,0,0,0,1,1);CHECK(same(p::camera_for(&key).up,axis.up)&&p::capture_camera_rig_snapshot(&key).roll_degrees==0);
        }
        p::cleanup_context(&key);p::cleanup_context(&restored);
    }
    auto context=std::make_shared<CameraInputContext>();
    context->held[input::action_index(input::Action::RollRight)]=true;
    editor_navigation(context,false,false,.05f);CHECK(p::capture_camera_rig_snapshot(context.get()).roll_degrees==0);
    editor_navigation(context,true,false,.05f);CHECK(p::capture_camera_rig_snapshot(context.get()).roll_degrees>0);
    p::reset_camera(context.get());project_navigation(context,true,true,false,.05f);CHECK(p::capture_camera_rig_snapshot(context.get()).roll_degrees==0);
    project_navigation(context,true,false,false,.05f);CHECK(p::capture_camera_rig_snapshot(context.get()).roll_degrees>0);
    p::cleanup_context(context.get());
    int canvas{};p::set_camera_mode(&canvas,p::CameraMode::Canvas2D);const auto before=p::camera_for(&canvas);
    p::step_camera(&canvas,.05f,0,0,0,0,0,1,1);CHECK(same(p::camera_for(&canvas).up,before.up)&&p::capture_camera_rig_snapshot(&canvas).roll_degrees==0);p::cleanup_context(&canvas);
}
void free_editor_orbit() {
    int key{},restored{},fps{};
    p::set_camera_mode(&key,p::CameraMode::Editor);
    auto seed=p::capture_camera_rig_snapshot(&key);
    seed.yaw_degrees=seed.free_yaw_degrees=0;
    seed.pitch_degrees=seed.free_pitch_degrees=0;
    seed.roll_degrees=0;
    CHECK(p::restore_camera_rig_snapshot(&key,seed));
    const auto initial=p::camera_for(&key);
    auto previousRight=p::normalize(p::cross(p::subtract(initial.target,initial.eye),initial.up));
    for(int i=1;i<=720;++i) {
        p::look_camera(&key,0,.5f);
        const auto current=p::camera_for(&key);
        const auto forward=p::normalize(p::subtract(current.target,current.eye));
        const auto right=p::normalize(p::cross(forward,current.up));
        CHECK(camera::resolve(p::camera_descriptor_for(&key)).valid);
        CHECK(finite(current.up)&&finite(right)&&p::dot(right,previousRight)>.999f);
        CHECK(std::abs(p::dot(right,forward))<1e-5f);
        if(i==180)CHECK(forward.y>.99999f);
        if(i==540)CHECK(forward.y<-.99999f);
        previousRight=right;
    }
    const auto completed=p::camera_for(&key);
    const auto eyeDelta=p::subtract(completed.eye,initial.eye);
    CHECK(p::dot(eyeDelta,eyeDelta)<1e-8f);
    CHECK(p::capture_camera_rig_snapshot(&key).pitch_degrees==0);
    p::look_camera(&key,0,120);
    const auto inverted=p::capture_camera_rig_snapshot(&key);
    CHECK(inverted.pitch_degrees==120&&inverted.free_pitch_degrees==120);
    CHECK(p::restore_camera_rig_snapshot(&restored,inverted));
    CHECK(same(p::camera_for(&restored).up,p::camera_for(&key).up));
    CHECK(p::set_editor_view(&key,p::EditorView::Front));
    const auto locked=p::capture_camera_rig_snapshot(&key);
    p::look_camera(&key,10,100);p::step_camera(&key,.05f,0,0,0,1,1,1,1);
    CHECK(p::capture_camera_rig_snapshot(&key).pitch_degrees==locked.pitch_degrees);
    CHECK(p::set_editor_view(&key,p::EditorView::Perspective));
    CHECK(p::capture_camera_rig_snapshot(&key).pitch_degrees==120);
    CHECK(p::restore_camera_rig_snapshot(&key,seed));
    for(int i=0;i<40;++i)p::step_camera(&key,.05f,0,0,0,0,1);
    CHECK(p::capture_camera_rig_snapshot(&key).pitch_degrees<-170);
    CHECK(p::restore_camera_rig_snapshot(&key,seed));p::look_camera(&key,0,90);
    for(int i=0;i<20;++i)p::step_camera(&key,.05f,0,0,0,0,0,1,1);
    CHECK(camera::resolve(p::camera_descriptor_for(&key)).valid);
    CHECK(std::abs(p::capture_camera_rig_snapshot(&key).roll_degrees-90)<.001f);
    p::set_camera_mode(&fps,p::CameraMode::FPS);
    p::look_camera(&fps,0,200);CHECK(p::capture_camera_rig_snapshot(&fps).pitch_degrees==85);
    p::step_camera(&fps,.05f,0,0,0,0,-100);CHECK(p::capture_camera_rig_snapshot(&fps).pitch_degrees==-85);
    auto fpsSnapshot=p::capture_camera_rig_snapshot(&fps);fpsSnapshot.pitch_degrees=200;
    CHECK(p::restore_camera_rig_snapshot(&fps,fpsSnapshot));
    CHECK(p::capture_camera_rig_snapshot(&fps).pitch_degrees==85);
    for(auto context:{&key,&restored,&fps})p::cleanup_context(context);
}
void surface_zoom() {
    int key{};p::set_camera_mode(&key,p::CameraMode::Editor);
    p::ObjectMarker marker;marker.primitive=p::ObjectPreviewPrimitive::IndexedMesh;marker.mesh=normalized_mesh();
    CHECK(p::set_object_markers(&key,std::span{&marker,1u}));CHECK(p::focus_camera(&key,{},10));
    for(unsigned i=0;i<20u;++i)p::zoom_camera(&key,12);
    auto eye=p::camera_for(&key).eye;
    CHECK(std::abs(eye.x)+std::abs(eye.y)+std::abs(eye.z)>10);
    CHECK(p::camera_distance_for(&key)>5&&p::camera_distance_for(&key)<10); // real octahedron, not radius/AABB proxy
    auto stopped=p::capture_camera_rig_snapshot(&key);const auto revision=p::camera_revision_for(&key);
    p::dolly_camera_drag(&key,-100);CHECK(p::camera_distance_for(&key)==stopped.distance);
    CHECK(p::camera_revision_for(&key)==revision); // repeated inward drag cannot creep through the surface
    p::zoom_camera(&key,-1);CHECK(p::camera_distance_for(&key)>stopped.distance);
    p::zoom_camera(&key,12);stopped=p::capture_camera_rig_snapshot(&key);stopped.fly_speed=100;
    CHECK(p::restore_camera_rig_snapshot(&key,stopped));
    p::step_camera(&key,.05f,1,0,0,0,0);eye=p::camera_for(&key).eye;
    CHECK(std::abs(eye.x)+std::abs(eye.y)+std::abs(eye.z)<10); // explicit keyboard inspection remains unrestricted
    p::cleanup_context(&key);
    // Rebased publication retains the navigation eye which produced it. Multiple
    // wheel ticks before the next render use that same transform, not world-space
    // rays against camera-relative terrain at the origin.
    p::set_camera_mode(&key,p::CameraMode::Editor);const p::Vec3 center{-39295424,0,0};
    CHECK(p::focus_camera(&key,center,6000000));auto camera=p::camera_for(&key);const auto worldEye=camera.eye;
    const auto preciseEye=p::camera_world_eye(camera);
    const auto direction=p::camera_forward(camera);camera.eye={};camera.target=direction;camera.navigationValid=false;
    marker.position={float(double(center.x)-preciseEye[0]),float(-preciseEye[1]),float(-preciseEye[2])};marker.scale={600000,600000,600000};marker.wireframe=true;
    marker.preciseTransform=p::make_precise_mesh_transform({double(center.x)-preciseEye[0],-preciseEye[1],-preciseEye[2]},{600000,600000,600000},{});
    CHECK(p::publish_scene_frame(&key,std::span{&marker,1u},camera,{}));
    CHECK(same(p::scene_frame_for(&key)->worldNavigationEye,worldEye));
    for(unsigned i=0;i<20u;++i)p::zoom_camera(&key,12);
    eye=p::subtract(p::camera_for(&key).eye,center);
    CHECK((double(std::abs(eye.x))+std::abs(eye.y)+std::abs(eye.z))/600000>10);
    CHECK(p::camera_distance_for(&key)>3000000&&p::camera_distance_for(&key)<6000000);
    check_camera(&key);p::cleanup_context(&key);
}
double eye_distance(const p::Camera& a,const p::Camera& b) {
    const auto one=p::camera_world_eye(a),two=p::camera_world_eye(b);
    double result{};for(unsigned axis=0;axis<3;++axis)result+=(one[axis]-two[axis])*(one[axis]-two[axis]);
    return std::sqrt(result);
}
void translated_close_navigation() {
    // At this actual operator-scale translation, float positions have an8m
    // quantum. Camera motion must retain its local displacement and direction.
    const p::Vec3 center{-94398480.0f,-6190638.0f,-917183.0f};
    for(const float distance:{4.0f,5.0f}) {
        int local{},translated{},restored{};
        CHECK(p::focus_camera(&local,{},1));CHECK(p::focus_camera(&translated,center,1));
        for(const auto key:{&local,&translated}) {
            auto view=p::capture_camera_rig_snapshot(key);view.distance=distance;
            CHECK(p::restore_camera_rig_snapshot(key,view));
        }
        const auto compare=[&] {
            const auto close=p::camera_for(&local),far=p::camera_for(&translated);
            const auto a=p::camera_world_eye(close),b=p::camera_world_eye(far);
            CHECK(std::abs((b[0]-center.x)-a[0])<.0001);
            CHECK(std::abs((b[1]-center.y)-a[1])<.0001);
            CHECK(std::abs((b[2]-center.z)-a[2])<.0001);
            CHECK(p::dot(p::camera_forward(close),p::camera_forward(far))>.99999f);
            CHECK(camera::resolve(p::camera_descriptor_for(&translated)).valid);
        };
        CHECK(p::camera_for(&translated).navigationValid);compare();
        const auto before=p::camera_for(&translated);
        for(unsigned i=0;i<100;++i) {
            p::pan_camera_drag(&local,.5f,-.25f);p::pan_camera_drag(&translated,.5f,-.25f);
            p::step_camera(&local,.016f,0,.125f,.25f,0,0);
            p::step_camera(&translated,.016f,0,.125f,.25f,0,0);
            compare();
        }
        CHECK(eye_distance(before,p::camera_for(&translated))>.5);
        const auto moved=p::camera_for(&translated);
        const auto snapshot=p::capture_camera_rig_snapshot(&translated);
        CHECK(p::restore_camera_rig_snapshot(&restored,snapshot));
        CHECK(eye_distance(moved,p::camera_for(&restored))<.0001);
        CHECK(p::capture_camera_rig_snapshot(&restored).navigation_origin==snapshot.navigation_origin);
        CHECK(p::set_editor_view(&restored,p::EditorView::Front));
        CHECK(p::set_editor_view(&restored,p::EditorView::Perspective));
        CHECK(eye_distance(moved,p::camera_for(&restored))<.0001);
        p::set_camera_mode(&restored,p::CameraMode::FPS);
        CHECK(eye_distance(moved,p::camera_for(&restored))<.0001);
        CHECK(p::dot(p::camera_forward(moved),p::camera_forward(p::camera_for(&restored)))>.99999f);
        p::set_camera_mode(&restored,p::CameraMode::Editor);
        CHECK(eye_distance(moved,p::camera_for(&restored))<.0001);
        auto invalid=snapshot;invalid.navigation_origin[0]=std::numeric_limits<double>::quiet_NaN();
        const auto revision=p::camera_revision_for(&restored);
        CHECK(!p::restore_camera_rig_snapshot(&restored,invalid));
        CHECK(p::camera_revision_for(&restored)==revision);
        p::reset_camera(&restored);
        CHECK(p::capture_camera_rig_snapshot(&restored).navigation_origin==std::array<double,3>{});
        CHECK(eye_distance(p::camera_for(&restored),before)>1000000);
        for(const auto key:{&local,&translated,&restored})p::cleanup_context(key);
    }
}
void near_surface_pan_scale() {
    int key{};const p::Vec3 center{-94398480.0f,0,0};
    CHECK(p::focus_camera(&key,center,1000000));
    auto view=p::capture_camera_rig_snapshot(&key);
    view.distance=1000005;view.yaw_degrees=-90;view.pitch_degrees=0;
    view.free_yaw_degrees=-90;view.free_pitch_degrees=0;
    CHECK(p::restore_camera_rig_snapshot(&key,view));
    auto camera=p::camera_for(&key);const auto origin=p::camera_world_eye(camera);
    const auto before=camera;camera.eye={};camera.target=p::camera_forward(camera);camera.navigationValid=false;
    p::ObjectMarker marker;marker.primitive=p::ObjectPreviewPrimitive::IndexedMesh;
    marker.mesh=normalized_mesh();marker.scale={100000,100000,100000};
    marker.position={float(double(center.x)-origin[0]),float(-origin[1]),float(-origin[2])};
    marker.preciseTransform=p::make_precise_mesh_transform(
        {double(center.x)-origin[0],-origin[1],-origin[2]},{100000,100000,100000},{});
    CHECK(p::publish_scene_frame(&key,std::span{&marker,1u},camera,{}));
    p::pan_camera_drag(&key,20,0);
    const double displacement=eye_distance(before,p::camera_for(&key));
    CHECK(displacement>.005&&displacement<1.0); // surface5m away, not orbit focus1000005m away
    CHECK(p::restore_camera_rig_snapshot(&key,view));
    p::set_camera_mode(&key,p::CameraMode::FPS);
    CHECK(eye_distance(before,p::camera_for(&key))<.0001);
    p::set_camera_mode(&key,p::CameraMode::Editor);
    CHECK(eye_distance(before,p::camera_for(&key))<.0001);
    CHECK(p::restore_camera_rig_snapshot(&key,view));
    for(unsigned i=0;i<20;++i)p::zoom_camera(&key,12);
    const auto close=p::camera_world_eye(p::camera_for(&key));
    CHECK(close[2]>1000000&&close[2]<1000001); // overview clip distance must not keep wheel metres away
    p::cleanup_context(&key);
}
int main(int argc,char**argv) {
    translated_close_navigation();
    near_surface_pan_scale();
    surface_zoom();
    free_editor_orbit();
    flight_controls();
    CHECK(argc==2 || argc==4);const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    if(argc==4) {
        int universeKey{};const auto input=mesh::read_artifact_file(argv[2]);CHECK(input);
        const float scale=std::strtof(argv[3],nullptr);
        p::ObjectMarker marker{.scale={scale,scale,scale},.primitive=p::ObjectPreviewPrimitive::IndexedMesh,.mesh=input.artifact};
        CHECK(p::frame_initial_orbit_mesh(&universeKey,marker,1.6f));
        const auto frame=p::camera_for(&universeKey);
        const auto matrix=p::multiply(p::projection_for(&universeKey,1.6f,frame),p::look_at(frame.eye,frame.target,frame.up));
        unsigned inside{},atFar{},outside{};float minimum=1.0f,maximum=-1.0f;
        for(const auto& vertex:input.artifact->vertices()) {
            const auto clip=p::transform_point(matrix,{vertex.position[0]*scale,vertex.position[1]*scale,vertex.position[2]*scale});
            const float depth=clip.z/clip.w;minimum=(std::min)(minimum,depth);maximum=(std::max)(maximum,depth);
            if(clip.w>0 && std::abs(clip.x)<=clip.w && std::abs(clip.y)<=clip.w && std::abs(clip.z)<=clip.w)++inside;
            if(depth==1.0f)++atFar;if(depth>1.0f)++outside;
        }
        std::fprintf(stderr,"Actual artifact: vertices=%zu distance=%.9g near=%.9g far=%.9g inside=%u atFar=%u beyondFar=%u depth=[%.9g,%.9g]\n",
            input.artifact->vertices().size(),p::camera_distance_for(&universeKey),frame.nearPlane,frame.farPlane,inside,atFar,outside,minimum,maximum);
        CHECK(inside==input.artifact->vertices().size() && atFar==0 && outside==0 && minimum<maximum);
        p::cleanup_context(&universeKey);
    }
    legacy_trace(root/"legacy.bin");
    int a{},b{},fresh{},badKey{};
    const auto artifact=normalized_mesh();
    for(float radius:{25000.0f,1737400.0f,6371000.0f,1000000000.0f}) {
        p::cleanup_context(&a);p::cleanup_context(&b);p::cleanup_context(&fresh);
        p::set_camera_mode(&a,p::CameraMode::Editor);
        p::ObjectMarker marker{.position={100,-200,300},.scale={radius/10,radius/10,radius/10},
            .primitive=p::ObjectPreviewPrimitive::IndexedMesh,.mesh=artifact};
        const auto pristine=p::capture_camera_rig_snapshot(&a);
        const auto framing=p::initial_orbit_mesh_framing(marker,pristine,1.6f);CHECK(framing);
        CHECK(std::abs(framing->geometryRadius/radius-1)<1e-6f && framing->distance>radius);
        CHECK(p::frame_initial_orbit_mesh(&a,marker,1.6f));check_camera(&a);
        check_framed_depth(&a,radius);
        const auto framed=p::capture_camera_rig_snapshot(&a);
        CHECK(same(framed.focus,marker.position) && framed.distance==framing->distance && framed.scene_extent>=radius);
        CHECK(p::camera_for(&a).farPlane>framed.distance+radius);
        CHECK(!p::frame_initial_orbit_mesh(&a,marker,1.6f)); // never overwrite the chosen view
        p::pan_camera_drag(&a,10,-5);check_camera(&a);
        CHECK(!same(p::capture_camera_rig_snapshot(&a).focus,framed.focus));
        const float beforeDolly=p::camera_distance_for(&a);p::dolly_camera_drag(&a,-10);
        CHECK(p::camera_distance_for(&a)<beforeDolly);check_camera(&a);
        CHECK(p::restore_camera_rig_snapshot(&a,framed));
        check_framed_depth(&a,radius);
        CHECK(p::set_editor_view(&a,p::EditorView::Front));
        CHECK(p::camera_for(&a).orientation==camera::ViewOrientation::front);
        CHECK(p::camera_distance_for(&a)>radius*2);check_camera(&a);
        p::zoom_camera(&a,1);const auto front=p::capture_camera_rig_snapshot(&a);
        CHECK(p::set_editor_view(&a,p::EditorView::Perspective));
        CHECK(p::capture_camera_rig_snapshot(&a).distance==framed.distance);
        CHECK(p::set_editor_view(&a,p::EditorView::Front));
        CHECK(p::capture_camera_rig_snapshot(&a).orthographic_vertical_size==front.orthographic_vertical_size);
        CHECK(p::restore_camera_rig_snapshot(&b,p::capture_camera_rig_snapshot(&a)));
        CHECK(p::capture_camera_rig_snapshot(&b).scene_extent==framed.scene_extent);check_camera(&b);
        CHECK(!p::frame_initial_orbit_mesh(&b,marker,1.6f)); // restored views remain authoritative
        CHECK(p::set_editor_view(&a,p::EditorView::Perspective));
        for(unsigned step=0;step<24;++step)p::zoom_camera(&a,12);
        CHECK(p::camera_distance_for(&a)==0.5f && p::camera_for(&a).farPlane>=2*radius
            && p::camera_for(&a).nearPlane==0.05f);check_camera(&a);
        for(unsigned step=0;step<24;++step)p::zoom_camera(&a,-12);
        CHECK(p::camera_distance_for(&a)==1e12f);check_camera(&a);
        CHECK(p::restore_camera_rig_snapshot(&b,p::capture_camera_rig_snapshot(&a)));
        CHECK(p::camera_distance_for(&b)==1e12f);check_camera(&b);
        // Fresh-project reopen has no camera layout codec: the same admitted mesh
        // reconstructs framing extent in a fresh rig through the real owner.
        CHECK(p::frame_initial_orbit_mesh(&fresh,marker,1.6f));
        CHECK(p::capture_camera_rig_snapshot(&fresh).scene_extent==framed.scene_extent);
        CHECK(p::camera_distance_for(&fresh)==framed.distance);check_camera(&fresh);
        auto custom=pristine;custom.distance=20;
        CHECK(!p::initial_orbit_mesh_framing(marker,custom,1.6f));
        CHECK(!p::initial_orbit_mesh_framing(marker,pristine,1.6f,false));
        const auto narrow=p::initial_orbit_mesh_framing(marker,pristine,0.5f);CHECK(narrow && narrow->distance>framing->distance);
        p::set_camera_mode(&b,p::CameraMode::FPS);
        CHECK(p::capture_camera_rig_snapshot(&b).scene_extent>=radius && p::camera_for(&b).farPlane>=2*radius);
        p::set_camera_mode(&b,p::CameraMode::Canvas2D);
        CHECK(p::capture_camera_rig_snapshot(&b).scene_extent==0 && p::camera_for(&b).farPlane==4096);
    }
    const auto stable=p::capture_camera_rig_snapshot(&a);const auto revision=p::camera_revision_for(&a);
    const p::ObjectMarker sample{.scale={2500,2500,2500},
        .primitive=p::ObjectPreviewPrimitive::IndexedMesh,.mesh=artifact};
    const float nan=std::numeric_limits<float>::quiet_NaN(), inf=std::numeric_limits<float>::infinity();
    for(float invalid:{nan,inf,-inf}) {
        CHECK(!p::focus_camera(&a,{invalid,0,0},1));CHECK(!p::focus_camera(&a,{},invalid));
        p::zoom_camera(&a,invalid);p::pan_camera_drag(&a,invalid,1);p::dolly_camera_drag(&a,invalid);
        p::look_camera(&a,invalid,1);p::step_camera(&a,invalid,1,0,0,0,0);
        p::step_camera(&a,.01f,1,0,0,invalid,0);p::step_camera(&a,.01f,1,0,0,0,0,invalid);
        CHECK(p::camera_revision_for(&a)==revision);
        auto bad=stable;bad.distance=invalid;CHECK(!p::restore_camera_rig_snapshot(&a,bad));
        bad=stable;bad.scene_extent=invalid;CHECK(!p::restore_camera_rig_snapshot(&a,bad));
        bad=stable;bad.view_distances[7]=invalid;CHECK(!p::restore_camera_rig_snapshot(&a,bad));
        bad=stable;bad.view_focuses[7].x=invalid;CHECK(!p::restore_camera_rig_snapshot(&a,bad));
        bad=stable;bad.view_orthographic_sizes[7]=invalid;CHECK(!p::restore_camera_rig_snapshot(&a,bad));
        auto invalidView=p::capture_camera_rig_snapshot(&badKey);invalidView.scene_extent=invalid;
        CHECK(!p::initial_orbit_mesh_framing(sample,invalidView,1.6f));
        CHECK(p::camera_revision_for(&a)==revision);
        p::zoom_camera(&badKey,invalid);p::pan_camera_drag(&badKey,invalid,0);
        p::step_camera(&badKey,invalid,1,0,0,0,0);CHECK(p::camera_revision_for(&badKey)==0);
    }
    CHECK(!p::focus_camera(&a,{(std::numeric_limits<float>::max)(),0,0},1));
    CHECK(!p::focus_camera(&a,{},(std::numeric_limits<float>::max)()));
    p::pan_camera_drag(&a,(std::numeric_limits<float>::max)(),1);
    p::step_camera(&a,.01f,1,0,0,(std::numeric_limits<float>::max)(),0);
    CHECK(p::camera_revision_for(&a)==revision);check_camera(&a);
    auto invalidExtent=stable;invalidExtent.scene_extent=1e13f;
    CHECK(!p::restore_camera_rig_snapshot(&a,invalidExtent));
    invalidExtent=stable;invalidExtent.orientation=static_cast<camera::ViewOrientation>(255);
    CHECK(!p::restore_camera_rig_snapshot(&a,invalidExtent));
    p::cleanup_context(&a);p::set_camera_mode(&a,p::CameraMode::Editor);
    CHECK(p::focus_camera(&a,{},1e12f) && p::camera_distance_for(&a)==1e12f);check_camera(&a);
    p::cleanup_context(&a);p::set_camera_mode(&a,p::CameraMode::Canvas2D);
    CHECK(p::focus_camera(&a,{},1e8f));
    auto canvas=p::capture_camera_rig_snapshot(&a);
    CHECK(canvas.distance==2048 && canvas.orthographic_vertical_size==4096 && canvas.scene_extent==0);
    p::zoom_camera(&a,-12);CHECK(p::camera_distance_for(&a)==4096);
    canvas.distance=1e12f;canvas.orthographic_vertical_size=1e12f;canvas.scene_extent=1e8f;
    CHECK(p::restore_camera_rig_snapshot(&a,canvas));canvas=p::capture_camera_rig_snapshot(&a);
    CHECK(canvas.distance==2048 && canvas.orthographic_vertical_size==4096 && canvas.scene_extent==0);
    CHECK(p::camera_for(&a).farPlane==4096 && p::camera_for(&a).nearPlane==0.01f);
    p::cleanup_context(&a);p::cleanup_context(&b);p::cleanup_context(&fresh);p::cleanup_context(&badKey);
}
'''


class CameraWorldExtent(unittest.TestCase):
    def test_actual_world_framing_navigation_and_restore(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 CPU module consumer")
        proof = ROOT / "Artifacts/Reports/camera-world-extent-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/camera-world-extent"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "camera.world_extent_contract.cpp"
        owner=(ROOT/"Engine/modules/input.engine.ixx").read_text(encoding="utf-8")
        types=("    export enum Key", "    export enum MouseButton", "    export enum class Action", "    export struct ActionBinding", "    export struct InputProfile", "    export enum class ProfilePreset")
        functions=("    export inline constexpr std::size_t action_index", "    export inline constexpr ActionBinding key_binding", "    export inline constexpr ActionBinding mouse_binding", "    export inline constexpr InputProfile make_default_profile", "    export inline constexpr InputProfile make_profile", "    export inline bool validate_profile")
        profile="namespace epochengine::input {\n"+"\n".join(definition(owner,m,True).replace("export ","") for m in types)+"\n"+"\n".join(definition(owner,m).replace("export ","") for m in functions)+"\n}"
        code=CODE.replace("INPUT_PROFILE_OWNERS",profile)
        for tag,file in (("EDITOR","epoch.session_runtime.inl"),("PROJECT","epoch.project_play_scene.inl")):
            owner=(ROOT/"Engine/src/epoch/implementation"/file).read_text(encoding="utf-8")
            start=owner.index("const bool keyboardNavigation =")
            end=owner.index("if (ctx->action_pressed_safe",start) if tag=="EDITOR" else owner.index("if ((flying",start)
            axes=owner[start:end]
            start=owner.index("epochengine::previewgrid::step_camera(",end);end=owner.index(";",start)+1
            code=code.replace(tag+"_FLIGHT_AXES",axes).replace(tag+"_FLIGHT_STEP",owner[start:end])
        source.write_text(code, encoding="utf-8")
        roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        available = {}
        for directory in roots:
            for path in directory.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    available[match.group(1)] = path
        pending, selected = ["render.preview_grid", "authoring.texture_detail"], {}
        while pending:
            name = pending.pop()
            if name in selected:
                continue
            self.assertIn(name, available)
            selected[name] = available[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        modules = " ".join('"'+path.as_posix()+'"' for path in selected.values())
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(CameraWorldExtent LANGUAGES CXX)
add_executable(camera_world_extent "{source.as_posix()}" "{(ROOT / 'Engine/src/render/render.preview_camera.cpp').as_posix()}" "{(ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}"
 "{(ROOT/'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}"
 "{(ROOT/'Engine/src/authoring/texture/authoring.texture_residency.cpp').as_posix()}")
target_compile_features(camera_world_extent PRIVATE cxx_std_23)
target_compile_options(camera_world_extent PRIVATE /W3 /WX /utf-8)
target_link_options(camera_world_extent PRIVATE /DEBUG:NONE)
target_include_directories(camera_world_extent PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(camera_world_extent PRIVATE FILE_SET CXX_MODULES BASE_DIRS {" ".join('"'+path.as_posix()+'"' for path in roots)} FILES {modules})
enable_testing()
add_test(NAME actual_camera_world_extent COMMAND camera_world_extent "{(proof/'fixture').as_posix()}/$<CONFIG>")
set_tests_properties(actual_camera_world_extent PROPERTIES TIMEOUT 20)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for configuration in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", configuration, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", configuration, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        for configuration in ("Debug", "Release"):
            data = (proof / "fixture" / configuration / "legacy.bin").read_bytes()
            self.assertEqual(len(data), 8160)
            self.assertEqual(hashlib.sha256(data).hexdigest(), LEGACY_TRACE_SHA256)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "modules": len(selected), "commands": results,
            "scope": "Actual preview camera/mesh module; no renderer, window, model or generated project execution",
            "sourceIdentity": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in selected.values()},
            "legacyTraceSha256": LEGACY_TRACE_SHA256,
            "cases": ["actual admitted octahedron surface wheel/dolly collision without radius/AABB proxies, reverse zoom and keyboard interior inspection",
                      "94-million-metre translation preserves4/5m camera eye/direction and100submetre pan/flight turns against the origin-view baseline",
                      "precise camera context snapshot, Editor/FPS/axis-view transfer/reset and nonfinite origin refusal",
                      "center-ray surface5m away governs panning despite a1-million-metre orbit focus distance",
                      "camera-relative wire mesh on translated 6-million-metre body; repeated wheel events reuse exact publication transform",
                      "actual flight roll, Space/Ctrl/Q/E profiles, custom-profile preservation, snapshot/reversal/finite/axis/Canvas2D guards",
                      "normal Editor/FPS/Canvas2D navigation matches8160-byte pre-change trace",
                      "25km/Moon/Earth/1-billion-metre-radius framing with distinct representable front/back depth below far",
                      "pan/dolly/zoom/axis view/saved view/context restore and fresh mesh reconstruction",
                      "retained extent survives close zoom and3D mode switches; Canvas2D bounds unchanged",
                      "1e12 input ceiling and atomic nonfinite/overflow/invalid snapshot refusal"],
            "persistence": "Context snapshot transfers the full typed rig. Existing layout preferences do not persist camera poses; fresh project framing reconstructs extent from admitted geometry. Disk-persisted custom camera views are not claimed.",
            "limits": "Bounded camera inputs and clipping only; no huge-world precision, terrain fidelity or rendering performance guarantee.",
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
