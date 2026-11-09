"""Actual managed Run forest projection and ordinary publication; CPU only."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest
from Tests.packages.test_planetary_scene_units import CODE as UNITS_CODE, owner

ROOT = Path(__file__).resolve().parents[2]
CODE = UNITS_CODE[:UNITS_CODE.index("// The real Windows translation unit")].replace(
    "import render.arcade;", "import render.arcade;\nimport forest.preview_mesh;\nimport forest.factory;") + r'''
namespace forest=epochengine::forest;
RUNTIME_PUBLICATION
class RuntimeProbe {
public:
 std::vector<ProjectRuntimeEntity> m_entities;
 std::uint64_t m_planetaryEntity{3};
 bool m_explorationController{true};
 std::optional<project_planetary_runtime::PreviewFrame> m_explorationPreview;
 std::optional<previewgrid::Camera> m_explorationRenderCamera{previewgrid::Camera{}};
 std::array<double,3> m_explorationRenderOrigin{};
 std::optional<project_planetary::SourceGeneration> m_meshPlanetarySource{project_planetary::SourceGeneration{}};
 std::optional<project_planetary::Settings> m_meshPlanetarySettings{project_planetary::Settings{}};
 std::unordered_map<std::uint64_t,std::shared_ptr<const asset::mesh::Artifact>> m_meshes;
 std::unordered_map<std::uint64_t,std::shared_ptr<const previewgrid::MeshBaseColorBinding>> m_meshBaseColorTextures;
 RUNTIME_METHODS
};
int main(){
 const auto compiled=forest::compile_forest_asset(forest::make_default_plant_lab_document());CHECK(compiled.valid);
 const auto geometry=forest::preview_mesh::build(compiled.graph,compiled.sample);CHECK(geometry.valid);
 std::vector<asset::mesh::Vertex> vertices;
 const auto root=compiled.graph.nodes.front().position;
 for(const auto& v:geometry.vertices)vertices.push_back({{v.position.x-root.x,v.position.y-root.y,v.position.z-root.z},
  {v.normal.x,v.normal.y,v.normal.z},{v.uv.x,v.uv.y},{.3f,.6f,.2f}});
 const auto mesh=asset::mesh::build_artifact(vertices,geometry.indices);CHECK(mesh);
 CHECK(mesh.artifact->vertices().size()>100&&mesh.artifact->indices().size()>300);
 RuntimeProbe probe;probe.m_meshPlanetarySource->content_hash="sha256:"+std::string(64,'a');
 ProjectRuntimeEntity terrain;terrain.source_id=3;terrain.type="IndexedMesh";terrain.category="Planetary Terrain";
 terrain.flags=scene_runtime::RuntimeEntityFlag::visible;
 terrain.position_anchor=std::array<double,3>{1e12+.125,-1e12+.25,1e12+.5};
 terrain.transform.position={.125f,.25f,.5f};
 terrain.mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/planet.epoch_mesh",{1,2,3,4},3,3,
  scene::SceneMeshCoordinates{1,{2e10,3e10,4e10}}};
 ProjectRuntimeEntity tree;tree.source_id=7;tree.type="ForestTrunk";tree.category="ForestFactory";
 tree.flags=scene_runtime::RuntimeEntityFlag::visible;
 tree.mesh_asset=scene::SceneMeshSnapshot{"Assets/Forest/tree.epoch_mesh",mesh.artifact->key().words,
  static_cast<std::uint32_t>(vertices.size()),static_cast<std::uint32_t>(geometry.indices.size())};
 tree.position_anchor=std::array<double,3>{6371000.125,.25,.5};tree.transform.position={.03125f,.0625f,.125f};
 scene::SceneForestRegionInstance region;
 region.region_id=9;region.terrain_object_id=3;region.plant_genome=21;region.plant_revision=3;region.plant_hash=42;
 region.candidate_count=1;region.radius_meters=12;region.density_per_square_meter=.2;
 region.body_radius_meters=6371000;region.center_direction={0,1,0};region.body_local_root_meters={0,6371000.125,0};
 region.radial_up={0,1,0};region.terrain_artifact_key=terrain.mesh_asset->artifact_key;
 region.admitted_body_center_meters={1e10,2e10,3e10};region.mesh_source_origin_meters={2e10,3e10,4e10};
 region.provider_content_hash=probe.m_meshPlanetarySource->content_hash.substr(7);
 region.recipe=project_planetary::encode_settings(*probe.m_meshPlanetarySettings);
 region.plant_source="Assets/Forest/default.epoch_forest";CHECK(region.valid());tree.forest_region=region;
 probe.m_entities={terrain,tree};probe.m_meshes[7]=mesh.artifact;
 project_planetary_runtime::PreviewFrame frame;frame.body_count=1;frame.meters_to_preview=10.0/6371000;
 frame.bodies[0].id=0;frame.bodies[0].radius=10;frame.bodies[0].center={0,0,0};
 probe.m_explorationPreview=frame;
 const auto pivot=*scene::world_position(terrain.transform.position,terrain.position_anchor);
 probe.m_explorationRenderOrigin={pivot[0],pivot[1]+6371000,pivot[2]};
 const auto savedTree=probe.m_entities[1];const auto savedTerrain=probe.m_entities[0];
 const auto routed=[&]{std::vector<previewgrid::ObjectMarker> markers;std::vector<std::uint64_t> managed;
  const auto status=probe.append_planetary_forest_markers(markers,managed,true);
  CHECK(status.empty()&&markers.size()==1&&managed==std::vector<std::uint64_t>{7});return markers[0];};
 auto marker=routed();CHECK(marker.mesh==mesh.artifact&&marker.preciseTransform);
 CHECK(near(marker.preciseTransform->position[0],.03125,1e-6));
 CHECK(near(marker.preciseTransform->position[1],.1875,1e-6));
 CHECK(near(marker.preciseTransform->position[2],.125,1e-6));
 CHECK(marker.scale.x==1&&marker.scale.y==1&&marker.scale.z==1);
 // Body translation is applied in the current anchor's metric frame.
 probe.m_explorationPreview->bodies[0].center={100*frame.meters_to_preview,20*frame.meters_to_preview,-7*frame.meters_to_preview};
 marker=routed();CHECK(near(marker.preciseTransform->position[0],100.03125,1e-4));
 CHECK(near(marker.preciseTransform->position[1],20.1875,1e-4));
 CHECK(near(marker.preciseTransform->position[2],-6.875,1e-4));
 // Changing anchor shifts the body and camera origin by the same metres.
 probe.m_explorationPreview->meters_to_preview=10.0/1000;
 probe.m_explorationPreview->bodies[0].center={-900*.01,20*.01,-7*.01};
 probe.m_explorationRenderOrigin[0]-=1000;
 const auto anchored=routed();CHECK(anchored.preciseTransform->position==marker.preciseTransform->position);
 probe.m_entities[1].transform.position[0]+=.015625f;
 CHECK(near(routed().preciseTransform->position[0]-marker.preciseTransform->position[0],.015625,1e-6));
 probe.m_entities[1]=savedTree;
 // Parent rotation/nonuniform scale uses the existing double transform; its
 // exact instance anchor is included before the final float conversion.
 probe.m_entities[0].transform.rotation={17,31,59};probe.m_entities[0].transform.scale={2,.5f,3};
 const auto parent=previewgrid::make_precise_mesh_transform(pivot,{2,.5,3},{17,31,59});CHECK(parent);
 const std::array<double,3> point{-900,6371020.125,-7};
 const auto expected=parent->apply(point);probe.m_explorationRenderOrigin=expected;
 marker=routed();CHECK(near(marker.preciseTransform->position[0],.03125,1e-6));
 CHECK(near(marker.preciseTransform->position[1],.0625,1e-6));CHECK(near(marker.preciseTransform->position[2],.125,1e-6));
 const auto actualUp=previewgrid::rotate_euler_degrees({0,1,0},marker.rotationDegrees);
 CHECK(near(actualUp.x,parent->rotation[1],1e-5)&&near(actualUp.y,parent->rotation[4],1e-5)&&near(actualUp.z,parent->rotation[7],1e-5));
 CHECK(probe.m_entities[1].transform.position==savedTree.transform.position
  &&probe.m_entities[1].position_anchor==savedTree.position_anchor&&probe.m_entities[1].forest_region==savedTree.forest_region);
 // Publish through the production ordinary/additional filter: one instance,
 // unchanged real mesh, real budget refusal without duplicate draw geometry.
 const epochengine::core::Context context;const auto* ctx=&context;
 std::vector<previewgrid::ObjectMarker> markers;std::vector<std::uint64_t> managed;
 CHECK(probe.append_planetary_forest_markers(markers,managed,true).empty());
 CHECK(publish_project_play_markers(ctx,probe.m_entities,&probe.m_meshes,{},markers,3,false,{},nullptr,{},nullptr,nullptr,{},managed));
 auto published=previewgrid::detail::g_objectMarkers.at(ctx).markers;CHECK(published.size()==1&&published[0].mesh==mesh.artifact);
 previewgrid::MeshFrameLimits tiny;tiny.maximum_indices=3;tiny.maximum_bytes=64;
 const auto retained=previewgrid::object_marker_geometry_for(ctx);
 CHECK(!publish_project_play_markers(ctx,probe.m_entities,&probe.m_meshes,tiny,markers,3,false,{},nullptr,{},nullptr,nullptr,{},managed));
 CHECK(previewgrid::object_marker_geometry_for(ctx)==retained);
 const auto refused=[&]{markers.clear();managed.clear();const auto status=probe.append_planetary_forest_markers(markers,managed,true);
  CHECK(!status.empty()&&markers.empty()&&managed.empty());};
 for(unsigned failure=0;failure<9;++failure){probe.m_entities[0]=savedTerrain;probe.m_entities[1]=savedTree;
  if(failure==0)probe.m_entities[1].forest_region->provider_content_hash=std::string(64,'b');
  if(failure==1)probe.m_entities[1].forest_region->recipe+="stale";
  if(failure==2)++probe.m_entities[1].forest_region->terrain_artifact_key[0];
  if(failure==3)probe.m_entities[1].forest_region->terrain_object_id=90;
  if(failure==4)probe.m_entities[1].forest_region->body_index=5;
  if(failure==5)probe.m_entities[1].forest_region->candidate_count=65;
  if(failure==6)probe.m_entities[1].forest_region->body_local_root_meters[0]=std::numeric_limits<double>::infinity();
  if(failure==7)probe.m_entities[0].transform.scale[1]=0;
  if(failure==8)probe.m_entities[1].transform.position[1]=std::numeric_limits<float>::quiet_NaN();
  refused();
 }
 // A stale attachment remains an ordinary authored tree; static/non-controller
 // Run and single placement likewise use the original ordinary pose exactly.
 probe.m_entities[0]=savedTerrain;probe.m_entities[1]=savedTree;
 probe.m_entities[1].forest_region->recipe+="stale";refused();
 CHECK(publish_project_play_markers(ctx,probe.m_entities,&probe.m_meshes,{},markers,3));
 published=previewgrid::detail::g_objectMarkers.at(ctx).markers;CHECK(published.size()==1);
 const auto ordinary=project_play_marker_for_entity(probe.m_entities[1],mesh.artifact,{});CHECK(ordinary);
 CHECK(published[0].preciseTransform==ordinary->preciseTransform);
 probe.m_explorationController=false;markers.clear();managed.clear();
 CHECK(probe.append_planetary_forest_markers(markers,managed,true).empty()&&markers.empty()&&managed.empty());
 probe.m_explorationController=true;probe.m_entities[1].forest_region.reset();
 CHECK(probe.append_planetary_forest_markers(markers,managed,true).empty()&&markers.empty()&&managed.empty());
 previewgrid::cleanup_context(ctx);return 0;
}
'''


class ForestRuntimeFollow(unittest.TestCase):
    def test_actual_managed_follow_and_publication(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module lane")
        paths = [ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp",
                 ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"]
        legacy, play = [p.read_text(encoding="utf-8-sig") for p in paths]
        publication = "\n".join(owner(legacy, marker) for marker in (
            "        [[nodiscard]] epochengine::previewgrid::Vec3 runtime_marker_color_for_entity(",
            "        [[nodiscard]] float runtime_marker_radius_for_entity(",
            "        [[nodiscard]] epochengine::previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(",
            "        [[nodiscard]] bool runtime_entity_is_forest_preview(",
            "        [[nodiscard]] bool runtime_forest_projection_admitted(",
            "        [[nodiscard]] bool project_play_entity_has_geometry(",
            "        [[nodiscard]] std::optional<epochengine::previewgrid::ObjectMarker> project_play_marker_for_entity(",
            "        [[nodiscard]] std::optional<previewgrid::ObjectMarker> project_play_forest_marker_for_body(",
            "        bool publish_project_play_markers("))
        methods = "\n".join(owner(play, marker) for marker in (
            "            [[nodiscard]] const ProjectRuntimeEntity* planetary_entity(",
            "            [[nodiscard]] double planetary_scene_units(",
            "            [[nodiscard]] std::string append_planetary_forest_markers("))
        self.assertLess(play.index("bool completeManagedFrame="), play.index("m_forestRuntimeStatus=append_planetary_forest_markers"))
        self.assertNotIn("filesystem", methods)
        code = CODE.replace("RUNTIME_PUBLICATION", publication).replace("RUNTIME_METHODS", methods)
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        proof = ROOT / "Artifacts/Reports/forest-runtime-follow-nov01" / identity
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        found = {}
        for folder in roots:
            for path in folder.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    found[match.group(1)] = path
        selected = {}
        pending = re.findall(r"^import\s+([\w.]+);", code, re.M) + ["authoring.texture_detail"]
        impl = [ROOT / p for p in ("Engine/src/render/render.preview_camera.cpp","Engine/src/assets/texture/asset.texture_artifact.cpp",
            "Engine/src/authoring/texture/authoring.texture_document.cpp",
            "Engine/src/authoring/texture/authoring.texture_residency.cpp", "Engine/src/forest/forest.preview_mesh.cpp",
            "Engine/src/assets/texture/asset.texture_import.cpp", "Engine/src/assets/image.stb.cpp",
            "Engine/src/platform/core.path.cpp", "Engine/dep/EpochPlatformEngine/src/core/core.log.cpp",
            "Engine/dep/EpochPlatformEngine/src/core/core.time.cpp")]
        for path in impl:
            pending += re.findall(r"^(?:export )?(?:import|module)\s+([\w.]+);", path.read_text(encoding="utf-8-sig"), re.M)
        while pending:
            name = pending.pop()
            if name in selected or name in ("std", "std.compat"):
                continue
            self.assertIn(name, found)
            selected[name] = found[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        quoted = lambda values: " ".join('"' + p.as_posix() + '"' for p in values)
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ForestRuntimeFollow LANGUAGES CXX)
add_executable(forest_follow consumer.cpp {quoted(impl)})
target_compile_features(forest_follow PRIVATE cxx_std_23)
target_compile_options(forest_follow PRIVATE /W3 /WX /utf-8 /EHsc)
target_include_directories(forest_follow PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_link_libraries(forest_follow PRIVATE shell32)
target_sources(forest_follow PRIVATE FILE_SET CXX_MODULES BASE_DIRS {quoted(roots)} FILES {quoted(selected.values())})
enable_testing()
add_test(NAME actual_forest_runtime_follow COMMAND forest_follow)
set_tests_properties(actual_forest_runtime_follow PROPERTIES TIMEOUT 20)
''', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/tests/forest-runtime-follow" / identity
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1", "--", "/nr:false"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        runs = []
        for index, command in enumerate(commands):
            done = subprocess.run(command, cwd=ROOT, env=dict(os.environ,EPOCH_ASSET_ROOT=str(ROOT/"Engine/resource")), capture_output=True, text=True, timeout=180)
            (proof / f"lane-{index}.log").write_text(done.stdout + done.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": done.returncode})
            (proof / "result.json").write_text(json.dumps({"passed": done.returncode == 0 and index == len(commands)-1,
                "scope": "Actual managed forest helper/runtime route/ordinary publisher with real compiled Plant Lab mesh. CPU only; no native renderer, provider or GUI acceptance.",
                "commands": runs, "sources": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths + list(selected.values()) + impl},
                "extracted_owner_sha256": {"publication": hashlib.sha256(publication.encode()).hexdigest(),
                    "methods": hashlib.sha256(methods.encode()).hexdigest()}}, indent=2), encoding="utf-8")
            self.assertEqual(done.returncode, 0, done.stdout[-9000:] + done.stderr[-2000:])


if __name__ == "__main__":
    unittest.main()
