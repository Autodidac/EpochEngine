"""Execute project Run persistence with the real JSON and native file owners."""
from pathlib import Path
import hashlib
import json
import os
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run, dependency_include

ROOT = Path(__file__).resolve().parents[2]


class ProjectRunSettings(unittest.TestCase):
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
 struct {struct {struct {struct {std::uint64_t generation{7};}selected_project;}generations;}data;
  const auto& evidence()const{return data;}}projectLifecycle;
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
        core = (ROOT / "Engine/modules/editor.core.ixx").read_text(encoding="utf-8")
        filesystem = (ROOT / "Engine/modules/platform.filesystem.ixx").read_text(encoding="utf-8")
        value = definition(core, "    export struct EditorProjectRunSettings final", True).replace("export ", "", 1)
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
@VALUE@
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
 const EditorProjectRunSettings requested{"vulkan",120.0};
 assert(editor_set_project_run_settings(root.string(),requested,diagnostic));
 assert(editor_project_run_settings(root.string())==requested);
 const auto saved=read_text_file(manifest);
 const auto document=nlohmann::ordered_json::parse(saved);
 assert(document["custom"]==nlohmann::ordered_json::parse(original)["custom"]&&document["build_configuration"]=="Release");
 assert(editor_set_project_run_settings(root.string(),requested,diagnostic)&&read_text_file(manifest)==saved);
 for(const auto fps:{0.0,30.0,120.0,1500.0,5000.0,20000.0}){
  const EditorProjectRunSettings manual{"opengl",fps};
  assert(editor_set_project_run_settings(root.string(),manual,diagnostic)&&editor_project_run_settings(root.string())==manual);
 }
 write(manifest,saved);admitted=false;
 assert(!editor_set_project_run_settings(root.string(),{"sfml",60},diagnostic)&&read_text_file(manifest)==saved);admitted=true;
 for(const auto invalid:{EditorProjectRunSettings{"unknown",120},EditorProjectRunSettings{"opengl",-1},EditorProjectRunSettings{"opengl",std::numeric_limits<double>::infinity()}})
  assert(!editor_set_project_run_settings(root.string(),invalid,diagnostic)&&read_text_file(manifest)==saved);
 for(const auto malformed:{R"({"run_backend":1})",R"({"run_backend":"bogus"})",R"({"run_frame_limit_fps":-1})",R"({"run_frame_limit_fps":"120"})",R"({"run_backend":"opengl","run_backend":"vulkan"})","{broken"}){
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
 std::ofstream(root/"result.json")<<"{\"run_roundtrip\":true,\"unknown_fields_retained\":true,\"busy_refused\":true,\"edition_refused\":true,\"alias_test_available\":"<<(!linkError?"true":"false")<<"}";
 assert(editor_project_run_settings(root.string())==requested);
} catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 92;}
'''.replace("@FILES@", files).replace("@VALUE@", value).replace("@CODEC@", codec).replace("@PERSISTENCE@", persistence)
        report = ROOT / "Artifacts/Reports/project-settings-save-nov01" / ("cpu-" + str(time.time_ns()))
        report.mkdir(parents=True)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        includes = [dependency_include(ROOT)]
        if os.name == "nt":
            sdk = Path("C:/Program Files (x86)/Windows Kits/10/Include")
            kit = sorted(sdk.glob("*"))[-1]
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
 std::optional<int> forestSurfacePlacement{1},forestBrushLastDirection{2};
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
 CHECK(!state.forestSurfacePlacement&&!state.forestBrushLastDirection&&state.forestBrushTerrain==0);
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
