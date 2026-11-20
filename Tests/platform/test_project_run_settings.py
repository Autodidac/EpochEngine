"""Execute project Run persistence with the real JSON and native file owners."""
from pathlib import Path
import hashlib
import json
import os
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run, dependency_include, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class ProjectRunSettings(unittest.TestCase):
    def test_android_save_requires_same_project_and_orders_writes(self):
        owner = ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl"
        play_owner = ROOT / "Engine/src/editor/application/editor.frame_project_runtime.inl"
        scene_owner = ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"
        source_text = owner.read_bytes()
        save = "\n".join(definition(source_text.decode("utf-8"), marker) for marker in (
            "        [[nodiscard]] static bool save_active_project_sources(",
            "        [[nodiscard]] static bool save_mobile_project_sources(",
            "        void repair_project_evidence("))
        play = definition(play_owner.read_text(encoding="utf-8"), "        auto play_active_context =", True)
        scene = scene_owner.read_text(encoding="utf-8")
        constructor = definition(scene, "            ProjectPlayScene(")
        entry_start = constructor.index("#if defined(__ANDROID__)")
        preview_entry = constructor[entry_start:constructor.index("#endif", entry_start) + len("#endif")]
        self.assertLess(constructor.index("prepare_project_gui(runSettings);"), entry_start)
        self.assertLess(constructor.index("if (m_guiApplication) return;"), entry_start)
        self.assertLess(entry_start, constructor.index("load_project_runtime_snapshot("))
        load = definition(scene, "            void load() override")
        self.assertLess(load.index("if (m_guiApplication || m_mobileAuthoredTool) return;"),
                        load.index("prepare_gameplay();"))
        body = r'''
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

struct ProjectProfile {
    std::string root_path = "files/epoch/Projects/Fixture";
    std::string manifest_path = root_path + "/project.epoch.json";
    std::string scene_path = root_path + "/worlds/main.epoch";
};
const ProjectProfile profile;
const ProjectProfile* editor_find_project_profile(std::string_view id)
{
    return id == "Fixture" ? &profile : nullptr;
}

struct EditorState {
    std::string projectId = "Fixture";
    std::string projectRoot = profile.root_path;
    std::string projectManifest = profile.manifest_path;
    std::string projectScenePath = profile.scene_path;
    std::string projectStatus;
    std::vector<std::string_view> events;
    bool guiOk = true;
    bool sceneOk = true;
    bool settingsOk = true;
    bool projectScreenSpaceGuiEnabled = true;
    bool activeGuiDocument = true;
    std::string projectKind = "tool";
    std::string projectName = "Fixture";
    std::string projectBuildStatus;
    int inputProfilePreset = 0;
    std::string guiEdit = "B";
    std::string savedGui = "A";
    std::string playedGui;
};

void push_editor_log(EditorState&, std::string_view) {}
void project_gui_document_layout(EditorState& editor)
{
    editor.events.push_back("layout");
}
bool save_project_gui_document(EditorState& editor, bool)
{
    editor.events.push_back("GUI");
    if (editor.guiOk)
        editor.savedGui = editor.guiEdit;
    return editor.guiOk;
}
bool save_editor_scene_snapshot(EditorState& editor)
{
    editor.events.push_back("scene");
    return editor.sceneOk;
}
bool persist_project_run_settings(EditorState& editor)
{
    editor.events.push_back("settings");
    if (!editor.settingsOk) editor.projectStatus = "Run settings save failed.";
    return editor.settingsOk;
}

#ifndef __ANDROID__
#define __ANDROID__ 1
#endif
@SAVE@

enum class EditorProjectKind { Tool, Game };
std::string_view editor_project_kind_name(EditorProjectKind) {return "tool";}
enum class EditorCommand { RunGame };
namespace project_planetary
{
    struct Requirement {bool ok = true, requires_project_runtime = false; std::string message;};
    Requirement runtime_requirement(const std::filesystem::path&) {return {};}
}
bool project_gui_document_is_active(const EditorState& editor) {return editor.activeGuiDocument;}
int project_camera_for_run(const EditorState&) {return 0;}
std::string project_runtime_scene_id(const EditorState& editor) {return "project:" + editor.projectId;}
std::string_view project_camera_argument(int) {return "canvas2d";}
std::string_view input_profile_argument(int) {return "default";}
void append_project_note(EditorState&, std::string_view, std::string_view, std::string_view) {}
void play_in_editor(EditorState& editor)
{
    constexpr bool projectScenePlayAvailable = true;
    constexpr auto projectMobilePlayMessage = "Play unavailable.";
    auto launch_active_project_context = [&](bool, unsigned, bool) {editor.events.push_back("child");};
    const auto emit_command = [&](EditorCommand, std::string_view id)
    {
        if (id.starts_with("project:Fixture|"))
            editor.playedGui = editor.savedGui;
        editor.events.push_back("play");
    };
@PLAY@
    play_active_context();
}

namespace previewgrid { enum class CameraMode { Editor, Canvas2D }; }

struct SceneEntry
{
    bool m_mobileAuthoredTool = false;
    previewgrid::CameraMode m_cameraMode = previewgrid::CameraMode::Editor;
    int gameplayLoads = 0;
    void open(bool m_canReturnToEditor, EditorProjectKind kind, std::string_view projectId)
    {
        const struct {std::string_view id; EditorProjectKind kind;} admitted{"Fixture", kind};
        const auto* profile = &admitted;
        const struct {std::string_view project_id;} launch{projectId};
@PREVIEW_ENTRY@
        ++gameplayLoads;
    }
};

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << "Failed: " << #__VA_ARGS__ << '\n'; return __LINE__; } } while (false)
int main()
{
    for (int mismatch = 0; mismatch < 7; ++mismatch)
    {
        EditorState editor;
        std::string_view requested = "Fixture";
        if (mismatch == 0) requested = {};
        if (mismatch == 1) requested = "Unknown";
        if (mismatch == 2) editor.projectId = "Other";
        if (mismatch == 3) editor.projectRoot = "files/epoch/Projects/Other";
        if (mismatch == 4) editor.projectManifest = "elsewhere/project.epoch.json";
        if (mismatch == 5) editor.projectScenePath = "elsewhere/main.epoch";
        if (mismatch == 6) editor.projectScenePath.clear();
        repair_project_evidence(editor, requested);
        CHECK(editor.events.empty() && !editor.projectStatus.empty());
    }

    EditorState saved;
    repair_project_evidence(saved, saved.projectId);
    CHECK(saved.events == std::vector<std::string_view>{"layout", "GUI", "scene", "settings"});
    CHECK(saved.projectStatus.starts_with("Active project saved"));

    EditorState guiFailed;
    guiFailed.guiOk = false;
    repair_project_evidence(guiFailed, guiFailed.projectId);
    CHECK(guiFailed.events == std::vector<std::string_view>{"layout", "GUI"});
    CHECK(guiFailed.projectStatus.starts_with("Project save blocked"));

    EditorState sceneFailed;
    sceneFailed.sceneOk = false;
    repair_project_evidence(sceneFailed, sceneFailed.projectId);
    CHECK(sceneFailed.events == std::vector<std::string_view>{"layout", "GUI", "scene"});
    CHECK(sceneFailed.projectStatus.starts_with("Project save blocked"));

    EditorState settingsFailed;
    settingsFailed.settingsOk = false;
    repair_project_evidence(settingsFailed, settingsFailed.projectId);
    CHECK(settingsFailed.events == saved.events);
    CHECK(settingsFailed.projectStatus == "Run settings save failed.");

    EditorState toolPlay;
    play_in_editor(toolPlay);
    CHECK(toolPlay.playedGui == "B");
    CHECK(toolPlay.events == std::vector<std::string_view>{"layout", "GUI", "scene", "settings", "play"});

    EditorState playGuiFailed;
    playGuiFailed.guiOk = false;
    play_in_editor(playGuiFailed);
    CHECK(playGuiFailed.events == std::vector<std::string_view>{"layout", "GUI"});
    CHECK(playGuiFailed.savedGui == "A" && playGuiFailed.playedGui.empty());

    EditorState playSceneFailed;
    playSceneFailed.sceneOk = false;
    play_in_editor(playSceneFailed);
    CHECK(playSceneFailed.events == std::vector<std::string_view>{"layout", "GUI", "scene"});
    CHECK(playSceneFailed.playedGui.empty());

    EditorState playSettingsFailed;
    playSettingsFailed.settingsOk = false;
    play_in_editor(playSettingsFailed);
    CHECK(playSettingsFailed.events == std::vector<std::string_view>{"layout", "GUI", "scene", "settings"});
    CHECK(playSettingsFailed.playedGui.empty());

    EditorState noGui;
    noGui.activeGuiDocument = false;
    play_in_editor(noGui);
    CHECK(noGui.events.empty() && !noGui.projectStatus.empty());

    EditorState disabledGui;
    disabledGui.projectScreenSpaceGuiEnabled = false;
    play_in_editor(disabledGui);
    CHECK(disabledGui.events.empty() && !disabledGui.projectStatus.empty());

    EditorState stalePlay;
    stalePlay.projectManifest.clear();
    play_in_editor(stalePlay);
    CHECK(stalePlay.events.empty() && stalePlay.playedGui.empty());

    EditorState gamePlay;
    gamePlay.projectKind = "game";
    play_in_editor(gamePlay);
    CHECK(gamePlay.events == std::vector<std::string_view>{"scene", "play"});

    SceneEntry emptyAuthoredTool;
    emptyAuthoredTool.open(true, EditorProjectKind::Tool, "Fixture");
    CHECK(emptyAuthoredTool.m_mobileAuthoredTool && emptyAuthoredTool.gameplayLoads == 0);
    CHECK(emptyAuthoredTool.m_cameraMode == previewgrid::CameraMode::Canvas2D);
    SceneEntry gameEntry, standalone, differentProject;
    gameEntry.open(true, EditorProjectKind::Game, "Fixture");
    standalone.open(false, EditorProjectKind::Tool, "Fixture");
    differentProject.open(true, EditorProjectKind::Tool, "Other");
    CHECK(!gameEntry.m_mobileAuthoredTool && gameEntry.gameplayLoads == 1);
    CHECK(!standalone.m_mobileAuthoredTool && standalone.gameplayLoads == 1);
    CHECK(!differentProject.m_mobileAuthoredTool && differentProject.gameplayLoads == 1);
}
'''.replace("@SAVE@", save).replace("@PLAY@", play).replace("@PREVIEW_ENTRY@", preview_entry)
        report = ROOT / "Artifacts/Reports/android-editor-save" / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        runs = compile_and_run(self, source, report)
        (report / "result.json").write_text(json.dumps({
            "passed": True,
            "runs": runs,
            "owner_sha256": hashlib.sha256(source_text).hexdigest(),
            "play_owner_sha256": hashlib.sha256(play_owner.read_bytes()).hexdigest(),
            "scene_owner_sha256": hashlib.sha256(scene_owner.read_bytes()).hexdigest(),
            "scope": "Extracted Android Save and Play dispatch and authored Tool preview entry with recording file and scene ports.",
        }, indent=2) + "\n", encoding="utf-8")

    def test_same_project_refresh_retains_bound_run_goal(self):
        owner = ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl"
        runtime_owner = ROOT / "Engine/src/editor/application/editor.frame_project_runtime.inl"
        reset_owner = ROOT / "Engine/src/editor/application/editor.application_project_documents.inl"
        repair = definition(owner.read_text(encoding="utf-8"), "        void repair_project_evidence(")
        capture_begin = repair.index("            const bool preservedToolGoalCurrent")
        captures = repair[capture_begin:repair.index("            const bool isSandbox", capture_begin)]
        restore = definition(repair, "                    if (preservedToolGoalCurrent")
        condition = repair[repair.index("                if (targetProject == previousProject"):]
        condition = condition[:condition.index("                {", 1)]
        reset = definition(reset_owner.read_text(encoding="utf-8"), "        void reset_project_bound_editor_state(")
        goal_reset = reset[reset.index("            state.aiGoalTools = {};"):reset.index("            state.aiAuthoringIgnoredThroughGeneration")]
        approval_reset = "\n".join(line for line in reset.splitlines()
                                   if "state.aiToolPlan = {};" in line or "state.aiToolPlanApplied = false;" in line)
        complete = definition(runtime_owner.read_text(encoding="utf-8"), "        const auto complete_tool_goal =", True)
        body = r'''
#include "editor.ai_chat_goal.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
using epochengine::editor::ai_chat_goal::ToolSequence;
struct EditorState {
 std::string projectId="new",projectRoot="Projects/new",projectScenePath="worlds/main.epoch";
 std::string aiGoal="Create a new planet with forest, save and run",aiGoalDraft=aiGoal;
 ToolSequence aiGoalTools;
 std::optional<bool> aiGoalPackageReceipt;
 bool aiGoalActive=true,aiGoalRunning=true,aiGoalEditing{},aiGoalPlanNextQueued{};
 std::size_t aiGoalCompletedMilestones{};
 std::vector<std::string> aiGoalAttemptedMilestoneSignatures;
 std::string aiGoalLastMilestoneSignature,aiGoalLastMilestoneReceipt="project.save accepted",aiToolStatus="runtime active";
 int aiToolPlan=17;bool aiToolPlanApplied=true;
 std::uint64_t sceneDocumentRevision{8};
 struct {std::uint64_t revision{1},contentHash{};} plantLabDocument;
 struct {
  struct {
   struct {
    struct {std::uint64_t generation{7};}selected_project;
    struct {std::uint64_t build_generation{11};}output_artifact;
   }generations;
  }data;
  const auto& evidence()const{return data;}
 }projectLifecycle;
};
void reset(EditorState& state){APPROVAL_RESET GOAL_RESET}
void refresh(EditorState& editor,bool sameId=true,bool sameRoot=true,bool sameScene=true,bool sameGeneration=true){
 const auto previousProject=editor.projectId,preservedProjectRoot=editor.projectRoot,preservedScenePath=editor.projectScenePath;
 const std::string targetProject=sameId?previousProject:"other";
 CAPTURES
 reset(editor);editor.projectId=targetProject;
 if(!sameRoot)editor.projectRoot="Projects/other";
 if(!sameScene)editor.projectScenePath="worlds/other.epoch";
 if(!sameGeneration)++editor.projectLifecycle.data.generations.selected_project.generation;
 CONDITION {RESTORE}
}
void finish(EditorState& editor,bool succeeded){COMPLETE complete_tool_goal("project.run",succeeded);}
EditorState ready(){
 EditorState editor;editor.aiGoalTools.enabled=true;editor.aiGoalTools.waiting=true;
 editor.aiGoalTools.bind_request(editor.aiGoal,editor.projectId,editor.projectRoot,7);
 (void)editor.aiGoalTools.receipt("project.create",true,editor.projectId,editor.projectRoot,7);
 editor.aiGoalTools.waiting=true;
 (void)editor.aiGoalTools.receipt("forest.paint",true,editor.projectId,editor.projectRoot,7);
 editor.aiGoalTools.waiting=true;
 (void)editor.aiGoalTools.receipt("project.save",true,editor.projectId,editor.projectRoot,7);
 editor.aiGoalCompletedMilestones=3;return editor;
}
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
int main(){
 auto active=ready();const auto objective=active.aiGoal;
 refresh(active);refresh(active); // Run and its prerequisite build both refresh the shell.
 CHECK(active.aiGoal==objective&&active.aiGoalDraft==objective&&active.aiGoalActive&&active.aiGoalRunning);
 CHECK(active.aiGoalTools.stage==3&&active.aiGoalTools.project_generation==7&&!active.aiGoalTools.waiting);
 CHECK(active.aiGoalCompletedMilestones==3&&active.aiGoalLastMilestoneReceipt=="project.save accepted");
 CHECK(active.aiToolPlan==0&&!active.aiToolPlanApplied&&!active.aiGoalPlanNextQueued);
 active.aiGoalTools.waiting=true;finish(active,true);
 CHECK(active.aiGoalTools.stage==4&&!active.aiGoalTools.stopped&&!active.aiGoalRunning&&!active.aiGoalPlanNextQueued);
 CHECK(active.aiGoalCompletedMilestones==4&&active.aiGoalLastMilestoneReceipt=="runtime active");
 auto failed=ready();refresh(failed);failed.aiGoalTools.waiting=true;finish(failed,false);
 CHECK(failed.aiGoalTools.stopped&&failed.aiGoalTools.stage==3&&!failed.aiGoalRunning);
 for(int changed=0;changed<4;++changed){auto other=ready();refresh(other,changed!=0,changed!=1,changed!=2,changed!=3);
  CHECK(!other.aiGoalActive&&!other.aiGoalRunning&&!other.aiGoalTools.enabled&&other.aiGoal.empty());
  CHECK(other.aiToolPlan==0&&!other.aiToolPlanApplied);}
 for(int invalid=0;invalid<7;++invalid){auto stale=ready();
  if(invalid==0)stale.aiGoalActive=false;
  if(invalid==1)stale.aiGoalRunning=false; // Pause/cancel must not be resumed by Save or Run.
  if(invalid==2)stale.aiGoalTools.stopped=true;
  if(invalid==3)stale.aiGoalTools.project_id="other";
  if(invalid==4)stale.aiGoalTools.project_root="Projects/other";
  if(invalid==5)--stale.aiGoalTools.project_generation;
  if(invalid==6){stale.aiGoalTools.stage=0;stale.aiGoalTools.project_id.clear();}
  refresh(stale);CHECK(!stale.aiGoalActive&&!stale.aiGoalRunning&&!stale.aiGoalTools.enabled);}
 auto interrupted=ready();refresh(interrupted);interrupted.aiGoalTools.waiting=true;
 interrupted.projectId="other";finish(interrupted,true);
 CHECK(interrupted.aiGoalTools.stopped&&interrupted.aiGoalTools.stage==3&&!interrupted.aiGoalRunning);
 auto queued=ready();queued.aiGoalTools.stage=2;queued.aiGoalPlanNextQueued=true;refresh(queued);
 CHECK(queued.aiGoalTools.stage==2&&queued.aiGoalPlanNextQueued); // Retain only reviewed goal progress, not a plan.
}
'''.replace("APPROVAL_RESET", approval_reset).replace("GOAL_RESET", goal_reset).replace("CAPTURES", captures).replace("CONDITION", condition).replace("RESTORE", restore).replace("COMPLETE", complete)
        identity = hashlib.sha256(body.encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/project-goal-refresh-nov06" / identity
        report.mkdir(parents=True, exist_ok=True)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        runs = compile_and_run(self, source, report, includes=[ROOT / "Engine/src/editor/ai"])
        (report / "result.json").write_text(json.dumps({"passed": True, "runs": runs,
            "sources": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in (owner, runtime_owner, reset_owner)},
            "scope": "Actual same-project goal capture/identity/restore, project reset and Run receipt owner with real ToolSequence: repeated materialization, successful and failed Run, pause/cancel, stale binding and project switch. File materialization ports only; no native runtime acceptance."}, indent=2) + "\n", encoding="utf-8")

    def test_planet_source_update_preserves_unsaved_draft(self):
        owner = (ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl").read_text(encoding="utf-8")
        poll = definition(owner, "        void poll_planetary_project_task(")
        begin = poll.index("                            const auto root = resolve_editor_path(")
        end = poll.index("                            if (editor.planetaryAttachmentIsInstall||", begin)
        # Execute the actual settings restoration block after the attachment's
        # project/root/generation check; file loading is the only recording port.
        restore = poll[begin:end]
        body = r'''
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
namespace project_planetary {
struct Settings {
 std::uint32_t seed=1337,body_index=0,size_reference_body=0,maximum_level=4;
 double reference_radius_meters=6371000;bool ocean_surface=false;
 bool operator==(const Settings&)const=default;
};
enum class DetailPreset{detailed};
inline std::optional<Settings> stored;inline unsigned loads{};
std::optional<Settings> load_settings(const std::filesystem::path&){++loads;return stored;}
std::optional<Settings> with_detail_preset(Settings value,DetailPreset){value.maximum_level=20;return value;}
bool supports_ocean_surface(const Settings&){return true;}
}
struct EditorState {
 std::string projectRoot="fixture",planetarySeedDraft="4294x",planetaryRadiusDraft="25000.";
 project_planetary::Settings planetarySettings{987,2,2,19,25000,true};
 double planetaryRadiusSynced=25000;unsigned planetaryRadiusReferenceSynced=2;
 bool planetaryAttachmentIsInstall=false,planetaryAttachmentIsUpdate=false;
};
std::filesystem::path resolve_editor_path(const std::filesystem::path& p){return p;}
void restore(EditorState& editor){RESTORE}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 project_planetary::stored=project_planetary::Settings{};
 EditorState update;update.planetaryAttachmentIsUpdate=true;
 const auto draft=update.planetarySettings;
 restore(update);
 CHECK(update.planetarySettings==draft&&update.planetarySeedDraft=="4294x");
 CHECK(update.planetaryRadiusDraft=="25000."&&update.planetaryRadiusSynced==25000&&update.planetaryRadiusReferenceSynced==2);
 CHECK(project_planetary::loads==0);
 EditorState reopen;restore(reopen);
 CHECK(reopen.planetarySettings==*project_planetary::stored&&reopen.planetarySeedDraft=="1337");
 CHECK(project_planetary::loads==1);
 project_planetary::stored.reset();
 EditorState install;install.planetaryAttachmentIsInstall=true;restore(install);
 CHECK(install.planetarySettings.reference_radius_meters==25000&&install.planetarySettings.maximum_level==20&&install.planetarySettings.ocean_surface);
 CHECK(install.planetarySeedDraft=="1337");
 EditorState missing;restore(missing);
 CHECK(missing.planetarySettings==project_planetary::Settings{}&&missing.planetarySeedDraft=="1337");
}
'''.replace("RESTORE", restore)
        report = ROOT / "Artifacts/Reports/terrain-settings-trace-nov01" / ("source-update-" + str(time.time_ns()))
        report.mkdir(parents=True)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        compile_and_run(self, source, report)

    def test_actual_persistence_readback_and_refusals(self):
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        core = (ROOT / "Engine/modules/editor/editor.core.ixx").read_text(encoding="utf-8")
        filesystem = (ROOT / "Engine/modules/platform/platform.filesystem.ixx").read_text(encoding="utf-8")
        value = definition(core, "    export struct EditorProjectRunSettings final", True).replace("export ", "", 1)
        camera_api = core.replace("launch_override = {}", "launch_override = std::nullopt")
        value += "\n" + "\n".join(definition(camera_api, marker).replace("export ", "", 1) for marker in (
            "    export [[nodiscard]] constexpr std::string_view editor_project_camera_token(",
            "    export [[nodiscard]] constexpr std::optional<previewgrid::CameraMode>\n        editor_project_camera_from_token(",
            "    export [[nodiscard]] constexpr previewgrid::CameraMode editor_resolve_project_run_camera(",
        ))
        legacy = (ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp").read_text(encoding="utf-8")
        launch = definition(legacy, "        struct ProjectSceneLaunchOptions", True) + "\n" + definition(
            legacy, "        [[nodiscard]] ProjectSceneLaunchOptions parse_project_scene_launch(")
        app = ROOT / "Engine/src/editor/application"
        runtime = (app / "editor.application_project_runtime.inl").read_text(encoding="utf-8")
        frame = (app / "editor.frame_project_runtime.inl").read_text(encoding="utf-8")
        scene_boot = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8")
        self.assertIn("projectBuildRunCameraMode = project_camera_for_run(editor)", runtime)
        self.assertIn("const auto playCameraMode = project_camera_for_run(editor)", frame)
        self.assertIn("runSettings ? runSettings->camera : std::nullopt, launch.camera_mode", scene_boot)
        codec = definition(registry, "    [[nodiscard]] static std::optional<nlohmann::ordered_json>\n        parse_project_build_configuration_manifest(")
        persistence = "\n".join(definition(scene, marker) for marker in (
            "    [[nodiscard]] static std::optional<std::string> read_project_build_configuration_manifest(",
            "    [[nodiscard]] static bool valid_project_run_settings(",
            "    std::optional<EditorProjectRunSettings> editor_project_run_settings(",
            "    bool editor_set_project_run_settings("))
        files = "\n".join(definition(filesystem, marker) for marker in (
            "    [[nodiscard]] inline bool exclusive_create_and_write(",
            "    [[nodiscard]] inline bool atomic_replace_same_filesystem("))
        body = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <crtdbg.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
#undef assert
#define assert(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#__VA_ARGS__);std::exit(91);}}while(false)
namespace fs=std::filesystem;
namespace platform::filesystem {@FILES@}
namespace updater::distribution {constexpr std::string_view edition_id="epoch.development.site.v1";}
namespace epochengine::previewgrid {enum class CameraMode{Editor,FPS,Canvas2D};}
namespace previewgrid=epochengine::previewgrid;
namespace input {enum class ProfilePreset{EditorDefault};ProfilePreset profile_preset_from_id(std::string_view){return ProfilePreset::EditorDefault;}}
enum class EditorProjectKind{Game,Tool,Editor};
@VALUE@
@LAUNCH@
inline bool admitted=true;
const bool* find_project_profile_by_root(std::string_view){return admitted?&admitted:nullptr;}
fs::path resolve_project_root_path(const fs::path& p){return p.empty()?fs::path{}:fs::absolute(p).lexically_normal();}
std::string read_text_file(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>{f},{}};}
void write(const fs::path& p,std::string_view s){std::ofstream f(p,std::ios::binary);f<<s;assert(f);}
@CODEC@
@PERSISTENCE@
int main(int argc,char**argv) try {
#ifdef _WIN32
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
 assert(argc==2);const fs::path root=argv[1];fs::create_directories(root);
 const auto manifest=root/"project.epoch.json";
 const std::string original=R"({"id":"Fixture","engine_edition":"epoch.development.site.v1","custom":{"retain":[1,2,3]},"build_configuration":"Release"})";
 write(manifest,original);std::string diagnostic;
 assert(editor_project_run_settings(root.string())==EditorProjectRunSettings{});
 assert(editor_project_run_settings(root.string())->frame_limit_fps == 0.0);
 using enum previewgrid::CameraMode;
 const auto direct=parse_project_scene_launch("Fixture");
 const auto explicitOrbit=parse_project_scene_launch("Fixture|camera=editor");
 const auto explicitFps=parse_project_scene_launch("Fixture|input=default|camera=first-person");
 const auto explicit2D=parse_project_scene_launch("Fixture|camera=2d-canvas");
 assert(direct.project_id=="Fixture"&&!direct.camera_mode);
 assert(explicitOrbit.camera_mode==Editor&&explicitFps.camera_mode==FPS&&explicit2D.camera_mode==Canvas2D);
 assert(parse_project_scene_launch("Fixture|camera=invalid").camera_mode==Editor);
 assert(editor_resolve_project_run_camera(EditorProjectKind::Game,true,{},direct.camera_mode)==Canvas2D);
 assert(editor_resolve_project_run_camera(EditorProjectKind::Game,false,{},direct.camera_mode)==Editor);
 assert(editor_resolve_project_run_camera(EditorProjectKind::Tool,true,{},direct.camera_mode)==Editor);
 assert(editor_resolve_project_run_camera(EditorProjectKind::Game,true,FPS,direct.camera_mode)==FPS);
 assert(editor_resolve_project_run_camera(EditorProjectKind::Game,true,FPS,explicitOrbit.camera_mode)==Editor);
 assert(editor_resolve_project_run_camera(EditorProjectKind::Game,true,Editor,explicit2D.camera_mode)==Canvas2D);
 const EditorProjectRunSettings requested{"vulkan",120.0,false,Canvas2D};
 assert(editor_set_project_run_settings(root.string(),requested,diagnostic));
 assert(editor_project_run_settings(root.string())==requested);
 const auto saved=read_text_file(manifest);
 const auto document=nlohmann::ordered_json::parse(saved);
 assert(document["custom"]==nlohmann::ordered_json::parse(original)["custom"]&&document["build_configuration"]=="Release");
 assert(document["screen_space_gui_enabled"]==false&&document["run_camera"]=="canvas2d");
 assert(editor_set_project_run_settings(root.string(),requested,diagnostic)&&read_text_file(manifest)==saved);
 for(const auto fps:{0.0,30.0,120.0,1500.0,5000.0,20000.0}){
  const EditorProjectRunSettings manual{"opengl",fps};
  assert(editor_set_project_run_settings(root.string(),manual,diagnostic)&&editor_project_run_settings(root.string())==manual);
  assert(!nlohmann::ordered_json::parse(read_text_file(manifest)).contains("run_camera"));
 }
 for(const bool enabled:{false,true,false}){
  const EditorProjectRunSettings guiSettings{"opengl",60.0,enabled};
  assert(editor_set_project_run_settings(root.string(),guiSettings,diagnostic));
  assert(editor_project_run_settings(root.string())==guiSettings);
 }
 write(manifest,saved);admitted=false;
 assert(!editor_set_project_run_settings(root.string(),{"sfml",60},diagnostic)&&read_text_file(manifest)==saved);admitted=true;
 for(const auto invalid:{EditorProjectRunSettings{"unknown",120},EditorProjectRunSettings{"opengl",-1},EditorProjectRunSettings{"opengl",std::numeric_limits<double>::infinity()}})
  assert(!editor_set_project_run_settings(root.string(),invalid,diagnostic)&&read_text_file(manifest)==saved);
 for(const auto malformed:{R"({"run_backend":1})",R"({"run_backend":"bogus"})",R"({"run_frame_limit_fps":-1})",R"({"run_frame_limit_fps":"120"})",R"({"run_backend":"opengl","run_backend":"vulkan"})",R"({"screen_space_gui_enabled":0})",R"({"screen_space_gui_enabled":"false"})",R"({"screen_space_gui_enabled":null})",R"({"screen_space_gui_enabled":false,"screen_space_gui_enabled":true})",R"({"run_camera":1})",R"({"run_camera":"2d"})",R"({"run_camera":"mixed"})",R"({"run_camera":"editor","run_camera":"fps"})","{broken"}){
  write(manifest,malformed);assert(!editor_project_run_settings(root.string()));
 }
 auto foreign=document;foreign["engine_edition"]="foreign";write(manifest,foreign.dump());
 const auto foreignBytes=read_text_file(manifest);
 assert(!editor_set_project_run_settings(root.string(),requested,diagnostic)&&read_text_file(manifest)==foreignBytes);
 write(manifest,saved);const auto temporary=root/"project.epoch.json.run-settings.write";write(temporary,"other owner");
 assert(!editor_set_project_run_settings(root.string(),{"sfml",60},diagnostic));
 assert(read_text_file(manifest)==saved&&read_text_file(temporary)=="other owner");fs::remove(temporary);
 const auto linked=root/"aliased.json";std::error_code linkError;fs::create_hard_link(manifest,linked,linkError);
 if(!linkError){
  assert(!editor_project_run_settings(root.string())&&!editor_set_project_run_settings(root.string(),requested,diagnostic));
  assert(read_text_file(manifest)==saved);fs::remove(linked);
 }
 std::ofstream(root/"result.json")<<"{\"run_roundtrip\":true,\"saved_camera_roundtrip\":true,\"automatic_camera_restored\":true,\"launch_override_precedence\":true,\"unknown_fields_retained\":true,\"busy_refused\":true,\"edition_refused\":true,\"alias_test_available\":"<<(!linkError?"true":"false")<<"}";
 assert(editor_project_run_settings(root.string())==requested);
} catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 92;}
'''.replace("@FILES@", files).replace("@VALUE@", value).replace("@LAUNCH@", launch).replace("@CODEC@", codec).replace("@PERSISTENCE@", persistence)
        report = ROOT / "Artifacts/Reports/project-settings-save-nov01" / ("cpu-" + str(time.time_ns()))
        report.mkdir(parents=True)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        includes = [dependency_include(ROOT)]
        if os.name == "nt":
            sdk = Path("C:/Program Files (x86)/Windows Kits/10")
            kit = windows_sdk_includes(sdk)[-1]
            includes += [kit / "um", kit / "shared"]
        compile_and_run(self, source, report, args=lambda mode: [report / "data" / mode], includes=includes)

    def test_actual_same_project_restore_and_planet_save_verdict(self):
        owner = (ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl").read_text(encoding="utf-8")
        repair = definition(owner, "        void repair_project_evidence(")
        # Execute the exact retained fields and matching-identity branch from
        # Save Project. The unrelated scene/GUI materialization is a CPU port.
        def statements(predicate):
            selected = []
            for line in repair.splitlines():
                if predicate(line):
                    start = repair.index(line)
                    selected.append(repair[start:repair.index(";", start)+1])
            return "\n".join(selected)
        captures = statements(lambda line: "const auto preservedPlanetary" in line
                              or "const auto preservedProjectFrameLimitEdit" in line
                              or "const auto preservedForestTerrainPainting" in line)
        writes = statements(lambda line: ("editor.planetary" in line and "= preservedPlanetary" in line)
                            or "editor.projectFrameLimitEdit = preservedProjectFrameLimitEdit" in line
                            or "editor.forestTerrainPainting = preservedForestTerrainPainting" in line)
        condition = repair[repair.index("                if (targetProject == previousProject"):]
        condition = condition[:condition.index("                {", 1)]
        save = definition(owner, "        void save_admitted_planetary_settings(")
        reset_owner = (ROOT / "Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8")
        reset = definition(reset_owner, "        void reset_project_bound_editor_state(")
        forest_reset = reset[reset.index("            state.forestSurfacePlacement.reset();"):reset.index("            state.planetaryCheckedRoot.clear();")]
        body = r'''
