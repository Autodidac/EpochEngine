"""Actual preview mesh loader and verified policy-refresh loop; no window/model."""
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
    folder=ROOT/'Engine/src/editor/application'
    preview=(folder/'editor.application_scene_preview.inl').read_text(encoding='utf-8')
    owner=(folder/'editor.application_asset_browsing.inl').read_text(encoding='utf-8')
    transactions=(folder/'editor.application_scene_transactions.inl').read_text(encoding='utf-8')
    start=preview.index('            const auto meshBudget=',preview.index('        void publish_editor_preview_markers('))
    end=preview.index('                if (!forestPainting && !restore_editor_mesh_base_color',start)
    load=preview[start:end]+'\n            }\n'
    project=definition(transactions,'        [[nodiscard]] bool project_scene_document_into_editor(')
    project+='\n'+ '\n'.join(definition(transactions.replace("std::string_view selected_name = {}", "std::string_view selected_name"),x) for x in ('        [[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(', '        [[nodiscard]] bool apply_editor_scene_transaction(', '        void append_editor_descriptor_operations(', '        [[nodiscard]] bool synchronize_editor_scene_category('))
    refresh=definition(owner,'        void refresh_editor_planetary_mesh_policy(')
    budget=definition(owner,'        [[nodiscard]] project_planetary::SceneMeshBudget editor_planetary_mesh_budget(')
    return r'''
#include "core.format_text.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
import asset.mesh_artifact;
import asset.texture_artifact;
import scene.document;
import scene.snapshot;
import scene.serializer;
using namespace epochengine;
namespace fs=std::filesystem;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Reopen budget contract line %d\n",line);std::exit(86);}}
#define CHECK(x) check(static_cast<bool>(x),__LINE__)
bool providerReady{};
namespace project_planetary{
 struct Settings{}; struct GeneratedMeshFrame{double meters_per_unit{1};std::array<double,3>source_origin_meters{};};
 struct Source{std::string content_hash;};
 struct SceneMeshBudget{asset::mesh::Limits artifact{0,0,0};std::uint64_t cache_bytes{16ull<<20},frame_indices{196608},frame_bytes{16ull<<20};};
 struct Identity{};
 std::optional<Settings>load_settings(const fs::path&){return providerReady?std::optional<Settings>{Settings{}}:std::nullopt;}
 std::optional<Source>capture_source_generation(const fs::path&,const std::string& hash){return Source{hash};}
 bool source_generation_is_current(const fs::path&,const Source&){return providerReady;}
 std::optional<Settings>mesh_generation_settings(const fs::path&,const Source&,Identity){return {};}
 SceneMeshBudget scene_mesh_budget(Settings){return {{65536,393216,4ull<<20},24ull<<20,1000000,24ull<<20};}
}
struct EditorEntity{
 scene::SceneObjectId sceneObjectId{};std::string name,type,category;scene::Vec3 position{},rotation{},scale{1,1,1};bool visible{true},editorOnly{};
 std::optional<scene::SceneTextureMaterialSnapshot>textureMaterial;std::optional<scene::SceneMeshSnapshot>meshAsset;
 std::optional<std::array<double,3>>positionAnchor;std::optional<scene::SceneForestRegionInstance>forestRegion;std::optional<project_planetary::GeneratedMeshFrame>meshSnapshotFrame;
 std::shared_ptr<const asset::mesh::Artifact>meshSnapshot;bool meshLoadAttempted{};
};
struct EditorState{
 struct PlanetaryMeshPolicy{std::string selected_root;project_planetary::Source source;project_planetary::Settings settings;std::optional<project_planetary::Identity>generated_mesh;};
 std::optional<PlanetaryMeshPolicy>planetaryMeshPolicy;
 struct Attachment{std::string content_hash{"verified-fixture-provider"};bool source_attached()const{return true;}}planetaryAttachment;
 std::string projectStatus;
 std::string projectRoot,planetaryCheckedRoot;std::optional<int>planetaryAttachmentPending;
 std::vector<EditorEntity>entities;std::vector<std::string>logs;
 authoring::scene::SceneDocument sceneDocument;std::uint64_t selectedEntityId{},sceneDocumentRevision{};
 struct{std::uint64_t frame_index{};double simulated_seconds{};}timeSnapshot;
 bool sceneInteractionDirty{},sceneLightingDirty{};
};
void synchronize_editor_selection(EditorState&){}
void push_editor_log(EditorState&s,std::string message){s.logs.push_back(std::move(message));}
fs::path resolve_editor_path(const fs::path&p){return p;}
bool is_screen_space_gui_entity(const EditorEntity&){return false;}
std::optional<project_planetary::Identity>editor_planetary_mesh_identity(const EditorEntity&){return {};}
void refresh_editor_planetary_body_inventory(EditorState&,const EditorEntity&){}
bool rebuild_editor_scene_document(EditorState&,std::string_view){return false;}
std::string editor_scene_identity(const EditorState&){return "reopen-budget";}
PROJECT
BUDGET
REFRESH
void load_preview_meshes(EditorState&state){const bool forestPreview=false,forestPainting=false;auto&previewEntities=state.entities;
LOAD
}
int main(int argc,char**argv){
 CHECK(argc==2);const fs::path root=fs::absolute(argv[1]);fs::create_directories(root);
 CHECK(asset::mesh::prepare_project_mesh_directory(root));
 std::array<asset::mesh::Vertex,3>vertices{};vertices[1].position={1,0,0};vertices[2].position={0,1,0};for(auto&v:vertices){v.normal={0,0,1};v.color={1,1,1};}
 scene::SceneSnapshot saved;saved.scene_id="reopen-budget";saved.project_id="project";saved.world_name="Budget fixture";
 for(unsigned i=0;i<3;++i){const std::size_t count=i==0?64512:94473;std::vector<std::uint32_t>indices(count);for(std::size_t j=0;j<count;++j)indices[j]=std::uint32_t(j%3);
  const auto mesh=asset::mesh::build_artifact(vertices,indices);CHECK(mesh);const auto logical="Assets/Mesh/"+std::to_string(i)+".epoch_mesh";const auto bytes=asset::mesh::encode_artifact(*mesh.artifact);
  std::ofstream output(root/logical,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));output.close();CHECK(output);
  scene::SceneObjectSnapshot object{.id=i+1u,.name="Mesh"+std::to_string(i),.type="IndexedMesh",.category=i==0?"Planetary Terrain":"World"};
  object.mesh_asset=scene::SceneMeshSnapshot{logical,mesh.artifact->key().words,3,std::uint32_t(count),scene::SceneMeshCoordinates{i==0?411127585.44979036:1.0,{0,0,0}}};saved.objects.push_back(std::move(object));}
 const auto disk=root/"main.epoch";{const auto bytes=scene::serialize_snapshot_text(saved);CHECK(!bytes.empty());std::ofstream out(disk,std::ios::binary);out<<bytes;}
 for(unsigned cycle=0;cycle<2;++cycle){
  std::ifstream in(disk,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>{in},{}};const auto parsed=scene::parse_snapshot_text(bytes);CHECK(parsed.ok);
  auto document=authoring::scene::SceneDocument::from_snapshot(parsed.snapshot);CHECK(document);EditorState state;state.projectRoot=root.generic_string();state.sceneDocument=std::move(document.value);
  CHECK(project_scene_document_into_editor(state));providerReady=false;
  if(cycle==1){state.planetaryCheckedRoot=state.projectRoot;state.planetaryAttachmentPending=1;}
  load_preview_meshes(state);
  CHECK(state.entities[0].meshSnapshot&&state.entities[1].meshSnapshot&&!state.entities[2].meshSnapshot&&state.entities[2].meshLoadAttempted);
  CHECK(state.logs.size()==1&&state.logs.back().find("Mesh loading deferred")!=std::string::npos);
  CHECK(state.logs.back().find("cache is full")==std::string::npos);
  load_preview_meshes(state);CHECK(state.logs.size()==1); // pending frames do not duplicate the message/retry
  const auto planet=state.entities[0].meshSnapshot;const auto first=state.entities[1].meshSnapshot;
  refresh_editor_planetary_mesh_policy(state);CHECK(state.entities[2].meshLoadAttempted&&!state.planetaryMeshPolicy);
  providerReady=true;state.planetaryCheckedRoot=state.projectRoot;state.planetaryAttachmentPending.reset();
  refresh_editor_planetary_mesh_policy(state);CHECK(state.planetaryMeshPolicy&&!state.entities[2].meshLoadAttempted);
  load_preview_meshes(state);CHECK(state.entities[2].meshSnapshot&&state.entities[2].meshLoadAttempted);
  CHECK(state.entities[0].meshSnapshot==planet&&state.entities[1].meshSnapshot==first);
  const auto second=state.entities[2].meshSnapshot;const auto logs=state.logs.size();load_preview_meshes(state);
  CHECK(state.entities[2].meshSnapshot==second&&state.logs.size()==logs);
  auto over=state.entities[2];over.sceneObjectId=4;over.name="Would exceed budget";over.meshSnapshot.reset();over.meshLoadAttempted=false;state.entities.push_back(std::move(over));
  load_preview_meshes(state);CHECK(!state.entities[3].meshSnapshot&&state.entities[3].meshLoadAttempted);
  CHECK(state.logs.back().find("cache is full")!=std::string::npos&&state.logs.back().find("Mesh loading deferred")==std::string::npos);
  const auto refusalLogs=state.logs.size();load_preview_meshes(state);CHECK(state.logs.size()==refusalLogs);
  CHECK(state.entities[0].meshSnapshot==planet&&state.entities[1].meshSnapshot==first&&state.entities[2].meshSnapshot==second);
  refresh_editor_planetary_mesh_policy(state);load_preview_meshes(state);CHECK(!state.entities[3].meshSnapshot);
 }
 // A completed unsuccessful check is not described as still pending.
 {auto document=authoring::scene::SceneDocument::from_snapshot(saved);CHECK(document);EditorState state;
  state.projectRoot=root.generic_string();state.planetaryCheckedRoot=state.projectRoot;state.sceneDocument=std::move(document.value);
  CHECK(project_scene_document_into_editor(state));providerReady=false;load_preview_meshes(state);
  CHECK(!state.entities[2].meshSnapshot&&state.logs.size()==1&&state.logs.back().find("cache is full")!=std::string::npos);}
 // The actual category diff and transaction must replace resident mesh/material,
 // even when identity/name/transform are unchanged, and preserve undo/reopen.
 {auto document=authoring::scene::SceneDocument::from_snapshot(saved);CHECK(document);EditorState state;state.projectRoot=root.generic_string();state.sceneDocument=std::move(document.value);
  CHECK(project_scene_document_into_editor(state));providerReady=true;refresh_editor_planetary_mesh_policy(state);load_preview_meshes(state);
  const auto old=state.entities[1];CHECK(old.meshSnapshot);auto changed=old;changed.meshAsset=state.entities[0].meshAsset;
  changed.textureMaterial=scene::SceneTextureMaterialSnapshot{.logical_path="Textures/Forest/bark",.artifact_key={1,2,3,4},.stable_key=123};
  auto sibling=state.entities[2];const std::vector<EditorEntity>desired{changed,sibling};
  CHECK(synchronize_editor_scene_category(state,"World",desired,"Refresh trees",changed.name));
  auto found=std::find_if(state.entities.begin(),state.entities.end(),[&](auto&e){return e.sceneObjectId==old.sceneObjectId;});CHECK(found!=state.entities.end());
  CHECK(!found->meshSnapshot&&!found->meshLoadAttempted&&found->meshAsset==changed.meshAsset&&found->textureMaterial==changed.textureMaterial);
  load_preview_meshes(state);CHECK(found->meshSnapshot&&found->meshSnapshot->key().words==changed.meshAsset->artifact_key&&found->meshSnapshot!=old.meshSnapshot);
  const auto snapshot=state.sceneDocument.project_snapshot();CHECK(snapshot);const auto parsed=scene::parse_snapshot_text(scene::serialize_snapshot_text(snapshot.snapshot));CHECK(parsed);
  const auto* persisted=scene::find_object_by_id(parsed.snapshot,old.sceneObjectId);CHECK(persisted&&persisted->mesh_asset==changed.meshAsset&&persisted->texture_material==changed.textureMaterial);
  CHECK(state.sceneDocument.undo());CHECK(project_scene_document_into_editor(state));
  found=std::find_if(state.entities.begin(),state.entities.end(),[&](auto&e){return e.sceneObjectId==old.sceneObjectId;});CHECK(found->meshAsset==old.meshAsset&&!found->meshSnapshot);
  CHECK(state.sceneDocument.redo());CHECK(project_scene_document_into_editor(state));load_preview_meshes(state);
  found=std::find_if(state.entities.begin(),state.entities.end(),[&](auto&e){return e.sceneObjectId==old.sceneObjectId;});CHECK(found->meshAsset==changed.meshAsset&&found->meshSnapshot);
 }
 // Unrelated ordinary-only scenes never wait for a nonexistent planetary envelope.
 {auto ordinary=saved;ordinary.objects.erase(ordinary.objects.begin());auto third=ordinary.objects.back();third.id=4;third.name="Third ordinary";ordinary.objects.push_back(third);
  auto document=authoring::scene::SceneDocument::from_snapshot(ordinary);CHECK(document);EditorState state;state.projectRoot=root.generic_string();state.sceneDocument=std::move(document.value);
  CHECK(project_scene_document_into_editor(state));load_preview_meshes(state);
  CHECK(!state.entities[2].meshSnapshot&&state.logs.size()==1&&state.logs.back().find("cache is full")!=std::string::npos);}
}
'''.replace('PROJECT',project).replace('BUDGET',budget).replace('REFRESH',refresh).replace('LOAD',load)


