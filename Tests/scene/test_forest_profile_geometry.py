"""Actual Forest property-to-mesh/history and trunk-only codec CPU contracts."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ForestProfileGeometry(unittest.TestCase):
    def test_actual_profile_mesh_and_trunk_only_roundtrip(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module lane")
        report = ROOT / os.environ.get("EPOCH_FOREST_PROFILE_REPORT", "Artifacts/Reports/forest-profile82-nov07/green")
        report.mkdir(parents=True, exist_ok=True)
        temporary = report / "process-temp"
        temporary.mkdir(exist_ok=True)
        build = report / "cmake"
        roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        source = ROOT / "Tests/scene/forest.profile_geometry_contract.cpp"
        implementations = [ROOT / p for p in (
            "Engine/src/forest/forest.preview_mesh.cpp", "Engine/src/project/project.forest_library.cpp",
            "Engine/src/assets/texture/asset.texture_artifact.cpp", "Engine/src/assets/texture/asset.texture_import.cpp",
            "Engine/src/assets/image.stb.cpp", "Engine/src/platform/core.path.cpp",
            "Engine/dep/EpochPlatformEngine/src/core/core.log.cpp", "Engine/dep/EpochPlatformEngine/src/core/core.time.cpp")]
        found = {}
        for folder in roots:
            for path in folder.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    found[match[1]] = path
        selected = {}
        pending = ["forest.factory", "forest.preview_mesh", "project.forest_library"]
        for path in implementations:
            pending += re.findall(r"^(?:export )?(?:import|module)\s+([\w.]+);", path.read_text(encoding="utf-8-sig"), re.M)
        while pending:
            name = pending.pop()
            if name in selected or name in ("std", "std.compat"):
                continue
            self.assertIn(name, found)
            selected[name] = found[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        paths = lambda values: " ".join('"' + p.as_posix() + '"' for p in values)
        (report / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ForestProfileGeometry LANGUAGES CXX)
add_executable(forest_profile "{source.as_posix()}" {paths(implementations)})
target_compile_features(forest_profile PRIVATE cxx_std_23)
target_link_libraries(forest_profile PRIVATE shell32)
target_compile_options(forest_profile PRIVATE /W3 /WX /utf-8 /Z7)
target_include_directories(forest_profile PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_sources(forest_profile PRIVATE FILE_SET CXX_MODULES BASE_DIRS {paths(roots)} FILES {paths(selected.values())})
''', encoding="utf-8")
        inputs = sorted(set([source, Path(__file__), *selected.values(), *implementations]))
        hashes = lambda: {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
        frozen = hashes()
        (report / "source-pre.json").write_text(json.dumps(frozen, indent=2) + "\n")
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary), EPOCH_ASSET_ROOT=str(ROOT / "Engine/resource"))
        commands = [("configure", ["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"])]
        for configuration in ("Debug", "Release"):
            commands += [("build-" + configuration, ["cmake", "--build", str(build), "--config", configuration, "--parallel", "1"]),
                         ("run-" + configuration, [str(build / configuration / "forest_profile.exe"), str(ROOT / "Artifacts/Intermediate/tests/forest82" / (report.name + "-" + configuration))])]
        runs = []
        result = {"pass": False, "runtime": "CPU only; no Editor/GPU/models", "runs": runs, "frozen_sources": frozen}
        for label, command in commands:
            started = time.monotonic()
            completed = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=240)
            (report / (label + ".log")).write_text(completed.stdout + completed.stderr, encoding="utf-8")
            runs.append({"label": label, "command": command, "exit": completed.returncode, "seconds": round(time.monotonic() - started, 3)})
            result["unchanged"] = frozen == hashes()
            (report / "result.json").write_text(json.dumps(result, indent=2) + "\n")
            self.assertEqual(completed.returncode, 0, f"{label} failed; retained evidence: {report}\n" + completed.stdout[-4000:] + completed.stderr)
            self.assertTrue(result["unchanged"], "Frozen source changed during CPU lane")
        result["pass"] = True
        (report / "source-post.json").write_text(json.dumps(hashes(), indent=2) + "\n")
        (report / "result.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
