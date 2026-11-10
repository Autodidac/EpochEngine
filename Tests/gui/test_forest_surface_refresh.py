"""Real cancellable refresh, scene history and cold persistence; no GPU/model."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / 'Engine/src/editor/application'


class ForestSurfaceRefresh(unittest.TestCase):
    def test_async_explicit_refresh_preserves_authored_trees_atomically(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC C++23 module consumer lane')
        source = (OWNER / 'editor.application_scene_preview.inl').read_text(encoding='utf-8')
        state = (OWNER / 'editor.application_state.inl').read_text(encoding='utf-8')
        transactions = (OWNER / 'editor.application_scene_transactions.inl').read_text(encoding='utf-8')
        context = (OWNER / 'editor.application_context_session.inl').read_text(encoding='utf-8')
        stop = definition(context, '        void request_editor_automation_stop(')
        refresh = '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] bool begin_forest_surface_refresh(',
            '        void complete_forest_surface_refresh(',
            '        void poll_forest_surface_placement('))
        structs = '\n'.join(definition(state, marker, True) for marker in (
            '            struct ForestSurfacePlacement final',
            '            struct ForestSurfacePrepared final'))
        transaction_functions = '\n'.join(definition(transactions, marker) for marker in (
            '        [[nodiscard]] bool project_scene_document_into_editor(',
            '        [[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(',
            '        [[nodiscard]] bool apply_editor_scene_transaction('))
        camera = (ROOT / 'Engine/src/render/render.preview_camera.cpp').read_text(encoding='utf-8')
        transform = definition((ROOT / 'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8'),
                               '    export struct PreciseMeshTransform final', True).replace('export ', '')
        transform += '\n' + '\n'.join(definition(camera, marker) for marker in (
            '    [[nodiscard]] bool valid_precise_mesh_transform(',
            '    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform('))
        code = r'''
#include "core.format_text.hpp"
#include "provider_ports.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <crtdbg.h>
import scene.document;
import scene.snapshot;
import scene.serializer;
import scene.persistence;
import project.planetary;
import asset.mesh_artifact;
import editor.task_scheduler;
import forest.factory;
#undef assert
#define assert(value) do {if(!(value)){std::fprintf(stderr,"Forest refresh assertion line %d: %s\n",__LINE__,#value);std::exit(83);}}while(false)
namespace fixture_ports {
std::atomic<int> mode{0}, calls{0}, entered{0};
std::atomic<bool> release{false}, source_current{true}, mesh_current{true};
std::mutex lock;std::condition_variable condition;
}
namespace epochengine {
namespace fs=std::filesystem;
namespace forest {
namespace preview_mesh {
std::optional<std::array<float,3>> surface_rotation(std::array<double,3>,std::array<double,3>){std::abort();}
}
}
namespace previewgrid {
struct Vec3 {float x{},y{},z{};};
TRANSFORM
}
struct EditorEntity {
 scene::SceneObjectId sceneObjectId{};std::string name,type,category;
 std::array<float,3> position{},rotation{},scale{1,1,1};bool visible{true},editorOnly{};
 decltype(scene::SceneObjectSnapshot{}.texture_material) textureMaterial{};
 decltype(scene::SceneObjectSnapshot{}.mesh_asset) meshAsset{};
 bool meshLoadAttempted{};std::shared_ptr<const asset::mesh::Artifact> meshSnapshot;
 std::optional<std::array<double,3>> positionAnchor;std::optional<scene::SceneForestRegionInstance>forestRegion;
};
struct EditorState {
 std::string projectId{"refresh-forest"},projectRoot,projectStatus,plantLabStatus;
 std::vector<EditorEntity> entities;authoring::scene::SceneDocument sceneDocument;
 std::uint64_t selectedEntityId{100},sceneDocumentRevision{},forestSurfaceSequence{};
 bool sceneInteractionDirty{},sceneLightingDirty{},projectBuildPending{},planetaryGeneratePending{};
 bool forestPlacementFocusPending{},aiGoalForestReceipt{},saveFails{};unsigned saveCalls{},plantCalls{};
 bool aiGoalRunning{true},aiGoalPlanNextQueued{true};unsigned taskStopCalls{};
 struct {bool stopped{},waiting{true};}aiGoalTools;
 struct PackageRequest {bool cancelled{};};std::optional<PackageRequest>aiPackageRequest{PackageRequest{}};
 struct {bool valid()const{return running;}bool running{};}projectRuntimeProcess;
 struct {std::uint64_t frame_index{};double simulated_seconds{};}timeSnapshot;
 struct {struct Evidence {struct {struct {std::uint64_t generation{9};}selected_project;}generations;}value;
  const Evidence& evidence()const{return value;}}projectLifecycle;
 struct {std::uint64_t sourceRevision{},sourceContentHash{};struct{std::uint64_t stableHash{};}genome;}plantLabCompiled;
 struct PlanetaryMeshPolicy {
  std::string selected_root;project_planetary::SourceGeneration source;project_planetary::Settings settings;
  std::optional<project_planetary::MeshGenerationIdentity>generated_mesh;
  std::vector<project_planetary::GeneratedBody>generated_bodies;bool body_inventory_checked{true};
 };
 std::optional<PlanetaryMeshPolicy>planetaryMeshPolicy;
 STRUCTS
 std::optional<ForestSurfacePlacement>forestSurfacePlacement;
 std::optional<std::future<ForestSurfacePrepared>>forestSurfacePending;
 editor_tasks::CancellationTicket forestSurfaceCancellation;
 std::unique_ptr<editor_tasks::Scheduler>taskScheduler{std::make_unique<editor_tasks::Scheduler>(
  editor_tasks::SchedulerLimits{.worker_count=2,.retained_finished_nodes=64})};
};
void push_editor_log(EditorState&,std::string_view){}
void synchronize_editor_selection(EditorState&){}
bool rebuild_editor_scene_document(EditorState&,std::string_view){std::abort();}
bool has_open_project(const EditorState&s){return !s.projectId.empty();}
fs::path resolve_editor_path(const fs::path&p){return p;}
std::string editor_scene_identity(const EditorState&){return "world:refresh-forest";}
std::optional<std::size_t>editor_entity_index(const EditorState&s,scene::SceneObjectId id){
 for(std::size_t i=0;i<s.entities.size();++i)if(s.entities[i].sceneObjectId==id)return i;return {};}
bool editor_planetary_stream_identity_matches(const EditorState&s,const EditorEntity&e){
 return e.category=="Planetary Terrain"&&e.meshAsset&&s.planetaryMeshPolicy&&s.planetaryMeshPolicy->generated_mesh
  &&e.meshAsset->artifact_key==s.planetaryMeshPolicy->generated_mesh->artifact_key;}
void refresh_editor_planetary_body_inventory(EditorState&,const EditorEntity&){}
struct Budget {asset::mesh::Limits artifact;std::uint64_t cache_bytes{128ull*1024*1024};};
Budget editor_planetary_mesh_budget(EditorState&){return {};}
template<class Operation>auto schedule_editor_cancellable_task(EditorState&s,std::string label,Operation&&operation){
 return s.taskScheduler->submit_cancellable(std::move(label),std::forward<Operation>(operation));}
TRANSACTIONS
bool save_editor_scene_snapshot(EditorState&s){
 ++s.saveCalls;if(s.saveFails)return false;
 const auto projection=s.sceneDocument.project_snapshot();assert(projection);
 return bool(scene::persistence::save_scene_snapshot_atomic(fs::path{s.projectRoot}/"main.epoch",projection.snapshot));}
bool ensure_plant_lab_compiled_asset(EditorState&s){++s.plantCalls;std::abort();}
template<class...T>bool create_editor_scene_entities(T&&...){std::abort();}
bool publish_plant_lab_project_asset(EditorState&){std::abort();}
constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
void request_editor_task_session_stop(EditorState&s){++s.taskStopCalls;}
STOP
REFRESH
EditorState make_state(const fs::path&root,bool multipleBodies=false){
 fs::create_directories(root);EditorState state;state.projectRoot=root.generic_string();
 scene::SceneSnapshot snapshot;snapshot.scene_id="world:refresh-forest";snapshot.project_id=state.projectId;snapshot.world_name="Refresh Forest";
 const std::array<asset::mesh::Vertex,3> vertices{{{{0,0,0}},{{1,0,0}},{{0,1,0}}}};
 const std::array<std::uint32_t,3>indices{0,1,2};const auto terrainMesh=asset::mesh::build_artifact(vertices,indices);assert(terrainMesh);
 fs::create_directories(root/"Assets/Mesh");const auto encoded=asset::mesh::encode_artifact(*terrainMesh.artifact);
 {std::ofstream out(root/"Assets/Mesh/current.epoch_mesh",std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());assert(out);}
 scene::SceneObjectSnapshot terrain;terrain.id=100;terrain.name="Planet";terrain.type="IndexedMesh";terrain.category="Planetary Terrain";
 terrain.position={2,3,4};terrain.position_anchor=std::array<double,3>{1e12,-2e12,3e12};terrain.rotation={0,0,90};terrain.scale={2,2,2};
 terrain.mesh_asset=scene::SceneMeshSnapshot{.logical_path="Assets/Mesh/current.epoch_mesh",.artifact_key=terrainMesh.artifact->key().words,
  .vertex_count=3,.index_count=3,.coordinates=scene::SceneMeshCoordinates{.meters_per_unit=1,.source_origin_meters={6500,100,50}}};
 snapshot.objects.push_back(terrain);
 for(unsigned i=0;i<5;++i){
  scene::SceneObjectSnapshot tree;tree.id=200+i;tree.name="SavedTree"+std::to_string(i);tree.type="ForestTrunk";tree.category="ForestFactory";
  tree.position={.125f,.25f,-.5f};tree.rotation={float(i),float(10+i),float(20+i)};tree.scale={1.1f,1.1f,1.1f};
  tree.position_anchor=std::array<double,3>{1000.0+i,2000,3000};tree.visible=i!=4;
  tree.mesh_asset=scene::SceneMeshSnapshot{.logical_path="Assets/Mesh/tree.epoch_mesh",.artifact_key={21,22,23,24},.vertex_count=3,.index_count=3};
  tree.texture_material=scene::SceneTextureMaterialSnapshot{.logical_path="Textures/Forest/custom_cutout",.artifact_key={31,32,33,34},.stable_key=35,
   .alpha=scene::SceneTextureAlphaMode::cutout};
  const auto angle=.001*double(i);const std::array<double,3>up{std::sin(angle),std::cos(angle),0};
  tree.forest_region=scene::SceneForestRegionInstance{.region_id=77,.terrain_object_id=100,.seed=1337,.plant_genome=45,.plant_revision=106,.plant_hash=46,
   .body_index=multipleBodies&&i>=3?1u:0u,.candidate_index=i,.candidate_count=5,.radius_meters=12,.density_per_square_meter=.01,.body_radius_meters=997,
   .center_direction={0,1,0},.body_local_root_meters={0,997,0},.radial_up=up,.terrain_artifact_key={1,2,3,4},
   .provider_content_hash=std::string(64,'a'),.recipe="old-compact-no-ocean",.plant_source="Assets/Forest/old_published.epoch_forest"};
  assert(tree.forest_region->valid());snapshot.objects.push_back(tree);
 }
 auto built=authoring::scene::SceneDocument::from_snapshot(snapshot);assert(built);state.sceneDocument=std::move(built.value);
 assert(project_scene_document_into_editor(state));state.entities[0].meshSnapshot=terrainMesh.artifact;
 state.planetaryMeshPolicy.emplace();auto&policy=*state.planetaryMeshPolicy;policy.settings.maximum_level=20;policy.settings.ocean_surface=true;
 policy.source={root,"project.epoch.json","sha256:"+std::string(64,'b')};
 policy.generated_mesh=project_planetary::MeshGenerationIdentity{terrain.mesh_asset->artifact_key,3,3,{1,{6500,100,50}}};
 policy.generated_bodies.push_back({.id=0,.name="Main",.center_meters={7000,0,0},.radius_meters=1000});
 if(multipleBodies)policy.generated_bodies.push_back({.id=1,.name="Moon",.center_meters={20000,0,0},.radius_meters=200});
 return state;
}
std::string scene_bytes(const EditorState&s){auto p=s.sceneDocument.project_snapshot();assert(p);return scene::serialize_snapshot_text(p.snapshot);}
void reset_ports(int mode=0){fixture_ports::mode=mode;fixture_ports::calls=0;fixture_ports::entered=0;fixture_ports::release=false;fixture_ports::source_current=true;fixture_ports::mesh_current=true;}
void wait_entered(){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds{3};
 while(!fixture_ports::entered.load()&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds{1});assert(fixture_ports::entered.load());}
void release_worker(){fixture_ports::release=true;fixture_ports::condition.notify_all();}
void finish(EditorState&s){release_worker();assert(s.forestSurfacePending);
 assert(s.forestSurfacePending->wait_for(std::chrono::seconds{3})==std::future_status::ready);poll_forest_surface_placement(s);assert(!s.forestSurfacePending&&!s.forestSurfacePlacement);}
void success_contract(const fs::path&root,bool multiple){
 reset_ports();auto s=make_state(root,multiple);const auto before=s.sceneDocument.project_snapshot().snapshot;
 const auto initial=scene_bytes(s);const auto revision=s.sceneDocument.revision().sequence;
 assert(begin_forest_surface_refresh(s));wait_entered();assert(scene_bytes(s)==initial&&s.saveCalls==0);
 assert(!begin_forest_surface_refresh(s));finish(s);assert(s.plantCalls==0&&s.saveCalls==1);
 assert(s.sceneDocument.revision().sequence==revision+1&&s.entities.size()==6);
 assert(fixture_ports::calls==(multiple?2:1));const auto&intent=s.planetaryMeshPolicy;
 for(std::size_t i=1;i<s.entities.size();++i){
  const auto&old=before.objects[i];const auto&now=s.entities[i];assert(now.sceneObjectId==old.id&&now.name==old.name);
  assert(now.position==old.position&&now.rotation==old.rotation&&now.scale==old.scale&&now.visible==old.visible);
  assert(now.meshAsset==old.mesh_asset&&now.textureMaterial==old.texture_material);
  auto expected=*old.forest_region;const auto&region=*now.forestRegion;const auto&body=intent->generated_bodies[region.body_index];
  expected.body_local_root_meters=region.body_local_root_meters;expected.radial_up=region.radial_up;
  expected.body_radius_meters=body.radius_meters;expected.admitted_body_center_meters=body.center_meters;
  expected.mesh_source_origin_meters=intent->generated_mesh->coordinates.source_origin_meters;
  expected.terrain_artifact_key=intent->generated_mesh->artifact_key;expected.provider_content_hash=std::string(64,'b');expected.recipe=project_planetary::encode_settings(intent->settings);
  assert(region==expected&&region.radial_up==old.forest_region->radial_up);
  const auto x=body.center_meters[0]-6500+region.radial_up[0]*(body.radius_meters+8);
  const auto y=-100+region.radial_up[1]*(body.radius_meters+8);const auto z=-50.0;
  assert(std::abs((*now.positionAnchor)[0]-(1e12+2-2*y))<.001);
  assert(std::abs((*now.positionAnchor)[1]-(-2e12+3+2*x))<.001);
  assert(std::abs((*now.positionAnchor)[2]-(3e12+4+2*z))<.001);
 }
 const auto updated=scene_bytes(s);assert(updated!=initial&&s.plantLabStatus.find("and saved")!=std::string::npos);
 assert(s.sceneDocument.undo());assert(project_scene_document_into_editor(s));
 auto undone=s.sceneDocument.project_snapshot().snapshot;undone.revision=before.revision;
 assert(scene::serialize_snapshot_text(undone)==scene::serialize_snapshot_text(before));
 assert(s.sceneDocument.redo());assert(project_scene_document_into_editor(s));
 const auto loaded=scene::persistence::load_scene_snapshot(root/"main.epoch");assert(loaded.result);
 const auto reopened=authoring::scene::SceneDocument::from_snapshot(loaded.snapshot);assert(reopened);
 const auto restored=reopened.value.project_snapshot();assert(restored&&restored.snapshot.objects.size()==6);
 for(std::size_t i=1;i<6;++i)assert(restored.snapshot.objects[i].id==s.entities[i].sceneObjectId
  &&restored.snapshot.objects[i].forest_region==s.entities[i].forestRegion
  &&restored.snapshot.objects[i].position_anchor==s.entities[i].positionAnchor
  &&restored.snapshot.objects[i].texture_material==s.entities[i].textureMaterial);
 const auto beforeNoop=s.sceneDocument.revision().sequence;reset_ports();assert(begin_forest_surface_refresh(s));wait_entered();finish(s);
 assert(s.sceneDocument.revision().sequence==beforeNoop&&s.saveCalls==1&&s.plantLabStatus.find("already current")!=std::string::npos);
}
void refusal_contract(const fs::path&root,int mode,const std::function<void(EditorState&)>&change={}){
 reset_ports(mode);auto s=make_state(root,true);const auto initial=scene_bytes(s);assert(begin_forest_surface_refresh(s));wait_entered();
 if(change)change(s);const auto beforeCompletion=scene_bytes(s);finish(s);
 assert(scene_bytes(s)==beforeCompletion&&s.saveCalls==0&&s.plantCalls==0);
 assert(s.plantLabStatus.starts_with("Forest placement unchanged:"));
}
}
int main(int argc,char**argv){
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
 using namespace epochengine;assert(argc==2);const auto root=fs::path{argv[1]};assert(root.is_absolute());
 success_contract(root/"one",false);success_contract(root/"two",true);
 for(int mode=1;mode<=7;++mode)refusal_contract(root/std::to_string(mode),mode);
 refusal_contract(root/"source",0,[](auto&){fixture_ports::source_current=false;});
 refusal_contract(root/"mesh",0,[](auto&){fixture_ports::mesh_current=false;});
 refusal_contract(root/"project",0,[](auto&s){++s.projectLifecycle.value.generations.selected_project.generation;});
 refusal_contract(root/"projectid",0,[](auto&s){s.projectId="other";});
 refusal_contract(root/"settings",0,[](auto&s){s.planetaryMeshPolicy->settings.maximum_level=19;});
 refusal_contract(root/"transform",0,[](auto&s){s.entities[0].rotation[2]=30;});
 refusal_contract(root/"tree",0,[](auto&s){s.entities[1].textureMaterial->artifact_key[3]^=1;});
 refusal_contract(root/"scene",0,[](auto&s){auto h=s.sceneDocument.find({201});assert(h);
  auto t=s.sceneDocument.object(*h)->descriptor.transform;t.position.x=2;
  const std::array<authoring::scene::SceneOperation,1>ops{authoring::scene::ObjectTransformChangedOperation{*h,t}};
  assert(apply_editor_scene_transaction(s,ops,"Operator edit"));});
 reset_ports();auto cancelled=make_state(root/"cancel");const auto original=scene_bytes(cancelled);
 assert(begin_forest_surface_refresh(cancelled));wait_entered();
 request_editor_automation_stop(cancelled);
 assert(cancelled.forestSurfacePending&&!cancelled.forestSurfacePlacement&&cancelled.forestSurfaceCancellation.valid());
 finish(cancelled);assert(scene_bytes(cancelled)==original&&cancelled.saveCalls==0);
 // Cancellation cannot retract a completed future. Common Stop must revoke
 // its still-pending scene commit before the ordinary poll consumes success.
 reset_ports();auto ready=make_state(root/"ready-stop");const auto readyOriginal=scene_bytes(ready);
 const auto readyRevision=ready.sceneDocument.revision().sequence;
 assert(begin_forest_surface_refresh(ready));wait_entered();release_worker();
 assert(ready.forestSurfacePending->wait_for(std::chrono::seconds{3})==std::future_status::ready);
 assert(ready.forestSurfacePlacement&&ready.forestSurfacePending);
 request_editor_automation_stop(ready);
 assert(ready.taskStopCalls==1&&!ready.aiGoalRunning&&!ready.aiGoalPlanNextQueued
  &&ready.aiGoalTools.stopped&&!ready.aiGoalTools.waiting&&ready.aiPackageRequest->cancelled);
 assert(!ready.forestSurfacePlacement&&ready.forestSurfacePending&&ready.forestSurfaceCancellation.valid());
 assert(scene_bytes(ready)==readyOriginal&&ready.saveCalls==0);
 poll_forest_surface_placement(ready);
 assert(!ready.forestSurfacePending&&!ready.forestSurfacePlacement&&ready.saveCalls==0&&ready.plantCalls==0);
 assert(scene_bytes(ready)==readyOriginal&&ready.sceneDocument.revision().sequence==readyRevision);
 assert(!fs::exists(root/"ready-stop/main.epoch"));
 // Retiring the revoked result must not leave the existing refresh owner stuck.
 reset_ports();assert(begin_forest_surface_refresh(ready));wait_entered();finish(ready);assert(ready.saveCalls==1);
 reset_ports();auto unsaved=make_state(root/"save-fail");unsaved.saveFails=true;const auto old=scene_bytes(unsaved);
 assert(begin_forest_surface_refresh(unsaved));wait_entered();finish(unsaved);
 assert(scene_bytes(unsaved)!=old&&unsaved.plantLabStatus.find("scene save failed")!=std::string::npos);
 assert(unsaved.sceneDocument.undo());assert(!fs::exists(root/"save-fail/main.epoch"));
 reset_ports();auto selected=make_state(root/"selected");selected.selectedEntityId=200;
 assert(begin_forest_surface_refresh(selected));assert(selected.forestSurfacePlacement->refreshTrees.size()==5);wait_entered();finish(selected);
 assert(selected.selectedEntityId==200&&selected.entities.size()==6);
 reset_ports();auto invalid=make_state(root/"invalid");invalid.entities[1].forestRegion->radial_up={};
 assert(!begin_forest_surface_refresh(invalid)&&!invalid.forestSurfacePending&&fixture_ports::calls==0);
 std::ofstream result(root/"case-result.json");result<<"{\"five_ids_preserved\":true,\"actual_scheduler\":true,\"actual_scene_history_and_persistence\":true,\"actual_common_stop_ready_result_revoked\":true,\"native_acceptance\":false}";
}
'''.replace('TRANSFORM', transform).replace('STRUCTS', structs).replace('TRANSACTIONS', transaction_functions).replace('STOP', stop).replace('REFRESH', refresh)
        port_header = r'''
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
namespace fixture_ports {
extern std::atomic<int> mode,calls,entered;
extern std::atomic<bool> release,source_current,mesh_current;
extern std::mutex lock;extern std::condition_variable condition;
}
'''
        provider = r'''
module;
#include "provider_ports.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <stop_token>
#include <string>
module project.planetary;
namespace epochengine::project_planetary {
std::string encode_settings(const Settings&s){return "test-approved-recipe:"+std::to_string(s.maximum_level)+":"+std::to_string(s.ocean_surface);}
bool source_generation_is_current(const std::filesystem::path&,const SourceGeneration&,std::stop_token){return fixture_ports::source_current;}
bool mesh_generation_is_current(const std::filesystem::path&,const SourceGeneration&,const Settings&,const MeshGenerationIdentity&){return fixture_ports::mesh_current;}
SurfaceBatchResult query_surfaces(const std::filesystem::path&,const SurfaceBatchQuery&batch,std::stop_token stop){
 ++fixture_ports::calls;fixture_ports::entered=true;
 {std::unique_lock guard{fixture_ports::lock};while(!fixture_ports::release&&!stop.stop_requested())
  fixture_ports::condition.wait_for(guard,std::chrono::milliseconds{2});}
 SurfaceBatchResult out;out.request_id=batch.query.request_id;
 if(stop.stop_requested()){out.code=SurfaceCode::cancelled;out.operation.message="cancelled";return out;}
 if(fixture_ports::mode==1){out.code=SurfaceCode::busy;out.operation.message="Terrain busy; retry Refresh Forest Placement.";return out;}
 out.code=SurfaceCode::ready;
 for(const auto&direction:batch.directions){
  SurfaceResult sample;sample.code=SurfaceCode::ready;sample.request_id=out.request_id;sample.radial_up=direction;sample.normal=direction;sample.height_meters=8;
  for(unsigned i=0;i<3;++i)sample.point_meters[i]=direction[i]*(batch.query.body.radius_meters+sample.height_meters);
  out.samples.push_back(sample);
 }
 if(fixture_ports::mode==2&&!out.samples.empty())out.samples.pop_back();
 if(fixture_ports::mode==3&&!out.samples.empty()){
  auto& sample=out.samples.back();sample.height_meters=-1;
  for(unsigned i=0;i<3;++i)sample.point_meters[i]=sample.radial_up[i]*(batch.query.body.radius_meters-1);}
 if(fixture_ports::mode==4&&!out.samples.empty())out.samples.back().point_meters[0]=std::nan("");
 if(fixture_ports::mode==5)++out.request_id;
 if(fixture_ports::mode==6&&!out.samples.empty())++out.samples.back().request_id;
 if(fixture_ports::mode==7&&!out.samples.empty())out.samples.back().radial_up={0,0,1};
 return out;
}
}
'''
        report = ROOT / 'Artifacts/Reports/forest-surface-refresh-nov06/owner'
        report.mkdir(parents=True, exist_ok=True)
        (report / 'consumer.cpp').write_text(code, encoding='utf-8')
        (report / 'provider_ports.hpp').write_text(port_header, encoding='utf-8')
        (report / 'provider.cpp').write_text(provider, encoding='utf-8')
        modules = {}
        for path in (ROOT / 'Engine/modules').glob('*.ixx'):
            match = re.search(r'^export module\s+([\w.:]+);', path.read_text(encoding='utf-8-sig'), re.M)
            if match:
                modules[match.group(1)] = path
        excluded = {'core.logger', 'core.mpmc_bounded_queue', 'systems.registry', 'systems.task', 'taskgraph.dotsystem', 'editor.task_scheduler', 'platform.engine'}
        pending = ['scene.document', 'scene.persistence', 'scene.serializer', 'scene.snapshot',
                   'asset.mesh_artifact', 'project.planetary', 'forest.factory']
        selected = {}
        while pending:
            name = pending.pop()
            if name in selected or name in excluded or name in ('std', 'std.compat'):
                continue
            self.assertIn(name, modules, name)
            path = modules[name]
            selected[name] = path
            pending += re.findall(r'^(?:export )?import\s+([\w.]+);', path.read_text(encoding='utf-8-sig'), re.M)
        fixture = ROOT / 'Artifacts/Runtime/TestData' / ('f' + f'{time.time_ns():x}'[-8:])
        paths = '\n'.join('"' + path.as_posix() + '"' for path in selected.values())
        (report / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ForestSurfaceRefresh LANGUAGES CXX)
add_executable(forest_refresh consumer.cpp provider.cpp "{(ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}")
target_compile_features(forest_refresh PRIVATE cxx_std_23)
target_compile_options(forest_refresh PRIVATE /W3 /WX /utf-8 /UNDEBUG /Z7)
target_include_directories(forest_refresh PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(forest_refresh PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {paths})
include("{(ROOT/'Engine/cmake/epoch_task_scheduler.cmake').as_posix()}")
epoch_admit_task_scheduler(forest_refresh "{ROOT.as_posix()}")
enable_testing()
add_test(NAME actual_async_forest_refresh COMMAND forest_refresh "{fixture.as_posix()}/$<CONFIG>")
set_tests_properties(actual_async_forest_refresh PROPERTIES TIMEOUT 30)
''', encoding='utf-8')
        build = ROOT / 'Artifacts/Intermediate/tests/forest-surface-refresh-owner'
        commands = [['cmake', '-S', str(report), '-B', str(build), '-G', 'Visual Studio 17 2022', '-A', 'x64']]
        for configuration in ('Debug', 'Release'):
            commands += [['cmake', '--build', str(build), '--config', configuration, '--parallel', '1'],
                         ['ctest', '--test-dir', str(build), '-C', configuration, '--output-on-failure']]
        runs = []
        for index, command in enumerate(commands):
            ran = subprocess.run(command, cwd=report, capture_output=True, text=True, timeout=180)
            (report / f'lane-{index}.log').write_text(ran.stdout + ran.stderr, encoding='utf-8')
            runs.append(dict(command=command, exit=ran.returncode))
            (report / 'result.json').write_text(json.dumps(dict(passed=False, runs=runs), indent=2), encoding='utf-8')
            self.assertEqual(ran.returncode, 0, ran.stdout[-12000:] + ran.stderr)
        (report / 'result.json').write_text(json.dumps(dict(passed=True, runs=runs, fixture=str(fixture),
            source_sha256=hashlib.sha256(source.encode()).hexdigest(), native_acceptance=False,
            stop_owner_sha256=hashlib.sha256(context.encode()).hexdigest(),
            scope='Actual begin/worker/poll/completion, actual scheduler, scene transaction, Undo/Redo, save/cold reopen. Provider process boundary is deterministic recording/failure port.'), indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
