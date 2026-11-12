"""Execute the existing project-AI writer and its real Package Manager actions.

Uses the production profile codec and filesystem functions. UI drawing and model
inventory are recording ports; no model, download, native window or engine runs.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
WRITER = ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl"
VIEW = ROOT / "Engine/src/editor/application/editor.frame_packages_about.inl"


def fixture():
    writer = WRITER.read_text(encoding="utf-8")
    filesystem = (ROOT / "Engine/modules/platform/platform.filesystem.ixx").read_text(encoding="utf-8")
    backend = "\n".join(definition(filesystem, marker) for marker in (
        "    [[nodiscard]] inline bool exclusive_create_and_write(",
        "    [[nodiscard]] inline bool atomic_replace_same_filesystem("))
    profile_writer = "\n".join(definition(writer, marker, marker.endswith("Mode")) for marker in (
        "        enum class ProjectAiProviderMode", "        [[nodiscard]] std::string read_bounded_package_file(",
        "        [[nodiscard]] bool write_project_ai_profile("))
    # Extract only the new AI branch, never the add-on/source-review UI.
    view = VIEW.read_text(encoding="utf-8")
    branch = definition(view, "            if (projectAiResources)\n            {\n                namespace native")
    emitter = definition((ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl").read_text(
        encoding="utf-8"), "    [[nodiscard]] static bool write_text_file_if_allowed(")
    return r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
import ai.project_profile;
namespace fs=std::filesystem;
namespace platform::filesystem {
FILESYSTEM
}
struct Process {bool active{};bool valid()const{return active;}};
struct EditorState {
 std::string projectId,projectRoot,packageInstallStatus;
 float packageInstallProgress{};bool projectBuildPending{};Process projectRuntimeProcess;
 bool aiModelManagerAutoRescanPending{},showPackageManagerModal=true,showAiModelManagerModal{};
 int aiModelManagerTab{};
};
struct Identity {bool succeeded{};};bool identityAllowed=true;unsigned identityCalls{},logCalls{};
Identity editor_inspect_project_manifest_identity(std::string_view manifest,std::string_view id,std::string_view root) {
 ++identityCalls;return {identityAllowed&&!id.empty()&&fs::path{manifest}==fs::path{root}/"project.epoch.json"};
}
fs::path resolve_editor_path(const fs::path&p){return p;}
void push_editor_log(EditorState&,const std::string&){++logCalls;}
WRITER
bool write_text_file(const fs::path&,std::string_view){std::abort();}
EMITTER
namespace epochengine::ai {
 std::string active_model_name(){return "";}
 namespace native_runtime {
  struct Model {std::string id,label,uri;bool active{},installed{};};
  struct Snapshot {bool busy{},selected{},inventory_preview=true;std::string model,inventory_error;std::vector<Model>models;};
  Snapshot current;unsigned refreshCalls{};
  Snapshot snapshot(bool=true){return current;}
  bool refresh(){++refreshCalls;return true;}
 }
}
namespace gui {
 struct Vec2{float x{},y{};};
 struct InlineButtonSpec{std::string_view label;float width{};bool enabled=true;};
 struct InlineButtonRowOptions{};
 struct ScrollAreaOptions{std::string_view id;Vec2 size;float content_height{};bool draw_background{},show_scrollbar{};};
 std::vector<std::string>labels;std::string requestedAction;bool settingsClick{};
 void wrapped_label(std::string_view s,float){labels.emplace_back(s);}
 void label(std::string_view s){labels.emplace_back(s);}
 float measure_wrapped_label_height(std::string_view,float){return 20;}
 template<std::size_t N> float inline_button_row_height(const std::array<InlineButtonSpec,N>&,float,float,const InlineButtonRowOptions&){return 30;}
 template<std::size_t N> std::optional<std::size_t> inline_button_row(const std::array<InlineButtonSpec,N>&items,float,float,const InlineButtonRowOptions&) {
  for(std::size_t i=0;i<N;++i)if(items[i].enabled&&items[i].label==requestedAction){requestedAction.clear();return i;}
  return {};
 }
 Vec2 cursor_position(){return {};}
 float line_height(){return 20;}
 bool begin_scroll_area(const ScrollAreaOptions&){return true;}
 void end_scroll_area(){}void set_cursor(Vec2){}void advance_cursor(Vec2){}
 bool text_link(std::string_view,Vec2,bool){return std::exchange(settingsClick,false);}
}
namespace editor_update_modal {
 struct Body{float listHeight=300,footerTop=400;};
 Body measure_package_manager_body(float,float,float,float){return {};}
}
enum class EditorMainSurface{Project};unsigned projectWorkspaceOpens{};
void open_editor_surface(EditorMainSurface,std::string_view){++projectWorkspaceOpens;}
std::string nativeModelId;
void render_project_ai(EditorState&editor) {
 const bool projectAiResources=true;const float contentWidth=500,packageContentBottom=600;
 const gui::Vec2 modalPos{};const gui::InlineButtonRowOptions packageActionLayout{};
VIEW
}
void check(bool ok,int line){if(!ok){std::fprintf(stderr,"Project AI choice contract line %d\n",line);std::exit(81);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
std::string read(const fs::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
void write(const fs::path&p,std::string_view bytes){std::ofstream out(p,std::ios::binary|std::ios::trunc);out<<bytes;}
int main(int argc,char**argv) {
 CHECK(argc==2);const fs::path root=fs::absolute(argv[1]);CHECK(!fs::exists(root));fs::create_directories(root/"assets");
 write(root/"project.epoch.json","{\"project_id\":\"fixture\"}\n");
 EditorState editor;editor.projectId="fixture";editor.projectRoot=root.string();
 CHECK(write_project_ai_profile(editor,ProjectAiProviderMode::engine_selected));
 const fs::path profile=root/"Assets/AI/project_ai.epochai";const auto enabled=read(profile);
 auto parsed=epochengine::ai::project_profile::parse_profile(enabled);CHECK(parsed);
 CHECK(parsed.profile.provider==epochengine::ai::project_profile::Provider::engine_selected);
 CHECK(parsed.profile.model_binding=="engine_selected_runtime_model"&&!parsed.profile.auto_start&&!parsed.profile.weights_bundled&&!parsed.profile.engine_source_write);
 CHECK(write_project_ai_profile(editor,ProjectAiProviderMode::epoch_local));
 CHECK(epochengine::ai::project_profile::parse_profile(read(profile)).profile.provider==epochengine::ai::project_profile::Provider::epoch_local_qwen38);
 CHECK(editor.packageInstallStatus.find("installed local Qwen")!=std::string::npos&&editor.packageInstallStatus.find("engine-selected")==std::string::npos);
 CHECK(write_project_ai_profile(editor,ProjectAiProviderMode::external_mcp));
 CHECK(epochengine::ai::project_profile::parse_profile(read(profile)).profile.provider==epochengine::ai::project_profile::Provider::external_mcp);
 CHECK(editor.packageInstallStatus.find("external provider")!=std::string::npos&&editor.packageInstallStatus.find("engine-selected")==std::string::npos);
 CHECK(write_project_ai_profile(editor,ProjectAiProviderMode::engine_selected)&&read(profile)==enabled);
 CHECK(write_text_file_if_allowed(profile,"would disable AI",false)&&read(profile)==enabled);
 editor.projectBuildPending=true;CHECK(!write_project_ai_profile(editor,ProjectAiProviderMode::disabled)&&read(profile)==enabled);editor.projectBuildPending=false;
 editor.projectRuntimeProcess.active=true;CHECK(!write_project_ai_profile(editor,ProjectAiProviderMode::disabled)&&read(profile)==enabled);editor.projectRuntimeProcess.active=false;
 identityAllowed=false;CHECK(!write_project_ai_profile(editor,ProjectAiProviderMode::disabled)&&read(profile)==enabled);identityAllowed=true;
 CHECK(!write_project_ai_profile(editor,static_cast<ProjectAiProviderMode>(255))&&read(profile)==enabled);
#ifdef _WIN32
 const HANDLE lock=::CreateFileW(profile.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(lock!=INVALID_HANDLE_VALUE);
 CHECK(!write_project_ai_profile(editor,ProjectAiProviderMode::disabled)&&read(profile)==enabled);CHECK(::CloseHandle(lock));
#endif
 write(profile,"not a profile");CHECK(!write_project_ai_profile(editor,ProjectAiProviderMode::engine_selected)&&read(profile)=="not a profile");write(profile,enabled);
 namespace native=epochengine::ai::native_runtime;
 native::current.models={{"flash","Flash Next","qwenflash://flash"},{"27b","Qwen 27B","qwenflash://27b"},{"9b","Qwen 9B","qwenflash://9b"}};
 gui::requestedAction="Disable Project AI";render_project_ai(editor);CHECK(epochengine::ai::project_profile::parse_profile(read(profile)).profile.provider==epochengine::ai::project_profile::Provider::disabled);
 gui::requestedAction="Enable Project AI";render_project_ai(editor);CHECK(read(profile)==enabled);
 for(const auto* name:{"Flash Next","Qwen 27B","Qwen 9B"})CHECK(std::find(gui::labels.begin(),gui::labels.end(),name)!=gui::labels.end());
 CHECK(std::none_of(gui::labels.begin(),gui::labels.end(),[](auto&s){return s.find("Bonsai")!=std::string::npos;}));
 gui::settingsClick=true;render_project_ai(editor);CHECK(nativeModelId=="flash"&&editor.showAiModelManagerModal&&!editor.showPackageManagerModal);
 editor.aiModelManagerAutoRescanPending=true;native::current.busy=true;render_project_ai(editor);CHECK(native::refreshCalls==0&&editor.aiModelManagerAutoRescanPending);
 native::current.busy=false;render_project_ai(editor);CHECK(native::refreshCalls==1&&!editor.aiModelManagerAutoRescanPending);render_project_ai(editor);CHECK(native::refreshCalls==1);
 EditorState absent;gui::requestedAction="Enable Project AI";render_project_ai(absent);CHECK(gui::requestedAction=="Enable Project AI");
 gui::requestedAction="Open Project Workspace";render_project_ai(absent);CHECK(projectWorkspaceOpens==1&&!absent.showPackageManagerModal);
 CHECK(identityCalls>0&&logCalls>0);return 0;
}
'''.replace("FILESYSTEM", backend).replace("WRITER", profile_writer).replace(
        "EMITTER", emitter).replace("VIEW", branch)


class ProjectAiResourceChoice(unittest.TestCase):
    def test_actual_profile_writer_and_resource_actions(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC module consumer; separate admitted Clang lane required")
        proof = ROOT / "build/validation/project-ai-resources-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "project.ai_resource_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ProjectAiResourceChoice LANGUAGES CXX)
add_executable(project_ai "{source.as_posix()}" "{(ROOT/'Engine/src/ai/ai.project_profile.cpp').as_posix()}")
target_compile_features(project_ai PRIVATE cxx_std_23)
target_compile_options(project_ai PRIVATE /W3 /WX /utf-8)
target_sources(project_ai PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES "{(ROOT/'Engine/modules/ai/ai.project_profile.ixx').as_posix()}" "{(ROOT/'Engine/modules/core/core.sha256.ixx').as_posix()}")
enable_testing()
add_test(NAME actual_project_ai_choice COMMAND project_ai "${{CMAKE_CURRENT_BINARY_DIR}}/project-fixture")
''', encoding="utf-8")
        records = []
        run_root = Path(tempfile.mkdtemp(prefix="run-", dir=proof))
        commands = [["cmake", "-S", str(proof), "-B", str(proof/"msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(proof/"msvc"), "--config", config, "--parallel", "1"],
                [str(proof/"msvc"/config/"project_ai.exe"), str(run_root/config)]]
        # Unique fixture destinations keep successful prior data and failed evidence.
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = run_root/f"lane-{index}.log"; log.write_text(completed.stdout+completed.stderr, encoding="utf-8")
            records.append({"command":command,"exit":completed.returncode,"log":str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-8000:]+completed.stderr)
        (proof/"result.json").write_text(json.dumps({"state":"ACTUAL_PROFILE_AND_UI_ACTION_CPU_PASS",
            "scope":"Real canonical codec, profile writer, atomic filesystem functions and actual AI UI branch; model inventory/drawing/identity parser are recording ports",
            "commands":records,"native":False,"inference":False,"autoload":False,
            "owners":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in (WRITER,VIEW)}},indent=2)+"\n")


if __name__ == "__main__":
    unittest.main()
