"""Actual inline placement + scene transaction + Forest library filesystem proof."""
from pathlib import Path
import json
import os
import re
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class ForestPlacementPersistence(unittest.TestCase):
    def test_inline_add_publishes_reopenable_authoring_before_scene_commit(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC module consumer lane')
        source = (ROOT / 'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(encoding='utf-8')
        transactions = (ROOT / 'Engine/src/editor/application/editor.application_scene_transactions.inl').read_text(encoding='utf-8')
        functions = '\n'.join(definition(transactions, marker) for marker in (
            '        [[nodiscard]] bool project_scene_document_into_editor(',
            '        [[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(',
            '        [[nodiscard]] bool apply_editor_scene_transaction('))
        # The callback's default {} precedes the body; extract the actual body
        # opening rather than mistaking that default argument for the function.
        start = transactions.index('        [[nodiscard]] bool create_editor_scene_entities(')
        brace = transactions.index('\n        {', start) + len('\n        ')
        end, depth = brace + 1, 1
        while depth:
            depth += (transactions[end] == '{') - (transactions[end] == '}')
            end += 1
        functions += '\n' + transactions[start:end]
        functions += '\n' + '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] bool ensure_plant_lab_compiled_asset(',
            '        [[nodiscard]] bool publish_plant_lab_project_asset(',
            '        [[nodiscard]] std::vector<EditorEntity>\n            project_compiled_forest_entities(',
            '        [[nodiscard]] forest::preview_mesh::MaterialAtlasResult prepare_plant_material_atlas(',
            '        [[nodiscard]] std::optional<scene::SceneTextureMaterialSnapshot> publish_forest_scene_material(',
            '        [[nodiscard]] bool persist_forest_scene_mesh(const std::filesystem::path&',
            '        [[nodiscard]] bool persist_forest_scene_mesh(EditorState&',
            '        [[nodiscard]] bool focus_forest_factory_placement(',
            '        [[nodiscard]] std::size_t place_forest_factory_asset('))
        code = r'''
#include "core.format_text.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <memory>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <crtdbg.h>
#undef assert
#define assert(value) do {if(!(value)){std::fprintf(stderr,"Contract assertion line %d: %s\n",__LINE__,#value);std::exit(81);}}while(false)
import editor.application;
import forest.factory;
import forest.preview_mesh;
import asset.mesh_artifact;
import asset.texture_import;
import core.path;
import render.texture_artifact;
import platform.filesystem;
import scene.persistence;
import project.forest_library;
import project.texture_pipeline;
import project.texture_library;
import scene.document;
import scene.snapshot;
import scene.interaction;
namespace epochengine {
namespace fs=std::filesystem;
namespace core {struct Context {};}
namespace visuals {int forest_trunk(){return 0;}int forest_foliage_cluster(){return 1;}}
namespace previewgrid {
 using Vec3=scene_interaction::Vec3;
 Vec3 visual_rgb(int slot){return slot==0?Vec3{0.35f,0.19f,0.08f}:Vec3{0.2f,0.6f,0.1f};}
 inline unsigned focusCalls{};inline scene_interaction::Vec3 target;inline float radius{};
 bool focus_camera(const core::Context*,scene_interaction::Vec3 value,float extent){
  ++focusCalls;target=value;radius=extent;return true;
 }
 bool focus_camera_precise(const core::Context*ctx,std::array<double,3> value,float extent){
  return focus_camera(ctx,{float(value[0]),float(value[1]),float(value[2])},extent);
 }
}
struct EditorEntity {
 scene::SceneObjectId sceneObjectId{};std::string name,type,category;
 std::array<float,3> position{},rotation{},scale{1,1,1};bool visible{true},editorOnly{};
 decltype(scene::SceneObjectSnapshot{}.texture_material) textureMaterial{};
 decltype(scene::SceneObjectSnapshot{}.mesh_asset) meshAsset{};
 bool meshLoadAttempted{};std::shared_ptr<const asset::mesh::Artifact> meshSnapshot;
 std::optional<std::array<double,3>> positionAnchor;std::optional<scene::SceneForestRegionInstance>forestRegion;
 std::optional<scene::SceneTerrainAttachment> terrainAttachment;
};
struct EditorState {
 std::string projectId{"inline-forest"},projectRoot,projectStatus,plantLabStatus,plantLabAssetStatus;
 forest::ForestAssetDocument plantLabDocument;forest::CompiledForestAsset plantLabCompiled;
 project_forests::ForestArtifactLocator plantLabPublishedArtifact;
 bool plantLabDocumentInitialized{true};std::uint64_t plantLabCompiledRevision{},plantLabCompiledContentHash{};
 std::vector<EditorEntity> entities;authoring::scene::SceneDocument sceneDocument;
 scene_interaction::SceneInteractionQuery sceneInteraction;std::array<double,3>sceneInteractionOrigin{};
 scene::SceneObjectId selectedEntityId{};std::uint64_t sceneDocumentRevision{};
 bool sceneInteractionDirty{},sceneLightingDirty{};int surfaceSettleFrames{};
 struct {std::uint64_t frame_index{};double simulated_seconds{};} timeSnapshot;
 struct {struct Evidence {bool project_selected{true};} value;const Evidence& evidence()const{return value;}} projectLifecycle;
};
void push_editor_log(EditorState&,std::string_view text){std::fprintf(stderr,"%.*s\n",int(text.size()),text.data());}
void synchronize_editor_selection(EditorState&){}
bool rebuild_editor_scene_document(EditorState&,std::string_view){return false;}
std::string editor_scene_identity(const EditorState&){return "world:inline-forest";}
bool reopen_plant_lab_project_asset(EditorState&,bool,bool* missing=nullptr){if(missing)*missing=true;return false;}
fs::path resolve_editor_path(const fs::path& p){return p;}
std::string display_project_path(const fs::path& p){return p.generic_string();}
constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
EditorEntity editor_entity_from_seed(const EditorSceneSeedEntity& s){return {0,s.name,s.type,s.category,s.position,s.rotation,s.scale,s.visible,s.editor_only};}
bool align_editor_entity_group_to_surface(std::vector<EditorEntity>& e,float){return !e.empty();}
float primary_ground_top_y(const EditorState&){return 0;}
std::optional<std::size_t> editor_entity_index(const EditorState& state,scene::SceneObjectId id){
 for(std::size_t i=0;i<state.entities.size();++i)if(state.entities[i].sceneObjectId==id)return i;return {};
}
bool ensure_editor_scene_interaction(EditorState& state){
 const auto projection=state.sceneDocument.project_snapshot(0,0);
 return projection&&state.sceneInteraction.rebuild(projection.snapshot).committed();
}
bool begin_forest_surface_placement(EditorState&,const EditorEntity&,std::size_t){return false;}
FUNCTIONS
EditorState make_state(const fs::path& root,std::uint32_t maximum=4096){
 fs::create_directories(root);EditorState state;state.projectRoot=root.generic_string();
 state.plantLabDocument=forest::make_default_plant_lab_document();
 scene::SceneSnapshot snapshot;snapshot.scene_id="world:inline-forest";snapshot.world_name="inline-forest";
 scene::SceneObjectSnapshot cube;cube.id=1;cube.name="ExistingCube";cube.type="Cube";cube.category="World";
 cube.position={40,2,0};cube.scale={1,1,1};snapshot.objects.push_back(cube);
 authoring::scene::DocumentLimits limits;limits.maximum_active_objects=maximum;
 auto built=authoring::scene::SceneDocument::from_snapshot(snapshot,limits);assert(built);
 state.sceneDocument=std::move(built.value);assert(project_scene_document_into_editor(state));return state;
}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
}
int main(int argc,char** argv){
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
 using namespace epochengine;
 assert(argc==2||argc==4);const auto root=fs::path{argv[1]};assert(root.is_absolute());fs::create_directories(root);
 auto state=make_state(root/"project");const auto source=fs::path{state.projectRoot}/kDefaultPlantLabLogicalPath;
 const float originalHeight=state.plantLabDocument.profile.config.targetHeightMeters;
 const auto originalJournal=state.plantLabDocument.journal.size();const auto originalCursor=state.plantLabDocument.historyCursor;
 assert(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,originalHeight+0.5f));
 const auto editedHash=state.plantLabDocument.contentHash;assert(!fs::exists(source));
 assert(place_forest_factory_asset(state)>0);assert(state.entities.size()>1&&state.entities.front().name=="ExistingCube");
 // Use the real interaction module to frame the just-added plant, including
 // foliage, while excluding another plant and a distant planet-sized object.
 const auto selectedTrunk=state.selectedEntityId;
 const auto added=editor_entity_index(state,selectedTrunk);assert(added&&state.entities[*added].meshAsset);
 const auto ref=*state.entities[*added].meshAsset;
 const auto material=state.entities[*added].textureMaterial;assert(material&&material->valid());
 const auto& atlas=forest::preview_mesh::material_atlas();assert(asset::texture::validate_compiled_artifact(atlas));
 const auto referenceRoot=core::path::engine_asset_dir()/"forest";
 const bool reference=fs::is_regular_file(referenceRoot/"bark_texture_preview.png")
  &&fs::is_regular_file(referenceRoot/"foliage_reference_board.png");
 assert(forest::preview_mesh::reference_material_available()==reference);
 const auto atlasWidth=reference?1024u:256u,atlasHeight=reference?512u:128u;
 assert(atlas.mips.size()==1&&atlas.mips[0].width==atlasWidth&&atlas.mips[0].height==atlasHeight);
 assert(atlas.identity.profile.color_space==(reference?asset::texture::ColorSpace::srgb:asset::texture::ColorSpace::linear));
 assert(material->alpha==scene::SceneTextureAlphaMode::cutout);
 assert(material->color_space==(reference?scene::SceneTextureColorSpace::srgb:scene::SceneTextureColorSpace::linear));
 unsigned transparent{},covered{};
 for(unsigned y=0;y<atlasHeight;++y)for(unsigned x=0;x<atlasWidth;++x){
  auto a=std::to_integer<unsigned>(atlas.mips[0].texels[(y*atlasWidth+x)*4u+3u]);
  if(x<atlasWidth/2u)assert(a==255);else {transparent+=a==0;covered+=a>=128;}}
 assert(transparent>(reference?10000u:512u)&&covered>(reference?1000u:64u));
 if(reference){
 const auto originalBark=asset::texture::import_texture_file({.source_path=core::path::engine_asset_dir()/"forest/bark_texture_preview.png"});
 assert(originalBark&&!originalBark.artifact.mips.empty());
 const auto& sourceBark=originalBark.artifact.mips.front();
 // Legacy/default selects the first bark cell; neither neighboring styles nor the
 // contact sheet divider may be repeated along a trunk.
 for(const auto coordinate:std::array<std::array<unsigned,2>,3>{{{18,0},{256,256},{494,511}}}){
  const auto x=coordinate[0],y=coordinate[1];const auto sx=7u+(x-18u)*241u/476u,sy=7u+y*241u/511u;
  for(unsigned channel=0;channel<4;++channel)
   assert(atlas.mips[0].texels[(y*1024u+x)*4u+channel]==sourceBark.texels[sy*sourceBark.row_pitch_bytes+sx*4u+channel]);}

 std::set<std::array<std::uint64_t,4>> barkKeys;
 for(unsigned tile=0;tile<forest::preview_mesh::kBarkTileCount;++tile){
  const auto& selected=forest::preview_mesh::material_atlas(tile);
  assert(asset::texture::validate_compiled_artifact(selected));
  assert(&selected==&forest::preview_mesh::material_atlas(tile));
  assert(barkKeys.insert(selected.identity.key.words).second);
  const auto& mip=selected.mips.front();
  // Entire bark payload, including horizontal guard pixels, comes from this
  // selected cell. Vertical sampler wrapping can never reach a different tile.
  for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x){
   const auto local=std::clamp(x,18u,494u)-18u;
   const auto sx=(tile%4u)*256u+7u+local*241u/476u;
   const auto sy=(tile/4u)*256u+7u+y*241u/511u;
   for(unsigned c=0;c<4;++c)
    assert(mip.texels[(y*1024u+x)*4u+c]==sourceBark.texels[sy*sourceBark.row_pitch_bytes+sx*4u+c]);
  }
  // Bark choices must leave the admitted leaf image/alpha unchanged.
  for(unsigned y=0;y<512;++y)for(unsigned x=512;x<1024;++x)for(unsigned c=0;c<4;++c)
   assert(mip.texels[(y*1024u+x)*4u+c]==atlas.mips.front().texels[(y*1024u+x)*4u+c]);
 }
 assert(&forest::preview_mesh::material_atlas(99u)==&atlas);


 // The admitted oak cell supplies the artwork after the cutout is fitted
 // around the petiole. The source colors and the attachment both survive.
 const auto originalLeaf=asset::texture::import_texture_file({.source_path=core::path::engine_asset_dir()/"forest/foliage_reference_board.png"});
 assert(originalLeaf&&!originalLeaf.artifact.mips.empty());
 const auto& sourceLeaf=originalLeaf.artifact.mips.front();assert(sourceLeaf.width==1254&&sourceLeaf.height==1254);
 const auto rgbKey=[](unsigned r,unsigned g,unsigned b){return (r<<16u)|(g<<8u)|b;};
 std::set<unsigned> sourceColors;
 for(unsigned y=4;y<310;++y)for(unsigned x=4;x<310;++x){
  const auto offset=y*sourceLeaf.row_pitch_bytes+x*4u;
  sourceColors.insert(rgbKey(std::to_integer<unsigned>(sourceLeaf.texels[offset]),
   std::to_integer<unsigned>(sourceLeaf.texels[offset+1u]),std::to_integer<unsigned>(sourceLeaf.texels[offset+2u])));
 }
 unsigned greenPhoto{},brownStem{},paleDetail{};
 for(unsigned y=18;y<=494;++y)for(unsigned x=18;x<=494;++x){
  const auto offset=(y*1024u+512u+x)*4u;
  const auto alpha=std::to_integer<unsigned>(atlas.mips[0].texels[offset+3u]);assert(alpha==0||alpha==255);
  if(alpha==255){
   const auto r=std::to_integer<unsigned>(atlas.mips[0].texels[offset]),g=std::to_integer<unsigned>(atlas.mips[0].texels[offset+1u]),b=std::to_integer<unsigned>(atlas.mips[0].texels[offset+2u]);
   assert(sourceColors.contains(rgbKey(r,g,b)));
   greenPhoto+=g>r&&g>b;
   brownStem+=r>g&&g>b;
   paleDetail+=r>130&&g>130&&b>80;
  }
 }
 assert(greenPhoto>10000&&brownStem>50&&paleDetail>50);
 assert(atlas.mips[0].texels[(18u*1024u+530u)*4u+3u]==std::byte{0});
 assert(atlas.mips[0].texels[(494u*1024u+768u)*4u+3u]==std::byte{255});
 }else{
  for(unsigned tile=0;tile<forest::preview_mesh::kBarkTileCount;++tile)
   assert(&forest::preview_mesh::material_atlas(tile)==&atlas);
 }
 assert(material->address_v==scene::SceneTextureAddress::repeat);
 forest::preview_mesh::Geometry sampleGeometry;
 sampleGeometry.segmentCount=1;sampleGeometry.segments[0].start={0,0,0};sampleGeometry.segments[0].end={0,3,0};
 sampleGeometry.leafCount=1;sampleGeometry.leaves[0].position={0,3,0};sampleGeometry.leaves[0].size=1;
 sampleGeometry.leaves[0].outward={0,0,1};sampleGeometry.leaves[0].up={0,1,0};sampleGeometry.leaves[0].variant=1;
 const auto shaped=forest::preview_mesh::build(sampleGeometry);assert(shaped.valid);
 assert(shaped.vertices[forest::preview_mesh::kRadialSides+1].uv.y==3.0f);
 const auto barkUV=forest::preview_mesh::material_uv(forest::preview_mesh::MaterialSlot::wood,{.5f,3.f});
 assert(barkUV.y==3.f); // physical length survives atlas mapping and repeats via material sampler
 std::size_t leaves{};for(const auto& vertex:shaped.vertices)if(vertex.material==forest::preview_mesh::MaterialSlot::foliage){
  ++leaves;assert(vertex.uv.x>=0&&vertex.uv.x<=1&&vertex.uv.y>=0&&vertex.uv.y<=1);}
 assert(leaves==12); // two source-art leaf surfaces; alpha owns their silhouette
 for(std::size_t i=shaped.indices.size()-24;i<shaped.indices.size();i+=3){
  const auto&a=shaped.vertices[shaped.indices[i]];const auto&b=shaped.vertices[shaped.indices[i+1]];const auto&c=shaped.vertices[shaped.indices[i+2]];
  const auto cross=forest::preview_mesh::detail::cross(forest::preview_mesh::detail::subtract(b.position,a.position),forest::preview_mesh::detail::subtract(c.position,a.position));
  assert(forest::preview_mesh::detail::dot(cross,a.normal)>0);}

 project_textures::ProjectTexturePipeline coldTextures{state.projectId,state.projectRoot};
 assert(coldTextures.restore_exact(material->logical_path,{material->artifact_key},texture_artifact::TransferUsage::base_color));
 const auto mesh=asset::mesh::read_project_artifact(state.projectRoot,ref.logical_path,{ref.artifact_key});
 assert(mesh&&mesh.artifact->vertices().size()==ref.vertex_count&&mesh.artifact->indices().size()==ref.index_count);
 const auto complete=forest::preview_mesh::build(state.plantLabCompiled.graph,state.plantLabCompiled.sample);
 assert(complete.valid&&mesh.artifact->indices().size()==complete.indices.size());
 auto snapshot=state.sceneDocument.project_snapshot(0,0);assert(snapshot);
 const auto sceneFile=fs::path{state.projectRoot}/"saved.epoch_scene";
 assert(scene::persistence::save_scene_snapshot_atomic(sceneFile,snapshot.snapshot));
 const auto cold=scene::persistence::load_scene_snapshot(sceneFile);assert(cold.result);
 auto coldDocument=authoring::scene::SceneDocument::from_snapshot(cold.snapshot);assert(coldDocument);
 EditorState reopenedScene;reopenedScene.sceneDocument=std::move(coldDocument.value);
 assert(project_scene_document_into_editor(reopenedScene));
 const auto reopenedPlant=editor_entity_index(reopenedScene,selectedTrunk);assert(reopenedPlant);
 assert(reopenedScene.entities[*reopenedPlant].meshAsset==state.entities[*added].meshAsset);
 assert(reopenedScene.entities[*reopenedPlant].textureMaterial==material);
 assert(asset::mesh::read_project_artifact(state.projectRoot,
  reopenedScene.entities[*reopenedPlant].meshAsset->logical_path,{ref.artifact_key}));
 EditorEntity otherPlant;otherPlant.name="ForestAsset_01_Trunk";otherPlant.type="ForestTrunk";
 otherPlant.category="ForestFactory";otherPlant.position={200,10,0};otherPlant.scale={20,20,20};
 EditorEntity planet;planet.name="Planet";planet.type="Cube";planet.category="Planetary Terrain";
 planet.position={-300,0,0};planet.scale={200,200,200};
 EditorEntity unrelated;unrelated.name="ForestAsset_00_Planet";unrelated.type="Cube";
 unrelated.category="World";unrelated.position={0,200,0};
 assert(create_editor_scene_entities(state,{otherPlant,planet,unrelated},"Other scene objects"));
 state.selectedEntityId=selectedTrunk;core::Context context;
 assert(focus_forest_factory_placement(&context,state));assert(state.selectedEntityId==selectedTrunk);
 std::vector<scene_interaction::SceneObjectId> plantIds,allIds;
 for(const auto& entity:state.entities){allIds.push_back({entity.sceneObjectId,1});
  if(entity.category=="ForestFactory"&&entity.name.starts_with("ForestAsset_00_"))plantIds.push_back({entity.sceneObjectId,1});}
 const auto groupFocus=state.sceneInteraction.focus_target(plantIds);
 const auto trunkFocus=state.sceneInteraction.focus_target(scene_interaction::SceneObjectId{selectedTrunk,1});
 const auto allFocus=state.sceneInteraction.focus_target(allIds);
 assert(groupFocus&&trunkFocus&&allFocus&&plantIds.size()==1);
 assert(previewgrid::radius==groupFocus->radius&&previewgrid::radius==trunkFocus->radius);
 assert(previewgrid::radius<allFocus->radius&&previewgrid::target.x==groupFocus->target.x
  &&previewgrid::target.y==groupFocus->target.y&&previewgrid::target.z==groupFocus->target.z);
 assert(!focus_forest_factory_placement(nullptr,state));state.selectedEntityId=1;
 assert(!focus_forest_factory_placement(&context,state)&&previewgrid::focusCalls==1);state.selectedEntityId=selectedTrunk;
 project_forests::ForestAssetLibrary library{state.projectId,state.projectRoot};
 auto reopened=library.load_source(kDefaultPlantLabLogicalPath);assert(reopened&&reopened.document.contentHash==editedHash);
 assert(reopened.document.journal.size()==originalJournal+1&&reopened.document.historyCursor==originalCursor+1);
 assert(forest::undo_forest_profile_edit(reopened.document));assert(reopened.document.profile.config.targetHeightMeters==originalHeight);
 assert(library.load_exact(kDefaultPlantLabLogicalPath,state.plantLabPublishedArtifact.artifact_key));
 const auto savedBytes=bytes(source);const auto oldArtifact=state.plantLabPublishedArtifact.storage_path;
 const auto artifactBytes=bytes(oldArtifact);const auto objects=state.entities.size();
 assert(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,originalHeight+1.0f));
 const auto speciesPath="Assets/Forest/species_fixture.epoch_forest";
 assert(publish_plant_lab_project_asset(state,nullptr,speciesPath));
 const auto savedSpecies=library.load_source(speciesPath);assert(savedSpecies);
 assert(savedSpecies.document.contentHash==state.plantLabDocument.contentHash);
 assert(library.load_latest(speciesPath)&&bytes(source)==savedBytes);
 const auto speciesBytes=bytes(savedSpecies.locator.storage_path);
 assert(publish_plant_lab_project_asset(state,nullptr,speciesPath));
 assert(!publish_plant_lab_project_asset(state,&reopened.document,speciesPath));
 assert(bytes(savedSpecies.locator.storage_path)==speciesBytes&&bytes(source)==savedBytes);
 assert(state.plantLabPublishedArtifact.storage_path==oldArtifact);
 // Break only this fixture's artifact directory. Failed publication must leave
 // canonical source/previous artifact bytes and all scene objects unchanged.
 const auto artifactDir=fs::path{state.projectRoot}/"Library/Forest";const auto held=artifactDir.parent_path()/"Forest-retained";
 fs::rename(artifactDir,held);std::ofstream(artifactDir)<<"ordinary-file";
 assert(place_forest_factory_asset(state)==0&&state.entities.size()==objects&&bytes(source)==savedBytes);
 fs::remove(artifactDir);fs::rename(held,artifactDir);assert(bytes(oldArtifact)==artifactBytes);
 assert(library.load_source(kDefaultPlantLabLogicalPath).document.contentHash==editedHash);
 // Scene preflight failure runs before publication.
 state.sceneDocument={};assert(place_forest_factory_asset(state)==0&&bytes(source)==savedBytes);
 // A later journal/scene-capacity refusal may follow successful publication.
 // Retain that authored asset honestly; never claim a joint rollback.
 auto barkState=make_state(root/"bark");
 const auto legacyHash=barkState.plantLabDocument.contentHash;
 assert(forest::apply_forest_profile_edit(barkState.plantLabDocument,forest::ForestProfileProperty::BarkTileIndex,7.0f));
 assert(barkState.plantLabDocument.contentHash!=legacyHash);
 assert(forest::undo_forest_profile_edit(barkState.plantLabDocument));assert(barkState.plantLabDocument.profile.barkTileIndex==0);
 assert(forest::redo_forest_profile_edit(barkState.plantLabDocument));assert(barkState.plantLabDocument.profile.barkTileIndex==7);
 assert(place_forest_factory_asset(barkState)>0);
 bool selectedBarkPlaced=false;
 for(const auto& entity:barkState.entities)if(entity.textureMaterial){
  const auto& barkMaterial=*entity.textureMaterial;
  assert(barkMaterial.artifact_key==forest::preview_mesh::material_atlas(7).identity.key.words);
  assert(barkMaterial.logical_path.ends_with("_bark_7"));
  project_textures::ProjectTexturePipeline coldBark{barkState.projectId,barkState.projectRoot};
  assert(coldBark.restore_exact(barkMaterial.logical_path,{barkMaterial.artifact_key},texture_artifact::TransferUsage::base_color));
  selectedBarkPlaced=true;
 }
 assert(selectedBarkPlaced);

 project_forests::ForestAssetLibrary barkLibrary{barkState.projectId,barkState.projectRoot};
 auto barkReopened=barkLibrary.load_source(kDefaultPlantLabLogicalPath);assert(barkReopened);
 assert(barkReopened.document.profile.barkTileIndex==7);
 assert(barkReopened.document.contentHash==barkState.plantLabDocument.contentHash);
 assert(forest::undo_forest_profile_edit(barkReopened.document));assert(barkReopened.document.profile.barkTileIndex==0);
 assert(forest::redo_forest_profile_edit(barkReopened.document));assert(barkReopened.document.profile.barkTileIndex==7);
 // Custom imported source images use exact keys through the same placement
 // owner, and the published combined material cold-restores with the scene.
 auto custom=make_state(root/"custom");
 project_textures::TextureArtifactLibrary customTextures{custom.projectId,custom.projectRoot};
 const auto customSource=[&](bool foliage){
  auto source=atlas;auto& mip=source.mips.front();mip.width=atlasHeight;mip.row_pitch_bytes=atlasHeight*4u;
  mip.texels.resize(std::size_t{mip.row_pitch_bytes}*mip.height);
  const auto& input=atlas.mips.front();
  for(unsigned y=0;y<mip.height;++y)
   std::copy_n(input.texels.data()+y*input.row_pitch_bytes+(foliage?mip.row_pitch_bytes:0u),
    mip.row_pitch_bytes,mip.texels.data()+y*mip.row_pitch_bytes);
  mip.content=asset::texture::compiled_texture_mip_content(mip);
  source.identity=asset::texture::build_compiled_texture_artifact_identity(
   {asset::texture::deterministic_content_hash(foliage?"custom-leaf-fixture":"custom-bark-fixture",mip.texels),1},
   {.format=asset::texture::ArtifactFormat::rgba8_srgb,.color_space=asset::texture::ColorSpace::srgb},mip.width,mip.height,1);
  source.identity.compilation_required=false;
  source.payload_content=asset::texture::compiled_texture_payload_content(source.identity,source.mips);
  assert(asset::texture::validate_compiled_artifact(source));return source;
 };
 const auto testTexture=[&](bool foliage){
  if(argc==2)return customSource(foliage);
  const std::string_view path{argv[foliage?3:2]};
  auto image=asset::texture::import_texture_file({.source_path=fs::path{std::u8string{path.begin(),path.end()}}});
  assert(image);
  return std::move(image.artifact);
 };
 const auto customBark=testTexture(false),customLeaf=testTexture(true);
 assert(customTextures.persist("Assets/custom_bark.png",customBark));
 assert(customTextures.persist("Assets/custom_leaf.png",customLeaf));
 assert(forest::apply_forest_material_edit(custom.plantLabDocument,forest::ForestTextureSlot::bark,
  {"Assets/custom_bark.png",customBark.identity.key.words}));
 assert(forest::apply_forest_material_edit(custom.plantLabDocument,forest::ForestTextureSlot::foliage,
  {"Assets/custom_leaf.png",customLeaf.identity.key.words}));
 assert(place_forest_factory_asset(custom)>0);
 auto composed=prepare_plant_material_atlas(custom.projectRoot,custom.projectId,custom.plantLabDocument.profile);assert(composed);
 bool customPlaced{};for(const auto&entity:custom.entities)if(entity.textureMaterial){
  customPlaced=true;const auto&binding=*entity.textureMaterial;
  assert(binding.artifact_key==composed.artifact.identity.key.words&&binding.logical_path.starts_with("Textures/Forest/project_bark_foliage_"));
  project_textures::ProjectTexturePipeline restored{custom.projectId,custom.projectRoot};
  assert(restored.restore_exact(binding.logical_path,{binding.artifact_key},texture_artifact::TransferUsage::base_color));
 }
 assert(customPlaced);
 const auto priorObjects=custom.entities.size();const auto priorSource=bytes(fs::path{custom.projectRoot}/kDefaultPlantLabLogicalPath);
 custom.plantLabDocument.profile.barkTexture.artifactKey[3]^=1u;
 custom.plantLabDocument.contentHash=forest::forest_profile_content_hash(custom.plantLabDocument.profile);
 assert(place_forest_factory_asset(custom)==0&&custom.entities.size()==priorObjects);
 // The preexisting Add owner publishes its authored recipe before scene
 // placement, but missing material bytes never change the saved scene.
 assert(!priorSource.empty());
 auto limited=make_state(root/"limited",1);assert(place_forest_factory_asset(limited)==0);
 assert(limited.entities.size()==1&&limited.entities[0].name=="ExistingCube");
 project_forests::ForestAssetLibrary retained{limited.projectId,limited.projectRoot};
 assert(retained.load_source(kDefaultPlantLabLogicalPath));assert(limited.sceneDocument.objects().size()==1);
}
'''.replace('FUNCTIONS', functions)
        proof = ROOT / 'Artifacts/Reports/forest-smooth-preview-oct27/placement'
        proof.mkdir(parents=True, exist_ok=True)
        (proof/'result.json').write_text(json.dumps({'passed':False,'status':'running'})+'\n',encoding='utf-8')
        (proof / 'consumer.cpp').write_text(code, encoding='utf-8')
        module_map = {}
        for base in [ROOT / 'Engine/modules', ROOT / 'Engine/dep/EpochPlatformEngine/modules']:
            for path in base.glob('*.ixx'):
                text = path.read_text(encoding='utf-8-sig')
                match = re.search(r'^export module\s+([\w.:]+);', text, re.M)
                if match:
                    module_map[match.group(1)] = path
        implementations = [ROOT / p for p in ('Engine/src/project/project.forest_library.cpp', 'Engine/src/project/project.asset_registry.cpp', 'Engine/src/forest/forest.preview_mesh.cpp', 'Engine/src/assets/texture/asset.texture_artifact.cpp', 'Engine/src/project/project.texture_library.cpp', 'Engine/src/project/project.texture_source.cpp', 'Engine/src/project/project.texture_pipeline.cpp', 'Engine/src/project/project.texture_resources.cpp', 'Engine/src/authoring/texture/authoring.texture_document.cpp', 'Engine/src/authoring/texture/authoring.texture_residency.cpp', 'Engine/src/renderers/canvas2d/render.canvas2d_scene.cpp')]
        implementations += [ROOT/p for p in ('Engine/src/assets/texture/asset.texture_import.cpp','Engine/src/assets/image.stb.cpp','Engine/src/platform/core.path.cpp','Engine/dep/EpochPlatformEngine/src/core/core.log.cpp','Engine/dep/EpochPlatformEngine/src/core/core.time.cpp')]
        pending = ['editor.application', 'forest.factory', 'project.forest_library', 'project.texture_pipeline', 'scene.document', 'scene.snapshot', 'scene.interaction','forest.preview_mesh','asset.mesh_artifact','scene.persistence']
        for path in implementations:
            pending += re.findall(r'^(?:export )?(?:import|module)\s+([\w.]+);', path.read_text(encoding='utf-8-sig'), re.M)
        selected = {}
        while pending:
            name = pending.pop()
            if name in selected or name in ('std', 'std.compat'):
                continue
            self.assertIn(name, module_map, name)
            path = module_map[name]
            selected[name] = path
            pending += re.findall(r'^(?:export )?(?:import|module)\s+([\w.]+);', path.read_text(encoding='utf-8-sig'), re.M)
        # This case nests under the Public release checkout in CI. Keep the
        # production texture library's temporary path below Windows MAX_PATH.
        fixture = ROOT / 'Artifacts/Tests' / ('f'+f'{time.time_ns():x}'[-8:])
        modules = '\n'.join('"' + p.as_posix() + '"' for p in selected.values())
        sources = '\n'.join('"' + p.as_posix() + '"' for p in implementations)
        native_textures = [os.environ.get(key) for key in ('EPOCH_TEST_NATIVE_BARK_PNG', 'EPOCH_TEST_NATIVE_FOLIAGE_PNG')]
        native_args = ''.join(f' "{Path(path).resolve(strict=True).as_posix()}"' for path in native_textures) if all(native_textures) else ''
        (proof / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ForestPlacementPersistence LANGUAGES CXX)
