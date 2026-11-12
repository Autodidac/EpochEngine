"""Actual renderer summary, pending-label and World/Project control fragments.

Rendering, project launch and capability assessment are labeled CPU ports; this
checks presentation/dispatch, not a renderer or backend-capability upgrade.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
APPLICATION = ROOT / "Engine/src/editor/application"


class ProjectRuntimePresentation(unittest.TestCase):
    def test_actual_feature_summary_and_busy_run_controls_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU fixture")
        paths = [APPLICATION / name for name in ("editor.application_scripting_renderers.inl",
            "editor.application_project_runtime.inl", "editor.frame_viewport.inl")]
        renderer, runtime, viewport = [p.read_text(encoding="utf-8") for p in paths]
        capability = (ROOT / "Engine/modules/capability/capability.profile.ixx").read_text(encoding="utf-8")
        api = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
        types = "\n".join(definition(capability, marker, True) for marker in (
            "    enum class BackendFamily", "    enum class Tier", "    enum class Feature",
            "    enum class MatchFailure", "    enum class AdmissionStatus"))
        names = "\n".join(definition(capability, marker) for marker in (
            "    [[nodiscard]] constexpr const char* tier_code(",
            "    [[nodiscard]] constexpr const char* backend_code(",
            "    [[nodiscard]] constexpr const char* admission_status_label(",
            "    [[nodiscard]] constexpr const char* match_failure_label("))
        gui_types = "\n".join(definition(api, marker, True).replace("export ", "", 1) for marker in (
            "    export struct SegmentedButtonSpec", "    export enum class ButtonIcon", "    export struct InlineButtonSpec", "    export enum class InlineButtonSizing",
            "    export enum class InlineButtonAlignment", "    export struct InlineButtonRowOptions"))
        summary = "\n".join(definition(renderer, marker) for marker in (
            "        [[nodiscard]] std::string renderer_feature_proof_names(",
            "        [[nodiscard]] std::string renderer_admission_summary("))
        label = definition(runtime, "        [[nodiscard]] std::string project_build_activity_label(")
        run_helpers = '\n'.join(definition(runtime, marker) for marker in (
            '        [[nodiscard]] unsigned scene_run_mode_index(',
            '        [[nodiscard]] std::string project_scene_run_button_label(',
            '        void select_scene_run_mode(', '        void exit_project_runtime_full_screen('))
        world = definition(viewport, "            if (showSceneRun)")
        show_run_output = definition(viewport, "        const auto show_project_run_output =", True)
        chrome = (ROOT / 'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
        measure = '\n'.join(definition(chrome, marker) for marker in (
            '    [[nodiscard]] static float segmented_button_width(',
            '    float segmented_button_row_width('))
        width_start = viewport.index('            const float runButtonWidth =')
        run_width = viewport[width_start:viewport.index(';', width_start)+1]
        pane_routing=(APPLICATION/'editor.application_pane_routing.inl').read_text(encoding='utf-8')
        record_output=definition(pane_routing,'        static void record_editor_run_output(')
        begin = viewport.rfind("if (editor.projectBuildPending)", 0, viewport.index("const std::array busyPlay"))
        play = viewport[begin:viewport.index("// Keep both controls", begin)]
        cpp = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)
namespace epochengine {
enum class RendererBackendKind {opengl,vulkan};
namespace capability {
CAPABILITY_TYPES
using FeatureMask=std::uint64_t;
constexpr FeatureMask feature_bit(Feature f){return FeatureMask{1}<<static_cast<unsigned>(f);}
CAPABILITY_NAMES
struct Profile{Tier tier;BackendFamily backend;};
struct SubsystemProfile{Profile capability;};
struct RequirementAdmission{AdmissionStatus status{AdmissionStatus::rejected};MatchFailure failure{MatchFailure::missing_feature};FeatureMask missing_features{},provisional_features{};};
SubsystemProfile renderer_subsystem_profile_for(RendererBackendKind kind){return kind==RendererBackendKind::vulkan
    ?SubsystemProfile{{Tier::explicit_api,BackendFamily::vulkan}}:SubsystemProfile{{Tier::portable_graphics,BackendFamily::opengl}};}
}}
using namespace epochengine;
struct EditorProjectCapabilityPolicy{std::string_view id="portable";};
capability::RequirementAdmission admission_port;
capability::RequirementAdmission renderer_admission(RendererBackendKind,const EditorProjectCapabilityPolicy&){return admission_port;}
SUMMARY
struct Handle{bool live{};bool valid()const{return live;}};
enum class EditorLayoutDrag{None};
enum class EditorWorkspaceTab{Output,Project};
struct EditorState{bool projectBuildPending{},projectRuntimeInEditor{},sceneRunFullscreenInEditor{},sceneRunMaximizedInEditor{},projectRuntimeViewportMaximized{};
std::optional<bool> projectRunAfterTerrainPreview;
bool projectRuntimeForceStopAvailable{},outputGeneralOnly{},outputShowDetails{},outputFollowTail{};
std::uint64_t outputScrollToEndGeneration{};
std::string outputSearch,outputLastRunDetail;EditorWorkspaceTab dockStatusTab{EditorWorkspaceTab::Project};
EditorLayoutDrag layoutDrag{};int surfaceSettleFrames{};
std::uintptr_t projectRuntimeWindow{};std::string projectRuntimeStatus,projectRuntimeAttachmentError;
Handle projectRuntimeProcess;std::string projectKind="game",projectBuildStatus="Building project (7s elapsed). Cancel is available.",projectBuildConfiguration="Debug",projectBuildRequestedConfiguration,projectRuntimeBuildConfiguration;};
ACTIVITY_LABEL
RUN_HELPERS
std::vector<std::string>run_details;
void push_editor_log(EditorState&,std::string line){run_details.push_back(std::move(line));}
RECORD_OUTPUT
unsigned output_requests{};
unsigned pane_dock_region(std::string_view route){return route=="pane.output"?3u:0u;}
void request_pane_region(std::string_view route,std::string_view label,unsigned region){if(route=="pane.output"&&label=="Output"&&region==3u)++output_requests;}
namespace gui {
struct Vec2{float x{},y{};};
GUI_TYPES
constexpr float kFontScale=1,kContentPadding=6,kButtonTextClipInset=2;
float measure_text_width(std::string_view text,float){return float(text.size())*7.f;}
float segmented_button_row_width(std::span<const SegmentedButtonSpec>,float=6.f) noexcept;
MEASURE
struct SelectBoxOptions{std::string_view id,placeholder,selected;std::span<const std::string_view> options;Vec2 size;float row_height{};unsigned max_visible_options{};bool enabled{};};
struct Selection{bool changed{},opened{};std::optional<std::size_t> selected_index;};
std::vector<std::string> buttons,labels;std::vector<bool> enabled;
Vec2 panel_position,panel_size;
struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};
void set_cursor(Vec2){}
float line_height(){return 18.f;}
void panel_rect(Vec2 p,Vec2 s){panel_position=p;panel_size=s;}
float measure_wrapped_label_height(std::string_view s,float width){return 18.f*std::ceil(float(s.size())*7.f/width);}
void wrapped_label(std::string_view s,float){labels.emplace_back(s);}
bool click_output{};
bool button(std::string_view s,Vec2){buttons.emplace_back(s);return s=="Show Output"?click_output:true;}
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> items,float,float,const InlineButtonRowOptions&){
for(const auto&i:items){buttons.emplace_back(i.label);enabled.push_back(i.enabled);}return 0u;}
Selection select_box(const SelectBoxOptions&){return {};}
void fresh(){buttons.clear();labels.clear();enabled.clear();panel_size={};}
}
namespace platform::child_process {enum class StopMode{graceful};}
unsigned runs{},plays{},stops{},output_requests_at_launch{};
bool queue_on_run{};EditorState* run_editor{};
bool stop_project_runtime(EditorState&,platform::child_process::StopMode){++stops;return true;}
void play_active_context(bool=false){++plays;output_requests_at_launch=output_requests;}
void launch_active_project_context(bool=false,std::uint64_t=0,bool=false){
 ++runs;output_requests_at_launch=output_requests;
 if(queue_on_run&&run_editor)run_editor->projectBuildPending=true;}
void world_controls(EditorState& editor,float width,float height,bool modal=false){
run_editor=&editor;
const auto projectBuildActivity=project_build_activity_label(editor);
const gui::Vec2 scene_pos{20,40},scene_size{width,height};
const float runModeWidth=120.f;const bool showSceneRun=true;
const float sceneNoticeTop=26.f;
const bool compiledProjectRunning=editor.projectRuntimeInEditor&&editor.projectRuntimeProcess.valid();
const bool projectViewportMaximized=editor.projectRuntimeViewportMaximized;
const auto sceneRunButtonLabel=project_scene_run_button_label(editor,compiledProjectRunning);
RUN_WIDTH
const float runControlOffset=projectViewportMaximized?8.f:96.f;
const bool menusBlockedByModal=modal,toolbarControlsBlockedByMenu=false;
const bool automationInputReserved=false;
std::optional<int> automationBackgroundInput;
const std::array<std::string_view,3> kSceneRunModes{"Separate Window","In Editor","In-Editor Full Screen"};
struct{bool scene_input_captured{};}result;
SHOW_RUN_OUTPUT
WORLD
}
void project_controls(EditorState& editor,float centerWidth){
const auto projectBuildActivity=project_build_activity_label(editor);
SHOW_RUN_OUTPUT
PROJECT_PLAY
}
int main(){
using namespace capability;
admission_port.missing_features=feature_bit(Feature::presentation);
const auto vk=renderer_admission_summary(RendererBackendKind::vulkan,{});
CHECK(vk.find("T2-VK")!=vk.npos&&vk.find("Not validated: presentation proof")!=vk.npos);
CHECK(vk.find("missing feature")==vk.npos);
admission_port.missing_features=feature_bit(Feature::textures)|feature_bit(Feature::render_targets);
CHECK(renderer_admission_summary(RendererBackendKind::opengl,{}).find("textures, render targets")!=std::string::npos);
admission_port={AdmissionStatus::experimental,MatchFailure::none,0,feature_bit(Feature::resource_residency)};
CHECK(renderer_admission_summary(RendererBackendKind::opengl,{}).find("Partial proof: resource residency")!=std::string::npos);
admission_port={AdmissionStatus::admitted,MatchFailure::none,0,0};
CHECK(renderer_admission_summary(RendererBackendKind::opengl,{}).find("Admitted")!=std::string::npos);
for(unsigned f=0;f<static_cast<unsigned>(Feature::count);++f)CHECK(!renderer_feature_proof_names(FeatureMask{1}<<f).empty());
EditorState e;CHECK(project_build_activity_label(e).empty());
e.projectBuildPending=true;e.projectBuildRequestedConfiguration="Release";
CHECK(project_build_activity_label(e)=="Building Release...");
e.projectBuildStatus="Cancelling project build; waiting for owned workers to retire.";
CHECK(project_build_activity_label(e)=="Cancelling Release...");
e.projectBuildStatus="Finishing project build; checking its result.";
CHECK(project_build_activity_label(e)=="Finishing Release...");
e.projectBuildRequestedConfiguration.clear();e.projectBuildStatus="Building project (7s elapsed). Cancel is available.";
for(const float width:{260.f,900.f})for(const float height:{30.f,400.f}){
gui::fresh();world_controls(e,width,height);
CHECK(runs==0&&plays==0&&stops==0);
CHECK(gui::buttons.size()==2&&gui::buttons.front()=="Building..."&&!gui::enabled.front());
CHECK(gui::labels.size()==1&&gui::labels.front()=="Building Debug...");
CHECK(std::count(gui::buttons.begin(),gui::buttons.end(),"Building Debug...")==0);
CHECK(std::count(gui::buttons.begin(),gui::buttons.end(),"Show Output")==1);
CHECK(!run_details.empty()&&run_details.back().find(e.projectBuildStatus)!=std::string::npos);
CHECK(gui::panel_position.x>=20&&gui::panel_position.y>=40&&gui::panel_position.x+gui::panel_size.x<=20+width);
CHECK(gui::panel_position.y+gui::panel_size.y<=40+height);
if(width==900.f)CHECK(gui::panel_size.x<400.f&&gui::panel_size.y<=24.f);
gui::fresh();project_controls(e,width);CHECK(plays==0&&gui::buttons.front()=="Building Debug..."&&!gui::enabled.front());
}
CHECK(output_requests==0);
e.projectBuildPending=false;e.outputSearch="old query";e.outputShowDetails=true;e.outputGeneralOnly=true;
gui::fresh();world_controls(e,900,400);CHECK(runs==1&&gui::buttons.front()=="Run");
CHECK(output_requests==1&&output_requests_at_launch==1&&e.dockStatusTab==EditorWorkspaceTab::Output);
CHECK(e.outputSearch.empty()&&!e.outputShowDetails&&!e.outputGeneralOnly&&e.outputFollowTail&&e.outputScrollToEndGeneration==1);
e.projectBuildPending=true;e.projectBuildStatus="Cancelling project build...";
CHECK(project_scene_run_button_label(e,false)=="Cancelling...");
e.projectBuildPending=false;e.projectBuildStatus="Building project (7s elapsed). Cancel is available.";
// The accepted click changes task state after the frame's initial label sample.
// Its notice must still be visible immediately, and the next click cannot launch twice.
queue_on_run=true;gui::fresh();world_controls(e,900,400);
CHECK(e.projectBuildPending&&!gui::labels.empty()&&gui::labels.front()=="Building Debug...");
CHECK(output_requests==2&&output_requests_at_launch==2);
const auto accepted_runs=runs;gui::fresh();world_controls(e,900,400);
CHECK(runs==accepted_runs&&gui::buttons.front()=="Building..."&&!gui::enabled.front()&&output_requests==2);
queue_on_run=false;e.projectBuildPending=false;runs=1;
e.projectRunAfterTerrainPreview=false;e.projectBuildStatus="Run queued. Waiting for the current terrain preview.";
gui::fresh();world_controls(e,900,400);
CHECK(runs==1&&gui::buttons.front()=="Run queued..."&&!gui::enabled.front());
CHECK(!gui::labels.empty()&&gui::labels.front()=="Run queued...");
e.projectRunAfterTerrainPreview.reset();
CHECK(e.projectBuildConfiguration=="Debug");
gui::fresh();world_controls(e,900,400,true);CHECK(runs==1);
e.projectRuntimeInEditor=true;e.projectRuntimeProcess.live=true;
e.projectRuntimeStatus="Starting project; waiting for its window to appear inside the Editor.";
gui::fresh();world_controls(e,900,400);CHECK(stops==1);
CHECK(!gui::labels.empty()&&gui::labels.front()=="Starting project in the Editor...");
e.projectRuntimeAttachmentError="Run In Editor could not attach the owned project window.";
gui::fresh();world_controls(e,900,400,true);
CHECK(!gui::labels.empty()&&gui::labels.front()=="Project launch failed. See Output.");
CHECK(run_details.back().find(e.projectRuntimeAttachmentError)!=std::string::npos);
const auto detail_count=run_details.size();gui::fresh();world_controls(e,900,400,true);CHECK(run_details.size()==detail_count);
e.outputSearch="old query";e.outputShowDetails=true;e.outputGeneralOnly=true;e.projectRuntimeViewportMaximized=true;
gui::click_output=true;gui::fresh();world_controls(e,900,400,true);CHECK(output_requests==2&&e.outputSearch=="old query");
gui::fresh();world_controls(e,900,400);CHECK(output_requests==3&&e.outputSearch.empty()&&!e.outputShowDetails&&!e.outputGeneralOnly&&!e.projectRuntimeViewportMaximized&&e.dockStatusTab==EditorWorkspaceTab::Output);
gui::click_output=false;
e.projectRuntimeAttachmentError.clear();
const auto stopsBeforeBusy=stops;e.projectBuildPending=true;gui::fresh();world_controls(e,900,400);
CHECK(stops==stopsBeforeBusy+1&&gui::buttons.front()=="Stop"&&!gui::labels.empty());
e.projectBuildPending=false;
e.projectRuntimeBuildConfiguration="Debug";e.projectBuildConfiguration="Release";
e.projectRuntimeViewportMaximized=true;
gui::fresh();world_controls(e,260,200);CHECK(!e.projectRuntimeViewportMaximized);
CHECK(gui::buttons.front()=="Stop"&&gui::buttons.back()=="Exit (Esc)");
CHECK(e.projectBuildConfiguration=="Release"&&e.projectRuntimeBuildConfiguration=="Debug");
CHECK(output_requests==3);
gui::fresh();project_controls(e,900);CHECK(plays==1&&output_requests==4&&output_requests_at_launch==4);
return 0;
}
'''
        for key, value in {"CAPABILITY_TYPES":types,"CAPABILITY_NAMES":names,"GUI_TYPES":gui_types,
                           "SUMMARY":summary,"ACTIVITY_LABEL":label,"RUN_HELPERS":run_helpers,"RECORD_OUTPUT":record_output,"WORLD":world,"PROJECT_PLAY":play,"MEASURE":measure,"RUN_WIDTH":run_width,"SHOW_RUN_OUTPUT":show_run_output}.items():
            cpp=cpp.replace(key,value)
        lane=ROOT/"Artifacts/Reports/project-runtime-presentation-oct28"/str(time.time_ns())
        lane.mkdir(parents=True)
        (lane/"editor.presentation_contract.cpp").write_text(cpp,encoding="utf-8")
        (lane/"CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ProjectRuntimePresentation LANGUAGES CXX)
add_executable(presentation editor.presentation_contract.cpp)
target_compile_features(presentation PRIVATE cxx_std_23)
target_compile_options(presentation PRIVATE /W3 /WX /EHsc /utf-8)
''',encoding="utf-8")
        build=ROOT/"Artifacts/Intermediate/cmake/project-runtime-presentation"/lane.name
        commands=[["cmake","-S",str(lane),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
        for config in ("Debug","Release"):
            commands.extend([["cmake","--build",str(build),"--config",config,"--parallel","1"],[str(build/config/"presentation.exe")]])
        runs_log=[]
        for i,command in enumerate(commands):
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=120)
            (lane/f"step-{i}.log").write_text(result.stdout+result.stderr,encoding="utf-8")
            runs_log.append({"command":command,"exit":result.returncode})
            (lane/"result.json").write_text(json.dumps({"passed":result.returncode==0 and i==len(commands)-1,"runs":runs_log,
                "sources":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths+[Path(__file__)]},
                "scope":"Actual presentation/dispatch fragments with labeled GUI/capability/launch ports, Debug/Release. No renderer capability upgrade, GPU, model or native GUI proof."},indent=2),encoding="utf-8")
            self.assertEqual(result.returncode,0,result.stdout[-4500:]+result.stderr+f"; proof={lane}")


if __name__=="__main__":unittest.main()
