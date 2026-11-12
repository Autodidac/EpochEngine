"""Run the real painted-tree refresh against real scene history and serialization."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class ForestAnchorRefresh(unittest.TestCase):
    def test_verified_roots_reconcile_without_changing_authored_offsets(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC module consumer lane")
        owner = ROOT / "Engine/src/editor/application/editor.application_scene_preview.inl"
        production = owner.read_text(encoding="utf-8")
        transactions = (ROOT / "Engine/src/editor/application/editor.application_scene_transactions.inl").read_text(encoding="utf-8").replace("std::string_view selected_name = {}", "std::string_view selected_name")
        functions = "\n".join(definition(transactions, marker) for marker in (
            "[[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(",
            "void append_editor_descriptor_operations(",
            "[[nodiscard]] bool synchronize_editor_scene_category("))
        helper_marker = "[[nodiscard]] std::optional<std::array<double,3>> verified_forest_surface_anchor("
        if helper_marker in production:
            functions += "\n" + definition(production, helper_marker)
        for marker in ("[[nodiscard]] bool painted_tree_uses_plant_lab(",
                       "struct PaintedTreeUpdateResult final",
                       "[[nodiscard]] PaintedTreeUpdateResult refresh_forest_factory_instances("):
            functions += "\n" + definition(production, marker, marker.startswith("struct "))
        preview = (ROOT / "Engine/modules/render.preview_grid.ixx").read_text(encoding="utf-8")
        camera = (ROOT / "Engine/src/render/render.preview_camera.cpp").read_text(encoding="utf-8")
        precise = definition(preview, "    export struct PreciseMeshTransform final", True).replace("export ", "")
        precise += "\n" + definition(camera, "    [[nodiscard]] bool valid_precise_mesh_transform(")
        precise += "\n" + definition(camera, "    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(")
        planetary = (ROOT / "Engine/modules/project.planetary.ixx").read_text(encoding="utf-8")
        types = "\n".join(definition(planetary, "    struct " + name + " final", True)
                          for name in ("Settings", "GeneratedMeshFrame", "MeshGenerationIdentity", "GeneratedBody", "SourceGeneration"))
        code = CODE.replace("ACTUAL_PLANETARY_TYPES", types).replace("ACTUAL_PRECISE", precise).replace("ACTUAL_FUNCTIONS", functions).replace("assert(", "CHECK(")
        proof = ROOT / "Artifacts/Reports/forest-parent-anchor-nov06/refresh-contract"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        module_root = ROOT / "Engine/modules"
        found = {p.stem: p for p in module_root.glob("*.ixx")}
        pending, selected = ["scene.document", "scene.serializer"], {}
        while pending:
            name = pending.pop()
            if name in selected or name in ("std", "std.compat"):
                continue
            self.assertIn(name, found)
            selected[name] = found[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", found[name].read_text(encoding="utf-8-sig"), re.M)
        paths = lambda values: " ".join('"' + p.as_posix() + '"' for p in values)
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ForestAnchorRefresh LANGUAGES CXX)
add_executable(refresh consumer.cpp)
target_compile_features(refresh PRIVATE cxx_std_23)
target_compile_options(refresh PRIVATE /W3 /WX /utf-8 /UNDEBUG)
target_include_directories(refresh PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(refresh PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{module_root.as_posix()}" FILES {paths(selected.values())})
enable_testing()
add_test(NAME actual_forest_anchor_refresh COMMAND refresh)
''', encoding="utf-8")
        build = proof / "cmake"
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            (proof / f"lane-{index}.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exit_code": completed.returncode})
            (proof / "result.json").write_text(json.dumps({"pass": False, "runs": results}, indent=2) + "\n")
            self.assertEqual(completed.returncode, 0, completed.stdout[-10000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({"pass": True, "runs": results,
            "owner_sha256": hashlib.sha256(owner.read_bytes()).hexdigest(),
            "scope": "Actual refresh, descriptor diff, category synchronization, scene transaction/history and codec. Provider currency/persistence boundaries are controlled CPU ports; no native rendering or model."}, indent=2) + "\n")


CODE = r'''
#include "core.format_text.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
import scene.snapshot;
import scene.document;
import scene.serializer;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Forest refresh contract failed line %d\n",line);std::exit(86);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
namespace epochengine {
namespace project_planetary {
ACTUAL_PLANETARY_TYPES
unsigned source_checks{},mesh_checks{};bool source_current=true,mesh_current=true;
std::string encode_settings(const Settings& value){return "verified-recipe-"+std::to_string(value.seed);}
bool source_generation_is_current(const std::filesystem::path&,const SourceGeneration&){++source_checks;return source_current;}
bool mesh_generation_is_current(const std::filesystem::path&,const SourceGeneration&,const Settings&,const MeshGenerationIdentity&){++mesh_checks;return mesh_current;}
}
namespace previewgrid {struct Vec3{float x{},y{},z{};};ACTUAL_PRECISE}
struct EditorEntity {
 std::uint64_t sceneObjectId{};std::string name,type,category;
 std::array<float,3>position{},rotation{},scale{1,1,1};bool visible=true,editorOnly{};
 std::optional<scene::SceneTextureMaterialSnapshot>textureMaterial;
 std::optional<scene::SceneMeshSnapshot>meshAsset;
 std::optional<std::array<double,3>>positionAnchor;
 std::optional<scene::SceneForestRegionInstance>forestRegion;
 std::shared_ptr<const int>meshSnapshot;bool meshLoadAttempted{};
};
struct EditorState {
 std::string projectRoot="fixture-project",projectStatus,plantLabStatus;
 std::vector<EditorEntity>entities;std::uint64_t selectedEntityId{};
 std::optional<int>forestSurfacePending,projectBuildPending;
 struct {bool running{};bool valid()const{return running;}}projectRuntimeProcess;
 struct {std::uint64_t sourceRevision=42,sourceContentHash=84;struct{std::uint64_t stableHash=126;}genome;}plantLabCompiled;
 struct PlanetaryMeshPolicy final {std::string selected_root;project_planetary::SourceGeneration source;
  project_planetary::Settings settings;std::optional<project_planetary::MeshGenerationIdentity>generated_mesh;
  std::vector<project_planetary::GeneratedBody>generated_bodies;bool body_inventory_checked{};};
 std::optional<PlanetaryMeshPolicy>planetaryMeshPolicy;
 authoring::scene::SceneDocument sceneDocument;
};
using Document=authoring::scene::SceneDocument;
unsigned saves{},commits{},projections{};bool saving=true;
constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
bool has_open_project(const EditorState& state){return !state.projectRoot.empty();}
bool ensure_plant_lab_compiled_asset(EditorState&){return true;}
void push_editor_log(EditorState&,std::string_view){}
std::filesystem::path resolve_editor_path(const std::filesystem::path& path){return path;}
std::optional<std::size_t>editor_entity_index(const EditorState& state,std::uint64_t id){
 for(std::size_t i=0;i<state.entities.size();++i)if(state.entities[i].sceneObjectId==id)return i;return {};}
bool editor_planetary_stream_identity_matches(const EditorState& state,const EditorEntity& terrain){
 const auto& p=state.planetaryMeshPolicy;
 return p&&p->selected_root==state.projectRoot&&p->generated_mesh&&terrain.meshAsset
  &&p->generated_mesh->artifact_key==terrain.meshAsset->artifact_key;}
std::vector<EditorEntity>project_compiled_forest_entities(const auto&,std::string_view,std::string_view,bool,float,float,bool){
 EditorEntity entity;entity.meshAsset=scene::SceneMeshSnapshot{"Assets/Mesh/refreshed.epoch_mesh",{7,8,9,10},3,3};
 entity.meshSnapshot=std::make_shared<const int>(5);return {entity};}
bool persist_forest_scene_mesh(EditorState&,std::vector<EditorEntity>&){return true;}
bool publish_plant_lab_project_asset(EditorState&){return true;}
bool save_editor_scene_snapshot(EditorState&){++saves;return saving;}
std::string editor_scene_identity(const EditorState&){return "forest-refresh";}
bool rebuild_editor_scene_document(EditorState&,std::string_view){assert(false);return false;}
void synchronize_editor_selection(EditorState&){}
void mirror(EditorState& state){
 const auto projected=state.sceneDocument.project_snapshot();assert(projected);++projections;state.entities.clear();
 for(const auto& value:projected.snapshot.objects){EditorEntity e;e.sceneObjectId=value.id;e.name=value.name;e.type=value.type;e.category=value.category;
  e.position=value.position;e.rotation=value.rotation;e.scale=value.scale;e.visible=value.visible;e.editorOnly=value.editor_only;
  e.textureMaterial=value.texture_material;e.meshAsset=value.mesh_asset;e.positionAnchor=value.position_anchor;e.forestRegion=value.forest_region;state.entities.push_back(e);}}
bool apply_editor_scene_transaction(EditorState& state,std::span<const authoring::scene::SceneOperation> operations,
 std::string_view label,std::optional<scene::SceneObjectId> selected={}){
 const auto result=state.sceneDocument.apply_transaction(operations,label);if(!result)return false;++commits;mirror(state);
 if(selected)state.selectedEntityId=*selected;return true;}
ACTUAL_FUNCTIONS
EditorState fixture(){
 EditorState state;scene::SceneSnapshot source;source.scene_id="forest-refresh";source.project_id="fixture-project";
 scene::SceneObjectSnapshot parent;parent.id=3;parent.name="Planet";parent.type="IndexedMesh";parent.category="Planetary Terrain";
 parent.position={4875653.5f,0,52489.25f};parent.mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/planet.epoch_mesh",{1,2,3,4},3,3,
  scene::SceneMeshCoordinates{637100,{1e9,2e9,3e9}}};source.objects.push_back(parent);
 EditorState::PlanetaryMeshPolicy p;p.selected_root=state.projectRoot;p.source={state.projectRoot,"manifest","sha256:"+std::string(64,'a')};
 p.generated_mesh=project_planetary::MeshGenerationIdentity{{1,2,3,4},3,3,{637100,{1e9,2e9,3e9}}};
 p.generated_bodies.push_back({.id=0,.center_meters={1e9,2e9,3e9},.radius_meters=6371000});p.body_inventory_checked=true;
 state.planetaryMeshPolicy=p;
 for(unsigned i=0;i<3;++i){scene::SceneObjectSnapshot tree;tree.id=10+i;tree.name="Tree"+std::to_string(i);tree.type="ForestTrunk";tree.category="ForestFactory";
  tree.position={.25f,-.5f,.75f};tree.rotation={3,7,11};tree.scale={.5f,.75f,1.25f};tree.position_anchor=std::array<double,3>{0,6371000,0};
  tree.mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/old.epoch_mesh",{11,12,13,14},3,3};
  scene::SceneForestRegionInstance r;r.region_id=100+i;r.terrain_object_id=3;r.plant_genome=1;r.plant_revision=1;r.plant_hash=1;
  r.candidate_count=1;r.radius_meters=12;r.density_per_square_meter=.01;r.body_radius_meters=6371000;r.center_direction={0,1,0};
  r.body_local_root_meters={0,6371000,0};r.admitted_body_center_meters={1e9,2e9,3e9};r.mesh_source_origin_meters={1e9,2e9,3e9};
  r.terrain_artifact_key={1,2,3,4};r.provider_content_hash=std::string(64,i==1?'b':'a');r.recipe=project_planetary::encode_settings(p.settings);
  r.plant_source=std::string{kDefaultPlantLabLogicalPath};assert(r.valid());tree.forest_region=r;
  if(i==2)tree.position_anchor=std::array<double,3>{4875653.5,6371000,52489.25};source.objects.push_back(tree);}
 auto built=Document::from_snapshot(source);assert(built);state.sceneDocument=std::move(built.value);state.selectedEntityId=10;mirror(state);return state;
}
EditorEntity tree(const EditorState& state,std::uint64_t id){const auto index=editor_entity_index(state,id);assert(index);return state.entities[*index];}
void no_mutation(const EditorState& before,const EditorState& after){assert(scene::serialize_snapshot_text(before.sceneDocument.project_snapshot().snapshot)
 ==scene::serialize_snapshot_text(after.sceneDocument.project_snapshot().snapshot));}
}
int main(){using namespace epochengine;
 auto state=fixture();const auto before=state.sceneDocument.project_snapshot();assert(before);
 const auto original=tree(state,10),stale=tree(state,11),correct=tree(state,12);const auto hash=state.sceneDocument.revision().content;
 assert(refresh_forest_factory_instances(state));const auto updated=tree(state,10);
 assert(updated.positionAnchor==correct.positionAnchor); // baseline fails: stale world pivot retained
 assert(updated.position==original.position&&updated.rotation==original.rotation&&updated.scale==original.scale&&updated.sceneObjectId==original.sceneObjectId);
 assert(tree(state,11).positionAnchor==stale.positionAnchor&&tree(state,11).forestRegion->provider_content_hash==std::string(64,'b'));
 assert(tree(state,12).positionAnchor==correct.positionAnchor&&state.entities.size()==4&&state.selectedEntityId==10);
 assert(state.plantLabStatus.find("1 tree root")!=std::string::npos&&state.plantLabStatus.find("1 root retained")!=std::string::npos);
 assert(project_planetary::source_checks==1&&project_planetary::mesh_checks==1);
 const auto after=state.sceneDocument.revision().content;assert(after!=hash);
 assert(state.sceneDocument.undo()&&state.sceneDocument.revision().content==hash);mirror(state);assert(tree(state,10).positionAnchor==original.positionAnchor);
 assert(state.sceneDocument.redo()&&state.sceneDocument.revision().content==after);mirror(state);
 const auto encoded=scene::serialize_snapshot_text(state.sceneDocument.project_snapshot().snapshot);const auto parsed=scene::parse_snapshot_text(encoded);assert(parsed);
 auto reopened=Document::from_snapshot(parsed.snapshot);assert(reopened&&reopened.value.revision().content==after);
 auto revision=state.sceneDocument.revision();assert(refresh_forest_factory_instances(state));assert(state.sceneDocument.revision()==revision); // idempotent
 for(unsigned failure=0;failure<12;++failure){auto value=fixture();
  if(failure==0)value.planetaryMeshPolicy.reset();
  if(failure==1)value.planetaryMeshPolicy->selected_root="another-project";
  if(failure==2)value.planetaryMeshPolicy->generated_mesh->artifact_key[0]^=1;
  if(failure==3)value.entities[1].forestRegion->terrain_object_id=999;
  if(failure==4)value.entities[1].forestRegion->recipe+="changed";
  if(failure==5)value.entities[1].forestRegion->mesh_source_origin_meters[0]+=1;
  if(failure==6)value.entities[1].forestRegion->admitted_body_center_meters[0]+=1;
  if(failure==7)value.entities[1].forestRegion->body_radius_meters+=1;
  if(failure==8)value.entities[1].forestRegion->body_index=5;
  if(failure==9)value.entities[0].scale[0]=0;
  if(failure==10)project_planetary::source_current=false;
  if(failure==11)project_planetary::mesh_current=false;
  const auto anchor=value.entities[1].positionAnchor;
  assert(refresh_forest_factory_instances(value));assert(tree(value,10).positionAnchor==anchor);
  project_planetary::source_current=project_planetary::mesh_current=true;}
 for(unsigned busy=0;busy<3;++busy){auto value=fixture();const auto untouched=value;
  if(busy==0)value.forestSurfacePending=1;if(busy==1)value.projectBuildPending=1;if(busy==2)value.projectRuntimeProcess.running=true;
  assert(!refresh_forest_factory_instances(value));no_mutation(untouched,value);}
 // Full double parent transform, preserving all separate authored offsets.
 auto rotated=fixture();rotated.entities[0].position={10,20,30};rotated.entities[0].rotation={0,0,90};rotated.entities[0].scale={2,2,2};
 assert(refresh_forest_factory_instances(rotated));const auto anchored=tree(rotated,10);
 assert(std::abs((*anchored.positionAnchor)[0]-(10-2*6371000.0))<1e-5&&std::abs((*anchored.positionAnchor)[1]-20)<1e-5&&(*anchored.positionAnchor)[2]==30);
 assert(anchored.position==original.position&&anchored.rotation==original.rotation&&anchored.scale==original.scale);
 auto failedSave=fixture();saving=false;const auto unsaved=refresh_forest_factory_instances(failedSave);
 assert(unsaved.applied&&!unsaved.saved&&failedSave.plantLabStatus.find("saving failed")!=std::string::npos);
 return 0;}
'''


if __name__ == "__main__":
    unittest.main()
