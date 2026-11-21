"""Exercise the Editor's arcade asset target without compiling or linking native code."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / "Engine"


class ArcadeBuildAssets(unittest.TestCase):
    def test_editor_asset_staging(self):
        cmake = (ENGINE / "CMakeLists.txt").read_text()
        layout = cmake[cmake.index("get_filename_component(EPOCH_ARTIFACT_ROOT"):cmake.index("if(CMAKE_SOURCE_DIR STREQUAL")]
        target = cmake.index("add_custom_target(epoch_arcade_assets")
        rule = cmake[cmake.rindex("if(NOT EPOCH_BUILD_STATIC_RUNTIME)", 0, target):cmake.index("endif()", target) + len("endif()")]
        proof = ROOT / "Artifacts/Reports/contract-tests"
        proof.mkdir(parents=True, exist_ok=True)
        for generator, managed in (("Ninja", False), ("Ninja", True), ("Ninja Multi-Config", False), ("Ninja Multi-Config", True)):
            with self.subTest(generator=generator, managed=managed), tempfile.TemporaryDirectory(dir=proof) as folder:
                fixture = Path(folder)
                engine = fixture / "Engine"
                games = engine / "assets/games"
                shutil.copytree(ENGINE / "assets/games", games)
                (games / "scratch.txt").write_text("outside the image set")
                (engine / "cmake").mkdir()
                shutil.copyfile(ENGINE / "cmake/epoch_stage_arcade_assets.cmake", engine / "cmake/epoch_stage_arcade_assets.cmake")
                # Only the native linker is replaced. CMake owns the target graph and all output paths.
                (engine / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 4.4)\nproject(Probe LANGUAGES NONE)\n" + layout + """
set(CMAKE_C_LINK_EXECUTABLE "cmake -E touch <TARGET>")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/input.o" "")
add_executable(epoch "${CMAKE_CURRENT_BINARY_DIR}/input.o")
set_target_properties(epoch PROPERTIES LINKER_LANGUAGE C)
""" + rule)
                build = fixture / ("Artifacts/Intermediate/cmake/probe" if managed else "build")
                subprocess.run(["cmake", "-S", engine, "-B", build, "-G", generator], check=True, capture_output=True)
                base = fixture / "Artifacts/Runtime/cmake/probe" if managed else build
                configs = ("Debug", "Release") if generator == "Ninja Multi-Config" else ("Release",)
                for config in configs:
                    product = base / config if len(configs) > 1 else base
                    command = ["cmake", "--build", build, "--config", config, "--target", "epoch"]
                    subprocess.run(command, check=True, capture_output=True)
                    staged = product / "assets/games"
                    actual = {p.relative_to(staged): p.read_bytes() for p in staged.rglob("*") if p.is_file()}
                    expected = {p.relative_to(games): p.read_bytes() for p in games.rglob("*.ppm")}
                    self.assertEqual(actual, expected)
                    editor, = [p for p in product.glob("epoch*") if p.is_file() and p.suffix in ("", ".exe")]
                    link_time = editor.stat().st_mtime_ns
                    unchanged = staged / "apacmanlike/ghost.ppm"
                    original_time = unchanged.stat().st_mtime_ns
                    player = games / "apacmanlike/pacman.ppm"
                    # Change one texel in this temporary source, retaining a valid binary PPM.
                    edited = bytearray(player.read_bytes())
                    edited[-1] ^= 1
                    player.write_bytes(edited)
                    (staged / "apacmanlike/pellet.ppm").unlink()
                    subprocess.run(command, check=True, capture_output=True)
                    self.assertEqual((staged / "apacmanlike/pacman.ppm").read_bytes(), edited)
                    self.assertEqual((staged / "apacmanlike/pellet.ppm").read_bytes(), (games / "apacmanlike/pellet.ppm").read_bytes())
                    self.assertEqual(unchanged.stat().st_mtime_ns, original_time)
                    self.assertEqual(editor.stat().st_mtime_ns, link_time)
