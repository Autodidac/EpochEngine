"""Persistent ambient is a mandatory scene setting, not a deletable light entity."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest
import time

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "Engine/src/editor/application"


def fixture():
    transactions = (APP / "editor.application_scene_transactions.inl").read_text(encoding="utf-8")
    preview = (APP / "editor.application_scene_preview.inl").read_text(encoding="utf-8")
    state = (APP / "editor.application_state.inl").read_text(encoding="utf-8")
    runtime = (ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp").read_text(encoding="utf-8")
    entity = definition(state, "        struct EditorEntity", True)
    functions = "\n".join(definition(transactions, marker) for marker in (
        "        [[nodiscard]] bool project_scene_document_into_editor(",
        "        [[nodiscard]] bool rebuild_editor_scene_document_from_snapshot(",
        "        [[nodiscard]] bool apply_editor_scene_transaction(",
        "        [[nodiscard]] std::optional<scene::Vec3> parse_editor_ambient_channels(",
        "        [[nodiscard]] bool set_editor_scene_ambient(",
        "        [[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(",
        "        [[nodiscard]] bool create_editor_scene_entity("))
    functions += "\n" + definition(preview, "        [[nodiscard]] epochengine::lighting::Vec3 light_direction_from_editor_rotation(")
    functions += "\n" + definition(preview, "        void rebuild_editor_lighting(")
    functions += "\n" + definition(runtime, "        [[nodiscard]] epochengine::lighting::LightingFrame build_project_play_lighting(")
    functions += "\n" + definition((APP / "editor.application_document_editing.inl").read_text(encoding="utf-8"),
        "        [[nodiscard]] editor_workspace_commands::Surface workspace_command_surface(")
    functions += "\n" + definition((APP / "editor.application_runtime_source.inl").read_text(encoding="utf-8"),
        "        [[nodiscard]] bool load_editor_scene_snapshot(")
    project = (APP / "editor.application_project_documents.inl").read_text(encoding="utf-8")
    restore = project.index("            const bool loadedScene = load_editor_scene_snapshot(state);")
    functions += "\nvoid restore_selected_scene(EditorState& state){\n" + project[
        restore:project.index("            synchronize_editor_selection(state);", restore)] + "\n}\n"
    return r'''
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
import scene.snapshot;
import scene.serializer;
import scene.persistence;
import scene.document;
import scene.runtime;
import render.lighting;
import editor.application;
import editor.workspace_commands;
import project.lifecycle;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Ambient contract failed line %d\n",line);std::exit(83);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
namespace epochengine{
namespace asset::mesh{struct Artifact;}
namespace previewgrid{struct MeshBaseColorBinding;}
namespace project_planetary{using GeneratedMeshFrame=scene::SceneMeshCoordinates;}
template<class...Args>std::string format_text(std::string_view text,Args&&...){return std::string{text};}
ENTITY
struct EditorState{
 authoring::scene::SceneDocument sceneDocument;lighting::LightManager sceneLighting{128};
 std::vector<lighting::LightHandle>sceneLightHandles;std::vector<EditorEntity>entities;
 bool sceneLightingDirty=true,sceneInteractionDirty=false;std::string projectStatus;
 scene::SceneObjectId selectedEntityId{};std::uint64_t sceneDocumentRevision{};
 std::filesystem::path scenePath;std::size_t selectedEntity{};unsigned seedRebuilds{};
 project_lifecycle::EvidenceLedger projectLifecycle;
 struct{std::uint64_t frame_index{};double simulated_seconds{};}timeSnapshot;
 EditorApplicationSurface mainSurface=EditorApplicationSurface::Scene;unsigned assetWorkspaceSection{};bool game2DGuiWorkspace{};std::string projectId;
 struct{struct{struct Snapshot{bool configured{};};Snapshot snapshot()const{return {};}}controller;}projectCodeWorkspace;
};
using EditorMainSurface=EditorApplicationSurface;
void synchronize_editor_selection(EditorState&){}void push_editor_log(EditorState&,std::string_view){}
std::filesystem::path active_editor_scene_path(const EditorState& state){return state.scenePath;}
void normalize_editor_entity_ids(EditorState&){}
void select_editor_entity(EditorState&,std::size_t){}
bool rebuild_editor_scene_document(EditorState& state,std::string_view reason){
 if(reason!="Load project scene seed")return false;
 ++state.seedRebuilds;return true;
}
scene::SceneObjectId allocate_editor_scene_object_id(EditorState&,std::string_view){CHECK(false);return 0;}
using ProjectRuntimeEntity=scene_runtime::RuntimeEntity;
FUNCTIONS
}
using namespace epochengine;
namespace document=epochengine::authoring::scene;
bool same(lighting::Color3 a,const scene::Vec3& b){return a.r==b[0]&&a.g==b[1]&&a.b==b[2];}
std::string bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
int main(int argc,char**argv){
 CHECK(argc==2);scene::SceneSnapshot empty;empty.scene_id="ambient-test";empty.project_id="project-test";empty.world_name="Ambient Test";
 CHECK(empty.ambient==scene::kDefaultAmbientLight&&empty.objects.empty());
 const std::string legacy=scene::serialize_snapshot_text(empty);
 const std::string golden="epoch_snapshot 4\nscene \"ambient-test\"\nproject \"project-test\"\nworld \"Ambient Test\"\ndocument_kind \"game\"\nsupport_tier \"baseline\"\nrevision 1\nprimary_camera 0\nprimary_spawn 0\nframe 0\ntime 0\npackages 0\nobjects 0\ntimeline_keys 0\n";
 CHECK(legacy==golden);
 for(char version:{'2','3','4'}){auto old=legacy;old[15]=version;const auto parsed=scene::parse_snapshot_text(old);CHECK(parsed.ok&&parsed.snapshot.ambient==scene::kDefaultAmbientLight);}
 const auto v1=scene::parse_snapshot_text("epoch_snapshot 1\nscene \"old\"\nworld \"legacy\"\nframe 0\ntime 0\nobjects 0\ntimeline_keys 0\n");
 CHECK(v1.ok&&v1.snapshot.ambient==scene::kDefaultAmbientLight);
 auto built=document::SceneDocument::from_snapshot(empty);CHECK(built);
 EditorState editor;editor.sceneDocument=std::move(built.value);CHECK(project_scene_document_into_editor(editor));
 const auto originalHash=editor.sceneDocument.revision().content;
 const auto originalHistory=editor.sceneDocument.history().size();
 CHECK(set_editor_scene_ambient(editor,scene::kDefaultAmbientLight)&&editor.sceneDocument.history().size()==originalHistory);
 const scene::Vec3 edited{1.25f,0.5f,0.125f};
 CHECK(parse_editor_ambient_channels({"1.25","0.5","0.125"})==edited);
 for(auto invalid:std::array<std::array<std::string,3>,5>{{{"-1","0","0"},{"nan","0","0"},{"inf","0","0"},{"1garbage","0","0"},{"1e999","0","0"}}})CHECK(!parse_editor_ambient_channels(invalid));
 CHECK(set_editor_scene_ambient(editor,edited));
 CHECK(editor.entities.empty()&&editor.sceneDocument.metrics().active_objects==0&&editor.sceneDocument.history().size()==originalHistory+1);
 CHECK(editor.sceneDocument.revision().content!=originalHash&&editor.sceneLightingDirty);
 const auto beforeLighting=editor.sceneLighting.build_frame();
 rebuild_editor_lighting(editor);CHECK(same(editor.sceneLighting.build_frame().environment.ambient,edited)&&editor.sceneLightHandles.empty());
 CHECK(lighting::diff_lighting_frames(&beforeLighting,editor.sceneLighting.build_frame()).global_invalidation);
 CHECK(editor.sceneDocument.undo());CHECK(project_scene_document_into_editor(editor));rebuild_editor_lighting(editor);
 CHECK(editor.sceneDocument.revision().content==originalHash&&same(editor.sceneLighting.build_frame().environment.ambient,scene::kDefaultAmbientLight));
 CHECK(editor.sceneDocument.redo());CHECK(project_scene_document_into_editor(editor));rebuild_editor_lighting(editor);
 CHECK(same(editor.sceneLighting.build_frame().environment.ambient,edited));
 const auto projection=editor.sceneDocument.project_snapshot();CHECK(projection&&projection.snapshot.ambient==edited);
 const std::string saved=scene::serialize_snapshot_text(projection.snapshot);CHECK(saved.starts_with("epoch_snapshot 5\n")&&saved.find("ambient ")!=std::string::npos);
 const auto parsed=scene::parse_snapshot_text(saved);CHECK(parsed.ok&&parsed.snapshot.ambient==edited);
 auto reopenedDocument=document::SceneDocument::from_snapshot(parsed.snapshot);
 CHECK(reopenedDocument&&reopenedDocument.value.project_settings().ambient==edited&&reopenedDocument.value.objects().empty());
 for(float invalid:{-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  const auto before=editor.sceneDocument.revision();auto bad=edited;bad[1]=invalid;
  CHECK(!set_editor_scene_ambient(editor,bad)&&editor.sceneDocument.revision().sequence==before.sequence);
  auto snapshot=projection.snapshot;snapshot.ambient=bad;CHECK(!scene::validate_scene_document(snapshot)&&scene::serialize_snapshot_text(snapshot).empty()&&!document::SceneDocument::from_snapshot(snapshot));
  auto settings=editor.sceneDocument.project_settings();settings.ambient=bad;const std::array<document::SceneOperation,1> operations{document::ProjectSettingsChangedOperation{settings}};
  CHECK(!editor.sceneDocument.apply_transaction(operations,"Invalid ambient")&&editor.sceneDocument.project_settings().ambient==edited);
 }
 auto bad=saved;const auto ambientStart=bad.find("ambient ");bad.erase(ambientStart,bad.find('\n',ambientStart)-ambientStart+1);CHECK(!scene::parse_snapshot_text(bad).ok);
 bad=saved;bad+="ambient 0,0,0\n";CHECK(!scene::parse_snapshot_text(bad).ok);
 for(std::string_view invalid:{"-1","nan","inf","1e999"}){
  bad=saved;bad.replace(bad.find("ambient "),bad.find('\n',bad.find("ambient "))-bad.find("ambient "),"ambient "+std::string{invalid}+",0,0");CHECK(!scene::parse_snapshot_text(bad).ok);
 }
 // Persistence owns the same canonical meaning and refuses to overwrite it with invalid data.
 const std::filesystem::path path=argv[1];std::filesystem::create_directories(path.parent_path());
 CHECK(scene::persistence::save_scene_snapshot_atomic(path,projection.snapshot));
 const auto loaded=scene::persistence::load_scene_snapshot(path);CHECK(loaded.result&&loaded.snapshot.ambient==edited);
 CHECK(scene::persistence::scene_snapshot_semantically_equal(projection.snapshot,loaded.snapshot));
 auto different=loaded.snapshot;different.ambient=scene::kDefaultAmbientLight;CHECK(!scene::persistence::scene_snapshot_semantically_equal(projection.snapshot,different));
 const auto beforeBytes=bytes(path);different.ambient[0]=-1;CHECK(!scene::persistence::save_scene_snapshot_atomic(path,different)&&bytes(path)==beforeBytes);
 // Fresh Editor state must restore a committed disk revision. Reopening the
 // same ID with a damaged file must retire the earlier commit evidence.
 EditorState reopened;reopened.projectId=loaded.snapshot.project_id;reopened.scenePath=path;
 CHECK(reopened.projectLifecycle.select_project(1));restore_selected_scene(reopened);
 CHECK(reopened.projectLifecycle.evidence().scene_committed&&reopened.seedRebuilds==0);
 CHECK(reopened.sceneDocumentRevision==loaded.snapshot.revision&&reopened.sceneDocument.project_settings().ambient==edited);
 CHECK(reopened.projectLifecycle.evidence().generations.committed_scene_generation==loaded.snapshot.revision);
 CHECK(reopened.projectLifecycle.record_shell_materialized()&&reopened.projectLifecycle.observe_build_inputs(2));
 const auto damaged=path.parent_path()/"damaged.epoch";
 {std::ofstream out(damaged,std::ios::binary);out<<"damaged scene";CHECK(out);}
 reopened.scenePath=damaged;
 const auto lastRevision=reopened.sceneDocument.revision().content;
 CHECK(!load_editor_scene_snapshot(reopened)&&reopened.sceneDocument.revision().content==lastRevision);
 CHECK(reopened.projectLifecycle.select_project(1));restore_selected_scene(reopened);
 const auto& invalid=reopened.projectLifecycle.evidence();
 CHECK(!invalid.scene_committed&&invalid.generations.committed_scene_generation==0&&!invalid.build_inputs_current);
 CHECK(reopened.seedRebuilds==1&&reopened.projectStatus.find("Showing the project seed in memory")!=std::string::npos);
 CHECK(bytes(damaged)=="damaged scene"&&bytes(path)==beforeBytes);
 // Deleting every ordinary light leaves the mandatory ambient scene setting untouched.
 auto tier0=document::SceneDocument::make_default_tier0();CHECK(tier0);auto settings=tier0.value.project_settings();settings.ambient=edited;
 const std::array<document::SceneOperation,1> environment{document::ProjectSettingsChangedOperation{settings}};CHECK(tier0.value.apply_transaction(environment,"Ambient"));
 std::vector<document::SceneOperation> removals;for(const auto& object:tier0.value.objects())if(object.descriptor.light)removals.emplace_back(document::ObjectDestroyedOperation{object.handle});
 CHECK(!removals.empty()&&tier0.value.apply_transaction(removals,"Delete every light"));
 const auto runtimeSource=tier0.value.project_snapshot();CHECK(runtimeSource&&runtimeSource.snapshot.ambient==edited);
 // Tier0's camera/spawn are preview-only by default. The runtime fixture makes
 // those exact existing objects visible/run-eligible; ambient stays unchanged.
 auto runnable=runtimeSource.snapshot;
 for(auto& object:runnable.objects)if(object.type=="Camera"||object.type=="Spawn"){object.visible=true;object.editor_only=false;}
 scene_runtime::SceneRuntime runtime;CHECK(runtime.replace(runnable).committed());CHECK(runtime.projection()->source().ambient==edited);
 const auto runtimeFrame=build_project_play_lighting(runtime.projection()->entities(),runtime.projection()->source().ambient);
 CHECK(runtimeFrame.lights.empty()&&same(runtimeFrame.environment.ambient,edited));
 EditorState fallback;rebuild_editor_lighting(fallback);CHECK(same(fallback.sceneLighting.build_frame().environment.ambient,scene::kDefaultAmbientLight));
 // Toolbar/menu availability is exercised through the actual command popup
 // in Tests.gui.test_outliner_context_menu; canonical creation remains here.
 // The actual scene creation transaction needs no project and inserts nothing
 // until explicitly requested. This fixture supplies an already-admitted ID.
 CHECK(editor.entities.empty());CHECK(create_editor_scene_entity(editor,EditorEntity{.sceneObjectId=99,.name="ScratchCube",.type="StaticMesh",.category="Gameplay"},"Create scratch cube"));
 CHECK(editor.projectId.empty()&&editor.entities.size()==1&&editor.sceneDocument.project_settings().ambient==edited);
 const auto beforeWater=editor.sceneDocument.revision().content;
 CHECK(create_editor_scene_entity(editor,EditorEntity{.sceneObjectId=100,.name="WaterSurface",.type="Water",.category="World",.position={1,2,3}},"Create water"));
 CHECK(editor.sceneDocument.undo());CHECK(editor.sceneDocument.revision().content==beforeWater);
 CHECK(project_scene_document_into_editor(editor));CHECK(editor.entities.size()==1);
 CHECK(editor.sceneDocument.redo());CHECK(project_scene_document_into_editor(editor));CHECK(editor.entities.size()==2);
 const auto waterProjection=editor.sceneDocument.project_snapshot();CHECK(waterProjection);
 CHECK(scene::persistence::save_scene_snapshot_atomic(path,waterProjection.snapshot));
 const auto waterReload=scene::persistence::load_scene_snapshot(path);CHECK(waterReload.result);
 const auto* water=scene::find_object_by_id(waterReload.snapshot,100);
 CHECK(water&&water->type=="Water"&&water->position==scene::Vec3({1,2,3}));
 CHECK(waterReload.snapshot.ambient==edited);
 CHECK(editor.sceneDocument.undo());CHECK(project_scene_document_into_editor(editor)&&editor.entities.size()==1);
 CHECK(editor.sceneDocument.undo());CHECK(project_scene_document_into_editor(editor)&&editor.entities.empty()&&editor.sceneDocument.project_settings().ambient==edited);
}
'''.replace("ENTITY", entity).replace("FUNCTIONS", functions)


class SceneAmbient(unittest.TestCase):
    def test_actual_persistent_ambient_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        proof = ROOT / "Artifacts/Reports/scene-ambient-outliner-oct29" / str(time.time_ns())
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "scene.ambient_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        module_map = {}
        roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        for root in roots:
            for path in root.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    module_map[match.group(1)] = path
        pending = ["scene.snapshot", "scene.serializer", "scene.persistence", "scene.document", "scene.runtime", "render.lighting", "editor.application", "editor.workspace_commands", "project.lifecycle"]
        selected = {}
        while pending:
            name = pending.pop()
            if name in selected or name in ("std", "std.compat"):
                continue
            self.assertIn(name, module_map)
            selected[name] = module_map[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        modules = " ".join('"' + p.as_posix() + '"' for p in selected.values())
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(SceneAmbient LANGUAGES CXX)
add_executable(scene_ambient "{source.as_posix()}")
target_compile_features(scene_ambient PRIVATE cxx_std_23)
target_compile_options(scene_ambient PRIVATE /W3 /WX /utf-8)
target_include_directories(scene_ambient PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(scene_ambient PRIVATE FILE_SET CXX_MODULES BASE_DIRS {" ".join('"'+r.as_posix()+'"' for r in roots)} FILES {modules})
enable_testing()
add_test(NAME actual_scene_ambient COMMAND scene_ambient "{(proof/'fixture/scene.epoch').as_posix()}")
set_tests_properties(actual_scene_ambient PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(proof / "msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(proof / "msvc"), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(proof / "msvc"), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual scene modules/history/codec/filesystem persistence + Editor transaction/projection/lighting/create and ProjectPlay lighting owners; recording selection/log/format ports; explicit admitted scratch ID",
            "passed": True, "nativeAcceptance": False, "modules": len(selected),
            "sourceIdentity": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in selected.values()},
            "commands": results,
            "cases": ["default v4 exact golden and v1-v4 compatibility", "v5 edited HDR roundtrip", "semantic Undo/Redo/hash/no-op",
                "no entity injection", "save/reopen and failed-save preservation", "actual editor disk reopen and stale evidence invalidation", "invalid numeric/parser/schema values",
                "delete every ordinary light preserves ambient", "actual Editor and runtime lighting consume canonical ambient",
                "actual explicit scratch creation/Undo retains ambient"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
