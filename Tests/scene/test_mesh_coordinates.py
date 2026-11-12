"""Actual scene history, codec, persistence and runtime preserve imported mesh units."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]

CODE = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
import scene.snapshot;
import scene.serializer;
import scene.document;
import scene.persistence;
import scene.runtime;
namespace scene = epochengine::scene;
namespace document = epochengine::authoring::scene;
namespace runtime = epochengine::scene_runtime;
namespace fs = std::filesystem;
void check(bool value, int line) {
    if (!value) { std::fprintf(stderr, "Mesh coordinate contract failed line %d\n", line); std::exit(91); }
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)

scene::SceneSnapshot legacy_source() {
    scene::SceneSnapshot source;
    source.scene_id = "metric-scene";
    source.project_id = "metric-project";
    source.world_name = "Coordinate Contract";
    source.primary_camera = 1;
    source.primary_spawn = 2;
    source.objects.push_back({.id=1, .name="Camera", .type="Camera"});
    source.objects.push_back({.id=2, .name="Spawn", .type="Spawn"});
    source.objects.push_back({.id=3, .name="Mesh", .type="StaticMesh", .category="World",
        .position={1,2,3}, .rotation={4,5,6}, .scale={0.125f,2,3},
        .mesh_asset=scene::SceneMeshSnapshot{"Assets/Mesh/unit.epoch_mesh", {1,2,3,4}, 3, 3}});
    return source;
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, {}};
}
std::string replace_once(std::string value, std::string_view from, std::string_view to) {
    const auto position = value.find(from); CHECK(position != std::string::npos);
    value.replace(position, from.size(), to); return value;
}
void legacy_contract() {
    const std::string golden4 =
        "epoch_snapshot 4\nscene \"metric-scene\"\nproject \"metric-project\"\nworld \"Coordinate Contract\"\n"
        "document_kind \"game\"\nsupport_tier \"baseline\"\nrevision 1\nprimary_camera 1\nprimary_spawn 2\n"
        "frame 0\ntime 0\npackages 0\nobjects 3\n"
        "object 1 \"Camera\" type \"Camera\" category \"\" pos 0,0,0 rot 0,0,0 scale 1,1,1 visible 1 editor_only 0 material 0 mesh 0\n"
        "object 2 \"Spawn\" type \"Spawn\" category \"\" pos 0,0,0 rot 0,0,0 scale 1,1,1 visible 1 editor_only 0 material 0 mesh 0\n"
        "object 3 \"Mesh\" type \"StaticMesh\" category \"World\" pos 1,2,3 rot 4,5,6 scale 0.125,2,3 visible 1 editor_only 0 material 0 mesh 1 path \"Assets/Mesh/unit.epoch_mesh\" key 1,2,3,4 vertices 3 indices 3\n"
        "timeline_keys 0\n";
    const auto golden5 = replace_once(replace_once(golden4, "epoch_snapshot 4", "epoch_snapshot 5"),
        "time 0\n", "time 0\nambient 1.25,0.5,0.125\n");
    // Captured from the unchanged production owners before coordinate support.
    const std::array<std::array<std::uint64_t,4>,2> oldHashes{{
        {12679151576735748582ull,10689055438674944615ull,5991253767599486136ull,14570484016080350904ull},
        {6134497873920357683ull,12642560979734270969ull,11316572794225628321ull,11593885405512694164ull}}};
    for (unsigned lane=0; lane<2; ++lane) {
        auto source = legacy_source();
        if (lane) source.ambient = {1.25f,0.5f,0.125f};
        const auto text = scene::serialize_snapshot_text(source);
        CHECK(text == (lane ? golden5 : golden4));
        const auto parsed = scene::parse_snapshot_text(text);
        CHECK(parsed && !parsed.snapshot.objects.back().mesh_asset->coordinates);
        CHECK(scene::serialize_snapshot_text(parsed.snapshot) == text);
        const auto built = document::SceneDocument::from_snapshot(source);
        CHECK(built && built.value.revision().content.words == oldHashes[lane]);
        CHECK(scene::mesh_render_scale(source.objects.back().scale,
            source.objects.back().mesh_asset) == source.objects.back().scale);
    }
}
void scale_contract() {
    const scene::Vec3 signedScale{-2.0f,0.0f,-0.0f};
    const auto ordinary = scene::mesh_render_scale(signedScale, std::nullopt);
    CHECK(ordinary && (*ordinary)[0] == -2.0f && (*ordinary)[1] == 0.0f && std::signbit((*ordinary)[2]));
    auto mesh = *legacy_source().objects.back().mesh_asset;
    CHECK(scene::mesh_render_scale(signedScale, mesh) == signedScale);
    mesh.coordinates = scene::SceneMeshCoordinates{637100.0, {1000.25,-2000.5,3000.75}};
    const auto planet = scene::mesh_render_scale({1,1,1}, mesh);
    CHECK(planet && *planet == scene::Vec3{637100,637100,637100});
    CHECK(double((*planet)[0])*10.0 == 6371000.0); // normalized radius10 -> physical Earth radius
    CHECK(scene::mesh_render_scale({1,1,1}, std::nullopt) == scene::Vec3{1,1,1}); // ordinary1m object
    CHECK(scene::mesh_render_scale({2,0.5f,-1}, mesh) == scene::Vec3{1274200,318550,-637100});
    CHECK(scene::mesh_render_scale(signedScale, mesh)->at(1) == 0.0f);
    CHECK(std::signbit(scene::mesh_render_scale(signedScale, mesh)->at(2)));
    CHECK(!scene::mesh_render_scale({std::numeric_limits<float>::infinity(),1,1}, std::nullopt));
    CHECK(!scene::mesh_render_scale({std::numeric_limits<float>::quiet_NaN(),1,1}, std::nullopt));
    CHECK(!scene::mesh_render_scale({(std::numeric_limits<float>::max)(),1,1}, mesh));
    mesh.coordinates->meters_per_unit = (std::numeric_limits<double>::denorm_min)();
    CHECK(mesh.coordinates->valid() && !scene::mesh_render_scale({1,1,1}, mesh));
    CHECK(!scene::mesh_render_scale({(std::numeric_limits<float>::denorm_min)(),0,0}, mesh));
    mesh.coordinates->meters_per_unit = 0;
    CHECK(!scene::mesh_render_scale({1,1,1}, mesh));
}
int main(int argc, char** argv) {
    CHECK(argc == 2); legacy_contract(); scale_contract();
    const fs::path root = argv[1]; fs::create_directories(root);
    auto seed = legacy_source(); seed.objects.pop_back();
    const auto canonical = document::SceneDocument::from_snapshot(seed); CHECK(canonical);
    const auto initial = canonical.value.project_snapshot(); CHECK(initial);
    auto built = document::SceneDocument::from_snapshot(initial.snapshot); CHECK(built);
    auto& doc = built.value;
    const auto before = doc.revision();
    const auto authored = legacy_source().objects.back();
    auto metricObject = authored;
    metricObject.mesh_asset->coordinates = scene::SceneMeshCoordinates{
        637100.123456789, {1234.25,-2345.5,3456.75}};
    const auto descriptor = document::descriptor_from_snapshot_object(metricObject, 2, doc.limits());
    CHECK(descriptor && descriptor->mesh_asset == metricObject.mesh_asset);
    const std::array<document::SceneOperation,1> create{
        document::ObjectCreatedOperation{.object=*descriptor}};
    CHECK(doc.apply_transaction(create, "Add metric mesh"));
    const auto committedHash = doc.revision().content;
    CHECK(committedHash != before.content && doc.objects().size() == 3);
    CHECK(doc.undo() && doc.objects().size() == 2 && doc.revision().content == before.content);
    CHECK(doc.redo() && doc.objects().size() == 3 && doc.revision().content == committedHash);
    const auto projected = doc.project_snapshot(); CHECK(projected);
    const auto* projectedMesh = scene::find_object_by_id(projected.snapshot, 3); CHECK(projectedMesh);
    CHECK(projectedMesh->mesh_asset == metricObject.mesh_asset);
    CHECK(projectedMesh->position == authored.position && projectedMesh->rotation == authored.rotation
        && projectedMesh->scale == authored.scale); // imported origin never changes authored transforms
    const auto text = scene::serialize_snapshot_text(projected.snapshot);
    CHECK(text.starts_with("epoch_snapshot 6\n") && text.find("ambient ") != std::string::npos);
    const auto parsed = scene::parse_snapshot_text(text); CHECK(parsed);
    CHECK(scene::find_object_by_id(parsed.snapshot,3)->mesh_asset == metricObject.mesh_asset);
    CHECK(scene::serialize_snapshot_text(parsed.snapshot) == text);
    auto reopened = document::SceneDocument::from_snapshot(parsed.snapshot);
    CHECK(reopened && reopened.value.revision().content == committedHash);
    const auto sceneFile = root / "metric.epoch";
    CHECK(scene::persistence::save_scene_snapshot_atomic(sceneFile, projected.snapshot));
    const auto loaded = scene::persistence::load_scene_snapshot(sceneFile); CHECK(loaded.result);
    CHECK(scene::persistence::scene_snapshot_semantically_equal(projected.snapshot, loaded.snapshot));
    runtime::SceneRuntime runtime;
    CHECK(runtime.replace(loaded.snapshot).committed());
    const auto* entity = runtime.projection()->find(3); CHECK(entity);
    CHECK(entity->mesh_asset == metricObject.mesh_asset && entity->transform.position == authored.position
        && entity->transform.rotation == authored.rotation && entity->transform.scale == authored.scale);
    const auto generation = runtime.projection()->generation();
    // Every coordinate field is canonical content, not an out-of-band transform ledger.
    for (unsigned field=0; field<5; ++field) {
        auto changed = loaded.snapshot; auto& coordinates = changed.objects.back().mesh_asset->coordinates;
        if (field == 0) coordinates.reset();
        else if (field == 1) coordinates->meters_per_unit += 0.25;
        else coordinates->source_origin_meters[field-2] += 0.25;
        const auto other = document::SceneDocument::from_snapshot(changed);
        CHECK(other && other.value.revision().content != committedHash);
        CHECK(!scene::persistence::scene_snapshot_semantically_equal(loaded.snapshot, changed));
    }
    auto mixed = loaded.snapshot;
    auto plain = authored; plain.id = 4; plain.name = "LegacyMesh"; mixed.objects.push_back(plain);
    const auto mixedText = scene::serialize_snapshot_text(mixed);
    CHECK(mixedText.find(" coordinates 0\n") != std::string::npos);
    const auto mixedRead = scene::parse_snapshot_text(mixedText);
    CHECK(mixedRead && !scene::find_object_by_id(mixedRead.snapshot,4)->mesh_asset->coordinates);
    CHECK(scene::find_object_by_id(mixedRead.snapshot,3)->mesh_asset == metricObject.mesh_asset);
    auto nondefaultAmbient = loaded.snapshot; nondefaultAmbient.ambient = {1.25f,0.5f,0.125f};
    const auto ambientRead = scene::parse_snapshot_text(scene::serialize_snapshot_text(nondefaultAmbient));
    CHECK(ambientRead && ambientRead.snapshot.ambient == nondefaultAmbient.ambient);
    CHECK(scene::find_object_by_id(ambientRead.snapshot,3)->mesh_asset == metricObject.mesh_asset);
    scene::SceneMeshCoordinates boundary{1.0e12, {1.0e15,-1.0e15,0}}; CHECK(boundary.valid());
    std::vector<scene::SceneMeshCoordinates> invalid;
    for (const double scale : {0.0,-1.0,1.0e12+1.0,std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()}) invalid.push_back({scale,{}});
    for (unsigned axis=0; axis<3; ++axis) for (const double value : {1.0e15+1.0,-1.0e15-1.0,
        std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        scene::SceneMeshCoordinates bad; bad.source_origin_meters[axis] = value; invalid.push_back(bad);
    }
    const auto savedBytes = bytes(sceneFile);
    for (const auto& coordinates : invalid) {
        CHECK(!coordinates.valid());
        auto bad = loaded.snapshot; bad.objects.back().mesh_asset->coordinates = coordinates;
        CHECK(!bad.objects.back().mesh_asset->valid() && !scene::validate_scene_document(bad));
        CHECK(scene::serialize_snapshot_text(bad).empty() && !document::SceneDocument::from_snapshot(bad));
        CHECK(!runtime.replace(bad).committed() && runtime.projection()->generation() == generation);
        CHECK(!scene::persistence::save_scene_snapshot_atomic(sceneFile,bad) && bytes(sceneFile) == savedBytes);
        auto badDescriptor = *descriptor; badDescriptor.id = {4}; badDescriptor.metadata.name = "Invalid";
        badDescriptor.mesh_asset->coordinates = coordinates;
        const std::array<document::SceneOperation,1> badCreate{
            document::ObjectCreatedOperation{.object=badDescriptor}};
        const auto priorRevision = doc.revision(); const auto priorHistory = doc.history().size();
        CHECK(!doc.apply_transaction(badCreate,"Invalid coordinate import"));
        CHECK(doc.revision() == priorRevision && doc.history().size() == priorHistory && doc.objects().size() == 3);
        CHECK(!scene::mesh_render_scale({1,1,1},badDescriptor.mesh_asset));
    }
    for (const std::string_view value : {"0","-1","nan","inf","1e999","1000000000001","1garbage"})
        CHECK(!scene::parse_snapshot_text(replace_once(text," meters_per_unit 637100.12345678895", " meters_per_unit " + std::string(value))));
    for (const std::string_view value : {"nan","inf","1e999","1000000000000001"})
        CHECK(!scene::parse_snapshot_text(replace_once(text,"source_origin_meters 1234.25", "source_origin_meters " + std::string(value))));
    CHECK(!scene::parse_snapshot_text(replace_once(text,"coordinates 1","coordinates 2")));
    CHECK(!scene::parse_snapshot_text(replace_once(text,"epoch_snapshot 6","epoch_snapshot 5")));
    CHECK(!scene::parse_snapshot_text(replace_once(text," coordinates 1","")));
    CHECK(!scene::parse_snapshot_text(replace_once(text,"3456.75\n","3456.75,4\n")));
    const auto coordinateStart = text.find(" coordinates 1"); CHECK(coordinateStart != std::string::npos);
    auto truncated = text; truncated.erase(coordinateStart, truncated.find('\n',coordinateStart)-coordinateStart);
    CHECK(!scene::parse_snapshot_text(truncated));
}
'''


class SceneMeshCoordinates(unittest.TestCase):
    def test_actual_history_save_reopen_runtime_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 CPU module consumer")
        proof = ROOT / "Artifacts/Reports/mesh-coordinates-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/mesh-coordinates"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "scene.mesh_coordinates_contract.cpp"
        source.write_text(CODE, encoding="utf-8")
        module_map = {}
        module_roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        for directory in module_roots:
            for path in directory.rglob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    module_map[match.group(1)] = path
        pending = ["scene.snapshot", "scene.serializer", "scene.document", "scene.persistence", "scene.runtime"]
        selected = {}
        while pending:
            name = pending.pop()
            if name in selected or name in ("std", "std.compat"):
                continue
            self.assertIn(name, module_map)
            selected[name] = module_map[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        modules = " ".join('"' + p.as_posix() + '"' for p in selected.values())
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(SceneMeshCoordinates LANGUAGES CXX)
add_executable(scene_mesh_coordinates "{source.as_posix()}")
target_compile_features(scene_mesh_coordinates PRIVATE cxx_std_23)
target_compile_options(scene_mesh_coordinates PRIVATE /W3 /WX /utf-8)
target_include_directories(scene_mesh_coordinates PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(scene_mesh_coordinates PRIVATE FILE_SET CXX_MODULES BASE_DIRS {" ".join('"'+p.as_posix()+'"' for p in module_roots)} FILES {modules})
enable_testing()
add_test(NAME actual_mesh_coordinates COMMAND scene_mesh_coordinates "{(proof/'fixture').as_posix()}/$<CONFIG>")
set_tests_properties(actual_mesh_coordinates PROPERTIES TIMEOUT 20)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for configuration in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", configuration, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", configuration, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-8000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False,
            "scope": "Actual scene descriptor/transaction/history, serializer, persistence and runtime modules; no fake model or scene roundtrip",
            "modules": len(selected), "commands": results,
            "sourceIdentity": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in selected.values()},
            "cases": ["v4/v5 exact legacy text and pre-change content hashes", "descriptor-create-Undo-Redo",
                      "all coordinate fields participate in canonical hash/equality", "v6 default/edited ambient and mixed-coordinate scenes",
                      "save/load/reopened document/runtime preserve coordinates and authored transforms",
                      "malformed, nonfinite and out-of-range values fail atomically", "one shared render-scale helper preserves legacy/signed/zero semantics and rejects overflow/underflow"],
            "limits": "Metadata and shared units only; huge-world camera-relative and patch-local precision require the separate producer/renderer integration.",
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
