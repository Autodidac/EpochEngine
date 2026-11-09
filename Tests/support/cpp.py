"""Compile existing CPU fixtures with the installed platform compiler.

This owns no application/model launch or source projection. Every executable is
the caller's generated contract in its checkout-local report directory.
"""
from functools import cache
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import time


@cache
def msvc_2022_root() -> Path | None:
    """Find the latest installed VS 2022 instance with the x64 C++ toolset."""
    if os.name != "nt":
        return None
    program_files = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
    vswhere = program_files / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        return None
    try:
        result = subprocess.run([
            str(vswhere), "-latest", "-products", "*", "-version", "[17.0,18.0)",
            "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property", "installationPath", "-utf8",
        ], capture_output=True, encoding="utf-8-sig", check=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return None
    installation = Path(result.stdout.strip())
    return installation if installation.is_absolute() and installation.is_dir() else None


def msvc_2022_toolsets() -> list[Path]:
    """List installed MSVC toolsets in version-directory order."""
    installation = msvc_2022_root()
    if installation is None:
        return []
    return sorted(tool for tool in (installation / "VC/Tools/MSVC").glob("*")
                  if (tool / "bin/Hostx64/x64/cl.exe").is_file())


def windows_sdk_includes(sdk_root: Path) -> list[Path]:
    """Find complete x64 Windows SDKs, ignoring partial newer installations."""
    complete = {}
    for headers in (sdk_root / "Include").glob("*"):
        version = headers.name.split(".")
        if len(version) != 4 or not all(part.isdecimal() for part in version):
            continue
        libraries = sdk_root / "Lib" / headers.name
        if all(path.is_file() for path in (headers / "ucrt/assert.h",
                headers / "shared/sdkddkver.h", headers / "um/Windows.h",
                libraries / "ucrt/x64/ucrt.lib", libraries / "um/x64/kernel32.lib")):
            complete[tuple(map(int, version))] = headers
    return [complete[version] for version in sorted(complete)]


def compile_and_run(test, source, report, *, args=(), includes=(), windows_flags=(),
                    linux_flags=(), compile_timeout=60, run_timeout=15):
    source, report = Path(source), Path(report)
    report.mkdir(parents=True, exist_ok=True)
    temporary = report / "tmp"
    temporary.mkdir(exist_ok=True)
    env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary))
    if os.name == "nt":
        tools = msvc_2022_toolsets()
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = windows_sdk_includes(sdk)
        if not tools or not kits:
            test.skipTest("Installed MSVC and Windows SDK required on Windows")
        tool, kit = tools[-1], kits[-1]
    else:
        selected = shlex.split(env.get("CXX", ""))
        compiler = selected or [shutil.which("clang++") or shutil.which("g++") or ""]
        if not compiler[0]:
            test.skipTest("Installed C++23 compiler required")
    records = []
    for configuration in ("Debug", "Release"):
        started = time.monotonic()
        output = report / (configuration + (".exe" if os.name == "nt" else ""))
        if os.name == "nt":
            flags = ["/Od", "/MDd", "/D_DEBUG"] if configuration == "Debug" else ["/O2", "/MD"]
            command = [str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/utf-8", "/EHsc", "/Z7", "/UNDEBUG",
                *flags, *windows_flags, "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"),
                *("/I" + str(path) for path in includes), "/Fo" + str(report / (configuration + ".obj")),
                "/Fe" + str(output), str(source), "/link", "/LIBPATH:" + str(tool / "lib/x64"),
                "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"),
                "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")]
        else:
            command = [*compiler, "-std=c++23", "-O0" if configuration == "Debug" else "-O2",
                "-pthread", "-UNDEBUG", *linux_flags, *("-I" + str(path) for path in includes), str(source), "-o", str(output)]
        built = subprocess.run(command, cwd=report, env=env, capture_output=True, text=True, timeout=compile_timeout)
        (report / (configuration + "-build.log")).write_text(built.stdout + built.stderr, encoding="utf-8")
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        arguments = args(configuration) if callable(args) else args
        ran = subprocess.run([str(output), *(str(item) for item in arguments)], cwd=report, env=env,
                             capture_output=True, text=True, timeout=run_timeout)
        (report / (configuration + "-run.log")).write_text(ran.stdout + ran.stderr, encoding="utf-8")
        test.assertEqual(ran.returncode, 0, f"{configuration} contract exit {ran.returncode}\n" + ran.stdout + ran.stderr)
        records.append(dict(config=configuration, configuration=configuration, exit=ran.returncode,
                            elapsed_seconds=time.monotonic()-started, command=command))
    return records


def configure_arguments(source, build):
    command = ["cmake", "-S", str(source), "-B", str(build)]
    return command + (["-G", "Visual Studio 17 2022", "-A", "x64"] if os.name == "nt"
                      else ["-G", "Ninja Multi-Config"])


def executable(build, configuration, name):
    return Path(build) / configuration / (name + (".exe" if os.name == "nt" else ""))


def dependency_include(root, header="nlohmann/json.hpp"):
    """Use an existing local dependency tree; never installs or downloads."""
    root=Path(root)
    override=os.environ.get("EPOCH_TEST_INCLUDE_ROOT")
    candidates=([Path(override)] if override else [])
    candidates += list((root/"Artifacts/Dependencies/vcpkg_installed").glob("*/include"))
    candidates += list((root/"Artifacts/Intermediate/cmake/linux").glob("*/*/vcpkg_installed/*/include"))
    for candidate in candidates:
        if (candidate/header).is_file():return candidate
    raise FileNotFoundError(f"Installed local {header} include root not found; set EPOCH_TEST_INCLUDE_ROOT")
