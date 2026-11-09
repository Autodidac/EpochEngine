"""Actual context modifier/action dispatch through counted CPU input ports.

The production safe key/mouse wrappers retain their focus checks. Native focus
and input callbacks are recording ports; this does not accept native latency.
"""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition


ROOT = Path(__file__).resolve().parents[2]
OWNERS = ("Engine/modules/input.engine.ixx", "Engine/modules/core.context.ixx")


class ContextModifierPolling(unittest.TestCase):
    def test_actual_required_modifier_queries_and_action_focus_guards(self):
        source = [(ROOT / path).read_text(encoding="utf-8-sig") for path in OWNERS]
        before = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS}
        input_api = source[0][source[0].index("namespace epochengine::input"):
                              source[0].index("    // State storage")].replace("export ", "") + "}\n"
        binding_lookup = definition(source[0], "    export inline ActionBinding binding_for(const InputProfile&").replace("export ", "")
        context_functions = "\n".join(definition(source[1], marker) for marker in (
            "        bool is_key_held_safe(",
            "        bool is_key_down_safe(",
            "        bool is_mouse_button_held_safe(",
            "        bool is_mouse_button_down_safe(",
            "        [[nodiscard]] bool action_modifiers_satisfied_safe(",
            "        [[nodiscard]] bool action_held_safe(input::Action action,",
            "        [[nodiscard]] bool action_pressed_safe(input::Action action,"))
        code = r'''
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
// Exercise the production Windows focus branches with CPU ports on every host.
#ifndef _WIN32
#define _WIN32 1
#endif
#ifdef EPOCH_MAIN_HEADLESS
#undef EPOCH_MAIN_HEADLESS
#endif
#define CHECK(...) do { if (!(__VA_ARGS__)) return __LINE__; } while (false)
''' + input_api + r'''
namespace epochengine::input {
''' + binding_lookup + r'''
inline unsigned fallbackHeldQueries{},fallbackPressedQueries{};
bool is_mouse_button_held(MouseButton) {++fallbackHeldQueries;return false;}
bool is_mouse_button_down(MouseButton) {++fallbackPressedQueries;return false;}
}
namespace epochengine {
struct ContextPort {
    void* hwnd{reinterpret_cast<void*>(1)};
    void* windowData{};
    bool focused=true,pointerFocused=true;
    std::size_t focusAcceptLimit=std::numeric_limits<std::size_t>::max();
    mutable std::size_t focusQueries{},pointerFocusQueries{};
    mutable std::vector<input::Key> heldQueries,pressedQueries;
    mutable std::vector<input::MouseButton> mouseHeldQueries,mousePressedQueries;
    std::array<bool,input::Key::Count> held{},pressed{};
    std::array<bool,input::MouseButton::MouseCount> mouseHeld{},mousePressed{};
    std::function<bool(input::Key)> is_key_held=[this](input::Key key){
        heldQueries.push_back(key);return held[key];};
    std::function<bool(input::Key)> is_key_down=[this](input::Key key){
        pressedQueries.push_back(key);return pressed[key];};
    std::function<bool(input::MouseButton)> is_mouse_button_held=[this](input::MouseButton button){
        mouseHeldQueries.push_back(button);return mouseHeld[button];};
    std::function<bool(input::MouseButton)> is_mouse_button_down=[this](input::MouseButton button){
        mousePressedQueries.push_back(button);return mousePressed[button];};
    bool has_input_focus_safe() const noexcept {
        ++focusQueries;return focused&&focusQueries<=focusAcceptLimit;}
    bool has_pointer_input_safe() const noexcept {
        ++pointerFocusQueries;return focused&&pointerFocused;}
    void reset_queries() {
        focusQueries=pointerFocusQueries=0;
        heldQueries.clear();pressedQueries.clear();
        mouseHeldQueries.clear();mousePressedQueries.clear();
    }
''' + context_functions + r'''
};
}
int main() {
    using namespace epochengine;
    using input::Action;using input::Key;using input::MouseButton;
    constexpr std::array<Key,6> modifiers={Key::LeftControl,Key::RightControl,
        Key::LeftShift,Key::RightShift,Key::LeftAlt,Key::RightAlt};
    constexpr auto action=Action::MoveForward;
    auto profile=input::make_default_profile();
    auto& binding=profile.bindings[input::action_index(action)];
    binding=input::key_binding(action,Key::W,Key::Up);

    ContextPort unmodified;
    unmodified.held[Key::W]=true;
    CHECK(unmodified.action_held_safe(action,profile));
    std::size_t modifierQueries{};
    for(auto key:unmodified.heldQueries)for(auto modifier:modifiers)if(key==modifier)++modifierQueries;
    {
        std::ofstream receipt("unmodified-action.json");
        receipt<<"{\"modifier_held_polls\":"<<modifierQueries
            <<",\"total_held_polls\":"<<unmodified.heldQueries.size()
            <<",\"focus_queries\":"<<unmodified.focusQueries<<"}\n";
    }
    CHECK(modifierQueries==0);
    CHECK(unmodified.heldQueries==std::vector<Key>{Key::W}&&unmodified.focusQueries==1);

    // All eight requirement masks, all 64 physical left/right states and both
    // focus outcomes. Extra held modifiers remain acceptable.
    std::size_t matrixCases{};
    for(unsigned required=0;required<8;++required)for(unsigned physical=0;physical<64;++physical)
    for(bool focused:{false,true}) {
        ContextPort port;port.focused=focused;
        binding.control=(required&1)!=0;binding.shift=(required&2)!=0;binding.alt=(required&4)!=0;
        for(std::size_t index=0;index<modifiers.size();++index)port.held[modifiers[index]]=(physical&(1u<<index))!=0;
        unsigned satisfied{};
        for(unsigned group=0;group<3;++group)if(focused&&(physical&(3u<<(2*group))))satisfied|=1u<<group;
        CHECK(port.action_modifiers_satisfied_safe(binding)==((required&satisfied)==required));

        // Required pairs are queried in their existing order. A held left key
        // avoids its right peer; the first missing required pair stops dispatch.
        std::vector<Key> expected;
        for(unsigned group=0;group<3;++group) {
            if(!(required&(1u<<group)))continue;
            expected.push_back(modifiers[2*group]);
            if(!focused||!(physical&(1u<<(2*group))))expected.push_back(modifiers[2*group+1]);
            if(!(satisfied&(1u<<group)))break;
        }
        CHECK(port.focusQueries==expected.size());
        CHECK(port.heldQueries==(focused?expected:std::vector<Key>{}));
        CHECK(port.pressedQueries.empty()&&port.pointerFocusQueries==0);
        ++matrixCases;
    }
    CHECK(matrixCases==1024);

    // Focus can be refused between required modifiers: no shared focus sample.
    binding=input::key_binding(action,Key::W,Key::Up,true,true);
    ContextPort changing;
    changing.held[Key::LeftControl]=changing.held[Key::LeftShift]=changing.held[Key::W]=true;
    changing.focusAcceptLimit=1;
    CHECK(!changing.action_held_safe(action,profile));
    CHECK(changing.focusQueries==3&&changing.heldQueries==std::vector<Key>{Key::LeftControl});
    changing.reset_queries();changing.focusAcceptLimit=std::numeric_limits<std::size_t>::max();
    CHECK(changing.action_held_safe(action,profile));
    CHECK(changing.heldQueries==std::vector<Key>({Key::LeftControl,Key::LeftShift,Key::W}));

    // Modifier checks use held state even for a pressed action. Primary and
    // secondary keys retain their separate held/pressed and short-circuit rules.
    binding=input::key_binding(action,Key::W,Key::Up,true);
    ContextPort keyboard;
    keyboard.held[Key::RightControl]=keyboard.held[Key::Up]=true;
    keyboard.pressed[Key::Up]=true;
    CHECK(keyboard.action_held_safe(action,profile));
    CHECK(keyboard.heldQueries==std::vector<Key>({Key::LeftControl,Key::RightControl,Key::W,Key::Up}));
    keyboard.reset_queries();
    CHECK(keyboard.action_pressed_safe(action,profile));
    CHECK(keyboard.heldQueries==std::vector<Key>({Key::LeftControl,Key::RightControl}));
    CHECK(keyboard.pressedQueries==std::vector<Key>({Key::W,Key::Up})&&keyboard.focusQueries==4);
    keyboard.reset_queries();keyboard.held[Key::W]=keyboard.pressed[Key::W]=true;
    CHECK(keyboard.action_held_safe(action,profile));
    CHECK(keyboard.heldQueries==std::vector<Key>({Key::LeftControl,Key::RightControl,Key::W}));
    keyboard.reset_queries();
    CHECK(keyboard.action_pressed_safe(action,profile));
    CHECK(keyboard.pressedQueries==std::vector<Key>{Key::W});
    keyboard.reset_queries();keyboard.pressed.fill(false);
    CHECK(keyboard.action_held_safe(action,profile)&&!keyboard.action_pressed_safe(action,profile));
    keyboard.reset_queries();keyboard.held[Key::RightControl]=false;keyboard.pressed[Key::RightControl]=true;
    CHECK(!keyboard.action_pressed_safe(action,profile));
    CHECK(keyboard.heldQueries==std::vector<Key>({Key::LeftControl,Key::RightControl})&&keyboard.pressedQueries.empty());

    // Removing modifier work must not bypass the base key or pointer authority.
    binding=input::key_binding(action,Key::W,Key::Up);
    for(bool windowDataOnly:{false,true}) {
        ContextPort refused;refused.focused=false;refused.held[Key::W]=refused.pressed[Key::W]=true;
        if(windowDataOnly){refused.windowData=refused.hwnd;refused.hwnd=nullptr;}
        CHECK(!refused.action_held_safe(action,profile)&&!refused.action_pressed_safe(action,profile));
        CHECK(refused.focusQueries==4&&refused.heldQueries.empty()&&refused.pressedQueries.empty());
    }
    ContextPort secondaryRefused;secondaryRefused.held[Key::Up]=true;secondaryRefused.focusAcceptLimit=1;
    CHECK(!secondaryRefused.action_held_safe(action,profile));
    CHECK(secondaryRefused.focusQueries==2&&secondaryRefused.heldQueries==std::vector<Key>{Key::W});
    ContextPort missingCallbacks;missingCallbacks.is_key_held={};missingCallbacks.is_key_down={};
    CHECK(missingCallbacks.action_modifiers_satisfied_safe(binding));
    CHECK(missingCallbacks.focusQueries==0);
    CHECK(!missingCallbacks.action_held_safe(action,profile)&&!missingCallbacks.action_pressed_safe(action,profile));
    binding.control=true;
    CHECK(!missingCallbacks.action_modifiers_satisfied_safe(binding));

    // Existing mouse-first dispatch and keyboard fallback both remain scoped.
    binding=input::mouse_binding(action,MouseButton::MouseRight,false,true);
    binding.primary=Key::W;
    ContextPort mouse;
    mouse.held[Key::RightShift]=mouse.held[Key::W]=mouse.pressed[Key::W]=true;
    mouse.mouseHeld[MouseButton::MouseRight]=mouse.mousePressed[MouseButton::MouseRight]=true;
    CHECK(mouse.action_held_safe(action,profile));
    CHECK(mouse.heldQueries==std::vector<Key>({Key::LeftShift,Key::RightShift}));
    CHECK(mouse.mouseHeldQueries==std::vector<MouseButton>{MouseButton::MouseRight}&&mouse.pointerFocusQueries==1);
    mouse.reset_queries();
    CHECK(mouse.action_pressed_safe(action,profile));
    CHECK(mouse.heldQueries==std::vector<Key>({Key::LeftShift,Key::RightShift})&&mouse.pressedQueries.empty());
    mouse.reset_queries();mouse.pointerFocused=false;
    CHECK(mouse.action_held_safe(action,profile));
    CHECK(mouse.mouseHeldQueries.empty()&&mouse.heldQueries.back()==Key::W);
    mouse.reset_queries();
    CHECK(mouse.action_pressed_safe(action,profile));
    CHECK(mouse.mousePressedQueries.empty()&&mouse.pressedQueries==std::vector<Key>{Key::W});
    binding.primary=Key::Unknown;
    mouse.reset_queries();
    CHECK(!mouse.action_held_safe(action,profile)&&!mouse.action_pressed_safe(action,profile));
    CHECK(mouse.mouseHeldQueries.empty()&&mouse.mousePressedQueries.empty());
    mouse.reset_queries();mouse.pointerFocused=true;mouse.focused=false;
    CHECK(!mouse.action_held_safe(action,profile)&&!mouse.action_pressed_safe(action,profile));
    CHECK(mouse.pointerFocusQueries==0&&mouse.mouseHeldQueries.empty()&&mouse.mousePressedQueries.empty());
    binding.shift=false;
    mouse.reset_queries();
    CHECK(!mouse.action_held_safe(action,profile)&&!mouse.action_pressed_safe(action,profile));
    CHECK(mouse.focusQueries==0&&mouse.pointerFocusQueries==2&&mouse.mouseHeldQueries.empty()&&mouse.mousePressedQueries.empty());
    mouse.reset_queries();mouse.focused=true;mouse.is_mouse_button_held={};mouse.is_mouse_button_down={};
    CHECK(!mouse.action_held_safe(action,profile)&&!mouse.action_pressed_safe(action,profile));
    CHECK(input::fallbackHeldQueries==1&&input::fallbackPressedQueries==1);

    ContextPort invalid;
    CHECK(!invalid.action_held_safe(Action::Count,profile)&&!invalid.action_pressed_safe(Action::Count,profile));
    CHECK(invalid.focusQueries==0&&invalid.pointerFocusQueries==0);
    return 0;
}
'''
        report = ROOT / "Artifacts/Reports/context-modifier-poll86-nov07" / before[OWNERS[1]][:12]
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / "core.context_modifier_contract.cpp"
        fixture.write_text(code, encoding="utf-8")
        receipt = dict(sources=before, scope="Actual production modifier/action and safe key/mouse wrappers; counted focus/input ports only, no native or hitch acceptance.")
        (report / "source.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        results = compile_and_run(self, fixture, report, windows_flags=("/WX",))
        after = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS}
        self.assertEqual(before, after)
        receipt.update(configurations=results, modifier_matrix_cases_per_configuration=1024,
                       unmodified_action=json.loads((report / "unmodified-action.json").read_text(encoding="utf-8")))
        (report / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
