"""Real project opt-in payload, guarded publication and canonical script adapter."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import dependency_include

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "Engine/src/editor/implementation/editor.project_registry.inl"
SHELL = ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl"
SCENE = ROOT / "Engine/src/editor/editor.scene.cpp"


def fixture():
    registry, shell, scene = (p.read_text(encoding="utf-8") for p in (REGISTRY, SHELL, SCENE))
    core = (ROOT / "Engine/modules/editor/editor.core.ixx").read_text(encoding="utf-8")
    app = (ROOT / "Engine/modules/editor/editor.application.ixx").read_text(encoding="utf-8")
    arcade = (ROOT / "Engine/modules/render/render.arcade.ixx").read_text(encoding="utf-8")
    spec = definition(shell, "    struct ProjectShellSpec", True)
    private_marker = "EPOCH_" "PRIVATE_SOURCE_"
    spec = re.sub(r"// " + private_marker + r"BEGIN:.*?// " + private_marker + r"END:[^\n]*", "", spec, flags=re.S)
    types = "\n".join(definition(core, marker, True).replace("export ", "") for marker in (
        "    export enum class EditorProjectKind", "    export struct EditorProjectCreationResult"))
    types += "\n" + definition(app, "    struct EditorSceneSeedEntity", True)
    nodes = "\n".join(definition(arcade, marker, True) for marker in (
        "    struct ArcadeSceneNodeContract", "    inline constexpr ArcadeSceneNodeContract kCabinetBodySceneNode",
        "    inline constexpr ArcadeSceneNodeContract kScreenSceneNode"))
    paths = "\n".join(definition(registry, marker) for marker in (
        "    [[nodiscard]] static bool project_path_below(", "    [[nodiscard]] static fs::path candidate_project_path_lexical(",
        "    [[nodiscard]] static fs::path candidate_project_path(", "    [[nodiscard]] static fs::path resolve_project_root_path(",
        "    [[nodiscard]] static std::string read_text_file(", "    [[nodiscard]] static std::string json_escape(",
        "    [[nodiscard]] static std::optional<nlohmann::ordered_json>\n        parse_project_build_configuration_manifest("))
    generators = "\n".join(definition(registry, marker) for marker in (
        "    [[nodiscard]] EditorSceneSeedEntity arcade_scene_seed(", "    [[nodiscard]] std::vector<EditorSceneSeedEntity> engine_arcade_seed_entities(",
        "    [[nodiscard]] std::vector<EditorSceneSeedEntity> sandbox_seed_entities(", "    [[nodiscard]] std::vector<EditorSceneSeedEntity> software_seed_entities("))
    generators += "\n" + spec
    generators += "\n" + "\n".join(definition(shell, marker) for marker in (
        "    [[nodiscard]] static std::string make_project_world_scene_text(", "    [[nodiscard]] static std::string json_array_from_csv(",
        "    [[nodiscard]] static std::string make_engine_arcade_script_text(", "    [[nodiscard]] static std::string make_engine_arcade_package_manifest_text("))
    generators += "\n" + definition(shell, "    [[nodiscard]] static const EditorProjectProfile* find_project_profile_by_root(")
    generators += "\n" + definition(registry, "    [[nodiscard]] static std::string project_script_source_path(")
    readers = definition(scene, "    [[nodiscard]] static std::optional<std::string> read_project_build_configuration_manifest(")
    stage = definition(scene, "    EditorProjectCreationResult editor_stage_engine_arcade_project(")
    return r'''
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include "updater.distribution.hpp"
import package.registry;
import scene.snapshot;
import scene.serializer;
import platform.filesystem;
namespace fs=std::filesystem;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Arcade opt-in contract failed line %d\n",line);std::exit(87);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
namespace epochengine{
TYPES
namespace render_arcade{NODES}
// Admitted profile rows and unrelated dedicated-application dispatch are ports;
// real project identity/edition/JSON/filesystem/asset/script owners run below.
struct EditorProjectProfile{std::string id,root_path;};std::vector<EditorProjectProfile> admittedProfiles;
struct ApplicationPort{int kind{};};struct ApplicationScenePort{std::vector<EditorSceneSeedEntity>entities;};
const ApplicationPort* editor_application_for_project(std::string_view){return nullptr;}
enum class EditorApplicationKind{Standard};
ApplicationScenePort make_editor_application_scene(int){CHECK(false);return {};}
ApplicationScenePort make_editor_application_scene(EditorApplicationKind){CHECK(false);return {};}
namespace core::path{fs::path privateRoot;fs::path candidate_data_root(){return privateRoot;}}
}
using namespace epochengine;
fs::path resolve_epoch_repo_root(const fs::path&){CHECK(false);return {};}
PATHS
const std::vector<EditorProjectProfile>& live_project_profiles(){return admittedProfiles;}
GENERATORS
namespace epochengine{READERS
STAGE}
void put(const fs::path& p,std::string_view s){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<s;CHECK(out.good());}
std::string manifest(std::string_view id){return std::string{"{\"id\":\""}+std::string{id}+"\",\"engine_edition\":\""+std::string{updater::distribution::edition_id}+"\",\"scene\":\"Assets/Scenes/Main.epoch\",\"default_script\":\"my_bootstrap\",\"custom\":{\"keep\":[1,2,3]}}\n";}
void setup(const fs::path& p){put(p/"project.epoch.json",manifest(p.filename().string()));put(p/"Assets/Scenes/Main.epoch","authored world bytes\n");put(p/"scripts/my_bootstrap.ascript.cpp","custom bootstrap\n");admittedProfiles.push_back({p.filename().string(),p.generic_string()});}
int main(int argc,char**argv){
 CHECK(argc==2);const fs::path base=argv[1];fs::create_directories(base);
 const fs::path root=base/"Selected";setup(root);const auto mainBefore=read_text_file(root/"Assets/Scenes/Main.epoch");const auto bootstrapBefore=read_text_file(root/"scripts/my_bootstrap.ascript.cpp");
 auto result=editor_stage_engine_arcade_project(root.string());if(!result.succeeded)std::fprintf(stderr,"stage: %s\n",result.summary.c_str());CHECK(result.succeeded);
 const auto selected=nlohmann::ordered_json::parse(read_text_file(root/"project.epoch.json"));
 CHECK(selected["engine_asset_packages"]==nlohmann::ordered_json::array({"engine_arcade"}));
 CHECK(selected["scene"]=="Assets/Scenes/Main.epoch"&&selected["default_script"]=="my_bootstrap"&&selected["custom"]["keep"].size()==3);
 CHECK(read_text_file(root/"Assets/Scenes/Main.epoch")==mainBefore&&read_text_file(root/"scripts/my_bootstrap.ascript.cpp")==bootstrapBefore);
 const auto package=nlohmann::ordered_json::parse(read_text_file(root/"Assets/packages/engine_arcade.package.json"));
 CHECK(package["package_id"]=="engine_arcade"&&package["scene_asset"]=="Assets/Scenes/EngineArcade.epoch"&&package["scenes"].size()==11);
 const auto world=scene::parse_snapshot_text(read_text_file(root/"Assets/Scenes/EngineArcade.epoch"));
 CHECK(world.ok&&world.snapshot.project_id=="Selected"&&world.snapshot.scene_id=="project:Selected:engine_arcade"&&world.snapshot.packages==std::vector<std::string>{"engine_arcade"}&&world.snapshot.objects.size()==7);
 CHECK(std::count_if(world.snapshot.objects.begin(),world.snapshot.objects.end(),[](const auto& x){return x.category=="EngineArcade";})==2);
 const auto script=project_script_source_path("engine_arcade_scene",root.string());CHECK(fs::is_regular_file(script)&&script==result.default_script_path);
 CHECK(read_text_file(script).find("request_engine_scene")!=std::string::npos&&!fs::exists(root/"scripts/script.engine_arcade_scene.cpp"));
 const std::array tracked{root/"project.epoch.json",root/"Assets/packages/engine_arcade.package.json",fs::path{script},root/"Assets/Scenes/EngineArcade.epoch"};std::vector<std::string> before;for(const auto& path:tracked)before.push_back(read_text_file(path));
 CHECK(editor_stage_engine_arcade_project(root.string()).succeeded);for(std::size_t i=0;i<tracked.size();++i)CHECK(read_text_file(tracked[i])==before[i]);
 const fs::path conflict=base/"Conflict";setup(conflict);put(conflict/"scripts/engine_arcade_scene.ascript.cpp","operator edit\n");const auto conflictBefore=read_text_file(conflict/"project.epoch.json");
 CHECK(!editor_stage_engine_arcade_project(conflict.string()).succeeded&&read_text_file(conflict/"scripts/engine_arcade_scene.ascript.cpp")=="operator edit\n"&&read_text_file(conflict/"project.epoch.json")==conflictBefore&&!fs::exists(conflict/"Assets/packages/engine_arcade.package.json"));
 const fs::path wrong=base/"Wrong";setup(wrong);auto foreign=nlohmann::ordered_json::parse(manifest("Wrong"));foreign["engine_edition"]="other-edition";put(wrong/"project.epoch.json",foreign.dump());
 CHECK(!editor_stage_engine_arcade_project(wrong.string()).succeeded&&!fs::exists(wrong/"Assets/packages"));
 const fs::path malformed=base/"Malformed";setup(malformed);put(malformed/"project.epoch.json","{\"id\":\"Malformed\",\"id\":\"Duplicate\"}");CHECK(!editor_stage_engine_arcade_project(malformed.string()).succeeded&&!fs::exists(malformed/"Assets/packages"));
 const fs::path linked=base/"Linked";setup(linked);fs::create_hard_link(linked/"project.epoch.json",base/"manifest-alias.json");CHECK(!editor_stage_engine_arcade_project(linked.string()).succeeded&&!fs::exists(linked/"Assets/packages"));
 core::path::privateRoot=base/"candidate";fs::create_directories(core::path::privateRoot/"Projects");CHECK(!editor_stage_engine_arcade_project(root.string()).succeeded);core::path::privateRoot.clear();
 CHECK(!editor_stage_engine_arcade_project((base/"NotAdmitted").string()).succeeded);
}
'''.replace("TYPES", types).replace("NODES", nodes).replace("PATHS", paths).replace("GENERATORS", generators).replace("READERS", readers).replace("STAGE", stage)


class EngineArcadeProject(unittest.TestCase):
    def test_actual_project_stage_and_script_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 CPU consumer")
        proof = ROOT / "Artifacts/Intermediate/test-fixtures/engine-arcade-project-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        fixture_root = proof / "fixture" / str(time.time_ns())
        (proof / "arcade.project_contract.cpp").write_text(fixture(), encoding="utf-8")
        modules = [engine_module_source(ROOT / "Engine/modules", name) for name in ("package.registry.ixx", "scene.snapshot.ixx", "scene.serializer.ixx", "platform.filesystem.ixx")]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EngineArcadeProject LANGUAGES CXX)
add_executable(arcade_project arcade.project_contract.cpp)
target_compile_features(arcade_project PRIVATE cxx_std_23)
target_compile_options(arcade_project PRIVATE /W3 /WX /utf-8)
target_include_directories(arcade_project PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{dependency_include(ROOT).as_posix()}")
target_sources(arcade_project PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_arcade_project COMMAND arcade_project "{fixture_root.as_posix()}/$<CONFIG>")
set_tests_properties(actual_arcade_project PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(proof / "msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(proof / "msvc"), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(proof / "msvc"), "-C", config, "--output-on-failure"]]
        results = []
        for i, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            (proof / f"lane-{i}.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        # Compile and execute the exact project-local staged adapter with the
        # actual C host API. Running it requires an explicit caller; staging made
        # no callback. There is no Engine, model, renderer or listener here.
        runner = proof / "script"
        runner.mkdir(exist_ok=True)
        (runner / "script.contract.cpp").write_text(r'''
#include <cstring>
#include <cstdlib>
#include "scripting.epoch_api.h"
extern "C" void run_script(EpochScriptHost*);
unsigned calls{};bool selectedSnake{};int select(void*,const char* id){++calls;selectedSnake=std::strcmp(id,"snake")==0;return selectedSnake?1:-1;}
int main(){EpochScriptHost host{};host.request_engine_scene=select;if(calls) return 1;run_script(&host);return calls==1&&selectedSnake?0:2;}
''', encoding="utf-8")
        (runner / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ArcadeScript LANGUAGES CXX)
add_executable(arcade_script script.contract.cpp "{(fixture_root/'Release/Selected/scripts/engine_arcade_scene.ascript.cpp').as_posix()}")
target_compile_features(arcade_script PRIVATE cxx_std_23)
target_include_directories(arcade_script PRIVATE "{(ROOT/'Engine/include').as_posix()}")
enable_testing()
add_test(NAME explicit_arcade_script COMMAND arcade_script)
''', encoding="utf-8")
        script_commands = [["cmake", "-S", str(runner), "-B", str(runner / "msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            script_commands += [["cmake", "--build", str(runner / "msvc"), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(runner / "msvc"), "-C", config, "--output-on-failure"]]
        for i, command in enumerate(script_commands):
            completed = subprocess.run(command, cwd=runner, capture_output=True, text=True, timeout=150)
            (runner / f"lane-{i}.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({"passed": True, "nativeAcceptance": False,
            "scope": "Actual staging API/generators/JSON/path guards/filesystem publication and exact generated script through actual C host API; admitted-profile and unrelated app dispatch ports", "fixtureRoot": str(fixture_root),
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (REGISTRY, SHELL, SCENE)}, "commands": results,
            "cases": ["real package/script/separate world payload", "selected project canonical script resolution", "main world/bootstrap/custom fields preserved", "idempotence", "edited payload conflict preserved", "wrong edition/duplicate JSON/hardlink/private-root rejection", "explicit-only host scene request"],
        }, indent=2) + "\n", encoding="utf-8")
        report = ROOT / "Artifacts/Reports/engine-arcade-project-oct27"
        report.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(proof / "result.json", report / "result.json")
        for path in proof.glob("lane-*.log"):
            shutil.copyfile(path, report / path.name)
        for path in runner.glob("lane-*.log"):
            shutil.copyfile(path, report / ("script-" + path.name))


if __name__ == "__main__":
    unittest.main()
