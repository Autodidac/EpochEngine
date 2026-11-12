"""Actual context action dispatch with independent runtime profiles; CPU ports only."""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition

ROOT = Path(__file__).resolve().parents[2]
OWNERS = ("Engine/modules/input/input.engine.ixx", "Engine/modules/core/core.context.ixx")


class ContextActionProfiles(unittest.TestCase):
    def test_actual_context_profiles_do_not_rebind_editor(self):
        source = [(ROOT / path).read_text(encoding="utf-8-sig") for path in OWNERS]
        before = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS}
        input_api = source[0][source[0].index("namespace epochengine::input"):
                              source[0].index("    // State storage")].replace("export ", "") + "}\n"
        input_functions = "\n".join(definition(source[0], marker) for marker in (
            "    export inline void set_active_profile(const InputProfile&",
            "    export inline void set_active_profile(ProfilePreset",
            "    export inline InputProfile active_profile_snapshot(",
            "    export inline ActionBinding binding_for(const InputProfile&",
            "    export inline ActionBinding binding_for(Action action)")) .replace("export ", "")
        context_functions = "\n".join(definition(source[1], marker) for marker in (
            "        [[nodiscard]] bool action_modifiers_satisfied_safe(",
            "        [[nodiscard]] bool action_held_safe(input::Action action,",
            "        [[nodiscard]] bool action_held_safe(input::Action action) const",
            "        [[nodiscard]] bool action_pressed_safe(input::Action action,",
            "        [[nodiscard]] bool action_pressed_safe(input::Action action) const"))
        code = r'''
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#define CHECK(x) do { if (!(x)) std::abort(); } while(false)
''' + input_api + r'''
namespace epochengine::input {
std::mutex g_profileMutex;
InputProfile g_activeProfile{make_default_profile()};
''' + input_functions + r'''
}
namespace epochengine {
struct ContextPort {
 bool focused=true,pointerFocused=true;
 std::array<bool,input::Key::Count> held{},pressed{};
 std::array<bool,input::MouseButton::MouseCount> mouseHeld{},mousePressed{};
 bool is_key_held_safe(input::Key key)const{return focused&&key<input::Key::Count&&held[key];}
 bool is_key_down_safe(input::Key key)const{return focused&&key<input::Key::Count&&pressed[key];}
 bool is_mouse_button_held_safe(input::MouseButton key)const{return focused&&pointerFocused&&mouseHeld[key];}
 bool is_mouse_button_down_safe(input::MouseButton key)const{return focused&&pointerFocused&&mousePressed[key];}
''' + context_functions + r'''
};
}
int main() {
 using namespace epochengine;using input::Action;using input::Key;using input::ProfilePreset;
 input::set_active_profile(ProfilePreset::EditorDefault);
 const auto editor=input::active_profile_snapshot();
 auto project=input::make_profile(ProfilePreset::LeftHanded);
 auto other=input::make_profile(ProfilePreset::RuntimeWASD);
 ContextPort first,second;
 first.held[Key::I]=first.pressed[Key::I]=true;
 second.held[Key::W]=second.pressed[Key::W]=true;
 CHECK(first.action_held_safe(Action::MoveForward,project));
 CHECK(first.action_pressed_safe(Action::MoveForward,project));
 CHECK(!first.action_held_safe(Action::MoveForward));
 CHECK(!first.action_held_safe(Action::MoveForward,other));
 CHECK(second.action_held_safe(Action::MoveForward,other));
 CHECK(second.action_held_safe(Action::MoveForward));
 CHECK(!second.action_held_safe(Action::MoveForward,project));
 first.pressed.fill(false);
 CHECK(first.action_held_safe(Action::MoveForward,project));
 CHECK(!first.action_pressed_safe(Action::MoveForward,project));
 project.bindings[input::action_index(Action::MoveForward)]=input::key_binding(
     Action::MoveForward,Key::P,Key::L,true,true,true);
 first.held.fill(false);first.held[Key::L]=true;
 CHECK(!first.action_held_safe(Action::MoveForward,project));
 first.held[Key::RightControl]=first.held[Key::LeftShift]=first.held[Key::RightAlt]=true;
 CHECK(first.action_held_safe(Action::MoveForward,project));
 first.focused=false;
 CHECK(!first.action_held_safe(Action::MoveForward,project));
 first.focused=true;
 project.bindings[input::action_index(Action::ContextMenu)]=input::mouse_binding(
     Action::ContextMenu,input::MouseButton::MouseMiddle,true);
 first.mouseHeld[input::MouseButton::MouseMiddle]=first.mousePressed[input::MouseButton::MouseMiddle]=true;
 CHECK(first.action_held_safe(Action::ContextMenu,project));
 CHECK(first.action_pressed_safe(Action::ContextMenu,project));
 CHECK(!first.action_held_safe(Action::ContextMenu));
 first.pointerFocused=false;
 CHECK(!first.action_held_safe(Action::ContextMenu,project));
 CHECK(!first.action_pressed_safe(Action::ContextMenu,project));
 CHECK(!first.action_held_safe(Action::Count,project));
 CHECK(!first.action_pressed_safe(Action::Count,project));
 const auto unchanged=input::active_profile_snapshot();
 CHECK(unchanged.bindings[input::action_index(Action::MoveForward)].primary==editor.bindings[input::action_index(Action::MoveForward)].primary);
 CHECK(second.action_held_safe(Action::MoveForward));
 return 0;
}
'''
        report = ROOT / "Artifacts/Reports/project-input-handoff-oct31/context-actions-cpu"
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / "context_profiles_contract.cpp"
        fixture.write_text(code, encoding="utf-8")
        results = compile_and_run(self, fixture, report, windows_flags=("/WX",))
        after = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS}
        self.assertEqual(before, after)
        (report / "result.json").write_text(json.dumps(dict(
            configurations=results, sources=after,
            scope="Actual input API/profile builders and context action dispatch; native focus/key polling are recording ports, not native acceptance."), indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
