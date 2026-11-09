"""Real project input admission/sampling and runtime navigation, CPU-only ports."""
from pathlib import Path
import hashlib
import json
import os
import re
import time
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition

ROOT = Path(__file__).resolve().parents[2]
OWNERS = (
    "Engine/modules/project.asset_registry.ixx",
    "Engine/modules/project.input_profile.ixx",
    "Engine/src/project/project.input_profile.cpp",
    "Engine/modules/input.engine.ixx",
    "Engine/modules/core.context.ixx",
    "Engine/modules/input.controller.ixx",
    "Engine/src/input/input.controller.cpp",
    "Engine/modules/project.input_controller.ixx",
    "Engine/src/project/project.input_controller.cpp",
    "Engine/modules/platform.filesystem.ixx",
    "Engine/modules/render.preview_grid.ixx",
    "Engine/src/epoch/epoch.engine_legacy.cpp",
    "Engine/src/epoch/implementation/epoch.project_play_scene.inl",
    "Engine/src/project/project.gameplay2d_runtime.cpp",
    "Engine/src/project/project.input_profile_contract.cpp",
)


def neutral(source):
    source = re.sub(r"^\s*(?:export )?(?:module|import)[^;]*;\s*$", "", source, flags=re.M)
    return source.replace("export ", "")


def consumer(s):
    registry, profile, implementation, inp, context, controller, controller_cpp, sampler, sampler_cpp, fs, preview, legacy, play, gameplay, profile_contract = s
    input_api = inp[inp.index("namespace epochengine::input"):inp.index("    // State storage")].replace("export ", "") + "}\n"
    active = "\n".join(definition(inp, sig) for sig in (
        "    export inline void set_active_profile(const InputProfile&",
        "    export inline void set_active_profile(ProfilePreset",
        "    export inline InputProfile active_profile_snapshot(",
        "    export inline ActionBinding binding_for(const InputProfile&",
        "    export inline ActionBinding binding_for(Action" )).replace("export ", "")
    actions = "\n".join(definition(context, sig) for sig in (
        "        [[nodiscard]] bool action_modifiers_satisfied_safe(",
        "        [[nodiscard]] bool action_held_safe(input::Action action,",
        "        [[nodiscard]] bool action_pressed_safe(input::Action action,"))
    # Use the real platform file publication function. This invokes disk APIs,
    # never a window, input, graphics or physical controller provider.
    atomic_replace = definition(fs, "    [[nodiscard]] inline bool atomic_replace_same_filesystem(")
    launch = definition(legacy, "        struct ProjectSceneLaunchOptions") + ";\n"
    launch += "\n".join(definition(legacy, sig) for sig in (
        "        [[nodiscard]] epochengine::previewgrid::CameraMode camera_mode_from_argument(",
        "        [[nodiscard]] ProjectSceneLaunchOptions parse_project_scene_launch(",
        "        [[nodiscard]] std::optional<input::Key> engine_key(",
        "        [[nodiscard]] epochengine::project_input::ModifierMask",
        "        [[nodiscard]] epochengine::project_input::InputSnapshot"))
    gestures = definition(preview, "    export [[nodiscard]] constexpr CameraNavigationGestures").replace("export ", "")
    methods = "\n".join(definition(play, sig) for sig in (
        "            void prepare_runtime_input(",
        "            [[nodiscard]] bool runtime_input_ready(",
        "            [[nodiscard]] float runtime_planar_axis(",
        "            void sample_runtime_input(",
        "            void shutdown_gameplay() noexcept",
        "            void advance_gameplay("))
    init_start = play.index("                const auto launch =")
    init = play[init_start:play.index("                const auto* profile =", init_start)]
    axes_start = play.index("                const bool altHeld =")
    axes = play[axes_start:play.index("                if ((flying", axes_start)]
    planet_start = play.index("                const bool panel=", play.index("void advance_planetary_exploration("))
    planet = play[planet_start:play.index("                controls.boost=", planet_start)]
    frame_start = play.index("                sample_runtime_input(ctx.get(),")
    frame = play[frame_start:play.index("\n\n", frame_start)]
    local_field = re.search(r"^.*input::InputProfile m_runtimeInputProfile[^\n]*", play, re.M)[0]
    # Assert source wiring instead of manufacturing a second runtime call path.
    assert play.count("                sample_runtime_input(ctx.get(),") == 1
    assert "set_active_profile(" not in play
    assert "input::mouse_look_sensitivity()" not in play and "input::wheel_zoom_step()" not in play
    assert "project_input::prepare_runtime_profile({" in gameplay
    # Keep the actual fixture owner's __FILE__ identity: its failure-retention
    # policy refuses roots outside that checkout, including external OS Temp.
    contract = '\n#line 1 "' + (ROOT/OWNERS[-1]).as_posix() + '"\n' + neutral(profile_contract)
    contract += '\n#line 1 "runtime-input.cpp"\n'
    return PREFIX + neutral(registry) + neutral(profile) + "\nnamespace epochengine::platform::filesystem {\n" + atomic_replace + "\n}\n" + neutral(implementation) + contract + input_api + "\nnamespace epochengine::input {std::mutex g_profileMutex;InputProfile g_activeProfile=make_default_profile();\n" + active + "\n}\n" + CONTEXT.replace("ACTUAL_ACTIONS", actions) + neutral(controller) + neutral(controller_cpp) + neutral(sampler) + neutral(sampler_cpp) + PREVIEW.replace("ACTUAL_GESTURES", gestures) + launch + PORTS + "\nstruct Owner {\n" + FIELDS.replace("LOCAL_FIELD", local_field) + methods + "\nvoid initialize(std::string_view project_payload){\n" + init + "\n}\nstd::array<float,3> navigation(const core::Context*ctx,bool mouse_right_down,bool captured){bool mouse_left_down=false,mouse_middle_down=false;bool navigationPointerCaptured=captured,projectGuiCapturesKeyboard=captured;\n" + axes + "\nreturn {rightInput,upInput,forwardInput};}\nControls planetary(core::Context*context,float dt,bool pointerCaptured,bool keyboardCaptured){gui::Vec2 mouse{};\n" + planet + "\nreturn controls;}\nvoid frame_input(std::shared_ptr<core::Context>ctx,bool projectGuiCapturesKeyboard,float dt){gui::Vec2 mouse_pos{};\n" + frame + "\n}\n};\n" + MAIN


