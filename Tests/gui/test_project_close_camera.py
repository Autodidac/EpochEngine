"""Actual Editor close wrapper and real camera owner; close admission is a port."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]


def consumer():
    owner=ROOT/'Engine/src/editor/application/editor.application_context_lifecycle.inl'
    wrapper=definition(owner.read_text(encoding='utf-8'),'    bool editor_close_project(')
    return r'''
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
import render.preview_grid;
namespace preview=epochengine::previewgrid;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Project close camera line %d\n",line);std::exit(86);}}
#define CHECK(x) check(static_cast<bool>(x),__LINE__)
bool same_vec(preview::Vec3 a,preview::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same(const preview::Camera&a,const preview::Camera&b){return same_vec(a.eye,b.eye)&&same_vec(a.target,b.target)&&same_vec(a.up,b.up)
 &&a.fovRadians==b.fovRadians&&a.nearPlane==b.nearPlane&&a.farPlane==b.farPlane&&a.projection==b.projection
 &&a.orientation==b.orientation&&a.orthographicVerticalSize==b.orthographicVerticalSize;}
bool same(const preview::CameraRigSnapshot&a,const preview::CameraRigSnapshot&b){
 if(a.valid!=b.valid||a.mode!=b.mode||!same_vec(a.focus,b.focus)||!same_vec(a.position,b.position)||a.yaw_degrees!=b.yaw_degrees
  ||a.pitch_degrees!=b.pitch_degrees||a.free_yaw_degrees!=b.free_yaw_degrees||a.free_pitch_degrees!=b.free_pitch_degrees
  ||a.distance!=b.distance||a.orthographic_vertical_size!=b.orthographic_vertical_size||a.fly_speed!=b.fly_speed||a.projection!=b.projection
  ||a.orientation!=b.orientation||a.view_distances!=b.view_distances||a.view_orthographic_sizes!=b.view_orthographic_sizes
  ||a.logical_view_id!=b.logical_view_id||a.logical_view_generation!=b.logical_view_generation||a.revision!=b.revision||a.scene_extent!=b.scene_extent)return false;
 for(std::size_t i=0;i<a.view_focuses.size();++i)if(!same_vec(a.view_focuses[i],b.view_focuses[i]))return false;
 return true;
}
namespace epochengine{
namespace core{struct Context{int identity{};};}
struct EditorState{int applicationKind{7};preview::CameraMode viewportCameraMode{preview::CameraMode::Editor};
 preview::CameraMode scratchMode{preview::CameraMode::Editor};bool blocked{};std::string projectId{"saved-project"};unsigned closeCalls{};};
std::unordered_map<const void*,EditorState>states;unsigned lookups{},profiles{};
EditorState&editor_state_for(const std::shared_ptr<core::Context>&context){++lookups;return states[context.get()];}
int editor_application_profile(int kind){++profiles;return kind+100;}
bool close_active_project(EditorState&state,int profile,bool clearScene){
 CHECK(profile==state.applicationKind+100&&clearScene);++state.closeCalls;
 if(state.blocked)return false;
 state.projectId.clear();state.viewportCameraMode=state.scratchMode;return true;
}
WRAPPER
}
std::string read(const std::filesystem::path&path){std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>{input},{}};}
int main(int argc,char**argv){
 using namespace epochengine;CHECK(argc==2);const std::filesystem::path source{argv[1]};std::filesystem::create_directories(source.parent_path());
 const std::string original="saved source and camera settings must stay intact\n";{std::ofstream out(source,std::ios::binary);out<<original;CHECK(out);}
 auto context=std::make_shared<core::Context>();auto other=std::make_shared<core::Context>();int expectedKey{};
 preview::set_camera_mode(context.get(),preview::CameraMode::Editor);
 preview::set_camera_mode(other.get(),preview::CameraMode::FPS);preview::step_camera(other.get(),.1f,1,0,0,0,0,1);
 preview::Camera external{.eye={170000,220000,310000},.target={170000,220000,309000},.up={1,0,0},.fovRadians=1.05f,.nearPlane=10,.farPlane=1e10f};
 CHECK(preview::set_external_camera_frame(other.get(),external));
 const auto otherCamera=preview::camera_for(other.get());const auto otherRig=preview::capture_camera_rig_snapshot(other.get());
 auto huge=preview::capture_camera_rig_snapshot(context.get());huge.focus={1e9f,2e8f,-3e9f};huge.distance=2.75e9f;huge.scene_extent=4e9f;
 huge.view_focuses.fill(huge.focus);huge.view_distances.fill(huge.distance);CHECK(preview::restore_camera_rig_snapshot(context.get(),huge));
 CHECK(preview::set_external_camera_frame(context.get(),external));
 const auto beforeOrdinaryReset=preview::capture_camera_rig_snapshot(context.get());
 preview::reset_camera(context.get()); // Ordinary in-scene reset retains known world extent.
 const auto ordinaryReset=preview::capture_camera_rig_snapshot(context.get());
 CHECK(ordinaryReset.scene_extent==huge.scene_extent&&ordinaryReset.logical_view_id==beforeOrdinaryReset.logical_view_id
  &&ordinaryReset.logical_view_generation==beforeOrdinaryReset.logical_view_generation&&!preview::clear_external_camera_frame(context.get()));
 CHECK(preview::restore_camera_rig_snapshot(context.get(),huge)&&preview::set_external_camera_frame(context.get(),external));
 const auto before=preview::camera_for(context.get());const auto beforeRig=preview::capture_camera_rig_snapshot(context.get());
 auto&state=states[context.get()];state.blocked=true;
 CHECK(!editor_close_project(context)&&state.projectId=="saved-project"&&state.closeCalls==1);
 CHECK(same(preview::camera_for(context.get()),before)&&same(preview::capture_camera_rig_snapshot(context.get()),beforeRig));
 CHECK(same(preview::camera_for(other.get()),otherCamera)&&same(preview::capture_camera_rig_snapshot(other.get()),otherRig));
 CHECK(read(source)==original);
 const auto priorLookups=lookups,priorProfiles=profiles;CHECK(!editor_close_project({}));CHECK(lookups==priorLookups&&profiles==priorProfiles);
 CHECK(same(preview::capture_camera_rig_snapshot(context.get()),beforeRig)&&read(source)==original);
 for(const auto mode:{preview::CameraMode::Editor,preview::CameraMode::Canvas2D}){
  state.blocked=false;state.projectId="saved-project";state.scratchMode=mode;
  preview::set_camera_mode(context.get(),preview::CameraMode::Editor);CHECK(preview::restore_camera_rig_snapshot(context.get(),huge));
  CHECK(preview::set_external_camera_frame(context.get(),external));
  const auto priorClose=preview::capture_camera_rig_snapshot(context.get());
  preview::set_camera_mode(&expectedKey,mode);preview::reset_camera(&expectedKey);const auto expected=preview::camera_for(&expectedKey);
  CHECK(editor_close_project(context)&&state.projectId.empty()&&preview::camera_mode_for(context.get())==mode);
  CHECK(same(preview::camera_for(context.get()),expected));
  const auto after=preview::capture_camera_rig_snapshot(context.get());const auto normal=preview::capture_camera_rig_snapshot(&expectedKey);
  CHECK(after.scene_extent==0&&after.distance==normal.distance&&after.orthographic_vertical_size==normal.orthographic_vertical_size);
  CHECK(after.logical_view_id==priorClose.logical_view_id&&after.logical_view_generation==priorClose.logical_view_generation);
  CHECK(!preview::clear_external_camera_frame(context.get()));
  CHECK(same(preview::camera_for(other.get()),otherCamera)&&same(preview::capture_camera_rig_snapshot(other.get()),otherRig));
  CHECK(read(source)==original);
 }
 preview::cleanup_context(context.get());preview::cleanup_context(other.get());preview::cleanup_context(&expectedKey);
}
'''.replace('WRAPPER',wrapper)


class ProjectCloseCamera(unittest.TestCase):
    def test_actual_close_wrapper_resets_only_successful_context(self):
        if os.name!='nt':self.skipTest('Installed MSVC CPU module consumer')
        report=ROOT/'Artifacts/Reports/project-close-camera-oct28'/str(time.time_ns());report.mkdir(parents=True)
        source=report/'editor.project_close_contract.cpp';source.write_text(consumer(),encoding='utf-8')
        roots=[ROOT/'Engine/modules',ROOT/'Engine/dep/EpochPlatformEngine/modules'];modules={}
        for folder in roots:
            for path in folder.glob('*.ixx'):
                match=re.search(r'^export module\s+([\w.:]+);',path.read_text(encoding='utf-8-sig'),re.M)
                if match:modules[match[1]]=path
        selected={};pending=['render.preview_grid','authoring.texture_detail']
        while pending:
            name=pending.pop()
            if name in selected:continue
            selected[name]=modules[name];pending+=re.findall(r'^(?:export )?import\s+([\w.]+);',modules[name].read_text(encoding='utf-8-sig'),re.M)
        implementations=[ROOT/path for path in ('Engine/src/render/render.preview_camera.cpp', 'Engine/src/render/render.preview_lighting.cpp','Engine/src/assets/texture/asset.texture_artifact.cpp','Engine/src/authoring/texture/authoring.texture_document.cpp','Engine/src/authoring/texture/authoring.texture_residency.cpp')]
        (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ProjectCloseCamera LANGUAGES CXX)
add_executable(contract "{source.as_posix()}" {' '.join('"'+p.as_posix()+'"' for p in implementations)})
target_compile_features(contract PRIVATE cxx_std_23)
target_compile_options(contract PRIVATE /W3 /WX /utf-8 /EHsc)
target_include_directories(contract PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {' '.join('"'+p.as_posix()+'"' for p in roots)} FILES {' '.join('"'+p.as_posix()+'"' for p in selected.values())})
''',encoding='utf-8')
        build=ROOT/'Artifacts/Intermediate/tests/project-close-camera'/report.name
        commands=[['cmake','-S',str(report),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']]
        for mode in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',mode,'--parallel','1'],[str(build/mode/'contract.exe'),str(report/mode/'saved.epoch')]]
        results=[]
        for index,command in enumerate(commands):
            ran=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=150)
            (report/f'lane-{index}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8');results.append({'command':command,'exit':ran.returncode})
            self.assertEqual(ran.returncode,0,ran.stdout[-6000:]+ran.stderr)
        owner=ROOT/'Engine/src/editor/application/editor.application_context_lifecycle.inl'
        (report/'result.json').write_text(json.dumps({'passed':True,'commands':results,'owners':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [owner,source,Path(__file__),*selected.values(),*implementations]},'scope':'Actual editor_close_project wrapper + real preview camera rig. Explicit close admission/state/profile/context ports; successful close resets huge navigation extent and external frame to returned scratch mode, blocked close and null context leave exact rig/project data unchanged, second context and saved disk source preserved. No window/model/GPU/full Engine build.'},indent=2),encoding='utf-8')


if __name__=='__main__':unittest.main()