#include <cassert>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace scene{constexpr unsigned kInvalidSceneObjectId=0;}
struct EditorState {
 std::string projectRoot="one",projectScenePath="worlds/main.epoch",projectId="one",planetaryStatus;
 std::string planetarySeedDraft="987",planetaryRadiusDraft="25000.",planetaryCheckedRoot="one";
 int planetarySettings=99,planetaryAttachment=7,planetaryMeshPolicy=8,planetaryBodySelection=9;
 double planetaryRadiusSynced=25000;unsigned planetaryRadiusReferenceSynced=2,refreshes{};
 int projectFrameLimitEdit=4;
 bool planetaryInitialFramePending{};std::uint64_t planetaryInitialFrameGeneration{7};
 bool forestTerrainPainting=true,forestBrushHeld=true,forestPlacementFocusPending=true;
 unsigned forestBrushTerrain=9;
 std::vector<std::array<double,3>> forestBrushDirections{{1.0,0.0,0.0}};
 std::optional<int> forestSurfacePlacement{1},previousSceneClick{2};
 std::optional<int> forestLassoGesture{1},selectedForestLasso{2};
 std::vector<int> forestTerrainRayCache{3};
 std::optional<bool> forestPlacementSaved{true},projectRunAfterTerrainPreview{true};
 struct {struct {struct {struct {std::uint64_t generation{7};}selected_project;}generations;} data;
  const auto& evidence()const{return data;}}projectLifecycle;
};
void reset(EditorState& state){state.planetarySettings=state.planetaryAttachment=state.planetaryMeshPolicy=state.planetaryBodySelection=0;
 FOREST_RESET
 state.planetarySeedDraft.clear();state.planetaryRadiusDraft.clear();state.planetaryCheckedRoot.clear();state.planetaryRadiusSynced=0;state.planetaryRadiusReferenceSynced=0;state.projectFrameLimitEdit=0;
 ++state.projectLifecycle.data.generations.selected_project.generation;
 state.planetaryInitialFramePending=true;state.planetaryInitialFrameGeneration=state.projectLifecycle.data.generations.selected_project.generation;}
