"""Compile the production context-menu retirement function without a renderer."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[2]
class ContextMenuOwnership(unittest.TestCase):
    def test_retirement_and_consumed_openers(self):
        engine=(ROOT/"Engine/src/epochgui/gui.engine.cpp").read_text(encoding="utf-8")
        helper="static void retire_context_menus_for_right_press() noexcept"+engine.split("static void retire_context_menus_for_right_press() noexcept",1)[1].split("struct ComposerFilePreview",1)[0]
        lifecycle=(ROOT/"Engine/src/epochgui/controls/gui.frame_lifecycle.inl").read_text(encoding="utf-8")
        self.assertLess(lifecycle.index("retire_context_menus_for_right_press();"),lifecycle.index("reset_frame();",lifecycle.index("void begin_frame")))
        text=(ROOT/"Engine/src/epochgui/controls/gui.text_controls.inl").read_text(encoding="utf-8")
        scroll=(ROOT/"Engine/src/epochgui/controls/gui.scroll_controls.inl").read_text(encoding="utf-8")
        self.assertIn("if (right_press_available() && hovered)",text)
        self.assertIn("consume_right_press();\n            menuState.open = true;",text)
        self.assertIn("options.show_context_menu && right_press_available()",text)
        self.assertIn("options.selectable && right_press_available()",scroll)
        code=r"""
#include <map>
struct State {bool open=true; bool contextMenuOpen=true; int selection=42;};
std::map<int,State> g_editBoxMenuStates{{1,{}},{2,{}}},g_scrollTextStates{{1,{}}},g_sourceEditorStates{{1,{}}},g_assetGridContextMenuStates{{1,{}}},g_consoleComposerMenus{{1,{}}};
"""+helper+r"""
int main(){
retire_context_menus_for_right_press();
for(auto& [key,s]:g_editBoxMenuStates) if(s.open||s.selection!=42)return 1;
if(g_scrollTextStates[1].contextMenuOpen||g_sourceEditorStates[1].contextMenuOpen||g_assetGridContextMenuStates[1].open||g_consoleComposerMenus[1].open)return 2;
// Next transaction opens its owner without reviving the previous owner's state.
g_scrollTextStates[1].contextMenuOpen=true;
retire_context_menus_for_right_press();
g_editBoxMenuStates[1].open=true;
if(g_scrollTextStates[1].contextMenuOpen||!g_editBoxMenuStates[1].open)return 3;
return 0;
}
"""
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="settings-viewport-", dir=parent) as temp:
            scratch=Path(temp); source=scratch/"contract.cpp"; source.write_text(code)
            if os.name=="nt":
                toolsets=sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))
                kitroot=Path("C:/Program Files (x86)/Windows Kits/10");kits=sorted((kitroot/"Include").glob("*"))
                if not toolsets or not kits:self.skipTest("MSVC/SDK unavailable")
                toolset,kit=toolsets[-1],kits[-1];binary=scratch/"contract.exe"
                args=[str(toolset/"bin/Hostx64/x64/cl.exe"),"/nologo","/std:c++latest","/EHsc","/WX",
                      "/I"+str(toolset/"include"),"/I"+str(kit/"ucrt"),"/I"+str(kit/"shared"),"/I"+str(kit/"um"),
                      "/Fo"+str(scratch/"contract.obj"),"/Fe"+str(binary),str(source),"/link",
                      "/LIBPATH:"+str(toolset/"lib/x64"),"/LIBPATH:"+str(kitroot/"Lib"/kit.name/"um/x64"),
                      "/LIBPATH:"+str(kitroot/"Lib"/kit.name/"ucrt/x64")]
            else:
                compiler=shutil.which("clang++") or shutil.which("g++")
                if not compiler:self.skipTest("C++23 compiler unavailable")
                binary=scratch/"contract";args=[compiler,"-std=c++23",str(source),"-o",str(binary)]
            built=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=90)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
            lines=code.splitlines();failure=lines[run.returncode-1] if 0<run.returncode<=len(lines) else ""
            self.assertEqual(run.returncode,0,failure)

if __name__=="__main__":unittest.main()
