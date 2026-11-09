"""Compile the real pane-admission policy with canonical feature defaults/overrides."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/editor.application.cpp"
STATE = ROOT / "Engine/src/editor/application/editor.application_state.inl"
BOOTSTRAP = ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl"


class TileMapFeatureGate(unittest.TestCase):
    def test_actual_pane_policy_defaults_and_explicit_disabled_features(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU lane")
        owner = OWNER.read_text(encoding="utf-8")
        gmf = owner[owner.index("module;"):owner.index("module editor.core;")]
        include = re.search(r'^#include "engine\.config\.hpp"$', gmf, re.M)
        self.assertIsNotNone(include, "MSVC's Editor must load canonical defaults before gated imports/state")
        state = STATE.read_text(encoding="utf-8")
        bootstrap = BOOTSTRAP.read_text(encoding="utf-8")
        declarations = definition(state, "        enum class EditorToolPane", True)
        declarations += "\nconstexpr std::size_t kEditorToolPaneCount=static_cast<std::size_t>(EditorToolPane::Count);\n"
        declarations += "\n".join(definition(state, marker) for marker in (
            "        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>\n            editor_tool_pane_routes(",
            "        [[nodiscard]] constexpr std::optional<EditorToolPane>\n            editor_tool_pane_from_route(",
            "        [[nodiscard]] constexpr bool is_outliner_tool_pane(",
            "        [[nodiscard]] constexpr bool is_inspector_tool_pane(",
            "        [[nodiscard]] constexpr bool is_console_tool_pane("))
        policy = definition(bootstrap, "        auto pane_supported =", True)
        code = include.group() + r'''
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>
''' + declarations + r'''
void check(bool value){if(!value)std::exit(91);}
int main(){
 struct {struct {bool outliner=true,inspector=true,console=true,ai_chat=true;} panes;} application;
''' + policy + r'''
 const auto map=editor_tool_pane_from_route("pane.tile_map");
 check(map && *map==EditorToolPane::TileMap);
 check(pane_supported(*map)==bool(EXPECT_TILEMAP));
 check(pane_supported(EditorToolPane::WorldOutliner));
 check(pane_supported(EditorToolPane::GuiHierarchy));
 check(!pane_supported(EditorToolPane::AiControls));
 application.panes.outliner=false;
 check(!pane_supported(*map) && !pane_supported(EditorToolPane::WorldOutliner));
 return 0;
}
'''
        proof = ROOT / "Artifacts/Reports/tilemap-msvc-feature-gate-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/tilemap-feature-policy"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.tilemap_feature_policy.cpp"
        source.write_text(code, encoding="utf-8")
        variants = {"default": "EXPECT_TILEMAP=1", "no_authoring": "EXPECT_TILEMAP=0 EPOCH_ENABLE_AUTHORING_PLATFORM=0",
                    "no_tilemap": "EXPECT_TILEMAP=0 EPOCH_ENABLE_TILEMAP_EDITOR=0",
                    "both_disabled": "EXPECT_TILEMAP=0 EPOCH_ENABLE_AUTHORING_PLATFORM=0 EPOCH_ENABLE_TILEMAP_EDITOR=0"}
        cmake = "cmake_minimum_required(VERSION 3.30)\nproject(TileMapFeaturePolicy LANGUAGES CXX)\nenable_testing()\n"
        for name, definitions in variants.items():
            cmake += f'''add_executable(tilemap_feature_{name} "{source.as_posix()}")
target_compile_features(tilemap_feature_{name} PRIVATE cxx_std_23)
target_include_directories(tilemap_feature_{name} PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_compile_definitions(tilemap_feature_{name} PRIVATE {definitions})
add_test(NAME actual_tilemap_feature_{name} COMMAND tilemap_feature_{name})
'''
        (proof / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=80)
            log = proof / f"policy-{index}.log"
            log.write_text(completed.stdout+completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-5000:]+completed.stderr)
        (proof / "policy-result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": results,
            "variants": variants, "sourceIdentity": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                                      for p in (OWNER, STATE, BOOTSTRAP)},
            "scope": "Actual canonical header and Editor pane route/admission function bodies; application visibility booleans supplied by a minimal port. Independent full Editor translation-unit compilation is recorded separately.",
        }, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
