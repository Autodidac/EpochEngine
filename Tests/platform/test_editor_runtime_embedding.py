"""Compile existing Editor runtime ownership against recorded OS/host ports."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes
from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class EditorRuntimeEmbedding(unittest.TestCase):
    def test_actual_launch_identity_survives_next_build_with_real_ledger(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC C++23 module lane')
        owner = ROOT/'Engine/src/editor/application/editor.application_project_runtime.inl'
        text = owner.read_text(encoding='utf-8')
        state = (ROOT/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
        evidence = definition(state, '            struct ProjectRuntimeLaunchEvidence final', True)
        functions = '\n'.join(definition(text, '        [[nodiscard]] '+kind+' '+name+'(')
                              for kind,name in [('std::string','project_runtime_correlation'),
                                                ('bool','project_runtime_launch_identity_matches'),
                                                ('bool','project_runtime_identity_current')])
        launch = definition(text, '        [[nodiscard]] bool launch_project_runtime(')
        binding = definition(launch, '            if (launched.owns_new_process())')
        code = r'''
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <core.format_text.hpp>
import project.lifecycle;
import platform.child_process;
import core.sha256;
namespace platform=epochengine::platform;
namespace project_lifecycle=epochengine::project_lifecycle;
namespace core::sha256 {using namespace epochengine::core::sha256;}
enum class EditorMainSurface{Scene};
struct EditorState {
 EVIDENCE
 ProjectRuntimeLaunchEvidence projectRuntimeLaunch{};
 platform::child_process::ProcessHandle projectRuntimeProcess{1,1};
 bool projectRuntimeInEditor{true};
 std::string projectBuildConfiguration{"Debug"};
 core::sha256::Digest projectBuildAcceptedOutputDigest{};
 project_lifecycle::EvidenceLedger projectLifecycle{};
};
constexpr std::string_view kProjectRuntimeProcessGroup="epoch.project.runtime";
FUNCTIONS
void bind(EditorState& editor,const platform::child_process::LaunchRequest& request,bool owned){
 struct{bool owned;bool owns_new_process()const{return owned;}}launched{owned};BINDING
}
// Former production predicate, retained only as a negative regression oracle.
bool former_identity_current(const EditorState& editor,const platform::child_process::ProcessSnapshot& observed){
 return editor.projectRuntimeProcess.valid() && observed.handle==editor.projectRuntimeProcess
  && observed.active() && observed.exclusive_group==kProjectRuntimeProcessGroup
  && !observed.correlation_key.empty()
  && observed.correlation_key==project_runtime_correlation(editor,editor.projectRuntimeInEditor)
  && observed.verified_executable && observed.verified_executable->valid()
  && observed.verified_executable->sha256==core::sha256::hex(editor.projectBuildAcceptedOutputDigest);
}
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(){
 EditorState editor;auto& ledger=editor.projectLifecycle;
 CHECK(ledger.select_project(7) && ledger.record_scene_commit(1) && ledger.record_shell_materialized() && ledger.observe_build_inputs(11));
 const auto build=ledger.begin_build();CHECK(build && ledger.complete_build(build,true));
 editor.projectBuildAcceptedOutputDigest=core::sha256::hash("accepted-original-image");
 platform::child_process::LaunchRequest request;
 request.correlation_key=project_runtime_correlation(editor,true);
 request.expected_executable=platform::child_process::ExecutableIdentity{321,core::sha256::hex(editor.projectBuildAcceptedOutputDigest)};
 bind(editor,request,true);
 platform::child_process::ProcessSnapshot observed;
 observed.handle=editor.projectRuntimeProcess;observed.state=platform::child_process::ProcessState::running;
 observed.exclusive_group=kProjectRuntimeProcessGroup;observed.correlation_key=request.correlation_key;observed.verified_executable=request.expected_executable;
 CHECK(former_identity_current(editor,observed) && project_runtime_identity_current(editor,observed));
 const auto captured=editor.projectRuntimeLaunch;
 editor.projectBuildConfiguration="Release";editor.projectBuildAcceptedOutputDigest=core::sha256::hash("next-build-image");
 CHECK(ledger.record_scene_commit(2) && ledger.observe_build_inputs(22));
 CHECK(!former_identity_current(editor,observed) && project_runtime_identity_current(editor,observed));
 const auto next=ledger.begin_build();CHECK(next && ledger.complete_build(next,true));
 CHECK(!former_identity_current(editor,observed) && project_runtime_identity_current(editor,observed));
 CHECK(ledger.evidence().generations.output_artifact.artifact_generation!=captured.artifact_generation);
 CHECK(editor.projectRuntimeLaunch.executable==captured.executable && editor.projectRuntimeLaunch.correlation==captured.correlation);
 auto foreignRequest=request;foreignRequest.correlation_key="foreign";bind(editor,foreignRequest,false);
 CHECK(editor.projectRuntimeLaunch.correlation==captured.correlation); // Never adopt focused/busy output.
 for(unsigned kind=0;kind<6;++kind){auto other=observed;
  if(kind==0)++other.handle.generation;else if(kind==1)other.correlation_key="foreign";
  else if(kind==2)other.exclusive_group="foreign";else if(kind==3)++other.verified_executable->size_bytes;
  else if(kind==4)other.verified_executable->sha256=core::sha256::hex(core::sha256::hash("different"));else other.verified_executable.reset();
  CHECK(!project_runtime_launch_identity_matches(editor,other) && !project_runtime_identity_current(editor,other));}
 CHECK(ledger.select_project(8) && !project_runtime_identity_current(editor,observed));
 CHECK(project_runtime_launch_identity_matches(editor,observed)); // Still owned, but cannot attach to the new project.
 CHECK(ledger.select_project(7) && !project_runtime_identity_current(editor,observed)); // New generation of same project isn't old admission.
}
'''.replace('EVIDENCE',evidence).replace('FUNCTIONS',functions).replace('BINDING',binding)
        proof=ROOT/'Artifacts/Reports/lantern-visible-oct28/runtime-launch-identity'
        build=ROOT/'Artifacts/Intermediate/tests/runtime-launch-identity'
        proof.mkdir(parents=True,exist_ok=True)
        (proof/'identity.contract.cpp').write_text(code,encoding='utf-8')
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(RuntimeLaunchIdentity LANGUAGES CXX)
add_executable(identity identity.contract.cpp)
target_compile_features(identity PRIVATE cxx_std_23)
target_compile_options(identity PRIVATE /utf-8 /EHsc /Z7)
target_include_directories(identity PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(identity PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES
 "{(ROOT/'Engine/modules/project.lifecycle.ixx').as_posix()}" "{(ROOT/'Engine/modules/platform.child_process.ixx').as_posix()}" "{(ROOT/'Engine/modules/core.sha256.ixx').as_posix()}")
enable_testing()
add_test(NAME identity COMMAND identity)
''',encoding='utf-8')
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64',
                   '-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],
                         ['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']]
        records=[]
        for number,command in enumerate(commands):
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90,
                                  env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'))
            (proof/f'lane-{number}.log').write_text(result.stdout+result.stderr,encoding='utf-8')
            records.append({'command':command,'exit':result.returncode})
            self.assertEqual(result.returncode,0,(result.stdout+result.stderr)[-7000:])
        (proof/'result.json').write_text(json.dumps({'passed':True,'commands':records,
            'ownerSha256':hashlib.sha256(owner.read_bytes()).hexdigest(),
            'scope':'Actual launch binding/correlation/identity functions with real ProjectStamp, EvidenceLedger, ExecutableIdentity and SHA256 modules; former mutable predicate fails after real next-build transitions; no GUI/process/GPU.'},indent=2)+'\n',encoding='utf-8')

    def test_actual_viewport_placement_and_grid_separation(self):
        host_path = ROOT/'Engine/src/renderers/host/host.context_win.cpp'
        interaction_path = ROOT/'Engine/src/renderers/host/implementation/host.window_interaction.inl'
        host = host_path.read_text(encoding='utf-8')
        interaction = interaction_path.read_text(encoding='utf-8')
        code = (ROOT/'Tests/platform/host.project_viewport_contract.cpp').read_text(encoding='utf-8')
        for token, source, marker, semicolon in (
            ('PRESENTATION_TYPE', interaction, '    struct ExternalProcessPresentation final', True),
            ('PRESENTATION_LOOKUP', interaction, '    [[nodiscard]] ExternalProcessPresentation external_presentation(', False),
            ('ATTACHMENT_LOOKUP', interaction, '    [[nodiscard]] std::shared_ptr<epochengine::platform::window_adapter::INativeWindowAttachment> external_attachment(', False),
            ('VIEWPORT_PLACEMENT', interaction, '    [[nodiscard]] bool place_external_viewport_attachment(', False),
            ('ATTACHMENT_CHECK', host, '    bool MultiContextManager::IsExternalProcessWindowAttached(', False),
            ('VIEWPORT_UPDATE', host, '    bool MultiContextManager::UpdateExternalProcessWindowViewport(', False),
        ):
            code = code.replace(token, definition(source, marker, semicolon))
        layout = definition(host, '    void MultiContextManager::ArrangeDockedWindowsGrid(')
        start = layout.index('        std::vector<WindowData*> viewportWindows;')
        end = layout.index('        std::stable_sort(', start)
        code = code.replace('GRID_FILTER', layout[start:end])
        proof = ROOT/'Artifacts/Reports/project-play-flicker-oct31/placement'
        proof.mkdir(parents=True, exist_ok=True)
        (proof/'placement.cpp').write_text(code, encoding='utf-8')
        # Negative is the former grid policy: its geometry still works, but
        # the ordinary project is counted as another renderer context.
        negative = code.replace('            viewportWindows.push_back(window);\n            return true;',
                                '            return false;')
        self.assertNotEqual(negative, code)
        (proof/'negative.cpp').write_text(negative, encoding='utf-8')
        synchronous = code.replace('SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);', 'SWP_NOACTIVATE);')
        self.assertNotEqual(synchronous, code)
        (proof/'synchronous.cpp').write_text(synchronous, encoding='utf-8')
        former_visibility = code.replace(
            'if (!context || !presentation.visible || context->gui_overlay_priority())',
            'if (!context || !presentation.visible || context->gui_overlay_priority()'
            ' || context->scene_preview_mode() != epochengine::core::ScenePreviewMode::Editor)')
        self.assertNotEqual(former_visibility, code)
        (proof/'former_visibility.cpp').write_text(former_visibility, encoding='utf-8')
        if os.name == 'nt':
            tools = msvc_2022_toolsets()
            sdk = Path('C:/Program Files (x86)/Windows Kits/10')
            kits = windows_sdk_includes(sdk)
            if not tools or not kits:
                self.skipTest('Installed MSVC/SDK required')
            tool, kit = tools[-1], kits[-1]
            results=[]
            for config in ('Debug','Release'):
                for name, expected in (('placement',0),('negative',None),('synchronous',None),('former_visibility',None)):
                    output=proof/config/name
                    output.mkdir(parents=True,exist_ok=True)
                    command=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/std:c++latest','/EHsc','/W4','/WX',
                        '/Z7' if config=='Debug' else '/O2','/MDd' if config=='Debug' else '/MD',
                        '/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/I'+str(kit/'shared'),'/I'+str(kit/'um'),
                        '/Fo'+str(output/'contract.obj'),'/Fe'+str(output/'contract.exe'),str(proof/(name+'.cpp')),
                        '/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),
                        '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
                    run=subprocess.run(command,cwd=output,capture_output=True,text=True,timeout=45)
                    (output/'compile.log').write_text(run.stdout+run.stderr,encoding='utf-8')
                    self.assertEqual(run.returncode,0,(run.stdout+run.stderr)[-9000:])
                    run=subprocess.run([str(output/'contract.exe')],cwd=output,capture_output=True,text=True,timeout=10)
                    results.append(dict(configuration=config,case=name,exit=run.returncode,expected=expected))
                    if expected is None:self.assertNotEqual(run.returncode,0,name+' must expose the historical failure')
                    else:self.assertEqual(run.returncode,expected,name+': '+str(run.returncode))
            (proof/'direct-result.json').write_text(json.dumps(dict(
                sources={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in (host_path,interaction_path)},cases=results,
                scope='Actual placement, exact-lease update and grid-exclusion owners; recorded native calls, no OS windows or renderers',
                negative='Former grid policy, synchronous foreign z-order and parent-preview child visibility must fail; opt-in trace bounded32 records',native=False),indent=2)+'\n',encoding='utf-8')
            return
        (proof/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.30)
project(ProjectViewportPlacement LANGUAGES CXX)
add_executable(placement placement.cpp)
add_executable(negative negative.cpp)
target_compile_features(placement PRIVATE cxx_std_23)
target_compile_features(negative PRIVATE cxx_std_23)
enable_testing()
add_test(NAME placement COMMAND placement)
add_test(NAME former_grid_policy COMMAND negative)
set_tests_properties(former_grid_policy PROPERTIES WILL_FAIL TRUE)
''', encoding='utf-8')
        configure = ['cmake','-S',str(proof),'-B',str(proof/'cmake')]
        configure += (['-G','Visual Studio 17 2022','-A','x64',
                       '-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']
                      if os.name == 'nt' else ['-G','Ninja','-DCMAKE_BUILD_TYPE=Release'])
        commands = [('configure', configure)]
        for config in (('Debug','Release') if os.name == 'nt' else ('Release',)):
            commands += [(config+'-build',['cmake','--build',str(proof/'cmake'),'--config',config,'--parallel','2']),
                         (config+'-test',['ctest','--test-dir',str(proof/'cmake'),'-C',config,'--output-on-failure'])]
        results=[]
        for phase,command in commands:
            run=subprocess.run(command,cwd=proof,capture_output=True,text=True,timeout=90,
                               env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'))
            (proof/(phase+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
            results.append(dict(phase=phase,exit=run.returncode,command=command))
            (proof/'result.json').write_text(json.dumps(dict(
                sources={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in (host_path,interaction_path)}, phases=results,
                scope='Actual placement, exact-lease update and grid-exclusion owners; recorded native geometry calls, no windows or renderers',
                negative='Former grid policy must fail',native=False),indent=2)+'\n',encoding='utf-8')
            self.assertEqual(run.returncode,0,(run.stdout+run.stderr)[-9000:])

    def test_session_does_not_reenable_covered_preview_before_editor_frame(self):
        # Renderer samples the shared atomic independently of the UI turn.
        # Once Editor publishes None for covered Play, neither session loop
        # may briefly publish Editor before the owning viewport runs again.
        for name in ('epoch.session_runtime.inl', 'epoch.session_editor_launcher.inl'):
            source = (ROOT/'Engine/src/epoch/implementation'/name).read_text(encoding='utf-8')
            marker = ('case SessionMode::Editor:' if name == 'epoch.session_runtime.inl'
                      else 'if (state == EditorSceneState::Editor)')
            begin = source.index(marker)
            frame = source.index('const auto editor_frame = epochengine::editor_run(ctx);', begin)
            self.assertNotIn('ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor)',
                             source[begin:frame], name)
        # Restore/load already publish the selected Editor state. A subsequent
        # handoff must not override None between independently sampled frames.
        runtime = (ROOT/'Engine/src/epoch/implementation/epoch.session_runtime.inl').read_text(encoding='utf-8')
        self.assertNotIn('targetCtx->set_scene_preview_mode(core::ScenePreviewMode::Editor)', runtime)

    def test_actual_presentation_and_retirement_owners(self):
        owner = ROOT / 'Engine/src/editor/application/editor.application_project_runtime.inl'
        text = owner.read_text(encoding='utf-8')
        names = [('bool', 'project_runtime_embedding_supported'), ('void', 'exit_project_runtime_full_screen'),
                 ('void', 'select_scene_run_mode'), ('bool', 'project_runtime_viewport_maximized'),
                 ('void', 'detach_project_runtime_window'),
                 ('bool', 'project_runtime_launch_identity_matches'),
                 ('bool', 'project_runtime_identity_current'), ('bool', 'present_project_runtime_viewport'),
                 ('bool', 'reconcile_project_runtime_window'),
                 ('void', 'reconcile_project_runtime'), ('bool', 'stop_project_runtime'),
                 ('void', 'reset_project_build_request')]
        selected = '\n'.join(definition(text, ('        [[nodiscard]] ' if kind == 'bool' else '        ')
                                        + kind + ' ' + name + '(') for kind, name in names)
        selected = definition(text, '        [[nodiscard]] unsigned scene_run_mode_index(') + '\n' + \
            definition(text, '        [[nodiscard]] std::string project_scene_run_button_label(') + '\n' + selected
        pane_family = definition((ROOT/'Engine/src/editor/application/editor.frame_bootstrap.inl').read_text(encoding='utf-8'),
                                 '        auto pane_family_enabled =', True)
        frame = (ROOT/'Engine/src/editor/application/editor.frame_project_runtime.inl').read_text(encoding='utf-8')
        state = (ROOT/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
        project_stamp = definition((ROOT/'Engine/modules/project.lifecycle.ixx').read_text(encoding='utf-8'),
                                   '    struct ProjectStamp final', True)
        launch_evidence = definition(state, '            struct ProjectRuntimeLaunchEvidence final', True)
        viewport = (ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        status_begin = viewport.index('                const std::string runStatus =')
        status = viewport[status_begin:viewport.index(';',status_begin)+1]
        capture_begin = viewport.index('            result.scene_input_captured = result.scene_input_captured\n                || editor_project_viewport_input_reserved(editor);')
        input_capture = viewport[capture_begin:viewport.index(';',capture_begin)+1]
        selected += '\n' + definition((ROOT/'Engine/src/editor/application/editor.application_document_editing.inl').read_text(encoding='utf-8'),
                                     '        [[nodiscard]] bool editor_project_viewport_input_reserved(')
        document = (ROOT/'Engine/src/editor/application/editor.application_document_editing.inl').read_text(encoding='utf-8')
        covered_marker = '        [[nodiscard]] bool editor_project_viewport_covered('
        if covered_marker in document:
            selected += '\n' + definition(document, covered_marker)
        preview_begin = viewport.index('            ctx->set_scene_preview_mode(sceneFits')
        preview_publish = viewport[preview_begin:viewport.index(';', preview_begin)+1]
        default = next(line.strip() for line in state.splitlines() if 'bool sceneRunFullscreenInEditor{' in line)
        capture = next(line.strip() for line in text.splitlines() if 'editor.projectBuildRunInEditor = runInEditor;' in line)
        poll_capture = next(line.strip() for line in frame.splitlines() if 'const bool runInEditor = editor.projectBuildRunInEditor;' in line)
        branch = definition(frame, '            if(requirement.requires_project_runtime)')
        launch_owner = definition(text, '        [[nodiscard]] bool launch_project_runtime(')
        reuse_guard = definition(launch_owner, '            if (editor.projectRuntimeProcess.valid())')
        launch_signature = frame[frame.index('auto launch_active_project_context ='):frame.index('{', frame.index('auto launch_active_project_context =') + len('auto launch_active_project_context = [&]'))]
        code = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
namespace epochengine {template<class... T> std::string format_text(std::string_view,T&&...){return "recorded status";}}
namespace core::sha256 {struct Digest{std::string text{"hash"};};std::string hex(const Digest& d){return d.text;}}
namespace platform::child_process {
struct ProcessHandle {unsigned slot{},generation{};bool valid()const{return generation!=0;}
 friend bool operator==(const ProcessHandle&,const ProcessHandle&)=default;};
enum class ProcessState {starting,running,stop_requested,exited};
enum class StopMode {graceful,force};enum class StopCode{requested,forced,unsupported,failed};
enum class RuntimeControlStatus{unavailable,idle,pending,accepted,rejected,disconnected};
struct RuntimeControlSnapshot{RuntimeControlStatus status{RuntimeControlStatus::unavailable};};
struct ExecutableIdentity {std::uint64_t size_bytes{10};std::string sha256{"hash"};bool valid()const{return size_bytes!=0&&!sha256.empty();}
 friend bool operator==(const ExecutableIdentity&,const ExecutableIdentity&)=default;};
struct ProcessSnapshot {
 ProcessHandle handle{1,1};ProcessState state{ProcessState::running};
 std::uint64_t platform_process_id{42},platform_window_id{100},runtime_ns{100};
 std::string correlation_key{"project:editor"},exclusive_group{"epoch.project.runtime"},message;
 int exit_code{};bool exit_code_valid{true};RuntimeControlSnapshot runtime_control{};
 std::optional<ExecutableIdentity> verified_executable{ExecutableIdentity{}};
 bool active()const{return state!=ProcessState::exited;}
};
inline unsigned stops{},releases{},polls{},group_queries{};inline StopMode last_mode{};
inline ProcessHandle last_stopped{},last_released{};inline std::optional<ProcessSnapshot> observed;
void poll(){++polls;}
std::optional<ProcessSnapshot> snapshot(ProcessHandle h){return observed&&observed->handle==h?observed:std::nullopt;}
std::optional<ProcessSnapshot> active_in_group(std::string_view){++group_queries;return ProcessSnapshot{};}
StopCode stop(ProcessHandle h,StopMode mode){++stops;last_stopped=h;last_mode=mode;return mode==StopMode::force?StopCode::forced:StopCode::requested;}
bool release(ProcessHandle h){++releases;last_released=h;return true;}
std::string_view process_state_name(ProcessState){return "recorded state";}
std::string_view stop_code_name(StopCode){return "recorded stop";}
}
namespace core {
enum class RoutedPanelDockTarget{none,right_context};
enum class ScenePreviewMode{None,Editor};
struct Context{ScenePreviewMode mode{};void set_scene_preview_mode(ScenePreviewMode value){mode=value;}};
inline bool contract_error{};
inline auto admitted_owner=std::make_shared<Context>();
inline std::shared_ptr<Context> current{};
inline unsigned tls_reads{};
struct MultiContextManager{static auto GetCurrent(){++tls_reads;return current;}};
struct Manager {
 bool running{true},parent{true},accept{true},accept_update{true},attached{},visible{};unsigned adds{},removes{},updates{};
 std::uintptr_t window{};std::uint64_t pid{};std::string route;bool retiring{};
 bool IsRunning()const{return running;}void* GetParentWindow()const{return parent?reinterpret_cast<void*>(1):nullptr;}
 bool IsExternalProcessWindowAttached(std::uintptr_t w,std::uint64_t p,std::string_view r)const{return attached&&window==w&&pid==p&&route==r;}
 bool AddExternalProcessWindow(std::uintptr_t w,std::uint64_t p,std::string r,RoutedPanelDockTarget target,const std::shared_ptr<Context>& context){++adds;
  if(target!=RoutedPanelDockTarget::none||context!=admitted_owner){contract_error=true;return false;}
  if(!accept)return false;window=w;pid=p;route=std::move(r);attached=true;return true;}
 bool UpdateExternalProcessWindowViewport(std::uintptr_t w,std::uint64_t p,std::string_view r,bool shown){++updates;
  if(!accept_update||!IsExternalProcessWindowAttached(w,p,r))return false;visible=shown;return true;}
 void RemoveExternalProcessWindow(std::uintptr_t w,bool retire){if(w!=window){contract_error=true;return;}++removes;attached=false;retiring=retire;}
};inline Manager* active{};Manager* GetActiveMultiContextManager(){return active;}
}
namespace logger {
enum class LogLevel{INFO,Error};inline unsigned errors{};
struct Port{void log(LogLevel level,std::string_view,std::source_location){if(level==LogLevel::Error)++errors;}};
Port& get(std::string_view){static Port port;return port;}
}
namespace project_lifecycle {PROJECT_STAMP}
LAUNCH_EVIDENCE
struct Lifecycle {
 struct Evidence{bool runtime_active{};struct Generations{project_lifecycle::ProjectStamp selected_project{1,1};
  struct Artifact{std::uint64_t artifact_generation{1};} output_artifact;} generations;};Evidence state;
 const Evidence& evidence()const{return state;}bool record_runtime_started(){state.runtime_active=true;return true;}
 void record_runtime_stopped(){state.runtime_active=false;}
};
enum class EditorMainSurface{Scene,Project,ForestFactory};
enum class EditorLayoutDrag{None,Side};
enum class EditorToolPane{Outliner,Inspector,Console,AiControls,AiChat};
bool is_outliner_tool_pane(EditorToolPane p){return p==EditorToolPane::Outliner;}
bool is_inspector_tool_pane(EditorToolPane p){return p==EditorToolPane::Inspector;}
bool is_console_tool_pane(EditorToolPane p){return p==EditorToolPane::Console;}
struct EditorState {
 core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};
 DEFAULT_MODE
 bool projectBuildRunAfterBuild{},projectBuildRunInEditor{};
 int projectBuildCancellation{},projectBuildProgressSample{},projectBuildRunCameraMode{},projectCameraMode{},
  projectBuildRunInputProfile{},inputProfilePreset{};
 double projectBuildRunFrameLimitFps{},projectRunFrameLimitFps{60};
 std::uint64_t projectBuildProjectKey{};
 std::string projectBuildRunScene,projectBuildOutputPath,projectBuildRunBackend,projectBuildProjectId,
  projectBuildProjectRoot,projectBuildRequestedConfiguration,projectStatus;
 unsigned route{};
 platform::child_process::ProcessHandle projectRuntimeProcess{1,1};
 ProjectRuntimeLaunchEvidence projectRuntimeLaunch{{1,1},1,{},"project:editor"};
 std::optional<platform::child_process::ProcessSnapshot> projectRuntimeSnapshot;
 bool projectRuntimeInEditor{true},projectRuntimeForceStopAvailable{},projectRuntimeViewportMaximized{},sceneRunMaximizedInEditor{};
 bool showOutliner{true},showInspector{true},showConsoleDock{true},showAiChat{true};
 int surfaceSettleFrames{};EditorLayoutDrag layoutDrag{EditorLayoutDrag::Side};
 std::string projectRuntimeBuildConfiguration,projectBuildConfiguration{"Debug"};
 std::uintptr_t projectRuntimeWindow{};std::uint64_t projectRuntimeWindowProcess{};
 std::string projectRuntimeStatus,projectRuntimeAttachmentError,projectRuntimeOutputLog;
 std::optional<int> projectBuildPending;std::string projectBuildStatus;
 core::sha256::Digest projectBuildAcceptedOutputDigest;
 Lifecycle projectLifecycle;
 EditorMainSurface mainSurface{EditorMainSurface::Scene};
 std::weak_ptr<core::Context> projectRuntimeViewportContext{core::admitted_owner};
};
constexpr std::string_view kProjectRuntimeProcessGroup="epoch.project.runtime",kProjectRuntimeWindowRoute="project_runtime";
std::string project_runtime_correlation(const EditorState&,bool editor){return editor?"project:editor":"project:separate";}
void push_editor_log(EditorState&,std::string_view){}
OWNERS
bool actual_pane_visibility(EditorState& editor,EditorToolPane pane){
 const bool projectViewportMaximized=project_runtime_viewport_maximized(editor);
 const auto pane_supported=[](EditorToolPane){return true;};
 PANE_FAMILY
 return pane_family_enabled(pane);
}
core::ScenePreviewMode actual_preview_mode(const EditorState& editor,bool sceneFits=true){
 core::Context recording;auto*ctx=&recording;
 @@PREVIEW_PUBLISH@@
 return recording.mode;
}
unsigned focuses{},new_logs{};
std::string actual_run_status(const EditorState& editor,bool compiledProjectRunning){std::string currentBuildActivity="Building";
 RUN_STATUS
 return runStatus;
}
bool actual_run_authoring_capture(const EditorState&editor,bool captured=false){struct{bool scene_input_captured;}result{captured};
 @@AUTHORING_GUARD@@
 return result.scene_input_captured;
}
bool actual_run_authoring_capture(bool captured,bool compiledProjectRunning){EditorState editor;
 editor.projectRuntimeInEditor=compiledProjectRunning;
 if(!compiledProjectRunning)editor.projectRuntimeProcess={};
 return actual_run_authoring_capture(editor,captured);}
bool focus_project_runtime(EditorState&){++focuses;return true;}
bool actual_runtime_reuse_guard(EditorState& editor,bool inEditor){
 REUSE_GUARD
 ++new_logs;return true;
}
void begin_build(EditorState& editor,bool runInEditor){CAPTURE}
bool finish_build(EditorState& editor){POLL_CAPTURE reset_project_build_request(editor);return runInEditor;}
void actual_run_branch(EditorState& editor,bool needs_runtime){struct {bool requires_project_runtime;std::string message;} requirement{needs_runtime,"compiled provider"};
 LAUNCH_SIGNATURE {editor.route=inEditor?1:2;};
 RUN_BRANCH
 editor.route=3;
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
void reset(){platform::child_process::stops=platform::child_process::releases=platform::child_process::polls=platform::child_process::group_queries=0;
 platform::child_process::observed.reset();core::active=nullptr;core::current.reset();core::tls_reads=0;logger::errors=0;}
int main(){
 namespace process=platform::child_process;
 {EditorState e;e.projectRuntimeStatus="Starting; waiting for its window";
  CHECK(actual_run_status(e,true)==e.projectRuntimeStatus&&actual_run_status(e,false).empty());
  e.projectRuntimeAttachmentError="Owned attach failed";CHECK(actual_run_status(e,true)==e.projectRuntimeAttachmentError);
  e.projectBuildPending=1;e.projectBuildStatus="Compiling";CHECK(actual_run_status(e,false)=="Building Compiling");
  CHECK(actual_run_authoring_capture(false,true)&&actual_run_authoring_capture(true,false)&&!actual_run_authoring_capture(false,false));}
 {EditorState e;e.projectRuntimeProcess={};e.projectRuntimeInEditor=false;e.projectBuildPending=1;
  CHECK(actual_run_authoring_capture(e)); // Build-only freezes the authored scene too.
  e.projectBuildRunAfterBuild=true;CHECK(actual_run_authoring_capture(e)); // Separate Window build.
  e.projectBuildRunInEditor=true;CHECK(actual_run_authoring_capture(e));
  e.projectBuildRunAfterBuild=false;CHECK(actual_run_authoring_capture(e)); // Cancel waits for build retirement.
  e.projectBuildRunAfterBuild=true;e.projectBuildPending.reset();CHECK(!actual_run_authoring_capture(e));
  CHECK(actual_run_authoring_capture(e,true));}
 {reset();EditorState e;process::observed=process::ProcessSnapshot{};e.projectRuntimeOutputLog="kept.log";
  CHECK(actual_runtime_reuse_guard(e,true)&&focuses==1&&new_logs==0&&e.projectRuntimeOutputLog=="kept.log");
  CHECK(!actual_runtime_reuse_guard(e,false)&&focuses==1&&new_logs==0);
  process::observed->verified_executable->sha256="different";
  CHECK(!actual_runtime_reuse_guard(e,true)&&focuses==1&&new_logs==0);
  process::observed.reset();CHECK(actual_runtime_reuse_guard(e,true)&&new_logs==1);}
 {EditorState e;CHECK(e.sceneRunFullscreenInEditor);begin_build(e,true);e.sceneRunFullscreenInEditor=false;
  CHECK(finish_build(e)&&!e.projectBuildRunInEditor);begin_build(e,false);e.sceneRunFullscreenInEditor=true;
  CHECK(!finish_build(e));actual_run_branch(e,true);CHECK(e.route==1);actual_run_branch(e,false);CHECK(e.route==3);}
 reset();EditorState editor;process::ProcessSnapshot s;core::Manager host;core::active=&host;
#if defined(_WIN32)
 CHECK(project_runtime_embedding_supported());
#else
 CHECK(!project_runtime_embedding_supported());
#endif
 CHECK(reconcile_project_runtime_window(editor,s));CHECK(host.adds==1&&host.attached&&host.pid==42&&host.route=="project_runtime");
 CHECK(!core::current&&core::tls_reads==0); // UI/session thread has no render TLS.
 CHECK(editor.projectRuntimeWindow==100&&editor.projectRuntimeWindowProcess==42&&process::stops==0);
 CHECK(host.visible&&host.updates==1);
 CHECK(actual_preview_mode(editor)==core::ScenePreviewMode::None);
 CHECK(actual_pane_visibility(editor,EditorToolPane::Console)); // Normal GUI stays available.
 CHECK(project_scene_run_button_label(editor,true)=="Stop");
 {EditorState pending;pending.projectRuntimeProcess={};CHECK(actual_preview_mode(pending)==core::ScenePreviewMode::Editor);
  pending.projectBuildPending=1;pending.projectBuildRunAfterBuild=true;pending.projectBuildRunInEditor=true;
  CHECK(actual_preview_mode(pending)==core::ScenePreviewMode::None);}
 {auto separate=editor;separate.projectRuntimeInEditor=false;
  CHECK(actual_preview_mode(separate)==core::ScenePreviewMode::None);
  separate=editor;separate.projectRuntimeProcess={};CHECK(actual_preview_mode(separate)==core::ScenePreviewMode::Editor);
  separate=editor;separate.projectRuntimeWindowProcess=0;CHECK(actual_preview_mode(separate)==core::ScenePreviewMode::None);
  separate=editor;separate.mainSurface=EditorMainSurface::ForestFactory;
  CHECK(actual_preview_mode(separate)==core::ScenePreviewMode::None);
  separate=editor;separate.projectRuntimeAttachmentError="placement failed";
  CHECK(actual_preview_mode(separate)==core::ScenePreviewMode::None);
  CHECK(actual_preview_mode(separate,false)==core::ScenePreviewMode::None);}

 CHECK(reconcile_project_runtime_window(editor,s)&&host.adds==1); // Idempotent; no repeated reparenting.
 // The next draft/build does not replace the already-running executable identity.
 editor.projectBuildAcceptedOutputDigest.text="next-build";editor.projectBuildConfiguration="Release";
 editor.projectLifecycle.state.generations.output_artifact.artifact_generation=2;
 CHECK(reconcile_project_runtime_window(editor,s)&&host.adds==1&&process::stops==0);
 editor.mainSurface=EditorMainSurface::Project;CHECK(present_project_runtime_viewport(editor)&&!host.visible);
 editor.mainSurface=EditorMainSurface::Scene;CHECK(present_project_runtime_viewport(editor)&&host.visible&&host.adds==1);
 process::observed=s;CHECK(stop_project_runtime(editor,process::StopMode::graceful));
 CHECK(process::stops==1&&process::last_stopped==s.handle&&process::last_mode==process::StopMode::graceful);
 CHECK(host.removes==1&&host.retiring&&editor.projectRuntimeWindow==0);
 CHECK(actual_preview_mode(editor)==core::ScenePreviewMode::None); // Stop request waits for owned retirement.
 s.state=process::ProcessState::exited;process::observed=s;reconcile_project_runtime(editor);
 CHECK(process::releases==1&&!editor.projectRuntimeProcess.valid()&&!editor.projectLifecycle.evidence().runtime_active);
 CHECK(process::group_queries==0);
 // Different generation, image, project/channel or group can neither dock nor stop.
 for(unsigned variant=0;variant<7;++variant){reset();EditorState e;process::ProcessSnapshot foreign;core::Manager h;core::active=&h;
  switch(variant){case 0:foreign.handle.generation++;break;case 1:foreign.correlation_key="foreign";break;
   case 2:foreign.exclusive_group="foreign";break;case 3:foreign.verified_executable.reset();break;
   case 4:foreign.verified_executable->sha256="other";break;case 5:e.projectRuntimeProcess={};break;
   default:foreign.state=process::ProcessState::exited;break;}
  CHECK(!reconcile_project_runtime_window(e,foreign)&&h.adds==0&&h.removes==0&&process::stops==0);}
 // No ready window is pending, never claimed as attached; expiration retires only owned handle.
 {reset();EditorState e;process::ProcessSnapshot pending;pending.platform_window_id=0;core::Manager h;core::active=&h;
  CHECK(reconcile_project_runtime_window(e,pending)&&h.adds==0&&e.projectRuntimeWindow==0&&process::stops==0);
  pending.runtime_ns=15'000'000'000ull;CHECK(!reconcile_project_runtime_window(e,pending));
  CHECK(process::stops==1&&process::last_stopped==pending.handle&&process::last_mode==process::StopMode::force);
  CHECK(!reconcile_project_runtime_window(e,pending)&&process::stops==1&&!e.projectRuntimeAttachmentError.empty()&&logger::errors==1);}
 {reset();EditorState e;process::ProcessSnapshot s;core::Manager h;h.accept=false;core::active=&h;
  CHECK(!reconcile_project_runtime_window(e,s)&&h.adds==1&&process::stops==1&&e.projectRuntimeWindow==0);}
 {reset();EditorState e;process::ProcessSnapshot s;core::Manager h;h.accept_update=false;core::active=&h;
  CHECK(!reconcile_project_runtime_window(e,s)&&h.adds==1&&h.updates==1&&process::stops==1);
  CHECK(actual_preview_mode(e)==core::ScenePreviewMode::None);}
 {reset();EditorState e;process::ProcessSnapshot s;core::Manager h;core::active=&h;core::current=std::make_shared<core::Context>();
  CHECK(reconcile_project_runtime_window(e,s)&&h.adds==1&&core::tls_reads==0);}
 for(bool expired:{false,true}){reset();EditorState e;process::ProcessSnapshot s;core::Manager h;core::active=&h;
  e.projectRuntimeViewportContext.reset();if(expired){auto owner=std::make_shared<core::Context>();e.projectRuntimeViewportContext=owner;}
  core::current=core::admitted_owner; // A primary/other renderer is no fallback.
  CHECK(!reconcile_project_runtime_window(e,s)&&h.adds==0&&process::stops==1&&core::tls_reads==0&&logger::errors==1);}
 {reset();EditorState e;process::ProcessSnapshot s;CHECK(!project_runtime_embedding_supported());
  CHECK(!reconcile_project_runtime_window(e,s)&&process::stops==1);}
 {reset();EditorState e;process::ProcessSnapshot s;e.projectRuntimeInEditor=false;s.correlation_key="project:separate";
  e.projectRuntimeLaunch.correlation=s.correlation_key;
  CHECK(reconcile_project_runtime_window(e,s)&&process::stops==0&&e.projectRuntimeWindow==0);}
 {reset();EditorState e;process::ProcessSnapshot s;core::Manager h;core::active=&h;
  ++e.projectLifecycle.state.generations.selected_project.generation;
  CHECK(!reconcile_project_runtime_window(e,s)&&h.adds==0&&process::stops==1&&logger::errors==1);
  CHECK(e.projectRuntimeStatus.find("Stop requested")!=std::string::npos);
  CHECK(!reconcile_project_runtime_window(e,s)&&process::stops==1&&logger::errors==1);}
 {reset();EditorState e;process::ProcessSnapshot s;core::Manager h;core::active=&h;
  CHECK(reconcile_project_runtime_window(e,s));h.pid=999; // Reused window belongs to another lease.
  detach_project_runtime_window(e,true);CHECK(h.removes==0&&e.projectRuntimeWindow==0);}
 {reset();EditorState e;e.projectRuntimeProcess={};reconcile_project_runtime(e);
  CHECK(process::group_queries==0&&!e.projectRuntimeProcess.valid()&&process::releases==0);}
 // The normal no-runtime poll runs before the next held divider frame. It
 // must not retire an unrelated layout gesture on every reconciliation.
 {reset();EditorState e;e.projectRuntimeProcess={};e.layoutDrag=EditorLayoutDrag::Side;
  for(unsigned frame=0;frame<4;++frame){reconcile_project_runtime(e);
   CHECK(e.layoutDrag==EditorLayoutDrag::Side&&e.surfaceSettleFrames==0);}
  detach_project_runtime_window(e,true);CHECK(e.layoutDrag==EditorLayoutDrag::Side&&e.surfaceSettleFrames==0);}
 {reset();EditorState e;process::ProcessSnapshot s;s.state=process::ProcessState::stop_requested;core::Manager h;core::active=&h;
  CHECK(reconcile_project_runtime_window(e,s)&&h.adds==0&&process::stops==0);}
 {reset();EditorState e;CHECK(scene_run_mode_index(e)==1u);
  select_scene_run_mode(e,2);CHECK(scene_run_mode_index(e)==2&&project_runtime_viewport_maximized(e));
  for(auto p:{EditorToolPane::Outliner,EditorToolPane::Inspector,EditorToolPane::Console,EditorToolPane::AiChat})CHECK(!actual_pane_visibility(e,p));
  CHECK(e.showOutliner&&e.showInspector&&e.showConsoleDock&&e.showAiChat);
  e.projectRuntimeBuildConfiguration="Debug";e.projectBuildConfiguration="Release";
  CHECK(project_scene_run_button_label(e,true)=="Stop"&&project_scene_run_button_label(e,false)=="Run");
  e.mainSurface=EditorMainSurface::Project;CHECK(!project_runtime_viewport_maximized(e));e.mainSurface=EditorMainSurface::Scene;
  e.projectRuntimeAttachmentError="failed";CHECK(!project_runtime_viewport_maximized(e));e.projectRuntimeAttachmentError.clear();
  exit_project_runtime_full_screen(e);CHECK(!project_runtime_viewport_maximized(e)&&scene_run_mode_index(e)==2);
  CHECK(e.layoutDrag==EditorLayoutDrag::None&&e.surfaceSettleFrames==1);
  e.layoutDrag=EditorLayoutDrag::Side;exit_project_runtime_full_screen(e);
  CHECK(e.layoutDrag==EditorLayoutDrag::Side&&e.surfaceSettleFrames==1);
  for(auto p:{EditorToolPane::Outliner,EditorToolPane::Inspector,EditorToolPane::Console,EditorToolPane::AiChat})CHECK(actual_pane_visibility(e,p));
  select_scene_run_mode(e,2);detach_project_runtime_window(e,false);CHECK(project_runtime_viewport_maximized(e));
  detach_project_runtime_window(e,true);CHECK(!project_runtime_viewport_maximized(e));
  select_scene_run_mode(e,0);CHECK(!e.sceneRunFullscreenInEditor&&!e.sceneRunMaximizedInEditor);
  select_scene_run_mode(e,1);CHECK(e.sceneRunFullscreenInEditor&&!e.sceneRunMaximizedInEditor);
  select_scene_run_mode(e,5);CHECK(scene_run_mode_index(e)==1);
  select_scene_run_mode(e,2);reconcile_project_runtime(e);CHECK(!e.projectRuntimeViewportMaximized);
 }
 CHECK(!core::contract_error);
 return 0;
}
'''.replace('OWNERS', selected).replace('DEFAULT_MODE', default).replace('POLL_CAPTURE', poll_capture).replace('CAPTURE', capture).replace('REUSE_GUARD', reuse_guard)
        code = code.replace('LAUNCH_SIGNATURE', launch_signature).replace('RUN_BRANCH', branch)
        code = code.replace('PROJECT_STAMP', project_stamp).replace('LAUNCH_EVIDENCE', launch_evidence)
        code = code.replace('RUN_STATUS',status).replace('@@AUTHORING_GUARD@@',input_capture)
        code = code.replace('PANE_FAMILY', pane_family).replace('@@PREVIEW_PUBLISH@@', preview_publish)
        finish = (ROOT/'Engine/src/editor/application/editor.frame_finish.inl').read_text(encoding='utf-8')
        self.assertLess(finish.index('ctx->set_gui_overlay_priority('),
                        finish.index('present_project_runtime_viewport(editor)'))
        launch = launch_owner
        self.assertIn('if (inEditor) editor.requestedMainSurface = EditorMainSurface::Scene;', launch)
        self.assertLess(launch.index('if (!launched)'), launch.index('editor.requestedMainSurface ='))
        application = (ROOT/'Engine/src/editor/editor.application.cpp').read_text(encoding='utf-8')
        frame_owner = application[application.index('    EditorFrameResult editor_run('):]
        self.assertLess(frame_owner.index('editor.projectRuntimeViewportContext = ctx;'),
                        frame_owner.index('#include "application/editor.frame_project_runtime.inl"'))
        self.assertIn('std::weak_ptr<core::Context> projectRuntimeViewportContext{};', state)
        self.assertLess(launch.index('if (editor.projectRuntimeProcess.valid())'),
                        launch.index('prepare_project_runtime_log(editor)'))
        self.assertIn('request.merged_output_path = outputLog;', launch)
        self.assertIn('request.append_output = true;', launch)
        self.assertIn('request.disconnect_standard_input = true;', launch)
        self.assertIn('ExclusiveFileLease::acquire(path)', definition(text,
                      '        [[nodiscard]] std::filesystem::path prepare_project_runtime_log('))
        proof = ROOT / 'Artifacts/Reports/divider-retirement-oct29' / hashlib.sha256(code.encode()).hexdigest()[:12]
        proof.mkdir(parents=True, exist_ok=True)
        (proof / 'embedding.contract.cpp').write_text(code, encoding='utf-8')
        (proof / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
project(EditorRuntimeEmbedding LANGUAGES CXX)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
add_executable(embedding embedding.contract.cpp)
target_compile_features(embedding PRIVATE cxx_std_23)
enable_testing()
add_test(NAME embedding COMMAND embedding)
''', encoding='utf-8')
        configure = ['cmake', '-S', str(proof), '-B', str(proof/'cmake')]
        configure += (['-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']
                      if os.name == 'nt' else ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'])
        commands = [('configure', configure)]
        for config in (('Debug', 'Release') if os.name == 'nt' else ('Release',)):
            commands += [(config+'-build', ['cmake', '--build', str(proof/'cmake'), '--config', config, '--parallel', '2']),
                         (config+'-test', ['ctest', '--test-dir', str(proof/'cmake'), '-C', config, '--output-on-failure'])]
        results = []
        for phase, command in commands:
            run = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=90,
                                 env=dict(os.environ, MSBUILDDISABLENODEREUSE='1'))
            (proof/(phase+'.log')).write_text(run.stdout+run.stderr, encoding='utf-8')
            results.append(dict(phase=phase, exit=run.returncode, command=command))
            (proof/'result.json').write_text(json.dumps(dict(
                source_sha256=hashlib.sha256(owner.read_bytes()).hexdigest(),
                scope='Actual existing ownership/presentation/stop/reconcile functions; recorded process/manager/identity ports, no native window/process launch',
                phases=results, native=False), indent=2)+'\n', encoding='utf-8')
            self.assertEqual(run.returncode, 0, (run.stdout+run.stderr)[-9000:])


if __name__ == '__main__':
    unittest.main()