class MeshReopenBudget(unittest.TestCase):
    def test_real_reopen_load_and_verified_policy_refresh_debug_release(self):
        if os.name!='nt':self.skipTest('Installed MSVC CPU module consumer')
        proof=ROOT/'Artifacts/Reports/mesh-reopen-budget-oct28'/str(time.time_ns());proof.mkdir(parents=True)
        source=proof/'editor.mesh_reopen_contract.cpp';source.write_text(consumer(),encoding='utf-8')
        roots=[ROOT/'Engine/modules',ROOT/'Engine/dep/EpochPlatformEngine/modules'];modules={}
        for folder in roots:
            for p in folder.glob('*.ixx'):
                m=re.search(r'^export module\s+([\w.:]+);',p.read_text(encoding='utf-8-sig'),re.M)
                if m:modules[m[1]]=p
        selected={};pending=['asset.mesh_artifact','scene.document','scene.serializer']
        while pending:
            name=pending.pop()
            if name in selected:continue
            selected[name]=modules[name];pending+=re.findall(r'^(?:export )?import\s+([\w.]+);',modules[name].read_text(encoding='utf-8-sig'),re.M)
        implementation=ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp'
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(MeshReopenBudget LANGUAGES CXX)
add_executable(contract "{source.as_posix()}" "{implementation.as_posix()}" "{(ROOT/'Engine/resource/epoch.manifest').as_posix()}")
target_include_directories(contract PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_compile_features(contract PRIVATE cxx_std_23)
target_compile_options(contract PRIVATE /W3 /WX /utf-8 /EHsc)
target_sources(contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {' '.join('"'+p.as_posix()+'"' for p in roots)} FILES {' '.join('"'+p.as_posix()+'"' for p in selected.values())})
''',encoding='utf-8')
        build=ROOT/'Artifacts/Intermediate/tests/mesh-reopen-budget'/proof.name
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']]
        for mode in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',mode,'--parallel','1'],[str(build/mode/'contract.exe'),str(proof/mode)]]
        results=[]
        for i,command in enumerate(commands):
            ran=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=150)
            (proof/f'lane-{i}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8');results.append({'command':command,'exit':ran.returncode})
            self.assertEqual(ran.returncode,0,ran.stdout[-7000:]+ran.stderr)
        (proof/'result.json').write_text(json.dumps({'passed':True,'commands':results,'scope':'Actual mesh-load phase and policy-refresh function extracted from production, actual SceneDocument projection/serializer/disk reopen and mesh artifact IO. Provider verification/result and budget envelope are explicit external-package ports; no runtime/model/render. Two reopen cycles prove fallback ordinary-budget refusal then verified refresh retry. Used meshes/pointers remain retained; third ordinary model still refuses under unchanged196608 cap.'},indent=2),encoding='utf-8')


if __name__=='__main__':unittest.main()
