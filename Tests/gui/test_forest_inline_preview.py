"""Execute actual inline Forest projection/publication/camera code with real forest modules."""
from pathlib import Path
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]


class ForestInlinePreview(unittest.TestCase):
    def test_preview_is_separate_and_edits_undo_restore_camera(self):
        if os.name!='nt':self.skipTest('Installed MSVC module consumer lane')
        production=(ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(encoding='utf-8')
        workspace=(ROOT/'Engine/src/editor/application/editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
        functions='\n'.join(definition(production, marker) for marker in (
            '[[nodiscard]] bool ensure_plant_lab_compiled_asset(',
            '[[nodiscard]] std::vector<EditorEntity>\n            project_compiled_forest_entities(',
            '[[nodiscard]] bool ensure_forest_factory_preview(',
            'void focus_forest_factory_preview(',
            'void publish_editor_preview_markers('))
        camera=workspace[workspace.index('const bool changedSurface ='):workspace.index('            if (changedSurface)\n                editor.surfaceSettleFrames')]
        code=r'''
#include "core.format_text.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
import editor.application;
import forest.factory;
namespace epochengine {
using EditorMainSurface=EditorApplicationSurface;
namespace core {enum class ScenePreviewMode {None,Editor};struct Context {};}
namespace render_arcade {struct Node{std::string_view name;};constexpr Node kScreenSceneNode{"screen"};}
namespace previewgrid {
 struct Vec3 {float x{},y{},z{};};
 struct ObjectMarker {Vec3 position,color,scale,rotationDegrees;float radius{};int primitive{};bool selected{},editorOnly{},sampledRenderSurface{};};
 struct CameraRigSnapshot {bool valid{true};int value{};};
 int cameraValue=5;std::vector<ObjectMarker> observed;
 CameraRigSnapshot capture_camera_rig_snapshot(const core::Context*){return {true,cameraValue};}
 bool restore_camera_rig_snapshot(const core::Context*,CameraRigSnapshot value){cameraValue=value.value;return true;}
 bool focus_camera(const core::Context*,Vec3,float){cameraValue=42;return true;}
 void clear_object_markers(const core::Context*){observed.clear();}
 void clear_lighting_frame(const core::Context*){}
 void set_lighting_frame(const core::Context*,int){}
 void set_object_markers(const core::Context*,std::vector<ObjectMarker> value){observed=std::move(value);}
}
struct EditorEntity {
 std::string name,type,category;std::array<float,3> position{},rotation{},scale{1,1,1};
 bool visible{true},editorOnly{};int sceneObjectId{};
};
EditorEntity editor_entity_from_seed(const EditorSceneSeedEntity& seed){return {seed.name,seed.type,seed.category,seed.position,seed.rotation,seed.scale,seed.visible,seed.editor_only};}
struct Oscillator {bool enabled{};int entity_id{},axis{};float frequency_hz{},phase_radians{},amplitude{};std::array<float,3> base_position{};};
struct Lighting {int build_frame(){return 0;}};
struct EditorState {
 EditorMainSurface mainSurface{EditorMainSurface::Scene};core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};
 forest::ForestAssetDocument plantLabDocument;forest::CompiledForestAsset plantLabCompiled;
 bool plantLabDocumentInitialized{};std::uint64_t plantLabCompiledRevision{},plantLabCompiledContentHash{};
 std::string plantLabStatus;std::vector<EditorEntity> entities,forestPreviewEntities;
 std::uint64_t forestPreviewRevision{},forestPreviewContentHash{};
 std::optional<previewgrid::CameraRigSnapshot> forestProjectCamera;
 Lighting sceneLighting;std::map<int,Oscillator> scriptCubeOscillators;Oscillator scriptOscillator;
 struct {double simulated_seconds{};} timeSnapshot;
};
bool reopen_plant_lab_project_asset(EditorState&,bool){return false;}
bool align_editor_entity_group_to_surface(std::vector<EditorEntity>& values,float){return !values.empty();}
void rebuild_editor_lighting(EditorState&){}
bool is_screen_space_gui_entity(const EditorEntity&){return false;}
bool editor_entity_selected(EditorState&,int id){return id==999;}
previewgrid::Vec3 selection_color_for_entity(const EditorEntity&,bool){return {1,1,1};}
float marker_radius_for_entity(const EditorEntity&){return 1;}
int preview_primitive_for_entity(const EditorEntity&){return 0;}
bool marker_selected_for_entity(const EditorEntity&,bool selected){return selected;}
bool is_plant_lab_preview_entity(const EditorEntity& entity){return entity.category=="PlantLabPreview";}
FUNCTIONS
void change_surface(EditorState& editor,EditorMainSurface surface,const std::shared_ptr<core::Context>& ctx){
 CAMERA
 if(changedSurface&&surface==EditorMainSurface::ForestFactory)focus_forest_factory_preview(ctx.get(),editor);
}
}
int main(){
 using namespace epochengine;
 auto ctx=std::make_shared<core::Context>();EditorState state;
 state.entities.push_back({"OriginalCube","Cube","World",{50,0,0},{},{1,1,1},true,false,999});
 const auto original=state.entities.front();
 change_surface(state,EditorMainSurface::ForestFactory,ctx);
 assert(state.forestProjectCamera&&previewgrid::cameraValue==42);
 publish_editor_preview_markers(ctx.get(),state);
 assert(!state.forestPreviewEntities.empty()&&previewgrid::observed.size()==state.forestPreviewEntities.size());
 assert(state.entities.size()==1&&state.entities.front().name==original.name&&state.entities.front().position==original.position);
 for(const auto& marker:previewgrid::observed)assert(marker.position.x!=50&&!marker.selected&&marker.editorOnly);
 const auto firstHash=state.forestPreviewContentHash;
 const auto firstHeight=state.plantLabDocument.profile.config.targetHeightMeters;
 assert(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,firstHeight+0.5f));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.forestPreviewContentHash!=firstHash&&state.plantLabCompiled.sourceContentHash==state.forestPreviewContentHash);
 assert(state.entities.size()==1&&state.entities.front().position==original.position);
 assert(forest::undo_forest_profile_edit(state.plantLabDocument));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.plantLabDocument.profile.config.targetHeightMeters==firstHeight);
 assert(forest::redo_forest_profile_edit(state.plantLabDocument));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.plantLabDocument.profile.config.targetHeightMeters==firstHeight+0.5f);
 change_surface(state,EditorMainSurface::Scene,ctx);
 publish_editor_preview_markers(ctx.get(),state);
 assert(!state.forestProjectCamera&&previewgrid::cameraValue==5);
 assert(previewgrid::observed.size()==1&&previewgrid::observed.front().position.x==50&&previewgrid::observed.front().selected);
}
'''.replace('FUNCTIONS',functions).replace('CAMERA',camera)
        proof=ROOT/'build/validation/forest-inline-preview';proof.mkdir(parents=True,exist_ok=True)
        (proof/'consumer.cpp').write_text(code,encoding='utf-8')
        engine=(ROOT/'Engine').as_posix()
        names=['editor.application','forest.factory','authoring.morphology','authoring.document','voxel.field','package.registry']
        modules='\n'.join(f'"{engine}/modules/{name}.ixx"' for name in names)
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EpochForestInlineProof LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(forest_inline consumer.cpp)
target_sources(forest_inline PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{engine}/modules" FILES
{modules})
target_include_directories(forest_inline PRIVATE "{engine}/include")
target_compile_options(forest_inline PRIVATE /W3 /WX /utf-8)
enable_testing()
add_test(NAME actual_forest_inline_preview COMMAND forest_inline)
''',encoding='utf-8')
        for index,command in enumerate((
            ['cmake','-S',str(proof),'-B',str(proof/'msvc'),'-G','Visual Studio 17 2022','-A','x64'],
            ['cmake','--build',str(proof/'msvc'),'--config','Debug','--parallel','1'],
            ['ctest','--test-dir',str(proof/'msvc'),'-C','Debug','--output-on-failure'])):
            checked=subprocess.run(command,cwd=proof,capture_output=True,text=True,timeout=120)
            (proof/f'lane-{index}.log').write_text(checked.stdout+checked.stderr,encoding='utf-8')
            self.assertEqual(checked.returncode,0,checked.stdout[-7000:]+checked.stderr)


if __name__=='__main__':unittest.main()
