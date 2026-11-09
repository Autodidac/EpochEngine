"""Actual mesh/scene modules and Editor/runtime admission owners; no GPU or provider process."""
from pathlib import Path
import json
import os
import re
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.packages.test_mesh_artifact_limits import CODE as GEOMETRY_CODE

ROOT = Path(__file__).resolve().parents[2]
EDIT = ROOT / "Engine/src/editor/application/editor.application_asset_browsing.inl"
PLAY = ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"


def owner_function(source, marker):
    start = source.index(marker)
    body = re.search(r"\n[ \t]*\{", source[start:])
    if not body:
        raise AssertionError(marker)
    brace = start + body.end() - 1
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


CODE = r'''
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <source_location>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
import asset.mesh_artifact;
import asset.texture_artifact;
import project.planetary;
import scene.snapshot;
import scene.serializer;
import scene.document;
import scene.runtime;
import scene.interaction;
import render.ray;
import render.preview_grid;
import render.lighting;
import context.type;
// CPU-only publication port, matching the actual backend admission query.
namespace epochengine::core {class Context {public:ContextType type{ContextType::OpenGL};};}
namespace mesh=epochengine::asset::mesh;namespace fs=std::filesystem;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Planetary admission failed line %d\n",line);std::exit(73);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
GEOMETRY
namespace probe {
namespace asset=epochengine::asset;namespace scene=epochengine::scene;namespace authoring=epochengine::authoring;
namespace previewgrid=epochengine::previewgrid;namespace scene_runtime=epochengine::scene_runtime;
namespace core=epochengine::core;namespace lighting=epochengine::lighting;
namespace scene_interaction=epochengine::scene_interaction;namespace ray=epochengine::ray;
// Recording boundaries only: the actual caller must request verified source and
// saved recipe evidence. This does not retest the filesystem attachment verifier.
namespace project_planetary {
 using Settings=epochengine::project_planetary::Settings;using SourceGeneration=epochengine::project_planetary::SourceGeneration;
 using SceneMeshBudget=epochengine::project_planetary::SceneMeshBudget;
 using MeshGenerationIdentity=epochengine::project_planetary::MeshGenerationIdentity;
 using GeneratedMeshFrame=epochengine::project_planetary::GeneratedMeshFrame;
 using GeneratedBody=epochengine::project_planetary::GeneratedBody;
 using epochengine::project_planetary::scene_mesh_budget;
 std::optional<Settings> saved;bool current=true;unsigned sourceChecks{};
 std::vector<std::pair<MeshGenerationIdentity,Settings>> generations;
 std::vector<GeneratedBody> body_receipts;unsigned body_reads{};
 std::optional<Settings> load_settings(const fs::path&){return saved;}
 std::optional<SourceGeneration> capture_source_generation(const fs::path& root,std::string_view hash){if(!current)return {};return SourceGeneration{root,"manifest",std::string(hash)};}
 bool source_generation_is_current(const fs::path&,const SourceGeneration&){++sourceChecks;return current;}
 std::optional<Settings> mesh_generation_settings(const fs::path&,const SourceGeneration&,const MeshGenerationIdentity& identity){if(current)for(const auto& entry:generations)if(entry.first==identity)return entry.second;return {};}
 std::vector<GeneratedBody> mesh_generation_bodies(const fs::path&,const SourceGeneration&,const MeshGenerationIdentity& identity,const mesh::Artifact& artifact){++body_reads;return current&&artifact.key().words==identity.artifact_key&&artifact.vertices().size()==identity.vertex_count&&artifact.indices().size()==identity.index_count?body_receipts:std::vector<GeneratedBody>{};}
}
namespace package_source {
 struct Attachment {std::string content_hash{"source-a"};bool attached=true;bool source_attached()const{return attached;}};
 Attachment active;Attachment load_project_attachment(const fs::path&,std::string_view){return active;}
}
namespace logger {enum class LogLevel{WARN};struct Logger{void log(LogLevel,const std::string&,std::source_location){}};Logger get(std::string_view){return {};}}
namespace platform::filesystem {
 bool exclusive_create_and_write(const fs::path& path,std::span<const std::byte> bytes,std::error_code& error){
  if(fs::exists(path)){error=std::make_error_code(std::errc::file_exists);return false;}
  std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));return file.good();}
}
struct EditorEntity {
 std::string name,type,category;std::uint64_t sceneObjectId{};bool visible=true;
 std::array<float,3> position{},rotation{},scale{1,1,1};
 std::optional<scene::SceneMeshSnapshot> meshAsset;std::shared_ptr<const mesh::Artifact> meshSnapshot;bool meshLoadAttempted{};
};
struct EditorState {
 std::string projectRoot,projectId{"planet"},projectStatus,worldHierarchyStatus,worldHierarchyFilter;
 std::uint64_t selectedEntityId{},worldHierarchyScrollSelection{};bool sceneInteractionDirty{};
 struct {struct Evidence {bool project_selected=true;}value;const Evidence& evidence()const{return value;}}projectLifecycle;
 package_source::Attachment planetaryAttachment;
 struct PlanetaryMeshPolicy {std::string selected_root;project_planetary::SourceGeneration source;project_planetary::Settings settings;std::optional<project_planetary::MeshGenerationIdentity> generated_mesh;std::vector<project_planetary::GeneratedBody> generated_bodies;bool body_inventory_checked{};};
 struct PlanetaryBodySelection{std::uint64_t entity{};std::array<std::uint64_t,4> mesh_key{};std::uint32_t body{};};
 std::optional<PlanetaryBodySelection> planetaryBodySelection;
 struct Hierarchy{std::uint64_t selected{};bool synchronize_selection(std::uint64_t value){selected=value;return true;}}worldHierarchy;
 std::optional<PlanetaryMeshPolicy> planetaryMeshPolicy;std::vector<EditorEntity> entities;
 authoring::scene::SceneDocument sceneDocument;
};
fs::path resolve_editor_path(const fs::path& value){return value;}
void push_editor_log(EditorState&,const std::string&){}
std::string make_entity_name(EditorState&,std::string_view name){return std::string(name);}
std::optional<std::size_t> editor_entity_index(EditorState& editor,std::uint64_t id){for(std::size_t i=0;i<editor.entities.size();++i)if(editor.entities[i].sceneObjectId==id)return i;return {};}
scene::SceneObjectSnapshot editor_entity_snapshot(const EditorEntity& entity){return {.id=entity.sceneObjectId,.name=entity.name,.type=entity.type,.category=entity.category,.position=entity.position,.rotation=entity.rotation,.scale=entity.scale,.visible=entity.visible,.mesh_asset=entity.meshAsset};}
void hydrate(EditorState& editor){const auto projected=editor.sceneDocument.project_snapshot();CHECK(projected);editor.entities.clear();for(const auto& object:projected.snapshot.objects)editor.entities.push_back({.name=object.name,.type=object.type,.category=object.category,.sceneObjectId=object.id,.visible=object.visible,.position=object.position,.rotation=object.rotation,.scale=object.scale,.meshAsset=object.mesh_asset});}
bool apply_editor_scene_transaction(EditorState& editor,std::span<const authoring::scene::SceneOperation> ops,std::string_view label,std::uint64_t selected){
 if(!editor.sceneDocument.apply_transaction(ops,label))return false;hydrate(editor);editor.selectedEntityId=selected;return true;}
bool create_editor_scene_entity(EditorState& editor,EditorEntity entity,const std::string& label){
 entity.sceneObjectId=17u;const auto converted=authoring::scene::descriptor_from_snapshot_object(editor_entity_snapshot(entity),0,editor.sceneDocument.limits());if(!converted)return false;
 const std::array<authoring::scene::SceneOperation,1> ops{authoring::scene::ObjectCreatedOperation{.object=*converted}};return apply_editor_scene_transaction(editor,ops,label,17u);}
void select_editor_entity(EditorState&,std::size_t);
EDITOR_FUNCTIONS
namespace gui {
 struct Vec2{float x{},y{};};enum class SemanticTone{info};enum class MenuItemIndicator{Radio};
 struct Item{std::string_view label,shortcut;Vec2 size;MenuItemIndicator indicator;bool checked{},selected{};float indentation{};};
 std::vector<std::string> labels;std::vector<float> indentations;std::string click;
 void label(std::string_view){}
 void set_cursor(Vec2){}
 void section_heading(std::string_view,SemanticTone){}
 void wrapped_label(const std::string&,float){}
 bool menu_item(const Item& item){labels.emplace_back(item.label);indentations.push_back(item.indentation);if(item.label!=click)return false;click.clear();return true;}
}
struct ContextPort{const void* get()const{return this;}}ctx;
std::optional<scene_interaction::FocusTarget> lastBodyFocus;
void select_editor_entity(EditorState& editor,std::size_t index){editor.selectedEntityId=editor.entities.at(index).sceneObjectId;}
void handle_scene_tool(EditorState& editor,const void*,std::string_view tool){CHECK(tool=="focus_selection");const auto index=editor_entity_index(editor,editor.selectedEntityId);lastBodyFocus=index?editor_planetary_body_focus(editor,editor.entities[*index]):std::nullopt;}
void render_body_rows(EditorState& editor){
 const float outlinerWidth=250,hierarchyRowHeight=26,hierarchyRowsHeight=100;
 const gui::Vec2 hierarchyOrigin{};
 std::size_t generatedBodyRow{};
 BODY_FILTER
 BODY_OUTLINER
}
// This mesh-only lane must never ask for a texture. Actual exact-material
// restore is covered by the base-color consumer; record/refuse that boundary.
namespace editor_project_textures {
 unsigned resolution_calls{};
 struct ResolvedTextureMaterial {
  decltype(previewgrid::MeshBaseColorBinding::artifact) artifact;
  decltype(previewgrid::MeshBaseColorBinding::logical) logical;
  decltype(previewgrid::MeshBaseColorBinding::sampler) sampler;
  explicit operator bool()const{return bool(artifact);}
 };
 struct ProjectTextureController {
  ProjectTextureController(const std::string&,const fs::path&){}
  ResolvedTextureMaterial resolve_scene_material(const scene::SceneTextureMaterialSnapshot&){++resolution_calls;return {};}
 };
}
class RuntimeProbe {
public:
 std::string m_projectRoot,m_projectId{"planet"},m_runtimeViewStatus;std::vector<scene_runtime::RuntimeEntity> m_entities;
 std::unordered_map<std::uint64_t,std::shared_ptr<const mesh::Artifact>> m_meshes;
 std::unordered_map<std::uint64_t,std::shared_ptr<const previewgrid::MeshBaseColorBinding>> m_meshBaseColorTextures;
 std::optional<project_planetary::Settings> m_meshPlanetarySettings;
 std::optional<project_planetary::SourceGeneration> m_meshPlanetarySource;
 previewgrid::MeshFrameLimits m_meshFrameLimits;
 RUNTIME_FUNCTION
};
using ProjectRuntimeEntity=scene_runtime::RuntimeEntity;
bool runtime_entity_is_forest_preview(const ProjectRuntimeEntity&){return false;}
bool runtime_forest_projection_admitted(const ProjectRuntimeEntity&){return false;}
previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(const ProjectRuntimeEntity&){return previewgrid::ObjectPreviewPrimitive::Cube;}
previewgrid::Vec3 runtime_marker_color_for_entity(const ProjectRuntimeEntity&,bool){return {1,1,1};}
float runtime_marker_radius_for_entity(const ProjectRuntimeEntity&){return 1;}
// This fixture exercises normal mesh publication. The asynchronous camera
// packet is a separate runtime boundary and must not run in this lane.
constexpr int m_runtimeViewportWidth=1280,m_runtimeViewportHeight=720;
bool pump_runtime_planetary_frame(const void*,previewgrid::SceneFrameRequest,bool){std::abort();}
PUBLICATION
}
int main(int argc,char** argv){
 CHECK(argc==2);using namespace probe;const fs::path root=argv[1];fs::create_directories(root);EditorState editor;editor.projectRoot=root.string();
 scene::SceneSnapshot initial;initial.scene_id="terrain";initial.project_id="planet";
 initial.objects.push_back({.id=20u,.name="Camera",.type="Camera"});initial.objects.push_back({.id=21u,.name="Spawn",.type="Spawn"});initial.primary_camera=20u;initial.primary_spawn=21u;
 auto document=authoring::scene::SceneDocument::from_snapshot(initial);CHECK(document);editor.sceneDocument=std::move(document.value);hydrate(editor);
 project_planetary::Settings settings;settings.cells_per_tile=32;settings.maximum_leaves=128;settings.snapshot_megabytes=32;settings.resident_megabytes=128;settings.ocean_surface=true;
 CHECK(epochengine::project_planetary::valid_settings(settings));project_planetary::saved=settings;refresh_editor_planetary_mesh_policy(editor);
 const auto budget=editor_planetary_mesh_budget(editor);CHECK(budget.artifact.maximum_bytes==32u*1024u*1024u&&budget.cache_bytes==48u*1024u*1024u);
 const auto checks=project_planetary::sourceChecks;for(unsigned frame=0;frame<50;++frame)CHECK(editor_planetary_mesh_budget(editor).cache_bytes==budget.cache_bytes);CHECK(project_planetary::sourceChecks==checks);
 auto geometry=sphere(210);auto built=mesh::build_artifact(geometry.vertices,geometry.indices,budget.artifact);CHECK(built);
 auto bytes=mesh::encode_artifact(*built.artifact);const auto artifact=root/"generated.epoch_mesh";{std::ofstream file(artifact,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));}
 CHECK(!mesh::read_artifact_file(artifact));CHECK(!import_project_mesh(editor,artifact,"World",budget.artifact));CHECK(editor.entities.size()==2);
 project_planetary::generations.push_back({{built.artifact->key().words,std::uint32_t(built.artifact->vertices().size()),std::uint32_t(built.artifact->indices().size()),{}},settings});
 CHECK(import_project_mesh(editor,artifact,"Planetary Terrain",budget.artifact));CHECK(editor.entities.size()==3&&editor.entities[0].meshSnapshot!=built.artifact);
 CHECK(editor_planetary_stream_identity_matches(editor,editor.entities[0]));
 // A disposable streamed cut is not the saved mesh whose body receipt is
 // bound to the source/recipe. Restoring policy after LOD publication must
 // validate the original project asset without replacing the live cut.
 {
  auto cutGeometry=sphere(4);const auto cut=mesh::build_artifact(cutGeometry.vertices,cutGeometry.indices);CHECK(cut);
  auto& entity=editor.entities[0];entity.meshSnapshot=cut.artifact;
  project_planetary::body_receipts={{.id=0,.name="Ground",.minimum={-1,-1,-1},.maximum={1,1,1}}};
  const auto reads=project_planetary::body_reads;
  refresh_editor_planetary_mesh_policy(editor);
  CHECK(editor.planetaryMeshPolicy->generated_bodies.size()==1&&editor.planetaryMeshPolicy->body_inventory_checked);
  CHECK(entity.meshSnapshot==cut.artifact&&project_planetary::body_reads==reads+1);
  refresh_editor_planetary_body_inventory(editor,entity);CHECK(project_planetary::body_reads==reads+1);
  entity.meshSnapshot=built.artifact;project_planetary::body_receipts.clear();
  refresh_editor_planetary_mesh_policy(editor);
 }
 // Production body focus and transform math; source/receipt ports above are
 // explicit. The real packet/range/filesystem verifier has its own consumer.
 {
  const auto original=editor.entities[0];const auto policy=editor.planetaryMeshPolicy;const auto revision=editor.sceneDocument.revision();
  auto& entity=editor.entities[0];entity.position={14,1,2};entity.rotation={0,90,0};entity.scale={2,1,3};
  entity.meshAsset->coordinates=scene::SceneMeshCoordinates{2500,{1000000000,-2000000000,3000000000}};
  editor.planetaryMeshPolicy->generated_mesh=editor_planetary_mesh_identity(entity);
  editor.planetaryMeshPolicy->generated_bodies={
   {.id=0,.name="Ground",.minimum={-1,-2,-3},.maximum={1,2,3}},
   {.id=5,.parent=0,.name="Moon",.minimum={1000,-1,-1},.maximum={1002,1,1}}};
  const auto focus=editor_planetary_body_focus(editor,entity);CHECK(focus);
  CHECK(std::abs(focus->target.x-14)<1&&std::abs(focus->target.y-1)<1&&std::abs(focus->target.z-2)<1);
  CHECK(std::abs(focus->radius-std::sqrt(22500.f*22500.f+5000.f*5000.f+5000.f*5000.f))<1);
  editor.planetaryBodySelection=EditorState::PlanetaryBodySelection{entity.sceneObjectId,entity.meshAsset->artifact_key,5};
  const auto moon=editor_planetary_body_focus(editor,entity);CHECK(moon&&std::abs(moon->target.z-(-5004998.f))<2&&moon->radius<15000);
  gui::labels.clear();gui::indentations.clear();gui::click="Moon";const auto count=editor.entities.size();render_body_rows(editor);
  CHECK(gui::click.empty()&&gui::labels==std::vector<std::string>({"Ground","Moon"}));
  CHECK(gui::indentations==std::vector<float>({0.0f,14.0f})); // real geometry, not spaces in canonical labels
  CHECK(editor.entities.size()==count&&editor.selectedEntityId==entity.sceneObjectId&&editor.worldHierarchy.selected==entity.sceneObjectId);
  CHECK(editor.planetaryBodySelection->body==5&&lastBodyFocus&&lastBodyFocus->target.z==moon->target.z);
  const auto selectedId=editor.selectedEntityId;project_planetary::current=false;
  CHECK(!select_editor_planetary_body(editor,0,0)&&editor.selectedEntityId==selectedId&&editor.planetaryBodySelection->body==5);
  project_planetary::current=true;CHECK(!select_editor_planetary_body(editor,0,99)&&editor.planetaryBodySelection->body==5);
  editor.planetaryBodySelection->mesh_key[0]^=1;CHECK(editor_planetary_body_focus(editor,entity)->radius==focus->radius);
  editor.planetaryMeshPolicy->selected_root="foreign";CHECK(!editor_planetary_body_focus(editor,entity));
  gui::labels.clear();render_body_rows(editor);CHECK(gui::labels.empty());
  editor.entities[0]=original;editor.planetaryMeshPolicy=policy;editor.planetaryBodySelection.reset();
  CHECK(editor.sceneDocument.revision()==revision);
 }
 CHECK(editor.entities[0].meshSnapshot&&editor.entities[0].meshSnapshot->key()==built.artifact->key());
 const auto projected=editor.sceneDocument.project_snapshot();CHECK(projected);const auto text=scene::serialize_snapshot_text(projected.snapshot);CHECK(!text.empty());
 {std::ofstream file(root/"world.epoch");file<<text;}
 std::ifstream file(root/"world.epoch");const std::string persisted((std::istreambuf_iterator<char>(file)),{});const auto reopened=scene::parse_snapshot_text(persisted);CHECK(reopened.ok);
 CHECK(reopened.snapshot.objects[0].mesh_asset==projected.snapshot.objects[0].mesh_asset);
 scene_runtime::SceneRuntime sceneRuntime;CHECK(sceneRuntime.replace(reopened.snapshot).committed());RuntimeProbe runtime;runtime.m_projectRoot=root.string();
 runtime.m_entities.assign(sceneRuntime.projection()->entities().begin(),sceneRuntime.projection()->entities().end());runtime.prepare_project_meshes();CHECK(runtime.m_meshes.size()==1&&runtime.m_meshPlanetarySettings);
 const epochengine::core::Context context;
 const auto* ctx=&context;
 publish_project_play_markers(ctx,runtime.m_entities,&runtime.m_meshes,runtime.m_meshFrameLimits);
 CHECK(previewgrid::detail::g_objectMarkers.at(ctx).markers.size()==1);
 const std::array marker{previewgrid::ObjectMarker{.primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,.mesh=built.artifact}};
 CHECK(!previewgrid::mesh_frame_admitted(marker));CHECK(previewgrid::mesh_frame_admitted(marker,runtime.m_meshFrameLimits));
 // Atomic publication refuses a full frame, leaving its previously admitted payload.
 CHECK(!previewgrid::set_object_markers(ctx,marker,{}));CHECK(previewgrid::detail::g_objectMarkers.at(ctx).markers.size()==1);
 auto invalid=marker;invalid[0].position.x=std::numeric_limits<float>::quiet_NaN();CHECK(!previewgrid::set_object_markers(ctx,invalid,runtime.m_meshFrameLimits));CHECK(previewgrid::detail::g_objectMarkers.at(ctx).markers.size()==1);
 // A verified package must not lend its index or byte allowance to ordinary meshes.
 const auto savedEntities=runtime.m_entities;
 auto smallGeometry=sphere(73);const auto small=mesh::build_artifact(smallGeometry.vertices,smallGeometry.indices);CHECK(small);
 const auto smallBytes=mesh::encode_artifact(*small.artifact);const auto smallPath=root/"Assets/Mesh/small.epoch_mesh";
 {std::ofstream out(smallPath,std::ios::binary);out.write(reinterpret_cast<const char*>(smallBytes.data()),std::streamsize(smallBytes.size()));}
 for(unsigned id:{30u,31u}){auto entity=runtime.m_entities.front();entity.source_id=id;entity.category="World";entity.mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/small.epoch_mesh",small.artifact->key().words,std::uint32_t(small.artifact->vertices().size()),std::uint32_t(small.artifact->indices().size())};runtime.m_entities.push_back(entity);}
 runtime.prepare_project_meshes();CHECK(runtime.m_meshes.size()==2&&runtime.m_meshes.contains(30u)&&!runtime.m_meshes.contains(31u));
 runtime.m_entities=savedEntities;
 smallGeometry.vertices.resize(65536,smallGeometry.vertices.front());const std::array<std::uint32_t,3> oneTriangle{0,1,75};
 const auto vertexHeavy=mesh::build_artifact(smallGeometry.vertices,oneTriangle);CHECK(vertexHeavy);const auto heavyBytes=mesh::encode_artifact(*vertexHeavy.artifact);
 {std::ofstream out(smallPath,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(heavyBytes.data()),std::streamsize(heavyBytes.size()));}
 for(unsigned id=40;id<46;++id){auto entity=runtime.m_entities.front();entity.source_id=id;entity.category="World";entity.mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/small.epoch_mesh",vertexHeavy.artifact->key().words,65536u,3u};runtime.m_entities.push_back(entity);}
 runtime.prepare_project_meshes();CHECK(runtime.m_meshes.size()==6&&runtime.m_meshes.contains(44u)&&!runtime.m_meshes.contains(45u));runtime.m_entities=savedEntities;
 // Category and recipe/source failures never expand ordinary imports or load a stale package artifact.
 runtime.m_entities[0].category="World";runtime.prepare_project_meshes();CHECK(runtime.m_meshes.empty());runtime.m_entities[0].category="Planetary Terrain";
 project_planetary::saved.reset();runtime.prepare_project_meshes();CHECK(runtime.m_meshes.empty()&&!runtime.m_meshPlanetarySettings);
 project_planetary::saved=settings;project_planetary::current=false;runtime.prepare_project_meshes();CHECK(runtime.m_meshes.empty());
 const auto revision=editor.sceneDocument.revision();CHECK(!import_project_mesh(editor,artifact,"Planetary Terrain",budget.artifact));CHECK(editor.sceneDocument.revision()==revision&&editor.entities.size()==3);
 project_planetary::current=true;refresh_editor_planetary_mesh_policy(editor);
 auto changed=settings;changed.snapshot_megabytes=4;changed.maximum_leaves=48;changed.cells_per_tile=16;project_planetary::saved=changed;
 const auto priorSnapshot=editor.entities[0].meshSnapshot;
 refresh_editor_planetary_mesh_policy(editor);CHECK(editor.planetaryMeshPolicy->settings==settings);
 CHECK(!editor_planetary_mesh_policy_current(editor)&&editor.planetaryMeshPolicy);
 CHECK(editor_planetary_mesh_budget(editor).cache_bytes==budget.cache_bytes);
 CHECK(editor.entities[0].meshSnapshot==priorSnapshot&&editor.sceneDocument.revision()==revision);
 // A real generation completion first admits its saved target recipe for
 // import. Failed import restores the prior policy in the owning pump.
 refresh_editor_planetary_mesh_policy(editor,true);CHECK(editor.planetaryMeshPolicy->settings==changed);
 CHECK(!editor_planetary_stream_identity_matches(editor,editor.entities[0]));
 refresh_editor_planetary_mesh_policy(editor);CHECK(editor.planetaryMeshPolicy->settings==settings);
 CHECK(!import_project_mesh(editor,artifact,"Planetary Terrain",budget.artifact));CHECK(editor.sceneDocument.revision()==revision);
 project_planetary::saved=settings;runtime.m_entities[0].mesh_asset->index_count-=3;runtime.prepare_project_meshes();CHECK(runtime.m_meshes.empty());
 runtime.m_entities[0].mesh_asset->index_count+=3;const auto local=root/runtime.m_entities[0].mesh_asset->logical_path;auto corrupt=bytes;corrupt.back()^=std::byte{1};{std::ofstream out(local,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(corrupt.data()),std::streamsize(corrupt.size()));}
 runtime.prepare_project_meshes();CHECK(runtime.m_meshes.empty());previewgrid::clear_object_markers(ctx);
 CHECK(project_planetary::sourceChecks>=6&&editor_project_textures::resolution_calls==0);std::fprintf(stderr,"Actual imported/saved/reopened/runtime/published large mesh: %zu vertices / %zu indices / %zu bytes\n",geometry.vertices.size(),geometry.indices.size(),bytes.size());
}
'''


