"""Actual reopen presentation owners with recorded admission phases; no native work."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets

ROOT = Path(__file__).resolve().parents[2]


class PlanetaryReopenStatus(unittest.TestCase):
    def test_pending_is_distinct_from_settled_refusal(self):
        base = ROOT / "Engine/src/editor/application"
        source = (base / "editor.application_asset_browsing.inl").read_text(encoding="utf-8")
        functions = "\n".join(definition(source, marker) for marker in (
            "        [[nodiscard]] std::optional<project_planetary::MeshGenerationIdentity>",
            "        [[nodiscard]] bool editor_planetary_stream_identity_matches(",
            "        [[nodiscard]] bool editor_planetary_outliner_only(",
            "        [[nodiscard]] std::string_view editor_planetary_restore_progress("))
        pump = (base / "editor.application_packages_creation.inl").read_text(encoding="utf-8")
        start = pump.index("                const auto progress=editor_planetary_restore_progress(editor);")
        assignment = pump[start:pump.index("                return;", start)]
        code = r'''
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace project_planetary {
 struct Coordinates {double meters_per_unit{1};std::array<double,3> source_origin_meters{};bool operator==(const Coordinates&)const=default;};
 struct MeshGenerationIdentity {std::array<unsigned long long,4> artifact_key{};unsigned vertex_count{},index_count{};Coordinates coordinates{};bool operator==(const MeshGenerationIdentity&)const=default;};
}
struct Mesh {std::array<unsigned long long,4> artifact_key{1,2,3,4};unsigned vertex_count{3},index_count{3};std::optional<project_planetary::Coordinates> coordinates;};
struct EditorEntity {std::string category{"Planetary Terrain"};bool visible{true};std::optional<Mesh> meshAsset{Mesh{}};std::shared_ptr<const int> meshSnapshot;bool meshLoadAttempted{};};
struct EditorState {
 std::string projectRoot,planetaryCheckedRoot,planetaryStreamStatus;
 bool planetaryAttachmentPending{};
 struct Attachment {std::string content_hash{"approved"};bool attached{true};bool source_attached()const{return attached;}} planetaryAttachment;
 struct Policy {std::string selected_root;struct Source {std::string content_hash{"approved"};} source;std::optional<project_planetary::MeshGenerationIdentity> generated_mesh;bool body_inventory_checked{};struct Settings {std::string mode{"planet"};} settings;};
 std::optional<Policy> planetaryMeshPolicy;
 std::vector<EditorEntity> entities{EditorEntity{}};
};
// Contract failures exit normally instead of opening a CRT assertion dialog.
#undef assert
#define assert(condition) do { if (!(condition)) return __LINE__; } while (false)
FUNCTIONS
void missing_identity_status(EditorState& editor){ASSIGNMENT}
int main(){
 EditorState editor;
 assert(editor_planetary_restore_progress(editor).empty());
 editor.projectRoot="saved-project";
 auto pending=editor_planetary_restore_progress(editor);assert(pending.starts_with("Validating"));
 missing_identity_status(editor);assert(editor.planetaryStreamStatus==pending&&pending.find("Generate")==pending.npos);
 editor.planetaryCheckedRoot=editor.projectRoot;editor.planetaryAttachmentPending=true;
 assert(editor_planetary_restore_progress(editor).starts_with("Validating"));
 editor.planetaryAttachmentPending=false;editor.planetaryAttachment.attached=false;
 assert(editor_planetary_restore_progress(editor).empty());missing_identity_status(editor);
 assert(editor.planetaryStreamStatus.find("Choose Build & Generate")!=std::string::npos);
 editor.planetaryAttachment.attached=true;
 editor.planetaryMeshPolicy=EditorState::Policy{editor.projectRoot};
 assert(editor_planetary_restore_progress(editor).empty()); // settled missing receipt is not pending
 editor.planetaryMeshPolicy->generated_mesh=editor_planetary_mesh_identity(editor.entities[0]);
 assert(editor_planetary_stream_identity_matches(editor,editor.entities[0]));
 assert(!editor_planetary_outliner_only(editor,editor.entities[0]));
 editor.planetaryMeshPolicy->settings.mode="universe";
 assert(editor_planetary_outliner_only(editor,editor.entities[0]));
 pending=editor_planetary_restore_progress(editor);assert(pending.find("Open World")!=pending.npos);
 assert(pending.find("Generate")==pending.npos);
 editor.entities[0].meshLoadAttempted=true;assert(editor_planetary_restore_progress(editor).empty()); // failed load is settled
 editor.entities[0].meshSnapshot=std::make_shared<const int>(1);
 assert(editor_planetary_restore_progress(editor).find("body inventory")!=pending.npos);
 editor.planetaryMeshPolicy->body_inventory_checked=true;assert(editor_planetary_restore_progress(editor).empty());
 editor.entities[0].meshSnapshot.reset();editor.entities[0].meshLoadAttempted=false;
 editor.planetaryMeshPolicy->source.content_hash="stale";assert(editor_planetary_restore_progress(editor).empty());
 assert(!editor_planetary_outliner_only(editor,editor.entities[0]));
 editor.planetaryMeshPolicy->source.content_hash="approved";editor.entities[0].meshAsset->index_count=6;
 assert(editor_planetary_restore_progress(editor).empty()); // foreign mesh never receives loading status
 assert(!editor_planetary_outliner_only(editor,editor.entities[0]));
 editor.entities[0].meshAsset->index_count=3;editor.planetaryMeshPolicy->selected_root="another-project";
 assert(editor_planetary_restore_progress(editor).empty());
}
'''.replace("FUNCTIONS", functions).replace("ASSIGNMENT", assignment)
        proof = ROOT / "Artifacts/Reports/planet-reopen-policy-oct28" / str(time.time_ns())
        proof.mkdir(parents=True)
        cpp = proof / "editor.reopen_status_contract.cpp"
        cpp.write_text(code, encoding="utf-8")
        phases = []
        for config in ("Debug", "Release"):
            binary = proof / ("contract-" + config)
            if os.name == "nt":
                kits = Path("C:/Program Files (x86)/Windows Kits/10")
                tool = msvc_2022_toolsets()[-1]
                kit = sorted((kits / "Include").glob("*"))[-1]
                binary = binary.with_suffix(".exe")
                command = [str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/utf-8", "/EHsc", "/W4", "/WX", "/Od" if config == "Debug" else "/O2", "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/Fo" + str(proof / (config + ".obj")), "/Fe" + str(binary), str(cpp), "/link", "/LIBPATH:" + str(tool / "lib/x64"), "/LIBPATH:" + str(kits / "Lib" / kit.name / "ucrt/x64"), "/LIBPATH:" + str(kits / "Lib" / kit.name / "um/x64")]
            else:
                compiler = shutil.which("clang++") or shutil.which("g++")
                if not compiler:
                    self.skipTest("C++23 compiler required")
                command = [compiler, "-std=c++23", "-O0" if config == "Debug" else "-O2", str(cpp), "-o", str(binary)]
            for label, args in ((config + "-build", command), (config + "-test", [str(binary)])):
                run = subprocess.run(args, cwd=proof, capture_output=True, text=True, timeout=45)
                (proof / (label + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
                phases.append({"phase": label, "exit": run.returncode})
                (proof / "result.json").write_text(json.dumps({"scope": "Actual presentation/identity predicates with admission-state recording ports; real saved receipt read proof is separate", "source_sha256": hashlib.sha256(source.encode()).hexdigest(), "phases": phases}, indent=2) + "\n", encoding="utf-8")
                self.assertEqual(run.returncode, 0, (run.stdout + run.stderr)[-6000:])


if __name__ == "__main__":
    unittest.main()
