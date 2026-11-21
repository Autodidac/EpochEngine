"""Compile and run standalone atlas registration and sprite pool lifetime using the production modules."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from Tests.support.cpp import configure_arguments, executable


ROOT = Path(__file__).resolve().parents[2]


class SpritePool(unittest.TestCase):
    def test_first_registration_and_configured_capacity(self):
        engine = ROOT / "Engine"
        platform = engine / "dep/EpochPlatformEngine"
        modules = [engine / "modules" / name for name in (
            "atlas/atlas.manager.ixx", "atlas/atlas.texture.ixx", "core/core.format.ixx",
            "core/core.logger.ixx", "core/core.mpmc_bounded_queue.ixx", "image/image.loader.ixx", "sprite/sprite.handle.ixx",
            "sprite/sprite.pool.ixx", "sprite/sprite.registry.ixx", "systems/systems.registry.ixx",
            "systems/systems.task.ixx", "taskgraph/taskgraph.dotsystem.ixx", "texture/texture.core.ixx")]
        modules += [platform / "modules" / name for name in
                    ("context.type.ixx", "core.log.ixx", "core.time.ixx", "platform.engine.ixx", "platform.native.ixx")]
        sources = [Path(__file__).with_name("atlas.sprite_pool_contract.cpp"), engine / "src/epoch/epoch.systems.cpp",
                   platform / "src/core/core.log.cpp", platform / "src/core/core.time.cpp"]

        def paths(items):
            return " ".join(f'"{p.as_posix()}"' for p in items)

        parent = ROOT / "Artifacts/Tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="sp-", dir=parent) as folder:
            work = Path(folder)
            build = work / "build"
            for name, ending in (("lf", b"\n"), ("cr", b"\r"), ("crlf", b"\r\n")):
                (work / (name + ".ppm")).write_bytes(ending.join((b"P6", b"1 1", b"255", bytes((10, 32, 9)))))
            env = dict(os.environ, TMP=folder, TEMP=folder, TMPDIR=folder,
                       MSBUILDDISABLENODEREUSE="1", EPOCH_LOG_DIR=folder)
            (work / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(SpritePoolContract LANGUAGES CXX)
add_executable(sprite_pool {paths(sources)})
set_property(TARGET sprite_pool PROPERTY MSVC_DEBUG_INFORMATION_FORMAT Embedded)
target_compile_features(sprite_pool PRIVATE cxx_std_23)
if(MSVC)
  target_compile_options(sprite_pool PRIVATE /utf-8 /EHsc)
endif()
target_include_directories(sprite_pool PRIVATE {paths([engine / "include", platform / "include"])})
target_sources(sprite_pool PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{engine.as_posix()}" FILES {paths(modules)})
''', encoding="utf-8")
            commands = [configure_arguments(work, build)]
            for config in ("Debug", "Release"):
                commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                             [str(executable(build, config, "sprite_pool")), str(engine / "assets/games/apacmanlike/pacman.ppm")]]
            for command in commands:
                result = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True, timeout=180)
                self.assertEqual(result.returncode, 0, f"{command}:\n{result.stdout}{result.stderr}")
