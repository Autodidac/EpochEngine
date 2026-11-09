"""Actual forest placement completion with recording project/provider ports."""
from pathlib import Path
import re
import unittest
from Tests.scene.test_forest_area import run_forest_cpu_contract
from Tests.scene.test_position_anchor import CODE as ANCHOR_CODE
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
owner=(ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(encoding='utf-8')
actual=definition(owner,'        void poll_forest_surface_placement(')
preview=(ROOT/'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
transform=preview[preview.index('    export struct PreciseMeshTransform'):preview.index('    export struct ObjectMarker')].replace('export ','')
camera_source=(ROOT/'Engine/src/render/render.preview_camera.cpp').read_text(encoding='utf-8')
transform+='\n'+definition(camera_source,'    [[nodiscard]] bool valid_precise_mesh_transform(')+'\n'+definition(camera_source,'    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(')
CODE=ANCHOR_CODE[:ANCHOR_CODE.index('int main(')]+r'''
#include "core.format_text.hpp"
#include <future>
#include <functional>
#include <memory>
#include <stop_token>
#include <chrono>
import forest.factory;
import asset.mesh_artifact;
namespace forest=epochengine::forest;
namespace asset=epochengine::asset;
namespace editor_tasks { struct TaskCancelled final {}; }
namespace project_planetary {
struct Settings {bool ocean_surface{};friend bool operator==(const Settings&,const Settings&)=default;};
struct Mesh {std::array<std::uint64_t,4>artifact_key{1,2,3,4};struct Coordinates {std::array<double,3>source_origin_meters{};friend bool operator==(const Coordinates&,const Coordinates&)=default;}coordinates;friend bool operator==(const Mesh&,const Mesh&)=default;};
struct Body {unsigned id{};double radius_meters{6371000};std::array<double,3>center_meters{};friend bool operator==(const Body&,const Body&)=default;};
struct Source {std::string content_hash="sha256:"+std::string(64,'a');};
struct SurfaceQuery {Source source;Settings settings;Mesh mesh;Body body;std::array<double,3>direction{0,1,0};std::uint64_t request_id{1};};
enum class SurfaceCode {ready,busy,cancelled,invalid};
struct Sample {SurfaceCode code{SurfaceCode::ready};double height_meters{1};std::array<double,3>point_meters{0,6371001,0},radial_up{0,1,0};};
struct Batch {SurfaceCode code{SurfaceCode::ready};std::uint64_t request_id{1};struct {std::string message;}operation;std::vector<Sample>samples;};
inline bool sourceCurrent=true,meshCurrent=true;
bool source_generation_is_current(const fs::path&,const Source&){return sourceCurrent;}
bool mesh_generation_is_current(const fs::path&,const Source&,const Settings&,const Mesh&){return meshCurrent;}
std::string encode_settings(const Settings&){return "exact recipe\n";}
}
namespace previewgrid {using Vec3=interaction::Vec3;TRANSFORM}
struct EditorEntity {
 std::uint64_t sceneObjectId{2};std::string name="Planet",type="Planet",category="Planetary Terrain";
 std::array<float,3>position{},rotation{},scale{1,1,1};bool visible{true};
 std::optional<std::array<double,3>>positionAnchor;
 std::optional<scene::SceneMeshSnapshot>meshAsset;
 std::shared_ptr<const asset::mesh::Artifact>meshSnapshot;
 std::optional<scene::SceneForestRegionInstance>forestRegion;
};
struct EditorState {
 std::string projectId="project",projectRoot="root",plantLabStatus;
 struct Lifecycle {struct Evidence {struct Generations {struct Project {std::uint64_t generation{1};}selected_project;}generations;}value;const Evidence& evidence()const{return value;}}projectLifecycle;
 struct Runtime {bool running{};bool valid()const{return running;}}projectRuntimeProcess;
 bool projectBuildPending{},forestPlacementFocusPending{},aiGoalForestReceipt{},compiledValid{true},identityCurrent{true};
 forest::CompiledForestAsset plantLabCompiled=forest::compile_forest_asset(forest::make_default_plant_lab_document());
 struct Policy {project_planetary::Settings settings;std::optional<project_planetary::Mesh>generated_mesh{project_planetary::Mesh{}};std::vector<project_planetary::Body>generated_bodies{project_planetary::Body{}};};
 std::optional<Policy>planetaryMeshPolicy{Policy{}};
 struct ForestSurfacePlacement {std::string projectId="project",projectRoot="root";std::uint64_t projectGeneration=1,plantRevision{},plantHash{},entity=2,ordinal{};std::array<float,3>position{},rotation{},scale{1,1,1};std::optional<std::array<double,3>>anchor;std::array<double,3>heading{0,0,1};project_planetary::SurfaceQuery query;std::optional<forest::ForestAreaDistribution>area{forest::ForestAreaDistribution{}};std::vector<std::array<double,3>>directions{{0,1,0},{0,1,0},{0,1,0}};std::vector<scene::SceneObjectSnapshot> refreshTrees;};
 struct ForestSurfacePrepared {project_planetary::Batch surface;std::vector<EditorEntity>prototype;};
 std::optional<ForestSurfacePlacement>forestSurfacePlacement;
 std::optional<std::future<ForestSurfacePrepared>>forestSurfacePending;
 struct Scheduler {unsigned calls{};bool cancel(int){++calls;return true;}}scheduler;Scheduler* taskScheduler=&scheduler;int forestSurfaceCancellation{};
 std::vector<EditorEntity>entities{EditorEntity{}};
 unsigned commits{},saves{},publishes{};bool allowCommit{true},allowPublish{true};std::vector<EditorEntity>created;
};
void push_editor_log(EditorState&,std::string_view){}
std::optional<std::size_t>editor_entity_index(const EditorState&s,std::uint64_t id){for(std::size_t i=0;i<s.entities.size();++i)if(s.entities[i].sceneObjectId==id)return i;return {};}
bool ensure_plant_lab_compiled_asset(EditorState&s){return s.compiledValid;}
bool editor_planetary_stream_identity_matches(const EditorState&s,const EditorEntity&){return s.identityCurrent;}
fs::path resolve_editor_path(const fs::path&p){return p;}
std::string editor_scene_identity(const EditorState&){return "scene";}
struct Budget {std::uint64_t cache_bytes{48ull*1024*1024};};Budget editor_planetary_mesh_budget(const EditorState&){return {};}
constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
bool publish_plant_lab_project_asset(EditorState&s){++s.publishes;return s.allowPublish;}
bool create_editor_scene_entities(EditorState&s,std::vector<EditorEntity>created,std::string_view,const std::function<bool()>&before){if(!before()||!s.allowCommit)return false;++s.commits;s.created=std::move(created);return true;}
bool save_editor_scene_snapshot(EditorState&s){++s.saves;return true;}
// The separate forest surface refresh contract exercises the actual refresh route.
void complete_forest_surface_refresh(EditorState&,const EditorState::ForestSurfacePlacement&,const project_planetary::Batch&){CHECK(false);}
ACTUAL
EditorState::ForestSurfacePrepared prepare(EditorState&s,unsigned count=3) {
 s.forestSurfacePlacement.emplace();s.forestSurfacePlacement->plantRevision=s.plantLabCompiled.sourceRevision;
 s.forestSurfacePlacement->plantHash=s.plantLabCompiled.sourceContentHash;
 EditorState::ForestSurfacePrepared ready;ready.surface.samples.resize(count);
 EditorEntity tree;tree.sceneObjectId=0;tree.name="Tree";tree.type="ForestTrunk";tree.category="ForestFactory";
 const std::array<asset::mesh::Vertex,3>vertices{{{{0,0,0}},{{1,0,0}},{{0,1,0}}}};
 const std::array<std::uint32_t,3>indices{0,1,2};const auto built=asset::mesh::build_artifact(vertices,indices);CHECK(built);
 tree.meshSnapshot=built.artifact;tree.meshAsset=scene::SceneMeshSnapshot{"Assets/Mesh/tree.epoch_mesh",built.artifact->key().words,3,3};
 ready.prototype.push_back(tree);return ready;
}
void deliver(EditorState&s,EditorState::ForestSurfacePrepared ready){std::promise<EditorState::ForestSurfacePrepared>promise;s.forestSurfacePending=promise.get_future();promise.set_value(std::move(ready));poll_forest_surface_placement(s);}
int main(int,char**) {
 for(unsigned kind=0;kind<15;++kind) {
  EditorState s;auto ready=prepare(s);
  if(kind==0)s.projectId="different";if(kind==1)s.projectLifecycle.value.generations.selected_project.generation=2;
  if(kind==2)++s.plantLabCompiled.sourceRevision;if(kind==3)s.entities[0].position[0]=1;
  if(kind==4)s.identityCurrent=false;if(kind==5)project_planetary::sourceCurrent=false;
  if(kind==6)project_planetary::meshCurrent=false;if(kind==7)ready.surface.request_id=2;
  if(kind==8)ready.surface.code=project_planetary::SurfaceCode::cancelled;if(kind==9)ready.surface.samples.pop_back();
  if(kind==10)s.projectRuntimeProcess.running=true;if(kind==11)s.projectBuildPending=true;
  if(kind==12)s.forestSurfacePlacement.reset();if(kind==13)ready.prototype.clear();
  if(kind==14)ready.surface.samples[2].radial_up={0,0,0};
  deliver(s,std::move(ready));CHECK(s.commits==0&&s.saves==0&&s.publishes==0);
  CHECK(!s.forestSurfacePending&&!s.forestSurfacePlacement);
  project_planetary::sourceCurrent=project_planetary::meshCurrent=true;
 }
 EditorState wet;auto ready=prepare(wet);wet.forestSurfacePlacement->query.settings.ocean_surface=true;wet.planetaryMeshPolicy->settings.ocean_surface=true;
 ready.surface.samples[1].height_meters=-1;deliver(wet,std::move(ready));
 CHECK(wet.commits==1&&wet.saves==1&&wet.publishes==1&&wet.created.size()==2);
 CHECK(wet.created[0].meshSnapshot==wet.created[1].meshSnapshot);
 CHECK(wet.created[0].forestRegion&&wet.created[0].forestRegion->candidate_index==0);
 CHECK(wet.created[0].forestRegion->provider_content_hash==std::string(64,'a'));
 CHECK(wet.created[1].forestRegion&&wet.created[1].forestRegion->candidate_index==2);
 CHECK(wet.created[0].forestRegion->region_id==wet.created[1].forestRegion->region_id);
 CHECK(wet.created[0].positionAnchor==std::array<double,3>{0,6371001,0});
 CHECK(wet.created[0].scale==std::array<float,3>{1,1,1});
 EditorState denied;auto rejected=prepare(denied);denied.allowPublish=false;deliver(denied,std::move(rejected));CHECK(denied.commits==0&&denied.saves==0);
}
'''
CODE=CODE.replace('TRANSFORM',transform).replace('ACTUAL',actual)
# Include all standard declarations before importing MSVC modules.
extra_includes='\n'.join(re.findall(r'^#include .*$',CODE,re.M))
CODE=re.sub(r'^#include .*\n','',CODE,flags=re.M)
CODE=extra_includes+'\n'+CODE

class ForestAreaCompletion(unittest.TestCase):
    def test_actual_completion_admission_and_one_commit(self):
        run_forest_cpu_contract(self, CODE, 'forest-area-completion', 'ForestAreaCompletion',
                                'forest.area_completion_contract.cpp')

if __name__ == "__main__":unittest.main()