add_executable(forest_placement consumer.cpp {sources})
target_compile_features(forest_placement PRIVATE cxx_std_23)
target_link_libraries(forest_placement PRIVATE shell32)
target_compile_options(forest_placement PRIVATE /W3 /WX /utf-8 /UNDEBUG /Z7)
target_include_directories(forest_placement PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_sources(forest_placement PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/modules').as_posix()}" FILES {modules})
enable_testing()
add_test(NAME actual_forest_placement_persistence COMMAND forest_placement "{fixture.as_posix()}/$<CONFIG>"{native_args})
set_tests_properties(actual_forest_placement_persistence PROPERTIES TIMEOUT 60)
''', encoding='utf-8')
        build=ROOT/'Artifacts/Intermediate/tests/forest-placement'
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],
                ['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']]
        env=dict(os.environ,EPOCH_ASSET_ROOT=str(ROOT/"Engine/resource"))
        runs=[]
        for index, command in enumerate(commands):
            result = subprocess.run(command, cwd=proof, env=env, capture_output=True, text=True, timeout=120)
            (proof/f'lane-{index}.log').write_text(result.stdout+result.stderr, encoding='utf-8')
            runs.append({'command':command,'exit':result.returncode})
            (proof/'result.json').write_text(json.dumps({'passed':False,'status':'failed' if result.returncode else 'running','runs':runs},indent=2)+'\n',encoding='utf-8')
            self.assertEqual(result.returncode, 0, result.stdout[-8000:]+result.stderr)
        (proof/'result.json').write_text(json.dumps({'scope':'Actual inline placement/create/publish + real SceneDocument and ForestAssetLibrary; no GUI/Engine build',
            'passed':True,'runs':runs,'fixture':str(fixture),'modules':len(selected),'cases':['edited source and journal reopen','failed publication preserves scene/source/artifact','preflight before publication','post-publication scene refusal retains authored asset without scene mutation','whole indexed tree focus excludes other plant/planet/category; scene+mesh cold save/reopen; selection unchanged'],
            'native_acceptance':False},indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
