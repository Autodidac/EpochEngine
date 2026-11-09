"""Compile real configuration persistence, build dispatch and lifecycle paths.

Reuse accepted CPU modules, never rebuild or execute Engine. Native child launch
is a recording substitute; real filesystem/JSON/GUI-state/lifecycle functions
are composed. Both platform branches of the path/dispatch owners are compiled.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from Tests.support.cpp import msvc_2022_toolsets
from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class ProjectBuildConfiguration(unittest.TestCase):
    def test_actual_release_fallback_and_managed_logging_migration(self):
        from Tests.support.cpp import compile_and_run, dependency_include
        import time

        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        core = (ROOT / "Engine/modules/editor.core.ixx").read_text(encoding="utf-8")
        shell = (ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl").read_text(encoding="utf-8")
        supported = definition(core, "    export [[nodiscard]] constexpr bool editor_project_build_configuration_supported(").replace("export ", "", 1)
        codec = definition(registry, "    [[nodiscard]] static std::optional<nlohmann::ordered_json>\n        parse_project_build_configuration_manifest(")
        codec += definition(registry, "    [[nodiscard]] static std::optional<std::string> project_build_configuration_manifest_text(")
        reader = definition(scene, "    [[nodiscard]] static std::optional<std::string> read_project_build_configuration_manifest(")
        reader += definition(scene, "    std::optional<std::string> editor_project_build_configuration(")
        repair = definition(shell, "    [[nodiscard]] static bool repair_generated_windows_child_project_build_files(")
        migration = definition(repair, '            if (scriptText.find("EPOCH_MANAGED_GENERATED_FILE: windows_build_v1")')
        replace = definition(shell, "    static void replace_all(")
        report = ROOT / "Artifacts/Reports/project-run-default-progress-oct31" / ("cpu-" + str(time.time_ns()))
        report.mkdir(parents=True)
        body = r'''
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#define CHECK(...) do { if(!(__VA_ARGS__)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#__VA_ARGS__); return 91; } } while(false)
namespace epochengine {
SUPPORTED
}
namespace fs=std::filesystem;
using epochengine::editor_project_build_configuration_supported;
fs::path resolve_project_root_path(const fs::path& path) { return path.empty()?fs::path{}:fs::absolute(path).lexically_normal(); }
std::string read_text_file(const fs::path& path) { std::ifstream f(path,std::ios::binary); return {std::istreambuf_iterator<char>{f},{}}; }
bool write(const fs::path& path,std::string_view text) { std::ofstream f(path,std::ios::binary); f<<text;return static_cast<bool>(f); }
CODEC
READER
REPLACE
void migrate(std::string& scriptText) {
MIGRATION
}
int main(int argc,char**argv) try {
 CHECK(argc==2);const fs::path root=argv[1];fs::create_directories(root);const auto manifest=root/"project.epoch.json";
 CHECK(editor_project_build_configuration(root.string())=="Release");
 const std::string original=R"({"id":"Fixture","custom":{"retain":[1,2,3]}})";
 CHECK(write(manifest,original));CHECK(editor_project_build_configuration(root.string())=="Release");CHECK(read_text_file(manifest)==original);
 for(const auto config:{"Debug","Release"}) {
  const auto changed=project_build_configuration_manifest_text(original,config);CHECK(changed);
  CHECK(write(manifest,*changed));CHECK(editor_project_build_configuration(root.string())==config);
  CHECK(read_text_file(manifest)==*changed);CHECK(project_build_configuration_manifest_text(*changed,config)==changed);
  CHECK(nlohmann::ordered_json::parse(*changed)["custom"]==nlohmann::ordered_json::parse(original)["custom"]);
 }
 for(const auto malformed:{R"({"build_configuration":1})",R"({"build_configuration":"Unsupported"})",
   R"({"build_configuration":"Debug","build_configuration":"Release"})",R"({"plugin":{"build_configuration":"Debug"}})","{broken"}) {
  CHECK(write(manifest,malformed));CHECK(!editor_project_build_configuration(root.string()));CHECK(read_text_file(manifest)==malformed);
 }
 std::string old="# EPOCH_MANAGED_GENERATED_FILE: windows_build_v1\nparam([string]$Configuration = 'Debug')\n$msbuildArgs = @('/clp:ErrorsOnly')\n";
 auto managed=old;migrate(managed);CHECK(managed.find("'Release'")!=std::string::npos);CHECK(managed.find("'/v:minimal'")!=std::string::npos);CHECK(managed.find("ErrorsOnly")==std::string::npos);
 const auto saved=managed;migrate(managed);CHECK(managed==saved);
 auto custom=old;custom.erase(0,custom.find('\n')+1);const auto customBefore=custom;migrate(custom);CHECK(custom==customBefore);
 auto unrelated=old;unrelated.replace(unrelated.find("windows_build_v1"),16,"windows_build_v9");const auto unrelatedBefore=unrelated;migrate(unrelated);CHECK(unrelated==unrelatedBefore);
 std::ofstream(root/"result.json")<<R"({"missing_defaults_release":true,"explicit_debug_release_preserved":true,"malformed_refused":true,"managed_migrated_idempotently":true,"custom_and_other_versions_unchanged":true})";
 return 0;
} catch(const std::exception& error) { std::fprintf(stderr,"%s\n",error.what());return 92; }
'''.replace("SUPPORTED", supported).replace("CODEC", codec).replace("READER", reader).replace("REPLACE", replace).replace("MIGRATION", migration)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        compile_and_run(self, source, report, args=lambda configuration: [report / "data" / configuration],
                        includes=[dependency_include(ROOT)])

    def test_actual_manifest_dispatch_and_configuration_bound_acceptance(self):
        prepared = ROOT / "build/validation/generated-project-application-oct26"
        if os.name != "nt" or not (prepared / "generator-cl.rsp").exists():
            self.skipTest("Accepted Windows CPU module/library closure unavailable")
        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        runtime = (ROOT / "Engine/src/editor/application/editor.application_project_runtime.inl").read_text(encoding="utf-8")
        paths = (ROOT / "Engine/src/editor/application/editor.application_runtime_source.inl").read_text(encoding="utf-8")
        core = (ROOT / "Engine/modules/editor.core.ixx").read_text(encoding="utf-8")
        shell = (ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl").read_text(encoding="utf-8")
        result_type = definition(core, "    export struct EditorProjectBuildResult", True).replace("export ", "", 1)
        supported = definition(core, "    export [[nodiscard]] constexpr bool editor_project_build_configuration_supported(").replace("export ", "", 1)
        codec = definition(registry, "    [[nodiscard]] static std::optional<nlohmann::ordered_json>\n        parse_project_build_configuration_manifest(")
        codec += definition(registry, "    [[nodiscard]] static std::optional<std::string> project_build_configuration_manifest_text(")
        persistence = definition(scene, "    [[nodiscard]] static std::optional<std::string> read_project_build_configuration_manifest(")
        persistence += definition(scene, "    std::optional<std::string> editor_project_build_configuration(")
        persistence += definition(scene, "    bool editor_set_project_build_configuration(")
        source_paths = "\n".join(definition(registry, marker) for marker in (
            "    [[nodiscard]] static std::string generated_project_artifact_stem(",
            "    [[nodiscard]] static fs::path generated_project_windows_build_script_path(",
            "    [[nodiscard]] static fs::path generated_project_linux_build_script_path(",
            "    [[nodiscard]] static fs::path generated_project_build_log_path(",
            "    [[nodiscard]] static fs::path reserve_generated_project_build_transcript(",
            "    [[nodiscard]] static fs::path generated_project_output_path(",
            "    [[nodiscard]] static std::vector<fs::path> generated_project_output_candidates(",
            "    [[nodiscard]] static fs::path generated_project_existing_output_path("))
        ui_paths = "\n".join(definition(paths, marker) for marker in (
            "        [[nodiscard]] std::string project_artifact_stem(",
            "        [[nodiscard]] std::filesystem::path project_output_exe_path(",
            "        [[nodiscard]] std::vector<std::filesystem::path> project_output_exe_candidates(",
            "        [[nodiscard]] std::filesystem::path project_existing_output_exe_path(",
            "        [[nodiscard]] std::filesystem::path project_build_log_path("))
        freshness = definition(runtime, "        struct ProjectBuildFreshness", True)
        freshness += "\n".join(definition(runtime, marker) for marker in (
            "        [[nodiscard]] std::filesystem::path project_linked_engine_library_path(",
            "        [[nodiscard]] bool file_is_newer_than(",
            "        [[nodiscard]] std::uint64_t project_build_input_fingerprint(",
            "        [[nodiscard]] ProjectBuildFreshness project_child_build_freshness("))
        settlement = "\n".join(definition(runtime, marker) for marker in (
            "        [[nodiscard]] bool project_build_targets_selected_project(",
            "        [[nodiscard]] std::filesystem::path resolved_project_build_output(",
            "        [[nodiscard]] std::optional<core::sha256::Digest>\n            project_output_digest(",
            "        [[nodiscard]] bool settle_project_build_lifecycle(",
            "        [[nodiscard]] bool select_project_build_configuration("))
        worker = definition(scene, "    EditorProjectBuildResult editor_build_project(\n")
        # The second platform branch shares the fixture State type. Rename only
        # these local function identifiers to prevent ADL finding the first
        # branch through that State; function bodies remain otherwise exact.
        portable_freshness = freshness.replace("project_build_input_fingerprint", "portable_build_input_fingerprint").replace(
            "project_linked_engine_library_path", "portable_linked_engine_library_path")
        body = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include "epoch.build_layout.hpp"
#include "updater.distribution.hpp"
import platform.filesystem;
import core.sha256;
import project.lifecycle;
#undef NDEBUG
#include <cassert>
namespace config {
namespace fs=std::filesystem;
namespace updater=epochengine::updater;
namespace build_layout=epochengine::build_layout;
namespace project_lifecycle=epochengine::project_lifecycle;
namespace core {namespace sha256=epochengine::core::sha256;namespace path {
inline fs::path sdk;fs::path executable_dir(){return sdk;}fs::path find_epoch_repo_root(const fs::path&){return sdk;}
}}
namespace platform {
namespace filesystem=epochengine::platform::filesystem;
namespace child_process {
enum class WindowMode{hidden};enum class LaunchCode{started};enum class WaitCode{exited,cancelled};
struct LaunchRequest{fs::path executable,working_directory,merged_output_path;std::vector<std::string>arguments;std::string correlation_key,exclusive_group,display_name;WindowMode window_mode;bool append_output{},disconnect_standard_input{};};
struct LaunchResult{LaunchCode code=LaunchCode::started;int handle=1;std::string message;};
struct Process{bool exit_code_valid=true;int exit_code=0;int handle=1;bool active()const{return false;}};
struct WaitResult{WaitCode code=WaitCode::exited;std::optional<Process>process=Process{};std::string message;};
inline LaunchRequest last;inline unsigned launches{};
LaunchResult launch_or_focus(const LaunchRequest& request){last=request;++launches;return {};}
WaitResult wait(int,std::stop_token){return {};}
std::optional<Process> active_in_group(std::string_view){return {};}
int focus(int){return 0;}
bool release(int){return true;}
}}
struct Profile{std::string id="Fixture";};inline Profile profile;inline bool admitted=true;
const Profile* find_project_profile_by_root(std::string_view){return admitted?&profile:nullptr;}
fs::path resolve_project_root_path(const fs::path& p){return p.empty()?fs::path{}:fs::absolute(p).lexically_normal();}
fs::path resolve_editor_path(const fs::path& p){return resolve_project_root_path(p);}
std::string display_project_path(const fs::path& p){return p.generic_string();}
std::string read_text_file(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>{f},{}};}
bool write_text_file(const fs::path& p,std::string_view text){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<text;return static_cast<bool>(f);}
struct Ensure{bool succeeded=true;std::string summary;};Ensure editor_ensure_project_shell(std::string_view){return {};}
bool repair_generated_windows_child_project_build_files(const fs::path&){return true;}
struct Runtime{bool active()const{return true;}};
struct EditorState{
std::string projectId="Fixture",projectRoot,activeScript,projectBuildConfiguration="Debug",projectBuildRequestedConfiguration,
projectBuildProjectRoot,projectBuildStatus;
std::uint64_t projectBuildProjectKey{},projectBuildLifecycleGeneration{};
bool projectBuildPending{},scriptBuildPending{},aiContinuousBuildPending{};
std::optional<Runtime>projectRuntimeSnapshot;
project_lifecycle::EvidenceLedger projectLifecycle;
core::sha256::Digest projectBuildAcceptedOutputDigest;
};
void push_editor_log(EditorState&,std::string_view){}
void clear_completed_editor_builds(EditorState&,std::string_view){}
void reconcile_project_runtime(EditorState&){}
bool planetary_project_busy(const EditorState&){return false;}
std::vector<fs::path> project_build_input_paths(const EditorState& e){return {fs::path{e.projectRoot}/"source/epoch.main.cpp"};}
std::string ai_build_path_stamp(const fs::path&p){return p.generic_string();}
}
namespace epochengine {
''' + supported + r'''
}
namespace {
namespace fs=std::filesystem;
''' + codec + source_paths + r'''
}
namespace config {
using epochengine::editor_project_build_configuration_supported;
''' + result_type + persistence + ui_paths + freshness + settlement + worker
        body += definition(shell, "    [[nodiscard]] static bool write_text_file_if_allowed(")
        body += r'''
void write(const fs::path&p,std::string_view text){assert(write_text_file(p,text));}
void begin(EditorState&e){
e.projectLifecycle={};assert(e.projectLifecycle.select_project(1));assert(e.projectLifecycle.record_scene_commit(1));
assert(e.projectLifecycle.record_shell_materialized());
auto fresh=project_child_build_freshness(e);assert(!fresh.rebuildRequired);
assert(e.projectLifecycle.observe_build_inputs(fresh.inputFingerprint));
e.projectBuildLifecycleGeneration=e.projectLifecycle.begin_build();assert(e.projectBuildLifecycleGeneration);
e.projectBuildProjectKey=1;e.projectBuildProjectRoot=e.projectRoot;e.projectBuildRequestedConfiguration=e.projectBuildConfiguration;
}
}
#undef _WIN32
namespace portable {
using namespace config;
''' + source_paths + ui_paths + portable_freshness + worker + r'''
}
#define _WIN32 1
int main(){
using namespace config;
const fs::path root=fs::current_path()/"Fixture";fs::create_directories(root);
const auto manifest=root/"project.epoch.json";
const auto original=std::string{"{\"id\":\"Fixture\",\"engine_edition\":\""}+std::string{updater::distribution::edition_id}+"\",\"custom\":{\"array\":[1,2,3],\"keep\":true}}";
write(manifest,original);
assert(editor_project_build_configuration(root.string())=="Release");
std::string diagnostic;
assert(editor_set_project_build_configuration(root.string(),"Release",diagnostic));
assert(editor_project_build_configuration(root.string())=="Release");
const auto saved=read_text_file(manifest);auto parsed=nlohmann::ordered_json::parse(saved);
assert(parsed["custom"]==nlohmann::ordered_json::parse(original)["custom"]);
assert(write_text_file_if_allowed(manifest,original,false));assert(read_text_file(manifest)==saved);
assert(!editor_set_project_build_configuration(root.string(),"ReleaseWithDebugInfo",diagnostic));assert(read_text_file(manifest)==saved);
for(const auto text:{"{\"build_configuration\":1}","{\"build_configuration\":\"Unsupported\"}","{\"build_configuration\":\"Debug\",\"build_configuration\":\"Release\"}","{\"plugin\":{\"build_configuration\":\"Debug\"}}","{broken"}){
write(manifest,text);assert(!editor_project_build_configuration(root.string()));assert(!editor_set_project_build_configuration(root.string(),"Release",diagnostic));assert(read_text_file(manifest)==text);
}
auto nested=parsed;nested["custom"]["build_configuration"]="plugin setting";nested["description"]="text containing \"build_configuration\": \"Debug\"";
write(manifest,nested.dump());assert(editor_project_build_configuration(root.string())=="Release");
assert(editor_set_project_build_configuration(root.string(),"Debug",diagnostic));
const auto retained=nlohmann::ordered_json::parse(read_text_file(manifest));
assert(retained["custom"]==nested["custom"]&&retained["description"]==nested["description"]);
write(manifest,saved);admitted=false;assert(!editor_set_project_build_configuration(root.string(),"Debug",diagnostic));admitted=true;
auto foreign=parsed;foreign["engine_edition"]="foreign";write(manifest,foreign.dump());assert(!editor_set_project_build_configuration(root.string(),"Debug",diagnostic));write(manifest,saved);
write(root/"source/epoch.main.cpp","source");write(root/"build_project.ps1","fixture");write(root/"build_project.sh","fixture");
const auto debug=generated_project_output_path(root,"Debug"),release=generated_project_output_path(root,"Release");
write(debug,"debug artifact");write(release,"release artifact");write(release.parent_path()/"EpochEditor.exe","foreign artifact");
assert(generated_project_output_candidates(root,"Release").size()==1);assert(project_output_exe_candidates(root.string(),"Release").size()==1);
assert(generated_project_existing_output_path(root,"Release")==release);assert(project_existing_output_exe_path(root.string(),"Release")==release);
fs::remove(release);assert(!fs::exists(generated_project_existing_output_path(root,"Release")));assert(fs::exists(debug));write(release,"release artifact");
assert(project_output_exe_path(root.string(),"../Debug").empty());
const auto windows=editor_build_project(root.string(),{},"Release");assert(windows.succeeded&&fs::path{windows.output_path}==release);
const auto args=platform::child_process::last.arguments;const auto argument=std::find(args.begin(),args.end(),"-Configuration");assert(argument!=args.end()&&*std::next(argument)=="Release");
const auto launches=platform::child_process::launches;assert(!editor_build_project(root.string(),{},"Debug").succeeded);assert(platform::child_process::launches==launches);
const auto portableRelease=portable::generated_project_output_path(root,"Release");write(portableRelease,"portable artifact");
const auto linux=portable::editor_build_project(root.string(),{},"Release");assert(linux.succeeded&&fs::path{linux.output_path}==portableRelease);
assert(platform::child_process::last.arguments.size()==2&&platform::child_process::last.arguments.back()=="Release");
assert(portable::project_output_exe_path(root.string(),"Release")==portableRelease);
EditorState state;state.projectRoot=root.string();state.projectBuildConfiguration="Release";
core::path::sdk=fs::current_path()/"sdk";write(core::path::sdk/"Tools/MSVC/EpochOutputLayout.props","epoch.build-output-layout/v1");
const auto now=fs::file_time_type::clock::now();fs::last_write_time(root/"source/epoch.main.cpp",now-std::chrono::hours{2});
fs::last_write_time(release,now-std::chrono::hours{1});fs::last_write_time(portableRelease,now-std::chrono::hours{1});
const auto releaseFingerprint=project_child_build_freshness(state).inputFingerprint;
state.projectBuildConfiguration="Debug";const auto debugFingerprint=project_build_input_fingerprint(state,project_build_input_paths(state));
assert(releaseFingerprint!=debugFingerprint);state.projectBuildConfiguration="Release";
auto linked=project_linked_engine_library_path(state);write(linked,"library");fs::last_write_time(linked,now);
assert(project_child_build_freshness(state).rebuildRequired);fs::remove(linked);
auto portableLinked=portable::portable_linked_engine_library_path(state);
assert(portableLinked==root/"Artifacts/Intermediate/cmake/linux/Clang-Release/epoch-engine/libEpochRuntime.a");write(portableLinked,"library");fs::last_write_time(portableLinked,now);
assert(portable::project_child_build_freshness(state).rebuildRequired);fs::remove(portableLinked);
begin(state);auto wrong=windows;wrong.output_path=debug.string();assert(!settle_project_build_lifecycle(state,wrong));
begin(state);auto foreignOutput=windows;foreignOutput.output_path=(release.parent_path()/"EpochEditor.exe").string();assert(!settle_project_build_lifecycle(state,foreignOutput));
begin(state);assert(settle_project_build_lifecycle(state,windows));assert(state.projectBuildAcceptedOutputDigest.valid());
state.projectBuildPending=true;assert(!select_project_build_configuration(state,"Debug"));assert(editor_project_build_configuration(root.string())=="Release");state.projectBuildPending=false;
assert(state.projectLifecycle.record_runtime_started());assert(!select_project_build_configuration(state,"Debug"));state.projectLifecycle.record_runtime_stopped();
assert(select_project_build_configuration(state,"Debug"));assert(editor_project_build_configuration(root.string())=="Debug");assert(!state.projectBuildAcceptedOutputDigest.valid());
assert(state.projectBuildConfiguration=="Debug");
}
'''
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix="project-configuration-", dir=parent))
        try:
            source, obj, binary = scratch / "contract.cpp", scratch / "contract.obj", scratch / "contract.exe"
            source.write_text(body, encoding="utf-8")
            compiler = (prepared / "generator-cl.rsp").read_text(encoding="utf-8").replace(str(prepared / "source/generator.cpp"), str(source))
            linker = (prepared / "generator-link.rsp").read_text(encoding="utf-8").replace(str(prepared / "generator.obj"), str(obj))
            # Retained module products remain valid; only their external
            # dependency cache was relocated by the output-layout migration.
            former_dependencies = re.escape(str(ROOT / "Engine/vcpkg_installed"))
            current_dependencies = str(ROOT / "Artifacts/Dependencies/vcpkg_installed")
            compiler = re.sub(former_dependencies, lambda _: current_dependencies, compiler, flags=re.I)
            linker = re.sub(former_dependencies, lambda _: current_dependencies, linker, flags=re.I)
            for flag, value in (("Fo", obj), ("Fd", scratch / "compiler.pdb"), ("Fa", scratch / "compiler.asm")):
                compiler = re.sub(r"/" + flag + r'(?:"[^"]*"|\S+)', lambda _: f'/{flag}"{value}"', compiler, flags=re.I)
            for flag, value in (("OUT", binary), ("PDB", scratch / "contract.pdb"), ("IMPLIB", scratch / "contract.lib"), ("MANIFESTFILE", scratch / "contract.manifest")):
                linker = re.sub(r"/" + flag + r':(?:"[^"]*"|\S+)', lambda _: f'/{flag}:"{value}"', linker, flags=re.I)
            self.assertIn(str(source), compiler)
            self.assertIn(str(obj), linker)
            (scratch / "cl.rsp").write_text(compiler, encoding="utf-8")
            (scratch / "link.rsp").write_text(linker, encoding="utf-8")
            tools = msvc_2022_toolsets()
            if not tools:
                self.skipTest("Installed MSVC compiler unavailable")
            tool = tools[-1]
            sdk = Path("C:/Program Files (x86)/Windows Kits/10")
            kit = sorted((sdk / "Include").glob("*"))[-1]
            environment = dict(os.environ)
            environment["INCLUDE"] = ";".join(map(str, (tool / "include", kit / "ucrt", kit / "shared", kit / "um", kit / "winrt")))
            environment["LIB"] = ";".join(map(str, (tool / "lib/x64", sdk / "Lib" / kit.name / "um/x64", sdk / "Lib" / kit.name / "ucrt/x64")))
            environment["PATH"] = str(tool / "bin/Hostx64/x64") + ";" + str(sdk / "bin" / kit.name / "x64") + ";" + environment.get("PATH", "")
            environment["TMP"] = environment["TEMP"] = str(scratch)
            for executable, response in (("cl.exe", "cl.rsp"), ("link.exe", "link.rsp")):
                result = subprocess.run([str(tool / "bin/Hostx64/x64" / executable), "@" + str(scratch / response)], cwd=ROOT, env=environment, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-10000:] + result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == "__main__":
    unittest.main()
