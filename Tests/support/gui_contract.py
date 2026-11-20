"""Shared CPU-only compiler lane for real GUI document/runtime consumers."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
from Tests.support.cpp import engine_module_source
ROOT = Path(__file__).resolve().parents[2]

def run_gui_contract(test, name, code, extra_owners, scope, *, extra_sources=(), extra_modules=(), libraries=(), long_paths=False):
    sources = [ROOT / "Engine/src" / name for name in (
        "authoring/gui/authoring.gui_templates.cpp",
        "authoring/gui/authoring.gui_document_codec.cpp",
        "authoring/gui/authoring.gui_compiler.cpp",
        "assets/gui/asset.gui_artifact.cpp",
        "project/project.gui_runtime.cpp")]
    modules = [engine_module_source(ROOT / "Engine/modules", name) for name in (
        "core.sha256.ixx", "authoring.document.ixx", "authoring.gui_document.ixx",
        "authoring.gui_compiler.ixx", "asset.gui_artifact.ixx", "project.gui_runtime.ixx")]
    sources += [ROOT / path for path in extra_sources]
    modules += [engine_module_source(ROOT / "Engine/modules", name) for name in extra_modules]
    owners = sources + modules + extra_owners + [Path(__file__)]
    manifest = ROOT / "Engine/resource/epoch.manifest"
    if long_paths:
        owners.append(manifest)
    digest = hashlib.sha256((name + code).encode())
    digest.update(Path(__file__).read_bytes())
    for owner in owners:
        digest.update(owner.read_bytes())
    identity = digest.hexdigest()[:12]
    report = ROOT / "Artifacts/Reports" / name / identity
    build = ROOT / "Artifacts/Intermediate/cmake" / name / identity
    report.mkdir(parents=True, exist_ok=True)
    temporary = report / "tmp"
    temporary.mkdir(exist_ok=True)
    (report / "consumer.cpp").write_text(code, encoding="utf-8")
    (report / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.30)\nproject(GuiContract LANGUAGES CXX)\n'
        'add_executable(gui_contract consumer.cpp ' + " ".join('"' + p.as_posix() + '"' for p in sources) + ')\n'
        'target_compile_features(gui_contract PRIVATE cxx_std_23)\n'
        'set_property(TARGET gui_contract PROPERTY MSVC_DEBUG_INFORMATION_FORMAT Embedded)\n'
        'target_compile_options(gui_contract PRIVATE /W3 /WX /utf-8 /EHsc)\n'
        'target_sources(gui_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS "' + ROOT.as_posix() + '" FILES '
        + " ".join('"' + p.as_posix() + '"' for p in modules) + ')\n'
        + ('target_link_libraries(gui_contract PRIVATE ' + ' '.join(libraries) + ')\n' if libraries else '')
        + (f'target_link_options(gui_contract PRIVATE "/MANIFESTINPUT:{manifest.as_posix()}")\n' if long_paths else ''),
        encoding="utf-8")
    commands = [["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
    for configuration in ("Debug", "Release"):
        commands += [["cmake", "--build", str(build), "--config", configuration, "--parallel", "1"],
                     [str(build / configuration / "gui_contract.exe")]]
    runs = []
    env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary), MSBUILDDISABLENODEREUSE="1")
    for index, command in enumerate(commands):
        result = subprocess.run(command, cwd=report, env=env, capture_output=True, text=True, timeout=150)
        log = report / f"lane-{index}.log"
        log.write_text(result.stdout + result.stderr, encoding="utf-8")
        runs.append({"command": command, "exit": result.returncode, "log": str(log)})
        (report / "result.json").write_text(json.dumps({
            "passed": result.returncode == 0 and index == len(commands) - 1,
            "scope": scope,
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in owners},
            "runs": runs}, indent=2) + "\n", encoding="utf-8")
        test.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-9000:])