PREFIX = r'''
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"CHECK line%d: %s\n",__LINE__,#__VA_ARGS__);std::exit(1);}}while(false)
// Diagnostic delivery is a test port; store/codec and disk operations stay real.
namespace epochengine::logger {
enum class LogLevel {Error};
inline std::vector<std::string> diagnostics;
struct DiagnosticLogger {
 void log(LogLevel,std::string_view message,std::source_location){
  diagnostics.emplace_back(message);
  std::fprintf(stderr,"%.*s\n",static_cast<int>(message.size()),message.data());
 }
};
inline DiagnosticLogger& get(std::string_view){static DiagnosticLogger log;return log;}
}
'''
CONTEXT = r'''
namespace input=epochengine::input;
namespace epochengine::core {
struct Context {
 bool focused=true;std::bitset<input::Key::Count> keys;
 bool has_input_focus_safe()const noexcept{return focused;}
 bool is_key_held_safe(input::Key key)const noexcept{return focused&&keys[key];}
 bool is_key_down_safe(input::Key key)const noexcept{return focused&&keys[key];}
 bool is_mouse_button_held_safe(input::MouseButton)const noexcept{return false;}
 bool is_mouse_button_down_safe(input::MouseButton)const noexcept{return false;}
 ACTUAL_ACTIONS
};}
namespace core=epochengine::core;
namespace project_input=epochengine::project_input;
'''
PREVIEW = r'''
namespace project_input_controller=epochengine::project_input_controller;
namespace controller_input=epochengine::controller_input;
namespace epochengine::previewgrid {
enum class CameraMode {Editor,FPS,Canvas2D};
struct CameraNavigationGestures {bool orbiting{},panning{},dollying{},flying{};};
ACTUAL_GESTURES
}
namespace previewgrid=epochengine::previewgrid;
namespace gui {struct Vec2{float x{},y{};};bool captured{};bool keyboard_input_captured(){return captured;}}
'''
PORTS = r'''
// Only2D gameplay delivery and UI-panel geometry are ports. Admission, codecs,
// evaluation, controller snapshots/sampler and runtime input bodies are real.
namespace epochengine::audio {
enum class PlaybackRuntimeCode {success,physical_queue_saturated};
std::string_view playback_runtime_code_name(PlaybackRuntimeCode){return "success";}
}
namespace epochengine::project_gameplay2d {std::string_view session_code_name(int){return "ready";}}
struct GameplayPort {
 project_input::CompiledInputProfile profile;unsigned deliveries{};std::uint64_t deliveredIndex{};
 const auto*input_profile()const{return &profile;}bool active()const{return true;}
 void close(){}std::string_view diagnostic()const{return "test delivery port";}
 struct Result{int code{};epochengine::audio::PlaybackRuntimeCode audio_code{};};
 Result advance(const project_input::InputSnapshot&s,std::span<const project_input::ActionImpulse>,double){++deliveries;deliveredIndex=s.frame_index;return {};}
};
struct Controls{double elapsed_seconds{},forward{},right{},rise{},yaw{},pitch{},roll{};};
'''
FIELDS = r'''
 std::string m_projectId="fixture",m_projectRoot,m_inputProfilePath,m_projectInputStatus,m_actorStatus,m_audioStatus;
 std::optional<project_input::CompiledInputProfile> m_projectInput;
 project_input::InputSnapshot m_projectInputSnapshot;
 project_input::ActionFrame m_projectActionFrame;
 project_input_controller::SnapshotSampler m_projectControllerSampler;
 std::uint64_t m_inputFrameIndex{};
 std::unique_ptr<GameplayPort>m_gameplayRuntime;
 std::optional<int>m_staticCanvasScene,m_runtimeCosts,m_runtimeBudgetAssessment;
 unsigned m_costFramesUntilRefresh{};
 std::array<std::optional<project_input::ActionImpulse>,6>m_pendingGuiActions;
 previewgrid::CameraMode m_cameraMode{previewgrid::CameraMode::Editor};
 input::ProfilePreset m_inputProfile{input::ProfilePreset::EditorDefault};
 LOCAL_FIELD
 Controls m_explorationInput{};bool panel{};
 bool runtime_panel_captures_pointer(gui::Vec2)const{return panel;}
'''
MAIN = r'''
bool same_profile(const input::InputProfile&a,const input::InputProfile&b){
 if(a.mouse_look_sensitivity!=b.mouse_look_sensitivity||a.wheel_zoom_step!=b.wheel_zoom_step)return false;
 for(std::size_t i=0;i<a.bindings.size();++i){const auto&x=a.bindings[i];const auto&y=b.bindings[i];
  if(x.action!=y.action||x.primary!=y.primary||x.secondary!=y.secondary||x.control!=y.control||x.shift!=y.shift||x.alt!=y.alt||x.mouse!=y.mouse)return false;}
 return true;
}
int main(int argc,char**argv){
 CHECK(argc==2);namespace fs=std::filesystem;namespace p=project_input;
 const fs::path root=argv[1];fs::create_directories(root);
 auto source=p::make_legacy_default_profile();CHECK(p::validate_profile_source(source)==p::ValidationCode::ready);
 const auto rebound=p::rebind_keyboard(source,p::stable_binding_id("legacy.move_y.w"),p::KeyCode::t);CHECK(rebound);
#if defined(_WIN32)
 // A real Windows handle denies DELETE sharing while allowing the owner's
 // reads. The artifact's MoveFileExW must fail after source publication.
 p::ProjectInputProfileStore lockedStore{"fixture",root/"artifact-locked"};
 auto ahead=rebound.source;ahead.revision.sequence=4u;CHECK(p::seal_profile_source(ahead)==p::ValidationCode::ready);
 const auto aheadArtifact=p::compile_profile("fixture",ahead),wantedArtifact=p::compile_profile("fixture",rebound.source);
 CHECK(aheadArtifact&&wantedArtifact&&lockedStore.save_source(source)&&lockedStore.publish_artifact(aheadArtifact.artifact));
 const auto lockedPath=lockedStore.artifact_path();
 const auto lock=::CreateFileW(lockedPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 CHECK(lock!=INVALID_HANDLE_VALUE);
 const auto probe=lockedPath.parent_path()/"native-error-probe.pending";
 {std::ofstream staged(probe,std::ios::binary);staged<<"diagnostic probe";CHECK(staged);}
 std::error_code observedError;
 CHECK(!epochengine::platform::filesystem::atomic_replace_same_filesystem(probe,lockedPath,observedError)&&observedError);
 CHECK(fs::remove(probe));epochengine::logger::diagnostics.clear();
 const auto failedPair=lockedStore.publish_source_and_artifact(rebound.source,wantedArtifact.artifact);
 CHECK(::CloseHandle(lock));
 CHECK(failedPair.code==p::StoreCode::atomic_replace_failure&&failedPair.source.code==p::StoreCode::ready
  &&failedPair.artifact.code==p::StoreCode::atomic_replace_failure);
 CHECK(lockedStore.load_source().source==rebound.source&&lockedStore.load_artifact().artifact==aheadArtifact.artifact);
 CHECK(epochengine::logger::diagnostics.size()==1u);
 const auto&diagnostic=epochengine::logger::diagnostics.front();
 CHECK(diagnostic.starts_with("Paired artifact replacement failed; error="+std::to_string(observedError.value())));
 CHECK(diagnostic.find("; category="+std::string{observedError.category().name()})!=std::string::npos
  &&diagnostic.find("; message="+observedError.message())!=std::string::npos);
 CHECK(diagnostic.find("; source="+lockedPath.string()+".pending.")!=std::string::npos
  &&diagnostic.find("; destination="+lockedPath.string())!=std::string::npos
  &&diagnostic.find("; source_length=")!=std::string::npos&&diagnostic.find("; destination_length=")!=std::string::npos);
 for(const auto&entry:fs::directory_iterator(lockedPath.parent_path()))CHECK(entry.path().filename()==lockedPath.filename());
 // Closing the handle allows a deliberate later publication. The failed
 // operation itself never retried or discarded either persisted version.
 CHECK(lockedStore.publish_source_and_artifact(rebound.source,wantedArtifact.artifact));
 CHECK(lockedStore.load_artifact().artifact==wantedArtifact.artifact);
#endif
 CHECK(p::project_input_profile_contract_failure()==p::ContractFailure::none);
 // Exercise the existing real fixture lifetime policy with checkout-local
 // TEMP: ordinary roots retire, explicitly failed roots retain their evidence.
 fs::path discarded,retained;
 {p::ContractRoot ordinary;CHECK(ordinary.valid());discarded=ordinary.path;}
 CHECK(!fs::exists(discarded));
 {p::ContractRoot failed;CHECK(failed.valid());retained=failed.path;failed.preserve_on_failure=true;
  std::ofstream evidence(retained/"synthetic-failure.txt");evidence<<"Intended fixture-retention contract; not a production failure.";CHECK(evidence);}
 CHECK(fs::is_regular_file(retained/"synthetic-failure.txt"));
 p::ProjectInputProfileStore store{"fixture",root/"good"};CHECK(store.save_source(rebound.source));
 const p::RuntimeProfileRequest req{"fixture",root/"good",std::string(p::canonical_source_path)};
 const auto admitted=p::prepare_runtime_profile(req);CHECK(admitted.artifact&&admitted.compiled_from_source&&!admitted.legacy_fallback);
 const auto again=p::prepare_runtime_profile(req);CHECK(again.artifact&&again.artifact==admitted.artifact);
 auto missing=req;missing.project_root=root/"missing";CHECK(!p::prepare_runtime_profile(missing).artifact);
 missing.fallback=p::RuntimeFallbackPolicy::allow_legacy_default;CHECK(p::prepare_runtime_profile(missing).legacy_fallback);
 auto wrong=req;wrong.logical_path="../wrong.epochinput";CHECK(!p::prepare_runtime_profile(wrong).artifact);
 fs::remove(store.source_path());const auto restored=p::prepare_runtime_profile(req);CHECK(restored.artifact&&restored.restored_from_artifact&&!restored.compiled_from_source);
 {std::ofstream bad(store.source_path(),std::ios::binary);bad<<"bad present source";}
 CHECK(!p::prepare_runtime_profile(req).artifact); // Never use still-valid stale artifact.
 fs::remove(store.source_path());{std::ofstream bad(store.artifact_path(),std::ios::binary);bad<<"bad artifact";}
 auto legacy=req;legacy.fallback=p::RuntimeFallbackPolicy::allow_legacy_default;CHECK(!p::prepare_runtime_profile(legacy).artifact);
 fs::remove(store.artifact_path());CHECK(store.save_source(rebound.source));
 input::set_active_profile(input::ProfilePreset::ArrowPilot);
 const auto editorProfile=input::active_profile_snapshot();
 Owner a,b;a.initialize("fixture|camera=fps|input=left-handed");b.initialize("fixture|camera=fps|input=runtime-wasd");
 CHECK(same_profile(input::active_profile_snapshot(),editorProfile));
 auto ctx=std::make_shared<core::Context>();ctx->keys[input::Key::I]=true;
 CHECK(a.navigation(ctx.get(),false,false)[2]==1&&b.navigation(ctx.get(),false,false)[2]==0);
 ctx->keys.reset();ctx->keys[input::Key::W]=true;
 CHECK(a.navigation(ctx.get(),false,false)[2]==0&&b.navigation(ctx.get(),false,false)[2]==1);
 ctx->focused=false;CHECK(b.navigation(ctx.get(),false,false)[2]==0);ctx->focused=true;
 CHECK(b.navigation(ctx.get(),false,true)[2]==0);gui::captured=true;CHECK(b.navigation(ctx.get(),false,false)[2]==0);gui::captured=false;
 a.m_projectRoot=(root/"good").string();a.m_inputProfilePath=p::canonical_source_path;a.prepare_runtime_input();CHECK(a.m_projectInput&&!a.runtime_input_ready());
 controller_input::reset();ctx->keys.reset();ctx->keys[input::Key::T]=true;
 a.frame_input(ctx,false,.016f);CHECK(a.m_inputFrameIndex==1&&a.runtime_input_ready());
 CHECK(a.navigation(ctx.get(),false,false)[2]==1&&a.planetary(ctx.get(),.016f,false,false).forward==1);
 CHECK(a.m_inputFrameIndex==1); // Both consumers reuse one physical sample.
 ctx->keys.reset();ctx->keys[input::Key::W]=true;a.frame_input(ctx,false,.016f);
 CHECK(a.navigation(ctx.get(),false,false)[2]==0); // Saved rebind wins, despite camera-preset W.
 ctx->keys.reset();ctx->keys[input::Key::T]=true;
 for(unsigned block=0;block<4;++block){ctx->focused=block!=0;gui::captured=block==1;a.panel=block==2;
  a.frame_input(ctx,block==3,.016f);CHECK(a.navigation(ctx.get(),false,false)[2]==0&&a.planetary(ctx.get(),.016f,false,block==3).forward==0);}
 ctx->focused=true;gui::captured=false;a.panel=false;
 controller_input::ProviderFrame hardware{};hardware.provider_available=true;hardware.devices[0].connected=true;hardware.devices[0].provider_id=19;
 hardware.devices[0].axes[std::size_t(controller_input::Axis::left_y)]=-32767;
 hardware.devices[0].buttons[std::size_t(controller_input::Button::south)]=true;CHECK(controller_input::publish_provider_frame(hardware));
 ctx->keys.reset();a.frame_input(ctx,false,.016f);CHECK(a.runtime_planar_axis(p::ActionSemantic::move_y,0)==1);
 CHECK(p::find_action(a.m_projectActionFrame,p::ActionSemantic::jump,*a.m_projectInput)->pressed);
 a.frame_input(ctx,false,.016f);CHECK(!p::find_action(a.m_projectActionFrame,p::ActionSemantic::jump,*a.m_projectInput)->pressed);
 hardware.devices[0].buttons[std::size_t(controller_input::Button::south)]=false;CHECK(controller_input::publish_provider_frame(hardware));
 hardware.devices[0].buttons[std::size_t(controller_input::Button::south)]=true;CHECK(controller_input::publish_provider_frame(hardware));
 a.frame_input(ctx,true,.016f);CHECK(!p::find_action(a.m_projectActionFrame,p::ActionSemantic::jump,*a.m_projectInput)->pressed);
 a.frame_input(ctx,false,.016f);CHECK(!p::find_action(a.m_projectActionFrame,p::ActionSemantic::jump,*a.m_projectInput)->pressed);
 // The2D path receives that same sample; it does not poll or re-sequence input.
 a.m_gameplayRuntime=std::make_unique<GameplayPort>();a.m_gameplayRuntime->profile=*a.m_projectInput;
 a.prepare_runtime_input();a.frame_input(ctx,false,.016f);CHECK(a.m_gameplayRuntime->deliveries==1&&a.m_gameplayRuntime->deliveredIndex==a.m_inputFrameIndex);
 const auto oldIndex=a.m_inputFrameIndex;a.navigation(ctx.get(),false,false);a.planetary(ctx.get(),.016f,false,false);CHECK(a.m_inputFrameIndex==oldIndex);
 // Stop/reopen clears owned profiles/samples, never the unrelated Editor preset.
 a.shutdown_gameplay();CHECK(!a.m_projectInput&&!a.m_projectActionFrame&&a.m_inputFrameIndex==0);
 CHECK(same_profile(input::active_profile_snapshot(),editorProfile));
 a.prepare_runtime_input();CHECK(a.m_projectInput);a.frame_input(ctx,false,.016f);CHECK(a.runtime_input_ready());
 {std::ofstream bad(store.source_path(),std::ios::binary);bad<<"malformed";}
 a.shutdown_gameplay();a.prepare_runtime_input();CHECK(!a.m_projectInput);a.frame_input(ctx,false,.016f);
 CHECK(!a.runtime_input_ready()&&a.navigation(ctx.get(),false,false)[2]==0&&a.planetary(ctx.get(),.016f,false,false).forward==0);
 CHECK(same_profile(input::active_profile_snapshot(),editorProfile));
}
'''


