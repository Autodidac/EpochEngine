"""Real immutable source discovery and asynchronous Editor review admission; CPU only."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import json
import os
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl"
STATE = ROOT / "Engine/src/editor/application/editor.application_state.inl"
PANEL = ROOT / "Engine/src/editor/application/editor.frame_packages_about.inl"


def consumer():
    source = OWNER.read_text(encoding="utf-8")
    state = STATE.read_text(encoding="utf-8")
    task = definition(state, "        struct EditorPackageSourceTask final", True)
    # Remove the default argument's braces only for lexical extraction; the
    # complete production function body and caller-supplied identity remain.
    functions = "\n".join(definition(source.replace("std::string expected_package_id = {}", "std::string expected_package_id"), marker) for marker in (
        "        [[nodiscard]] bool package_source_task_current(",
        "        void poll_package_source_task(",
        "        void open_package_source_review(",
        "        void start_package_source_inspection("))
    panel = PANEL.read_text(encoding="utf-8")
    actions = definition(panel, "                const std::array sourceActions", True)
    guard = panel[panel.index("                const bool canAttach ="):
                  panel.index("                const std::array sourceActions", panel.index("                const bool canAttach ="))]
    for marker in ("        void start_planetary_project_attachment(",
                   "        void create_planetary_example_project(",
                   "        void approve_and_stage_package_source("):
        body = definition(source, marker)
        if "!editor.packageSourceReviewCurrent" not in body[:body.index(" return;")+8]:
            raise AssertionError(marker + " must reject an obsolete review before doing work")
    installer=(ROOT/'Engine/src/packages/package.source_install.cpp').read_text(encoding='utf-8')
    commit=definition(installer,'        enum class UpdateCommit',True)+'\ntemplate<class Rename>\n'+definition(installer,'        UpdateCommit commit_project_source_update(')
    planetary=(ROOT/'Engine/src/project/project.planetary.cpp').read_text(encoding='utf-8')
    build_cache='\n'.join(definition(planetary.replace('std::string_view water_hash={}', 'std::string_view water_hash=std::string_view()'),marker) for marker in (
        '        std::string read_small(', '        std::optional<fs::path> ordinary_directory(', '        std::optional<fs::path> project_directory(',
        '        std::optional<fs::path> generation_build_directory('))
    return PREFIX.replace('EDITION_HEADER',(ROOT/'Engine/include/epoch.edition.hpp').as_posix()) + task + STATE_PORT + functions + commit + build_cache + "\nbool can_attach(EditorState& editor){\n" + \
        "const bool sourceStaged=editor.packageSourceStaged.source_staged();const bool sourceReady=editor.packageSourceReview&&editor.packageSourceReview->review_ready();const bool sourceBusy=package_source_task_busy(editor);\n" + \
        guard + "return canAttach;\n}\n" + ACTION_PORT.replace("SOURCE_ACTIONS", actions) + MAIN


PREFIX = r'''
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include "EDITION_HEADER"
import package.source_install;
import asset.texture_artifact;
import package.registry;
import platform.filesystem;
namespace package_source=epochengine::package_source;
namespace package_registry=epochengine::package_registry;
namespace platform=epochengine::platform;
namespace asset=epochengine::asset;
namespace fs=std::filesystem;
using namespace std::chrono_literals;
namespace gui {struct InlineButtonSpec {std::string_view label{};float width{};bool enabled{true};};}
void check(bool value,int line){if(!value){std::fprintf(stderr,"Review contract line %d\n",line);std::exit(91);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
inline fs::path testRepository,testData;
namespace core::path {
fs::path candidate_data_root(){return testData;}
fs::path executable_dir(){return testData;}
fs::path find_epoch_repo_root(const fs::path&){return testRepository;}
}
'''

STATE_PORT = r'''
struct EditorState {
 std::string projectRoot{};
 std::string packageSourceExpectedId{},packageSourcePathDraft{},packageSourceStatus{};
 bool packageSourceView{},packageSourceReviewCurrent{},packageSourceTechnicalDetails{};
 bool projectBuildPending{},scriptBuildPending{},planetaryFeatureRequest{};
 std::optional<package_source::Inspection> packageSourceReview;
 std::vector<std::string> packageSourceFileLines;
 package_source::StageResult packageSourceStaged;
 std::unique_ptr<EditorPackageSourceTask> packageSourceTask;
 struct Lifecycle {struct Evidence {struct Generations {struct Selected{std::uint64_t generation{1};} selected_project;} generations;} value;
 const auto& evidence() const{return value;}} projectLifecycle;
};
bool package_source_task_busy(const EditorState& s){return s.packageSourceTask&&s.packageSourceTask->busy();}
bool planetary_project_busy(const EditorState&){return false;}
void push_editor_log(EditorState&,const std::string&){}
bool ai_package_generation_awaiting_attachment(const EditorState&){return false;}
'''


ACTION_PORT = r'''
auto source_actions(EditorState& editor,bool sourceBusy){
 const bool standardPackage=!editor.packageSourceExpectedId.empty();
 const bool sourceReady=editor.packageSourceReview&&editor.packageSourceReview->review_ready();
 const bool sourceStaged=editor.packageSourceStaged.source_staged();
 const bool canAttach=can_attach(editor),updateProjectSource=false;
 SOURCE_ACTIONS
 return sourceActions;
}
'''

MAIN = r'''
void write(const fs::path& path,std::string_view value){fs::create_directories(path.parent_path());std::ofstream f{path,std::ios::binary};f.write(value.data(),value.size());CHECK(f.good());}
void settle(EditorState& state){const auto end=std::chrono::steady_clock::now()+5s;while(state.packageSourceTask&&std::chrono::steady_clock::now()<end){poll_package_source_task(state);std::this_thread::sleep_for(1ms);}CHECK(!state.packageSourceTask);}
int main(){
 const auto root=fs::current_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 testRepository=root;testData=root/"data";
 const auto extension=root/"Engine/dep/EpochEngineExtensions";
 const auto manifest=extension/"packages/planetary_terrain/package.epoch-package.json";
 const auto unit=manifest.parent_path()/"src/unit.cpp";
 const std::string manifestBytes=R"({"id":"planetary_terrain","displayName":"Planet source","summary":"Review refresh contract","manifestLicense":"LicenseRef-MIT-NoSell","kind":"code-extension","activation":"package-manager-opt-in","security":{"serverOrListenerAllowed":false,"requiresExplicitNetworkApproval":false,"autoRunAllowed":false,"includeByDefault":false},"source":{"repo":"https://github.com/Autodidac/EpochEngineExtensions","path":"packages/planetary_terrain"},"script":{"path":"scripts/unit.ascript.cpp","language":"C++23","api":"epoch_package_script_api","humanGated":true,"autoRunAllowed":false},"build":{"humanGated":true,"sourceFiles":["src/unit.cpp"]}})";
 const auto makeSource=[&](const fs::path& owner){
  write(owner/"LICENSE","Contract fixture only.\n");write(owner/"packages/_api/epoch_package_script_api.hpp","// fixture\n");
  write(owner/"packages/planetary_terrain/package.epoch-package.json",manifestBytes);
  write(owner/"packages/planetary_terrain/src/unit.cpp","// version one\n");
  write(owner/"packages/planetary_terrain/scripts/unit.ascript.cpp","// never execute\n");
 };
 // An operator's Projects tree is not a durable package source authority.
 makeSource(root/"Projects/Extensions/EpochEngineExtensions");
 // Neither legacy root is a fallback source authority after consolidation.
 makeSource(root/"Extensions/EpochEngineExtensions");
 EditorState state;state.packageSourceExpectedId="research_planetary_terrain";
 open_package_source_review(state,state.packageSourceExpectedId);settle(state);
 CHECK(!state.packageSourceReviewCurrent&&!can_attach(state));
 CHECK(source_actions(state,false)[0].label=="Check Source Again"&&source_actions(state,false)[0].enabled);
 CHECK(!source_actions(state,true)[0].enabled&&!fs::exists(extension));
 makeSource(extension);
 open_package_source_review(state,state.packageSourceExpectedId);CHECK(state.packageSourceTask&&!state.packageSourceReviewCurrent);settle(state);
 CHECK(state.packageSourceReviewCurrent&&can_attach(state));
 CHECK(source_actions(state,false)[0].label=="Create Terrain Project"&&source_actions(state,false)[0].enabled);
 state.projectRoot="an-open-project";CHECK(source_actions(state,false)[0].label=="Add to Project"&&source_actions(state,false)[0].enabled);state.projectRoot.clear();
 const auto original=*state.packageSourceReview;const auto first=state.packageSourceStaged;
 write(unit,"// version two: explicit source update\n");
 open_package_source_review(state,state.packageSourceExpectedId);CHECK(state.packageSourceTask&&!state.packageSourceReviewCurrent&&!can_attach(state));
 CHECK(state.packageSourceReview->content_hash==original.content_hash);settle(state);
 CHECK(state.packageSourceReviewCurrent&&can_attach(state));const auto second=*state.packageSourceReview;
 CHECK(second.content_hash!=original.content_hash&&state.packageSourceStaged.content_hash==second.content_hash);
 CHECK(package_source::verify_staged_source(original,testData/"cache/packages").source_staged());
 // Failed refresh leaves the old readable identity but cannot approve it.
 write(manifest,"invalid new source manifest\n");open_package_source_review(state,state.packageSourceExpectedId);settle(state);
 CHECK(!state.packageSourceReviewCurrent&&!can_attach(state));CHECK(state.packageSourceReview->content_hash==second.content_hash);
 CHECK(source_actions(state,false)[0].label=="Check Source Again"&&source_actions(state,false)[0].enabled);
 CHECK(state.packageSourceStaged.content_hash==second.content_hash);CHECK(state.packageSourceStatus.find("Previous reviewed source is retained")!=std::string::npos);
 write(manifest,manifestBytes);
 // No stale/cancelled asynchronous result can become current.
 open_package_source_review(state,state.packageSourceExpectedId);state.packageSourceTask->cancellation.request_stop();settle(state);
 CHECK(!state.packageSourceReviewCurrent&&!can_attach(state));CHECK(state.packageSourceReview->content_hash==second.content_hash);
 open_package_source_review(state,state.packageSourceExpectedId);++state.projectLifecycle.value.generations.selected_project.generation;settle(state);
 CHECK(!state.packageSourceReviewCurrent&&!can_attach(state));CHECK(state.packageSourceReview->content_hash==second.content_hash);
 open_package_source_review(state,state.packageSourceExpectedId);state.packageSourceExpectedId="a-different-package";settle(state);
 CHECK(!state.packageSourceReviewCurrent&&!can_attach(state));
 // Explicit Advanced inspection uses the same lifecycle/cancellation gate.
 state.packageSourceExpectedId.clear();state.packageSourcePathDraft=manifest.generic_string();start_package_source_inspection(state);
 ++state.projectLifecycle.value.generations.selected_project.generation;settle(state);CHECK(!state.packageSourceReviewCurrent);
 state.packageSourceExpectedId="research_planetary_terrain";open_package_source_review(state,state.packageSourceExpectedId);settle(state);
 CHECK(state.packageSourceReviewCurrent&&can_attach(state));CHECK(state.packageSourceReview->content_hash==second.content_hash);
 CHECK(package_source::verify_staged_source(original,testData/"cache/packages").source_staged());
 CHECK(!fs::exists(extension/"project.epoch.json"));
 const auto cacheProject=root/"generation-cache";write(cacheProject/"project.epoch.json","{}\n");
 const auto oldHash="sha256:"+std::string(64,'a'),newHash="sha256:"+std::string(64,'b');
 const auto cacheSdk=root/"sdk";fs::create_directories(cacheSdk);
 const auto originalBuild=generation_build_directory(cacheProject,oldHash,cacheSdk);CHECK(originalBuild&&originalBuild->parent_path()==cacheSdk/"Artifacts/Intermediate/planet");
 write(*originalBuild/"old-compiler.bin","retained old compiler output");
 const auto newBuild=generation_build_directory(cacheProject,newHash,cacheSdk);CHECK(newBuild&&*newBuild!=*originalBuild);
 CHECK(generation_build_directory(cacheProject,newHash,cacheSdk)==newBuild&&generation_build_directory(cacheProject,oldHash,cacheSdk)==originalBuild);
 const auto readCache=[](const fs::path& path){std::ifstream input(path,std::ios::binary);return std::string{std::istreambuf_iterator<char>{input},{}};};
 CHECK(readCache(*originalBuild/"build.identity").find(oldHash)!=std::string::npos&&readCache(*originalBuild/"old-compiler.bin")=="retained old compiler output");
 const auto bound=readCache(*newBuild/"build.identity");write(*newBuild/"build.identity","wrong owner\n");
 CHECK(!generation_build_directory(cacheProject,newHash,cacheSdk));write(*newBuild/"build.identity",bound);
 CHECK(readCache(*newBuild/"build.identity")==bound&&!generation_build_directory(cacheProject,"invalid",cacheSdk));
 const auto unknown=root/"unbound-cache";write(unknown/"project.epoch.json","{}\n");write(unknown/"build/planet/foreign.bin","untouched");
 const auto separate=generation_build_directory(unknown,oldHash,cacheSdk);CHECK(separate&&*separate!=*originalBuild&&readCache(unknown/"build/planet/foreign.bin")=="untouched");
 // Actual commit helper, real directories; only rename failure is a labeled
 // deterministic port. Publication failure restores old source, while an
 // unknown rollback failure retains both complete trees for recovery.
 for(bool failRollback:{false,true}) {
  const auto folder=root/(failRollback?"recovery-port":"rollback-port");
  const auto staged=folder/"staged",installed=folder/"installed",previous=folder/"previous";
  write(installed/"source.txt","old");write(staged/"source.txt","new");unsigned calls{};
  const auto result=commit_project_source_update(staged,installed,previous,[&](const fs::path& from,const fs::path& to,std::error_code& error) {
   if(++calls==2u||(failRollback&&calls==3u)){error=std::make_error_code(std::errc::permission_denied);return;}
   fs::rename(from,to,error);
  });
  const auto read=[](const fs::path& path){std::ifstream file(path,std::ios::binary);return std::string{std::istreambuf_iterator<char>{file},{}};};
  CHECK(read(staged/"source.txt")=="new");
  if(failRollback){CHECK(result==UpdateCommit::recovery_required&&!fs::exists(installed)&&read(previous/"source.txt")=="old");}
  else {CHECK(result==UpdateCommit::rolled_back&&read(installed/"source.txt")=="old"&&!fs::exists(previous));
   CHECK(commit_project_source_update(staged,installed,previous,[](const auto& from,const auto& to,std::error_code& error){fs::rename(from,to,error);})==UpdateCommit::published);
   CHECK(read(installed/"source.txt")=="new"&&read(previous/"source.txt")=="old");}
 }
 return 0;
}
'''


class SourceReviewRefresh(unittest.TestCase):
    def test_actual_immutable_versions_and_review_owner_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC module consumer")
        proof = ROOT / "Artifacts/Reports/water-packages-oct31" / str(time.time_ns())
        build = proof / "build"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.package_review_contract.cpp"
        source.write_text(consumer(), encoding="utf-8")
        from Tests.packages.test_planetary_water_admission import consumer as water_consumer, durable_consumer
        water_source = proof / "editor.water_admission_contract.cpp"
        water_source.write_text(water_consumer(), encoding="utf-8")
        durable_source = proof / "project.water_requirement_contract.cpp"
        durable_source.write_text(durable_consumer(), encoding="utf-8")
        modules = [engine_module_source(ROOT / "Engine/modules", name + ".ixx") for name in
                   ("core.sha256", "package.registry", "package.catalog", "platform.filesystem", "package.source_install", "asset.texture_artifact")]
        implementation = [ROOT / "Engine/src/packages" / name for name in ("package.catalog.cpp", "package.source_install.cpp")]
        implementation.append(ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp')
        cmake = "cmake_minimum_required(VERSION 3.30)\ncmake_policy(SET CMP0141 NEW)\nset(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)\nproject(PackageReviewFreshness LANGUAGES CXX)\nenable_testing()\n"
        for name, consumer_path in (("package_source", ROOT / "Tests/packages/package.source_install_contract.cpp"), ("editor_review", source), ("editor_water", water_source)):
            cmake += f'''add_executable({name} "{consumer_path.as_posix()}" {" ".join('"'+p.as_posix()+'"' for p in implementation)} "{(ROOT/'Engine/resource/epoch.manifest').as_posix()}")
target_sources({name} PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
target_compile_features({name} PRIVATE cxx_std_23)
target_compile_options({name} PRIVATE /utf-8 /EHsc /permissive-)
target_link_libraries({name} PRIVATE winhttp)
add_test(NAME actual_{name} COMMAND {name})
set_tests_properties(actual_{name} PROPERTIES TIMEOUT 20)
'''
        durable_modules = modules + [engine_module_source(ROOT / "Engine/modules", name + ".ixx") for name in (
            "project.planetary", "asset.cloud_volume", "project.planetary_snapshot", "water.wave", "asset.mesh_artifact", "platform.child_process", "platform.work_admission")]
        durable_modules.append(ROOT / "Engine/dep/EpochPlatformEngine/modules/core.env.ixx")
        durable_implementation = implementation + [ROOT / name for name in (
            "Engine/src/project/project.planetary.cpp", "Engine/src/project/project.planetary_snapshot.cpp", "Engine/src/platform/platform.child_process.cpp",
            "Engine/src/platform/platform.work_admission.cpp", "Engine/dep/EpochPlatformEngine/src/core/core.env.cpp")]
        cmake += f'''add_executable(project_water "{durable_source.as_posix()}" {" ".join('"'+p.as_posix()+'"' for p in durable_implementation)} "{(ROOT/'Engine/resource/epoch.manifest').as_posix()}")
target_sources(project_water PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in durable_modules)})
target_compile_features(project_water PRIVATE cxx_std_23)
target_compile_options(project_water PRIVATE /utf-8 /EHsc /permissive-)
target_compile_definitions(project_water PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(project_water PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_link_libraries(project_water PRIVATE winhttp shell32 userenv advapi32 bcrypt comdlg32 windowscodecs ole32)
add_test(NAME actual_project_water COMMAND project_water)
set_tests_properties(actual_project_water PROPERTIES TIMEOUT 20)
'''
        (proof / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64", "-T", "host=x64",
                     "-DCMAKE_TOOLCHAIN_FILE=", "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        temporary = proof / "tmp"
        temporary.mkdir()
        environment = os.environ.copy()
        for key in ("TEMP", "TMP", "TMPDIR"):
            environment[key] = str(temporary)
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True, timeout=120)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout+completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-5000:]+completed.stderr)
        owners = [OWNER, STATE, PANEL, ROOT / "Engine/src/editor/editor.application.cpp", *durable_implementation, *durable_modules, ROOT / "Tests/packages/package.source_install_contract.cpp",
                  ROOT / "Tests/packages/test_planetary_water_admission.py"]
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": results,
            "sourceIdentity": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in owners},
            "scope": "Actual immutable source installer and Editor asynchronous review functions; application lifecycle/path/log boundary ports. No operator cache/project modifications or execution.",
            "cases": ["fresh/updated explicit durable source outside Projects", "deleted user Projects cannot remove durable source", "missing/failed source exposes enabled retry rather than disabled Create", "busy retry disabled and Add/Create still require exact source approval", "old cache/project source unchanged", "corrupt cache refuses repair by overwrite",
                      "cache-only ambiguity preserved", "failed/cancelled/stale package/project refresh cannot approve", "same-package success rechecks source", "explicit disabled stale Add/Create/Stage guards",
                      "planet-ocean feature resolves exact FFT source with explicit hash approval", "dry feature never inspects/requires FFT",
                      "actual hidden-modal frame cleanup preserves Project-owned pending review/approved admission and revokes idle hidden approval",
                      "decline/cancel/busy/stale parent/project/cache tamper preserve prior scene/settings/source",
                      "approved dependency reopens without original source/cache; corrupt existing dependency is not overwritten"],
        }, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
