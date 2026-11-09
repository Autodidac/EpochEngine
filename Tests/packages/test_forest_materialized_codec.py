"""Build/run the actual Forest library CPU contract with installed MSVC only.

Run from the repository root (or use an absolute script path elsewhere):
    python Tests/packages/test_forest_materialized_codec.py

The shared Tools/run_tests.py discovers this test too. No renderer, Engine
executable, model, dependency installation, copied implementation or v2 migration
is involved. Successful scratch builds are removed; failed evidence is retained.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULES = (
    "authoring.document", "authoring.morphology", "voxel.field", "forest.factory",
    "project.forest_library", "project.asset_registry", "package.registry",
    "platform.filesystem", "core.sha256",
)


class ForestMaterializedCodec(unittest.TestCase):
    def test_actual_library_v1_golden_and_v2_verified_materialization(self):
        if os.name != "nt":
            self.skipTest("Windows x64 MSVC golden/codec lane; other toolchains remain separate proof")
        cmake = shutil.which("cmake")
        if not cmake:
            self.skipTest("Installed CMake required; this test never downloads tools")
        version = subprocess.run([cmake, "--version"], capture_output=True, text=True, timeout=10)
        match = re.search(r"cmake version (\d+)\.(\d+)", version.stdout)
        if not match or tuple(map(int, match.groups())) < (3, 28):
            self.skipTest("Installed CMake3.28+ required for C++23 module file sets")
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.is_file():
            self.skipTest("Installed VS2022 C++ toolchain required; never bootstrap")
        found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-version", "[17.0,18.0)",
                                "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                "-property", "installationPath"], capture_output=True, text=True, timeout=10)
        if found.returncode or not found.stdout.strip():
            self.skipTest("Installed VS2022 C++ toolchain required; never bootstrap")

        fixtures = ROOT / "Tests/packages/fixtures/forest_library_v1"
        golden = json.loads((fixtures / "receipt.json").read_text(encoding="utf-8"))
        for name, expected in golden["files"].items():
            data = (fixtures / name).read_bytes()
            self.assertEqual(len(data), expected["bytes"], name)
            self.assertEqual(hashlib.sha256(data).hexdigest(), expected["sha256"], name)

        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(parent.resolve().is_relative_to(ROOT.resolve()), "scratch parent is redirected outside checkout")
        scratch = Path(tempfile.mkdtemp(prefix="forest-codec-", dir=parent))
        source = scratch / "source"
        source.mkdir()
        build = scratch / "msvc"
        success = False
        configurations = {}
        invocations = {}
        environment = os.environ.copy()
        temporary = scratch / "process-temp"
        temporary.mkdir()
        for name in ("TEMP", "TMP", "TMPDIR"):
            environment[name] = str(temporary)
        try:
            engine = ROOT / "Engine"
            module_paths = "\n".join(f'    "{(engine / "modules" / (name + ".ixx")).as_posix()}"' for name in MODULES)
            # Refer directly to actual owners. Do not mirror module definitions
            # or generate substitutes for the compiler/decoder under test.
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(EpochForestMaterializedContract LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
add_library(forest_codec_owners STATIC "{(engine / 'src/project/project.forest_library.cpp').as_posix()}")
target_sources(forest_codec_owners PUBLIC FILE_SET forest_modules TYPE CXX_MODULES
    BASE_DIRS "{(engine / 'modules').as_posix()}" FILES
{module_paths})
target_include_directories(forest_codec_owners PRIVATE
    "{(engine / 'include').as_posix()}" "{(engine / 'dep/EpochPlatformEngine/include').as_posix()}")
target_compile_options(forest_codec_owners PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8)
add_executable(forest_materialized_contract
    "{(ROOT / 'Tests/packages/project.forest_materialized_contract.cpp').as_posix()}"
    "{(engine / 'resource/epoch.manifest').as_posix()}")
target_link_libraries(forest_materialized_contract PRIVATE forest_codec_owners)
''', encoding="utf-8")

            def checked(name, command, timeout=120):
                started = time.monotonic()
                completed = subprocess.run(command, cwd=ROOT, capture_output=True,
                                           text=True, timeout=timeout, env=environment)
                invocations[name] = {"command": command, "exit": completed.returncode,
                                     "wall_seconds": time.monotonic() - started}
                transcript = completed.stdout + completed.stderr
                (scratch / (name + ".log")).write_text(transcript, encoding="utf-8")
                self.assertEqual(completed.returncode, 0,
                                 f"{name} failed. Evidence: {scratch}\n{transcript[-7000:]}")
                return transcript

            checked("configure", [cmake, "-S", str(source), "-B", str(build), "-G", "Visual Studio 17 2022",
                                  "-A", "x64", "-DCMAKE_TOOLCHAIN_FILE=",
                                  "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"])
            for config in ("Debug", "Release"):
                with self.subTest(configuration=config):
                    checked("build-" + config, [cmake, "--build", str(build), "--config", config,
                                                "--target", "forest_materialized_contract", "--parallel", "1"])
                    case = scratch / ("project-" + config.lower())
                    checked("contract-" + config, [str(build / config / "forest_materialized_contract.exe"),
                                                   str(fixtures), str(case)], timeout=30)
                    result = (case / "result.txt").read_text(encoding="utf-8")
                    self.assertTrue(result.startswith("PASS:"), result)
                    configurations[config] = {"exit": 0, "result": result.strip()}
                    selected_case = scratch / ("selected-" + config.lower())
                    consumer = str(build / config / "forest_materialized_contract.exe")
                    checked("selected-producer-" + config,
                            [consumer, str(fixtures), str(selected_case), "--selected-producer"], timeout=30)
                    checked("selected-reopen-" + config,
                            [consumer, str(fixtures), str(selected_case), "--selected-reopen"], timeout=30)
                    configurations[config]["separate_process_selected_reference"] = {
                        "producer": (selected_case / "selected-producer.txt").read_text(encoding="utf-8").strip(),
                        "cold_reopen": (selected_case / "selected-reopen.txt").read_text(encoding="utf-8").strip(),
                        "atomic_failure": (selected_case / "selected-atomic-failure-status.txt").read_text(encoding="utf-8").strip(),
                        "source_replay_required": True,
                    }
            self.assertTrue(all(
                config in configurations and "separate_process_selected_reference" in configurations[config]
                for config in ("Debug", "Release")), "Incomplete configuration: preserve failed scratch evidence")
            self.assertTrue(all(value["exit"] == 0 for value in invocations.values()),
                            "A subtest failed: preserve failed scratch evidence")
            record = {
                "schema": "epoch.forest-materialized-contract.v1",
                "configurations": configurations,
                "separate_subprocess_invocations": invocations,
                "actual_modules": list(MODULES),
                "actual_implementation": "Engine/src/project/project.forest_library.cpp",
                "fixture": "Tests/packages/fixtures/forest_library_v1/receipt.json",
                "renderer_or_model_started": False,
                "source_sha256": {
                    str(path.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in [engine / "src/project/project.forest_library.cpp",
                                 ROOT / "Tests/packages/project.forest_materialized_contract.cpp",
                                 ROOT / "Tests/packages/test_forest_materialized_codec.py",
                                 *[engine / "modules" / (name + ".ixx") for name in MODULES]]
                },
            }
            (parent / "forest-materialized-last.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
            success = True
        finally:
            if success:
                resolved = scratch.resolve()
                self.assertEqual(resolved.parent, parent.resolve(), "refuse cleanup of redirected scratch")
                self.assertTrue(resolved.is_relative_to(ROOT.resolve()), "refuse cleanup outside checkout")
                shutil.rmtree(resolved)
            else:
                print("Forest materialized contract evidence retained:", scratch)


if __name__ == "__main__":
    unittest.main()