class PlanetaryMeshAdmission(unittest.TestCase):
    def test_actual_editor_import_runtime_load_and_frame(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC actual module CPU lane")
        source = EDIT.read_text(encoding="utf-8")
        functions = "\n".join(owner_function(source, marker) for marker in (
            "[[nodiscard]] project_planetary::SceneMeshBudget editor_planetary_mesh_budget(",
            "[[nodiscard]] bool editor_planetary_mesh_policy_current(",
            "[[nodiscard]] std::optional<project_planetary::MeshGenerationIdentity>",
            "[[nodiscard]] bool editor_planetary_stream_identity_matches(",
            "void refresh_editor_planetary_body_inventory(",
            "[[nodiscard]] bool select_editor_planetary_body(",
            "[[nodiscard]] std::optional<scene_interaction::FocusTarget> editor_planetary_body_focus(",
            "void refresh_editor_planetary_mesh_policy(", "[[nodiscard]] bool import_project_mesh("))
        geometry = GEOMETRY_CODE[GEOMETRY_CODE.index("struct Geometry"):GEOMETRY_CODE.index("int main(")]
        play = PLAY.read_text(encoding="utf-8")
        runtime = definition(play, "            void prepare_project_meshes(")
        legacy = (ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp").read_text(encoding="utf-8")
        publication = "\n".join(owner_function(legacy,marker) for marker in (
            "[[nodiscard]] std::optional<epochengine::previewgrid::Vec3> project_play_position_for_origin(",
            "[[nodiscard]] bool project_play_entity_has_geometry(",
            "[[nodiscard]] std::optional<epochengine::previewgrid::ObjectMarker> project_play_marker_for_entity("))
        publication += "\n" + owner_function(play, "            bool publish_project_play_markers(")
        outliner_source = (ROOT/'Engine/src/editor/application/editor.frame_outliner.inl').read_text(encoding='utf-8')
        body_filter = owner_function(outliner_source, 'const auto containsNoCase = [](') + ';'
        outliner = definition(outliner_source,
            '        if(editor.planetaryMeshPolicy&&!editor.planetaryMeshPolicy->generated_bodies.empty())')
        code = CODE.replace("GEOMETRY", geometry).replace("EDITOR_FUNCTIONS", functions).replace("RUNTIME_FUNCTION", runtime).replace("PUBLICATION", publication).replace('BODY_FILTER',body_filter).replace('BODY_OUTLINER',outliner)
        proof = ROOT / "Artifacts/Reports/planetary-mesh-admission-oct28" / str(time.time_ns())
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        owners = (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules")
        module_map = {}
        for directory in owners:
            for path in directory.glob("*.ixx"):
                found = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if found:
                    self.assertNotIn(found[1], module_map)
                    module_map[found[1]] = path
        pending = re.findall(r"^import\s+([\w.:]+);", code, re.M) + ["authoring.texture_detail"]
        modules = set()
        while pending:
            name = pending.pop()
            if name in modules:
                continue
            modules.add(name)
            self.assertIn(name, module_map)
            pending.extend(re.findall(r"^(?:export\s+)?import\s+([\w.:]+);", module_map[name].read_text(encoding="utf-8-sig"), re.M))
        module_paths = "\n".join('"'+module_map[name].as_posix()+'"' for name in sorted(modules))
        implementations = [ROOT / path for path in ("Engine/src/render/render.preview_camera.cpp","Engine/src/assets/texture/asset.texture_artifact.cpp",
            "Engine/src/authoring/texture/authoring.texture_document.cpp", "Engine/src/authoring/texture/authoring.texture_residency.cpp")]
        fixture = ROOT / "Artifacts/Tests" / f"pma{time.time_ns():x}"
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetaryMeshAdmission LANGUAGES CXX)
add_executable(admission consumer.cpp {" ".join('"'+path.as_posix()+'"' for path in implementations)})
target_compile_features(admission PRIVATE cxx_std_23)
target_compile_options(admission PRIVATE /W3 /WX /utf-8 /EHsc)
target_include_directories(admission PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(admission PRIVATE FILE_SET CXX_MODULES BASE_DIRS {" ".join('"'+path.as_posix()+'"' for path in owners)} FILES {module_paths})
enable_testing()
add_test(NAME actual_planetary_mesh_admission COMMAND admission "{fixture.as_posix()}/data/$<CONFIG>")
set_tests_properties(actual_planetary_mesh_admission PROPERTIES TIMEOUT 60)
''', encoding="utf-8")
        build = fixture / "build"
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1", "--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"], ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            result = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=180)
            log = proof / f"lane-{index}.log"
            log.write_text(result.stdout+result.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": result.returncode, "log": str(log)})
            self.assertEqual(result.returncode, 0, result.stdout[-9000:]+result.stderr)
        (proof / "result.json").write_text(json.dumps({"passed": True, "commands": results,
            "scope": "Actual codec, saved scene/document/runtime modules, extracted actual Editor import/runtime load/publication; recording source/recipe and exclusive-write ports. No GPU/provider/OS-confinement proof.",
            "cases": ["267126 vertices/1587600 indices explicit32MiB import, save, parse, runtime load/frame publication", "ordinary/category/absent recipe/stale source/changed recipe/default frame rejection", "wrong reference counts/tampered artifact", "failed replacement retains scene revision and previous render frame"]}, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
