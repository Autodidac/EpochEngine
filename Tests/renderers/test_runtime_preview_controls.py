"""Actual preview geometry and runtime shortcut owners, without a window/GPU."""
from pathlib import Path
import json
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


def wire_fixture():
    source = (ROOT / "Engine/modules/render/render.preview_grid.ixx").read_text(encoding="utf-8-sig")
    baseline = actual_function(source, "inline std::vector<Vertex> detail::build_marker_solids(")
    baseline = baseline.replace("detail::build_marker_solids(", "baseline_marker_solids(")
    baseline = baseline.replace("std::vector<TexturedVertex>* textured, bool wire_depth)", "std::vector<TexturedVertex>* textured = nullptr, bool wire_depth = false)")
    # The current production implementation centralizes vertex positioning in
    # private helpers. Extract those real bodies into the recording namespace;
    # do not substitute a second transform implementation in this consumer.
    private_helpers = "\n".join(actual_function(source, marker) + (";" if "struct " in marker else "")
        for marker in ("        struct EulerRotation final", "        [[nodiscard]] inline EulerRotation euler_rotation(",
            "        [[nodiscard]] inline Vec3 rotate_euler(", "        [[nodiscard]] inline Vec3 mesh_vertex_position(",
            "        [[nodiscard]] inline Vec3 mesh_vertex_normal(",
            "        [[nodiscard]] inline bool mesh_wire_requested(",
            "        [[nodiscard]] inline bool mesh_source_reverse("))
    lighting_owner = (ROOT / "Engine/src/render/render.preview_lighting.cpp").read_text(encoding="utf-8-sig")
    shadow_limit = source.index("inline constexpr std::size_t kMaxForestShadowVertices =")
    private_helpers += "\n" + source[shadow_limit:source.index(";", shadow_limit) + 1]
    private_helpers += "\n" + actual_function(lighting_owner, "        struct ForestShadow final") + ";\n"
    private_helpers += "\n".join(actual_function(lighting_owner, marker)
        for marker in ("        [[nodiscard]] static const ForestShadow* forest_shadow_for(",
            "        [[nodiscard]] const ForestShadow* prepare_forest_shadow(",
            "        [[nodiscard]] float forest_shadow_visibility("))
    baseline = baseline.replace("detail::", "baseline_detail::")
    # Keep the pre-hoist reference cost: compute the exact owning Euler values
    # for each transformed source vertex, rather than changing that benchmark
    # when the implementation gains the shared positioning helper.
    baseline = baseline.replace("source.position,rotation)",
        "source.position,baseline_detail::euler_rotation(marker.rotationDegrees))")
    baseline = baseline.replace("baseline_detail::rotate_euler(\n", "rotate_euler_degrees(\n")
    baseline = baseline.replace("source.normal[2] / marker.scale.z}, rotation));",
        "source.normal[2] / marker.scale.z}, marker.rotationDegrees));")
    baseline = "namespace baseline_detail { using namespace epochengine::previewgrid;\n" + private_helpers + "\n}\n" + baseline
    play = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8-sig")
    exploration_begin = play.index("            void advance_planetary_exploration(")
    exploration_camera_begin = play.index("                auto camera=previewgrid::camera_for(context);", exploration_begin)
    exploration_camera = play[exploration_camera_begin:play.index("                camera.farPlane=", exploration_camera_begin)]
    begin = play.index("                    bool completeManagedFrame=")
    managed_gate = play[begin:play.index(";", begin) + 1]
    assert "const bool published=completeManagedFrame&&frameLighting&&publish_project_play_markers(" in play
    contracts = (ROOT / "Engine/src/epoch/implementation/epoch.contracts_engine.inl").read_text(encoding="utf-8-sig")
    routing = contracts[contracts.index("        const auto cubePreviewRoute ="):
        contracts.index("        const auto forestTrunkRoute =")].replace("        check(", "        CHECK_ROUTING(", 1)
    preview_owner = (ROOT / "Engine/src/editor/application/editor.application_scene_preview.inl").read_text(encoding="utf-8")
    preview_pump = actual_function(preview_owner, "        [[nodiscard]] bool pump_editor_planetary_preview(")
    paint_focus = actual_function(preview_owner,"        [[nodiscard]] bool focus_forest_paint_region(")
    focus_owner = (ROOT / "Engine/src/editor/application/editor.application_scripting_renderers.inl").read_text(encoding="utf-8")
    paint_focus_dispatch = actual_function(focus_owner,"                if(index&&state.mainSurface==EditorMainSurface::ForestFactory&&state.forestTerrainPainting")
    editor_framing = actual_function(preview_owner,"        [[nodiscard]] bool frame_editor_scene_target(")
    generic_focus = actual_function(focus_owner,'            if (toolId == "focus_selection")')
    generic_focus = generic_focus[generic_focus.index('{')+1:generic_focus.index('#if defined(_WIN32)')]
    cache_helpers = actual_function(preview_owner,'        struct EditorPreviewMeshCacheUsage')+';\n'
    cache_helpers += '\n'.join(actual_function(preview_owner,marker)for marker in (
        '        [[nodiscard]] std::optional<EditorPreviewMeshCacheUsage> editor_preview_mesh_cache_usage(',
        '        [[nodiscard]] bool restore_editor_preview_mesh(',
        '        [[nodiscard]] bool ensure_editor_forest_focus_mesh('))
    selection_owner = (ROOT / 'Engine/src/editor/application/editor.application_scene_selection.inl').read_text(encoding='utf-8')
    scene_interaction_owner = actual_function(selection_owner,'        [[nodiscard]] bool ensure_editor_scene_interaction(')
    preview_caller = actual_function(preview_owner, "        void publish_editor_preview_markers(")
    assert 'restore_editor_preview_mesh(state,entity,meshBudget,awaitingPlanetaryBudget,*meshUsage)' in preview_caller
    caller_entry = re.search(r"const bool meshWireframe\s*=.*?;",preview_caller,re.S).group(0)
    caller_dispatch = re.search(r"published=pump_editor_planetary_preview\(.*?;",preview_caller,re.S).group(0)
    direct_branch = actual_function(preview_caller[preview_caller.index(caller_dispatch):], "\n            else\n")
    # This fixture supplies assembled markers and exercises publication and retirement.
    direct_begin = direct_branch.index("                // Inline previews replace")
    caller_direct = "{\n" + direct_branch[direct_begin:]
    caller_wire = re.search(r"\.wireframe\s*=\s*(forestPainting\s*\?[^,]+),",preview_caller).group(1)
    documents = (ROOT / "Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8")
    preview_reset = actual_function(documents,"        void reset_planetary_preview_publication(")
    preview_retire = actual_function(documents,"        void retire_planetary_preview_publication(")
    bootstrap = (ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl").read_text(encoding="utf-8")
    retire_call = re.search(r"retire_planetary_preview_publication\(editor\);",bootstrap).group(0)
    assert "reset_planetary_preview_publication(state);" in actual_function(documents,"        void reset_project_bound_editor_state(")
    state = (ROOT / "Engine/src/editor/application/editor.application_state.inl").read_text(encoding="utf-8")
    state_begin = state.index("            std::optional<std::future<std::optional<previewgrid::PreparedSceneFrame>>> planetaryPreviewPending")
    state_end = state.index("            bool planetaryPreviewWireframe{};", state_begin) + len("            bool planetaryPreviewWireframe{};")
    preview_fields = state[state_begin:state_end]
    runtime_publication = '\n'.join(actual_function(play, marker) + (';' if 'struct ' in marker else '')
        for marker in ('            struct RuntimeSceneFrameResult final',
            '            struct PlanetaryFramePublication final',
            '            [[nodiscard]] bool planetary_frame_preparing(',
            '            [[nodiscard]] bool pump_runtime_planetary_frame('))
    retirement=actual_function(play,'            void shutdown_planetary_runtime() noexcept')
    retirement=retirement[retirement.index('                    if(m_planetaryScheduler)'):
        retirement.index('                    m_planetaryFramePublications.clear();')+len('                    m_planetaryFramePublications.clear();')]
    fixture = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <future>
#include <memory>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <source_location>
#include <stop_token>
#include <string>
#include <string_view>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
import asset.mesh_artifact;
import render.preview_grid;
import render.device;
import authoring.texture;
import render.texture_artifact;
import render.lighting;
import render.arcade;
import visuals.engine;
import editor.task_scheduler;
import scene.snapshot;
import scene.interaction;
import project.planetary;
bool deny_all{},deny_large{};unsigned blocked{};std::atomic<void(*)()> allocation_hook{};
thread_local bool skip_allocation_hook{};
void* operator new(std::size_t count){if(deny_all||(deny_large&&count>65536)){++blocked;throw std::bad_alloc{};}
 if(!skip_allocation_hook&&count>=4096)if(const auto hook=allocation_hook.exchange(nullptr))hook();
 if(auto* value=std::malloc(count?count:1))return value;throw std::bad_alloc{};}
void* operator new[](std::size_t count){return ::operator new(count);}
void operator delete(void* value)noexcept{std::free(value);}void operator delete[](void* value)noexcept{std::free(value);}
void operator delete(void* value,std::size_t)noexcept{std::free(value);}void operator delete[](void* value,std::size_t)noexcept{std::free(value);}
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"Contract failed at line %d: %s\n",__LINE__,#value); return __LINE__; } } while(false)
#define CHECK_ROUTING(name,...) CHECK((__VA_ARGS__))
using namespace epochengine::previewgrid;
namespace render_arcade=epochengine::render_arcade;
BASELINE_OWNER
namespace previewgrid=epochengine::previewgrid;
namespace editor_tasks=epochengine::editor_tasks;
namespace scene=epochengine::scene;
namespace scene_interaction=epochengine::scene_interaction;
namespace project_planetary=epochengine::project_planetary;
namespace asset=epochengine::asset;
namespace timing {using Clock=std::chrono::steady_clock;}
namespace logger {
enum class LogLevel{INFO};
struct Timing {double queue_ms{},prepare_ms{},commit_ms{},total_ms{};bool committed{};std::string outcome;};
inline std::vector<Timing> evidence;
struct Sink{void logf(LogLevel,std::source_location,const char*,double queue,double prepare,double commit,double total,bool published,std::string_view outcome={}){
 evidence.push_back({queue,prepare,commit,total,published,std::string{outcome}});}};
inline Sink& get(const char*){static Sink sink;return sink;}
}
struct RuntimePublicationProbe{
 struct {std::uint32_t anchor{};}m_explorationFrame;
 bool m_runtimeWaterVisible{true},m_runtimeCloudsVisible{true};
 std::unique_ptr<editor_tasks::Scheduler>m_planetaryScheduler=std::make_unique<editor_tasks::Scheduler>(
  editor_tasks::SchedulerLimits{.worker_count=1,.retained_finished_nodes=32});
 RUNTIME_PUBLICATION
 std::unordered_map<const void*,PlanetaryFramePublication>m_planetaryFramePublications;
 std::optional<std::future<int>>m_planetaryPending,m_planetaryWaterPending;
 editor_tasks::CancellationTicket m_planetaryCancellation,m_planetaryWaterCancellation;
 void actual_retire(){RUNTIME_RETIREMENT}
 void ensure_planetary_scheduler(){(void)m_planetaryScheduler->maintain();}
};
enum class EditorMainSurface{Scene,ForestFactory};
struct EditorEntity{
 scene::SceneObjectId sceneObjectId{};std::string category,name{"Object"},type{"IndexedMesh"};
 std::array<float,3>position{},rotation{},scale{1,1,1};
 bool visible{true},editorOnly{},meshLoadAttempted{};
 std::shared_ptr<const asset::mesh::Artifact>meshSnapshot;
 std::optional<scene::SceneMeshCoordinates>meshSnapshotFrame;
 std::optional<std::array<double,3>>positionAnchor;
 std::optional<scene::SceneMeshSnapshot>meshAsset;
 std::optional<scene::SceneForestRegionInstance>forestRegion;
};
struct EditorState{
 struct Lifecycle{
  struct Evidence{struct Generations{struct Selected{std::uint64_t generation{1};}selected_project;}generations;}ledger;
  const auto& evidence()const{return ledger;}
 }projectLifecycle;
 std::string projectRoot{"project"},projectStatus;
 std::uint64_t sceneDocumentRevision{1},selectedEntityId{};
 std::vector<std::uint64_t>selectedEntityIds;
 EditorMainSurface mainSurface{EditorMainSurface::Scene};bool forestTerrainPainting{};
 std::vector<EditorEntity>entities;
 scene_interaction::SceneInteractionQuery sceneInteraction;
 std::array<double,3>sceneInteractionOrigin{};bool sceneInteractionDirty{true};
 project_planetary::SceneMeshBudget meshBudget;
 bool viewportMeshWireframe{};
 std::optional<bool>viewportMeshWireframeRequest;
 std::unique_ptr<editor_tasks::Scheduler>taskScheduler=std::make_unique<editor_tasks::Scheduler>(
  editor_tasks::SchedulerLimits{.worker_count=1,.retained_finished_nodes=32});
 std::vector<std::string>logs;
 PREVIEW_FIELDS
};
void push_editor_log(EditorState&state,const std::string&text){state.logs.push_back(text);}
std::optional<std::size_t>editor_entity_index(const EditorState&state,scene::SceneObjectId id){
 for(std::size_t i=0;i<state.entities.size();++i)if(state.entities[i].sceneObjectId==id)return i;return {};
}
template<class Operation>auto schedule_editor_cancellable_task(EditorState&state,std::string label,Operation&&operation){
 return state.taskScheduler->submit_cancellable(std::move(label),std::forward<Operation>(operation));
}
std::filesystem::path resolve_editor_path(const std::filesystem::path&path){return path;}
void refresh_editor_planetary_body_inventory(EditorState&,const EditorEntity&){}
project_planetary::SceneMeshBudget editor_planetary_mesh_budget(const EditorState&state){return state.meshBudget;}
void synchronize_editor_selection(EditorState&){}
bool editor_planetary_outliner_only(const EditorState&,const EditorEntity&){return false;}
bool editor_has_selection(const EditorState&state){return state.selectedEntityId!=0;}
scene::SceneSnapshot capture_editor_scene_projection(const EditorState&state){
 scene::SceneSnapshot snapshot;snapshot.scene_id="forest-focus";snapshot.project_id="project";
 for(const auto&entity:state.entities){scene::SceneObjectSnapshot object;
  object.id=entity.sceneObjectId;object.name=entity.name;object.type=entity.type;object.category=entity.category;
  object.position=entity.position;object.rotation=entity.rotation;object.scale=entity.scale;
  object.position_anchor=entity.positionAnchor;object.mesh_asset=entity.meshAsset;
  object.visible=entity.visible;object.editor_only=entity.editorOnly;snapshot.objects.push_back(object);}
 return snapshot;
}
std::optional<scene_interaction::FocusTarget>editor_planetary_body_focus(const EditorState&,const EditorEntity&){return {};}
EDITOR_FRAMING
CACHE_HELPERS
SCENE_INTERACTION_OWNER
PREVIEW_PUMP
PREVIEW_RETIRE
PREVIEW_RESET
PAINT_FOCUS
void actual_forest_focus_selection(const void*ctx,EditorState&state){
 const auto index=editor_entity_index(state,state.selectedEntityId);
 PAINT_FOCUS_DISPATCH
}
void actual_generic_focus_selection(const void*ctx,EditorState&state){
 GENERIC_FOCUS
 (void)focused;
}
int verify_forest_focus_exact_mesh(){
 namespace mesh=asset::mesh;namespace fs=std::filesystem;
 const auto root=fs::absolute(fs::path{
#ifdef NDEBUG
 "forest-focus-cache-Release"
#else
 "forest-focus-cache-Debug"
#endif
 });fs::create_directories(root/"Assets/Mesh");
 std::array<mesh::Vertex,8>vertices{};
 for(unsigned i=0;i<8;++i)vertices[i].position={(i&1)?1.5964558f:-1.6706042f,(i&2)?4.522974f:0.0f,(i&4)?1.9171542f:-1.954205f};
 const std::array<std::uint32_t,36>indices{0,1,2,1,3,2,4,6,5,5,6,7,0,4,1,1,4,5,2,3,6,3,7,6,0,2,4,2,6,4,1,5,3,3,5,7};
 const auto built=mesh::build_artifact(vertices,indices);CHECK(built);
 const auto bytes=mesh::encode_artifact(*built.artifact);
 {std::ofstream file{root/"Assets/Mesh/focus.epoch_mesh",std::ios::binary|std::ios::trunc};
  file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));CHECK(file.good());}
 EditorEntity tree;tree.sceneObjectId=7;tree.name="Tree";tree.category="ForestFactory";
 tree.rotation={178.587189f,45.5128403f,-116.996452f};
 tree.positionAnchor=std::array<double,3>{3981266.0319582061,2993450.0744901774,3980586.9828443839};
 tree.meshAsset=scene::SceneMeshSnapshot{"Assets/Mesh/focus.epoch_mesh",built.artifact->key().words,8,36};
 tree.meshLoadAttempted=true; // Earlier nonresident/refused draw is an explicit Focus retry.
 int context{};EditorState state;state.projectRoot=root.string();state.entities={tree};state.selectedEntityId=7;
 CHECK(ensure_editor_scene_interaction(state));
 const auto fallback=state.sceneInteraction.focus_target({7,1});CHECK(fallback&&fallback->radius*3.25f<4.43f);
 actual_generic_focus_selection(&context,state);
 CHECK(state.entities[0].meshSnapshot&&state.entities[0].meshSnapshot->key()==built.artifact->key());
 CHECK(!state.sceneInteractionDirty&&camera_distance_for(&context)>17.3f);
 const auto real=state.sceneInteraction.focus_target({7,1});CHECK(real&&real->radius>5.3f);
 const auto view=capture_camera_rig_snapshot(&context);
 const std::array target{real->target.x,real->target.y,real->target.z};
 for(unsigned axis=0;axis<3;++axis)CHECK(view.navigation_origin[axis]==state.sceneInteractionOrigin[axis]+double(target[axis]));
 CHECK(state.entities[0].positionAnchor==tree.positionAnchor);
 auto usage=editor_preview_mesh_cache_usage(state.entities,state.meshBudget,false);CHECK(usage&&usage->entries==1);
 const auto cached=state.entities[0].meshSnapshot;const auto retainedBytes=usage->bytes;
 CHECK(restore_editor_preview_mesh(state,state.entities[0],state.meshBudget,false,*usage));
 CHECK(state.entities[0].meshSnapshot==cached&&usage->bytes==retainedBytes&&usage->entries==1);
 // Same exact read admission refuses insufficient cache, bad content and
 // declared-count mismatch without moving the camera to substitute bounds.
 for(unsigned rejected=0;rejected<3;++rejected){
  EditorState refused;refused.projectRoot=root.string();refused.entities={tree};refused.selectedEntityId=7;
  if(rejected==0)refused.meshBudget.cache_bytes=1;
  if(rejected==1)refused.entities[0].meshAsset->artifact_key[0]^=1;
  if(rejected==2)++refused.entities[0].meshAsset->vertex_count;
  const auto before=capture_camera_rig_snapshot(&context);actual_generic_focus_selection(&context,refused);
  CHECK(!refused.entities[0].meshSnapshot&&capture_camera_rig_snapshot(&context).revision==before.revision);
  CHECK(refused.logs.back().find("Focus was not applied")!=std::string::npos);
 }
 // Nonforest objects keep their existing Focus path, even with no residency.
 EditorState plain;plain.projectRoot=root.string();plain.entities={tree};plain.selectedEntityId=7;
 plain.entities[0].category="Imported";plain.entities[0].meshAsset->artifact_key[0]^=1;
 actual_generic_focus_selection(&context,plain);
 CHECK(!plain.entities[0].meshSnapshot&&camera_distance_for(&context)<4.43f);
 cleanup_context(&context);return 0;
}
int verify_forest_paint_focus_parent(){
 int context{};EditorState state;state.mainSurface=EditorMainSurface::ForestFactory;state.forestTerrainPainting=true;
 EditorEntity terrain;terrain.sceneObjectId=3;terrain.category="Planetary Terrain";
 terrain.position={4875653.5f,0,52489.25f};terrain.meshAsset.emplace();terrain.meshAsset->artifact_key={1,2,3,4};
 EditorEntity tree;tree.sceneObjectId=7;
 tree.positionAnchor=std::array<double,3>{3981266.0319582061,2993450.0744901774,3980586.9828443839};
 scene::SceneForestRegionInstance region;
 region.region_id=9;region.terrain_object_id=3;region.plant_genome=1;region.plant_revision=104;region.plant_hash=1;
 region.candidate_count=1;region.radius_meters=12;region.density_per_square_meter=.01;region.body_radius_meters=6371000;
 region.center_direction={.62439113264554114,.46947164430025656,.62428526225727266};
 region.body_local_root_meters=*tree.positionAnchor;
 region.radial_up={.62439195318945395,.46947029507262511,.62428545621108389};
 region.admitted_body_center_meters={1025656447.6465962,51685790.584686734,1291455547.2228227};
 region.mesh_source_origin_meters=region.admitted_body_center_meters;
 region.terrain_artifact_key={1,2,3,4};region.provider_content_hash=std::string(64,'a');
 region.recipe="planet";region.plant_source="Assets/Forest/default.epoch_forest";CHECK(region.valid());tree.forestRegion=region;
 state.entities={terrain,tree};state.selectedEntityId=7;
 const auto anchor=tree.positionAnchor;
 for(unsigned transformed=0;transformed<2;++transformed){
  if(transformed){state.entities[0].positionAnchor=std::array<double,3>{1e9+.125,-1e9+.25,1e9+.5};
   state.entities[0].rotation={17,31,59};state.entities[0].scale={2,.5f,3};}
  const auto&parent=state.entities[0];const auto pivot=scene::world_position(parent.position,parent.positionAnchor);CHECK(pivot);
  const auto transform=make_precise_mesh_transform(*pivot,{parent.scale[0],parent.scale[1],parent.scale[2]},
   {parent.rotation[0],parent.rotation[1],parent.rotation[2]});CHECK(transform);
  const auto expected=transform->apply(region.body_local_root_meters);
  CHECK(focus_camera_precise(&context,{0,0,0},11036646.0f));
  actual_forest_focus_selection(&context,state);
  const auto view=capture_camera_rig_snapshot(&context);CHECK(view.valid&&view.navigation_origin==expected);
  CHECK(view.mode==CameraMode::Editor&&view.distance>=39&&view.roll_degrees==0);
  const auto camera=camera_for(&context);const auto eye=camera_world_eye(camera);
  std::array<double,3>normal{};
  for(unsigned row=0;row<3;++row)for(unsigned axis=0;axis<3;++axis)
   normal[row]+=transform->rotation[row*3+axis]*region.radial_up[axis]/transform->scale[axis];
  const auto length=std::hypot(normal[0],normal[1],normal[2]);double outward{};
  for(unsigned axis=0;axis<3;++axis){normal[axis]/=length;outward+=(eye[axis]-expected[axis])*normal[axis];}
  CHECK(outward>double(view.distance)*.99999);
  CHECK(dot(camera_forward(camera),Vec3{float(normal[0]),float(normal[1]),float(normal[2])})<-.99999f);
  CHECK(state.entities[1].positionAnchor==anchor&&state.entities[1].forestRegion==region);
 }
 const auto before=capture_camera_rig_snapshot(&context);
 state.entities[1].forestRegion->terrain_artifact_key[0]=77;
 actual_forest_focus_selection(&context,state);
 CHECK(capture_camera_rig_snapshot(&context).revision==before.revision);
 CHECK(state.logs.back().find("no longer matches")!=std::string::npos);
 cleanup_context(&context);return 0;
}
void actual_editor_bootstrap_retire(EditorState&state){auto&editor=state;RETIRE_CALL}
// Actual caller's mode selection, marker-mode expression and dispatch. The
// recorded source scene replaces only unrelated loading/lighting setup here.
bool actual_editor_preview_frame(const void*ctx,EditorState&state,SceneFrameRequest request){
 actual_editor_bootstrap_retire(state);
 const bool forestPainting=false;
 CALLER_ENTRY
 for(auto&marker:request.markers)marker.wireframe=CALLER_WIRE;
 bool published{};
 CALLER_DISPATCH
 return published;
}
bool actual_editor_inline_frame(const void*ctx,EditorState&state,std::vector<ObjectMarker>markers){
 const auto meshBudget=state.meshBudget;bool published{};
 CALLER_DIRECT
 return published;
}
int verify_editor_planet_after_inline_forest(){
 namespace mesh=epochengine::asset::mesh;
 const std::array<mesh::Vertex,3>vertices{{{{0,0,0}},{{1,0,0}},{{0,1,0}}}};
 const std::array<std::uint32_t,3>indices{0,1,2};const auto planet=mesh::build_artifact(vertices,indices);CHECK(planet);
 int context{};EditorState state;SceneFrameRequest request;
 request.markers.push_back(ObjectMarker{.primitive=ObjectPreviewPrimitive::IndexedMesh,.mesh=planet.artifact});
 CHECK(!actual_editor_preview_frame(&context,state,request)&&state.planetaryPreviewPending);
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(actual_editor_preview_frame(&context,state,request)&&!state.planetaryPreviewPending);
 const auto originalRequest=state.planetaryPreviewRequest;
 const std::vector<ObjectMarker>forest{ObjectMarker{.position={40,0,0},.primitive=ObjectPreviewPrimitive::Cube}};
 state.mainSurface=EditorMainSurface::ForestFactory;
 CHECK(actual_editor_inline_frame(&context,state,forest));
 CHECK(!scene_frame_for(&context)); // Direct Forest publication replaced the planetary packet.
 state.mainSurface=EditorMainSurface::Scene;
 CHECK(!actual_editor_preview_frame(&context,state,request)&&state.planetaryPreviewPending);
 CHECK(state.planetaryPreviewRequest!=originalRequest);
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(actual_editor_preview_frame(&context,state,request)&&!state.planetaryPreviewPending);
 const auto restoredRequest=state.planetaryPreviewRequest;const auto restored=scene_frame_for(&context);CHECK(restored);
 CHECK(restored->markers.size()==1&&restored->markers[0].mesh==planet.artifact);
 for(unsigned turn=0;turn<8u;++turn){
  CHECK(!actual_editor_preview_frame(&context,state,request)&&!state.planetaryPreviewPending);
  CHECK(state.planetaryPreviewRequest==restoredRequest&&scene_frame_for(&context)->solids==restored->solids);
 }
 // Both a queued job and an already completed old packet are invalidated by
 // inline publication. A later return cannot consume their retained geometry.
 for(bool alreadyReady:{false,true}){
  request.markers[0].position.x+=1;
  std::promise<void>entered,release;auto released=release.get_future().share();
  auto blocker=state.taskScheduler->submit("Hold planetary work across inline Forest",[&]{entered.set_value();released.wait();});
  CHECK(entered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(!actual_editor_preview_frame(&context,state,request)&&state.planetaryPreviewPending);
  if(alreadyReady){release.set_value();blocker.get();CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);}
  state.mainSurface=EditorMainSurface::ForestFactory;
  CHECK(actual_editor_inline_frame(&context,state,forest));
  CHECK(!state.planetaryPreviewRequest&&!scene_frame_for(&context));
  if(!alreadyReady){
   release.set_value();blocker.get();
   if(state.planetaryPreviewPending)CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  }
  actual_editor_bootstrap_retire(state);
  CHECK(!state.planetaryPreviewPending&&!scene_frame_for(&context));
  CHECK(detail::g_objectMarkers.at(&context).markers[0].position.x==40);
  state.mainSurface=EditorMainSurface::Scene;
  CHECK(!actual_editor_preview_frame(&context,state,request)&&state.planetaryPreviewPending);
  CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(actual_editor_preview_frame(&context,state,request)&&!state.planetaryPreviewPending);
  CHECK(scene_frame_for(&context)->markers[0].position.x==request.markers[0].position.x);
 }
 cleanup_context(&context);return 0;
}
int verify_editor_wire_request_lifetime(){
 namespace mesh=epochengine::asset::mesh;
 std::array<mesh::Vertex,3>vertices{};
 vertices[0].position={0,0,0};vertices[1].position={1,0,0};vertices[2].position={0,1,0};
 const std::array<std::uint32_t,3>indices{0,1,2};const auto built=mesh::build_artifact(vertices,indices);CHECK(built);
 int context{};EditorState state;SceneFrameRequest request;
 request.markers.push_back(ObjectMarker{.primitive=ObjectPreviewPrimitive::IndexedMesh,.mesh=built.artifact});
 CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting));
 // The first request survives subsequent real caller turns. The dropdown
 // stays Solid until the immutable wire packet is actually committed.
 std::promise<void>entered,release;auto released=release.get_future().share();
 auto blocker=state.taskScheduler->submit("Hold wire preparation",[&]{entered.set_value();released.wait();});
 CHECK(entered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 state.viewportMeshWireframeRequest=true;
 CHECK(!actual_editor_preview_frame(&context,state,request));
 const auto ticket=state.planetaryPreviewCancellation;
 for(unsigned frame=0;frame<20;++frame){
  CHECK(!actual_editor_preview_frame(&context,state,request));
  CHECK(state.viewportMeshWireframeRequest==true&&!state.viewportMeshWireframe);
  CHECK(state.planetaryPreviewCancellation.node==ticket.node);
 }
 release.set_value();blocker.get();
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(actual_editor_preview_frame(&context,state,request));
 CHECK(state.viewportMeshWireframe&&!state.viewportMeshWireframeRequest&&!state.planetaryPreviewPending);
 CHECK(scene_frame_for(&context)->wire->depth_triangles->size()==3&&scene_frame_for(&context)->solids->geometry->empty());
 // Resize invalidates a queued narrow-view fit before it can publish into a
 // wider viewport, then admits the current aspect through the same owner.
 std::promise<void>resizeEntered,resizeRelease;auto resizeReleased=resizeRelease.get_future().share();
 auto resizeBlocker=state.taskScheduler->submit("Hold editor resize",[&]{resizeEntered.set_value();resizeReleased.wait();});
 CHECK(resizeEntered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 request.depth_fit_aspect=1;CHECK(!actual_editor_preview_frame(&context,state,request));
 const auto narrowRequest=state.planetaryPreviewRequest;
 request.depth_fit_aspect=2;CHECK(!actual_editor_preview_frame(&context,state,request));
 resizeRelease.set_value();resizeBlocker.get();
 for(unsigned turn=0;turn<4&&state.planetaryPreviewPending;++turn){
  CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  const bool resized=actual_editor_preview_frame(&context,state,request);
  if(resized)CHECK(state.planetaryPreviewRequest!=narrowRequest&&state.planetaryPreviewRequest->depth_fit_aspect==2);
 }
 CHECK(!state.planetaryPreviewPending&&state.planetaryPreviewRequest->depth_fit_aspect==2);
 // A second explicit toggle supersedes the queued first one. Neither its
 // completion nor its cancellation may roll the committed mode backwards.
 std::promise<void>entered2,release2;auto released2=release2.get_future().share();
 auto blocker2=state.taskScheduler->submit("Hold superseded wire preparation",[&]{entered2.set_value();released2.wait();});
 CHECK(entered2.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 state.viewportMeshWireframeRequest=false;CHECK(!actual_editor_preview_frame(&context,state,request));
 state.viewportMeshWireframeRequest=true;CHECK(!actual_editor_preview_frame(&context,state,request));
 CHECK(state.viewportMeshWireframe&&state.viewportMeshWireframeRequest==true);
 release2.set_value();blocker2.get();
 bool committed{};
 for(unsigned frame=0;frame<4&&!committed;++frame){
  CHECK(state.planetaryPreviewPending&&state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  committed=actual_editor_preview_frame(&context,state,request);CHECK(state.viewportMeshWireframe);
 }
 CHECK(committed&&!state.viewportMeshWireframeRequest&&!state.planetaryPreviewPending);
 // Refusal clears only the pending choice, not the last accepted wire packet.
 request.limits.maximum_bytes=0;state.viewportMeshWireframeRequest=false;
 CHECK(!actual_editor_preview_frame(&context,state,request));
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(!actual_editor_preview_frame(&context,state,request));
 CHECK(!state.planetaryPreviewPending&&!state.viewportMeshWireframeRequest&&state.viewportMeshWireframe);
 CHECK(state.logs.size()==1&&scene_frame_for(&context)->wire->depth_triangles->size()==3);
 // The actual project reset retires its pending choice and invalidates an
 // old completion even when the same project is immediately reopened.
 request.limits={};std::promise<void>entered3,release3;auto released3=release3.get_future().share();
 auto blocker3=state.taskScheduler->submit("Hold project reset preparation",[&]{entered3.set_value();released3.wait();});
 CHECK(entered3.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 state.viewportMeshWireframeRequest=false;CHECK(!actual_editor_preview_frame(&context,state,request));
 reset_planetary_preview_publication(state);
 CHECK(!state.viewportMeshWireframeRequest&&!state.planetaryPreviewRequest&&state.viewportMeshWireframe);
 release3.set_value();blocker3.get();committed=actual_editor_preview_frame(&context,state,request);
 for(unsigned frame=0;frame<4&&!committed;++frame){
  CHECK(state.planetaryPreviewPending&&state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  committed=actual_editor_preview_frame(&context,state,request);CHECK(state.viewportMeshWireframe);
 }
 CHECK(committed&&!state.viewportMeshWireframeRequest&&!state.planetaryPreviewPending&&state.logs.size()==1);
 cleanup_context(&context);return 0;
}
int verify_retired_editor_preview_result(){
 int context{};SceneFrameRequest request;request.markers.push_back(ObjectMarker{});
 CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting));
 for(bool alreadyReady:{false,true}){
  EditorState state;auto source=capture_scene_frame_source(&context);CHECK(source);
  auto prepared=prepare_scene_frame(*source,request.markers,request.camera,request.lighting);CHECK(prepared);
  const std::weak_ptr retained{prepared->replacement->scene_frame};
  std::promise<std::optional<PreparedSceneFrame>>completion;
  state.planetaryPreviewPending.emplace(completion.get_future());
  state.planetaryPreviewRequest=std::make_shared<const SceneFrameRequest>(request);
  state.planetaryPreviewSceneRevision=state.sceneDocumentRevision;
  if(alreadyReady)completion.set_value(std::move(prepared));
  actual_editor_bootstrap_retire(state);
  CHECK(state.planetaryPreviewPending&&!retained.expired()); // Valid work is not discarded.
  reset_planetary_preview_publication(state);
  if(!alreadyReady){
   CHECK(state.planetaryPreviewPending); // Reset never waits on active work.
   actual_editor_bootstrap_retire(state);CHECK(state.planetaryPreviewPending);
   completion.set_value(std::move(prepared));
   actual_editor_bootstrap_retire(state); // Works without a Scene/planetary pump.
  }
  CHECK(!state.planetaryPreviewPending&&!state.planetaryPreviewRequest&&retained.expired());
 }
 cleanup_context(&context);return 0;
}
int verify_editor_planetary_preview_queue(){
 int context{};EditorState state;
 SceneFrameRequest request;request.markers.push_back(ObjectMarker{});
 CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting));
 const auto original=scene_frame_for(&context);
 std::promise<void>entered,release;auto released=release.get_future().share();
 auto blocker=state.taskScheduler->submit("Hold sole worker for deterministic queue test",[&]{entered.set_value();released.wait();});
 CHECK(entered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(!pump_editor_planetary_preview(&context,state,request,false)&&state.planetaryPreviewPending);
 const auto ticket=state.planetaryPreviewCancellation;
 const auto firstRequest=state.planetaryPreviewRequest;
 for(unsigned repeat=0;repeat<20;++repeat){
  CHECK(!pump_editor_planetary_preview(&context,state,request,false));
  CHECK(state.planetaryPreviewRequest==firstRequest&&state.planetaryPreviewCancellation.node==ticket.node);
 }
 request.camera.eye.x+=1;request.camera.target.x+=1;
 CHECK(!pump_editor_planetary_preview(&context,state,request,false));
 CHECK(state.planetaryPreviewRequest==firstRequest&&scene_frame_for(&context)->solids==original->solids);
 release.set_value();blocker.get();
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(pump_editor_planetary_preview(&context,state,request,false)&&state.planetaryPreviewPending);
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(pump_editor_planetary_preview(&context,state,request,false)&&!state.planetaryPreviewPending);
 CHECK(!pump_editor_planetary_preview(&context,state,request,false)&&!state.planetaryPreviewPending);
 // Stale project/scene/selection/wireframe completions never become visible.
 for(unsigned stale=0;stale<5;++stale){
  auto source=capture_scene_frame_source(&context);CHECK(source);
  auto obsolete=request;obsolete.markers[0].position.x=100+float(stale);
  auto prepared=prepare_scene_frame(*source,obsolete.markers,obsolete.camera,obsolete.lighting);CHECK(prepared);
  std::promise<std::optional<PreparedSceneFrame>>completion;
  state.planetaryPreviewPending.emplace(completion.get_future());
  state.planetaryPreviewRequest=std::make_shared<const SceneFrameRequest>(obsolete);
  state.planetaryPreviewProjectRoot=state.projectRoot;
  state.planetaryPreviewProjectGeneration=state.projectLifecycle.evidence().generations.selected_project.generation;
  state.planetaryPreviewSceneRevision=state.sceneDocumentRevision;
  state.planetaryPreviewSelection=state.selectedEntityId;state.planetaryPreviewSelections=state.selectedEntityIds;
  state.planetaryPreviewWireframe=false;
  if(stale==0)++state.projectLifecycle.ledger.generations.selected_project.generation;
  if(stale==1)++state.sceneDocumentRevision;
  if(stale==2)++state.selectedEntityId;
  if(stale==3)state.selectedEntityIds.push_back(55);
  completion.set_value(std::move(prepared));
  CHECK(!pump_editor_planetary_preview(&context,state,request,stale==4));
  CHECK(scene_frame_for(&context)->markers[0].position.x!=obsolete.markers[0].position.x);
  CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(pump_editor_planetary_preview(&context,state,request,stale==4));
 }
 // Admission failure retains the frame and reports once instead of resending
 // the same expensive request every owner turn.
 request.markers[0].primitive=ObjectPreviewPrimitive::IndexedMesh;
 CHECK(!pump_editor_planetary_preview(&context,state,request,false)&&state.planetaryPreviewPending);
 CHECK(state.planetaryPreviewPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
 CHECK(!pump_editor_planetary_preview(&context,state,request,false)&&!state.planetaryPreviewPending);
 CHECK(state.logs.size()==1&&!state.projectStatus.empty());
 CHECK(!pump_editor_planetary_preview(&context,state,request,false)&&!state.planetaryPreviewPending&&state.logs.size()==1);
 cleanup_context(&context);return 0;
}
// Preserve the former production per-index math as a byte-exact reference;
// Compare surface colors/positions with the former per-index math, then
// independently normalize its triangle order to the shared clockwise contract.
auto per_index_reference(const epochengine::previewgrid::ObjectMarker& marker,
 const epochengine::lighting::LightingFrame& lights,const epochengine::previewgrid::Camera& camera) {
 namespace preview=epochengine::previewgrid;namespace lighting=epochengine::lighting;
 auto color=marker.selected?preview::visual_rgb(epochengine::visuals::object_selected()):marker.color;
 if(marker.editorOnly&&!marker.selected)color=preview::scale(color,epochengine::visuals::editor_solid_opacity_factor());
 std::vector<preview::Vertex> output;
 for(const auto index:marker.mesh->indices()) {
  const auto& source=marker.mesh->vertices()[index];
  const preview::Vec3 local{source.position[0]*marker.scale.x,source.position[1]*marker.scale.y,source.position[2]*marker.scale.z};
  const auto position=preview::add(marker.position,preview::rotate_euler_degrees(local,marker.rotationDegrees));
  const auto normal=preview::normalize(preview::rotate_euler_degrees({source.normal[0]/marker.scale.x,
   source.normal[1]/marker.scale.y,source.normal[2]/marker.scale.z},marker.rotationDegrees));
  preview::Vec3 albedo{source.color[0],source.color[1],source.color[2]};
  if(marker.selected)albedo=preview::add(preview::scale(albedo,0.85f),preview::scale(color,0.15f));
  preview::Vec3 shaded;
  if(lights.revision==0&&lights.lights.empty()) {
   const float facing=0.70f+(std::max)(0.0f,normal.y)*0.46f;
   shaded={(std::clamp)(albedo.x*facing+0.08f,0.0f,1.0f),(std::clamp)(albedo.y*facing+0.08f,0.0f,1.0f),
    (std::clamp)(albedo.z*facing+0.08f,0.0f,1.0f)};
  }else {
   const auto light=lighting::evaluate_reference_raster_lighting(lights,
    lighting::SurfaceSample{.position={position.x,position.y,position.z},.normal={normal.x,normal.y,normal.z},
     .viewDirection={camera.eye.x-position.x,camera.eye.y-position.y,camera.eye.z-position.z},
     .albedo={albedo.x,albedo.y,albedo.z},.specularColor={0.18f,0.18f,0.18f},.shininess=24.0f});
   shaded={(std::clamp)(light.combined.r,0.0f,1.0f),(std::clamp)(light.combined.g,0.0f,1.0f),(std::clamp)(light.combined.b,0.0f,1.0f)};
  }
  output.push_back({position,shaded});
 }
 const auto vertices=marker.mesh->vertices();const auto indices=marker.mesh->indices();
 for(std::size_t t=0;t<indices.size();t+=3u) {
  preview::Vec3 p[3],normal{};
  for(unsigned i=0;i<3;++i){const auto& v=vertices[indices[t+i]];p[i]={v.position[0],v.position[1],v.position[2]};normal=preview::add(normal,{v.normal[0],v.normal[1],v.normal[2]});}
  if(preview::dot(preview::cross(preview::subtract(p[1],p[0]),preview::subtract(p[2],p[0])),normal)>0)
   std::swap(output[t+1u],output[t+2u]);
 }
 return output;
}
auto detailed_mesh(unsigned cells=210) {
 namespace mesh=epochengine::asset::mesh;
 std::vector<mesh::Vertex> vertices;std::vector<std::uint32_t> indices;
 for(unsigned face=0;face<6;++face){const auto base=std::uint32_t(vertices.size());
  for(unsigned y=0;y<=cells;++y)for(unsigned x=0;x<=cells;++x){
   const float s=2.f*x/cells-1,t=2.f*y/cells-1;std::array<float,3> p;
   switch(face){case 0:p={1,s,t};break;case 1:p={-1,s,t};break;case 2:p={s,1,t};break;
    case 3:p={s,-1,t};break;case 4:p={s,t,1};break;default:p={s,t,-1};break;}
   const float inverse=1/std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);mesh::Vertex vertex;
   for(unsigned axis=0;axis<3;++axis){vertex.normal[axis]=p[axis]*inverse;vertex.position[axis]=10*vertex.normal[axis];}
   vertices.push_back(vertex);
  }
  for(unsigned y=0;y<cells;++y)for(unsigned x=0;x<cells;++x){const auto a=base+y*(cells+1)+x,b=a+1,c=a+cells+1,d=c+1;
   for(auto index:{a,b,d,a,d,c})indices.push_back(index);}
 }
 return mesh::build_artifact(vertices,indices,{1'048'576,6'291'456,64ull*1024*1024});
}
std::promise<void>* runtimePrepareEntered{};std::shared_future<void>* runtimePrepareReleased{};
void hold_runtime_preparation(){runtimePrepareEntered->set_value();runtimePrepareReleased->wait();}
int verify_mixed_planetary_publication(){
 namespace texture=epochengine::asset::texture;
 namespace author=epochengine::authoring::texture;
 namespace transfer=epochengine::texture_artifact;
 namespace lighting=epochengine::lighting;
 namespace mesh=epochengine::asset::mesh;
 author::CanvasDescriptor canvas;canvas.width=512;canvas.height=1024;canvas.tile_extent=128;canvas.mip_count=1;
 canvas.format=author::PixelFormat::rgba8_srgb;canvas.color_space=texture::ColorSpace::srgb;
 author::TextureDocument document{author::DocumentHandle{91,1},author::BranchIdentity{19,1},canvas};
 CHECK(document.create_layer(author::LayerDescriptor{.name="Mixed frame base color"},0,author::TemporalPoint{{19,1},1}));
 texture::TextureCompileProfile profile;auto compiled=document.compile_artifact(profile);CHECK(compiled);
 auto sealed=transfer::seal_compiled_texture(std::move(compiled));CHECK(sealed);
 CHECK(sealed.artifact.artifact()->mips[0].texels.size()==2u*1024u*1024u);
 auto binding=std::make_shared<MeshBaseColorBinding>();binding->artifact=sealed.artifact;
 binding->logical={71,transfer::compiled_artifact_revision(sealed.artifact.artifact()->identity)};
 binding->alpha_cutout=true;binding->sampler.address_u=epochengine::AddressMode::repeat;
 CHECK(valid_mesh_base_color_binding(*binding));
 // Match the native working-set shape, without depending on retained user data:
 // five5607-vertex/12648-index trees and a407092-triangle planetary surface.
 std::vector<mesh::Vertex> treeVertices(5607);std::vector<std::uint32_t> treeIndices(12648);
 for(unsigned i=0;i<treeVertices.size();++i){const auto triangle=i/3u,corner=i%3u;
  treeVertices[i].position={float(triangle%31u)+float(corner==1u),float(triangle/31u)+float(corner==2u),0};
  treeVertices[i].normal={0,0,1};treeVertices[i].uv={float(corner==1u),float(corner==2u)};}
 for(unsigned i=0;i<treeIndices.size();++i)treeIndices[i]=(i/3u%1869u)*3u+i%3u;
 SceneFrameRequest request;request.limits={6'291'456,64ull*1024*1024};request.markers.emplace_back();
 for(unsigned tree=0;tree<5;++tree){auto artifact=mesh::build_artifact(treeVertices,treeIndices);CHECK(artifact);
  ObjectMarker marker;marker.primitive=ObjectPreviewPrimitive::IndexedMesh;marker.mesh=artifact.artifact;
  marker.position.x=float(tree)*3;marker.baseColorTexture=binding;request.markers.push_back(marker);}
 int context{};CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting,request.limits));
 const auto tiny=scene_frame_for(&context);
 for(const auto& marker:tiny->solids->markers)CHECK(marker.geometry);
 const auto sphere=detailed_mesh(184);CHECK(sphere);
 std::vector<std::uint32_t> planetIndices(sphere.artifact->indices().begin(),sphere.artifact->indices().end());
 while(planetIndices.size()<1'221'276u)planetIndices.insert(planetIndices.end(),{0,1,2});
 const auto planet=mesh::build_artifact(sphere.artifact->vertices(),planetIndices,{1'048'576,6'291'456,64ull*1024*1024});CHECK(planet);
 request.markers[0].primitive=ObjectPreviewPrimitive::IndexedMesh;request.markers[0].mesh=planet.artifact;
 CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting,request.limits));
 const auto first=scene_frame_for(&context);CHECK(first->solids->geometry->size()==1'221'276u);
 CHECK(!first->solids->markers[0].geometry&&!first->solids->markers[0].textured);
 const auto same=[](const auto& a,const auto& b){return a.size()==b.size()
  &&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))==0);};
 const auto expected=baseline_marker_solids(request.markers,request.lighting,request.camera);
 CHECK(same(*first->solids->geometry,expected));
 for(std::size_t i=1;i<request.markers.size();++i){std::vector<TexturedVertex> uv;
  const auto fallback=baseline_marker_solids(std::span{&request.markers[i],1},request.lighting,request.camera,&uv);
  CHECK(fallback.empty()&&first->solids->markers[i].geometry->capacity()==0u);
  CHECK(first->solids->markers[i].textured&&same(*first->solids->markers[i].textured,uv));
  CHECK(first->solids->markers[i].marker.baseColorTexture==binding);}
 request.lighting.revision=1;
 request.lighting.lights.push_back({.handle={0,1},.desc={.kind=lighting::LightKind::Directional,.direction={.2f,-1,.1f}}});
 for(unsigned frame=0;frame<2;++frame){request.camera.eye.x+=1;request.camera.target.x+=1;
  CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting,request.limits));
  const auto current=scene_frame_for(&context);
  CHECK(same(*current->solids->geometry,baseline_marker_solids(request.markers,request.lighting,request.camera)));
  CHECK(current->solids!=first->solids&&same(*first->solids->geometry,expected));}
 const auto retained=scene_frame_for(&context);auto source=capture_scene_frame_source(&context);CHECK(source);
 auto unchanged=prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits);CHECK(unchanged);
 CHECK(unchanged->replacement->solid_geometry==retained->solids);
 auto equivalent=std::make_shared<MeshBaseColorBinding>(*binding);request.markers[1].baseColorTexture=equivalent;
 auto sameMaterial=prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits);CHECK(sameMaterial);
 CHECK(sameMaterial->replacement->solid_geometry==retained->solids);
 auto changed=std::make_shared<MeshBaseColorBinding>(*binding);changed->modulate_vertex_color=false;
 request.markers[1].baseColorTexture=changed;
 auto changedMaterial=prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits);CHECK(changedMaterial);
 CHECK(changedMaterial->replacement->solid_geometry->markers[1].textured!=retained->solids->markers[1].textured);
 CHECK(changedMaterial->replacement->solid_geometry->markers[2].textured==retained->solids->markers[2].textured);
 request.markers[1].baseColorTexture=binding;
 auto invalid=std::make_shared<MeshBaseColorBinding>(*binding);++invalid->logical.artifact_revision;
 request.markers[1].baseColorTexture=invalid;
 CHECK(!prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits));
 request.markers[1].baseColorTexture=binding;
 std::stop_source cancelled;cancelled.request_stop();
 CHECK(!prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits,{},cancelled.get_token()));
 auto oversized=*binding->artifact.artifact();oversized.mips[0].texels.reserve(maximum_solid_cache_working_bytes+1u);
 auto oversizedSeal=transfer::seal_compiled_texture(std::move(oversized));CHECK(oversizedSeal);
 auto overBudget=std::make_shared<MeshBaseColorBinding>(*binding);overBudget->artifact=oversizedSeal.artifact;
 request.markers[1].baseColorTexture=overBudget;
 CHECK(!prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits));
 CHECK(scene_frame_for(&context)==retained&&same(*first->solids->geometry,expected));
 request.markers[1].baseColorTexture=binding;
 CHECK(prepare_scene_frame(*source,request.markers,request.camera,request.lighting,request.limits));
 cleanup_context(&context);
 // A wire-only textured mesh must not reserve an unused indices*32 UV buffer.
 // Four million depth vertices fit; six million depth vertices are refused.
 int wireContext{};ObjectMarker wire;wire.primitive=ObjectPreviewPrimitive::IndexedMesh;
 wire.baseColorTexture=binding;wire.wireframe=true;
 std::vector<std::uint32_t> wireIndices(4'000'002u);for(std::size_t i=0;i<wireIndices.size();++i)wireIndices[i]=std::uint32_t(i%3u);
 auto wireMesh=mesh::build_artifact(std::span{treeVertices}.first(3),wireIndices,{1'048'576,6'291'456,64ull*1024*1024});CHECK(wireMesh);
 wire.mesh=wireMesh.artifact;
 CHECK(publish_scene_frame(&wireContext,std::span{&wire,1},Camera{},lighting::LightingFrame{},request.limits));
 const auto wireFrame=scene_frame_for(&wireContext);
 CHECK(wireFrame->wire->depth_triangles->size()==wireIndices.size());
 CHECK(wireFrame->solids->markers[0].textured&&wireFrame->solids->markers[0].textured->capacity()==0u);
 wireIndices.resize(6'291'456u);for(std::size_t i=0;i<wireIndices.size();++i)wireIndices[i]=std::uint32_t(i%3u);
 wire.mesh=mesh::build_artifact(std::span{treeVertices}.first(3),wireIndices,{1'048'576,6'291'456,64ull*1024*1024}).artifact;CHECK(wire.mesh);
 CHECK(!publish_scene_frame(&wireContext,std::span{&wire,1},Camera{},lighting::LightingFrame{},request.limits));
 CHECK(scene_frame_for(&wireContext)==wireFrame);cleanup_context(&wireContext);return 0;
}