class ProjectRuntimeInputProfile(unittest.TestCase):
    def test_actual_admission_sampling_and_local_navigation(self):
        sources = [(ROOT/path).read_text(encoding="utf-8-sig") for path in OWNERS]
        before = {path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in OWNERS}
        report = ROOT/"Artifacts/Reports/project-input-handoff-oct31/repair-cpu"/str(time.time_ns())
        report.mkdir(parents=True, exist_ok=True)
        source = report/"runtime-input.cpp"
        source.write_text(consumer(sources), encoding="utf-8")
        includes = []
        if os.name == "nt":
            sdk = sorted(Path("C:/Program Files (x86)/Windows Kits/10/Include").glob("*"))[-1]
            includes = [sdk/"um", sdk/"shared"]
        records = compile_and_run(self, source, report, includes=includes,
            args=lambda configuration:(report/"files"/configuration,))
        after = {path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in OWNERS}
        self.assertEqual(before, after)
        (report/"result.json").write_text(json.dumps({"passed":True,"records":records,
            "source_before":before,"source_after":after,
            "limits":"Actual full profile/controller owners and runtime snippets, compiled as a CPU consumer. Context physical input, GUI capture, diagnostic delivery and2D delivery are explicit ports; no native focus/attachment/graphics/device acceptance."}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