void save_refresh(EditorState& editor,bool sameId,bool sameRoot,bool sameScene){
 const auto previousProject=editor.projectId,preservedProjectRoot=editor.projectRoot,preservedScenePath=editor.projectScenePath;
 const std::string targetProject=sameId?previousProject:"another";
 CAPTURES
 reset(editor);if(!sameRoot)editor.projectRoot="another";if(!sameScene)editor.projectScenePath="different.epoch";
 CONDITION {WRITES}
}
namespace project_planetary {inline bool saveOk=true,readOk=true;inline int stored{};inline unsigned saves{};
 bool save_settings(const std::filesystem::path&,int value){++saves;if(saveOk)stored=value;return saveOk;}
 std::optional<int> load_settings(const std::filesystem::path&){return readOk?std::optional{stored}:std::nullopt;}}
std::filesystem::path resolve_editor_path(const std::filesystem::path& p){return p;}
void refresh_editor_planetary_mesh_policy(EditorState& state){++state.refreshes;}
SAVE
int main(){
 EditorState state;save_refresh(state,true,true,true);
 assert(state.planetarySettings==99&&state.planetaryAttachment==7&&state.planetaryMeshPolicy==8&&state.planetaryBodySelection==9);
 assert(state.planetarySeedDraft=="987"&&state.planetaryRadiusDraft=="25000."&&state.planetaryCheckedRoot=="one");
 assert(state.planetaryRadiusSynced==25000&&state.planetaryRadiusReferenceSynced==2);
 assert(state.projectFrameLimitEdit==4);
 // Native regression: same-project Save must not introduce the initial orbit
 // request that later replaces a focused tree camera and navigation origin.
 #define CHECK(value) do{if(!(value))return __LINE__;}while(false)
 CHECK(!state.planetaryInitialFramePending&&state.planetaryInitialFrameGeneration==0);
 CHECK(state.forestTerrainPainting&&!state.forestBrushHeld&&!state.forestPlacementFocusPending);
 CHECK(!state.forestSurfacePlacement&&state.forestBrushDirections.empty()&&state.forestBrushTerrain==0);
 CHECK(!state.forestPlacementSaved&&!state.projectRunAfterTerrainPreview&&!state.previousSceneClick);
 EditorState plant;plant.forestTerrainPainting=false;save_refresh(plant,true,true,true);
 CHECK(!plant.forestTerrainPainting&&!plant.forestBrushHeld&&!plant.forestSurfacePlacement);
 for(const bool pending:{false,true}){
  EditorState view;view.planetaryInitialFramePending=pending;
  save_refresh(view,true,true,true);
  CHECK(view.planetaryInitialFramePending==pending);
  CHECK(view.planetaryInitialFrameGeneration==(pending?8u:0u));
 }
 EditorState stale;stale.planetaryInitialFramePending=true;stale.planetaryInitialFrameGeneration=6;
 save_refresh(stale,true,true,true);CHECK(!stale.planetaryInitialFramePending&&stale.planetaryInitialFrameGeneration==0);
 for(int changed=0;changed<3;++changed){EditorState other;save_refresh(other,changed!=0,changed!=1,changed!=2);
  assert(other.planetarySettings==0&&other.planetarySeedDraft.empty()&&other.planetaryRadiusDraft.empty());
  CHECK(other.planetaryInitialFramePending&&other.planetaryInitialFrameGeneration==8);}
 for(int changed=0;changed<3;++changed){EditorState other;save_refresh(other,changed!=0,changed!=1,changed!=2);
  CHECK(!other.forestTerrainPainting&&!other.forestBrushHeld&&!other.forestSurfacePlacement);}
 save_admitted_planetary_settings(state);assert(state.refreshes==1&&state.planetaryStatus.find("settings saved")!=std::string::npos);
 project_planetary::saveOk=false;save_admitted_planetary_settings(state);
 assert(state.refreshes==1&&state.planetarySettings==99&&state.planetaryStatus.find("Could not save")!=std::string::npos);
 project_planetary::saveOk=true;project_planetary::readOk=false;save_admitted_planetary_settings(state);
 assert(state.refreshes==1&&state.planetarySettings==99&&state.planetaryStatus.find("Could not save")!=std::string::npos);
}
'''.replace("CAPTURES", captures).replace("CONDITION", condition).replace("WRITES", writes).replace("SAVE", save).replace("FOREST_RESET", forest_reset)
        report = ROOT / "Artifacts/Reports/project-settings-save-nov01" / ("restore-" + str(time.time_ns()))
        report.mkdir(parents=True)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        runs = compile_and_run(self, source, report)
        (report / "result.json").write_text(json.dumps({"passed": True, "runs": runs,
            "owner_sha256": hashlib.sha256((ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl").read_bytes()).hexdigest(),
            "scope": "Actual same-project Save capture/identity/restore statements: no new initial orbit request, pending current request rebound, stale generation discarded; Forest terrain-paint view survives while actual project-reset statements discard brush gestures/surface intent. Project/root/scene switches retain normal open framing and clear Forest mode. Scene/GUI materialization ports only; no native camera or GPU claim."}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
