"""Actual model review/publication, SceneDocument history and disk reopen; no GPU."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.scene.test_glb_model_import import consumer as glb_consumer
from Tests.support.cpp import dependency_include

ROOT=Path(__file__).resolve().parents[2]


def fixture():
    base=ROOT/'Engine/src/editor/application'
    state=(base/'editor.application_state.inl').read_text(encoding='utf-8')
    owner=(base/'editor.application_asset_browsing.inl').read_text(encoding='utf-8')
    transactions=(base/'editor.application_scene_transactions.inl').read_text(encoding='utf-8')
    documents=(base/'editor.application_project_documents.inl').read_text(encoding='utf-8')
    selection=(base/'editor.application_scene_selection.inl').read_text(encoding='utf-8')
    entity=definition(state,'        struct EditorEntity',True)
    review=definition(state,'            struct ModelImportReview final',True)
    target=definition(state,'            struct ModelShorelineTarget final',True)
    pose=definition(owner,'        struct ModelSurfacePose final',True)
    forest_surface=(ROOT/'Engine/modules/forest.preview_mesh.ixx').read_text(encoding='utf-8')
    surface_rotation=definition(forest_surface,'    [[nodiscard]] inline std::optional<std::array<float,3>> surface_rotation(')
    functions='\n'.join(definition(transactions,marker) for marker in (
        '        [[nodiscard]] bool project_scene_document_into_editor(',
        '        [[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(',
        '        [[nodiscard]] scene::SceneObjectId allocate_editor_scene_object_id(',
        '        [[nodiscard]] bool apply_editor_scene_transaction(',
        '        [[nodiscard]] const EditorEntity* selected_editor_reach_model(',
        '        [[nodiscard]] bool set_editor_reach_objective(',
        '        [[nodiscard]] std::optional<std::array<authoring::scene::SceneOperation, 2>> editor_surface_spawn_operations(',
        '        [[nodiscard]] bool set_editor_surface_spawn(',
        '        [[nodiscard]] bool set_editor_walk_box(',
        '        [[nodiscard]] bool detach_editor_model_from_terrain('))
    functions+='\n'+definition(documents,'        [[nodiscard]] std::string make_entity_name(')
    functions='\n'.join(definition(selection,marker) for marker in (
        '        [[nodiscard]] std::string_view editor_scene_identity(',
        '        [[nodiscard]] std::optional<std::size_t> editor_entity_index(',
        '        void normalize_editor_entity_ids(',
        '        void clear_editor_selection(',
        '        [[nodiscard]] bool editor_entity_selected(',
        '        void apply_editor_primary_selection_side_effects(',
        '        void select_editor_entity(',
        '        void add_editor_entity_selection(',
        '        void synchronize_editor_selection('))+'\n'+functions
    functions+='\n'+'\n'.join(definition(owner,marker) for marker in (
        '        [[nodiscard]] bool model_import_review_current(',
        '        [[nodiscard]] bool review_project_model(',
        '        [[nodiscard]] bool prepare_model_import_directory(',
        '        [[nodiscard]] bool retain_model_import_file(',
        '        [[nodiscard]] std::optional<ModelSurfacePose> model_surface_pose(',
        '        [[nodiscard]] bool approve_project_model_import('))
    preview=(base/'editor.application_scene_preview.inl').read_text(encoding='utf-8')
    functions+='\n'+definition(preview,'        [[nodiscard]] bool restore_editor_mesh_base_color(')
    runtime_root=ROOT/'Engine/src/epoch'
    runtime=(runtime_root/'epoch.engine_legacy.cpp').read_text(encoding='utf-8')
    runtime_scene=(runtime_root/'implementation/epoch.project_play_scene.inl').read_text(encoding='utf-8')
    def runtime_definition(source, marker):
        start=source.index(marker)
        indentation=marker[:len(marker)-len(marker.lstrip())]
        body_marker=f'\n{indentation}{{'
        body=source.index(body_marker,start)
        return source[start:body]+definition(source[body:],body_marker)
    functions+='\n'+'\n'.join(runtime_definition(runtime,marker) for marker in (
        '        [[nodiscard]] std::optional<epochengine::previewgrid::Vec3> project_play_position_for_origin(',
        '        [[nodiscard]] bool project_play_entity_has_geometry(',
        '        [[nodiscard]] std::optional<epochengine::previewgrid::ObjectMarker> project_play_marker_for_entity('))
    functions+='\n'+runtime_definition(runtime_scene,'            bool publish_project_play_markers(')
    return glb_consumer().split('int main(int argc,char** argv)')[0]+r'''
#include <charconv>
#include <cstdlib>
#include <optional>
#include <memory>
#include <span>
#include <unordered_set>
#include <unordered_map>
#include "core.format_text.hpp"
import core.sha256;
import platform.filesystem;
import project.asset_registry;
import project.texture_library;
import project.planetary_runtime;
import scene.document;
import scene.snapshot;
import scene.serializer;
import scene.persistence;
import scene.runtime;
import scene.surface_alignment;
import editor.project_textures;
import render.preview_grid;
import render.lighting;
import render.texture_artifact;
import render.canvas2d;
import render.device;
namespace epochengine {
namespace fs=std::filesystem;
using SpriteHandle=int;
namespace core{enum class ContextType{OpenGL,Other};struct Context{ContextType type{ContextType::OpenGL};};}
namespace project_planetary {
using GeneratedMeshFrame=scene::SceneMeshCoordinates;
enum class SurfaceCode{invalid,ready};
struct SurfaceQuery{struct{GeneratedMeshFrame coordinates;std::array<std::uint64_t,4>artifact_key;}mesh;
 struct{std::array<double,3>center_meters;std::uint32_t id{};}body;std::uint64_t request_id{};};
struct SurfaceResult{SurfaceCode code{};std::uint64_t request_id{};std::array<double,3>point_meters{},radial_up{};double height_meters{};};
}
namespace forest::preview_mesh{
FOREST_SURFACE_ROTATION
const asset::texture::CompiledTextureArtifact& material_atlas(unsigned){std::abort();}
bool reference_material_available(){std::abort();}
}
ENTITY
struct EditorState{
 MODEL_SHORELINE_TARGET
 REVIEW
 struct{struct{unsigned barkTileIndex{};}profile;}plantLabDocument;
 std::optional<ModelImportReview>modelImportReview;
 std::string projectRoot,projectId{"model-project"},projectStatus,meshImportPathDraft,projectTextureStatus;
 std::unique_ptr<editor_project_textures::ProjectTextureController>projectTextures;
 std::vector<EditorEntity>entities;authoring::scene::SceneDocument sceneDocument;
 scene::SceneObjectId selectedEntityId{};std::uint64_t sceneDocumentRevision{};
 std::size_t selectedEntity{},sceneDragEntity{};scene::SceneObjectId sceneDragEntityId{};
 std::vector<scene::SceneObjectId>selectedEntityIds;std::vector<int>sceneDragStartPositions;
 bool sceneDragActive{},sceneDragHasPlaneHit{},sceneMarqueeActive{},textureBrushActive{};
 int guiSelectedWidget{},guiContentEditorWidget{};
 scene::SceneTextureFilter textureFilter{};scene::SceneTextureAddress textureAddressU{},textureAddressV{};
 scene::SceneTextureAlphaMode textureAlpha{};scene::SceneTextureColorSpace textureColorSpace{};float textureAlphaCutoff{};
 std::string selectedAssetPath,selectedProjectFile,activeRuntimeScene,activeWorld;
 bool sceneInteractionDirty{},sceneLightingDirty{};
 struct{std::uint64_t frame_index{};double simulated_seconds{};}timeSnapshot;
 struct Lifecycle{struct Evidence{bool project_selected{true};struct{struct{std::uint64_t generation{1};}selected_project;}generations;}value;
  const Evidence&evidence()const{return value;}}projectLifecycle;
};
namespace project_planetary{struct SceneMeshBudget{std::uint64_t frame_indices{1'000'000},cache_bytes{64ull<<20};};}
bool editor_planetary_mesh_policy_current(const EditorState&){return true;}
project_planetary::SceneMeshBudget editor_planetary_mesh_budget(const EditorState&){return {};}
void push_editor_log(EditorState&,std::string_view){}
bool rebuild_editor_scene_document(EditorState&,std::string_view){return false;}
fs::path resolve_editor_path(const fs::path&p){return p;}
editor_project_textures::ProjectTextureController*project_texture_controller(EditorState&s){if(!s.projectTextures)s.projectTextures=std::make_unique<editor_project_textures::ProjectTextureController>(s.projectId,s.projectRoot);return s.projectTextures.get();}
using ProjectRuntimeEntity=scene_runtime::RuntimeEntity;
bool runtime_entity_is_forest_preview(const ProjectRuntimeEntity&){return false;}
bool runtime_forest_projection_admitted(const ProjectRuntimeEntity&){return true;}
previewgrid::Vec3 runtime_marker_color_for_entity(const ProjectRuntimeEntity&,bool){return {1,1,1};}
float runtime_marker_radius_for_entity(const ProjectRuntimeEntity&){return .5f;}
previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(const ProjectRuntimeEntity&){return previewgrid::ObjectPreviewPrimitive::Cube;}
constexpr int m_runtimeViewportWidth=800,m_runtimeViewportHeight=600;
bool pump_runtime_planetary_frame(const void*,previewgrid::SceneFrameRequest,bool){std::abort();}
MODEL_SURFACE_POSE
FUNCTIONS
std::string contents(const fs::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
void write(const fs::path&p,std::span<const std::byte>b){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(out);}
EditorState make_state(const fs::path&root,unsigned maximum=4096){
 fs::create_directories(root);EditorState s;s.projectRoot=fs::canonical(root).generic_string();
 scene::SceneSnapshot snapshot;snapshot.scene_id="model-scene";snapshot.project_id=s.projectId;snapshot.world_name="Model import";
 snapshot.objects.push_back({.id=1,.name="Original",.type="Cube",.category="World",.position={20,4,3}});
 authoring::scene::DocumentLimits limits;limits.maximum_active_objects=maximum;
 auto made=authoring::scene::SceneDocument::from_snapshot(snapshot,limits);CHECK(made);s.sceneDocument=std::move(made.value);
 CHECK(project_scene_document_into_editor(s));return s;
}
}
int main(int argc,char**argv){
 using namespace epochengine;CHECK(argc==2);const auto root=fs::absolute(fs::path{argv[1]});fs::create_directories(root);
 auto opaque=fixture();auto& m=opaque.json["materials"][0];for(const char*k:{"normalTexture","occlusionTexture","emissiveTexture","emissiveFactor"})m.erase(k);
 m["doubleSided"]=false;m["alphaMode"]="OPAQUE";auto&pbr=m["pbrMetallicRoughness"];
 pbr.erase("metallicRoughnessTexture");pbr["baseColorFactor"]=Json::array({1,1,1,1});pbr["metallicFactor"]=0;pbr["roughnessFactor"]=1;
 opaque.json["samplers"][0].erase("minFilter");
 const auto bytes=glb(opaque);const auto source=root/"external/model.glb";write(source,bytes);
 auto s=make_state(root/"project");s.meshImportPathDraft=source.generic_string();const auto initial=s.sceneDocument.project_snapshot();CHECK(initial);
 select_editor_entity(s,0);CHECK(s.selectedEntityIds==std::vector<scene::SceneObjectId>{1});
 CHECK(review_project_model(s,source)&&model_import_review_current(s));CHECK(s.entities.size()==1&&!fs::exists(root/"project/Assets"));
 const auto approvedSha=s.modelImportReview->sourceSha256;
 const auto junk=std::as_bytes(std::span{"changed after review",20});write(source,junk);
 const auto approved=approve_project_model_import(s);if(!approved)std::fprintf(stderr,"Import refused: %s\n",s.projectStatus.c_str());CHECK(approved&&!s.modelImportReview&&s.entities.size()==2);
 const auto& added=s.entities.back();CHECK(added.meshAsset&&added.textureMaterial&&added.meshSnapshot);
 const auto importedId=added.sceneObjectId;const auto importedGeometry=added.meshSnapshot;
 CHECK(s.selectedEntityId==importedId&&s.selectedEntityIds==std::vector<scene::SceneObjectId>{importedId});
 CHECK(!editor_entity_selected(s,1));
 CHECK(added.meshAsset->coordinates&&added.meshAsset->coordinates->meters_per_unit==1&&added.scale[0]==1);
 CHECK(added.textureMaterial->color_space==scene::SceneTextureColorSpace::srgb&&added.textureMaterial->alpha==scene::SceneTextureAlphaMode::opaque);
 CHECK(added.meshSnapshot->vertices()[1].uv[0]==1&&added.meshSnapshot->vertices()[2].uv[1]==1);
 const auto originalCopy=contents(root/"project/Assets/Models"/(approvedSha+".glb"));CHECK(originalCopy.size()==bytes.size()&&std::equal(originalCopy.begin(),originalCopy.end(),reinterpret_cast<const char*>(bytes.data())));
 const auto material=*added.textureMaterial;project_textures::TextureArtifactLibrary library{s.projectId,s.projectRoot};
 project_textures::ProjectTexturePipeline defaultPipeline{s.projectId,s.projectRoot};
 CHECK(!defaultPipeline.restore_exact(material.logical_path,{material.artifact_key})); // default linear-data contract unchanged
 core::Context gl;
 CHECK(restore_editor_mesh_base_color(&gl,s,s.entities.back()));
 const auto binding=s.entities.back().meshBaseColorTexture;CHECK(binding&&previewgrid::valid_mesh_base_color_binding(*binding));
 const auto restorationCount=s.projectTextures->pipeline().metrics().restorations;
 CHECK(restore_editor_mesh_base_color(&gl,s,s.entities.back()));CHECK(s.entities.back().meshBaseColorTexture==binding);
 CHECK(s.projectTextures->pipeline().metrics().restorations==restorationCount); // no per-frame disk restore
 core::Context unsupported{core::ContextType::Other};CHECK(!restore_editor_mesh_base_color(&unsupported,s,s.entities.back()));
 auto changed=material;changed.address_u=scene::SceneTextureAddress::repeat;s.entities.back().textureMaterial=changed;
 CHECK(restore_editor_mesh_base_color(&gl,s,s.entities.back()));CHECK(s.entities.back().meshBaseColorTexture!=binding);
 CHECK(s.entities.back().meshBaseColorTexture->sampler.address_u==AddressMode::repeat);
 s.entities.back().textureMaterial=material;CHECK(restore_editor_mesh_base_color(&gl,s,s.entities.back()));
 auto missing=material;missing.artifact_key[0]^=1u;s.entities.back().textureMaterial=missing;
 CHECK(!restore_editor_mesh_base_color(&gl,s,s.entities.back())&&!s.entities.back().meshBaseColorTexture);
 s.entities.back().textureMaterial=material;CHECK(restore_editor_mesh_base_color(&gl,s,s.entities.back()));
 CHECK(restore_editor_mesh_base_color(&gl,s,s.entities.front())&&!s.entities.front().meshBaseColorTexture);
 const auto loaded=library.load_exact(material.logical_path,{material.artifact_key});CHECK(loaded&&loaded.artifact.identity.profile.color_space==asset::texture::ColorSpace::srgb);
 CHECK(asset::mesh::read_project_artifact(s.projectRoot,added.meshAsset->logical_path,{added.meshAsset->artifact_key}));
 CHECK(s.sceneDocument.undo());CHECK(project_scene_document_into_editor(s)&&s.entities.size()==1&&s.entities.front().position[0]==20);
 CHECK(s.sceneDocument.redo());CHECK(project_scene_document_into_editor(s)&&s.entities.size()==2&&s.entities.back().textureMaterial==material);
 CHECK(!s.entities.back().meshBaseColorTexture&&restore_editor_mesh_base_color(&gl,s,s.entities.back()));
 const auto snapshot=s.sceneDocument.project_snapshot();CHECK(snapshot);const auto saved=root/"project/world.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(saved,snapshot.snapshot));const auto reopened=scene::persistence::load_scene_snapshot(saved);CHECK(reopened.result);
 auto rebuilt=authoring::scene::SceneDocument::from_snapshot(reopened.snapshot);CHECK(rebuilt&&rebuilt.value.objects().size()==2);
 auto runnable=reopened.snapshot;
 runnable.objects.push_back({.id=2,.name="Fixture Camera",.type="Camera",.category="World"});runnable.primary_camera=2;
 runnable.objects.push_back({.id=3,.name="Fixture Spawn",.type="Spawn",.category="World"});runnable.primary_spawn=3;
 scene_runtime::SceneRuntime runtime;CHECK(runtime.replace(runnable).committed());
 std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>> meshes{{importedId,importedGeometry}};
 std::unordered_map<std::uint64_t,std::shared_ptr<const previewgrid::MeshBaseColorBinding>> colors{{importedId,binding}};
 const auto runtimeEntities=runtime.projection()->entities();
 CHECK(publish_project_play_markers(&gl,runtimeEntities,&meshes,{}, {},0,false,{},&colors));
 const auto rendered=previewgrid::object_solid_frame_for(&gl);
 CHECK(std::any_of(rendered->markers.begin(),rendered->markers.end(),[](const auto&m){return m.marker.baseColorTexture&&m.textured&&!m.textured->empty();}));
 CHECK(!publish_project_play_markers(&gl,runtimeEntities,&meshes));
 CHECK(previewgrid::object_solid_frame_for(&gl)==rendered); // missing texture never paints a color-only replacement
 auto terrainMarker=rendered->markers.front().marker;
 terrainMarker.baseColorTexture.reset();
 const std::array runtimeMarkers{terrainMarker,rendered->markers.back().marker};
 CHECK(runtimeMarkers.back().baseColorTexture);
 CHECK(publish_project_play_markers(&unsupported,runtimeEntities,&meshes,{},runtimeMarkers,0,false,{},&colors));
 const auto otherFrame=previewgrid::object_solid_frame_for(&unsupported);
 CHECK(otherFrame&&otherFrame->markers.size()==2);
 CHECK(std::none_of(otherFrame->markers.begin(),otherFrame->markers.end(),[](const auto& entry){return bool(entry.marker.baseColorTexture);}));
 previewgrid::clear_object_markers(&unsupported);
 previewgrid::clear_object_markers(&gl);
 CHECK(reopened.snapshot.objects.back().texture_material);
 const auto& reopenedMaterial=*reopened.snapshot.objects.back().texture_material;
 CHECK(reopenedMaterial==material);
 CHECK(reopened.snapshot.objects.back().mesh_asset&&reopened.snapshot.objects.back().mesh_asset->coordinates);
 CHECK(reopened.snapshot.objects.back().mesh_asset->coordinates->meters_per_unit==1);
 EditorState fresh;fresh.projectId=s.projectId;fresh.projectRoot=s.projectRoot;fresh.sceneDocument=std::move(rebuilt.value);
 CHECK(project_scene_document_into_editor(fresh)&&!fresh.entities.back().meshBaseColorTexture);
 CHECK(restore_editor_mesh_base_color(&gl,fresh,fresh.entities.back()));
 CHECK(fresh.entities.back().meshBaseColorTexture->artifact.artifact()->mips[0].texels==binding->artifact.artifact()->mips[0].texels);
 auto wrongProject=make_state(root/"foreign-project");wrongProject.projectId="different-project";auto* foreignController=project_texture_controller(wrongProject);
 CHECK(!foreignController->pipeline().resources().validated_artifact(s.projectTextures->pipeline().registry(),binding->logical));
 s.projectTextures.reset();CHECK(binding->artifact&&previewgrid::valid_mesh_base_color_binding(*binding)); // shared immutable lifetime
 write(source,bytes);
 auto multipart=opaque;multipart.json["meshes"][0]["primitives"].push_back(multipart.json["meshes"][0]["primitives"][0]);
 const auto multiSource=root/"external/multipart.glb";write(multiSource,glb(multipart));
 auto multi=make_state(root/"multipart-project");multi.meshImportPathDraft=multiSource.generic_string();select_editor_entity(multi,0);
 CHECK(review_project_model(multi,multiSource)&&approve_project_model_import(multi));
 CHECK(multi.entities.size()==3&&multi.selectedEntityIds.size()==2&&!editor_entity_selected(multi,1));
 for(std::size_t part=1;part<multi.entities.size();++part)CHECK(editor_entity_selected(multi,multi.entities[part].sceneObjectId));
 CHECK(multi.sceneDocument.undo()&&project_scene_document_into_editor(multi)&&multi.entities.size()==1);
 CHECK(multi.sceneDocument.redo()&&project_scene_document_into_editor(multi)&&multi.entities.size()==3);
 auto shorelineModel=multipart;
 shorelineModel.json["nodes"][0]["translation"]=Json::array({0,-0.375,0});
 const auto shorelinePath=root/"external/beacon.glb";write(shorelinePath,glb(shorelineModel));
 auto shore=make_state(root/"shoreline-project");shore.meshImportPathDraft=shorelinePath.generic_string();
 const auto terrain=previewgrid::make_precise_mesh_transform({1e11+.25,-2e11-.125,2},{2,2,2},{0,0,90});
 CHECK(terrain);
 const auto originalShore=shore.sceneDocument.project_snapshot();CHECK(originalShore);
 auto terrainScene=originalShore.snapshot;
 auto& ground=terrainScene.objects.front();
 ground.type="IndexedMesh";ground.category="Planetary Terrain";ground.position={};
 ground.position_anchor=terrain->position;ground.rotation={0,0,90};ground.scale={2,2,2};
 ground.mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/planet.epoch_mesh",{1,2,3,4},3,3,scene::SceneMeshCoordinates{1,{100,200,300}}};
 auto terrainDocument=authoring::scene::SceneDocument::from_snapshot(terrainScene);CHECK(terrainDocument);
 shore.sceneDocument=std::move(terrainDocument.value);CHECK(project_scene_document_into_editor(shore));
 CHECK(review_project_model(shore,shorelinePath));
 CHECK(shore.projectStatus.starts_with("Model bounds ")&&shore.projectStatus.find("Walking collision is off")!=std::string::npos);
 project_planetary::SurfaceQuery query{};query.request_id=12;
 query.body.center_meters={101,200,301};query.mesh.coordinates=*ground.mesh_asset->coordinates;
 query.mesh.artifact_key=ground.mesh_asset->artifact_key;
 project_planetary::SurfaceResult land{.code=project_planetary::SurfaceCode::ready,.request_id=12,
  .point_meters={0,6371000.5,0},.radial_up={0,1,0},.height_meters=.5};
 const auto support=terrain->apply({1,6371000.5,1});
 const auto placement=model_surface_pose(shore.modelImportReview->compiled,*terrain,query,land,1);
 CHECK(placement&&std::abs(placement->pivot[0]-(support[0]-.375))<1e-6);
 CHECK(std::abs(placement->pivot[1]-support[1])<1e-6&&std::abs(placement->pivot[2]-support[2])<1e-6);
 const auto placedBasis=previewgrid::make_precise_mesh_transform({0,0,0},{1,1,1},
  {placement->rotation[0],placement->rotation[1],placement->rotation[2]});
 CHECK(placedBasis&&std::abs(placedBasis->rotation[1]+1.0)<1e-7);
 auto wet=land;wet.height_meters=-.25;
 CHECK(!model_surface_pose(shore.modelImportReview->compiled,*terrain,query,wet,1));
 ++wet.request_id;wet.height_meters=1;
 CHECK(!model_surface_pose(shore.modelImportReview->compiled,*terrain,query,wet,1));
 CHECK(approve_project_model_import(shore,&*placement)&&shore.entities.size()==3);
 for(std::size_t part=1;part<shore.entities.size();++part){
  CHECK(shore.entities[part].positionAnchor==placement->pivot&&shore.entities[part].rotation==placement->rotation);
  CHECK(shore.entities[part].terrainAttachment==placement->attachment);
  CHECK(shore.entities[part].meshAsset&&shore.entities[part].textureMaterial);}
 CHECK(shore.sceneDocument.undo()&&project_scene_document_into_editor(shore)&&shore.entities.size()==1);
 CHECK(shore.sceneDocument.redo()&&project_scene_document_into_editor(shore)&&shore.entities.size()==3);
 const auto shoreSnapshot=shore.sceneDocument.project_snapshot();CHECK(shoreSnapshot);
 const auto shoreSave=root/"shoreline-project/world.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(shoreSave,shoreSnapshot.snapshot));
 const auto shoreReopened=scene::persistence::load_scene_snapshot(shoreSave);
 CHECK(shoreReopened.result&&shoreReopened.snapshot.objects.size()==3);
 CHECK(contents(shoreSave).starts_with("epoch_snapshot 9\n"));
 for(const auto& object:shoreReopened.snapshot.objects)if(object.id!=1){
  CHECK(scene::world_position(object)==placement->pivot&&object.rotation==placement->rotation);
  CHECK(object.terrain_attachment==placement->attachment);
  CHECK(object.mesh_asset&&object.texture_material);}
 const auto attachedModel=shore.entities[1];
 CHECK(detach_editor_model_from_terrain(shore,shore.entities[1]));
 CHECK(!shore.entities[1].terrainAttachment&&shore.entities[1].positionAnchor==attachedModel.positionAnchor);
 CHECK(shore.entities[1].meshAsset==attachedModel.meshAsset&&shore.entities[1].textureMaterial==attachedModel.textureMaterial);
 CHECK(shore.entities[2].terrainAttachment==placement->attachment);
 CHECK(shore.sceneDocument.undo()&&project_scene_document_into_editor(shore));
 CHECK(shore.entities[1].terrainAttachment==placement->attachment);
 select_editor_entity(shore,1);
 const auto objectiveModel=selected_editor_reach_model(shore);CHECK(objectiveModel);
 const scene::SceneReachObjective objective{objectiveModel->sceneObjectId,7.5};
 CHECK(set_editor_reach_objective(shore,objective));
 const auto objectiveSnapshot=shore.sceneDocument.project_snapshot();CHECK(objectiveSnapshot);
 CHECK(objectiveSnapshot.snapshot.reach_objective==objective);
 CHECK(shore.sceneDocument.undo()&&!shore.sceneDocument.project_settings().reach_objective);
 CHECK(shore.sceneDocument.redo()&&shore.sceneDocument.project_settings().reach_objective==objective);
 const auto objectiveSave=root/"shoreline-project/with-objective.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(objectiveSave,objectiveSnapshot.snapshot));
 CHECK(contents(objectiveSave).starts_with("epoch_snapshot 10\n"));
 const auto objectiveReopened=scene::persistence::load_scene_snapshot(objectiveSave);
 CHECK(objectiveReopened.result&&objectiveReopened.snapshot.reach_objective==objective);
 CHECK(scene::persistence::scene_snapshot_semantically_equal(objectiveReopened.snapshot,objectiveSnapshot.snapshot));
 auto invalidObjective=objectiveReopened.snapshot;invalidObjective.reach_objective->radius_meters=0;
 CHECK(scene::serialize_snapshot_text(invalidObjective).empty());
 CHECK(set_editor_reach_objective(shore,std::nullopt)&&!shore.sceneDocument.project_settings().reach_objective);
 auto spawnScene=objectiveReopened.snapshot;
 spawnScene.objects.push_back({.id=9001,.name="PlayerStart",.type="Spawn",.category="Gameplay"});
 spawnScene.primary_spawn=9001;
 auto spawnDocument=authoring::scene::SceneDocument::from_snapshot(spawnScene);CHECK(spawnDocument);
 auto spawnEditor=make_state(root/"surface-start-project");
 spawnEditor.sceneDocument=std::move(spawnDocument.value);CHECK(project_scene_document_into_editor(spawnEditor));
 const auto modelIndex=editor_entity_index(spawnEditor,objective.target);CHECK(modelIndex);
 const auto spawnSource=spawnEditor.entities[*modelIndex];
 auto staleSource=spawnSource;++staleSource.terrainAttachment->terrain_artifact_key[0];
 CHECK(!set_editor_surface_spawn(spawnEditor,staleSource));
 CHECK(set_editor_surface_spawn(spawnEditor,spawnSource));
 const auto saveStart=spawnEditor.sceneDocument.project_snapshot();CHECK(saveStart);
 const auto startPath=root/"surface-start-project/world.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(startPath,saveStart.snapshot));
 const auto loadedStart=scene::persistence::load_scene_snapshot(startPath);CHECK(loadedStart.result);
 const auto start=std::ranges::find(loadedStart.snapshot.objects,9001u,&scene::SceneObjectSnapshot::id);
 CHECK(start!=loadedStart.snapshot.objects.end()&&loadedStart.snapshot.primary_spawn==9001);
 CHECK(!start->mesh_asset&&start->terrain_attachment==placement->attachment&&scene::world_position(*start)==placement->pivot);
 CHECK(spawnEditor.sceneDocument.undo());
 const auto handle=spawnEditor.sceneDocument.find({9001});CHECK(handle);
 CHECK(!spawnEditor.sceneDocument.object(*handle)->descriptor.terrain_attachment);
 CHECK(spawnEditor.sceneDocument.redo()&&spawnEditor.sceneDocument.object(*handle)->descriptor.terrain_attachment==placement->attachment);
 CHECK(!set_editor_walk_box(spawnEditor,staleSource,true));
 CHECK(set_editor_walk_box(spawnEditor,spawnSource,true));
 const auto solidSnapshot=spawnEditor.sceneDocument.project_snapshot();CHECK(solidSnapshot);
 const auto solidPath=root/"surface-start-project/solid.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(solidPath,solidSnapshot.snapshot));
 CHECK(contents(solidPath).starts_with("epoch_snapshot 11\n"));
 const auto solidReopened=scene::persistence::load_scene_snapshot(solidPath);
 CHECK(solidReopened.result&&scene::persistence::scene_snapshot_semantically_equal(solidSnapshot.snapshot,solidReopened.snapshot));
 const auto solidModel=std::ranges::find(solidReopened.snapshot.objects,objective.target,&scene::SceneObjectSnapshot::id);
 CHECK(solidModel!=solidReopened.snapshot.objects.end()&&solidModel->terrain_attachment->solid_walk_box);
 CHECK(spawnEditor.sceneDocument.undo()&&project_scene_document_into_editor(spawnEditor));
 CHECK(!spawnEditor.entities[*editor_entity_index(spawnEditor,objective.target)].terrainAttachment->solid_walk_box);
 CHECK(spawnEditor.sceneDocument.redo()&&project_scene_document_into_editor(spawnEditor));
 const auto flaggedModel=spawnEditor.entities[*editor_entity_index(spawnEditor,objective.target)];
 CHECK(flaggedModel.terrainAttachment->solid_walk_box&&set_editor_surface_spawn(spawnEditor,flaggedModel));
 CHECK(!spawnEditor.sceneDocument.object(*handle)->descriptor.terrain_attachment->solid_walk_box);
 CHECK(set_editor_reach_objective(spawnEditor,std::nullopt));
 const auto noObjective=spawnEditor.sceneDocument.project_snapshot();CHECK(noObjective);
 CHECK(scene::persistence::save_scene_snapshot_atomic(solidPath,noObjective.snapshot));
 CHECK(contents(solidPath).starts_with("epoch_snapshot 11\n")&&contents(solidPath).find("reach_objective none\n")!=std::string::npos);
 CHECK(scene::persistence::load_scene_snapshot(solidPath).result);
 CHECK(set_editor_walk_box(spawnEditor,flaggedModel,false));
 CHECK(scene::serialize_snapshot_text(spawnEditor.sceneDocument.project_snapshot().snapshot).starts_with("epoch_snapshot 9\n"));
 const auto landingTerrain=previewgrid::make_precise_mesh_transform(terrain->position,{1,1,1},{0,0,90});CHECK(landingTerrain);
 auto landingScene=terrainScene;landingScene.objects[0].scale={1,1,1};
 auto landingEditor=make_state(root/"landing-project");
 auto landingDocument=authoring::scene::SceneDocument::from_snapshot(landingScene);CHECK(landingDocument);
 landingEditor.sceneDocument=std::move(landingDocument.value);CHECK(project_scene_document_into_editor(landingEditor));
 landingEditor.meshImportPathDraft=shorelinePath.generic_string();CHECK(review_project_model(landingEditor,shorelinePath));
 auto startSample=land;startSample.point_meters[2]+=25;
 const auto landingPose=model_surface_pose(landingEditor.modelImportReview->compiled,*landingTerrain,query,land,1,&startSample);
 CHECK(landingPose&&landingPose->playerStart);
 CHECK(!approve_project_model_import(landingEditor,&*landingPose)&&landingEditor.entities.size()==1&&!fs::exists(root/"landing-project/Assets"));
 landingScene.objects.push_back({.id=9001,.name="PlayerStart",.type="Spawn",.category="Gameplay"});
 landingScene.primary_spawn=9001;
 landingDocument=authoring::scene::SceneDocument::from_snapshot(landingScene);CHECK(landingDocument);
 landingEditor.sceneDocument=std::move(landingDocument.value);CHECK(project_scene_document_into_editor(landingEditor));
 CHECK(review_project_model(landingEditor,shorelinePath));
 CHECK(approve_project_model_import(landingEditor,&*landingPose)&&landingEditor.entities.size()==4);
 const auto landingSnapshot=landingEditor.sceneDocument.project_snapshot();CHECK(landingSnapshot&&landingSnapshot.snapshot.reach_objective);
 CHECK(landingSnapshot.snapshot.reach_objective->radius_meters==5.0);
 const auto landingStart=std::ranges::find(landingSnapshot.snapshot.objects,9001u,&scene::SceneObjectSnapshot::id);
 CHECK(landingStart!=landingSnapshot.snapshot.objects.end()&&scene::world_position(*landingStart)==landingPose->playerStart->pivot);
 CHECK(landingStart->terrain_attachment&&!landingStart->terrain_attachment->solid_walk_box);
 unsigned solidParts{};
 for(const auto& part:landingSnapshot.snapshot.objects)if(part.terrain_attachment&&part.terrain_attachment->solid_walk_box)++solidParts;
 CHECK(solidParts==2);
 CHECK(landingEditor.sceneDocument.undo()&&landingEditor.sceneDocument.objects().size()==2
  &&!landingEditor.sceneDocument.project_settings().reach_objective);
 CHECK(landingEditor.sceneDocument.redo()&&landingEditor.sceneDocument.project_settings().reach_objective);
 const auto landingSave=root/"landing-project/world.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(landingSave,landingSnapshot.snapshot));
 CHECK(scene::persistence::scene_snapshot_semantically_equal(scene::persistence::load_scene_snapshot(landingSave).snapshot,landingSnapshot.snapshot));
 auto follow=authoring::scene::SceneDocument::from_snapshot(shoreReopened.snapshot);CHECK(follow);
 auto& document=follow.value;
 const auto parent=document.find({1});CHECK(parent);
 const auto firstId=shoreReopened.snapshot.objects[1].id;
 const auto child=document.find({firstId});CHECK(child);
 const auto initialFollowHash=document.revision().content;
 auto move=document.object(*parent)->descriptor.transform;move.position.x+=0.125f;
 CHECK(document.apply_transaction(std::array<authoring::scene::SceneOperation,1>{
  authoring::scene::ObjectTransformChangedOperation{*parent,move}},"Move terrain"));
 const auto shifted=document.project_snapshot();CHECK(shifted);
 for(const auto& object:shifted.snapshot.objects)if(object.id!=1)
  CHECK((*scene::world_position(object))[0]==placement->pivot[0]+0.125&&object.terrain_attachment==placement->attachment);
 const auto movedHash=document.revision().content;
 CHECK(document.undo()&&document.revision().content==initialFollowHash);
 CHECK(document.redo()&&document.revision().content==movedHash);
 move.scale.x=3.0f;
 CHECK(!document.apply_transaction(std::array<authoring::scene::SceneOperation,1>{
  authoring::scene::ObjectTransformChangedOperation{*parent,move}},"Reject shear"));
 CHECK(document.revision().content==movedHash);
 move=document.object(*parent)->descriptor.transform;move.position.y+=0.25f;
 auto explicitChild=document.object(*child)->descriptor.transform;explicitChild.position.y+=0.25f;
 CHECK(document.apply_transaction(std::array<authoring::scene::SceneOperation,2>{
  authoring::scene::ObjectTransformChangedOperation{*parent,move},
  authoring::scene::ObjectTransformChangedOperation{*child,explicitChild}},"Move selected parent and child"));
 const auto grouped=document.project_snapshot();CHECK(grouped);
 for(const auto& object:grouped.snapshot.objects)if(object.id!=1)
  CHECK((*scene::world_position(object))[1]==placement->pivot[1]+0.25);
 const auto parentBefore=document.object(*parent)->descriptor;
 auto replacement=parentBefore.mesh_asset;++replacement->artifact_key[0];
 CHECK(document.apply_transaction(std::array<authoring::scene::SceneOperation,1>{
  authoring::scene::GeometryComponentChangedOperation{.object=*parent,.mesh_asset=replacement,
   .position_anchor=parentBefore.position_anchor}},"Replace terrain artifact"));
 move.position.z+=1.0f;
 CHECK(document.apply_transaction(std::array<authoring::scene::SceneOperation,1>{
  authoring::scene::ObjectTransformChangedOperation{*parent,move}},"Move replacement terrain"));
 const auto staleGround=document.project_snapshot();CHECK(staleGround);
 for(const auto& object:staleGround.snapshot.objects)if(object.id!=1)
  CHECK((*scene::world_position(object))[2]==placement->pivot[2]&&object.terrain_attachment==placement->attachment);
 auto forged=staleGround.snapshot;forged.objects[1].terrain_attachment->terrain_object_id=forged.objects[1].id;
 CHECK(scene::serialize_snapshot_text(forged).empty());
 for(unsigned guard=0;guard<3;++guard){auto stale=make_state(root/("stale-"+std::to_string(guard)));stale.meshImportPathDraft=source.generic_string();CHECK(review_project_model(stale,source));
  if(guard==0)++stale.projectLifecycle.value.generations.selected_project.generation;if(guard==1)stale.projectId="other";if(guard==2)stale.meshImportPathDraft+="x";
  CHECK(!approve_project_model_import(stale)&&stale.entities.size()==1&&!fs::exists(fs::path{stale.projectRoot}/"Assets"));}
 auto denied=make_state(root/"full-scene",1);denied.meshImportPathDraft=source.generic_string();CHECK(review_project_model(denied,source));CHECK(!approve_project_model_import(denied)&&denied.entities.size()==1&&!fs::exists(root/"full-scene/Assets"));
 auto collision=make_state(root/"collision");collision.meshImportPathDraft=source.generic_string();CHECK(review_project_model(collision,source));
 const auto key=asset::texture::content_hash_hex(collision.modelImportReview->compiled.textures[0].artifact.identity.key);
 const auto png=root/"collision/Assets/Textures"/(key+".png");write(png,junk);CHECK(!approve_project_model_import(collision)&&collision.entities.size()==1&&contents(png)=="changed after review");
 CHECK(fs::canonical(root/"redirected/Library")==fs::canonical(root/"outside"));
 auto redirected=make_state(root/"redirected");redirected.meshImportPathDraft=source.generic_string();CHECK(review_project_model(redirected,source));
 CHECK(!approve_project_model_import(redirected)&&redirected.entities.size()==1&&fs::is_empty(root/"outside"));
}
'''.replace('ENTITY',entity).replace('REVIEW',review).replace('FUNCTIONS',functions) \
   .replace('MODEL_SHORELINE_TARGET',target).replace('MODEL_SURFACE_POSE',pose) \
   .replace('FOREST_SURFACE_ROTATION',surface_rotation)


class ModelProjectImport(unittest.TestCase):
    def test_actual_approval_publication_history_save_reopen_debug_release(self):
        if os.name!='nt':self.skipTest('Installed MSVC C++23 consumer')
        proof=ROOT/'Artifacts/Reports/model-project-import-oct28'/str(time.time_ns())
        build=ROOT/'Artifacts/Intermediate/tests/model-project-import'/proof.name
        proof.mkdir(parents=True);source=proof/'editor.model_import_contract.cpp';source.write_text(fixture(),encoding='utf-8')
        roots=[ROOT/'Engine/modules',ROOT/'Engine/dep/EpochPlatformEngine/modules'];module_map={}
        for folder in roots:
            for path in folder.glob('*.ixx'):
                text=path.read_text(encoding='utf-8-sig');match=re.search(r'^export module\s+([\w.:]+);',text,re.M)
                if match:module_map[match.group(1)]=path
        pending=['asset.model_import','asset.mesh_artifact','asset.texture_import','core.sha256','platform.filesystem','project.asset_registry','project.texture_library','project.planetary_runtime','scene.document','scene.serializer','scene.persistence','scene.runtime','scene.surface_alignment','editor.project_textures','render.preview_grid','authoring.texture_detail'];selected={}
        while pending:
            name=pending.pop()
            if name in selected or name in ('std','std.compat'):continue
            self.assertIn(name,module_map);selected[name]=module_map[name]
            pending+=re.findall(r'^(?:export )?import\s+([\w.]+);',selected[name].read_text(encoding='utf-8-sig'),re.M)
        implementations=[ROOT/p for p in ('Engine/src/render/render.preview_camera.cpp', 'Engine/src/render/render.preview_lighting.cpp','Engine/src/assets/texture/asset.texture_artifact.cpp','Engine/src/assets/texture/asset.texture_import.cpp','Engine/src/assets/image.stb.cpp','Engine/src/assets/mesh/asset.model_import.cpp','Engine/src/project/project.asset_registry.cpp','Engine/src/project/project.texture_library.cpp','Engine/src/project/project.texture_source.cpp','Engine/src/project/project.texture_pipeline.cpp','Engine/src/project/project.texture_resources.cpp','Engine/src/authoring/texture/authoring.texture_document.cpp','Engine/src/authoring/texture/authoring.texture_residency.cpp','Engine/src/editor/editor.project_textures.cpp','Engine/src/renderers/canvas2d/render.canvas2d_scene.cpp','Engine/dep/EpochPlatformEngine/src/core/core.env.cpp','Engine/dep/EpochPlatformEngine/src/core/core.time.cpp')]
        includes=dependency_include(ROOT)
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ModelProjectImport LANGUAGES CXX)
add_executable(model_project "{source.as_posix()}" {' '.join('"'+p.as_posix()+'"' for p in implementations)})
target_sources(model_project PRIVATE "{(ROOT/'Engine/resource/epoch.manifest').as_posix()}")
target_compile_features(model_project PRIVATE cxx_std_23)
target_compile_options(model_project PRIVATE /W3 /WX /utf-8 /EHsc)
target_include_directories(model_project PRIVATE "{includes.as_posix()}" "{(ROOT/'Engine/include').as_posix()}")
target_sources(model_project PRIVATE FILE_SET CXX_MODULES BASE_DIRS {' '.join('"'+p.as_posix()+'"' for p in roots)} FILES {' '.join('"'+p.as_posix()+'"' for p in selected.values())})
''',encoding='utf-8')
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']]
        fixture_data=ROOT/'Artifacts/Tests'/f'm{time.time_ns():x}'
        for config in ('Debug','Release'):
            data=fixture_data/config
            outside=data/'outside'
            redirected=data/'redirected'
            outside.mkdir(parents=True)
            redirected.mkdir()
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],
                ['cmd.exe','/d','/c','mklink','/J',str(redirected/'Library'),str(outside)],
                [str(build/config/'model_project.exe'),str(data)]]
        results=[]
        for i,command in enumerate(commands):
            ran=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=150)
            (proof/f'lane-{i}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8');results.append({'command':command,'exit':ran.returncode})
            self.assertEqual(ran.returncode,0,ran.stdout[-14000:]+ran.stderr)
        (proof/'result.json').write_text(json.dumps({'passed':True,'commands':results,'source_owners_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,*implementations]},'scope':'Actual Editor review/publish, shoreline pose for a supplied terrain receipt, and scene transactions with real GLB/PNG/artifact, texture library, SceneDocument history, serializer, disk persistence and runtime modules. Recording editor/lifecycle/terrain ports; no generator, GPU or live window.'},indent=2),encoding='utf-8')


if __name__=='__main__':unittest.main()
