"""Actual optional controller/physics/session consumers; no graphics or models."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"


class PlanetaryRuntimeScope(unittest.TestCase):
    def test_actual_controller_scope_and_legacy_universe(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC standalone module consumer")
        owners = [PACKAGE / relative for relative in (
            "src/planet.exploration_runtime.cpp", "tests/planet.runtime_controller_consumer.cpp",
            "modules/planet.clouds.ixx", "modules/planet.terrain.ixx", "CMakeLists.txt",
            "tests/planet.clouds_consumer.cpp",
            "upstream/EpochPlanet-0.2.3/include/epoch/planet/simulation.hpp",
            "upstream/EpochPlanet-0.2.3/src/core/simulation.cpp")]
        owners.append(ROOT / "Engine/modules/project.planetary_runtime.ixx")
        frozen = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in owners}
        identity = hashlib.sha256(json.dumps(frozen, sort_keys=True).encode()+Path(__file__).read_bytes()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/planet-runtime-scope-nov01" / identity
        build = ROOT / "Artifacts/Intermediate/cmake/planet-runtime-scope" / identity
        report.mkdir(parents=True, exist_ok=True)
        commands = [["cmake", "-S", str(PACKAGE), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64",
                     "-DBUILD_TESTING=ON", "-DEPOCH_PLANET_BUILD_RUNTIME_CONTROLLER=ON",
                     "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW", "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded",
                     "-DEPOCH_PLANET_BUILD_MESH_EXPORT=OFF", "-DEPOCH_PLANET_BUILD_WATER=OFF",
                     f"-DEPOCH_ENGINE_SOURCE_ROOT={ROOT}"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1", "--target",
                          "epoch_planet_runtime_controller_contract", "epoch_planet_clouds_contract"],
                         ["ctest", "--test-dir", str(build), "-C", config, "-R",
                          "^epoch_planet_(runtime_controller|clouds)_contract$", "--output-on-failure"]]
        runs = []
        for index, command in enumerate(commands):
            started = time.monotonic()
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
            log = report / f"lane-{index}.log"
            log.write_text(run.stdout + run.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": run.returncode, "seconds": time.monotonic() - started,
                         "log": str(log)})
            unchanged = all(hashlib.sha256(path.read_bytes()).hexdigest() == frozen[str(path.relative_to(ROOT))]
                            for path in owners)
            (report / "result.json").write_text(json.dumps({
                "passed": unchanged and run.returncode == 0 and index == len(commands) - 1,
                "scope": "Actual optional controller, cloud module, core simulation and host module: selected Planet inventory/physics/controls, bounded deterministic cloud geometry/winding/time, nonzero body projection, scoped session refusal, unchanged legacy Universe behavior. CPU only; native integration remains separate.",
                "sources": frozen, "sources_unchanged": unchanged, "commands": runs}, indent=2) + "\n", encoding="utf-8")
            self.assertTrue(unchanged, "Owner bytes changed during proof")
            self.assertEqual(run.returncode, 0, run.stdout[-6000:] + run.stderr[-2000:])


if __name__ == "__main__":
    unittest.main()
