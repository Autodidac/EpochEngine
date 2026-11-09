"""Actual ProjectPlay host controls bounds/capture/toggle, neutral drawing only."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest
from Tests.renderers.test_runtime_preview_controls import shortcut_fixture,actual_function
ROOT=Path(__file__).resolve().parents[2]
OWNER=ROOT/"Engine/src/epoch/implementation/epoch.project_play_scene.inl"

class RuntimeControlsVisibility(unittest.TestCase):
    def test_actual_project_controls_visibility_debug_release(self):
        if os.name!="nt":self.skipTest("Installed MSVC CPU consumer")
        source=OWNER.read_text(encoding="utf-8-sig")
        admission=actual_function(source,"void prepare_project_gui(")
        self.assertLess(admission.index("if (!loaded)"),admission.index("m_runtimeControlsVisible = false;"))
        self.assertLess(admission.index("if (!runtime->valid())"),admission.index("m_runtimeControlsVisible = false;"))
        self.assertLess(admission.index("m_projectGuiAdapter ="),admission.index("m_runtimeControlsVisible = false;"))
        self.assertIn("bool m_runtimeControlsVisible{true};",source)
        self.assertIn("if (!m_runtimeControlsVisible)\n                    draw_runtime_controls_restore();\n                else",source)
        self.assertIn("gui::ScopedInputSuppression controlsOwnInput{\n                        runtime_panel_captures_pointer(mouse_pos)};",source)
        self.assertIn("controls.position,\n                        controls.size);",source)
        self.assertIn("draw_runtime_controls_hide(controlsContentWidth);",source)
        code=shortcut_fixture();identity=hashlib.sha256(code.encode()).hexdigest()[:12]
        lane=ROOT/"Artifacts/Reports/runtime-controls-visibility-oct28"/identity;lane.mkdir(parents=True,exist_ok=True)
        (lane/"consumer.cpp").write_text(code,encoding="utf-8")
        (lane/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.30)\nproject(RuntimeControlsVisibility LANGUAGES CXX)\nadd_executable(controls consumer.cpp)\ntarget_compile_features(controls PRIVATE cxx_std_23)\ntarget_compile_options(controls PRIVATE /W3 /WX /utf-8 /EHsc)\nenable_testing()\nadd_test(NAME actual_host_controls COMMAND controls)\n',encoding="utf-8")
        build=ROOT/"Artifacts/Intermediate/cmake/runtime-controls-visibility"/identity
        commands=[["cmake","-S",str(lane),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64",
            "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW","-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"]]
        for config in ("Debug","Release"):commands += [["cmake","--build",str(build),"--config",config,"--parallel","1"],["ctest","--test-dir",str(build),"-C",config,"--output-on-failure"]]
        runs=[]
        for index,command in enumerate(commands):
            r=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
            (lane/f"lane-{index}.log").write_text(r.stdout+r.stderr,encoding="utf-8");runs.append({"command":command,"exit":r.returncode})
            (lane/"result.json").write_text(json.dumps({"passed":r.returncode==0 and index==len(commands)-1,"owner_sha256":hashlib.sha256(OWNER.read_bytes()).hexdigest(),"commands":runs,
                "scope":"Actual sole bounds/capture/wheel and Show/Hide button callers plus existing F2/F12 behavior. Responsive bounds at1393,400,180,20,1px; no GUI/renderer/native claim. Canonical GUI admission/source callers bound by explicit assertions."},indent=2),encoding="utf-8")
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)

if __name__=="__main__":unittest.main()
