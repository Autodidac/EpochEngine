"""Actual bounded planetary planner, no renderer or model process."""
from pathlib import Path
import json
import os
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"


class PlanetaryViewPriority(unittest.TestCase):
    def test_view_directed_approach_keeps_complete_body_and_budget(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU proof")
        if not PACKAGE.is_dir():
            self.skipTest("Optional planetary owner is absent")
        proof = ROOT / "Artifacts/Reports/planet-view-priority-nov01"
        proof.mkdir(parents=True, exist_ok=True)
        source = PACKAGE / "upstream/EpochPlanet-0.2.3"
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(PlanetViewPriority LANGUAGES CXX)
add_executable(view_priority "{(PACKAGE/'tests/planet.view_priority_consumer.cpp').as_posix()}"
 "{(source/'src/core/world.cpp').as_posix()}" "{(source/'src/core/terrain.cpp').as_posix()}")
target_include_directories(view_priority PRIVATE "{(source/'include').as_posix()}")
target_compile_features(view_priority PRIVATE cxx_std_23)
target_compile_options(view_priority PRIVATE /sdl /utf-8 /permissive- /Zc:__cplusplus /EHsc /Z7)
''', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/cmake/planet-view-priority"
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         [str(build / config / "view_priority.exe"), str(proof / f"measurements-{config}.json")]]
        runs = []
        for i, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
            (proof / f"lane-{i}.log").write_text(completed.stdout+completed.stderr, encoding="utf-8")
            runs.append({"command": command, "exit_code": completed.returncode})
            self.assertEqual(completed.returncode, 0, completed.stdout[-5000:]+completed.stderr)
        (proof / "result.json").write_text(json.dumps({"passed": True, "runs": runs,
            "scope": "Actual planner and measured nearest generated cells; no native renderer or streaming performance claim."}, indent=2)+"\n")


if __name__ == "__main__":
    unittest.main()
