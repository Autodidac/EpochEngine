"""Actual shared memory/file image decoder/compiler modules; no GPU or model."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import json
import os
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


class TextureMemoryImport(unittest.TestCase):
    def test_memory_and_file_share_identity_and_bounds(self):
        if os.name != "nt": self.skipTest("Installed MSVC CPU module consumer")
        lane = ROOT/"Artifacts/Reports/texture-memory-import-oct28"/str(time.time_ns())
        lane.mkdir(parents=True)
        temp = lane/"tmp"; temp.mkdir()
        env = dict(os.environ, TEMP=str(temp), TMP=str(temp), TMPDIR=str(temp))
        module_paths = [engine_module_source(ROOT / "Engine/modules", name + ".ixx") for name in ("asset.texture_artifact", "asset.texture_import")]
        source_paths = [ROOT/path for path in ("Tests/scene/asset.texture_memory_contract.cpp", "Engine/src/assets/texture/asset.texture_artifact.cpp", "Engine/src/assets/texture/asset.texture_import.cpp", "Engine/src/assets/image.stb.cpp")]
        cmake = f'''cmake_minimum_required(VERSION 3.30)
project(TextureMemoryImport LANGUAGES CXX)
add_executable(texture_memory {" ".join('"'+p.as_posix()+'"' for p in source_paths)})
target_compile_features(texture_memory PRIVATE cxx_std_23)
target_compile_options(texture_memory PRIVATE /W3 /WX /utf-8 /EHsc)
target_sources(texture_memory PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in module_paths)})
enable_testing()
add_test(NAME actual_memory_file_import COMMAND texture_memory "{(lane/'data').as_posix()}")
'''
        (lane/"CMakeLists.txt").write_text(cmake, encoding="utf-8")
        build=ROOT/"Artifacts/Intermediate/cmake/texture-memory-import"/lane.name
        commands=[["cmake","-S",str(lane),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
        for config in ("Debug","Release"):
            commands += [["cmake","--build",str(build),"--config",config,"--parallel","1"],["ctest","--test-dir",str(build),"-C",config,"--output-on-failure"]]
        runs=[]
        for index,command in enumerate(commands):
            start=time.monotonic()
            result=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,timeout=120)
            (lane/f"lane-{index}.log").write_text(result.stdout+result.stderr,encoding="utf-8")
            runs.append({"command":command,"exit":result.returncode,"seconds":time.monotonic()-start})
            owners=module_paths+source_paths+[Path(__file__)]
            (lane/"result.json").write_text(json.dumps({"passed":result.returncode==0 and index==len(commands)-1,"commands":runs,"owners":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in owners},"scope":"Actual memory/file common PNG+PPM decoder/compiler, exact serialized artifact equality, stable byte snapshot under path replacement, alpha/colorspace/sequence/budgets/truncation/corruption plus existing file contract. No model/window/GPU."},indent=2),encoding="utf-8")
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=="__main__": unittest.main()