int verify_runtime_planetary_publication(){
 using Clock=std::chrono::steady_clock;
 int context{},baselineContext{};RuntimePublicationProbe owner;
 SceneFrameRequest request;request.markers.push_back(ObjectMarker{});
 CHECK(publish_scene_frame(&context,request.markers,request.camera,request.lighting));
 const auto original=scene_frame_for(&context);
 // ~307k real triangles exercise the same transform/lighting/flatten owner
 // as the native planet, rather than a sleep or substitute worker payload.
 const auto detailed=detailed_mesh(160);CHECK(detailed);
 request.markers[0].primitive=ObjectPreviewPrimitive::IndexedMesh;request.markers[0].mesh=detailed.artifact;
 request.limits={6'291'456,64ull*1024*1024};
 const auto baselineStart=Clock::now();
 CHECK(publish_scene_frame(&baselineContext,request.markers,request.camera,request.lighting,request.limits));
 const auto baselineNs=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-baselineStart).count();
 const auto expected=scene_frame_for(&baselineContext)->solids->geometry->size();
 CHECK(expected==921600);cleanup_context(&baselineContext);
 std::promise<void>entered,release;auto released=release.get_future().share();
 auto blocker=owner.m_planetaryScheduler->submit("Hold existing runtime scheduler",[&]{entered.set_value();released.wait();});
 CHECK(entered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 const auto submitStart=Clock::now();
 CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 const auto submitNs=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-submitStart).count();
 auto& work=owner.m_planetaryFramePublications.at(&context);
 const auto ticket=work.cancellation;const auto first=work.request;
 CHECK(owner.planetary_frame_preparing(&context));
 const auto queued=editor_tasks::activity_snapshot(owner.m_planetaryScheduler->snapshot());
 CHECK(queued.running_tasks==1&&queued.queued_tasks==1&&queued.outstanding_tasks==2);
 for(unsigned frame=0;frame<20;++frame){
  request.camera.eye.x+=.1f;request.camera.target.x+=.1f;
  CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
  CHECK(work.request==first&&work.cancellation.node==ticket.node);
  CHECK(scene_frame_for(&context)->solids==original->solids);
  CHECK(std::abs(scene_frame_for(&context)->camera.eye.x-request.camera.eye.x)<1e-5f);
 }
 release.set_value();blocker.get();
 CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
 CHECK(owner.pump_runtime_planetary_frame(&context,request,false));
 CHECK(scene_frame_for(&context)->solids->geometry->size()==expected);
 // The camera moved while preparation ran: queue only the latest request,
 // then settle without resending an unchanged completed frame.
 CHECK(work.pending&&work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
 CHECK(owner.pump_runtime_planetary_frame(&context,request,false)&&!work.pending);
 const auto accepted=scene_frame_for(&context)->solids;
 CHECK(owner.pump_runtime_planetary_frame(&context,request,false)&&!work.pending);
 std::promise<void>resizeEntered,resizeRelease;auto resizeReleased=resizeRelease.get_future().share();
 auto resizeBlocker=owner.m_planetaryScheduler->submit("Hold runtime resize",[&]{resizeEntered.set_value();resizeReleased.wait();});
 CHECK(resizeEntered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 request.depth_fit_aspect=1;CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 const auto narrowRequest=work.request;
 request.depth_fit_aspect=2;CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 resizeRelease.set_value();resizeBlocker.get();
 for(unsigned turn=0;turn<4&&work.pending;++turn){
  CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
  const bool resized=owner.pump_runtime_planetary_frame(&context,request,false);
  if(resized)CHECK(work.request!=narrowRequest&&work.request->depth_fit_aspect==2);
 }
 CHECK(!work.pending&&work.admitted&&work.request->depth_fit_aspect==2);
 // Refusal retains complete geometry and is not repeated for identical input.
 request.limits.maximum_bytes=0;
 CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
 work.next_evidence={};
 CHECK(!owner.pump_runtime_planetary_frame(&context,request,false)&&!work.pending);
 CHECK(!logger::evidence.empty()&&logger::evidence.back().outcome=="preparation refused");
 CHECK(scene_frame_for(&context)->solids==accepted);
 CHECK(!owner.pump_runtime_planetary_frame(&context,request,false)&&!work.pending);
 // A queued explicit display change can be superseded without presenting it.
 request.limits={};request.markers[0].mesh.reset();request.markers[0].primitive=ObjectPreviewPrimitive::Cube;
 std::promise<void>entered2,release2;auto released2=release2.get_future().share();
 auto blocker2=owner.m_planetaryScheduler->submit("Hold superseded runtime request",[&]{entered2.set_value();released2.wait();});
 CHECK(entered2.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 request.markers[0].wireframe=true;CHECK(!owner.pump_runtime_planetary_frame(&context,request,true));
 request.markers[0].wireframe=false;CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 CHECK(scene_frame_for(&context)->solids==accepted);
 release2.set_value();blocker2.get();
 for(unsigned turn=0;turn<4&&owner.planetary_frame_preparing(&context);++turn){
  CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
  (void)owner.pump_runtime_planetary_frame(&context,request,false);
  CHECK(scene_frame_for(&context)->wire->depth_triangles->empty());
  CHECK(!scene_frame_for(&context)->markers.front().wireframe);
 }
 CHECK(!work.pending&&work.admitted&&scene_frame_for(&context)->solids->geometry->size()==36);
 // Changing back to the cancelled policy must retry, not deduplicate its
 // cancelled request forever. The sole worker is still genuinely occupied.
 std::promise<void>entered3,release3;auto released3=release3.get_future().share();
 auto blocker3=owner.m_planetaryScheduler->submit("Hold runtime display policy bounce",[&]{entered3.set_value();released3.wait();});
 CHECK(entered3.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 request.markers[0].wireframe=true;CHECK(!owner.pump_runtime_planetary_frame(&context,request,true));
 request.markers[0].wireframe=false;CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 request.markers[0].wireframe=true;CHECK(!owner.pump_runtime_planetary_frame(&context,request,true));
 release3.set_value();blocker3.get();
 for(unsigned turn=0;turn<4&&owner.planetary_frame_preparing(&context);++turn){
  CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
  (void)owner.pump_runtime_planetary_frame(&context,request,true);
 }
 CHECK(!work.pending&&work.admitted&&scene_frame_for(&context)->markers.front().wireframe);
 // Cancel after the actual preparation body starts. Its cooperative stop path
 // returns an empty optional, not TaskCancelled. Returning to the same display
 // policy must therefore retry despite matching the original request exactly.
 std::promise<void>preparingEntered,preparingRelease;auto preparingReleased=preparingRelease.get_future().share();
 runtimePrepareEntered=&preparingEntered;runtimePrepareReleased=&preparingReleased;
 request.markers[0].primitive=ObjectPreviewPrimitive::IndexedMesh;request.markers[0].mesh=detailed.artifact;
 request.markers[0].wireframe=false;request.limits={6'291'456,64ull*1024*1024};
 skip_allocation_hook=true;allocation_hook=hold_runtime_preparation;
 CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 CHECK(preparingEntered.get_future().wait_for(std::chrono::seconds{5})==std::future_status::ready);
 const auto cooperativeRequest=work.request;const auto beforeCooperative=scene_frame_for(&context)->solids;
 request.markers[0].wireframe=true;CHECK(!owner.pump_runtime_planetary_frame(&context,request,true));
 request.markers[0].wireframe=false;CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 CHECK(work.request==cooperativeRequest&&scene_frame_for(&context)->solids==beforeCooperative);
 preparingRelease.set_value();
 CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
 skip_allocation_hook=false;work.next_evidence={};
 CHECK(!owner.pump_runtime_planetary_frame(&context,request,false));
 CHECK(work.pending&&work.request!=cooperativeRequest);
 CHECK(!logger::evidence.empty()&&logger::evidence.back().outcome=="cancelled");
 CHECK(scene_frame_for(&context)->solids==beforeCooperative);
 CHECK(work.pending->wait_for(std::chrono::seconds{5})==std::future_status::ready);
 CHECK(owner.pump_runtime_planetary_frame(&context,request,false)&&!work.pending);
 CHECK(scene_frame_for(&context)->solids->geometry->size()==expected);
 // Execute the actual runtime retirement block with a cooperative active
 // terrain job and a queued display. Stop owns cancellation and joins before
 // dropping immutable requests; it never publishes the queued replacement.
 std::promise<void>activeEntered;
 auto active=owner.m_planetaryScheduler->submit_cancellable("Active terrain cancellation port",[&](std::stop_token stop){
  activeEntered.set_value();while(!stop.stop_requested())std::this_thread::yield();return 1;
 });
 owner.m_planetaryCancellation=active.cancellation;
 owner.m_planetaryPending.emplace(std::move(active.completion));
 CHECK(activeEntered.get_future().wait_for(std::chrono::seconds{2})==std::future_status::ready);
 request.markers[0].wireframe=true;CHECK(!owner.pump_runtime_planetary_frame(&context,request,true));
 const auto beforeRetirement=scene_frame_for(&context)->solids;
 owner.actual_retire();
 CHECK(!owner.m_planetaryScheduler&&owner.m_planetaryFramePublications.empty());
 CHECK(scene_frame_for(&context)->solids==beforeRetirement);
 CHECK(!logger::evidence.empty()&&logger::evidence.front().committed);
 const auto& phases=logger::evidence.front();
 std::ofstream timings{
#ifdef NDEBUG
 "runtime-publication-Release.json"
#else
 "runtime-publication-Debug.json"
#endif
 };
 timings<<"{\"expanded_vertices\":"<<expected<<",\"synchronous_owner_ns\":"<<baselineNs
  <<",\"queued_owner_ns\":"<<submitNs<<",\"queue_ms\":"<<phases.queue_ms
  <<",\"worker_prepare_ms\":"<<phases.prepare_ms<<",\"owner_commit_ms\":"<<phases.commit_ms
  <<",\"total_ms\":"<<phases.total_ms<<",\"native_gpu\":false}\n";
 CHECK(timings.good());cleanup_context(&context);return 0;
}
int verify_lighting_invalidation() {
 namespace preview=epochengine::previewgrid;namespace lighting=epochengine::lighting;namespace mesh=epochengine::asset::mesh;
 std::array<mesh::Vertex,3> vertices{};vertices[0].position={-1,0,-1};vertices[1].position={1,0,-1};vertices[2].position={0,0,1};
 const std::array<std::uint32_t,3> indices{0,2,1};const auto artifact=mesh::build_artifact(vertices,indices);CHECK(artifact);
 preview::ObjectMarker marker{.primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=artifact.artifact};
 const auto verify=[&](const lighting::LightingFrame* before,const lighting::LightingFrame& current,
  bool clear,bool expectedGlobal,bool expectedChanged,bool expectedColorChange) {
  int context{};if(!preview::set_object_markers(&context,std::span{&marker,1}))return false;
  if(before)preview::set_lighting_frame(&context,*before);
  const auto old=preview::object_solid_geometry_for(&context);
  const auto previousGeometryRevision=preview::preview_content_revision_for(&context);
  const auto cameraRevision=preview::camera_revision_for(&context);
  if(clear)preview::clear_lighting_frame(&context);else preview::set_lighting_frame(&context,current);
  const auto geometryRevision=preview::preview_content_revision_for(&context);
  const auto lightingDamage=lighting::diff_lighting_frames(before, clear ? lighting::LightingFrame{} : current);
  const auto next=preview::object_solid_geometry_for(&context);
  const bool colorChange=old->size()!=next->size()||std::memcmp(old->data(),next->data(),old->size()*sizeof(preview::Vertex))!=0;
  const bool valid=lightingDamage.global_invalidation==expectedGlobal
   &&(geometryRevision!=previousGeometryRevision)==expectedChanged
   &&preview::camera_revision_for(&context)==cameraRevision&&colorChange==expectedColorChange;
  preview::cleanup_context(&context);
  if(!valid){std::ofstream failure{"lighting-invalidation-failure.txt"};failure<<"global "<<lightingDamage.global_invalidation
   <<" expected "<<expectedGlobal<<" revision "<<geometryRevision<<" before "<<previousGeometryRevision
   <<" colorChange "<<colorChange<<" expected "<<expectedColorChange;}
  return valid;
 };
 lighting::LightingFrame fallback,ambient;ambient.revision=1;auto revisionOnly=ambient;revisionOnly.revision=2;
 CHECK(verify(nullptr,ambient,false,true,true,true));
 CHECK(verify(&fallback,ambient,false,true,true,true));
 CHECK(verify(&ambient,revisionOnly,false,false,false,false));
 CHECK(verify(&ambient,fallback,false,true,true,true));
 CHECK(verify(&ambient,fallback,true,true,true,true));
 CHECK(verify(&fallback,fallback,false,false,false,false));
 auto disabled=fallback;disabled.lights.push_back({.handle={0,1},.desc={.kind=lighting::LightKind::Point,.enabled=false}});
 CHECK(verify(&fallback,disabled,false,true,true,true));CHECK(verify(&disabled,fallback,false,true,true,true));
 auto point=ambient;point.lights.push_back({.handle={0,1},.desc={.kind=lighting::LightKind::Point,.position={0,4,0},.range=20}});
 CHECK(verify(&point,ambient,false,false,true,true));
 auto moved=point;moved.lights[0].desc.position.x=1;CHECK(verify(&point,moved,false,false,true,true));
 return 0;
}
int verify_lighting_damage_primitives() {
 namespace lighting=epochengine::lighting;
 lighting::LightingInvalidationFrame pending,incoming;
 pending.revision=3;incoming.revision=4;
 pending.changes.push_back({.had_previous=true,.previous={{1,2,3},4}});
 incoming.changes.push_back({.has_current=true,.current={{5,6,7},8}});
 const auto original=pending.changes.front();
 lighting::coalesce_lighting_invalidations(pending,pending);
 CHECK(!pending.global_invalidation&&pending.changes.size()==1&&pending.changes.front().previous.center.x==original.previous.center.x);
 const auto priorBlocked=blocked;deny_all=true;
 lighting::coalesce_lighting_invalidations(pending,incoming);
 deny_all=false;CHECK(blocked>priorBlocked&&pending.global_invalidation&&pending.changes.empty()&&pending.revision==4);
 blocked=priorBlocked;
 lighting::LightingInvalidationFrame noop;noop.revision=5;
 lighting::coalesce_lighting_invalidations(pending,noop);CHECK(pending.global_invalidation&&pending.changes.empty()&&pending.revision==5);
 lighting::coalesce_lighting_invalidations(pending,pending);CHECK(pending.global_invalidation&&pending.changes.empty());
 return 0;
}
int* racedContext{};ObjectMarker racedMarker;Camera racedCamera;epochengine::lighting::LightingFrame racedLights;
bool raced{};
void publish_competing_scene() {raced=publish_scene_frame(racedContext,std::span{&racedMarker,1},racedCamera,racedLights);}
struct BodyInventoryPort{unsigned body_count{};};
bool managed_frame_admitted(bool rebased,std::optional<BodyInventoryPort> m_explorationPreview,std::span<const ObjectMarker> planetaryMarkers){
 ACTUAL_MANAGED_GATE
 return completeManagedFrame;
}
int verify_async_camera_handoff() {
 namespace mesh=epochengine::asset::mesh;namespace lighting=epochengine::lighting;
 std::array<mesh::Vertex,3> vertices{};
 vertices[0].position={0,0,0};vertices[1].position={1,0,0};vertices[2].position={0,1,0};
 const std::array<std::uint32_t,3> indices{0,1,2};const auto built=mesh::build_artifact(vertices,indices);CHECK(built);
 const std::array<double,3> queuedOrigin{6'371'000.125,-2'556'826.75,8'276'710.5};
 const std::array<double,3> latestOrigin{12'370'104.625,-5'113'652.25,16'553'420.75};
 Camera initial;initial.eye={};initial.target={0,0,-1};initial.nearPlane=4;initial.farPlane=1000;
 Camera latest=initial;latest.eye={.125f,-.25f,.5f};
 const auto forward=normalize(Vec3{.6035167f,-.2414371f,.7599182f});
 const auto right=normalize(cross(forward,{0,1,0})),upright=normalize(cross(right,forward));
 latest.target=add(latest.eye,scale(forward,2));latest.up=add(scale(upright,.8f),scale(right,.6f));
 latest.nearPlane=.18f;latest.farPlane=42'813'184;latest.fovRadians=.9f;
 CloudVolume cloud{.center={0,-6'371'000,0},.radius=6'371'000,.recipe={.seed=91,
  .base_altitude_meters=2000,.thickness_meters=1500,.horizontal_scale_meters=50000}};
 lighting::LightingFrame lights;
 for(bool wire:{false,true})for(bool explicitLatest:{false,true}) {
  int context{};ObjectMarker marker{.primitive=ObjectPreviewPrimitive::IndexedMesh,.mesh=built.artifact,.wireframe=wire};
  CHECK(publish_scene_frame(&context,std::span{&marker,1},initial,lights,{}, {},std::span{&cloud,1}));
  auto source=capture_scene_frame_source(&context);CHECK(source);
  auto queuedCloud=cloud;for(unsigned axis=0;axis<3;++axis)queuedCloud.center[axis]-=queuedOrigin[axis];
  std::optional<PreparedSceneFrame> prepared;
  std::promise<void> entered,release;auto released=release.get_future().share();
  std::thread worker([&]{entered.set_value();released.wait();
   prepared=prepare_scene_frame(*source,std::span{&marker,1},initial,lights,{},queuedOrigin,{},std::span{&queuedCloud,1});});
  entered.get_future().wait();
  auto latestCloud=cloud;latestCloud.time_seconds=20;
  for(unsigned axis=0;axis<3;++axis)latestCloud.center[axis]-=latestOrigin[axis];
  SceneFrameRequest currentView;currentView.camera=latest;currentView.render_origin=latestOrigin;currentView.clouds={latestCloud};
  if(explicitLatest&&wire){currentView.camera.projection=epochengine::render_camera::ProjectionKind::orthographic;
   currentView.camera.orthographicVerticalSize=37;}
  const bool moved=explicitLatest||advance_scene_frame_camera(&context,latest,latestOrigin,std::span<const CloudVolume>{&latestCloud,1});
  release.set_value();worker.join();CHECK(moved&&prepared);
  const auto geometry=prepared->replacement->scene_frame->solids;
  const auto previous=scene_frame_for(&context);
  CHECK(commit_scene_frame(&context,std::move(*prepared),explicitLatest?&currentView:nullptr));
  // A renderer may acquire immediately after this one publication. It must
  // never observe the queued camera before a later camera-only correction.
  std::shared_ptr<const PublishedSceneFrame> observed;std::thread renderer([&]{observed=scene_frame_for(&context);});renderer.join();
  CHECK(observed&&observed->solids==geometry&&observed->render_origin==queuedOrigin);
  CHECK(dot(camera_forward(observed->camera),forward)>.999999f);
  CHECK(dot(normalize(observed->camera.up),normalize(latest.up))>.999999f);
  for(unsigned axis=0;axis<3;++axis) {
   const float eye=axis==0?latest.eye.x:axis==1?latest.eye.y:latest.eye.z;
   const float actual=axis==0?observed->camera.eye.x:axis==1?observed->camera.eye.y:observed->camera.eye.z;
   const double expected=double(eye)+latestOrigin[axis]-queuedOrigin[axis];
   CHECK(std::abs(double(actual)-expected)<=1.0);
   CHECK(std::abs(observed->clouds[0].center[axis]+queuedOrigin[axis]-cloud.center[axis])<1e-6);
  }
  CHECK(observed->camera.nearPlane<=latest.nearPlane&&observed->camera.farPlane>=latest.farPlane);
  CHECK(observed->camera.fovRadians==latest.fovRadians&&observed->revision==previous->revision+1);
  CHECK(observed->camera.projection==currentView.camera.projection);
  CHECK(observed->camera.orthographicVerticalSize==currentView.camera.orthographicVerticalSize);
  CHECK(observed->clouds[0].time_seconds==20&&previous->clouds[0].time_seconds==(explicitLatest?0:20));
  // Refusing malformed live data leaves the entire last admitted packet intact.
  for(unsigned invalid=0;invalid<4;++invalid) {
   source=capture_scene_frame_source(&context);CHECK(source);
   prepared=prepare_scene_frame(*source,std::span{&marker,1},initial,lights,{},queuedOrigin);CHECK(prepared);
   auto bad=currentView;
   if(invalid==0)bad.camera.nearPlane=bad.camera.farPlane+1;
   if(invalid==1)bad.camera.up.x=(std::numeric_limits<float>::quiet_NaN)();
   if(invalid==2)bad.render_origin[0]=(std::numeric_limits<double>::infinity)();
   if(invalid==3)bad.clouds[0].time_seconds=(std::numeric_limits<double>::quiet_NaN)();
   CHECK(!commit_scene_frame(&context,std::move(*prepared),&bad)&&scene_frame_for(&context)==observed);
  }
  // A newer geometry publication still invalidates this worker's source graph.
  source=capture_scene_frame_source(&context);CHECK(source);
  prepared=prepare_scene_frame(*source,std::span{&marker,1},initial,lights,{},queuedOrigin);CHECK(prepared);
  marker.selected=true;CHECK(publish_scene_frame(&context,std::span{&marker,1},initial,lights,{},queuedOrigin));
  const auto competing=scene_frame_for(&context);
  CHECK(!commit_scene_frame(&context,std::move(*prepared),&currentView)&&scene_frame_for(&context)==competing);
  cleanup_context(&context);
 }
 // An unchanged latest pose must retain its prepared depth fit. The caller's
 // raw conservative lens is supplied on every turn, including settled views.
 {int context{};ObjectMarker marker{.primitive=ObjectPreviewPrimitive::IndexedMesh,.mesh=built.artifact};
  CHECK(publish_scene_frame(&context,std::span{&marker,1},initial,lights));
  auto source=capture_scene_frame_source(&context);CHECK(source);
  auto fitted=initial;fitted.nearPlane=20;fitted.farPlane=200;
  auto prepared=prepare_scene_frame(*source,std::span{&marker,1},fitted,lights);CHECK(prepared);
  SceneFrameRequest unchanged;unchanged.camera=initial;
  CHECK(commit_scene_frame(&context,std::move(*prepared),&unchanged));
  const auto observed=scene_frame_for(&context);
  CHECK(observed->camera.nearPlane==20&&observed->camera.farPlane==200);
  cleanup_context(&context);
 }
 return 0;
}
int verify_relative_scene_publication() {
 namespace mesh=epochengine::asset::mesh;namespace lighting=epochengine::lighting;
 std::array<mesh::Vertex,3> source{};
 source[0].position={10,0,0};source[1].position={10,0,1};source[2].position={10,1,0};
 const std::array<std::uint32_t,3> indices{0,1,2};const auto built=mesh::build_artifact(source,indices);CHECK(built);
 ObjectMarker marker{.position={-1737001.8f,0,0},.scale={173700,173700,173700},
  .primitive=ObjectPreviewPrimitive::IndexedMesh,.mesh=built.artifact};
 marker.preciseTransform=make_precise_mesh_transform({-1737001.8,0,0},{173700,173700,173700},{});CHECK(marker.preciseTransform);
 Camera camera;camera.eye={};camera.target={-1,0,0};camera.up={0,1,0};camera.farPlane=1e9f;
 lighting::LightingFrame lights;lights.revision=17;
 int context{},other{};const auto publish=[&]{return publish_scene_frame(&context,std::span{&marker,1},camera,lights);};
 CHECK(publish());const auto original=scene_frame_for(&context);
 CHECK(original&&original->solids&&original->solids->geometry&&original->lines&&original->wire);
 CHECK(original->wire->helper_lines&&original->wire->depth_triangles);
 CHECK(original->solids->geometry->size()==3&&original->camera.eye.x==0&&original->solids->lighting.revision==17);
 CHECK(std::abs(original->solids->geometry->front().position.x+1.8f)<1e-6f);
 const auto oldFloat=add(marker.position,scale(Vec3{10,0,0},marker.scale.x));CHECK(std::abs(oldFloat.x+1.8f)>.01f);
 CHECK(publish_scene_frame(&other,std::span{&marker,1},camera,lights));CHECK(scene_frame_for(&other)!=original);
 // Preparation is CPU-only: a worker cannot replace the visible packet.
 // Camera-only owner updates may continue, but competing geometry rejects
 // the prepared replacement instead of overwriting a newer publication.
 {int asyncContext{};CHECK(publish_scene_frame(&asyncContext,std::span{&marker,1},camera,lights));
  const auto before=scene_frame_for(&asyncContext);auto source=capture_scene_frame_source(&asyncContext);CHECK(source);
  std::optional<PreparedSceneFrame> prepared;
  std::thread worker([&]{prepared=prepare_scene_frame(*source,std::span{&marker,1},camera,lights);});worker.join();
  CHECK(prepared&&scene_frame_for(&asyncContext)==before);
  CHECK(advance_scene_frame_camera(&asyncContext,camera,{}));
  const auto moved=scene_frame_for(&asyncContext);CHECK(moved->revision>before->revision);
  CHECK(commit_scene_frame(&asyncContext,std::move(*prepared)));
  CHECK(scene_frame_for(&asyncContext)->revision>moved->revision);
  source=capture_scene_frame_source(&asyncContext);CHECK(source);
  prepared=prepare_scene_frame(*source,std::span{&marker,1},camera,lights);CHECK(prepared);
  auto rival=marker;rival.selected=!rival.selected;
  CHECK(publish_scene_frame(&asyncContext,std::span{&rival,1},camera,lights));
  const auto competing=scene_frame_for(&asyncContext);
  CHECK(!commit_scene_frame(&asyncContext,std::move(*prepared))&&scene_frame_for(&asyncContext)==competing);
  std::stop_source cancelled;cancelled.request_stop();
  CHECK(!prepare_scene_frame(*source,std::span{&marker,1},camera,lights,{}, {},cancelled.get_token()));
  source=capture_scene_frame_source(&asyncContext);prepared=prepare_scene_frame(*source,std::span{&marker,1},camera,lights);
  CHECK(prepared);clear_object_markers(&asyncContext);
  CHECK(!commit_scene_frame(&asyncContext,std::move(*prepared)));
 }
 CHECK(managed_frame_admitted(true,BodyInventoryPort{1},std::span{&marker,1}));
 CHECK(managed_frame_admitted(false,std::nullopt,{}));
 const auto attemptManaged=[&](std::optional<BodyInventoryPort> inventory,std::span<const ObjectMarker> markers){
  return managed_frame_admitted(true,inventory,markers)&&publish_scene_frame(&context,markers,camera,lights);};
 CHECK(!attemptManaged(std::nullopt,std::span{&marker,1})&&scene_frame_for(&context)==original);
 CHECK(!attemptManaged(BodyInventoryPort{0},{})&&scene_frame_for(&context)==original);
 CHECK(!attemptManaged(BodyInventoryPort{2},std::span{&marker,1})&&scene_frame_for(&context)==original);
 const std::array extraManaged{marker,marker};
 CHECK(!attemptManaged(BodyInventoryPort{1},extraManaged)&&scene_frame_for(&context)==original);
 // Finite double transforms still must fit the float GPU coordinate range.
 // Admission must reject before allocating and retain the entire prior packet.
 const auto mirrored=make_precise_mesh_transform({},{-1,2,-4},{});CHECK((mirrored&&mirrored->apply({2,3,4})==std::array<double,3>{-2,6,-16}));
 CHECK(!make_precise_mesh_transform({},{0,1,1},{}));
 const auto good=marker;for(unsigned invalid=0;invalid<7;++invalid){marker=good;
  if(invalid==0)marker.preciseTransform->position[0]=1e300;
  if(invalid==1)marker.preciseTransform->scale[0]=1e300;
  if(invalid==2)marker.preciseTransform->scale[0]=1e38;
  if(invalid==3)marker.preciseTransform->position[0]=(std::numeric_limits<double>::quiet_NaN)();
  if(invalid==4)marker.mesh.reset();
  if(invalid==5)marker.preciseTransform->scale[0]=-1;
  if(invalid==6)marker.preciseTransform->scale[0]=0;
  const auto before=blocked;deny_all=true;const bool accepted=publish();deny_all=false;
  CHECK(!accepted&&blocked==before&&scene_frame_for(&context)==original);
 }
 marker=good;MeshFrameLimits tooSmall;tooSmall.maximum_indices=2;
 CHECK(!publish_scene_frame(&context,std::span{&marker,1},camera,lights,tooSmall)&&scene_frame_for(&context)==original);
 auto badCamera=camera;badCamera.eye.x=(std::numeric_limits<float>::infinity)();
 CHECK(!publish_scene_frame(&context,std::span{&marker,1},badCamera,lights)&&scene_frame_for(&context)==original);
 auto badLights=lights;badLights.lights.push_back({.desc={.intensity=-1}});
 CHECK(!publish_scene_frame(&context,std::span{&marker,1},camera,badLights)&&scene_frame_for(&context)==original);
 // Only high-precision metadata changes; stale float metadata is identical.
 marker.preciseTransform->position[0]-=.01;CHECK(publish());const auto moved=scene_frame_for(&context);
 CHECK(moved!=original&&moved->revision==original->revision+1&&original->solids->geometry->front().position.x!=moved->solids->geometry->front().position.x);
 CHECK(scene_frame_for(&other)->solids->geometry->front().position.x==original->solids->geometry->front().position.x);
 marker.wireframe=true;CHECK(publish());const auto wire=scene_frame_for(&context);
 CHECK(wire->wire->depth_triangles->size()==3&&wire->lines->empty()&&wire->solids->geometry->empty());
 const float expected=wire->wire->depth_triangles->front().position.x;
 for(const auto& vertex:*wire->lines)CHECK(vertex.position.x==expected);
 marker.preciseTransform->position[0]-=.01;CHECK(publish());const auto wireMoved=scene_frame_for(&context);
 CHECK(wireMoved->wire->depth_triangles!=wire->wire->depth_triangles&&wireMoved->wire->depth_triangles->front().position.x!=expected);
 // A competing publication during allocation supersedes the old snapshot.
 // The late outer preparation must never overwrite it.
 racedContext=&context;racedMarker=good;racedCamera=camera;racedLights=lights;
 std::vector<ObjectMarker> many(64,good);
 allocation_hook=publish_competing_scene;
 CHECK(!publish_scene_frame(&context,many,camera,lights)&&!allocation_hook&&raced);
 CHECK(scene_frame_for(&context)->markers.size()==1&&scene_frame_for(&context)!=wireMoved);
 // Empty relative views are admitted with complete non-null empty geometry.
 CHECK(publish_scene_frame(&context,{},camera,lights));const auto empty=scene_frame_for(&context);
 CHECK(empty&&empty->lines->empty()&&empty->solids->geometry->empty()&&empty->wire->depth_triangles->empty()&&empty->wire->helper_lines->empty());
 CHECK(set_object_markers(&context,std::span{&good,1})&&!scene_frame_for(&context));
 cleanup_context(&context);cleanup_context(&other);return 0;
}
int main() {
 if(const auto handoff=verify_async_camera_handoff();handoff)return handoff;
#if defined(_MSC_VER)
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
 if(const auto resident=verify_forest_focus_exact_mesh();resident)return resident;
 if(const auto focused=verify_forest_paint_focus_parent();focused)return focused;
 if(const auto retired=verify_retired_editor_preview_result();retired)return retired;
 if(const auto modes=verify_editor_wire_request_lifetime();modes)return modes;
 if(const auto returned=verify_editor_planet_after_inline_forest();returned)return returned;
 if(const auto queued=verify_editor_planetary_preview_queue();queued)return queued;
 if(const auto mixed=verify_mixed_planetary_publication();mixed)return mixed;
 if(const auto runtime=verify_runtime_planetary_publication();runtime)return runtime;
 if(const auto relative=verify_relative_scene_publication();relative)return relative;
 if(const auto lighting=verify_lighting_invalidation();lighting)return lighting;
 if(const auto pending=verify_lighting_damage_primitives();pending)return pending;
 namespace mesh=epochengine::asset::mesh;
 namespace preview=epochengine::previewgrid;
 // The actual retained planetary snapshot reproduces the old solid-to-wire
 // rejection without a renderer. The ordinary fixture has the same size class.
 {
  const auto path=std::getenv("EPOCH_WIREFRAME_REPRO_MESH");
  const auto largeMesh=path?mesh::read_artifact_file(path,{1'048'576,6'291'456,64ull*1024*1024}):detailed_mesh(210);
  CHECK(largeMesh);int framedContext{};
  preview::ObjectMarker terrain{.primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=largeMesh.artifact};
  preview::Camera view;epochengine::lighting::LightingFrame lighting;
  const preview::MeshFrameLimits limits{6'291'456,64ull*1024*1024};
  const std::array<double,3> origin{6'371'000.125,20.25,-30.5};
  CHECK(preview::publish_scene_frame(&framedContext,std::span{&terrain,1},view,lighting,limits,origin));
  const auto solid=preview::scene_frame_for(&framedContext);terrain.wireframe=true;
  CHECK(!preview::set_object_markers(&framedContext,std::span{&terrain,1},limits));
  CHECK(preview::publish_scene_frame(&framedContext,std::span{&terrain,1},view,lighting,limits,origin));
  auto wire=preview::scene_frame_for(&framedContext);
  CHECK(wire->lines->empty()&&wire->wire->helper_lines==wire->lines);
  CHECK(wire->wire->depth_triangles->size()==largeMesh.artifact->indices().size());
  CHECK(preview::detail::g_objectMarkers.at(&framedContext).wire_edges.empty());
  for(unsigned step=0;step<3;++step){terrain.position.x+=.25f;view.eye.x+=.5f;view.target.x+=.5f;
   CHECK(preview::publish_scene_frame(&framedContext,std::span{&terrain,1},view,lighting,limits,origin));}
  wire=preview::scene_frame_for(&framedContext);
  auto movedOrigin=origin;movedOrigin[0]+=.125;movedOrigin[2]-=.5;
  const auto before=wire->camera;auto limited=limits;limited.maximum_indices=2;
  CHECK(!preview::publish_scene_frame(&framedContext,std::span{&terrain,1},view,lighting,limited,movedOrigin));
  CHECK(preview::scene_frame_for(&framedContext)==wire);
  CHECK(preview::advance_scene_frame_camera(&framedContext,view,movedOrigin));
  const auto advanced=preview::scene_frame_for(&framedContext);
  CHECK(advanced->wire==wire->wire&&advanced->solids==wire->solids&&advanced->lines==wire->lines);
  CHECK(advanced->render_origin==origin&&advanced->revision==wire->revision+1);
  CHECK(std::abs(advanced->camera.eye.x-before.eye.x-.125f)<1e-6f);
  CHECK(std::abs(advanced->camera.eye.z-before.eye.z+.5f)<1e-6f);
  CHECK(std::abs(advanced->camera.target.x-before.target.x-.125f)<1e-6f);
  // Retaining a rejected planetary packet can leave the camera millions of
  // meters from its old origin. A unit look target must not round independently
  // into a different heading as the pilot continues moving forward.
  auto inherited=view;inherited.eye={10'000'000,0,0};inherited.target={9'999'900,0,0};
  inherited.navigation={{10'000'000,0,0},{-1,0,0}};inherited.navigationValid=true;
  CHECK(preview::set_external_camera_frame(&framedContext,inherited));
  const auto direction=preview::normalize({.6035167f,-.2414371f,.7599182f});
  const auto up=std::optional<preview::Vec3>{{0,1,0}};
  const auto context=&framedContext;
  namespace previewgrid=epochengine::previewgrid;
  ACTUAL_EXPLORATION_CAMERA
  auto flight=camera;
  CHECK(!flight.navigationValid&&preview::dot(preview::camera_forward(flight),direction)>.999999f);
  flight.nearPlane=.18f;flight.farPlane=42'813'184.0f;
  const auto intended=preview::camera_forward(flight);
  for(unsigned step=0;step<32;++step) {
   auto distant=origin;
   distant[0]+=5'999'104.5+double(step)*.125;
   distant[1]-=2'556'826.75-double(step)*.03125;
   distant[2]+=8'276'710.0+double(step)*.125;
   CHECK(preview::advance_scene_frame_camera(&framedContext,flight,distant));
   const auto retained=preview::scene_frame_for(&framedContext);
   const auto actual=preview::normalize(preview::subtract(retained->camera.target,retained->camera.eye));
   CHECK(preview::dot(actual,intended)>0.999999f);
   CHECK(retained->wire==wire->wire&&retained->render_origin==origin);
   CHECK(retained->camera.nearPlane==flight.nearPlane&&retained->camera.farPlane==flight.farPlane);
   CHECK(retained->camera.fovRadians==flight.fovRadians&&retained->camera.projection==flight.projection);
   const auto actualView=preview::look_at(retained->camera.eye,retained->camera.target,retained->camera.up);
   CHECK(std::abs(actualView[2]+intended.x)<.000002f
    &&std::abs(actualView[6]+intended.y)<.000002f&&std::abs(actualView[10]+intended.z)<.000002f);
  }
  const auto lastRetained=preview::scene_frame_for(&framedContext);
  movedOrigin[0]=(std::numeric_limits<double>::quiet_NaN)();
  CHECK(!preview::advance_scene_frame_camera(&framedContext,view,movedOrigin));
  CHECK(preview::scene_frame_for(&framedContext)==lastRetained);
  const std::array<double,3> preciseFocus{6'371'000.125,20.0625,-30.03125};
  CHECK(preview::focus_camera_precise(&framedContext,preciseFocus,.1f));
  const auto focused=preview::capture_camera_rig_snapshot(&framedContext);
  CHECK(preview::detail::g_cameraRigs.at(&framedContext).navigationOrigin==preciseFocus
   &&focused.focus.x==0&&focused.focus.y==0&&focused.focus.z==0);
  preview::cleanup_context(&framedContext);
 }
 SHARED_ROUTING_OWNER
 std::array<mesh::Vertex,4> vertices{};
 vertices[0].position={-1,0,-1};vertices[1].position={1,0,-1};
 vertices[2].position={1,0,1};vertices[3].position={-1,0,1};
 const std::array<std::uint32_t,6> indices{0,2,1,0,3,2};
 const auto artifact=mesh::build_artifact(vertices,indices);CHECK(artifact);
 preview::ObjectMarker marker{.position={10,20,30},.color={0.2f,0.7f,0.9f},
  .scale={2,3,4},.rotationDegrees={0,90,0},
  .primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=artifact.artifact};
 int a{},b{};const auto publish=[&](const void* context) {
  return preview::set_object_markers(context,std::span{&marker,1});
 };
 CHECK(publish(&a));CHECK(publish(&b));
 CHECK(preview::object_marker_vertices_for(&a).empty());
 CHECK(preview::object_solid_vertices_for(&a).size()==6);
 int equivalence{};
 for(unsigned variant=0;variant<8;++variant) {
  auto sample=marker;sample.selected=(variant&1)!=0;sample.editorOnly=(variant&2)!=0;
  if(variant&4){sample.rotationDegrees={31,-47,16};sample.scale={0.7f,2.1f,1.3f};}
  epochengine::lighting::LightingFrame lights;
  if(variant&2){lights.revision=1;
   lights.lights.push_back({.handle={0,1},.desc={.kind=epochengine::lighting::LightKind::Directional,.direction={0.2f,-1,0.1f}}});
   lights.lights.push_back({.handle={1,1},.desc={.kind=epochengine::lighting::LightKind::Point,.position={10,23,30},.range=20}});}
  preview::set_lighting_frame(&equivalence,lights);
  CHECK(preview::set_object_markers(&equivalence,std::span{&sample,1}));
  const auto reference=per_index_reference(sample,lights,preview::camera_for(&equivalence));
  const auto optimized=preview::object_solid_vertices_for(&equivalence);
  CHECK(optimized.size()==reference.size());
  CHECK(std::memcmp(optimized.data(),reference.data(),reference.size()*sizeof(preview::Vertex))==0);
 }
 preview::cleanup_context(&equivalence);
 const auto before=preview::preview_content_revision_for(&a);
 marker.wireframe=true;CHECK(publish(&a));
 CHECK(preview::preview_content_revision_for(&a)==before+1);
 CHECK(preview::object_solid_vertices_for(&a).empty());
 const auto cache=preview::object_marker_geometry_for(&a);
 deny_all=true;
 for(unsigned repeat=0;repeat<100;++repeat) CHECK(preview::object_marker_geometry_for(&a)==cache);
 deny_all=false;CHECK(blocked==0);
 const auto edges=preview::object_marker_vertices_for(&a);
 CHECK(edges.size()==10); // Five distinct triangle edges, not six or a box.
 const std::array<preview::Vec3,4> expected{{{6,20,32},{6,20,28},{14,20,28},{14,20,32}}};
 const auto vertex_id=[&](const preview::Vec3& value) {
  for(int i=0;i<4;++i) if(std::abs(value.x-expected[i].x)<1e-5f
   &&std::abs(value.y-expected[i].y)<1e-5f&&std::abs(value.z-expected[i].z)<1e-5f) return i;
  return -1;
 };
 std::array<std::array<bool,4>,4> seen{};
 for(std::size_t i=0;i<edges.size();i+=2) {
  int first=vertex_id(edges[i].position),second=vertex_id(edges[i+1].position);
  CHECK(first>=0&&second>=0&&first!=second);
  if(first>second){const auto temporary=first;first=second;second=temporary;}
  CHECK(!seen[first][second]);seen[first][second]=true;
  CHECK(edges[i].color.y==marker.color.y&&edges[i+1].color.z==marker.color.z);
 }
 CHECK(seen[0][1]&&seen[0][2]&&seen[0][3]&&seen[1][2]&&seen[2][3]);
 CHECK(!seen[1][3]);
 CHECK(publish(&a)&&preview::preview_content_revision_for(&a)==before+1);
 CHECK(preview::object_marker_geometry_for(&a)==cache);
 CHECK(preview::object_solid_vertices_for(&b).size()==6);
 CHECK(preview::object_marker_vertices_for(&b).empty());
 marker.wireframe=false;CHECK(publish(&a));
 CHECK(preview::object_solid_vertices_for(&a).size()==6);
 CHECK(preview::object_marker_vertices_for(&a).empty());
 // Forest painting changes only submitted albedo, and invalidates it by value.
 int paintContext{};auto painted=marker;painted.selected=false;
 auto paint=std::make_shared<preview::MeshPaintView>();paint->color={0.2f,0.3f,0.4f};painted.paintView=paint;
 CHECK(preview::set_object_markers(&paintContext,std::span{&painted,1}));
 const auto paintFrame=preview::object_solid_frame_for(&paintContext);
 CHECK(!paintFrame->geometry->empty());
 const auto paintRevision=preview::preview_content_revision_for(&paintContext);
 painted.paintView=std::make_shared<preview::MeshPaintView>(*paint);
 CHECK(preview::set_object_markers(&paintContext,std::span{&painted,1}));
 CHECK(preview::preview_content_revision_for(&paintContext)==paintRevision);
 auto changedPaint=std::make_shared<preview::MeshPaintView>(*paint);changedPaint->color={0.7f,0.6f,0.1f};painted.paintView=changedPaint;
 CHECK(preview::set_object_markers(&paintContext,std::span{&painted,1}));
 CHECK(preview::preview_content_revision_for(&paintContext)>paintRevision);
 CHECK(preview::object_solid_frame_for(&paintContext)->geometry!=paintFrame->geometry);
 // Selection keeps the visible shaded surface; complete triangle edges are
 // requested only by explicit Wireframe, never by selecting a solid planet.
 int selectionContext{};auto selectionMarker=marker;selectionMarker.selected=true;
 CHECK(preview::set_object_markers(&selectionContext,std::span{&selectionMarker,1}));
 const auto selectionLines=preview::object_marker_geometry_for(&selectionContext);
 CHECK(selectionLines->empty()&&preview::object_solid_vertices_for(&selectionContext).size()==6);
 CHECK(preview::detail::g_objectMarkers.at(&selectionContext).wire_edges.empty());
 selectionMarker.wireframe=true;CHECK(preview::set_object_markers(&selectionContext,std::span{&selectionMarker,1}));
 const auto explicitSelectionWire=preview::object_marker_geometry_for(&selectionContext);
 CHECK(explicitSelectionWire->size()==10&&preview::object_solid_vertices_for(&selectionContext).empty());
 const auto selectionColor=preview::visual_rgb(epochengine::visuals::object_selected_outline());
 for(const auto& vertex:*explicitSelectionWire){CHECK(vertex_id(vertex.position)>=0);CHECK(vertex.color.x==selectionColor.x&&vertex.color.y==selectionColor.y&&vertex.color.z==selectionColor.z);}
 selectionMarker.wireframe=false;selectionMarker.rotationDegrees={31,-47,16};selectionMarker.scale={0.7f,2.1f,1.3f};
 CHECK(preview::set_object_markers(&selectionContext,std::span{&selectionMarker,1}));
 const auto selectionSolids=preview::object_solid_vertices_for(&selectionContext);
 CHECK(selectionSolids.size()==6&&preview::object_marker_vertices_for(&selectionContext).empty());
 selectionMarker.wireframe=true;CHECK(preview::set_object_markers(&selectionContext,std::span{&selectionMarker,1}));
 for(const auto& edge:preview::object_marker_vertices_for(&selectionContext)) {
  CHECK(std::any_of(selectionSolids.begin(),selectionSolids.end(),[&](const auto& solid){return std::abs(solid.position.x-edge.position.x)<1e-5f
   &&std::abs(solid.position.y-edge.position.y)<1e-5f&&std::abs(solid.position.z-edge.position.z)<1e-5f;}));
 }
 const std::array sharedWire{selectionMarker,selectionMarker};
 CHECK(preview::set_object_markers(&selectionContext,sharedWire));
 CHECK(preview::detail::g_objectMarkers.at(&selectionContext).wire_edges.size()==1&&preview::object_marker_vertices_for(&selectionContext).size()==20);
 selectionMarker.wireframe=false;const std::array sharedSelection{selectionMarker,selectionMarker};
 CHECK(preview::set_object_markers(&selectionContext,sharedSelection));
 CHECK(preview::detail::g_objectMarkers.at(&selectionContext).wire_edges.empty()&&preview::object_marker_vertices_for(&selectionContext).empty());
 CHECK(preview::object_solid_vertices_for(&selectionContext).size()==12);
 preview::cleanup_context(&selectionContext);CHECK(selectionLines->empty()&&explicitSelectionWire->size()==10);
 // Helper selection boxes enclose exactly the visible helper geometry,
 // including the camera's asymmetric frustum, irrespective of entity scale.
 const auto bounds=[](std::span<const preview::Vertex> data){std::array<float,6> box{1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
  for(const auto& vertex:data){box[0]=(std::min)(box[0],vertex.position.x);box[1]=(std::min)(box[1],vertex.position.y);box[2]=(std::min)(box[2],vertex.position.z);
   box[3]=(std::max)(box[3],vertex.position.x);box[4]=(std::max)(box[4],vertex.position.y);box[5]=(std::max)(box[5],vertex.position.z);}return box;};
 for(const auto primitive:{preview::ObjectPreviewPrimitive::Light,preview::ObjectPreviewPrimitive::Camera}) {
  CHECK(preview::object_preview_geometry_route(primitive).selection_wire);
  preview::ObjectMarker helper{.position={2,3,4},.scale={100,200,300},.rotationDegrees={32,45,-17},.radius=1.2f,.primitive=primitive};
  CHECK(preview::set_object_markers(&selectionContext,std::span{&helper,1}));
  const auto visible=preview::object_marker_geometry_for(&selectionContext);const auto expectedBounds=bounds(*visible);
  helper.selected=true;CHECK(preview::set_object_markers(&selectionContext,std::span{&helper,1}));
  const auto selected=preview::object_marker_geometry_for(&selectionContext);CHECK(selected->size()==visible->size()+24);
  const auto selectedBounds=bounds(std::span{*selected}.subspan(visible->size()));
  for(std::size_t axis=0;axis<6;++axis)CHECK(std::abs(expectedBounds[axis]-selectedBounds[axis])<1e-6f);
  CHECK(preview::object_solid_vertices_for(&selectionContext).empty());preview::cleanup_context(&selectionContext);
 }
 preview::ObjectMarker cube{.scale={.1f,.2f,.3f},.rotationDegrees={31,-47,16},.selected=true};
 CHECK(preview::set_object_markers(&selectionContext,std::span{&cube,1}));
 const auto cubeLines=preview::object_marker_vertices_for(&selectionContext),cubeSolids=preview::object_solid_vertices_for(&selectionContext);
 CHECK(cubeLines.size()==24&&cubeSolids.size()==36);
 for(const auto& edge:cubeLines)CHECK(std::any_of(cubeSolids.begin(),cubeSolids.end(),[&](const auto& solid){return std::abs(solid.position.x-edge.position.x)<1e-6f
  &&std::abs(solid.position.y-edge.position.y)<1e-6f&&std::abs(solid.position.z-edge.position.z)<1e-6f;}));
 preview::cleanup_context(&selectionContext);
 preview::cleanup_context(&a);preview::cleanup_context(&b);
 CHECK(preview::object_marker_vertices_for(&a).empty());
 CHECK(preview::object_solid_vertices_for(&a).empty());
 CHECK(cache->size()==10); // Retained backend ownership survives context retirement.
 const auto detailed=detailed_mesh();CHECK(detailed);
 CHECK(detailed.artifact->vertices().size()==267'126&&detailed.artifact->indices().size()==1'587'600);
 marker.mesh=detailed.artifact;marker.wireframe=true;marker.rotationDegrees={};marker.scale={1,1,1};
 const preview::MeshFrameLimits large{6'291'456,64ull*1024*1024};
 // The210-grid still exceeds the128MiB ceiling even with one shared line
 // stream. Wire depth and compatible legacy lines remain bounded.
 CHECK(!preview::wireframe_frame_admitted(std::span{&marker,1}));
 // Actual six-face160-grid (~307k triangles): switching the retained solid
 // frame to wire must fit without a duplicate semantic line copy or counting
 // the same source artifact twice. Keep the old frame alive through publication.
 int largeWireContext{};const auto admittedWire=detailed_mesh(160);CHECK(admittedWire);
 auto largeWireMarker=marker;largeWireMarker.mesh=admittedWire.artifact;largeWireMarker.wireframe=false;
 CHECK(preview::set_object_markers(&largeWireContext,std::span{&largeWireMarker,1},large));
 const auto priorSolid=preview::object_solid_frame_for(&largeWireContext);CHECK(priorSolid->geometry->size()==921600);
 largeWireMarker.wireframe=true;CHECK(preview::set_object_markers(&largeWireContext,std::span{&largeWireMarker,1},large));
 const auto largeWire=preview::object_mesh_wire_frame_for(&largeWireContext);CHECK(largeWire->depth_triangles->size()==921600);
 CHECK(preview::object_marker_geometry_for(&largeWireContext)->size()==925440);
 largeWireMarker.position.x+=20;CHECK(preview::set_object_markers(&largeWireContext,std::span{&largeWireMarker,1},large));
 CHECK(preview::object_mesh_wire_frame_for(&largeWireContext)->depth_triangles!=largeWire->depth_triangles);
 CHECK(priorSolid->geometry->size()==921600);preview::cleanup_context(&largeWireContext);
 // The runtime republishes a matched frame after camera/lighting changes even
 // when the same immutable wire triangles are retained. A real transform also
 // fits once the framed path stops building an unused all-edge line stream.
 int repeatedWireContext{};const auto repeatedWireArtifact=detailed_mesh(180);CHECK(repeatedWireArtifact);
 auto repeatedWireMarker=largeWireMarker;repeatedWireMarker.mesh=repeatedWireArtifact.artifact;
 preview::Camera repeatedCamera;epochengine::lighting::LightingFrame repeatedLights;
 CHECK(preview::publish_scene_frame(&repeatedWireContext,std::span{&repeatedWireMarker,1},repeatedCamera,repeatedLights,large));
 const auto firstRepeatedWire=preview::scene_frame_for(&repeatedWireContext);
 for(unsigned frame=0;frame<3;++frame){repeatedCamera.eye.x+=1;repeatedCamera.target.x+=1;++repeatedLights.revision;
  CHECK(preview::publish_scene_frame(&repeatedWireContext,std::span{&repeatedWireMarker,1},repeatedCamera,repeatedLights,large));
  CHECK(preview::scene_frame_for(&repeatedWireContext)->wire->depth_triangles==firstRepeatedWire->wire->depth_triangles);}
 const auto retainedRepeatedWire=preview::scene_frame_for(&repeatedWireContext);repeatedWireMarker.position.x+=1;
 CHECK(preview::publish_scene_frame(&repeatedWireContext,std::span{&repeatedWireMarker,1},repeatedCamera,repeatedLights,large));
 CHECK(blocked==0&&preview::scene_frame_for(&repeatedWireContext)->wire->depth_triangles!=retainedRepeatedWire->wire->depth_triangles);
 preview::cleanup_context(&repeatedWireContext);
 const auto wireDetailed=detailed_mesh(100);CHECK(wireDetailed);
 CHECK(wireDetailed.artifact->vertices().size()==61'206&&wireDetailed.artifact->indices().size()==360'000);
 marker.mesh=wireDetailed.artifact;
 CHECK(preview::wireframe_frame_admitted(std::span{&marker,1}));
 CHECK(preview::set_object_markers(&a,std::span{&marker,1},large));
 const auto detailed_cache=preview::object_marker_geometry_for(&a);
 CHECK(detailed_cache->size()==362'400); // Six actual100x100 sphere grids, shared edges once.
 CHECK(preview::set_object_markers(&a,std::span{&marker,1},large));
 CHECK(preview::object_marker_geometry_for(&a)==detailed_cache);
 const auto cached_edges=preview::detail::g_objectMarkers.at(&a).wire_edges.front().edges;
 marker.position.x+=20;marker.rotationDegrees.y=27;
 CHECK(preview::set_object_markers(&a,std::span{&marker,1},large));
 CHECK(preview::detail::g_objectMarkers.at(&a).wire_edges.front().edges==cached_edges);
 const auto transformed_cache=preview::object_marker_geometry_for(&a);
 CHECK(transformed_cache!=detailed_cache&&transformed_cache->size()==detailed_cache->size());
 CHECK(detailed_cache->size()==362'400); // Prior backend frame stays valid during republish.
 // A valid6M-index artifact is admitted for solid rendering, but not wire's
 // temporary keys plus emitted lines. Rejection preserves the exact prior frame.
 std::vector<std::uint32_t> huge_indices(6'291'456);
 for(std::size_t i=0;i<huge_indices.size();++i)huge_indices[i]=indices[i%indices.size()];
 const auto huge=mesh::build_artifact(vertices,huge_indices,{1'048'576,6'291'456,64ull*1024*1024});CHECK(huge);
 marker.mesh=huge.artifact;const auto old_revision=preview::preview_content_revision_for(&a);
 CHECK(preview::mesh_frame_admitted(std::span{&marker,1},large));
 CHECK(!preview::wireframe_frame_admitted(std::span{&marker,1}));
 deny_large=true;CHECK(!preview::set_object_markers(&a,std::span{&marker,1},large));deny_large=false;
 CHECK(blocked==0);
 CHECK(preview::preview_content_revision_for(&a)==old_revision);
 CHECK(preview::object_marker_geometry_for(&a)==transformed_cache);
 marker.wireframe=false;marker.selected=true;
 CHECK(preview::mesh_frame_admitted(std::span{&marker,1},large));
 CHECK(preview::wireframe_frame_admitted(std::span{&marker,1}));
 deny_large=true;CHECK(preview::set_object_markers(&a,std::span{&marker,1},large));deny_large=false;
 CHECK(blocked==0&&preview::preview_content_revision_for(&a)!=old_revision);
 CHECK(preview::object_marker_geometry_for(&a)->empty());marker.selected=false;
 preview::cleanup_context(&a);
 // A large terrain batch must survive lit-camera movement without retaining
 // a second per-marker copy of the same joined triangle stream. Small batches
 // still reuse unchanged object geometry when another object moves.
 int batchContext{},smallBatchContext{};
 std::array<preview::ObjectMarker,6> batch{};
 batch[0].primitive=preview::ObjectPreviewPrimitive::IndexedMesh;batch[0].mesh=detailed.artifact;
 for(unsigned i=1;i<batch.size();++i){batch[i].primitive=preview::ObjectPreviewPrimitive::IndexedMesh;
  batch[i].mesh=artifact.artifact;batch[i].position.x=float(i)*3;}
 preview::Camera batchView;epochengine::lighting::LightingFrame batchLights;batchLights.revision=1;
 batchLights.lights.push_back({.desc={.kind=epochengine::lighting::LightKind::Directional,.intensity=1.35f}});
 CHECK(preview::publish_scene_frame(&batchContext,batch,batchView,batchLights,large));
 const auto firstBatch=preview::scene_frame_for(&batchContext);
 CHECK(firstBatch->solids->markers.size()==batch.size()&&!firstBatch->solids->markers[0].geometry);
 const auto batchReference=baseline_marker_solids(batch,batchLights,batchView);
 CHECK(firstBatch->solids->geometry->size()==batchReference.size()
  &&std::memcmp(firstBatch->solids->geometry->data(),batchReference.data(),batchReference.size()*sizeof(preview::Vertex))==0);
 CHECK(preview::publish_scene_frame(&batchContext,batch,batchView,batchLights,large));
 CHECK(preview::scene_frame_for(&batchContext)->solids==firstBatch->solids);
 batchView.eye.x+=.125f;batchView.target.x+=.125f;
 CHECK(preview::publish_scene_frame(&batchContext,batch,batchView,batchLights,large));
 CHECK(preview::scene_frame_for(&batchContext)->solids->geometry!=firstBatch->solids->geometry);
 const auto replacementBatch=detailed_mesh(200);CHECK(replacementBatch);
 batch[0].mesh=replacementBatch.artifact;
 CHECK(preview::publish_scene_frame(&batchContext,batch,batchView,batchLights,large));
 batchLights.environment.ambient={.6f,.6f,.6f};
 CHECK(preview::publish_scene_frame(&batchContext,batch,batchView,batchLights,large));
 CHECK(preview::scene_frame_for(&batchContext)->solids->lighting.environment.ambient.r==.6f);
 CHECK(firstBatch->solids->geometry->size()==batchReference.size());preview::cleanup_context(&batchContext);
 std::array smallBatch{batch[1],batch[2]};
 CHECK(preview::publish_scene_frame(&smallBatchContext,smallBatch,batchView,batchLights,large));
 const auto smallBefore=preview::scene_frame_for(&smallBatchContext);
 CHECK(smallBefore->solids->markers[0].geometry&&smallBefore->solids->markers[1].geometry);
 smallBatch[1].position.x+=1;
 CHECK(preview::publish_scene_frame(&smallBatchContext,smallBatch,batchView,batchLights,large));
 const auto smallAfter=preview::scene_frame_for(&smallBatchContext);
 CHECK(smallAfter->solids->markers[0].geometry==smallBefore->solids->markers[0].geometry
  &&smallAfter->solids->markers[1].geometry!=smallBefore->solids->markers[1].geometry);
 preview::cleanup_context(&smallBatchContext);
 int solidContext{},otherContext{};
 std::ofstream progress{"solid-progress.txt"};const auto stage=[&](const char* text){progress<<text<<'\n';progress.flush();};stage("benchmark");
 marker.mesh=detailed.artifact;marker.wireframe=false;marker.rotationDegrees={31,-47,16};marker.scale={0.7f,2.1f,1.3f};
 epochengine::lighting::LightingFrame ambient;ambient.revision=1;
 preview::set_lighting_frame(&solidContext,ambient);
 CHECK(preview::set_object_markers(&solidContext,std::span{&marker,1},large));
 const auto oldStart=std::chrono::steady_clock::now();
 const auto oldGeometry=baseline_marker_solids(std::span{&marker,1},ambient,preview::camera_for(&solidContext));
 const auto oldNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-oldStart).count();
 const auto coldStart=std::chrono::steady_clock::now();
 const auto solids=preview::object_solid_geometry_for(&solidContext);
 stage("cold complete");
 const auto coldNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-coldStart).count();
 CHECK(solids->size()==oldGeometry.size()&&std::memcmp(solids->data(),oldGeometry.data(),solids->size()*sizeof(preview::Vertex))==0);
 stage("equivalence complete");
 stage(preview::detail::g_objectMarkers.at(&solidContext).solid_geometry?"cache stored":"cache absent");
 const auto hitStart=std::chrono::steady_clock::now();
 bool hitFailed{};
 try{deny_all=true;for(unsigned repeat=0;repeat<1000;++repeat)
  if(preview::object_solid_geometry_for(&solidContext)!=solids){hitFailed=true;break;}}
 catch(const std::bad_alloc&){hitFailed=true;}
 deny_all=false;
 if(hitFailed||blocked){stage("hit failed");preview::cleanup_context(&solidContext);return 190;}
 stage("hits complete");
 const auto hitNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-hitStart).count();
 auto camera=preview::camera_for(&solidContext);camera.eye.x+=5;camera.target.x+=5;
 CHECK(preview::set_external_camera_frame(&solidContext,camera));
 CHECK(preview::object_solid_geometry_for(&solidContext)==solids); // Ambient does not use eye.
 auto moon=marker;moon.mesh=artifact.artifact;moon.position.x+=15;
 stage("orbit");
 std::array pair{marker,moon};
 CHECK(preview::set_object_markers(&solidContext,pair,large));
 const auto joined=preview::object_solid_geometry_for(&solidContext);
 const auto terrainCached=preview::detail::g_objectMarkers.at(&solidContext).solid_geometry->markers[0].geometry;
 CHECK(terrainCached==solids);
 pair[1].position.y+=2;CHECK(preview::set_object_markers(&solidContext,pair,large));
 const auto orbitStart=std::chrono::steady_clock::now();
 const auto orbit=preview::object_solid_geometry_for(&solidContext);
 const auto orbitNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-orbitStart).count();
 CHECK(orbit!=joined&&preview::detail::g_objectMarkers.at(&solidContext).solid_geometry->markers[0].geometry==terrainCached);
 // A different context, changed light descriptor without a revision change,
 // and the fallback-to-ambient revision transition cannot share stale color.
 marker.mesh=artifact.artifact;
 stage("lighting keys");
 CHECK(preview::set_object_markers(&otherContext,std::span{&marker,1}));
 const auto fallback=preview::object_solid_geometry_for(&otherContext);
 preview::set_lighting_frame(&otherContext,ambient);
 const auto ambientGeometry=preview::object_solid_geometry_for(&otherContext);CHECK(ambientGeometry!=fallback);
 auto directional=ambient;directional.lights.push_back({.handle={0,1},.desc={.kind=epochengine::lighting::LightKind::Directional,.direction={.2f,-1,.1f}}});
 preview::set_lighting_frame(&otherContext,directional);
 const auto litGeometry=preview::object_solid_geometry_for(&otherContext);CHECK(litGeometry!=ambientGeometry);
 auto view=preview::camera_for(&otherContext);view.target.x+=1;
 CHECK(preview::set_external_camera_frame(&otherContext,view));
 CHECK(preview::object_solid_geometry_for(&otherContext)==litGeometry); // Look rotation alone does not change shading.
 view.eye.x+=2;view.target.x+=2;CHECK(preview::set_external_camera_frame(&otherContext,view));
 const auto movedEye=preview::object_solid_geometry_for(&otherContext);CHECK(movedEye!=litGeometry);
 directional.lights[0].desc.intensity+=1;preview::set_lighting_frame(&otherContext,directional);
 const auto relit=preview::object_solid_geometry_for(&otherContext);CHECK(relit!=movedEye);
 const auto expectedRelit=per_index_reference(marker,directional,preview::camera_for(&otherContext));
 CHECK(expectedRelit.size()==relit->size()&&std::memcmp(expectedRelit.data(),relit->data(),relit->size()*sizeof(preview::Vertex))==0);
 // Retire and reuse the same context address while actual geometry is built.
 static int* retiringContext;static preview::ObjectMarker replacementMarker;
 stage("retirement race");
 retiringContext=&otherContext;replacementMarker=marker;replacementMarker.position.x+=30;
 marker.mesh=detailed.artifact;CHECK(preview::set_object_markers(&otherContext,std::span{&marker,1},large));
 allocation_hook=[] {preview::cleanup_context(retiringContext);
  (void)preview::set_object_markers(retiringContext,std::span{&replacementMarker,1});};
 const auto retired=preview::object_solid_geometry_for(&otherContext);
 stage("retirement returned");
 CHECK(!allocation_hook&&retired->size()==detailed.artifact->indices().size());
 CHECK(!preview::detail::g_objectMarkers.at(&otherContext).solid_geometry);
 const auto replacement=preview::object_solid_geometry_for(&otherContext);CHECK(replacement->size()==6);
 // A lighting publisher can run on another thread during a build. The next
 // acquire must observe it even when marker publication itself is unchanged.
 static epochengine::lighting::LightingFrame concurrentLights;
 stage("lighting race");
 concurrentLights=directional;concurrentLights.environment.ambient.r+=.17f;
 marker.mesh=detailed.artifact;CHECK(preview::set_object_markers(&otherContext,std::span{&marker,1},large));
 allocation_hook=[] {std::thread publish([] {preview::set_lighting_frame(retiringContext,concurrentLights);});publish.join();};
 const auto duringLighting=preview::object_solid_geometry_for(&otherContext);CHECK(!allocation_hook);
 stage("lighting returned");
 const auto afterLighting=preview::object_solid_geometry_for(&otherContext);CHECK(afterLighting!=duringLighting);
 // Two dense instances exceed the cache working set but remain valid source
 // geometry. The existing uncached route must not retain another large cache.
 std::array oversized{marker,marker};oversized[1].position.x+=30;
 stage("oversized");
 CHECK(preview::set_object_markers(&otherContext,oversized,large));
 const auto uncached=preview::object_solid_geometry_for(&otherContext);
 CHECK(uncached->size()==2*detailed.artifact->indices().size());
 CHECK(!preview::detail::g_objectMarkers.at(&otherContext).solid_geometry);
 // Actual equal-content/separate-allocation boundary: existing buffers plus
 // the new join fit128MiB; retaining a second decoded source would not.
 int allocationContext{};
 std::vector<std::uint32_t> repeated(1'725'000);
 for(std::size_t i=0;i<repeated.size();++i)repeated[i]=indices[i%indices.size()];
 const auto allocationSource=mesh::build_artifact(vertices,repeated,{1'048'576,6'291'456,64ull*1024*1024});CHECK(allocationSource);
 preview::ObjectMarker allocationMarker{.primitive=preview::ObjectPreviewPrimitive::IndexedMesh,.mesh=allocationSource.artifact};
 std::array allocationFrame{allocationMarker,preview::ObjectMarker{.position={50,0,0}}};
 CHECK(preview::set_object_markers(&allocationContext,allocationFrame,large));
 const auto allocationGeometry=preview::object_solid_geometry_for(&allocationContext);
 CHECK(preview::detail::g_objectMarkers.at(&allocationContext).solid_geometry);
 auto wireProbe=allocationFrame;wireProbe[0].wireframe=true;
 CHECK(!preview::set_object_markers(&allocationContext,wireProbe,large));
 CHECK(preview::object_solid_geometry_for(&allocationContext)==allocationGeometry);
 const auto decodedCopy=mesh::decode_artifact(mesh::encode_artifact(*allocationSource.artifact),
  {1'048'576,6'291'456,64ull*1024*1024});CHECK(decodedCopy);
 CHECK(decodedCopy.artifact!=allocationSource.artifact&&decodedCopy.artifact->key()==allocationSource.artifact->key());
 allocationFrame[0].mesh=decodedCopy.artifact;allocationFrame[1].position.y+=1;
 CHECK(preview::set_object_markers(&allocationContext,allocationFrame,large));
 const auto separateAllocation=preview::object_solid_geometry_for(&allocationContext);
 CHECK(separateAllocation->size()==allocationGeometry->size());
 CHECK(!preview::detail::g_objectMarkers.at(&allocationContext).solid_geometry);
 preview::cleanup_context(&allocationContext);
 preview::cleanup_context(&solidContext);preview::cleanup_context(&otherContext);
 CHECK(preview::object_solid_geometry_for(&solidContext)->empty()&&solids->size()==detailed.artifact->indices().size());
 std::ofstream benchmark{
#ifdef NDEBUG
 "solid-benchmark-Release.json"
#else
 "solid-benchmark-Debug.json"
#endif
 };
 benchmark<<"{\"unique_vertices\":"<<detailed.artifact->vertices().size()<<",\"expanded_vertices\":"<<solids->size()
  <<",\"baseline_ns\":"<<oldNs<<",\"cold_optimized_ns\":"<<coldNs<<",\"unchanged_1000_ns\":"<<hitNs
  <<",\"orbit_other_body_ns\":"<<orbitNs<<",\"bitwise_equal\":true,\"native_gpu\":false}\n";
 CHECK(benchmark.good());
 return 0;
}
'''
    return fixture.replace("RUNTIME_RETIREMENT",retirement).replace("RUNTIME_PUBLICATION",runtime_publication).replace("BASELINE_OWNER", baseline).replace("SHARED_ROUTING_OWNER",routing).replace("ACTUAL_MANAGED_GATE", managed_gate).replace("ACTUAL_EXPLORATION_CAMERA", exploration_camera).replace("PREVIEW_FIELDS",preview_fields).replace("CACHE_HELPERS",cache_helpers).replace("SCENE_INTERACTION_OWNER",scene_interaction_owner).replace("GENERIC_FOCUS",generic_focus).replace("EDITOR_FRAMING",editor_framing).replace("PREVIEW_PUMP",preview_pump).replace("PREVIEW_RETIRE",preview_retire).replace("PREVIEW_RESET",preview_reset).replace("PAINT_FOCUS_DISPATCH",paint_focus_dispatch).replace("PAINT_FOCUS",paint_focus).replace("RETIRE_CALL",retire_call).replace("CALLER_ENTRY",caller_entry).replace("CALLER_DISPATCH",caller_dispatch).replace("CALLER_DIRECT",caller_direct).replace("CALLER_WIRE",caller_wire)


def actual_function(source, signature):
    begin = source.index(signature)
    opening = source.index("{", begin)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[begin:cursor]


def source_receipt_fixture():
    play = globals().get('SOURCE_RECEIPT_PLAY') or (ROOT / 'Engine/src/epoch/implementation/epoch.project_play_scene.inl').read_text(encoding='utf-8-sig')
    planet = globals().get('SOURCE_RECEIPT_PLANET') or (ROOT / 'Engine/src/project/project.planetary.cpp').read_text(encoding='utf-8-sig')
    pump = actual_function(play, '            void pump_planetary_runtime(')
    changed = actual_function(play, '            [[nodiscard]] static bool planetary_view_changed(')
    start = planet.index('            if(cancellation.stop_requested()||!source_generation_is_current(project_root,*admitted,cancellation)', planet.index('    StreamResult stream_step('))
    end = planet.index('            operation.ok = true;', start) + len('            operation.ok = true;')
    final = planet[start:end].replace('!source_generation_is_current(', '!project_planetary::source_generation_is_current(')
    code = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
import asset.mesh_artifact;
import project.planetary;
import render.preview_grid;
import scene.snapshot;
import editor.task_scheduler;
namespace fs=std::filesystem;
namespace asset=epochengine::asset;
namespace previewgrid=epochengine::previewgrid;
namespace scene=epochengine::scene;
namespace editor_tasks=epochengine::editor_tasks;
namespace timing {using Clock=std::chrono::steady_clock;}
namespace core {struct Context {};}
namespace logger {enum class LogLevel {WARN};struct Sink {void log(LogLevel,const std::string&,std::source_location){}};Sink get(const char*){return {};}}
#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"Source receipt contract line %d: %s\n",__LINE__,#value);return __LINE__;}}while(false)
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
void write(const fs::path& path,const std::string& text){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;}
namespace package_source {
struct Attachment {std::string content_hash{"water-a"};bool source_attached()const{return true;}};
Attachment load_project_attachment(const fs::path&,const char*,std::stop_token){return {};}
}
namespace project_planetary {
using namespace epochengine::project_planetary;
std::thread::id owner_thread;
std::atomic<unsigned> owner_checks{},worker_checks{},producer_calls{};
std::promise<void>* entered{};std::shared_future<void> release;
bool throw_worker{},omit_receipt{};
std::shared_ptr<const asset::mesh::Artifact> replacement;
bool source_generation_is_current(const fs::path& root,const SourceGeneration& source,std::stop_token stop={}) {
 if(std::this_thread::get_id()==owner_thread)++owner_checks;else ++worker_checks;
 return !stop.stop_requested()&&root==source.project_root&&read(root/"project.epoch.json")==source.project_manifest
  &&read(root/"provider.txt")==source.content_hash;
}
std::optional<std::string> recipe_guard(const fs::path& root){return read(root/"recipe.txt");}
bool mesh_generation_receipt_matches(const fs::path&,const SourceGeneration&,const Settings&,const MeshGenerationIdentity&){
 throw std::logic_error("Runtime fixture unexpectedly received an Editor mesh binding");
}
StreamResult final_admission(const fs::path& project_root,const std::optional<SourceGeneration>& admitted,
 const std::optional<std::string>& observedRecipe,const Settings& settings,StreamResult result,std::stop_token cancellation) {
 auto& operation=result.operation;
 const bool providerWater=true;const package_source::Attachment water;
 const StreamMeshBinding* binding=nullptr;
 FINAL_ADMISSION
 if(omit_receipt)result.verified_source.reset();
 return result;
}
// Recorded provider boundary. The actual worker admission block above and
// actual owner below are compiled unchanged; no producer process is launched.
StreamResult stream_step(const fs::path& root,Settings settings,StreamView view,std::stop_token stop) {
 ++producer_calls;
 const auto admitted=SourceGeneration{root,read(root/"project.epoch.json"),read(root/"provider.txt")};
 const auto recipe=recipe_guard(root);
 if(entered)entered->set_value();
 if(release.valid())release.wait();
 if(throw_worker)throw std::runtime_error("recorded worker failure");
 StreamResult result;result.request_id=view.request_id;result.snapshot_ready=true;result.snapshot=replacement;
 return final_admission(root,admitted,recipe,settings,std::move(result),stop);
}
}
struct Transform {std::array<float,3>position{},rotation{},scale{1,1,1};};
struct Entity {std::uint64_t source_id{7};Transform transform;std::optional<scene::SceneMeshSnapshot>mesh_asset;bool visible()const{return true;}};
struct RuntimeStreamProbe {
 fs::path root;std::string m_projectRoot;
 std::vector<Entity>m_entities{Entity{}};
 std::optional<project_planetary::Settings>m_planetarySettings{project_planetary::Settings{}},m_planetaryRequestSettings;
 std::optional<project_planetary::SourceGeneration>m_meshPlanetarySource;
 struct Preview {std::array<double,3>camera{0,0,26.5};std::optional<std::array<double,3>>forward{{0,0,-1}},up{{0,1,0}};};
 std::optional<Preview>m_explorationPreview{Preview{}};
 struct {double seconds{};std::uint32_t anchor{};}m_explorationFrame;
 bool m_explorationController{true},m_planetaryPaused{},m_planetaryFailed{},m_planetaryContinue{true};
 bool m_planetaryPublishedOcean{};
 std::optional<project_planetary::GeneratedMeshFrame>m_planetaryMeshFrame;
 std::optional<std::uint32_t>m_planetaryLodBody;
 std::shared_ptr<const epochengine::project_planetary_snapshot::Snapshot>m_planetaryWaterSource;
 std::uint64_t m_planetaryEntity{7},m_planetarySequence{};
 std::uint32_t m_planetaryRequestBody{};
 project_planetary::StreamView m_planetaryView;
 std::string m_planetaryStatus,m_runtimeViewStatus;
 std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>>m_meshes;
 timing::Clock::time_point m_planetaryNextTick{};
 std::optional<std::future<project_planetary::StreamResult>>m_planetaryPending;
 editor_tasks::CancellationTicket m_planetaryCancellation;
 std::unique_ptr<editor_tasks::Scheduler>m_planetaryScheduler=std::make_unique<editor_tasks::Scheduler>(
  editor_tasks::SchedulerLimits{.worker_count=1,.retained_finished_nodes=32});
 unsigned reset_water_calls{},geometry_admissions{};
 explicit RuntimeStreamProbe(fs::path path):root(std::move(path)),m_projectRoot(root.generic_string()) {
  write(root/"project.epoch.json","manifest-a");write(root/"provider.txt","source-a");write(root/"recipe.txt","recipe-a");
  m_meshPlanetarySource=project_planetary::SourceGeneration{root,"manifest-a","source-a"};
 }
 ~RuntimeStreamProbe(){m_planetaryScheduler->graph().Shutdown();}
 bool runtime_water_visibility_pending()const{return false;}
 bool planetary_mesh_frame_fits(const std::shared_ptr<const asset::mesh::Artifact>& value){++geometry_admissions;return bool(value);}
 void reset_planetary_water(){++reset_water_calls;}
 void ensure_planetary_scheduler(){(void)m_planetaryScheduler->maintain();}
 void retry(){m_planetaryFailed=false;m_planetaryContinue=true;m_planetaryNextTick={};}
 ACTUAL_CHANGED
 ACTUAL_PUMP
};
int main(){
 project_planetary::owner_thread=std::this_thread::get_id();
 const auto base=fs::absolute("source-receipt-data");std::error_code ignored;
 // This fixture owns the fixed child directory and no operator project data.
 fs::create_directories(base);
 std::array<asset::mesh::Vertex,3>vertices{};
 vertices[0].position={0,0,0};vertices[1].position={1,0,0};vertices[2].position={0,1,0};
 const std::array<std::uint32_t,3>indices{0,1,2};
 const auto mesh=asset::mesh::build_artifact(vertices,indices);CHECK(mesh);project_planetary::replacement=mesh.artifact;
 core::Context context;
 const auto wait_ready=[&](auto& owner){return owner.m_planetaryPending
  &&owner.m_planetaryPending->wait_for(std::chrono::seconds{3})==std::future_status::ready;};
 const auto retained=mesh.artifact;
 // The default API result carries no verified source. It cannot authorize a
 // runtime replacement just because the producer claims operation.ok.
 CHECK(!project_planetary::StreamResult{}.verified_source);
 {
  RuntimeStreamProbe owner{base/"valid"};owner.m_meshes[7]=retained;
  owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);
  CHECK(owner.geometry_admissions==1&&owner.reset_water_calls==1&&!owner.m_planetaryFailed&&!owner.m_planetaryPending);
  CHECK(project_planetary::owner_checks==0&&project_planetary::worker_checks==1);
 }
 // A source replaced while queued can be valid for a newer project generation;
 // its receipt must still not cross into the old running project.
 {
  RuntimeStreamProbe owner{base/"queued-change"};owner.m_meshes[7]=retained;
  std::promise<void>entered,release;auto blocked=owner.m_planetaryScheduler->submit("Hold existing LOD worker",[&]{entered.set_value();release.get_future().wait();});
  entered.get_future().wait();owner.pump_planetary_runtime(&context,1280,720);
  write(owner.root/"provider.txt","source-b");release.set_value();blocked.get();CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);
  CHECK(owner.m_planetaryFailed&&!owner.m_planetaryPending&&owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
  CHECK(owner.m_planetaryStatus.find("verified source")!=std::string::npos);
  write(owner.root/"provider.txt","source-a");owner.retry();owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);CHECK(!owner.m_planetaryFailed&&owner.geometry_admissions==1);
 }
 // The actual final worker block rejects source, manifest and recipe changes
 // during work, retains the old terrain, and permits an explicit retry.
 for(unsigned mutation=0;mutation<3;++mutation){
  RuntimeStreamProbe owner{base/("during-work-"+std::to_string(mutation))};owner.m_meshes[7]=retained;
  std::promise<void>entered,release;project_planetary::entered=&entered;project_planetary::release=release.get_future().share();
  owner.pump_planetary_runtime(&context,1280,720);entered.get_future().wait();
  const auto file=mutation==0?"provider.txt":mutation==1?"project.epoch.json":"recipe.txt";
  const auto old=read(owner.root/file);write(owner.root/file,"changed");release.set_value();CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);
  CHECK(owner.m_planetaryFailed&&!owner.m_planetaryPending&&owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
  CHECK(owner.m_planetaryStatus.find("changed during patch admission")!=std::string::npos);
  project_planetary::entered=nullptr;project_planetary::release={};write(owner.root/file,old);
  owner.retry();owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));owner.pump_planetary_runtime(&context,1280,720);
  CHECK(!owner.m_planetaryFailed&&owner.geometry_admissions==1);
 }
 // A ready receipt is tied to all three owner identity fields. No disk access
 // is needed to refuse a changed in-memory project admission.
 for(unsigned mutation=0;mutation<4;++mutation){
  RuntimeStreamProbe owner{base/("stale-owner-"+std::to_string(mutation))};owner.m_meshes[7]=retained;
  owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));
  if(mutation==0)owner.m_meshPlanetarySource->project_root=base/"foreign";
  if(mutation==1)owner.m_meshPlanetarySource->project_manifest="changed";
  if(mutation==2)owner.m_meshPlanetarySource->content_hash="changed";
  if(mutation==3)owner.m_meshPlanetarySource.reset();
  owner.pump_planetary_runtime(&context,1280,720);
  CHECK(owner.m_planetaryFailed&&owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
 }
 {
  RuntimeStreamProbe owner{base/"missing-receipt"};owner.m_meshes[7]=retained;project_planetary::omit_receipt=true;
  owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));owner.pump_planetary_runtime(&context,1280,720);
  CHECK(owner.m_planetaryFailed&&owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
  project_planetary::omit_receipt=false;owner.retry();owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);CHECK(owner.geometry_admissions==1&&!owner.m_planetaryFailed);
 }
 {
  RuntimeStreamProbe owner{base/"cancel"};owner.m_meshes[7]=retained;
  std::promise<void>entered,release;project_planetary::entered=&entered;project_planetary::release=release.get_future().share();
  owner.pump_planetary_runtime(&context,1280,720);entered.get_future().wait();
  (void)owner.m_planetaryScheduler->cancel(owner.m_planetaryCancellation);release.set_value();CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);
  CHECK(!owner.m_planetaryPending&&owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
  project_planetary::entered=nullptr;project_planetary::release={};owner.retry();owner.pump_planetary_runtime(&context,1280,720);
  CHECK(wait_ready(owner));owner.pump_planetary_runtime(&context,1280,720);CHECK(owner.geometry_admissions==1&&!owner.m_planetaryFailed);
 }
 {
  RuntimeStreamProbe owner{base/"worker-failure"};owner.m_meshes[7]=retained;project_planetary::throw_worker=true;
  owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));owner.pump_planetary_runtime(&context,1280,720);
  CHECK(owner.m_planetaryFailed&&!owner.m_planetaryPending&&owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
  project_planetary::throw_worker=false;owner.retry();owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);CHECK(owner.geometry_admissions==1&&!owner.m_planetaryFailed);
 }
 CHECK(project_planetary::owner_checks==0);
 std::printf("owner_filesystem_checks=%u worker_final_checks=%u provider_calls=%u\n",project_planetary::owner_checks.load(),
  project_planetary::worker_checks.load(),project_planetary::producer_calls.load());
}
'''
    return code.replace('FINAL_ADMISSION', final).replace('ACTUAL_CHANGED', changed).replace('ACTUAL_PUMP', pump)


def shortcut_fixture():
    owner = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8-sig")
    functions = "\n".join(actual_function(owner, signature) for signature in (
        "void update_runtime_view_shortcuts(", "void finish_runtime_screenshot(",
        "void finish_runtime_mesh_display(", "[[nodiscard]] float runtime_panel_height(",
        "[[nodiscard]] bool runtime_panel_captures_pointer(", "[[nodiscard]] gui::WidgetBounds runtime_panel_bounds(",
        "void draw_runtime_controls_restore(", "void draw_runtime_controls_hide(",
        "static void apply_runtime_inspection_light(", "[[nodiscard]] std::string_view runtime_terrain_activity(",
        "[[nodiscard]] int consume_runtime_navigation_wheel("))
    return r'''
#include <array>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <cmath>
#include <limits>
#include <utility>
namespace input {enum class Key {LeftControl,RightControl,LeftAlt,RightAlt,LeftSuper,RightSuper,F12,PrintScreen,F2};}
namespace core {struct Context {
 bool focused=true;std::array<bool,9> held{},pressed{};
 bool has_input_focus_safe() const{return focused;}
 bool is_key_held_safe(input::Key key) const{return held[static_cast<int>(key)];}
 bool is_key_down_safe(input::Key key) const{return pressed[static_cast<int>(key)];}
 void* get_hwnd() const{return reinterpret_cast<void*>(0x1234);}
};}
namespace gui {struct Vec2 {float x{},y{};};struct WidgetBounds{Vec2 position{},size{};};bool captured{};int wheel=7,consumed{};
 bool keyboard_input_captured(){return captured;}
 int consume_mouse_wheel_delta(){++consumed;return std::exchange(wheel,0);}
 WidgetBounds recordedWindow{},recordedButton{};Vec2 cursor{};bool nextClick{};std::string label;
 void begin_window(std::string_view,Vec2 position,Vec2 size,bool,bool){recordedWindow={position,size};}
 void end_window(){}void set_cursor(Vec2 position){cursor=position;}
 bool button(std::string_view value,Vec2 size){label=value;recordedButton={cursor,size};return std::exchange(nextClick,false);}
 struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};
}
namespace editor_clipboard_screenshot {
 int calls{};std::uintptr_t last{};
 struct Result{bool ok;std::string message;};
 Result copy_window(std::uintptr_t value){++calls;last=value;return {true,"copied"};}
}
namespace logger {
 enum class LogLevel{INFO,WARN};struct Logger {void log(LogLevel,const std::string&,std::source_location){}};
 Logger& get(const char*){static Logger instance;return instance;}
}
namespace lighting {
 struct Color{float r{},g{},b{};};struct LightingFrame{unsigned revision{};struct{Color ambient{};}environment;};
}
namespace platform::child_process {
 enum class MeshDrawMode{solid,wireframe};struct RuntimeMeshCommand{std::uint64_t sequence{};MeshDrawMode mode{};};
 inline unsigned acknowledgements{};inline bool lastPublished{};inline MeshDrawMode lastMode{};
 void acknowledge_runtime_mesh_command(const RuntimeMeshCommand&,bool published,MeshDrawMode mode){++acknowledgements;lastPublished=published;lastMode=mode;}
}
struct ActualOwner {
 bool m_editorRuntimeControlled{};std::optional<platform::child_process::RuntimeMeshCommand> m_ownedRuntimeMeshCommand{};
 bool m_meshWireframe{},m_screenshotHeld{},m_wireframeHeld{};
 std::optional<bool> m_requestedMeshWireframe{};
 std::string m_runtimeViewStatus{},m_tileMapPath{};float m_runtimeViewportHeight{628},m_runtimeViewportWidth{1280};
 bool m_runtimeControlsVisible{true};
 bool m_runtimeInspectionLight{};
 bool m_planetaryFailed{},m_planetaryPaused{},m_planetaryPending{},m_planetaryContinue{};
 struct Gameplay {bool active()const{return false;}};Gameplay* m_gameplayRuntime{};
 bool m_planetarySettings{},m_explorationController{};
 int m_screenshotAfterFrames{};std::string m_screenshotStatus{};
''' + functions + r'''
};
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
int main(){
 ActualOwner owner;core::Context context;
 const auto key=[&](input::Key value,bool held){context.held[static_cast<int>(value)]=held;};
 key(input::Key::F12,true);context.focused=false;
 owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_screenshotAfterFrames==0);
 context.focused=true;key(input::Key::LeftAlt,true);
 owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_screenshotAfterFrames==0);
 key(input::Key::LeftAlt,false); // Releasing a modifier does not repeat a held shortcut.
 owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_screenshotAfterFrames==0);
 key(input::Key::F12,false);owner.update_runtime_view_shortcuts(&context,false);
 key(input::Key::F12,true);owner.update_runtime_view_shortcuts(&context,false);
 CHECK(owner.m_screenshotAfterFrames==2);
 owner.finish_runtime_screenshot(&context);CHECK(editor_clipboard_screenshot::calls==0);
 owner.finish_runtime_screenshot(&context);CHECK(editor_clipboard_screenshot::calls==1);
 CHECK(editor_clipboard_screenshot::last==0x1234&&owner.m_screenshotStatus=="copied");
 owner.update_runtime_view_shortcuts(&context,false);owner.finish_runtime_screenshot(&context);
 CHECK(editor_clipboard_screenshot::calls==1);
 key(input::Key::F12,false);owner.update_runtime_view_shortcuts(&context,false);
 key(input::Key::PrintScreen,true);owner.update_runtime_view_shortcuts(&context,false);
 CHECK(owner.m_screenshotAfterFrames==2);
 context.focused=false;owner.finish_runtime_screenshot(&context);
 CHECK(owner.m_screenshotAfterFrames==0&&editor_clipboard_screenshot::calls==1);
 context.focused=true;key(input::Key::PrintScreen,false);key(input::Key::F2,true);
 owner.update_runtime_view_shortcuts(&context,true);CHECK(!owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe));
 key(input::Key::F2,false);owner.update_runtime_view_shortcuts(&context,false);
 key(input::Key::F2,true);gui::captured=true;
 owner.update_runtime_view_shortcuts(&context,false);CHECK(!owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe));
 gui::captured=false;key(input::Key::F2,false);owner.update_runtime_view_shortcuts(&context,false);
 key(input::Key::F2,true);owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe));
 owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe));
 key(input::Key::F2,false);owner.update_runtime_view_shortcuts(&context,false);
 key(input::Key::F2,true);owner.update_runtime_view_shortcuts(&context,false);CHECK(!owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe));
 owner.m_screenshotAfterFrames=1;owner.finish_runtime_screenshot(nullptr);
 CHECK(owner.m_screenshotAfterFrames==0&&editor_clipboard_screenshot::calls==1);
 context.held.fill(false);context.pressed[static_cast<int>(input::Key::F2)]=true;
 owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe));
 context.pressed.fill(false);owner.update_runtime_view_shortcuts(&context,false);
 CHECK(owner.m_requestedMeshWireframe.value_or(owner.m_meshWireframe)); // A complete queued tap need not remain held.
 context.pressed[static_cast<int>(input::Key::F12)]=true;
 owner.update_runtime_view_shortcuts(&context,false);CHECK(owner.m_screenshotAfterFrames==2);
 owner.finish_runtime_mesh_display(false,true);
 CHECK(!owner.m_meshWireframe&&!owner.m_requestedMeshWireframe&&!owner.m_runtimeViewStatus.empty());
 owner.finish_runtime_mesh_display(true,false);CHECK(owner.m_runtimeViewStatus.empty());
 owner.m_requestedMeshWireframe=true;
 owner.finish_runtime_mesh_display(true,true);
 CHECK(owner.m_meshWireframe&&!owner.m_requestedMeshWireframe&&owner.m_runtimeViewStatus.empty());
 owner.finish_runtime_mesh_display(false,false);CHECK(owner.m_meshWireframe);
 owner.m_editorRuntimeControlled=true;owner.m_ownedRuntimeMeshCommand=platform::child_process::RuntimeMeshCommand{1,platform::child_process::MeshDrawMode::solid};
 owner.finish_runtime_mesh_display(false,false);CHECK(owner.m_meshWireframe&&!owner.m_ownedRuntimeMeshCommand);
 CHECK(platform::child_process::acknowledgements==1&&!platform::child_process::lastPublished&&platform::child_process::lastMode==platform::child_process::MeshDrawMode::wireframe);
 owner.m_ownedRuntimeMeshCommand=platform::child_process::RuntimeMeshCommand{2,platform::child_process::MeshDrawMode::solid};
 owner.finish_runtime_mesh_display(true,false);CHECK(!owner.m_meshWireframe&&platform::child_process::acknowledgements==2&&platform::child_process::lastPublished);
 key(input::Key::F2,false);context.pressed[static_cast<int>(input::Key::F2)]=false;owner.update_runtime_view_shortcuts(&context,false);
 key(input::Key::F2,true);owner.update_runtime_view_shortcuts(&context,false);CHECK(!owner.m_requestedMeshWireframe);
 key(input::Key::F2,false);owner.update_runtime_view_shortcuts(&context,false);
 owner.m_editorRuntimeControlled=false;
 owner.m_explorationController=true;owner.m_planetarySettings=true;
 CHECK(owner.runtime_panel_height()>250);
 CHECK(owner.runtime_terrain_activity()=="Terrain: ready");
 owner.m_planetaryContinue=true;CHECK(owner.runtime_terrain_activity()=="Terrain: refinement queued");
 owner.m_planetaryPending=true;CHECK(owner.runtime_terrain_activity()=="Terrain: refining (request in progress)");
 owner.m_planetaryPaused=true;CHECK(owner.runtime_terrain_activity()=="Terrain: stopping refinement");
 owner.m_planetaryPending=false;CHECK(owner.runtime_terrain_activity()=="Terrain: paused");
 owner.m_planetaryFailed=true;CHECK(owner.runtime_terrain_activity()=="Terrain: needs attention");
 owner.m_planetaryFailed=owner.m_planetaryPaused=owner.m_planetaryPending=owner.m_planetaryContinue=false;
 lighting::LightingFrame normal{};normal.revision=42;normal.environment.ambient={0.05f,0.8f,0.1f};
 auto inspected=normal;owner.apply_runtime_inspection_light(inspected);
 CHECK(inspected.environment.ambient.r==0.6f&&inspected.environment.ambient.g==0.8f&&inspected.environment.ambient.b==0.6f);
 CHECK(normal.environment.ambient.r==0.05f&&normal.environment.ambient.g==0.8f&&normal.environment.ambient.b==0.1f);
 CHECK(inspected.revision==normal.revision&&!owner.m_runtimeInspectionLight);
 CHECK(owner.runtime_panel_height()<=580&&owner.runtime_panel_height()>0);
 owner.m_runtimeViewportHeight=450;CHECK(owner.runtime_panel_height()<=402);
 CHECK(owner.runtime_panel_captures_pointer({24,24}));
 CHECK(owner.runtime_panel_captures_pointer({454,24+owner.runtime_panel_height()}));
 CHECK(!owner.runtime_panel_captures_pointer({23.9f,24})&&!owner.runtime_panel_captures_pointer({454.1f,24}));
 CHECK(!owner.runtime_panel_captures_pointer({24,std::numeric_limits<float>::quiet_NaN()}));
 gui::captured=false;CHECK(owner.consume_runtime_navigation_wheel({900,900},false)==0&&gui::consumed==0&&gui::wheel==7);
 owner.m_explorationController=false;
 CHECK(owner.consume_runtime_navigation_wheel({24,24},false)==0&&gui::consumed==0);
 CHECK(owner.consume_runtime_navigation_wheel({900,900},true)==0&&gui::consumed==0);
 gui::captured=true;CHECK(owner.consume_runtime_navigation_wheel({900,900},false)==0&&gui::consumed==0);
 gui::captured=false;CHECK(owner.consume_runtime_navigation_wheel({900,900},false)==7&&gui::consumed==1&&gui::wheel==0);
 CHECK(owner.consume_runtime_navigation_wheel({900,900},false)==0&&gui::consumed==2);
 owner.m_runtimeViewportHeight=20;CHECK(owner.runtime_panel_height()==1);
 const auto tiny=owner.runtime_panel_bounds();
 CHECK(owner.runtime_panel_captures_pointer({tiny.position.x,tiny.position.y+tiny.size.y}));
 CHECK(!owner.runtime_panel_captures_pointer({tiny.position.x,tiny.position.y+tiny.size.y+0.1f}));
 owner.m_runtimeViewportHeight=720;owner.m_runtimeControlsVisible=false;
 const auto compact=owner.runtime_panel_bounds();CHECK(compact.position.x==1096&&compact.position.y==24&&compact.size.x==160&&compact.size.y==32);
 CHECK(!owner.runtime_panel_captures_pointer({24,24})&&!owner.runtime_panel_captures_pointer({100,200}));
 CHECK(owner.runtime_panel_captures_pointer({compact.position.x+1,compact.position.y+1}));
 gui::wheel=9;const auto oldConsumed=gui::consumed;
 CHECK(owner.consume_runtime_navigation_wheel({100,200},false)==9&&gui::consumed==oldConsumed+1);
 gui::wheel=9;CHECK(owner.consume_runtime_navigation_wheel(compact.position,false)==0&&gui::wheel==9);
 gui::nextClick=false;owner.draw_runtime_controls_restore();CHECK(!owner.m_runtimeControlsVisible&&gui::label=="Controls");
 CHECK(gui::recordedButton.position.x==compact.position.x&&gui::recordedButton.position.y==compact.position.y&&gui::recordedButton.size.x==compact.size.x&&gui::recordedButton.size.y==compact.size.y);
 gui::nextClick=true;owner.draw_runtime_controls_restore();CHECK(owner.m_runtimeControlsVisible&&owner.runtime_panel_captures_pointer({100,100}));
 gui::nextClick=false;owner.draw_runtime_controls_hide(390);CHECK(owner.m_runtimeControlsVisible);
 gui::nextClick=true;owner.draw_runtime_controls_hide(390);CHECK(!owner.m_runtimeControlsVisible&&gui::label=="Hide Controls");
 for(const auto dimensions:std::array{gui::Vec2{1393,943},gui::Vec2{400,300},gui::Vec2{180,120},gui::Vec2{20,20},gui::Vec2{1,1}})
  for(bool visible:{false,true}){owner.m_runtimeViewportWidth=dimensions.x;owner.m_runtimeViewportHeight=dimensions.y;owner.m_runtimeControlsVisible=visible;
   const auto bounds=owner.runtime_panel_bounds();CHECK(bounds.position.x>=0&&bounds.position.y>=0&&bounds.size.x>0&&bounds.size.y>0);
   CHECK(bounds.position.x+bounds.size.x<=dimensions.x&&bounds.position.y+bounds.size.y<=dimensions.y);
   CHECK(owner.runtime_panel_captures_pointer(bounds.position));}
 return 0;
}
'''


class RuntimePreviewControls(unittest.TestCase):
    def test_actual_planetary_horizontal_look_direction(self):
        owner = ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"
        source = owner.read_text(encoding="utf-8")
        begin = source.index("            void advance_planetary_exploration(")
        end = source.index("            [[nodiscard]] std::vector<previewgrid::ObjectMarker>", begin)
        body = source[begin:end]
        keyboard = re.search(r"^\s*(controls\.yaw=.*;)$", body, re.M)
        mouse = re.search(r"^\s*(controls\.yaw[+-]=.*;)$", body, re.M)
        self.assertIsNotNone(keyboard)
        self.assertIsNotNone(mouse)
        keyboard, mouse = keyboard.group(1), mouse.group(1)
        proof = ROOT / "build/validation/planetary-look-direction-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        code = r'''
#include <cmath>
#include <initializer_list>
namespace input {
enum class Action {LookRight,LookLeft};
}
double actual_yaw(bool right,bool left,double dx,bool dragging) {
 struct {double yaw{},elapsed_seconds{0.1};}controls;
 struct {double mouse_look_sensitivity{0.15};}m_runtimeInputProfile;
 const auto axis=[&](input::Action positive,input::Action negative) {
  const auto held=[&](input::Action action){return action==input::Action::LookRight?right:left;};
  return double(held(positive))-double(held(negative));
 };
 // Actual production assignment, extracted without changing its expression.
 KEYBOARD
 if(dragging) {
  struct {double x;} mouse{dx},m_explorationLastMouse{0};
  constexpr double radians=0.017453292519943295;
  MOUSE
 }
 return controls.yaw;
}
int main() {
 // Original Simulation rotates forward(0,0,-1) about up(0,1,0):
 // rightward look needs negative yaw, yielding a positive X direction.
 const auto right_key=actual_yaw(true,false,0,false),left_key=actual_yaw(false,true,0,false);
 const auto right_mouse=actual_yaw(false,false,10,true),left_mouse=actual_yaw(false,false,-10,true);
 if(!(right_key<0&&left_key>0&&right_mouse<0&&left_mouse>0))return 1;
 for(const auto yaw:{right_key,right_mouse})if(-std::sin(yaw)<=0)return 2;
 if(actual_yaw(false,false,100,false)!=0)return 3;
 if(std::abs(right_key+left_key)>1e-12||std::abs(right_mouse+left_mouse)>1e-12)return 4;
 if(actual_yaw(true,false,10,true)>=right_key)return 5;
 return 0;
}
'''.replace("KEYBOARD", keyboard).replace("MOUSE", mouse)
        (proof / "runtime.yaw_contract.cpp").write_text(code, encoding="utf-8")
        (proof / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetaryLookDirection LANGUAGES CXX)
add_executable(yaw_contract runtime.yaw_contract.cpp)
target_compile_features(yaw_contract PRIVATE cxx_std_23)
enable_testing()
add_test(NAME planetary_look_direction COMMAND yaw_contract)
""", encoding="utf-8")
        configure = ["cmake", "-S", str(proof), "-B", str(proof / "cmake")]
        configure += (["-G", "Visual Studio 17 2022", "-A", "x64",
                       "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"]
                      if os.name == "nt" else ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"])
        commands = [("configure", configure)]
        for config in (("Debug", "Release") if os.name == "nt" else ("Release",)):
            commands += [(config + "-build", ["cmake", "--build", str(proof / "cmake"), "--config", config,
                                             "--parallel", "1"]),
                         (config + "-test", ["ctest", "--test-dir", str(proof / "cmake"), "-C", config,
                                            "--output-on-failure"])]
        results = []
        for label, command in commands:
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            (proof / (label + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
            results.append({"phase": label, "exit": run.returncode, "command": command})
            (proof / "result.json").write_text(json.dumps({
                "scope": "Actual production horizontal look assignments compiled against recorded action/mouse inputs; no window/GPU",
                "production_assignments": [keyboard, mouse], "phases": results, "native": False}, indent=2) + "\n",
                encoding="utf-8")
            self.assertEqual(run.returncode, 0, (run.stdout + run.stderr)[-8000:])

    def test_actual_geometry_and_shortcut_owners(self):
        proof = Path(os.environ.get("EPOCH_RUNTIME_PREVIEW_PROOF",
            str(ROOT / "Artifacts/Reports/runtime-planetary-publication-nov07")))
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "preview.wire_contract.cpp").write_text(wire_fixture(), encoding="utf-8")
        (proof / "runtime.shortcuts_contract.cpp").write_text(shortcut_fixture(), encoding="utf-8")
        (proof / "runtime.source_receipt_contract.cpp").write_text(source_receipt_fixture(), encoding="utf-8")
        module_map = {}
        owners = (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules")
        for owner in owners:
            for path in owner.rglob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    self.assertNotIn(match.group(1), module_map)
                    module_map[match.group(1)] = path
        pending, closure = ["render.preview_grid", "render.device", "authoring.texture_detail", "scene.interaction", "project.planetary"], set()
        while pending:
            name = pending.pop()
            if name in closure:
                continue
            self.assertIn(name, module_map)
            closure.add(name)
            pending.extend(re.findall(r"^(?:export\s+)?import\s+([\w.:]+);",
                module_map[name].read_text(encoding="utf-8-sig"), re.M))
        modules = " ".join('"' + module_map[name].as_posix() + '"' for name in sorted(closure))
        bases = " ".join('"' + path.as_posix() + '"' for path in owners)
        # The dense-mesh budget cases run unoptimized in Windows Debug.
        wire_timeout = 90 if os.name == "nt" else 30
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(RuntimePreview LANGUAGES CXX)
add_executable(wire_contract preview.wire_contract.cpp "{(ROOT / 'Engine/src/render/render.preview_camera.cpp').as_posix()}" "{(ROOT / 'Engine/src/render/render.preview_lighting.cpp').as_posix()}" "{(ROOT / 'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_residency.cpp').as_posix()}")
target_sources(wire_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {bases} FILES {modules})
target_include_directories(wire_contract PRIVATE "{(ROOT / 'Engine/include').as_posix()}")
if(MSVC)
 target_compile_definitions(wire_contract PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
include("{(ROOT / 'Engine/cmake/epoch_task_scheduler.cmake').as_posix()}")
epoch_admit_task_scheduler(wire_contract "{ROOT.as_posix()}")
add_executable(shortcut_contract runtime.shortcuts_contract.cpp)
add_executable(source_receipt_contract runtime.source_receipt_contract.cpp "{(ROOT / 'Engine/src/render/render.preview_camera.cpp').as_posix()}" "{(ROOT / 'Engine/src/render/render.preview_lighting.cpp').as_posix()}" "{(ROOT / 'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_residency.cpp').as_posix()}")
target_sources(source_receipt_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {bases} FILES {modules})
target_include_directories(source_receipt_contract PRIVATE "{(ROOT / 'Engine/include').as_posix()}")
target_link_libraries(source_receipt_contract PRIVATE Epoch::TaskScheduler)
foreach(target wire_contract shortcut_contract source_receipt_contract)
 target_compile_features(${{target}} PRIVATE cxx_std_23)
 if(MSVC)
  target_compile_options(${{target}} PRIVATE /sdl /permissive- /utf-8 /Zc:__cplusplus /EHsc)
 endif()
endforeach()
enable_testing()
add_test(NAME mesh_wireframe COMMAND wire_contract)
add_test(NAME runtime_shortcuts COMMAND shortcut_contract)
add_test(NAME runtime_source_receipt COMMAND source_receipt_contract)
set_tests_properties(mesh_wireframe PROPERTIES TIMEOUT {wire_timeout})
set_tests_properties(runtime_shortcuts runtime_source_receipt PROPERTIES TIMEOUT 30)
''', encoding="utf-8")
        configure = ["cmake", "-S", str(proof), "-B", str(proof / "cmake")]
        configure += ["-G", "Visual Studio 17 2022", "-A", "x64"] if os.name == "nt" else ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
        commands = [("configure", configure)]
        for config in (("Debug", "Release") if os.name == "nt" else ("Release",)):
            build = ["cmake", "--build", str(proof / "cmake"), "--config", config, "--parallel", "1"]
            if os.name == "nt":
                build += ["--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]
            commands += [(config + "-build", build), (config + "-contract", ["ctest", "--test-dir", str(proof / "cmake"), "-C", config, "--output-on-failure"])]
        completed = []
        for label, command in commands:
            # A clean hosted MSVC build compiles two module consumers for each
            # configuration. Keep a separate bound for compilation and execution.
            timeout = 420 if os.name == "nt" and label.endswith("-build") else 180
            result = subprocess.run(command, cwd=ROOT, env=dict(os.environ, MSBUILDDISABLENODEREUSE="1"),
                capture_output=True, text=True, timeout=timeout)
            (proof / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            completed.append({"phase": label, "exit": result.returncode, "command": command})
            (proof / "result.json").write_text(json.dumps({"module_count": len(closure), "completed": completed,
                "native_screenshot_or_wireframe": False, "full_engine_build": False}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-10000:])


if __name__ == "__main__":
    unittest.main()
