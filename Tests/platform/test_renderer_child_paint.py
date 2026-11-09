"""Execute actual native paint branches against a renderer-held DC on CPU only."""
from pathlib import Path
import json
import unittest

from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class RendererChildPaint(unittest.TestCase):
    def test_actual_child_paint_keeps_render_dc_exclusive(self):
        dispatch = (ROOT / "Engine/src/renderers/host/implementation/host.native_dispatch.inl").read_text(encoding="utf-8")
        parent, child = dispatch.split("LRESULT CALLBACK MultiContextManager::ChildProc(", 1)
        child_cases = "case WM_ERASEBKGND:" + child.split("case WM_ERASEBKGND:", 1)[1].split("case WM_CLOSE:", 1)[0]
        parent_case = "case WM_PAINT:" + parent.split("case WM_PAINT:", 1)[1].split("case WM_DROPFILES:", 1)[0]
        opengl = (ROOT / "Engine/src/renderers/opengl/opengl.context_init.cpp").read_text(encoding="utf-8")
        fallback = opengl.split("inline LRESULT CALLBACK gl_child_proc(", 1)[1].split("inline void ensure_gl_child_class_registered()", 1)[0]
        fallback_cases = "case WM_ERASEBKGND:" + fallback.split("case WM_ERASEBKGND:", 1)[1].split("default:", 1)[0]
        code = r'''
#include <cstdint>
using HWND = void*;
using HDC = void*;
using UINT = unsigned;
using WPARAM = std::uintptr_t;
struct RECT { int left{},top{},right{},bottom{}; };
struct PAINTSTRUCT { RECT rcPaint{}; };
constexpr UINT WM_PAINT=15, WM_ERASEBKGND=20;
unsigned begins{},ends{},fills{},validations{},lookups{},violations{},overlays{};
bool rendererOwnsDc{},updateRegion{};
HWND expectedWindow{};
struct WindowData { bool firstPresent{}; } window;
WindowData* admitted{};
WindowData* resolveWindowData(){++lookups;return admitted;}
bool should_draw_opengl_startup_placeholder(WindowData* value){return value&&!value->firstPresent;}
HDC BeginPaint(HWND hwnd, PAINTSTRUCT*) {
    ++begins; if(rendererOwnsDc||hwnd!=expectedWindow)++violations;
    updateRegion=false;return reinterpret_cast<HDC>(1);
}
void EndPaint(HWND, const PAINTSTRUCT*) {++ends;}
int ValidateRect(HWND hwnd, const RECT* rectangle) {
    ++validations;if(hwnd!=expectedWindow||rectangle)++violations;
    updateRegion=false;return 1;
}
int GetClientRect(HWND,RECT*){return 1;}
void* parent_background_brush(){return nullptr;}
int FillRect(HDC,const RECT*,void*){++fills;if(rendererOwnsDc)++violations;return 1;}
void paint_parent_context_dock_overlay(HWND,HDC){++overlays;}
'''
        for name, cases in (("child", child_cases), ("fallback", fallback_cases), ("parent", parent_case)):
            code += f"int {name}(HWND hwnd, UINT message, WPARAM wParam) {{ switch(message) {{\n{cases}\ndefault:return -1;}} }}\n"
        code += r'''
#define CHECK(value) do {if(!(value))return __LINE__;} while(false)
int main(){
    expectedWindow=reinterpret_cast<HWND>(2);
    rendererOwnsDc=true;
    // The renderer can own the DC before first present, during normal frames,
    // and after WindowData leaves the active registry during deferred cleanup.
    for(int state=0;state<3;++state){
        admitted=state==2?nullptr:&window;window.firstPresent=state==1;
        for(auto handler:{child,fallback}){
            updateRegion=true;const auto before=validations;
            CHECK(handler(expectedWindow,WM_ERASEBKGND,1)==1);
            CHECK(updateRegion&&validations==before);
            CHECK(handler(expectedWindow,WM_PAINT,1)==0);
            CHECK(!updateRegion&&validations==before+1);
            CHECK(begins==0&&ends==0&&fills==0&&lookups==0&&violations==0);
        }
    }
    // Parent chrome/dock background still owns and executes its GDI painting.
    rendererOwnsDc=false;updateRegion=true;
    CHECK(parent(expectedWindow,WM_PAINT,1)==0);
    CHECK(!updateRegion&&begins==1&&ends==1&&fills==1&&overlays==1&&violations==0);
    return 0;
}
'''
        # Braced handler iteration needs the standard initializer_list declaration.
        code = "#include <initializer_list>\n" + code
        report = ROOT / "Artifacts/Reports/renderer-child-dc-ownership-oct30/cpu"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "renderer_child_paint_contract.cpp"
        source.write_text(code, encoding="utf-8")
        results = compile_and_run(self, source, report, windows_flags=("/WX",))
        (report / "result.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
