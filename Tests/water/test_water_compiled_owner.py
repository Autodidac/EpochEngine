"""Compile the real water module implementation and an independent CPU consumer."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULES = ("render.math", "voxel.field", "voxel.storage", "water.wave", "water.system")
OWNER = ROOT / "Engine/src/water/water.system.cpp"
CONSUMER = ROOT / "Engine/src/water/water.system_contract.cpp"


class WaterCompiledOwner(unittest.TestCase):
    def test_independent_module_consumer_debug_release(self):
        self._consumer(False)

    def test_analytic_surface_debug_release(self):
        self._consumer(True)

    def _consumer(self, surface):
        if os.name != "nt":
            self.skipTest("This installed MSVC lane proves Windows only")
        cmake = shutil.which("cmake")
        if not cmake:
            self.skipTest("Installed CMake3.28+ required; no bootstrap")
        report = ROOT / "Artifacts/Reports" / ("water-surface-oct29" if surface else "water-compiled-owner-oct29") / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "source"
        source.mkdir()
        temporary = report / "tmp"
        temporary.mkdir()
        environment = os.environ.copy()
        for key in ("TEMP", "TMP", "TMPDIR"):
            environment[key] = str(temporary)
        engine = ROOT / "Engine"
        modules = (*MODULES, "water.surface", "asset.texture_artifact", "asset.mesh_artifact") if surface else MODULES
        implementations = [OWNER]
        if surface:
            implementations.extend([engine / "src/water/water.surface.cpp",
                                    engine / "src/assets/texture/asset.texture_artifact.cpp"])
        consumer = engine / "src/water/water.surface_contract.cpp" if surface else CONSUMER
        implementation_paths = "\n".join(f'    "{path.as_posix()}"' for path in implementations)
        module_paths = "\n".join(f'    "{(engine / "modules" / (name + ".ixx")).as_posix()}"' for name in modules)
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0141 NEW)
project(EpochWaterCompiledOwner LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
add_library(water_owners STATIC
{implementation_paths})
target_sources(water_owners PUBLIC FILE_SET water_modules TYPE CXX_MODULES
    BASE_DIRS "{(engine / 'modules').as_posix()}" FILES
{module_paths})
target_compile_options(water_owners PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8 /W4 /WX)
add_executable(water_consumer "{consumer.as_posix()}")
target_link_libraries(water_consumer PRIVATE water_owners)
''', encoding="utf-8")
        paths = [*implementations, consumer, Path(__file__), *[engine / "modules" / (name + ".ixx") for name in modules]]
        hashes = lambda: {str(path.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
        before = hashes()
        invocations = []
        verdict = {"schema": "epoch.water-compiled-owner.v1", "passed": False,
                   "source_sha256": before, "invocations": invocations,
                   "actual_modules": modules, "runtime_or_model_started": False}

        def checked(name, command, timeout=120):
            started = time.monotonic()
            result = subprocess.run(command, cwd=ROOT, env=environment,
                                    capture_output=True, text=True, timeout=timeout)
            transcript = result.stdout + result.stderr
            (report / (name + ".log")).write_text(transcript, encoding="utf-8")
            invocations.append({"phase": name, "command": command, "exit": result.returncode,
                                "wall_seconds": time.monotonic() - started})
            self.assertEqual(result.returncode, 0, f"{name}: {report}\n{transcript[-6000:]}")

        try:
            build = report / "build"
            checked("configure", [cmake, "-S", str(source), "-B", str(build),
                                  "-G", "Visual Studio 17 2022", "-A", "x64", "-T", "host=x64",
                                  "-DCMAKE_TOOLCHAIN_FILE=",
                                  "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"])
            for configuration in ("Debug", "Release"):
                checked("build-" + configuration, [cmake, "--build", str(build), "--config", configuration,
                                                  "--target", "water_consumer", "--parallel", "1"])
                output = report / (configuration + "-replay.txt")
                checked("contract-" + configuration,
                        [str(build / configuration / "water_consumer.exe"), str(output)], timeout=20)
                expected = ("PASS: bounded analytic geometry, query normals, upward winding, temporal artifact replay"
                            if surface else "PASS: handles, validation, temporal replay, bounded voxel stamping")
                self.assertIn(expected, output.read_text(encoding="utf-8"))
            self.assertEqual(hashes(), before, "Owner changed while validating; keep this attempt unaccepted")
            self.assertEqual((report / "Debug-replay.txt").read_bytes(),
                             (report / "Release-replay.txt").read_bytes(), "Explicit-time output differs by configuration")
            verdict["replay_sha256"] = hashlib.sha256((report / "Release-replay.txt").read_bytes()).hexdigest()
            verdict["passed"] = True
        finally:
            (report / "result.json").write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
