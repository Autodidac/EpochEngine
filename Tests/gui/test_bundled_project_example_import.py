"""Exercise the bundled project transaction with real filesystem publication."""
import os
from pathlib import Path
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.gui_contract import run_gui_contract

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == "nt", "MSVC CPU contract lane")
class BundledProjectExampleImport(unittest.TestCase):
    def test_import_authored_inputs_without_replacing_a_project(self):
        scene = ROOT / "Engine/src/editor/editor.scene.cpp"
        shell = ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl"
        registry = ROOT / "Engine/src/editor/implementation/editor.project_registry.inl"
        module = ROOT / "Engine/modules/editor/editor.core.ixx"
        source = scene.read_text(encoding="utf-8")
        bodies = source[source.index("        constexpr std::string_view kProjectExampleManifest"):
                        source.index("    std::span<const EditorProjectProfile> editor_project_profiles() noexcept")]
        bodies = "    namespace\n    {\n" + bodies
        helpers = "\n".join(
            definition(owner.read_text(encoding="utf-8"), marker)
            for owner, marker in (
                (shell, "    static void replace_all("),
                (shell, "    [[nodiscard]] static bool replace_delimited_value("),
                (shell, "    [[nodiscard]] static bool claim_new_project_root("),
                (shell, "    [[nodiscard]] static bool publish_new_project_manifest("),
                (registry, "    [[nodiscard]] static std::string to_windows_path("),
                (registry, "    [[nodiscard]] static std::string xml_escape("),
                (registry, "    [[nodiscard]] static std::string json_escape("),
                (registry, "    [[nodiscard]] static std::string powershell_escape_single_quoted("),
                (registry, "    [[nodiscard]] static std::string bash_escape_single_quoted("),
            )
        )
        descriptor = definition(module.read_text(encoding="utf-8"),
                                "    export struct EditorProjectExample", True).replace("export ", "")
        example = ROOT / "Engine/examples/BasicSoftwareExampleCalculator"
        platformer = ROOT / "Engine/examples/PlatformerExample2D"
        code = r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
