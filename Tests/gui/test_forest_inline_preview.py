"""Execute actual inline Forest projection/publication/camera code with real forest modules."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]


class ForestInlinePreview(unittest.TestCase):
    def test_preview_is_separate_and_edits_undo_restore_camera(self):
        if os.name!='nt':self.skipTest('Installed MSVC module consumer lane')
        production=(ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(encoding='utf-8')
        workspace=(ROOT/'Engine/src/editor/application/editor.frame_workspace_docking.inl').read_text(encoding='utf-8')
        functions='\n'.join(definition(production, marker, marker.startswith('struct ')) for marker in (
            '[[nodiscard]] bool frame_editor_scene_target(',
            '[[nodiscard]] bool ensure_plant_lab_compiled_asset(',
            '[[nodiscard]] std::vector<EditorEntity>\n            project_compiled_forest_entities(',
            '[[nodiscard]] bool ensure_forest_factory_preview(',
            'void focus_forest_factory_preview(',
            '[[nodiscard]] std::shared_ptr<const previewgrid::MeshPaintView> forest_paint_view(',
            'struct EditorPreviewMeshCacheUsage',
            '[[nodiscard]] std::optional<EditorPreviewMeshCacheUsage> editor_preview_mesh_cache_usage(',
            '[[nodiscard]] bool restore_editor_preview_mesh(',
            'void publish_editor_preview_markers(',
            '[[nodiscard]] bool painted_tree_uses_plant_lab(',
            'struct PaintedTreeUpdateResult final',
            '[[nodiscard]] PaintedTreeUpdateResult refresh_forest_factory_instances('))
        camera=workspace[workspace.index('const bool changedSurface ='):workspace.index('            if (changedSurface)\n                editor.surfaceSettleFrames')]
        preview=(ROOT/'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
        camera_owner=(ROOT/'Engine/src/render/render.preview_camera.cpp').read_text(encoding='utf-8')
        precise=definition(preview,'    export struct PreciseMeshTransform final',True).replace('export ','')
        precise+='\n'+definition(preview,'    export struct CloudVolume final',True).replace('export ','')
        precise+='\n'+definition(camera_owner,'    [[nodiscard]] bool valid_precise_mesh_transform(')
        precise+='\n'+definition(camera_owner,'    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(')
        camera_helpers=definition(preview,'    export [[nodiscard]] inline std::array<double,3> camera_world_eye(').replace('export ','')
        camera_helpers+='\n'+definition(preview,'    export [[nodiscard]] inline Vec3 camera_forward(').replace('export ','')
        world_position=definition((ROOT/'Engine/modules/scene.snapshot.ixx').read_text(encoding='utf-8').replace('anchor = {}','anchor'),
            '    [[nodiscard]] inline std::optional<std::array<double, 3>> world_position(')
        input_reserved=definition((ROOT/'Engine/src/editor/application/editor.application_document_editing.inl').read_text(encoding='utf-8'),
            '        [[nodiscard]] bool editor_project_viewport_input_reserved(')
        paint_types=preview[preview.index("    // Presentation-only paint colors."):preview.index("    export struct SolidDrawMarker final")].replace("export ","")
        forest_region=definition((ROOT/"Engine/modules/scene.snapshot.ixx").read_text(encoding="utf-8"), "    struct SceneForestRegionInstance final",True)
        code=r'''
#include "core.format_text.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
import editor.application;
import forest.factory;
import forest.preview_mesh;
import asset.mesh_artifact;
import asset.cloud_volume;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Forest inline contract failed line %d\n",line);std::exit(86);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
namespace epochengine {
using EditorMainSurface=EditorApplicationSurface;
namespace core {enum class ScenePreviewMode {None,Editor};enum class ContextType{OpenGL,Other};struct Context {
 ContextType type{ContextType::OpenGL};
 struct Viewport {int width=800,height=600;};Viewport scene_viewport()const{return {};}
};}
namespace render_arcade {struct Node{std::string_view name;};constexpr Node kScreenSceneNode{"screen"};}
namespace project_planetary {
 struct Settings {std::string mode;};
 struct SourceGeneration{};struct MeshGenerationIdentity{};
 bool source_generation_is_current(const std::filesystem::path&,const SourceGeneration&){assert(false);return false;}
 bool mesh_generation_is_current(const std::filesystem::path&,const SourceGeneration&,const Settings&,const MeshGenerationIdentity&){assert(false);return false;}
 struct SceneMeshBudget {asset::mesh::Limits artifact{0,0,0};std::uint64_t cache_bytes=16ull*1024*1024,frame_indices=196608u,frame_bytes=16ull*1024*1024;};
 bool supports_exploration(const Settings&){assert(false);return false;}
 std::optional<Settings> load_settings(const std::filesystem::path&){assert(false);return {};}
}
namespace render_camera {enum class ProjectionKind {perspective,orthographic};}
namespace lighting {enum class LightKind{Directional,Point};struct Point{float x{},y{},z{};};struct Light{struct Desc{LightKind kind{};Point position;}desc;};struct Frame{std::vector<Light>lights;};}
namespace visuals {int forest_trunk(){return 0;}int forest_foliage_cluster(){return 1;}}
namespace previewgrid {
 struct Vec3 {float x{},y{},z{};};
 Vec3 visual_rgb(int slot){return slot==0?Vec3{0.35f,0.19f,0.08f}:Vec3{0.2f,0.6f,0.1f};}
 enum ObjectPreviewPrimitive { Cube,IndexedMesh };
 struct MeshBaseColorBinding{};ACTUAL_PRECISE
 ACTUAL_PAINT_TYPES
 struct ObjectMarker {Vec3 position,color,scale,rotationDegrees;float radius{};ObjectPreviewPrimitive primitive{};bool selected{},editorOnly{},sampledRenderSurface{};std::shared_ptr<const asset::mesh::Artifact> mesh;bool wireframe{};std::shared_ptr<const MeshBaseColorBinding>baseColorTexture;std::optional<PreciseMeshTransform>preciseTransform;std::shared_ptr<const MeshPaintView>paintView;};
 struct Camera{Vec3 eye{},target{0,0,-1};render_camera::ProjectionKind projection{render_camera::ProjectionKind::perspective};struct {std::array<double,3>eye{};Vec3 forward{0,0,-1};}navigation;bool navigationValid{};};
 Camera camera_for(const core::Context*){return {};}
 Vec3 subtract(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}Vec3 normalize(Vec3 a){return a;}
 ACTUAL_CAMERA_HELPERS
 Camera fit_visible_depth_range(Camera value,std::span<const ObjectMarker>,double=0.0){assert(false);return value;}
 unsigned publications{},lightingSets{},clears{};
 struct CameraRigSnapshot {bool valid{true};int value{};};
 int cameraValue=5;std::vector<ObjectMarker> observed;
 enum class CameraMode{Editor,FPS};CameraMode mode{CameraMode::Editor};
 CameraMode camera_mode_for(const void*){return mode;}
 void set_camera_mode(const void*,CameraMode value){mode=value;}
 CameraRigSnapshot capture_camera_rig_snapshot(const void*){return {true,cameraValue};}
 bool restore_camera_rig_snapshot(const void*,CameraRigSnapshot value){cameraValue=value.value;return true;}
 bool focus_camera(const void*,Vec3,float){cameraValue=42;return true;}
 bool focus_camera_precise(const void*,const std::array<double,3>&,float){cameraValue=42;return true;}
 void clear_object_markers(const core::Context*){++clears;observed.clear();}
 void clear_lighting_frame(const core::Context*){}
 void set_lighting_frame(const core::Context*,const lighting::Frame&){++lightingSets;}
 struct MeshFrameLimits {std::uint64_t maximum_indices=196608u,maximum_bytes=16ull*1024*1024;};
 struct SceneFrameRequest {std::vector<ObjectMarker>markers;Camera camera;lighting::Frame lighting;MeshFrameLimits limits;std::array<double,3>render_origin;std::vector<CloudVolume>clouds;double depth_fit_aspect{};};
 bool set_object_markers(const core::Context*,std::vector<ObjectMarker> value,MeshFrameLimits limits){++publications;assert(limits.maximum_indices==196608u&&limits.maximum_bytes==16ull*1024*1024);observed=std::move(value);return true;}
 bool publish_scene_frame(const core::Context*,const std::vector<ObjectMarker>&,Camera,const lighting::Frame&,MeshFrameLimits){assert(false);return false;}
 bool frame_initial_orbit_mesh(const core::Context*,const ObjectMarker&,float){assert(false);return false;}
}
namespace scene {bool valid_scene_asset_path(std::string_view value){return value.starts_with("Assets/");}ACTUAL_FOREST_REGION
using Vec3=std::array<float,3>;struct SceneMeshCoordinates {double meters_per_unit{};std::array<double,3>source_origin_meters{};};ACTUAL_WORLD_POSITION}
struct EditorEntity {
 std::string name,type,category;std::array<float,3> position{},rotation{},scale{1,1,1};
 bool visible{true},editorOnly{};int sceneObjectId{};
 struct Mesh {std::uint64_t vertex_count{},index_count{};std::array<std::uint64_t,4> artifact_key{};std::string logical_path;std::optional<scene::SceneMeshCoordinates>coordinates{};};
 std::optional<scene::SceneForestRegionInstance> forestRegion;
 std::optional<int> textureMaterial;
 std::optional<Mesh> meshAsset;bool meshLoadAttempted{};std::shared_ptr<const asset::mesh::Artifact> meshSnapshot;std::shared_ptr<const previewgrid::MeshBaseColorBinding>meshBaseColorTexture;
 std::optional<std::array<double,3>>positionAnchor{};
 struct MeshFrame {double meters_per_unit{};std::array<double,3>source_origin_meters{};};std::optional<MeshFrame>meshSnapshotFrame{};
};
EditorEntity editor_entity_from_seed(const EditorSceneSeedEntity& seed){return {seed.name,seed.type,seed.category,seed.position,seed.rotation,seed.scale,seed.visible,seed.editor_only};}
struct Oscillator {bool enabled{};int entity_id{},axis{};float frequency_hz{},phase_radians{},amplitude{};std::array<float,3> base_position{};};
namespace scene {std::optional<std::array<float,3>>mesh_render_scale(std::array<float,3>value,const std::optional<EditorEntity::Mesh>&){return value;}}
namespace water::surface {unsigned builds{};int default_body(){return 0;}struct Result{std::shared_ptr<const asset::mesh::Artifact>artifact;explicit operator bool()const{return bool(artifact);}};Result build(int,double){++builds;assert(false);return {};}}
namespace project_motion_track {struct Binding{std::uint64_t project_generation{},scene_id{};int entity_id{};std::string project_id;};struct Sample{std::array<double,3>offset{};explicit operator bool()const{return false;}};Sample sample_translation(int,const Binding&,double){assert(false);return {};}}
unsigned lightingRebuilds{},restores{};
struct Lighting {lighting::Frame build_frame(){return {};}};
struct EditorState {
 EditorMainSurface mainSurface{EditorMainSurface::Scene};core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};
 forest::ForestAssetDocument plantLabDocument;forest::CompiledForestAsset plantLabCompiled;
 bool plantLabDocumentInitialized{};std::uint64_t plantLabCompiledRevision{},plantLabCompiledContentHash{};
 std::string plantLabStatus,plantLabAssetStatus;std::vector<EditorEntity> entities,forestPreviewEntities;
 std::uint64_t forestPreviewRevision{},forestPreviewContentHash{};
 std::optional<previewgrid::CameraRigSnapshot> forestProjectCamera,assetGenerationPreviousCamera;
 std::optional<previewgrid::CameraRigSnapshot> forestDesignCamera;
 std::optional<previewgrid::Camera> forestPlacementCamera;bool forestTerrainPainting{};
 std::optional<int>forestSurfacePending;int selectedEntityId{};
 bool sceneInteractionDirty{},planetaryInitialFramePending{};std::uint64_t planetaryInitialFrameGeneration{};
 std::string projectRoot,projectId,projectStatus,projectTextureStatus;
 std::optional<int>forestProfileSliderDraft,planetaryPreviewPending;
 std::shared_ptr<const previewgrid::SceneFrameRequest>planetaryPreviewRequest;
 struct {struct {struct {struct {std::uint64_t generation{};}selected_project;}generations;}value;
  const auto& evidence()const{return value;}}projectLifecycle;
 bool covered{},viewportMeshWireframe{};std::optional<bool>viewportMeshWireframeRequest;
 struct ProcessPort{bool running{};bool valid()const{return running;}}projectRuntimeProcess;
 std::optional<int>projectBuildPending{};
 struct PlanetaryMeshPolicy {project_planetary::SourceGeneration source;project_planetary::Settings settings;std::optional<project_planetary::MeshGenerationIdentity>generated_mesh;};
 std::optional<PlanetaryMeshPolicy>planetaryMeshPolicy;
 struct CloudLayer {std::shared_ptr<const asset::mesh::Artifact>artifact;std::array<double,3>center_meters{};double radius_meters{},wind_period_seconds{};std::optional<asset::cloud_volume::Recipe>density{};};
 std::optional<CloudLayer>planetaryStreamCloud;int planetaryStreamEntity{};std::shared_ptr<const asset::mesh::Artifact>planetaryStreamSnapshot;
 struct{double playhead_seconds{};}timelineState;
 std::shared_ptr<const asset::mesh::Artifact>temporalWaterMesh;double temporalWaterSampleTime{};
 std::string planetaryCheckedRoot;bool planetaryAttachmentPending{};std::map<int,int>scriptTranslationTracks;
 struct Document{struct Identity{struct{std::uint64_t value{};}scene;};Identity identity()const{return {};}}sceneDocument;
 Lighting sceneLighting;std::map<int,Oscillator> scriptCubeOscillators;Oscillator scriptOscillator;
 struct {double simulated_seconds{};} timeSnapshot;
};
bool reopen_plant_lab_project_asset(EditorState&,bool,bool* missing=nullptr){if(missing)*missing=true;return false;}
project_planetary::SceneMeshBudget editor_planetary_mesh_budget(const EditorState&){return {};}
bool align_editor_entity_group_to_surface(std::vector<EditorEntity>& values,float){return !values.empty();}
// Material restore/composition has an independent real-owner contract.
bool bind_plant_preview_material(EditorState&,std::vector<EditorEntity>&,const forest::ForestFactoryProfile&profile){assert(profile.barkTexture.empty()&&profile.foliageTexture.empty());return true;}
bool editor_project_viewport_covered(const EditorState&s){return s.covered;}
ACTUAL_INPUT_RESERVED
void rebuild_editor_lighting(EditorState&){++lightingRebuilds;}
bool restore_editor_mesh_base_color(const core::Context*,EditorState&,EditorEntity&){++restores;return true;}
void refresh_editor_planetary_body_inventory(EditorState&,const EditorEntity&){assert(false);}
bool editor_seed_cameras_unchanged(const std::vector<EditorSceneSeedEntity>&,const std::vector<EditorSceneSeedEntity>&){assert(false);return false;}
struct Focus{previewgrid::Vec3 target;float radius{};};std::optional<Focus>editor_planetary_body_focus(const EditorState&,const EditorEntity&){assert(false);return {}; }
bool is_screen_space_gui_entity(const EditorEntity&){return false;}
bool editor_entity_selected(const EditorState&,int id){return id==999;}
previewgrid::Vec3 selection_color_for_entity(const EditorEntity&,bool){return {1,1,1};}
float marker_radius_for_entity(const EditorEntity&){return 1;}
previewgrid::ObjectPreviewPrimitive preview_primitive_for_entity(const EditorEntity&){return previewgrid::Cube;}
bool marker_selected_for_entity(const EditorEntity&,bool selected){return selected;}
bool is_plant_lab_preview_entity(const EditorEntity& entity){return entity.category=="PlantLabPreview";}
void push_editor_log(EditorState&,std::string_view){}
std::filesystem::path resolve_editor_path(const std::filesystem::path& path){return path;}
std::vector<EditorSceneSeedEntity> editor_seed_entities_for_project(std::string_view){assert(false);return {};}

constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/tree.epoch_forest";
bool has_open_project(const EditorState&s){return !s.projectRoot.empty();}
bool editor_planetary_stream_identity_matches(const EditorState&,const EditorEntity&){assert(false);return false;}
std::optional<std::array<double,3>>verified_forest_surface_anchor(const EditorEntity&,const scene::SceneForestRegionInstance&,const EditorState::PlanetaryMeshPolicy&){assert(false);return {};}
// Recording boundary only: this fixture verifies marker construction, not the
// Scheduler/async commit implementation exercised by runtime-preview contracts.
unsigned pumpCalls{};std::optional<previewgrid::SceneFrameRequest>lastPumpRequest;
void reset_planetary_preview_publication(EditorState& state){
 state.planetaryPreviewRequest.reset();state.planetaryPreviewPending.reset();}
bool pump_editor_planetary_preview(const core::Context*,EditorState&,previewgrid::SceneFrameRequest request,bool){
 ++pumpCalls;lastPumpRequest=request;++previewgrid::publications;previewgrid::observed=std::move(request.markers);return true;}
unsigned transactions{},saves{};bool saving=true;
bool persist_forest_scene_mesh(EditorState&,std::vector<EditorEntity>&v){v[0].meshAsset=EditorEntity::Mesh{};v[0].textureMaterial=42;return true;}
bool publish_plant_lab_project_asset(EditorState&){return true;}
bool save_editor_scene_snapshot(EditorState&){++saves;return saving;}
bool synchronize_editor_scene_category(EditorState&s,std::string_view category,std::vector<EditorEntity>next,std::string_view,std::string_view){
 ++transactions;std::erase_if(s.entities,[&](auto&e){return e.category==category;});s.entities.insert(s.entities.end(),next.begin(),next.end());return true;}

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
 assert(state.forestPreviewEntities.size()==1&&previewgrid::observed.size()==1);
 assert(forest::preview_mesh::run_contract().passed());
 const auto beforeBuildPublication=previewgrid::publications,beforeBuildLighting=lightingRebuilds;
 state.projectBuildPending=7;publish_editor_preview_markers(ctx.get(),state);
 assert(previewgrid::publications==beforeBuildPublication&&lightingRebuilds==beforeBuildLighting);
 state.projectBuildPending.reset();
 const auto complete=forest::preview_mesh::build(state.plantLabCompiled.graph,state.plantLabCompiled.sample);
 assert(complete.valid&&complete.segmentCount==state.plantLabCompiled.sample.segments.size());
 assert(previewgrid::observed.front().primitive==previewgrid::IndexedMesh);
 const auto mesh=previewgrid::observed.front().mesh;
 assert(mesh&&mesh->indices().size()==complete.indices.size()&&mesh->vertices().size()==complete.vertices.size());
 for(const auto& vertex:mesh->vertices())for(float value:vertex.position)assert(value>=-0.50001f&&value<=0.50001f);
 const auto firstKey=mesh->key();
 assert(state.entities.size()==1&&state.entities.front().name==original.name&&state.entities.front().position==original.position);
 for(const auto& marker:previewgrid::observed)assert(marker.position.x!=50&&!marker.selected&&marker.editorOnly);
 // Covered Play must retain the previous complete authoring packet and do
 // no lighting, mesh/texture residency or water work, even after authoring edits.
 const auto oldPublication=previewgrid::publications,oldLighting=lightingRebuilds,oldRestores=restores;
 const auto oldMesh=previewgrid::observed.front().mesh;
 const auto oldCompiled=state.plantLabCompiled.sourceContentHash;
 state.covered=true;state.projectRuntimeProcess.running=true;state.viewportMeshWireframeRequest=true;
 const auto coveredHeight=state.plantLabDocument.profile.config.targetHeightMeters;
 assert(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,coveredHeight+0.25f));
 for(unsigned frame=0;frame<32;++frame)publish_editor_preview_markers(ctx.get(),state);
 assert(previewgrid::publications==oldPublication&&lightingRebuilds==oldLighting&&restores==oldRestores&&water::surface::builds==0);
 assert(previewgrid::observed.front().mesh==oldMesh&&state.plantLabCompiled.sourceContentHash==oldCompiled);
 assert(state.viewportMeshWireframeRequest&&*state.viewportMeshWireframeRequest&&!state.viewportMeshWireframe);
 state.covered=false;state.projectRuntimeProcess.running=false;publish_editor_preview_markers(ctx.get(),state);
 assert(previewgrid::publications==oldPublication+1&&lightingRebuilds==oldLighting+1&&state.viewportMeshWireframe);
 assert(!state.viewportMeshWireframeRequest&&state.plantLabCompiled.sourceContentHash!=oldCompiled);
 assert(forest::undo_forest_profile_edit(state.plantLabDocument));publish_editor_preview_markers(ctx.get(),state);
 const auto firstHash=state.forestPreviewContentHash;
 const auto firstHeight=state.plantLabDocument.profile.config.targetHeightMeters;
 assert(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,firstHeight+0.5f));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.forestPreviewContentHash!=firstHash&&state.plantLabCompiled.sourceContentHash==state.forestPreviewContentHash);
 assert(previewgrid::observed.front().mesh->key()!=firstKey);
 assert(state.entities.size()==1&&state.entities.front().position==original.position);
 assert(forest::undo_forest_profile_edit(state.plantLabDocument));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.plantLabDocument.profile.config.targetHeightMeters==firstHeight);
 assert(forest::redo_forest_profile_edit(state.plantLabDocument));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.plantLabDocument.profile.config.targetHeightMeters==firstHeight+0.5f);
 // Exercise a larger admitted tree through the real authoring edit. Its
 // complete branch/canopy mesh must survive the old 32/28 display caps.
 assert(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::BranchLevels,5));
 publish_editor_preview_markers(ctx.get(),state);
 const auto dense=forest::preview_mesh::build(state.plantLabCompiled.graph,state.plantLabCompiled.sample);
 assert(dense.valid&&dense.segmentCount>32&&dense.leafCardCount>28);
 assert(dense.segmentCount==state.plantLabCompiled.sample.segments.size());
 assert(previewgrid::observed.size()==1&&previewgrid::observed.front().mesh->indices().size()==dense.indices.size());
 auto invalid=state.plantLabCompiled;invalid.sample.segments.front().radius_end_meters=0;
 assert(project_compiled_forest_entities(invalid,"BadPlant","PlantLabPreview",true).empty());
 assert(forest::undo_forest_profile_edit(state.plantLabDocument));
 publish_editor_preview_markers(ctx.get(),state);
 assert(state.entities.size()==1&&state.entities.front().position==original.position);
 change_surface(state,EditorMainSurface::Scene,ctx);
 publish_editor_preview_markers(ctx.get(),state);
 assert(!state.forestProjectCamera&&previewgrid::cameraValue==5);
 assert(state.forestDesignCamera&&state.forestDesignCamera->value==42&&!state.forestPlacementCamera);
 assert(previewgrid::observed.size()==1&&previewgrid::observed.front().position.x==50&&previewgrid::observed.front().selected);
 // A hidden 3D authoring scene may contain both animated Water and an unread
 // planetary asset. Neither may sample/load behind the child viewport.
 EditorState hidden;hidden.covered=true;hidden.projectRuntimeProcess.running=true;
 hidden.entities.push_back({"Ocean","Water","World"});
 hidden.entities.push_back({"Unread","IndexedMesh","Planetary Terrain"});
 hidden.entities.back().meshAsset=EditorEntity::Mesh{3,3,{},"must-not-read.epoch_mesh"};
 const auto scenePublications=previewgrid::publications,sceneLighting=lightingRebuilds,sceneRestores=restores;
 for(unsigned frame=0;frame<32;++frame)publish_editor_preview_markers(ctx.get(),hidden);
 assert(previewgrid::publications==scenePublications&&lightingRebuilds==sceneLighting&&restores==sceneRestores);
 assert(!hidden.entities.back().meshLoadAttempted&&water::surface::builds==0&&previewgrid::observed.front().position.x==50);
 hidden.entities={original};hidden.covered=false;hidden.projectRuntimeProcess.running=false;publish_editor_preview_markers(ctx.get(),hidden);
 assert(previewgrid::publications==scenePublications+1&&lightingRebuilds==sceneLighting+1);
 // Painting is a view of the existing terrain; it must not rewrite or hide authored entities.
 EditorState painting;painting.mainSurface=EditorMainSurface::ForestFactory;painting.forestTerrainPainting=true;painting.viewportMeshWireframe=true;
 EditorEntity terrain;terrain.sceneObjectId=8;terrain.type="IndexedMesh";terrain.category="Planetary Terrain";
 terrain.meshSnapshot=mesh;terrain.meshAsset=EditorEntity::Mesh{mesh->vertices().size(),mesh->indices().size(),mesh->key().words,"Assets/Mesh/planet.epoch_mesh",scene::SceneMeshCoordinates{2,{1000,2000,3000}}};terrain.meshLoadAttempted=true;
 EditorEntity tree;tree.sceneObjectId=999;tree.category="ForestFactory";tree.type="ForestTrunk";
 scene::SceneForestRegionInstance region;region.region_id=3;region.terrain_object_id=8;region.plant_genome=1;region.plant_revision=1;region.plant_hash=1;
 region.candidate_count=1;region.radius_meters=10;region.density_per_square_meter=.1;region.body_radius_meters=100;
 region.center_direction={0,1,0};region.body_local_root_meters={0,100,0};region.admitted_body_center_meters={1000,2000,3000};
 region.terrain_artifact_key=mesh->key().words;region.provider_content_hash=std::string(64,'a');region.recipe="recipe";region.plant_source="Assets/Forest/tree.epoch_forest";
 assert(region.valid());tree.forestRegion=region;
 painting.entities={terrain,tree,tree,original};
 // Admit the real immutable cloud mesh through the normal terrain-snapshot
 // identity. It must not obscure the simplified region painting surface.
 const auto cloudBuilt=asset::mesh::build_artifact(mesh->vertices(),mesh->indices());assert(cloudBuilt);
 const auto cloud=cloudBuilt.artifact;assert(cloud!=mesh);
 painting.planetaryStreamEntity=terrain.sceneObjectId;painting.planetaryStreamSnapshot=mesh;
 painting.planetaryStreamCloud=EditorState::CloudLayer{cloud,{1000,2000,3000},100,100};
 const auto retainedCloud=painting.planetaryStreamCloud;
 const auto before=painting.entities;publish_editor_preview_markers(ctx.get(),painting);
 assert(previewgrid::observed.size()==1&&previewgrid::observed.front().paintView);
 const auto& painted=previewgrid::observed.front();assert(!painted.wireframe&&!painted.baseColorTexture&&painted.mesh==mesh);
 assert(painting.viewportMeshWireframe&&painting.entities.size()==before.size());
 for(std::size_t i=0;i<before.size();++i)assert(painting.entities[i].visible==before[i].visible&&painting.entities[i].position==before[i].position);
 assert(painted.paintView->regions.size()==1); // one cap, not one per instance
 assert(painting.planetaryStreamCloud&&painting.planetaryStreamCloud->artifact==cloud
  &&painting.planetaryStreamCloud->center_meters==retainedCloud->center_meters
  &&painting.planetaryStreamCloud->radius_meters==retainedCloud->radius_meters
  &&painting.planetaryStreamCloud->wind_period_seconds==retainedCloud->wind_period_seconds
  &&painting.planetaryStreamSnapshot==mesh);
 auto sample=previewgrid::mesh_paint_albedo(*painted.paintView,{0,50,0});assert(sample.x==.95f&&sample.y==.67f);
 sample=previewgrid::mesh_paint_albedo(*painted.paintView,{0,-50,0});assert(sample.x==.34f); // rear cap stays shaded
 sample=previewgrid::mesh_paint_albedo(*painted.paintView,{0,500,0});assert(sample.x==.34f); // another body rejected
 sample=previewgrid::mesh_paint_albedo(*painted.paintView,{50,0,0});assert(sample.x==.34f);
 painting.entities[1].forestRegion->terrain_artifact_key[0]^=1;
 painting.entities[2].forestRegion.reset();
 assert(forest_paint_view(painting,painting.entities[0])->regions.empty()); // stale identity never paints
 painting.entities={terrain,original};painting.forestTerrainPainting=false;painting.mainSurface=EditorMainSurface::Scene;
 publish_editor_preview_markers(ctx.get(),painting);assert(previewgrid::observed.size()==3);
 assert(!previewgrid::observed.front().paintView&&previewgrid::observed.front().wireframe);
 assert(previewgrid::observed.back().mesh==cloud&&previewgrid::observed.back().preciseTransform);
 const auto initialCloudTransform=*previewgrid::observed.back().preciseTransform;
 painting.timelineState.playhead_seconds=25;publish_editor_preview_markers(ctx.get(),painting);
 assert(previewgrid::observed.size()==3&&previewgrid::observed.back().mesh==cloud);
 assert(previewgrid::observed.back().preciseTransform->rotation!=initialCloudTransform.rotation);
 painting.timelineState.playhead_seconds=0;publish_editor_preview_markers(ctx.get(),painting);
 assert(previewgrid::observed.back().preciseTransform->rotation==initialCloudTransform.rotation);
 assert(previewgrid::observed.back().preciseTransform->position==initialCloudTransform.position);
 // A stale terrain lease must still suppress the cloud in the ordinary view.
 painting.planetaryStreamSnapshot.reset();publish_editor_preview_markers(ctx.get(),painting);
 assert(previewgrid::observed.size()==2&&painting.planetaryStreamCloud->artifact==cloud);
 painting.planetaryStreamSnapshot=mesh;painting.mainSurface=EditorMainSurface::ForestFactory;
 painting.forestTerrainPainting=false;publish_editor_preview_markers(ctx.get(),painting);
 assert(previewgrid::observed.size()==1&&previewgrid::observed.front().mesh!=cloud);
 assert(painting.planetaryStreamCloud->artifact==cloud&&painting.entities.size()==2);
 EditorState refresh;refresh.projectRoot="project";refresh.entities={terrain,tree,original};
 refresh.entities[1].sceneObjectId=1234;refresh.entities[1].forestRegion=region;refresh.entities[1].position={3,4,5};refresh.entities[1].rotation={2,3,4};refresh.entities[1].positionAnchor=std::array<double,3>{1e8,2e8,3e8};
 const auto refreshBefore=refresh.entities[1];refresh.projectBuildPending=1;
 assert(!refresh_forest_factory_instances(refresh)&&transactions==0);refresh.projectBuildPending.reset();
 assert(refresh_forest_factory_instances(refresh)&&transactions==1&&saves==1);
 const auto found=std::find_if(refresh.entities.begin(),refresh.entities.end(),[](auto&e){return e.sceneObjectId==1234;});assert(found!=refresh.entities.end());
 assert(found->position==refreshBefore.position&&found->rotation==refreshBefore.rotation&&found->scale==refreshBefore.scale&&found->positionAnchor==refreshBefore.positionAnchor);
 assert(found->forestRegion->region_id==region.region_id&&found->forestRegion->center_direction==region.center_direction);
 assert(found->textureMaterial==42&&found->meshSnapshot&&found->forestRegion->plant_revision==refresh.plantLabCompiled.sourceRevision);
 assert(refresh.entities.size()==3&&refresh.entities[0].sceneObjectId==terrain.sceneObjectId);
 saving=false;assert(refresh_forest_factory_instances(refresh)&&transactions==2&&refresh.plantLabStatus.find("saving failed")!=std::string::npos);


}
'''.replace('FUNCTIONS',functions).replace('CAMERA\n',camera+'\n').replace('ACTUAL_PRECISE',precise).replace('ACTUAL_CAMERA_HELPERS',camera_helpers).replace('ACTUAL_WORLD_POSITION',world_position).replace('ACTUAL_INPUT_RESERVED',input_reserved).replace('ACTUAL_PAINT_TYPES',paint_types).replace('ACTUAL_FOREST_REGION',forest_region).replace('assert(', 'CHECK(')
        proof=ROOT/'Artifacts/Reports/preview-contracts-oct31/forest';proof.mkdir(parents=True,exist_ok=True)
        (proof/'consumer.cpp').write_text(code,encoding='utf-8')
        engine=(ROOT/'Engine').as_posix()
        roots=[ROOT/'Engine/modules',ROOT/'Engine/dep/EpochPlatformEngine/modules']
        found={}
        for folder in roots:
            for path in folder.glob('*.ixx'):
                match=re.search(r'^export module\s+([\w.:]+);',path.read_text(encoding='utf-8-sig'),re.M)
                if match:found[match.group(1)]=path
        implementations=[ROOT/p for p in ('Engine/src/assets/texture/asset.texture_artifact.cpp','Engine/src/forest/forest.preview_mesh.cpp',
            'Engine/src/assets/texture/asset.texture_import.cpp','Engine/src/assets/image.stb.cpp','Engine/src/platform/core.path.cpp',
            'Engine/dep/EpochPlatformEngine/src/core/core.log.cpp','Engine/dep/EpochPlatformEngine/src/core/core.time.cpp')]
        pending=re.findall(r'^import\s+([\w.]+);',code,re.M)
        for path in implementations:pending+=re.findall(r'^(?:export )?(?:import|module)\s+([\w.]+);',path.read_text(encoding='utf-8-sig'),re.M)
        selected={}
        while pending:
            name=pending.pop()
            if name in selected or name in ('std','std.compat'):continue
            self.assertIn(name,found);selected[name]=found[name]
            pending+=re.findall(r'^(?:export )?import\s+([\w.]+);',selected[name].read_text(encoding='utf-8-sig'),re.M)
        paths=lambda values:' '.join('"'+path.as_posix()+'"' for path in values)
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(EpochForestInlineProof LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(forest_inline consumer.cpp {paths(implementations)})
target_sources(forest_inline PRIVATE FILE_SET CXX_MODULES BASE_DIRS {paths(roots)} FILES {paths(selected.values())})
target_include_directories(forest_inline PRIVATE "{engine}/include" "{engine}/dep/EpochPlatformEngine/include")
target_link_libraries(forest_inline PRIVATE shell32)
target_compile_options(forest_inline PRIVATE /W3 /WX /utf-8 /UNDEBUG)
enable_testing()
add_test(NAME actual_forest_inline_preview COMMAND forest_inline)
''',encoding='utf-8')
        build=proof/'cmake'
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],
                ['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']]
        runs=[]
        (proof/'result.json').write_text(json.dumps({'passed':False,'status':'running'})+'\n',encoding='utf-8')
        for index,command in enumerate(commands):
            checked=subprocess.run(command,cwd=proof,env=dict(os.environ,EPOCH_ASSET_ROOT=str(ROOT/'Engine/resource')),capture_output=True,text=True,timeout=120)
            (proof/f'lane-{index}.log').write_text(checked.stdout+checked.stderr,encoding='utf-8')
            runs.append({'command':command,'exit':checked.returncode})
            (proof/'result.json').write_text(json.dumps({'passed':False,'runs':runs},indent=2)+'\n',encoding='utf-8')
            self.assertEqual(checked.returncode,0,checked.stdout[-7000:]+checked.stderr)
        (proof/'result.json').write_text(json.dumps({'passed':True,'runs':runs,
            'scope':'Actual publish_editor_preview_markers with real Forest/mesh modules. Recording renderer/scheduler ports, no GUI/native runtime.',
            'cloud_cases':['painting excludes admitted cloud without mutating it','normal view restores identical artifact','explicit time advances and reverses wind transform','stale terrain snapshot excludes cloud','individual plant preview excludes cloud'],
            'owner_sha256':hashlib.sha256((ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_bytes()).hexdigest(),
            'native_acceptance':False},indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':unittest.main()
