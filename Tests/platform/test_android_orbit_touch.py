"""Exercise the Android SDL touch router and the actual scene hit-test on CPU."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes


ROOT = Path(__file__).resolve().parents[2]


def fixture():
    state = (ROOT / "Engine/modules/sdl/sdl.state.ixx").read_text(encoding="utf-8")
    state = state.split("    enum class AndroidTouchTarget", 1)[1].split("#endif", 1)[0]
    adapter = (ROOT / "Engine/src/renderers/sdl/core.context_sdl.cpp").read_text(encoding="utf-8")
    scene = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8")
    hit_test = definition(scene, "            static sdlcontext::state::AndroidTouchTarget android_orbit_touch_target(")
    panel = definition(scene, "            [[nodiscard]] bool runtime_panel_captures_pointer(")
    flight_bounds = definition(scene, "            [[nodiscard]] std::optional<gui::WidgetBounds> flight_touch_bounds(")
    flight_enabled = definition(scene, "            [[nodiscard]] bool packaged_planetary_flight_enabled()")
    return r'''#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>
using SDL_TouchID = std::uint64_t;
using SDL_FingerID = std::uint64_t;
constexpr auto SDL_MOUSE_TOUCHID = static_cast<SDL_TouchID>(-1);
constexpr auto SDL_PEN_TOUCHID = static_cast<SDL_TouchID>(-2);
constexpr auto genuine = static_cast<SDL_TouchID>(-3);
enum {SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_MOTION, SDL_EVENT_FINGER_UP, SDL_EVENT_FINGER_CANCELED};
struct SDL_TouchFingerEvent { SDL_TouchID touchID; SDL_FingerID fingerID; unsigned windowID; float x,y; };
struct SDL_Event { unsigned type; SDL_TouchFingerEvent tfinger; };
constexpr unsigned s_windowId=11;
int s_renderer{};
bool SDL_ConvertEventToRenderCoordinates(int,SDL_Event* event) {
    event->tfinger.x *= 2; event->tfinger.y *= 2; return true;
}
namespace epochengine {
namespace core {struct Context {};}
namespace gui {
enum class EventType {None,MouseMove,MouseDown,MouseUp,FocusLost};
struct Vec2 {float x{},y{};};
struct WidgetBounds {Vec2 position,size;};
struct InputEvent {EventType type; Vec2 mouse_pos{};};
struct DragSurfaceOptions {float drag_threshold=4;};
std::vector<InputEvent> observed;
void push_input_for_context(const core::Context*,InputEvent event){observed.push_back(event);}
}
namespace sdlcontext::state {
enum class AndroidTouchTarget''' + state + r'''
}
}
using namespace epochengine;
using namespace epochengine::sdlcontext::state;
void send_android_gui_input(core::Context& context,const gui::InputEvent& event) noexcept {
    gui::push_input_for_context(&context,event);
}
''' + definition(adapter, "    void cancel_android_touch(") + "\n" + definition(adapter, "    void dispatch_android_orbit_touch(") + r'''
struct ProjectPlayScene {
    gui::WidgetBounds m_runtimeGuiBounds{{12,80},{500,700}};
    struct Pose { bool allow_flight{}; };
    std::optional<Pose> m_packagedPlanetaryPose, m_packagedShoreline;
    bool m_explorationController{}, m_runtimeControlsVisible{};
    struct ProjectGui { bool pointer_captured(gui::Vec2 p) const { return p.x>310 && p.y<220; } };
    const ProjectGui* m_projectGuiRuntime{};
    gui::WidgetBounds runtime_panel_bounds() const { return {{36,104},{220,120}}; }
''' + panel + "\n" + flight_bounds + "\n" + flight_enabled + "\n" + hit_test + r'''
};
void check(bool value,int line) {
    if (!value) {std::fprintf(stderr,"Android touch check failed at line %d\n",line);std::exit(1);}
}
#define CHECK(value) check(static_cast<bool>(value),__LINE__)
int main() {
    core::Context context;
    ProjectPlayScene scene, unrelated;
    const ProjectPlayScene::ProjectGui authored;
    scene.m_projectGuiRuntime=&authored;
    const auto sample=[&]{return sample_android_orbit_touch(&scene,ProjectPlayScene::android_orbit_touch_target);};
    const auto suspend=[&]{return sample_android_orbit_touch(&scene,nullptr);};
    const auto event=[&](unsigned type,float x,float y,SDL_FingerID finger=1,SDL_TouchID device=genuine,unsigned window=s_windowId){
        dispatch_android_orbit_touch(context,{type,{device,finger,window,x/2,y/2}});
    };
    CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,290,400);
    event(SDL_EVENT_FINGER_MOTION,292,400);CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_MOTION,320,420);const auto first=sample();CHECK(first.x==30&&first.y==20);
    event(SDL_EVENT_FINGER_MOTION,350,430);event(SDL_EVENT_FINGER_UP,0,0);
    const auto second=sample();CHECK(second.x==30&&second.y==10);CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,290,400);
    event(SDL_EVENT_FINGER_MOTION,320,420);event(SDL_EVENT_FINGER_MOTION,350,430);event(SDL_EVENT_FINGER_UP,0,0);
    const auto collapsed=sample();CHECK(collapsed.x==first.x+second.x&&collapsed.y==first.y+second.y);

    event(SDL_EVENT_FINGER_DOWN,290,400);
    event(SDL_EVENT_FINGER_DOWN,380,450,2);
    event(SDL_EVENT_FINGER_MOTION,310,410,1,75);
    event(SDL_EVENT_FINGER_MOTION,310,410,2);
    event(SDL_EVENT_FINGER_MOTION,310,410,1,genuine,12);
    CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_UP,0,0);event(SDL_EVENT_FINGER_MOTION,330,410,2);
    CHECK(sample().x==0);

    event(SDL_EVENT_FINGER_DOWN,50,130);event(SDL_EVENT_FINGER_MOTION,290,600);event(SDL_EVENT_FINGER_UP,0,0);
    CHECK(sample().x==0&&gui::observed.size()==3);
    CHECK(gui::observed[0].type==gui::EventType::MouseDown&&gui::observed[2].mouse_pos.x==290);
    gui::observed.clear();
    event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,50,130);event(SDL_EVENT_FINGER_UP,0,0);
    CHECK(sample().x==-240&&gui::observed.empty());

    event(SDL_EVENT_FINGER_DOWN,320,180);event(SDL_EVENT_FINGER_UP,320,180);
    CHECK(sample().x==0&&gui::observed.size()==2);
    gui::observed.clear();
    event(SDL_EVENT_FINGER_DOWN,290,55);event(SDL_EVENT_FINGER_MOTION,360,400);event(SDL_EVENT_FINGER_UP,0,0);
    event(SDL_EVENT_FINGER_DOWN,290,400,1,SDL_MOUSE_TOUCHID);event(SDL_EVENT_FINGER_MOTION,330,420);
    event(SDL_EVENT_FINGER_DOWN,290,400,1,SDL_PEN_TOUCHID);event(SDL_EVENT_FINGER_MOTION,330,420);
    CHECK(sample().x==0&&gui::observed.empty());

    event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,340,420);
    event(SDL_EVENT_FINGER_CANCELED,0,0);event(SDL_EVENT_FINGER_MOTION,400,460);
    CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,340,420);
    cancel_android_touch(&context);event(SDL_EVENT_FINGER_DOWN,375,430);
    CHECK(sample().x==0);event(SDL_EVENT_FINGER_MOTION,440,435);CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,50,130);cancel_android_touch(&context);
    CHECK(gui::observed.back().type==gui::EventType::FocusLost);
    CHECK(sample().x==0);event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,320,420);
    retire_android_orbit_touch(&unrelated);CHECK(sample().x==30);
    retire_android_orbit_touch(&scene);CHECK(!android_orbit_touch_state().owner);

    CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,340,425);
    auto motion=suspend();CHECK(motion.x==0&&motion.y==0);
    CHECK(suspend().x==0&&android_orbit_touch_state().owner==&scene);
    CHECK(!android_orbit_touch_state().device && sample().x==0);
    event(SDL_EVENT_FINGER_MOTION,400,440);event(SDL_EVENT_FINGER_UP,0,0);
    CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,312,407);
    event(SDL_EVENT_FINGER_UP,0,0);CHECK(suspend().x==0&&sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,290,400);event(SDL_EVENT_FINGER_MOTION,312,407);
    event(SDL_EVENT_FINGER_UP,0,0);motion=sample();CHECK(motion.x==22&&motion.y==7);

    gui::observed.clear();
    event(SDL_EVENT_FINGER_DOWN,50,130);event(SDL_EVENT_FINGER_MOTION,55,135);
    CHECK(suspend().x==0&&sample().x==0);
    event(SDL_EVENT_FINGER_MOTION,355,500);event(SDL_EVENT_FINGER_UP,0,0,2);
    event(SDL_EVENT_FINGER_UP,0,0,1,genuine,12);CHECK(gui::observed.size()==2);
    event(SDL_EVENT_FINGER_UP,0,0);event(SDL_EVENT_FINGER_UP,0,0);
    CHECK(gui::observed.size()==3&&gui::observed.back().type==gui::EventType::FocusLost);
    CHECK(!android_orbit_touch_state().device&&sample().x==0);

    gui::observed.clear();
    event(SDL_EVENT_FINGER_DOWN,50,130);CHECK(suspend().x==0);
    event(SDL_EVENT_FINGER_CANCELED,0,0);
    CHECK(gui::observed.size()==2&&gui::observed.back().type==gui::EventType::FocusLost);
    CHECK(!android_orbit_touch_state().device&&sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,50,130);CHECK(suspend().x==0&&suspend().x==0);
    cancel_android_touch(&context);event(SDL_EVENT_FINGER_UP,0,0);
    CHECK(gui::observed.size()==4&&gui::observed.back().type==gui::EventType::FocusLost);
    CHECK(sample().x==0);
    event(SDL_EVENT_FINGER_DOWN,50,130);event(SDL_EVENT_FINGER_UP,0,0);
    CHECK(gui::observed.size()==6&&gui::observed.back().type==gui::EventType::MouseUp);

    scene.m_explorationController=true;
    scene.m_packagedPlanetaryPose=ProjectPlayScene::Pose{true};
    event(SDL_EVENT_FINGER_DOWN,280,672);CHECK(sample().rise==1&&sample().rise==1);
    event(SDL_EVENT_FINGER_DOWN,430,672,2);CHECK(sample().rise==1);
    event(SDL_EVENT_FINGER_UP,0,0);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_UP,0,0,2);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_DOWN,430,672);CHECK(sample().rise==-1);
    event(SDL_EVENT_FINGER_MOTION,280,672);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_MOTION,430,672);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_UP,0,0);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_DOWN,430,672);CHECK(sample().rise==-1);
    CHECK(suspend().rise==0&&sample().rise==0);
    event(SDL_EVENT_FINGER_UP,0,0);
    event(SDL_EVENT_FINGER_DOWN,430,672);CHECK(sample().rise==-1);
    cancel_android_touch(&context);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_DOWN,285,610);event(SDL_EVENT_FINGER_MOTION,305,635);
    motion=sample();CHECK(motion.x==20&&motion.y==25&&motion.rise==0);
    event(SDL_EVENT_FINGER_UP,0,0);
    scene.m_runtimeControlsVisible=true;
    event(SDL_EVENT_FINGER_DOWN,430,672);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_MOTION,415,611);CHECK(sample().x==0&&sample().y==0);
    event(SDL_EVENT_FINGER_UP,0,0);
    scene.m_runtimeControlsVisible=false;
    scene.m_packagedPlanetaryPose->allow_flight=false;
    event(SDL_EVENT_FINGER_DOWN,430,672);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_MOTION,415,611);CHECK(sample().x==0&&sample().y==0);
    event(SDL_EVENT_FINGER_UP,0,0);
    scene.m_packagedPlanetaryPose.reset();
    scene.m_packagedShoreline.emplace();
    event(SDL_EVENT_FINGER_DOWN,280,672);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_UP,0,0);
    scene.m_packagedShoreline->allow_flight=true;
    event(SDL_EVENT_FINGER_DOWN,280,672);CHECK(sample().rise==1);
    event(SDL_EVENT_FINGER_UP,0,0);CHECK(sample().rise==0);
    event(SDL_EVENT_FINGER_DOWN,285,610);event(SDL_EVENT_FINGER_MOTION,305,635);
    motion=sample();CHECK(motion.x==20&&motion.y==25&&motion.rise==0);
    event(SDL_EVENT_FINGER_UP,0,0);
}
'''


class AndroidOrbitTouch(unittest.TestCase):
    def test_contact_ownership_and_cancellation(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU fixture")
        versions = msvc_2022_toolsets()
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = windows_sdk_includes(sdk)
        if not versions or not kits:
            self.skipTest("MSVC/SDK unavailable")
        tool, kit = versions[-1], kits[-1]
        code = fixture()
        identity = hashlib.sha256(code.encode()).hexdigest()
        root = ROOT / "Artifacts/Reports/android-orbit-touch" / identity[:12]
        root.mkdir(parents=True, exist_ok=True)
        source = root / "consumer.cpp"
        source.write_text(code, encoding="utf-8")
        exe = root / "consumer.exe"
        command = [str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/EHsc", "/utf-8",
                   "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"),
                   "/Fo" + str(root / "consumer.obj"), "/Fe" + str(exe), str(source), "/link",
                   "/LIBPATH:" + str(tool / "lib/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"),
                   "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")]
        env = dict(os.environ, TMP=str(root), TEMP=str(root))
        build = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=45)
        (root / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, timeout=15)
        (root / "result.json").write_text(json.dumps({"fixture_sha256": identity, "exit": run.returncode}) + "\n")
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
