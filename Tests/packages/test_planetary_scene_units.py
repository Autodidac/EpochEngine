"""Actual Editor/runtime metric projection owners; CPU only, no provider or GUI."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


def owner(source, marker):
    start = source.index(marker)
    body = re.search(r"\n[ \t]*\{", source[start:])
    if not body:
        raise AssertionError(marker)
    opening = start + body.end() - 1
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


CODE = r'''
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
bool deny_solar_allocations{};
void* operator new(std::size_t bytes){if(deny_solar_allocations)throw std::bad_alloc{};if(auto* value=std::malloc(bytes?bytes:1u))return value;throw std::bad_alloc{};}
void operator delete(void* value)noexcept{std::free(value);}
void operator delete(void* value,std::size_t)noexcept{std::free(value);}
import asset.mesh_artifact;
import asset.cloud_volume;
import project.planetary;
import project.planetary_runtime;
import scene.snapshot;
import scene.serializer;
import scene.document;
import scene.runtime;
import render.preview_grid;
import render.camera;
import render.arcade;
import render.lighting;
import context.type;
// CPU-only Context port: the current publisher checks the backend kind when
// a textured mesh is present. No native window or graphics device is owned.
namespace epochengine::core {class Context {public:ContextType type{ContextType::OpenGL};};}
namespace core=epochengine::core;
namespace scene=epochengine::scene;
namespace scene_runtime=epochengine::scene_runtime;
namespace project_planetary=epochengine::project_planetary;
namespace project_planetary_runtime=epochengine::project_planetary_runtime;
namespace asset=epochengine::asset;
namespace previewgrid=epochengine::previewgrid;
namespace render_camera=epochengine::render_camera;
namespace lighting=epochengine::lighting;
using ProjectRuntimeEntity=scene_runtime::RuntimeEntity;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Planet scene units failed line %d\n",line);std::exit(89);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
bool near(double a,double b,double tolerance=0.0002){return std::abs(a-b)<=tolerance;}
double vector_length(previewgrid::Vec3 value){return std::hypot(double(value.x),double(value.y),double(value.z));}
// The real Windows translation unit retains Win32's legacy near macro.
// Exercise the owning helpers under that context rather than a clean fixture.
#define near
constexpr int m_runtimeViewportWidth=1280,m_runtimeViewportHeight=720;
// These cases publish without a runtime camera packet. Camera and geometry
// math below stay real while the asynchronous scheduler remains separate.
bool pump_runtime_planetary_frame(const void*,previewgrid::SceneFrameRequest,bool){std::abort();}
RUNTIME_PUBLICATION
#undef near
class RuntimeProbe {
public:
 std::vector<ProjectRuntimeEntity> m_entities;
 std::uint64_t m_planetaryEntity{3};
 std::uint32_t m_planetaryLodBody{99};
 std::optional<project_planetary::GeneratedMeshFrame> m_planetaryMeshFrame;
 std::optional<project_planetary_runtime::PreviewFrame> m_explorationPreview;
 project_planetary_runtime::Frame m_explorationFrame;
 std::array<double,3> m_explorationRenderOrigin{};
 std::optional<previewgrid::Camera> m_explorationRenderCamera;
 std::string m_explorationAdvanceStatus;
 bool m_explorationController=true;
 lighting::LightManager m_lightingManager{128};lighting::LightHandle m_sunlightHandle;
 lighting::LightingFrame m_lightingFrame;
 std::optional<project_planetary::Settings> m_meshPlanetarySettings{project_planetary::Settings{}};
 std::optional<project_planetary::Settings> m_planetarySettings{project_planetary::Settings{}};
 std::shared_ptr<const epochengine::project_planetary_snapshot::Snapshot> m_planetaryWaterSource;
 struct RuntimeClock{double playhead_seconds{};};std::optional<RuntimeClock>m_runtimePresentation;
 bool m_runtimeCloudsVisible{true},m_runtimeCloudBackendUnavailable{};
 std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>> m_meshes;
 static std::uint64_t suppressed_planet(bool rebased,const std::vector<previewgrid::ObjectMarker>& planetaryMarkers,
  std::uint64_t m_planetaryEntity){return RUNTIME_SUPPRESSION;}
 RUNTIME_METHODS
 std::optional<previewgrid::Camera> exploration_camera(const project_planetary_runtime::Frame& frame){
  const auto projected=project_planetary_runtime::preview_frame(frame);if(!projected)return {};
  const auto* entity=planetary_entity();if(!entity)return {};
  const struct {project_planetary_runtime::Frame frame;}result{frame};
  static const epochengine::core::Context contextPort;
  const auto* context=&contextPort;
  CAMERA_POLICY
  return m_explorationRenderCamera;
 }
 bool scale_refused{};std::string m_explorationStatus;
 void check_exploration_scale(){
  scale_refused=true;
  SCALE_GUARD
  scale_refused=false;
 }
};
struct EditorEntity {
 std::array<float,3> position{},rotation{},scale{1,1,1};std::string name,type,category;
 std::optional<std::array<double,3>> positionAnchor;
 std::uint64_t sceneObjectId{3};bool editorOnly{};
 std::optional<scene::SceneMeshSnapshot> meshAsset;
 std::shared_ptr<const asset::mesh::Artifact> meshSnapshot;
 std::optional<project_planetary::GeneratedMeshFrame> meshSnapshotFrame;
 std::shared_ptr<const previewgrid::MeshBaseColorBinding> meshBaseColorTexture;
};
struct EditorState {std::string projectStatus;};
std::shared_ptr<const previewgrid::MeshPaintView> forest_paint_view(const EditorState&,const EditorEntity&){return {};}
// This legacy/unit-framing lane has no admitted provider body inventory.
// Actual body bounds/selection/Outliner projection are exercised by
// test_planetary_mesh_admission; keep its absent-inventory boundary explicit.
struct BodyFocusPort {previewgrid::Vec3 target{};float radius{};};
std::optional<BodyFocusPort> editor_planetary_body_focus(const EditorState&,const EditorEntity&){return {};}
bool editor_entity_selected(const EditorState&,std::uint64_t){return false;}
previewgrid::Vec3 selection_color_for_entity(const EditorEntity&,bool){return {1,1,1};}
float marker_radius_for_entity(const EditorEntity&){return .34f;}
bool marker_selected_for_entity(const EditorEntity&,bool selected){return selected;}
bool is_plant_lab_preview_entity(const EditorEntity&){return false;}
previewgrid::ObjectPreviewPrimitive preview_primitive_for_entity(const EditorEntity&){return previewgrid::ObjectPreviewPrimitive::Cube;}
EDITOR_FOCUS_TARGET
void editor_marker(EditorState& state,const EditorEntity& entity,std::vector<previewgrid::ObjectMarker>& markers){
 bool forestPreview{},forestPainting{},meshWireframe{};const auto previewPosition=entity.position;
 EDITOR_MARKER
}
void editor_focus(const epochengine::core::Context* ctx,const EditorEntity* planet){
 EditorState state;
 struct {int width=1600,height=1000;}viewport;
 EDITOR_FOCUS
}
std::optional<std::array<double,3>> editor_stream_inverse(const EditorEntity& entity,const previewgrid::Camera& camera){
 const auto* entityAt=&entity;bool viewValid=true;
 project_planetary::StreamView desiredView;
 EDITOR_INVERSE
 return viewValid?std::optional{desiredView.camera}:std::nullopt;
}
std::optional<std::array<double,3>> runtime_stream_inverse(const ProjectRuntimeEntity& entity,const previewgrid::Camera& camera){
 const auto& transform=entity.transform;
 const auto meshScale=scene::mesh_render_scale(transform.scale,entity.mesh_asset);if(!meshScale)return {};
 project_planetary::StreamView view;
 RUNTIME_INVERSE
 return view.camera;
}
bool import_coordinates(EditorEntity& entity,std::optional<project_planetary::GeneratedMeshFrame> mesh_frame){
 const auto fail=[](std::string_view){return false;};
 IMPORT_COORDINATES
 return true;
}
void check_adaptive_near_plane(){
 previewgrid::Camera camera;camera.eye={0,0,0};camera.target={0,0,1};camera.farPlane=1e9f;
 std::vector<ProjectRuntimeEntity> entities;
 std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>> meshes;
 const auto near_plane=[&](double clearance=5000,double units=1.0,std::array<double,3> origin={}){
  return project_play_adaptive_near_plane(camera,clearance,units,entities,meshes,3,origin);};
 CHECK(near_plane()==500.0f);
 CHECK(near(near_plane(-5000),1e-7,1e-10));
 ProjectRuntimeEntity cube;cube.source_id=9;cube.type="StaticMesh";cube.category="World";
 cube.flags=scene_runtime::RuntimeEntityFlag::visible;cube.transform.position={0,0,2};entities.push_back(cube);
 CHECK(near(near_plane(),.75)); // 1m cube two metres ahead survives the altitude proposal.
 camera.eye={0,0,1.75f};camera.target={0,0,2.75f};CHECK(near(near_plane(),1e-7,1e-10)); // inside bounds
 camera.eye={0,0,3};camera.target={0,0,4};CHECK(near_plane()==500); // wholly behind
 camera.eye={0,0,0};camera.target={0,0,1};entities[0].transform.position[2]=100;
 CHECK(near(near_plane(),49.75)); // current authored movement, without publishing a frame
 entities[0].transform.position[2]=1002;CHECK(near(near_plane(5000,1,{0,0,-1000}),.75));
 deny_solar_allocations=true;const auto allocationFreeNear=near_plane(5000,1,{0,0,-1000});deny_solar_allocations=false;
 CHECK(near(allocationFreeNear,.75));
 const auto metric=near_plane(5000,1,{0,0,-1000});
 entities[0].transform.position={0,0,2.0f*.0004f};entities[0].transform.scale={.0004f,.0004f,.0004f};
 // Primitive proxy minimum sizes intentionally remain scene-unit controls;
 // indexed canonical geometry below verifies physically equivalent scales.
 std::array<asset::mesh::Vertex,3> points{};
 points[0].position={-1,-1,-1};points[1].position={1,-1,1};points[2].position={0,1,1};
 const std::array<std::uint32_t,3> triangle{0,1,2};const auto mesh=asset::mesh::build_artifact(points,triangle);CHECK(mesh);
 scene::SceneMeshSnapshot metadata{"Assets/Mesh/cockpit.epoch_mesh",mesh.artifact->key().words,3,3};
 entities[0].type="IndexedMesh";entities[0].mesh_asset=metadata;meshes.emplace(9,mesh.artifact);
 entities[0].transform.position={0,0,3};entities[0].transform.scale={1,1,1};
 const auto metricMesh=near_plane();CHECK(near(metricMesh,1.0));
 entities[0].transform.position={0,0,3*.0004f};entities[0].transform.scale={.0004f,.0004f,.0004f};
 CHECK(near(near_plane(5000*.0004, .0004)/.0004,metricMesh,1e-5));
 entities[0].transform.position={3,4,8};entities[0].transform.scale={-2,.5f,3};
 for(const auto rotation:std::array{std::array<float,3>{0,0,0},std::array<float,3>{17,31,59},std::array<float,3>{0,90,0}}){
  entities[0].transform.rotation=rotation;
  const auto marker=project_play_marker_for_entity(entities[0],mesh.artifact,{});CHECK(marker);
  const auto bounds=project_play_marker_depth_bounds(*marker,camera);CHECK(bounds);
  CHECK(near_plane()<=(*bounds)[0]&&near_plane()>0);
  for(const auto& point:mesh.artifact->vertices()){
   const auto world=previewgrid::add(marker->position,previewgrid::rotate_euler_degrees(
    {point.position[0]*marker->scale.x,point.position[1]*marker->scale.y,point.position[2]*marker->scale.z},marker->rotationDegrees));
   CHECK(world.z>=(*bounds)[0]-.00001&&world.z<=(*bounds)[1]+.00001);
  }
 }
 entities[0].source_id=3;CHECK(near_plane()==500); // only the controller-managed planet is excluded
 entities[0].source_id=9;entities[0].mesh_asset.reset();entities[0].type="ForestFoliageCluster";
 CHECK(near(near_plane(),1e-7,1e-10)); // unknown geometry cannot justify clipping it
 entities[0].flags=scene_runtime::RuntimeEntityFlag::none;CHECK(near_plane()==500);
 CHECK(std::isfinite(metric));
}
int main(int argc,char**argv){
 CHECK(argc==2);const auto output=std::filesystem::path{argv[1]};std::filesystem::create_directories(output);
 check_adaptive_near_plane();
 std::array<asset::mesh::Vertex,3> vertices{};vertices[0].position={-10,0,0};vertices[1].position={10,0,0};vertices[2].position={0,10,0};
 const std::array<std::uint32_t,3> indices{0,1,2};const auto artifact=asset::mesh::build_artifact(vertices,indices);CHECK(artifact);
 scene::SceneSnapshot saved;saved.scene_id="metric-world";saved.project_id="metric-project";saved.world_name="25km";
 saved.primary_camera=1;saved.primary_spawn=2;
 saved.objects.push_back({.id=1,.name="Camera",.type="Camera"});saved.objects.push_back({.id=2,.name="Spawn",.type="Spawn"});
 scene::SceneMeshSnapshot metadata{"Assets/Mesh/planet.epoch_mesh",artifact.artifact->key().words,3,3,
  scene::SceneMeshCoordinates{2500,{1e9,2e9,3e9}}};
 saved.objects.push_back({.id=3,.name="Planet",.type="IndexedMesh",.category="Planetary Terrain",.position={100,200,300},.mesh_asset=metadata});
 saved.objects.push_back({.id=4,.name="Pad",.type="Level",.category="World",.scale={14,1,14}});
 saved.objects.push_back({.id=5,.name="One metre",.type="StaticMesh",.category="World",.position={3,4,5}});
 const auto serialized=scene::serialize_snapshot_text(saved);CHECK(!serialized.empty());
 {std::ofstream file(output/"metric.epoch",std::ios::binary);file<<serialized;}
 std::ifstream file(output/"metric.epoch",std::ios::binary);const std::string disk{std::istreambuf_iterator<char>{file},{}};
 const auto reopened=scene::parse_snapshot_text(disk);CHECK(reopened&&reopened.snapshot.objects[2].mesh_asset==metadata);
 scene_runtime::SceneRuntime runtime;CHECK(runtime.replace(reopened.snapshot).committed());
 RuntimeProbe probe;probe.m_entities.assign(runtime.projection()->entities().begin(),runtime.projection()->entities().end());
 CHECK(probe.planetary_entity());
 const epochengine::core::Context contextPort,focusPort,legacyFocusPort;
 const auto* context=&contextPort;
 std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>> meshes{{3,artifact.artifact}};
 CHECK(publish_project_play_markers(context,probe.m_entities,&meshes));
 auto markers=previewgrid::detail::g_objectMarkers.at(context).markers;
 CHECK(markers.size()==3&&markers[0].scale.x==2500&&10.0*markers[0].scale.x==25000);
 CHECK(markers[1].scale.x==14&&markers[1].scale.z==14&&markers[2].scale.x==1);
 CHECK(markers[0].position.x==100&&markers[0].position.y==200&&markers[0].position.z==300); // origin is not authored placement
 EditorEntity editorPlanet;editorPlanet.position={100,200,300};editorPlanet.type="IndexedMesh";editorPlanet.meshAsset=metadata;editorPlanet.meshSnapshot=artifact.artifact;
 EditorState editorState;std::vector<previewgrid::ObjectMarker> editorMarkers;
 editor_marker(editorState,editorPlanet,editorMarkers);CHECK(editorMarkers.size()==1&&editorMarkers[0].scale.x==2500&&editorMarkers[0].position.x==100);
 const auto* focusContext=&focusPort;
 editor_focus(focusContext,&editorPlanet);const auto metricDistance=previewgrid::camera_distance_for(focusContext);
 CHECK(std::isfinite(metricDistance)&&metricDistance>10000);
 const auto* legacyFocusContext=&legacyFocusPort;
 auto oldEditorPlanet=editorPlanet;oldEditorPlanet.meshAsset->coordinates.reset();
 editor_focus(legacyFocusContext,&oldEditorPlanet);const auto oldDistance=previewgrid::camera_distance_for(legacyFocusContext);
 CHECK(oldDistance<100&&near(metricDistance/oldDistance,2500,1));
 auto imported=editorPlanet;imported.meshAsset->coordinates.reset();
 CHECK(import_coordinates(imported,project_planetary::GeneratedMeshFrame{2500,{9e8,8e8,7e8}}));
 CHECK(imported.position==editorPlanet.position&&imported.scale==editorPlanet.scale&&imported.meshAsset->coordinates->meters_per_unit==2500);
 // Real runtime frame owner: selected anchor affects only normalized storage,
 // never the physical size of ordinary objects or any body's rendered radius.
 auto& frame=probe.m_explorationFrame;frame.valid=true;frame.anchor=0;frame.target=5;frame.body_count=2;frame.seed=1337;
 frame.camera={25001.8,0,0};frame.forward={-1,0,0};frame.up={0,1,0};frame.time_scale=1;frame.speed_factor=1;frame.altitude=1.8;
 frame.bodies[0]={.id=0,.parent=-1,.name="World",.center={0,0,0},.radius=25000,.landable=true,.coarse_mesh=artifact.artifact};
 frame.bodies[1]={.id=5,.parent=0,.name="Moon",.center={1000000,0,0},.radius=1000,.landable=true,.coarse_mesh=artifact.artifact};
 CHECK(project_planetary_runtime::valid_frame(frame));
 probe.m_explorationPreview=project_planetary_runtime::preview_frame(frame);CHECK(probe.m_explorationPreview&&probe.planetary_scene_units()==2500);
 const auto firstBodyMarkers=probe.planetary_exploration_markers();CHECK(firstBodyMarkers.size()==2&&firstBodyMarkers[0].scale.x==2500&&firstBodyMarkers[1].scale.x==100);
 frame.bodies[0].cloud_mesh=artifact.artifact;frame.bodies[0].cloud_wind_degrees=90.0;
 probe.m_explorationPreview=project_planetary_runtime::preview_frame(frame);
 std::vector<previewgrid::CloudVolume> cloudVolumes;
 auto cloudMarkers=probe.planetary_exploration_markers();CHECK(probe.append_planetary_cloud_markers(cloudMarkers,cloudVolumes,true));
 CHECK(cloudVolumes.empty()&&!probe.m_runtimeCloudBackendUnavailable);
 CHECK(cloudMarkers.size()==3&&cloudMarkers.back().mesh==artifact.artifact&&cloudMarkers.back().preciseTransform);
 const auto cloudPoint=cloudMarkers.back().preciseTransform->apply({10,0,0});
 CHECK(near(cloudPoint[0],100,1e-8)&&near(cloudPoint[1],200,1e-8)&&near(cloudPoint[2],-24700,1e-8));
 probe.m_explorationRenderOrigin={500,600,700};
 std::vector<previewgrid::ObjectMarker> movedClouds;CHECK(probe.append_planetary_cloud_markers(movedClouds,cloudVolumes,true)&&movedClouds.size()==1);
 probe.m_runtimeCloudsVisible=false;std::vector<previewgrid::ObjectMarker> hiddenClouds;
 CHECK(probe.append_planetary_cloud_markers(hiddenClouds,cloudVolumes,true)&&hiddenClouds.empty());
 probe.m_runtimeCloudsVisible=true;
 auto genericCloudProbe=probe;genericCloudProbe.m_explorationPreview.reset();genericCloudProbe.m_explorationController=false;
 auto cloudSnapshot=std::make_shared<epochengine::project_planetary_snapshot::Snapshot>();
 cloudSnapshot->metadata.cloud=epochengine::project_planetary_snapshot::CloudLayer{
  .body=0,.center_meters={10,20,30},.radius_meters=25000,.artifact=artifact.artifact};
 genericCloudProbe.m_planetaryWaterSource=cloudSnapshot;
 std::vector<previewgrid::ObjectMarker> genericClouds;
 CHECK(genericCloudProbe.append_planetary_cloud_markers(genericClouds,cloudVolumes,true,302400)&&genericClouds.size()==1);
 CHECK(genericClouds[0].mesh==artifact.artifact&&genericClouds[0].preciseTransform);
 CHECK(publish_project_play_markers(context,probe.m_entities,&meshes,{},genericClouds,
  RuntimeProbe::suppressed_planet(false,genericClouds,3)));
 CHECK(previewgrid::detail::g_objectMarkers.at(context).markers.size()==4);
 CHECK(previewgrid::detail::g_objectMarkers.at(context).markers[0].mesh==artifact.artifact);
 CHECK(RuntimeProbe::suppressed_planet(true,cloudMarkers,3)==3);
 genericCloudProbe.m_runtimeCloudsVisible=false;genericClouds.clear();
 CHECK(genericCloudProbe.append_planetary_cloud_markers(genericClouds,cloudVolumes,true)&&genericClouds.empty());
 const auto movedCloudPoint=movedClouds.back().preciseTransform->apply({10,0,0});
 CHECK(near(movedCloudPoint[0],cloudPoint[0]-500,1e-8)&&near(movedCloudPoint[1],cloudPoint[1]-600,1e-8)
    &&near(movedCloudPoint[2],cloudPoint[2]-700,1e-8));
 probe.m_explorationRenderOrigin={};frame.bodies[0].cloud_mesh.reset();frame.bodies[0].cloud_wind_degrees=0;
 probe.m_explorationPreview=project_planetary_runtime::preview_frame(frame);
 probe.m_planetaryLodBody=0;probe.m_meshes[3]=artifact.artifact;
 probe.m_planetaryMeshFrame=project_planetary::GeneratedMeshFrame{1,{25001.8,0,0}};
 const auto metricMarkers=probe.planetary_exploration_markers();CHECK(metricMarkers.size()==2&&metricMarkers[0].preciseTransform);
 CHECK(near(metricMarkers[0].scale.x,1)&&near(metricMarkers[0].preciseTransform->position[0],25101.8,1e-8));
 CHECK(near(metricMarkers[0].preciseTransform->position[1],200,1e-8));
 probe.m_planetaryMeshFrame.reset();probe.m_planetaryLodBody=99;
 const auto firstEye=RuntimeProbe::planetary_preview_vector(probe.m_explorationPreview->camera,*probe.planetary_entity(),true,probe.planetary_scene_units());CHECK(firstEye);
 CHECK(near(firstEye->x,25101.8,.002)&&firstEye->y==200&&firstEye->z==300);
 CHECK(probe.planetary_scene_origin_offset()==std::array<double,3>{0,0,0}); // planet uses initialbody basis, not provenance origin
 frame.anchor=5;probe.m_explorationPreview=project_planetary_runtime::preview_frame(frame);CHECK(probe.planetary_scene_units()==100);
 const auto offset=probe.planetary_scene_origin_offset();CHECK(offset==std::array<double,3>{-1000000,0,0});
 auto advancedFrame=frame;advancedFrame.anchor=0; // camera policy must use the just-advanced frame, not stored anchor5
 CHECK(probe.planetary_scene_origin_offset(&advancedFrame)==std::array<double,3>{0,0,0});
 CHECK(probe.planetary_scene_origin_offset()==offset);
 CHECK(probe.exploration_camera(frame));
 RuntimeProbe cameraProbe;cameraProbe.m_entities=probe.m_entities;cameraProbe.m_explorationFrame=frame;
 advancedFrame.camera={30000,0,0};advancedFrame.altitude=5000;
 auto cockpit=probe.m_entities.back();cockpit.source_id=19;cockpit.transform.position={30099.8f,200,300};cockpit.transform.scale={.1f,.1f,.1f};
 cameraProbe.m_entities.push_back(cockpit);
 const auto protectedCamera=cameraProbe.exploration_camera(advancedFrame);
 CHECK(protectedCamera&&protectedCamera->nearPlane<.1f&&protectedCamera->nearPlane>0);
 RuntimeProbe scaledCameraProbe;scaledCameraProbe.m_entities.push_back(probe.m_entities[2]);
 const auto unscaledCamera=scaledCameraProbe.exploration_camera(advancedFrame);CHECK(unscaledCamera&&near(unscaledCamera->nearPlane,500));
 scaledCameraProbe.m_entities[0].transform.scale={.25f,2,-4};
 const auto scaledCamera=scaledCameraProbe.exploration_camera(advancedFrame);
 CHECK(scaledCamera&&near(scaledCamera->nearPlane,125)); // minimum singular scale protects the nearest terrain
 CHECK(near(scaledCamera->farPlane,(double(unscaledCamera->farPlane)-64)*4+64,8)); // far still covers the maximum extent
 advancedFrame.altitude=-5000;
 const auto undergroundCamera=scaledCameraProbe.exploration_camera(advancedFrame);
 CHECK(undergroundCamera&&near(undergroundCamera->nearPlane,2.5e-8,1e-11));
 advancedFrame.altitude=0;
 const auto surfaceCamera=scaledCameraProbe.exploration_camera(advancedFrame);
 CHECK(surfaceCamera&&surfaceCamera->nearPlane==undergroundCamera->nearPlane);
 advancedFrame.altitude=5000;
 const auto secondBodyMarkers=probe.planetary_exploration_markers();CHECK(secondBodyMarkers[0].scale.x==2500&&secondBodyMarkers[1].scale.x==100);
 CHECK(publish_project_play_markers(context,probe.m_entities,&meshes,{},secondBodyMarkers,3,false,offset));
 const auto shifted=previewgrid::detail::g_objectMarkers.at(context).markers;
 CHECK(shifted.size()==4&&shifted[0].scale.x==14&&shifted[1].scale.x==1&&shifted[1].position.x==3-1000000);
 lighting::LightingFrame lights;lights.revision=7;
 lights.lights.push_back({.desc={.kind=lighting::LightKind::Point,.position={3,4,5}}});
 lights.lights.push_back({.desc={.kind=lighting::LightKind::Spot,.position={3,4,5},.direction={0,-1,0}}});
 lights.lights.push_back({.desc={.kind=lighting::LightKind::Directional,.position={7,8,9},.direction={1,-1,0}}});
 const auto shiftedLights=project_play_lighting_for_origin(lights,offset);CHECK(shiftedLights&&shiftedLights->revision==7);
 CHECK(shiftedLights->lights[0].desc.position.x==shifted[1].position.x&&shiftedLights->lights[0].desc.position.y==shifted[1].position.y);
 CHECK(shiftedLights->lights[1].desc.position.x==shifted[1].position.x&&lighting::same_light_desc(shiftedLights->lights[2].desc,lights.lights[2].desc));
 const auto legacyLights=project_play_lighting_for_origin(lights,{});CHECK(legacyLights);
 for(unsigned i=0;i<3;++i)CHECK(lighting::same_light_desc(legacyLights->lights[i].desc,lights.lights[i].desc));
 const scene::Vec3 signedZero{-0.0f,0.0f,-0.0f};
 const auto signedPoint=project_play_position_for_origin(signedZero,{});CHECK(signedPoint);
 CHECK(std::bit_cast<std::array<std::uint32_t,3>>(*signedPoint)==std::bit_cast<std::array<std::uint32_t,3>>(signedZero));
 lights.lights[0].desc.position={-0.0f,0.0f,-0.0f};
 const auto signedLight=project_play_lighting_for_origin(lights,{});CHECK(signedLight);
 CHECK(std::bit_cast<std::array<std::uint32_t,3>>(signedLight->lights[0].desc.position)==std::bit_cast<std::array<std::uint32_t,3>>(signedZero));
 CHECK(!project_play_lighting_for_origin(lights,{std::numeric_limits<double>::infinity(),0,0}));
 // The real retained manager owns authored lights and one explicitly enabled
 // provider sun. No-op frames do not allocate, advance revisions or damage.
 lighting::LightManager sunManager{128};sunManager.set_environment({{.16f,.18f,.22f}});
 const auto author=sunManager.create({.kind=lighting::LightKind::Point,.position={3,4,5}});
 CHECK(author.valid());const auto authored=*sunManager.get(author);
 auto publishedSun=sunManager.build_frame();lighting::LightHandle sunHandle;
 CHECK(update_project_play_sunlight(sunManager,sunHandle,publishedSun,std::nullopt,{})==ProjectSunlightUpdate::unchanged);
 const auto offRevision=sunManager.revision();
 project_planetary_runtime::Sunlight sun{.direction_to_sun={1,0,0}};
 CHECK(update_project_play_sunlight(sunManager,sunHandle,publishedSun,sun,{})==ProjectSunlightUpdate::changed);
 CHECK(sunHandle.valid()&&publishedSun.lights.size()==2&&sunManager.revision()==offRevision+1);
 CHECK(lighting::same_light_desc(*sunManager.get(author),authored)&&lighting::same_environment(publishedSun.environment,{{.16f,.18f,.22f}}));
 CHECK(sunManager.get(sunHandle)->direction.x==-1&&sunManager.get(sunHandle)->intensity==1.12f);
 const auto stableSun=publishedSun;const auto stableRevision=sunManager.revision();
 deny_solar_allocations=true;
 const auto unchangedSun=update_project_play_sunlight(sunManager,sunHandle,publishedSun,sun,{});
 deny_solar_allocations=false;
 CHECK(unchangedSun==ProjectSunlightUpdate::unchanged&&sunManager.revision()==stableRevision
  &&lighting::diff_lighting_frames(&stableSun,publishedSun).empty());
 deny_solar_allocations=true;
 const auto failedSun=update_project_play_sunlight(sunManager,sunHandle,publishedSun,sun,{0,90,0});
 deny_solar_allocations=false;
 CHECK(failedSun==ProjectSunlightUpdate::refused&&sunManager.revision()==stableRevision
  &&lighting::diff_lighting_frames(&stableSun,publishedSun).empty());
 CHECK(update_project_play_sunlight(sunManager,sunHandle,publishedSun,sun,{0,90,0})==ProjectSunlightUpdate::changed);
 CHECK(near(sunManager.get(sunHandle)->direction.z,1,1e-6)&&sunManager.revision()==stableRevision+1);
 CHECK(lighting::diff_lighting_frames(&stableSun,publishedSun).global_invalidation);
 const auto directional=*sunManager.get(sunHandle);
 const std::array<lighting::FrameLight,1> justSun{{{sunHandle,directional}}};
 const auto Lambert=lighting::evaluate_reference_raster_lighting(justSun,{{0,0,0},{0,0,-1},{0,0,-1},{1,1,1},{0,0,0},24});
 CHECK(near(Lambert.diffuse.r,1.12,1e-6)&&near(Lambert.diffuse.g,1.12,1e-6));
 auto badSun=sun;badSun.direction_to_sun[0]=std::numeric_limits<double>::quiet_NaN();
 CHECK(update_project_play_sunlight(sunManager,sunHandle,publishedSun,badSun,{})==ProjectSunlightUpdate::refused);
 CHECK(update_project_play_sunlight(sunManager,sunHandle,publishedSun,std::nullopt,{})==ProjectSunlightUpdate::changed);
 CHECK(publishedSun.lights.size()==1&&lighting::same_light_desc(publishedSun.lights[0].desc,authored));
 const auto disabledRevision=sunManager.revision();
 CHECK(update_project_play_sunlight(sunManager,sunHandle,publishedSun,std::nullopt,{})==ProjectSunlightUpdate::unchanged&&sunManager.revision()==disabledRevision);
 lighting::LightManager fullManager{1};CHECK(fullManager.create(authored).valid());
 auto fullPublished=fullManager.build_frame();const auto beforeFull=fullPublished;lighting::LightHandle fullSun;
 CHECK(update_project_play_sunlight(fullManager,fullSun,fullPublished,sun,{})==ProjectSunlightUpdate::refused);
 CHECK(!fullSun.valid()&&fullManager.revision()==beforeFull.revision&&lighting::diff_lighting_frames(&beforeFull,fullPublished).empty());
 probe.m_explorationFrame.sunlight=sun;
 probe.update_planetary_sunlight();CHECK(probe.m_lightingFrame.lights.empty()); // unsolicited metadata cannot enable off setting
 probe.m_meshPlanetarySettings->original_sunlight=true;probe.update_planetary_sunlight();CHECK(probe.m_lightingFrame.lights.size()==1);
 probe.m_meshPlanetarySettings->original_sunlight=false;probe.update_planetary_sunlight();CHECK(probe.m_lightingFrame.lights.empty());
 // Universe authoring uses the exporter basis; it is rebased for the anchor,
 // rather than being added to the scene's authored object transform.
 probe.m_meshPlanetarySettings->mode="universe";
 CHECK(probe.planetary_scene_origin_offset()==std::array<double,3>{999000000,2000000000,3000000000});
 probe.m_meshPlanetarySettings->mode="planet";
 for(const auto rotation:std::array{std::array<float,3>{0,0,0},std::array<float,3>{17,31,59},std::array<float,3>{0,0,90}}){
  probe.m_entities[2].transform.rotation=rotation;editorPlanet.rotation=rotation;
  const auto& entity=*probe.planetary_entity();
  const std::array<double,3> source{10.004,0.002,-.003}; // 10m physical motion relative to radius10
  const auto point=RuntimeProbe::planetary_preview_vector(source,entity,true,2500);CHECK(point);
  previewgrid::Camera camera;camera.eye=*point;
  const auto editorInverse=editor_stream_inverse(editorPlanet,camera),runtimeInverse=runtime_stream_inverse(entity,camera);
  CHECK(editorInverse&&runtimeInverse);
  for(unsigned axis=0;axis<3;++axis){CHECK(near((*editorInverse)[axis],source[axis],0.00001));CHECK(near((*runtimeInverse)[axis],source[axis],0.00001));}
  const auto direction=RuntimeProbe::planetary_preview_vector({1,0,0},entity,false,2500);CHECK(direction&&near(vector_length(*direction),1));
  const auto moved=RuntimeProbe::planetary_preview_vector({source[0]+.004,source[1],source[2]},entity,true,2500);CHECK(moved);
  CHECK(near(vector_length(previewgrid::subtract(*moved,*point)),10,.006));
 }
 probe.check_exploration_scale();CHECK(!probe.scale_refused);
 // The same LOD inverse must use the precise camera before subtracting a
 // large authored planet translation. Widening an already rounded float eye
 // loses submetre changes and can ask the provider for the wrong close cut.
 {
  auto planet=probe.m_entities[2];auto editor=editorPlanet;
  planet.transform.position={-94398480,-6190638,-917183};editor.position=planet.transform.position;
  planet.transform.scale={.5f,2,1.5f};editor.scale=planet.transform.scale;
  const std::array<double,3> source{10.0001,.0002,-.0003};
  for(const auto rotation:std::array{std::array<float,3>{0,0,0},std::array<float,3>{17,31,59}}){
   planet.transform.rotation=rotation;editor.rotation=rotation;
   const previewgrid::Vec3 angle{rotation[0],rotation[1],rotation[2]};
   const std::array basis{previewgrid::rotate_euler_degrees({1,0,0},angle),
    previewgrid::rotate_euler_degrees({0,1,0},angle),previewgrid::rotate_euler_degrees({0,0,1},angle)};
   const auto units=scene::mesh_render_scale(planet.transform.scale,planet.mesh_asset);CHECK(units);
   previewgrid::Camera camera;camera.navigationValid=true;camera.navigation.forward={0,0,-1};
   for(unsigned axis=0;axis<3;++axis){
    double point=planet.transform.position[axis];
    for(unsigned local=0;local<3;++local)point+=source[local]*(*units)[local]
     *(axis==0?basis[local].x:axis==1?basis[local].y:basis[local].z);
    camera.navigation.eye[axis]=point;
   }
   camera.eye={float(camera.navigation.eye[0]),float(camera.navigation.eye[1]),float(camera.navigation.eye[2])};
   const auto a=editor_stream_inverse(editor,camera),b=runtime_stream_inverse(planet,camera);CHECK(a&&b);
   for(unsigned axis=0;axis<3;++axis){CHECK(near((*a)[axis],source[axis],.00001));CHECK(near((*b)[axis],source[axis],.00001));}
   if(rotation==std::array<float,3>{}){
    camera.navigationValid=false;const auto rounded=editor_stream_inverse(editor,camera);CHECK(rounded);
    CHECK(std::abs((*rounded)[0]-source[0])>.00001); // counterfactual former float path loses the movement
   }
  }
 }
 probe.m_entities[2].transform.scale={2,1,1};probe.check_exploration_scale();CHECK(probe.scale_refused&&probe.m_explorationStatus.find("World Size / Radius")!=std::string::npos);
 probe.m_entities[2].transform.scale={1,1,1};probe.m_entities[2].mesh_asset->coordinates.reset();
 CHECK(probe.planetary_scene_units()==1&&probe.planetary_scene_origin_offset()==std::array<double,3>{0,0,0});
 CHECK(publish_project_play_markers(context,probe.m_entities,&meshes));
 const auto legacy=previewgrid::detail::g_objectMarkers.at(context).markers;
 for(std::size_t i=0;i<3;++i){const auto& original=probe.m_entities[i+2];
  CHECK(std::bit_cast<std::array<std::uint32_t,3>>(legacy[i].position)==std::bit_cast<std::array<std::uint32_t,3>>(original.transform.position));
  CHECK(std::bit_cast<std::array<std::uint32_t,3>>(legacy[i].scale)==std::bit_cast<std::array<std::uint32_t,3>>(original.transform.scale));
  CHECK(std::bit_cast<std::array<std::uint32_t,3>>(legacy[i].rotationDegrees)==std::bit_cast<std::array<std::uint32_t,3>>(original.transform.rotation));
 }
 const auto retained=previewgrid::object_marker_geometry_for(context);
 auto invalidEntities=probe.m_entities;invalidEntities[4].transform.position[0]=std::numeric_limits<float>::quiet_NaN();
 CHECK(!publish_project_play_markers(context,invalidEntities,&meshes)&&previewgrid::object_marker_geometry_for(context)==retained);
 const auto legacyPoint=RuntimeProbe::planetary_preview_vector({4,5,6},*probe.planetary_entity(),true);
 const auto oldPoint=previewgrid::add(previewgrid::rotate_euler_degrees({4,5,6},
  {probe.planetary_entity()->transform.rotation[0],probe.planetary_entity()->transform.rotation[1],probe.planetary_entity()->transform.rotation[2]}),{100,200,300});
 CHECK(legacyPoint&&std::bit_cast<std::array<std::uint32_t,3>>(*legacyPoint)==std::bit_cast<std::array<std::uint32_t,3>>(oldPoint));
 previewgrid::cleanup_context(context);previewgrid::cleanup_context(focusContext);previewgrid::cleanup_context(legacyFocusContext);
 std::ofstream measured(output/"units-result.json");measured<<"{\"radius_meters\":25000,\"pad_meters\":14,\"cube_meters\":1,\"movement_meters\":10,\"native\":false,\"ground_precision_proven\":false}\n";
 CHECK(measured.good());return 0;
}
'''


class PlanetarySceneUnits(unittest.TestCase):
    def test_actual_editor_runtime_metric_projection(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC module consumer")
        paths = {name: ROOT / path for name, path in {
            "play": "Engine/src/epoch/implementation/epoch.project_play_scene.inl",
            "legacy": "Engine/src/epoch/epoch.engine_legacy.cpp",
            "editor": "Engine/src/editor/application/editor.application_scene_preview.inl",
            "package": "Engine/src/editor/application/editor.application_packages_creation.inl",
            "import": "Engine/src/editor/application/editor.application_asset_browsing.inl"}.items()}
        source = {name: path.read_text(encoding="utf-8-sig") for name, path in paths.items()}
        publication = "\n".join(owner(source["legacy"], marker) for marker in (
            "        [[nodiscard]] epochengine::previewgrid::Vec3 runtime_marker_color_for_entity(",
            "        [[nodiscard]] float runtime_marker_radius_for_entity(",
            "        [[nodiscard]] epochengine::previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(",
            "        [[nodiscard]] bool runtime_entity_is_forest_preview(",
            "        [[nodiscard]] bool runtime_forest_projection_admitted(",
            "        [[nodiscard]] std::optional<epochengine::previewgrid::Vec3> project_play_position_for_origin(",
            "        [[nodiscard]] bool project_play_entity_has_geometry(",
            "        [[nodiscard]] std::optional<epochengine::previewgrid::ObjectMarker> project_play_marker_for_entity(",
            "        [[nodiscard]] std::optional<std::array<double,2>> project_play_marker_depth_bounds(",
            "        [[nodiscard]] float project_play_adaptive_near_plane("))
        publication += "\n" + owner(source["play"], "            bool publish_project_play_markers(")
        publication += "\n" + owner(source["play"],
            "            [[nodiscard]] static std::optional<epochengine::lighting::LightingFrame> project_play_lighting_for_origin(").replace("static std::optional", "std::optional", 1)
        publication += "\n" + source["legacy"][source["legacy"].index("        enum class ProjectSunlightUpdate"):].splitlines()[0] + "\n"
        publication += owner(source["legacy"], "        [[nodiscard]] ProjectSunlightUpdate update_project_play_sunlight(")
        methods = "\n".join(owner(source["play"], marker) for marker in (
            "            [[nodiscard]] const ProjectRuntimeEntity* planetary_entity(",
            "            void update_planetary_sunlight(",
            "            [[nodiscard]] static std::optional<previewgrid::Vec3> planetary_preview_vector(",
            "            [[nodiscard]] double planetary_scene_units(",
            "            [[nodiscard]] std::array<double,3> planetary_scene_origin_offset(",
            "            [[nodiscard]] std::vector<previewgrid::ObjectMarker> planetary_exploration_markers(",
            "            [[nodiscard]] bool append_planetary_cloud_markers("))
        suppression = source["play"].split("}, &m_meshes, m_meshFrameLimits, planetaryMarkers,",1)[1].split(", requestedWireframe",1)[0].strip()
        start = source["play"].index("                if(const auto* entity=planetary_entity();entity&&entity->mesh_asset&&entity->mesh_asset->coordinates)")
        end = source["play"].index("                std::error_code error;", start)
        scale_guard = source["play"][start:end]
        start = source["editor"].index("                const bool selected = !forestPreview")
        end = source["editor"].index("\n            }", start)
        marker = source["editor"][start:end]
        start = source["editor"].index("                const auto planetScale = scene::mesh_render_scale(")
        end = source["editor"].index("\n            }", start)
        focus = source["editor"][start:end]
        focus_target = owner(source["editor"], "        [[nodiscard]] bool frame_editor_scene_target(")
        start = source["package"].index("                const auto eye=previewgrid::camera_world_eye(camera);")
        end = source["package"].index("                view.viewport_height", start)
        inverse = source["package"][start:end]
        start = source["play"].index("                    const auto eye=previewgrid::camera_world_eye(camera);")
        end = source["play"].index("\n                }", start)
        runtime_inverse = source["play"][start:end].replace("return;", "return {};")
        imported = definition(source["import"], "                if (mesh_frame)")
        code = CODE.replace("RUNTIME_PUBLICATION", publication).replace("RUNTIME_METHODS", methods).replace("RUNTIME_SUPPRESSION", suppression)
        camera_start = source["play"].index("                const double sceneUnits=entity->mesh_asset&&entity->mesh_asset->coordinates")
        camera_end = source["play"].index("                if(m_explorationFrame.anchor!=result.frame.anchor)", camera_start)
        camera_policy = source["play"][camera_start:camera_end].replace("return;", "return {};")
        code = code.replace("CAMERA_POLICY", camera_policy)
        code = code.replace("SCALE_GUARD", scale_guard).replace("EDITOR_MARKER", marker)
        code = code.replace("EDITOR_FOCUS_TARGET", focus_target).replace("EDITOR_FOCUS", focus)
        code = code.replace("EDITOR_INVERSE", inverse).replace("RUNTIME_INVERSE", runtime_inverse)
        code = code.replace("IMPORT_COORDINATES", imported)
        extracted_hashes = {name: hashlib.sha256(value.encode()).hexdigest() for name, value in {
            "runtime_publication": publication, "runtime_methods": methods, "scale_guard": scale_guard,
            "editor_marker": marker, "editor_focus": focus, "editor_focus_target": focus_target, "editor_inverse": inverse,
            "runtime_inverse": runtime_inverse, "import_coordinates": imported,"camera_policy":camera_policy}.items()}
        self.assertIn("binding->size_reference_body!=settings.size_reference_body", source["play"])
        self.assertIn("binding->reference_radius_meters!=settings.reference_radius_meters", source["play"])
        self.assertIn(".size_reference_body=settings->size_reference_body", source["legacy"])
        implementation_paths = [ROOT / path for path in (
            "Engine/src/render/render.preview_camera.cpp",
            "Engine/src/assets/texture/asset.texture_artifact.cpp",
            "Engine/src/authoring/texture/authoring.texture_document.cpp",
            "Engine/src/authoring/texture/authoring.texture_residency.cpp")]
        implementation_hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in implementation_paths}
        identity = hashlib.sha256(code.encode() + json.dumps(implementation_hashes, sort_keys=True).encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/planet-scene-units-oct28" / identity
        report.mkdir(parents=True, exist_ok=True)
        (report / "consumer.cpp").write_text(code, encoding="utf-8")
        module_roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        module_map = {}
        for directory in module_roots:
            for path in directory.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    module_map[match.group(1)] = path
        pending = ["asset.mesh_artifact", "project.planetary", "project.planetary_runtime", "scene.document",
                   "scene.serializer", "scene.runtime", "render.preview_grid", "render.arcade", "context.type",
                   "authoring.texture_detail"]
        selected = {}
        while pending:
            name = pending.pop()
            if name in selected or name in ("std", "std.compat"):
                continue
            self.assertIn(name, module_map)
            selected[name] = module_map[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        modules = "\n".join('"' + path.as_posix() + '"' for path in selected.values())
        fixture = ROOT / "Artifacts/Tests" / f"psu{time.time_ns():x}"
        (report / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetSceneUnits LANGUAGES CXX)
add_executable(scene_units consumer.cpp {" ".join('"'+path.as_posix()+'"' for path in implementation_paths)})
target_compile_features(scene_units PRIVATE cxx_std_23)
target_compile_options(scene_units PRIVATE /W3 /WX /utf-8 /EHsc)
target_include_directories(scene_units PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(scene_units PRIVATE FILE_SET CXX_MODULES BASE_DIRS {" ".join('"'+path.as_posix()+'"' for path in module_roots)} FILES {modules})
enable_testing()
add_test(NAME actual_planet_scene_units COMMAND scene_units "{fixture.as_posix()}/data/$<CONFIG>")
set_tests_properties(actual_planet_scene_units PROPERTIES TIMEOUT 20)
''', encoding="utf-8")
        build = fixture / "build"
        commands = [["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1", "--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        runs = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
            log = report / f"lane-{index}.log"
            log.write_text(completed.stdout+completed.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": completed.returncode, "log": str(log)})
            (report / "result.json").write_text(json.dumps({"passed": completed.returncode == 0 and index == len(commands)-1,
                "scope": "Actual source-body/expression projection and real canonical scene/serializer/runtime/render modules. CPU fixture body positions, not original provider/GPU/ground-precision acceptance.",
                "extracted_owner_sha256": extracted_hashes,
                "compiled_implementation_sha256": implementation_hashes,
                "production_text_snapshot_sha256": {str(paths[name].relative_to(ROOT)): hashlib.sha256(value.encode()).hexdigest() for name,value in source.items()},
                "commands": runs}, indent=2), encoding="utf-8")
            self.assertEqual(completed.returncode, 0, completed.stdout[-7000:]+completed.stderr[-2000:])


if __name__ == "__main__":
    unittest.main()
