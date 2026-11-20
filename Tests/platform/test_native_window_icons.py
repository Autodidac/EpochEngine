"""Exercise the promoted-shell icon owner without creating a native window."""
import hashlib
import json
from pathlib import Path
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition


ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / 'Engine/src/renderers/host/implementation'


class NativeWindowIcons(unittest.TestCase):
    def test_promotion_assigns_the_two_icon_sizes_to_the_same_visible_owner(self):
        interaction = (HOST / 'host.window_interaction.inl').read_text(encoding='utf-8')
        dispatch = (HOST / 'host.native_dispatch.inl').read_text(encoding='utf-8')
        owner = definition(interaction, '    inline void apply_epoch_window_icon(')
        placement = definition(interaction, '    inline void position_top_level_shell(')
        direct = dispatch.split('else if (::GetParent(hwnd) == drag.originalParent)', 1)[1]
        direct = direct.split('else if (window && backend_requires_owner_thread_dock_commands(window))', 1)[0]
        for path in (placement, direct):
            self.assertLess(path.index('::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, 0)'),
                            path.index('apply_epoch_window_icon(hwnd)'))
            self.assertLess(path.index('apply_epoch_window_icon(hwnd)'), path.index('SWP_FRAMECHANGED'))
        self.assertIn('position_top_level_shell(\n            window->host_hwnd,',
                      definition(interaction, '    inline void undock_sfml_proxy_window('))
        self.assertIn('position_top_level_shell(target, x, y, width, height)',
                      definition(interaction, '    [[nodiscard]] inline bool post_owner_thread_dock_command('))

        code = r'''
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <string_view>
#include "resource.h"

#define CHECK(...) do { if (!(__VA_ARGS__)) std::_Exit(__LINE__); } while (false)
#define MAKEINTRESOURCEW(id) reinterpret_cast<const wchar_t*>(static_cast<std::uintptr_t>(id))

using DWORD = std::uint32_t;
using HICON = void*;
using HINSTANCE = void*;
using LPARAM = std::intptr_t;
struct Window {
    DWORD thread{}, process{};
    bool child{};
    Window* root{};
    HICON big{}, small{};
};
using HWND = Window*;
constexpr unsigned WM_SETICON = 0x80u, ICON_BIG = 1u, ICON_SMALL = 0u, IMAGE_ICON = 1u;
constexpr int GWL_STYLE = -16, GA_ROOT = 2, WS_CHILD = 0x40000000,
    SM_CXSMICON = 49, SM_CYSMICON = 50;
unsigned loads{}, applications{}, readbacks{}, destroySmall{};
int largeIcon{}, smallIcon{}, imageModule{};
DWORD GetWindowThreadProcessId(HWND window, DWORD* process) {
    *process = window->process;
    return window->thread;
}
DWORD GetCurrentThreadId() { return 11u; }
DWORD GetCurrentProcessId() { return 22u; }
int GetWindowLongPtrW(HWND window, int index) {
    CHECK(index == GWL_STYLE);
    return window->child ? WS_CHILD : 0;
}
HWND GetAncestor(HWND window, int relation) {
    CHECK(relation == GA_ROOT);
    return window->root ? window->root : window;
}
HINSTANCE GetModuleHandleW(const wchar_t* module) {
    CHECK(module == nullptr);
    return &imageModule;
}
HICON LoadIconW(HINSTANCE executable, const wchar_t* group) {
    CHECK(executable == &imageModule && group == MAKEINTRESOURCEW(IDI_EPOCH_EDITOR));
    ++loads;
    return &largeIcon;
}
int GetSystemMetrics(int index) {
    CHECK(index == SM_CXSMICON || index == SM_CYSMICON);
    return 24;
}
void* LoadImageW(HINSTANCE executable, const wchar_t* group, unsigned type, int width, int height, unsigned flags) {
    CHECK(executable == &imageModule && group == MAKEINTRESOURCEW(IDI_EPOCH_EDITOR));
    CHECK(type == IMAGE_ICON && width == 24 && height == 24 && flags == 0);
    ++loads;
    return &smallIcon;
}
int DestroyIcon(HICON icon) {
    CHECK(icon == &smallIcon);
    ++destroySmall;
    return 1;
}
LPARAM SendMessageW(HWND window, unsigned message, unsigned kind, LPARAM icon) {
    CHECK(message == WM_SETICON && (kind == ICON_BIG || kind == ICON_SMALL));
    auto& slot = kind == ICON_BIG ? window->big : window->small;
    slot = reinterpret_cast<HICON>(icon);
    ++applications;
    return 0;
}
namespace epochengine::core::backend {
void log_native_window_icon_identity(HWND window, std::string_view phase) {
    CHECK(phase == "promoted" && window->big == &largeIcon && window->small == &smallIcon);
    ++readbacks;
}
}
ACTUAL_OWNER

int main() {
    CHECK(std::atexit([] { CHECK(destroySmall == 1); }) == 0);
    Window shell{11, 22}, backend{11, 22};
    Window nested{11, 22, true, &shell};
    Window stillEmbedded{11, 22, false, &shell};
    Window otherThread{12, 22}, otherProcess{11, 23};
    for (auto* target : {static_cast<HWND>(nullptr), &nested, &stillEmbedded, &otherThread, &otherProcess})
        apply_epoch_window_icon(target);
    CHECK(loads == 0 && applications == 0);
    apply_epoch_window_icon(&shell);
    apply_epoch_window_icon(&backend);
    CHECK(loads == 2 && applications == 4 && readbacks == 2 && destroySmall == 0);
    CHECK(shell.big == backend.big && shell.small == backend.small && shell.big != shell.small);
    CHECK(nested.big == nullptr && otherThread.big == nullptr && otherProcess.big == nullptr);
}
'''.replace('ACTUAL_OWNER', owner)
        report = ROOT / 'Artifacts/Reports/native-window-icons-20261116' / hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True, exist_ok=True)
        source = report / 'promotion_owner.cpp'
        source.write_text(code, encoding='utf-8')
        results = compile_and_run(self, source, report, includes=(ROOT / 'Engine/resource',))
        (report / 'result.json').write_text(json.dumps({
            'scope': 'actual promoted icon owner, separate large and small resource sizes, owner and root admission',
            'results': results,
        }, indent=2), encoding='utf-8')
