"""Compile actual Settings geometry: no renderer/window/model is started."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class SettingsViewportGeometry(unittest.TestCase):
    def test_actual_viewport_fit_body_and_footer(self):
        bootstrap = (ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl").read_text(encoding="utf-8")
        settings = (ROOT / "Engine/src/editor/application/editor.frame_consent_settings.inl").read_text(encoding="utf-8")
        helpers = "const auto centered_modal_position" + bootstrap.split("const auto centered_modal_position", 1)[1].split("const auto measure_modal_text", 1)[0]
        modal = "const gui::Vec2 editorSettingsModalSize" + bootstrap.split("const gui::Vec2 editorSettingsModalSize", 1)[1].split("const core::RoutedPanelDockDragProjection", 1)[0]
        body = "const float settingsBodyHeight" + settings.split("const float settingsBodyHeight", 1)[1].split(";", 1)[0] + ";"
        footer = "const float actionY" + settings.split("const float actionY", 1)[1].split("gui::set_cursor", 1)[0]
        manager = (ROOT / "Engine/src/editor/application/editor.frame_model_manager.inl").read_text(encoding="utf-8")
        column = "const float managerWidth" + manager.split("const float managerWidth", 1)[1].split("gui::set_cursor", 1)[0]
        self.assertIn("gui::set_cursor({managerContentX, gui::cursor_position().y})", manager)
        self.assertIn("gui::set_cursor({managerBodyOrigin.x, managerFooterY})", manager)
        self.assertIn("(std::min)(1000.0f, w * 0.90f)", manager)
        self.assertEqual(bootstrap.count("fit_modal_size({ (std::min)(1000.0f, w * 0.90f), h * 0.90f }, { 500.0f, 480.0f })"), 2)
        self.assertIn(".alignment = gui::InlineButtonAlignment::Center", manager)
        code = r"""
#include <algorithm>
#include <cmath>
namespace gui { struct Vec2 {float x,y;}; }
#define CHECK(x) do { if (!(x)) return __LINE__; } while(false)
int check(float w,float h,float inset) {
""" + helpers + modal + r"""
const auto modalSize=editorSettingsModalSize;
const auto modalPos=editorSettingsModalPos;
const gui::Vec2 settingsViewport{modalPos.x+6.0f,modalPos.y+inset};
""" + body + footer + r"""
CHECK(modalSize.x>0&&modalSize.y>0);
CHECK(modalPos.x>=0&&modalPos.y>=0);
CHECK(modalPos.x+modalSize.x<=w&&modalPos.y+modalSize.y<=h);
CHECK(settingsBodyHeight>=1);
CHECK(settingsViewport.y+settingsBodyHeight<=actionY);
CHECK(actionY>=modalPos.y&&actionY+30<=modalPos.y+modalSize.y);
CHECK(resetWidth>0&&aiWidth>0&&closeWidth>0);
CHECK(actionLeft>=modalPos.x);
CHECK(actionLeft+resetWidth+actionGap+aiWidth<=actionRight-closeWidth+0.01f);
CHECK(actionRight<=modalPos.x+modalSize.x);
if(w==1920&&h==1080) {CHECK(modalSize.x==700&&modalSize.y==620);CHECK(actionScale==1);}
return 0;
}
int column_check(float width) {
const gui::Vec2 modelModalSize{width,800}, modelModalPos{100,50};
""" + column + r"""
CHECK(managerWidth>0&&managerWidth<=width);
CHECK(managerControlWidth>0&&managerControlWidth<=720&&managerControlWidth<=managerWidth);
CHECK(managerContentX==modelModalPos.x+29.0f);
CHECK(managerContentX>=modelModalPos.x&&managerContentX+managerWidth<=modelModalPos.x+width);
return 0;
}
int main() {
for(float width : {256.0f,416.0f,700.0f,1080.0f,2130.0f,3840.0f}) if(int result=column_check(width)) return result;
for(auto viewport : {gui::Vec2{1920,1080},gui::Vec2{960,540},gui::Vec2{600,360},gui::Vec2{480,270},gui::Vec2{320,220}})
  for(float inset : {36.0f,42.0f,54.0f}) if(int result=check(viewport.x,viewport.y,inset)) return result;
return 0;
}
"""
        self.assertIn("begin_modal_background_capture(editorSettingsModalPos, editorSettingsModalSize)", bootstrap)
        self.assertIn(".position = modalPos", settings)
        self.assertIn(".size = modalSize", settings)
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
