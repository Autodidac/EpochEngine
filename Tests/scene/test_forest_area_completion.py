"""Actual forest placement completion with recording project/provider ports."""
from pathlib import Path
import re
import unittest
from Tests.scene.test_forest_area import run_forest_cpu_contract
from Tests.scene.test_position_anchor import CODE as ANCHOR_CODE
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
owner=(ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(encoding='utf-8')
actual='\n'.join(definition(owner, marker) for marker in (
    '        [[nodiscard]] bool forest_area_matches_terrain_frame(',
    '        void poll_forest_surface_placement(',
    '        [[nodiscard]] const EditorEntity* selected_forest_species_target(',
    '        [[nodiscard]] bool add_plant_lab_species_to_forest('))
preview=(ROOT/'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
transform=definition(preview,'    export struct PreciseMeshTransform final',True).replace('export ','')
camera_source=(ROOT/'Engine/src/render/render.preview_camera.cpp').read_text(encoding='utf-8')
transform+='\n'+definition(camera_source,'    [[nodiscard]] bool valid_precise_mesh_transform(')+'\n'+definition(camera_source,'    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(')
CODE=ANCHOR_CODE[:ANCHOR_CODE.index('int main(')]+r'''
#include "core.format_text.hpp"
#include <future>
#include <functional>
#include <memory>
#include <span>
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
namespace previewgrid {TRANSFORM}
struct EditorEntity {
 std::uint64_t sceneObjectId{2};std::string name="Planet",type="Planet",category="Planetary Terrain";
 std::array<float,3>position{},rotation{},scale{1,1,1};bool visible{true};
 std::optional<std::array<double,3>>positionAnchor;
 std::optional<scene::SceneMeshSnapshot>meshAsset;
 std::shared_ptr<const asset::mesh::Artifact>meshSnapshot;
 std::optional<scene::SceneForestRegionInstance>forestRegion;
};
struct EditorState {
 std::string projectId="project",projectRoot="root",plantLabStatus,projectStatus;
 struct Lifecycle {struct Evidence {struct Generations {struct Project {std::uint64_t generation{1};}selected_project;}generations;}value;const Evidence& evidence()const{return value;}}projectLifecycle;
 struct Runtime {bool running{};bool valid()const{return running;}}projectRuntimeProcess;
 bool projectBuildPending{},forestPlacementFocusPending{},aiGoalForestReceipt{},compiledValid{true},identityCurrent{true};
 std::optional<bool> forestPlacementSaved;
 forest::CompiledForestAsset plantLabCompiled=forest::compile_forest_asset(forest::make_default_plant_lab_document());
 struct PlanetaryMeshPolicy {project_planetary::Settings settings;std::optional<project_planetary::Mesh>generated_mesh{project_planetary::Mesh{}};std::vector<project_planetary::Body>generated_bodies{project_planetary::Body{}};bool body_inventory_checked{true};};
 std::optional<PlanetaryMeshPolicy>planetaryMeshPolicy{PlanetaryMeshPolicy{}};
 struct ForestSurfacePlacement {std::string projectId="project",projectRoot="root";std::uint64_t projectGeneration=1,plantRevision{},plantHash{},entity=2,ordinal{},existingRegion{},sceneRevision{};std::array<float,3>position{},rotation{},scale{1,1,1};std::optional<std::array<double,3>>anchor;std::array<double,3>heading{0,0,1};project_planetary::SurfaceQuery query;std::optional<forest::ForestAreaDistribution>area{forest::ForestAreaDistribution{}};std::vector<std::array<double,3>>directions{{0,1,0},{0,1,0},{0,1,0}},brushCenters;bool findOceanShoreline{},refreshAttachments{};std::vector<scene::SceneObjectSnapshot> refreshObjects;};
 struct ForestSurfacePrepared {project_planetary::Batch surface;std::vector<EditorEntity>prototype;std::optional<std::array<double,3>>shorelineCenter;std::vector<double>groundSupports;std::vector<bool>brushInland;};
 std::optional<ForestSurfacePlacement>forestSurfacePlacement;
 std::optional<std::future<ForestSurfacePrepared>>forestSurfacePending;
 struct Scheduler {unsigned calls{};bool cancel(int){++calls;return true;}}scheduler;Scheduler* taskScheduler=&scheduler;int forestSurfaceCancellation{};
 std::vector<EditorEntity>entities{EditorEntity{}};
 std::uint64_t selectedEntityId{};std::vector<std::uint64_t> selectedEntityIds;
 std::uint64_t sceneDocumentRevision{5};
 std::string publishedPath;
 unsigned commits{},saves{},publishes{};bool allowCommit{true},allowPublish{true};std::vector<EditorEntity>created;
};
void push_editor_log(EditorState&,std::string_view){}
bool has_open_project(const EditorState&s){return !s.projectId.empty();}
std::optional<std::size_t>editor_entity_index(const EditorState&s,std::uint64_t id){for(std::size_t i=0;i<s.entities.size();++i)if(s.entities[i].sceneObjectId==id)return i;return {};}
bool ensure_plant_lab_compiled_asset(EditorState&s){return s.compiledValid;}
bool editor_planetary_stream_identity_matches(const EditorState&s,const EditorEntity&){return s.identityCurrent;}
fs::path resolve_editor_path(const fs::path&p){return p;}
std::string editor_scene_identity(const EditorState&){return "scene";}
struct Budget {std::uint64_t cache_bytes{48ull*1024*1024};};Budget editor_planetary_mesh_budget(const EditorState&){return {};}
constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
bool publish_plant_lab_project_asset(EditorState&s,const forest::ForestAssetDocument*,std::string_view path){
 ++s.publishes;s.publishedPath=path;return s.allowPublish;}
bool create_editor_scene_entities(EditorState&s,std::vector<EditorEntity>created,std::string_view,const std::function<bool()>&before){if(!before()||!s.allowCommit)return false;++s.commits;s.created=std::move(created);return true;}
bool save_editor_scene_snapshot(EditorState&s){++s.saves;return true;}
// The separate forest surface refresh contract exercises the actual refresh route.
void complete_forest_surface_refresh(EditorState&,const EditorState::ForestSurfacePlacement&,const project_planetary::Batch&,std::span<const double>){CHECK(false);}
unsigned queuedSpecies{};
bool begin_forest_surface_placement(EditorState&,const EditorEntity& planet,std::size_t,
 forest::ForestAreaDistribution area,std::optional<std::array<double,3>> direction,bool shoreline,
 const scene::SceneForestRegionInstance* region){
 CHECK(planet.sceneObjectId==region->terrain_object_id&&!direction&&!shoreline);
 CHECK(area.radiusMeters==region->radius_meters&&area.densityPerSquareMeter==region->density_per_square_meter);
 CHECK(area.seed!=region->seed);++queuedSpecies;return true;}
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
 EditorState shore;auto coastal=prepare(shore);
 auto& intent=*shore.forestSurfacePlacement;intent.findOceanShoreline=true;
 coastal.shorelineCenter=std::array<double,3>{1,0,0};
 const auto directions=forest::forest_area_directions(*intent.area,*coastal.shorelineCenter,intent.query.body.radius_meters);
 coastal.surface.samples.resize(directions.size());
 for(std::size_t i=0;i<directions.size();++i){
  auto& sample=coastal.surface.samples[i];sample.radial_up=directions[i];
  for(std::size_t axis=0;axis<3;++axis)sample.point_meters[axis]=sample.radial_up[axis]*6371001.0;
 }
 deliver(shore,std::move(coastal));CHECK(shore.commits==1&&shore.saves==1);
 CHECK(shore.created.front().forestRegion->center_direction==std::array<double,3>{1,0,0});
 EditorState stroke;auto strokeResult=prepare(stroke,4);
 stroke.forestSurfacePlacement->brushCenters={{0,1,0},{0,1,0},{1,0,0},{0,0,1}};
 stroke.forestSurfacePlacement->directions=stroke.forestSurfacePlacement->brushCenters;
 stroke.forestSurfacePlacement->query.settings.ocean_surface=true;stroke.planetaryMeshPolicy->settings.ocean_surface=true;
 strokeResult.brushInland={true,true,false,true};
 for(std::size_t i=0;i<strokeResult.surface.samples.size();++i){
  auto& sample=strokeResult.surface.samples[i];sample.radial_up=stroke.forestSurfacePlacement->directions[i];
  sample.height_meters=8.0;
  for(std::size_t axis=0;axis<3;++axis)sample.point_meters[axis]=sample.radial_up[axis]*6371008.0;
 }
 deliver(stroke,std::move(strokeResult));
 CHECK(stroke.commits==1&&stroke.saves==1&&stroke.created.size()==2);
 CHECK(stroke.created[0].forestRegion->region_id==stroke.created[1].forestRegion->region_id);
 CHECK(stroke.created[1].forestRegion->candidate_index==3);
 CHECK(stroke.created[1].forestRegion->center_direction==std::array<double,3>{0,0,1});
 EditorState mixed;auto species=prepare(mixed);auto original=wet.created.front();
 original.forestRegion->provider_content_hash=std::string(64,'b');
 original.forestRegion->recipe="previous recipe\n";
 auto& terrain=mixed.entities.front();
 terrain.meshAsset=scene::SceneMeshSnapshot{"Assets/Mesh/planet.epoch_mesh",{1,2,3,4},3,3};
 terrain.meshAsset->coordinates.emplace();
 CHECK(forest_area_matches_terrain_frame(terrain,*original.forestRegion,*mixed.planetaryMeshPolicy));
 terrain.meshAsset->artifact_key[0]=5;
 CHECK(!forest_area_matches_terrain_frame(terrain,*original.forestRegion,*mixed.planetaryMeshPolicy));
 terrain.meshAsset->artifact_key[0]=1;
 original.sceneObjectId=71;mixed.entities.push_back(original);
 mixed.forestSurfacePlacement->existingRegion=original.forestRegion->region_id;
 mixed.forestSurfacePlacement->sceneRevision=mixed.sceneDocumentRevision;
 species.surface.samples[0].point_meters={0,6371004,0};
 species.surface.samples[1].point_meters={2,6371001,0};
 species.surface.samples[2].point_meters={2.5,6371001,0};
 for (auto& sample : species.surface.samples) {
  const auto length=std::hypot(sample.point_meters[0],sample.point_meters[1]);
  sample.radial_up={sample.point_meters[0]/length,sample.point_meters[1]/length,0};
 }
 deliver(mixed,std::move(species));
 CHECK(mixed.commits==1&&mixed.saves==1&&mixed.created.size()==1);
 CHECK(mixed.forestPlacementSaved==true&&mixed.forestPlacementFocusPending);
 CHECK(mixed.entities.back().forestRegion==original.forestRegion);
 const auto& added=mixed.created.front();
 CHECK(added.forestRegion&&added.forestRegion->region_id==original.forestRegion->region_id);
 CHECK(added.forestRegion->provider_content_hash==std::string(64,'a'));
 CHECK(added.forestRegion->plant_source==mixed.publishedPath&&mixed.publishedPath.starts_with("Assets/Forest/species_"));
 CHECK(added.positionAnchor==std::array<double,3>{2,6371001,0});
 mixed.selectedEntityId=71;
 CHECK(!add_plant_lab_species_to_forest(mixed)&&!queuedSpecies);
 mixed.entities.back().forestRegion->plant_hash++;
 CHECK(add_plant_lab_species_to_forest(mixed)&&queuedSpecies==1);
 mixed.selectedEntityIds={2,71};
 CHECK(!add_plant_lab_species_to_forest(mixed)&&queuedSpecies==1);
 mixed.selectedEntityIds.clear();mixed.entities.back().visible=false;
 CHECK(!add_plant_lab_species_to_forest(mixed)&&queuedSpecies==1);
 EditorState changed;auto outdated=prepare(changed);changed.entities.push_back(original);
 changed.forestSurfacePlacement->existingRegion=original.forestRegion->region_id;
 changed.forestSurfacePlacement->sceneRevision=changed.sceneDocumentRevision++;
 deliver(changed,std::move(outdated));CHECK(changed.commits==0&&changed.saves==0&&changed.publishes==0);
 CHECK(changed.forestPlacementSaved==false&&!changed.forestPlacementFocusPending);
 EditorState missing;auto empty=prepare(missing);missing.forestSurfacePlacement->findOceanShoreline=true;
 deliver(missing,std::move(empty));CHECK(missing.commits==0&&missing.saves==0);
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
