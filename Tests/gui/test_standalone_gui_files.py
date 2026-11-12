"""Real GUI codec/writer and save/reload handlers; dialog/scene ports are recorded."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    docs = (ROOT / "Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8-sig")
    state = (ROOT / "Engine/src/editor/application/editor.application_state.inl").read_text(encoding="utf-8-sig")
    fragments = "\n".join(definition(docs, marker) for marker in (
        "        [[nodiscard]] bool gui_document_has_unsaved_changes(",
        "        [[nodiscard]] std::optional<std::vector<std::byte>>\n        read_project_gui_document_bytes(",
        "        [[nodiscard]] bool write_verified_gui_document(",
        "        [[nodiscard]] std::unique_ptr<\n            authoring::gui::GuiDocument>\n        decode_gui_document_file(",
        "        [[nodiscard]] bool open_gui_document_from_dialog(",
        "        [[nodiscard]] bool save_gui_document_from_dialog(",
        "        [[nodiscard]] bool reload_standalone_gui_document("))
    identity = definition(state, "            struct GuiWidgetIdentity final", True)
    catalog = definition(state, "        struct GuiTemplateDefinition final", True)
    catalog += definition(state, "        inline constexpr std::array kGuiTemplateCatalog", True)
    return r'''
#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
import authoring.gui_document;
import platform.filesystem;
namespace authoring=epochengine::authoring;
namespace native_fs=epochengine::platform::filesystem;
namespace fs=std::filesystem;
namespace core::path {fs::path candidate;fs::path candidate_data_root(){return candidate;}}
namespace platform::filesystem {
 using SaveFileDialogResult=native_fs::SaveFileDialogResult;
 using OpenFileDialogResult=native_fs::OpenFileDialogResult;
 SaveFileDialogResult save_result;OpenFileDialogResult open_result;
 std::wstring last_title;unsigned saves{},opens{};
 SaveFileDialogResult save_file_dialog(std::wstring_view title,std::wstring_view,std::wstring_view,
  std::wstring_view,std::wstring_view,const fs::path&){++saves;last_title=title;return save_result;}
 OpenFileDialogResult open_file_dialog(std::wstring_view,std::wstring_view,std::wstring_view,const fs::path&){++opens;return open_result;}
 bool exclusive_create_and_write(const fs::path& path,std::span<const std::byte> bytes,std::error_code& error){
  return native_fs::exclusive_create_and_write(path,bytes,error);}
 bool atomic_replace_same_filesystem(const fs::path& from,const fs::path& to,std::error_code& error){
  return native_fs::atomic_replace_same_filesystem(from,to,error);}
}
using GuiTemplatePreset=authoring::gui::TemplatePreset;
''' + catalog + r'''
struct EditorState {
''' + identity + r'''
 std::unique_ptr<authoring::gui::GuiDocument> guiDocument;
 std::string projectId,projectRoot,guiDocumentSourcePath,guiDocumentStatus;
 std::uint64_t guiDocumentSavedRevision{};std::size_t guiTemplatePreset{};
 bool guiDocumentReplaceArmed{};GuiWidgetIdentity guiSelectedWidget,guiContentEditorWidget;
};
bool has_open_project(const EditorState& state){return !state.projectId.empty();}
auto* synchronize_gui_document(EditorState& state){return state.guiDocument.get();}
fs::path resolve_editor_path(const fs::path& value){return value;}
std::string display_project_path(const fs::path& value){return fs::absolute(value).lexically_normal().generic_string();}
unsigned admissions{},installations{};
fs::path editor_resolve_candidate_project_path(const fs::path& value){
 ++admissions;const auto root=core::path::candidate_data_root()/"Projects";
 const auto relative=value.lexically_normal().lexically_relative(root);
 if(relative.empty()||relative.is_absolute())return {};
 for(const auto& part:relative)if(part=="..")return {};
 return value;
}
void push_editor_log(EditorState&,const std::string&){}
// Scene installation is a recording boundary. Persistence and codecs below
// are the actual production functions; this test makes no scene/native claim.
bool install_gui_document(EditorState& state,std::unique_ptr<authoring::gui::GuiDocument> document,
 std::string source,bool saved,std::string status){
 ++installations;state.guiDocument=std::move(document);state.guiDocumentSourcePath=std::move(source);
 state.guiDocumentSavedRevision=saved?state.guiDocument->revision().sequence:0;
 state.guiDocumentReplaceArmed=false;state.guiDocumentStatus=std::move(status);return true;
}
constexpr std::uint64_t kMaximumProjectGuiDocumentBytes=64ull*1024*1024;
''' + fragments + r'''
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
std::string read(const fs::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>{file},{}};}
int main(int argc,char**argv){
 CHECK(argc==2);const fs::path root=argv[1];fs::create_directories(root);
 EditorState state;auto source=authoring::gui::make_template_document(GuiTemplatePreset::desktop_app);CHECK(source);
 state.guiDocument=std::make_unique<authoring::gui::GuiDocument>(std::move(*source));
 state.guiDocumentSourcePath="Unsaved Desktop App template";
 const auto original=state.guiDocument->snapshot();
 auto selected=state.guiDocument->widgets()[1].handle;
 state.guiSelectedWidget={state.guiDocument->document(),state.guiDocument->branch(),selected,"","",state.guiDocumentSourcePath};
 state.guiContentEditorWidget=state.guiSelectedWidget;
 platform::filesystem::save_result={true,false,root/"document",{}};
 CHECK(save_gui_document_from_dialog(state));
 const auto path=root/"document.epochgui";CHECK(fs::is_regular_file(path));
 CHECK(platform::filesystem::last_title==L"Save Epoch GUI Document");
 CHECK(!gui_document_has_unsaved_changes(state));
 CHECK(state.guiSelectedWidget.sourcePath==display_project_path(path));
 CHECK(state.guiContentEditorWidget.sourcePath==state.guiDocumentSourcePath);
 std::string failure;const auto decoded=decode_gui_document_file(path,failure);CHECK(decoded&&decoded->snapshot()==original);
 const auto saved_path=state.guiDocumentSourcePath,saved_bytes=read(path);
 const auto saved_revision=state.guiDocumentSavedRevision;
 platform::filesystem::save_result={false,true,{},{}};CHECK(!save_gui_document_from_dialog(state));
 CHECK(state.guiDocumentSourcePath==saved_path&&state.guiDocumentSavedRevision==saved_revision&&read(path)==saved_bytes);
 // An unrelated colliding temporary file must never be deleted or overwritten.
 {std::ofstream file(root/"document.epochgui.write");file<<"operator file";}
 platform::filesystem::save_result={true,false,path,{}};CHECK(!save_gui_document_from_dialog(state));
 CHECK(read(root/"document.epochgui.write")=="operator file"&&read(path)==saved_bytes);
 CHECK(state.guiDocumentSourcePath==saved_path&&state.guiDocumentSavedRevision==saved_revision);
 fs::remove(root/"document.epochgui.write");
 // Template export remains a copy: it cannot mark this document saved elsewhere.
 platform::filesystem::save_result={true,false,root/"template-copy.epochgui",{}};
 CHECK(save_gui_document_from_dialog(state,true));CHECK(state.guiDocumentSourcePath==saved_path);
 CHECK(platform::filesystem::last_title==L"Save Epoch GUI Template");
 // A real mutation requires explicit Reload confirmation and then restores bytes.
 auto layout=state.guiDocument->widget(selected)->descriptor.layout;layout.x+=17;
 CHECK(state.guiDocument->update_layout(selected,layout));CHECK(gui_document_has_unsaved_changes(state));
 const auto dirty=state.guiDocument->snapshot();const auto prior_installs=installations;
 CHECK(!reload_standalone_gui_document(state));CHECK(state.guiDocument->snapshot()==dirty&&installations==prior_installs);
 CHECK(reload_standalone_gui_document(state));CHECK(state.guiDocument->snapshot()==original);
 CHECK(!gui_document_has_unsaved_changes(state));
 // Corrupt source is retained; the active authoring document is not replaced.
 {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"invalid";}
 CHECK(!reload_standalone_gui_document(state));CHECK(state.guiDocument->snapshot()==original&&read(path)=="invalid");
 CHECK(write_verified_gui_document(*state.guiDocument,path,failure));
 // Ordinary standalone Open is saved, while importing to a project stays a draft.
 EditorState opened;platform::filesystem::open_result={true,false,path,{}};
 CHECK(open_gui_document_from_dialog(opened));CHECK(!gui_document_has_unsaved_changes(opened));
 EditorState project;project.projectId="project";project.projectRoot=root.generic_string();
 CHECK(open_gui_document_from_dialog(project));CHECK(gui_document_has_unsaved_changes(project));
 // Existing candidate admission remains mandatory for every selected destination.
 core::path::candidate=root/"candidate";platform::filesystem::save_result={true,false,root/"outside.epochgui",{}};
 const auto old_source=opened.guiDocumentSourcePath;CHECK(!save_gui_document_from_dialog(opened));
 CHECK(admissions>0&&!fs::exists(root/"outside.epochgui")&&opened.guiDocumentSourcePath==old_source);
 platform::filesystem::save_result={true,false,core::path::candidate/"Projects/private.epochgui",{}};
 CHECK(save_gui_document_from_dialog(opened));CHECK(fs::is_regular_file(platform::filesystem::save_result.path));
 return 0;
}
'''


class StandaloneGuiFiles(unittest.TestCase):
    def test_real_document_save_reload_and_failure_preservation(self):
        proof = ROOT / "build/validation/standalone-gui-files-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "gui.files_contract.cpp").write_text(fixture(), encoding="utf-8")
        modules = [engine_module_source(ROOT / "Engine/modules", name) for name in (
            "authoring.document.ixx", "authoring.gui_document.ixx", "platform.filesystem.ixx", "core.sha256.ixx")]
        files = " ".join('"' + path.as_posix() + '"' for path in modules)
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(StandaloneGuiFiles LANGUAGES CXX)
add_executable(gui_files gui.files_contract.cpp
 "{(ROOT/'Engine/src/authoring/gui/authoring.gui_document_codec.cpp').as_posix()}"
 "{(ROOT/'Engine/src/authoring/gui/authoring.gui_templates.cpp').as_posix()}")
target_compile_features(gui_files PRIVATE cxx_std_23)
target_sources(gui_files PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {files})
if(MSVC)
 target_compile_options(gui_files PRIVATE /permissive- /utf-8 /EHsc)
 target_link_libraries(gui_files PRIVATE comdlg32 shell32 ole32)
endif()
enable_testing()
add_test(NAME actual_gui_file_handlers COMMAND gui_files "${{CMAKE_CURRENT_BINARY_DIR}}/files/$<CONFIG>")
set_tests_properties(actual_gui_file_handlers PROPERTIES TIMEOUT 30)
''', encoding="utf-8")
        commands = [("configure", ["cmake", "-S", str(proof), "-B", str(proof / "cmake")])]
        commands[0][1].extend(["-G", "Visual Studio 17 2022", "-A", "x64"] if os.name == "nt"
            else ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"])
        for config in (("Debug", "Release") if os.name == "nt" else ("Release",)):
            build = ["cmake", "--build", str(proof / "cmake"), "--config", config, "--parallel", "1"]
            if os.name == "nt":
                build += ["--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]
            commands += [(config + "-build", build), (config + "-contract", ["ctest", "--test-dir",
                str(proof / "cmake"), "-C", config, "--output-on-failure"])]
        completed = []
        for label, command in commands:
            result = subprocess.run(command, cwd=ROOT, env=dict(os.environ, MSBUILDDISABLENODEREUSE="1"),
                capture_output=True, text=True, timeout=180)
            (proof / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            completed.append({"phase": label, "exit": result.returncode, "command": command})
            (proof / "result.json").write_text(json.dumps({"completed": completed,
                "actual_modules": 4, "dialog_and_scene_installation": "recorded boundaries",
                "native_or_full_engine_acceptance": False}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-12000:])

    def test_dedicated_gui_controls_reach_existing_handlers(self):
        outliner = (ROOT / "Engine/src/editor/application/editor.frame_outliner.inl").read_text(encoding="utf-8")
        menu = (ROOT / "Engine/src/editor/application/editor.frame_command_menus.inl").read_text(encoding="utf-8")
        actions = outliner[outliner.index("const std::array saveReloadActions"):outliner.index("templateLabels{")]
        self.assertIn('projectOpen ? "Save GUI" : "Save GUI..."', actions)
        self.assertIn("if(!projectOpen)", actions)
        self.assertIn("save_gui_document_from_dialog(editor)", actions)
        self.assertIn("reload_standalone_gui_document(editor)", actions)
        self.assertIn("save_project_gui_document(editor)", actions)
        self.assertIn("save_gui_document_from_dialog(editor,true)", outliner)
        self.assertIn('menu_item(standaloneGui ? "Save GUI..." : "Save Project"', menu)


if __name__ == "__main__":
    unittest.main()