import platform.filesystem;
namespace fs = std::filesystem;
#define CHECK(x) do { if (!(x)) return __LINE__; } while (false)
namespace epochengine {
namespace updater::distribution {
constexpr std::string_view edition_id = "epoch.development.site.v1";
}
DESCRIPTOR
struct EditorProjectAdmissionResult {
    bool succeeded{};
    std::string project_id, manifest_path, summary;
};

HELPERS

fs::path repo, projects;
std::string known_id;
unsigned admissions{};
bool deny_admission{};

fs::path editor_projects_root() { return projects; }
fs::path resolve_epoch_repo_root(const fs::path&) { return repo; }
bool is_epoch_repo_root(const fs::path& path) { return path == repo; }
bool generated_project_source_edition_allowed(const fs::path&) { return true; }
void editor_refresh_project_profiles() {}
bool editor_find_project_profile(std::string_view id) { return id == known_id; }
std::optional<std::string> extract_json_string_field(std::string_view text, std::string_view field) {
    const auto key = text.find("\"" + std::string{field} + "\":");
    if (key == text.npos) return {};
    const auto start = text.find('"', key + field.size() + 3);
    if (start == text.npos) return {};
    const auto end = text.find('"', start + 1);
    if (end == text.npos) return {};
    return std::string{text.substr(start + 1, end - start - 1)};
}
EditorProjectAdmissionResult editor_admit_project_manifest(std::string_view path) {
    ++admissions;
    return {.succeeded = !deny_admission && fs::is_regular_file(path),
        .manifest_path = std::string{path}};
}

OWNERS

std::string read_bytes(const fs::path& path) {
    std::ifstream file{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{file}, {}};
}
int verify() {
    const auto selected = editor_bundled_project_examples();
    CHECK(selected.size() == 2u && selected[1].id == "PlatformerExample2D");
    CHECK(selected.front().display_name == "Basic AI-Enabled Calculator");
    const std::string id{selected.front().id};
    const auto scratch = fs::current_path() / ("fixture-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::create_directory(scratch));
    repo = scratch / "repo";
    projects = scratch / "mutable/Projects";
    const auto source = repo / "Engine/examples" / id;
    fs::create_directories(source.parent_path());
    fs::copy(fs::path{"EXAMPLE_DIR"}, source, fs::copy_options::recursive);
    fs::create_directory(source / "Library");
    std::ofstream{source / "Library/not-source.log"} << "generated";
    const auto destination = projects / id;
    CHECK(!editor_import_bundled_project_example("unknown").succeeded);
    known_id = id;
    CHECK(!editor_import_bundled_project_example(id).succeeded && !fs::exists(destination));
    known_id.clear();
    deny_admission = true;
    CHECK(!editor_import_bundled_project_example(id).succeeded && !fs::exists(destination));
    deny_admission = false;
    CHECK(editor_import_bundled_project_example(id).succeeded && admissions == 2u);
    CHECK(!fs::exists(destination / "Library") && !fs::exists(destination / "Artifacts"));
    CHECK(read_bytes(destination / "Assets/Gui/main.epochgui") == read_bytes(source / "Assets/Gui/main.epochgui"));
    for (const auto file : {"source/epoch.main.cpp", "source/project.gui_application.hpp",
            "source/calculator.arithmetic.hpp", "source/calculator.coach.hpp", "source/calculator.scientific.hpp",
            "source/calculator.coach_android.hpp", "source/calculator.coach_android_worker.cpp",
            "android/calculator.coach.cmake", "android/EpochCalculatorCoach.java", "android/EpochCalculatorCoachService.java",
            "android/EpochCalculatorCoachModelActivity.java",
            "android/verify_coach_source.py", "android/llama.cpp.LICENSE"}) {
        const auto bytes = read_bytes(source / file);
        CHECK(!bytes.empty() && bytes == read_bytes(destination / file));
    }
    CHECK(read_bytes(destination / "build_project.ps1").find("$repoRoot = '") != std::string::npos);
    CHECK(read_bytes(destination / "epoch.project.cmake").find(repo.generic_string()) != std::string::npos);
    CHECK(read_bytes(destination / "build_project.sh").find("\r") == std::string::npos);
    CHECK(read_bytes(destination / "BasicSoftwareExampleCalculator.vcxproj").find("<EpochRepoRoot>") != std::string::npos);
    std::ofstream{destination / "README.md"} << "user-owned edit";
    CHECK(!editor_import_bundled_project_example(id).succeeded && admissions == 2u);
    CHECK(read_bytes(destination / "README.md") == "user-owned edit");
    const std::string platformerId{selected[1].id};
    const auto platformerSource = source.parent_path() / platformerId;
    fs::copy(fs::path{"PLATFORMER_DIR"}, platformerSource, fs::copy_options::recursive);
    const auto platformerDestination = projects / platformerId;
    CHECK(editor_import_bundled_project_example(platformerId).succeeded && admissions == 3u);
    for (const auto file : {"Assets/Maps/main.epochmap", "Assets/Textures/starter_actor.epoch_texture",
            "Assets/Audio/default_jump.wav", "worlds/main.epoch", "project.epoch.json"}) {
        const auto bytes = read_bytes(platformerSource / file);
        CHECK(!bytes.empty() && bytes == read_bytes(platformerDestination / file));
    }
    CHECK(read_bytes(platformerDestination / "PlatformerExample2D.vcxproj").find("<EpochRepoRoot>") != std::string::npos);
    CHECK(!fs::exists(platformerDestination / "Library") && !fs::exists(platformerDestination / "Artifacts"));
    return 0;
}
}
int main() { return epochengine::verify(); }
'''.replace("DESCRIPTOR", descriptor).replace("HELPERS", helpers).replace("OWNERS", bodies
        ).replace("EXAMPLE_DIR", example.as_posix()).replace("PLATFORMER_DIR", platformer.as_posix())
        run_gui_contract(self, "bundled-project-example-import", code,
                         [scene, shell, registry, module, Path(__file__)],
                         "Real example file transaction and build binding, with manifest admission stubbed",
                         extra_modules=("platform.filesystem.ixx",), libraries=("ole32", "shell32"))
