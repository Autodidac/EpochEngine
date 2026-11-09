"""Run the real emitted Windows build script with a recording MSBuild substitute.

The source/edition guard, installed-cache preflight, argument assembly and script
exit handling run unchanged. No compiler, installer, network or renderer runs
through the substitute. MSBuild property queries only evaluate the real project.
The fixture uses its own lock-file destination so it cannot wait on or block an
operator's real project build; the production exclusive-lock code is unchanged.
"""
from pathlib import Path
import json
import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest
from xml.sax.saxutils import quoteattr
from Tests.support.cpp import msvc_2022_root, msvc_2022_toolsets, windows_sdk_includes

from Tests.platform.test_child_close_router import definition
from Tools.source.owned_source_text import read_owned_source

ROOT = Path(__file__).resolve().parents[2]
SHELL = ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl"


@unittest.skipUnless(os.name == "nt", "Windows generated build script")
class GeneratedProjectDependencies(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        installation = msvc_2022_root()
        tools = msvc_2022_toolsets()
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = windows_sdk_includes(sdk)
        if installation is None or not tools or not kits:
            raise unittest.SkipTest("Installed MSVC CPU consumer unavailable")
        cls.msbuild = installation / "MSBuild/Current/Bin/amd64/MSBuild.exe"
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        cls.scratch = Path(tempfile.mkdtemp(prefix="project-dependencies-", dir=parent))
        source = SHELL.read_text(encoding="utf-8")
        owned = read_owned_source(SHELL)
        fragments = "\n".join(definition(source, marker) for marker in (
            "    static void replace_all(",
            "    [[nodiscard]] static std::string generated_child_project_library_properties(",
            "    [[nodiscard]] static constexpr std::string_view generated_windows_dependency_properties(",
            "    [[nodiscard]] static std::string generated_windows_dependency_script(",
            "    [[nodiscard]] static constexpr std::string_view generated_windows_msbuild_discovery(",
            "    [[nodiscard]] static constexpr std::string_view generated_windows_msbuild_candidates("))
        fragments += definition((ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8"),
            "    [[nodiscard]] static std::string to_windows_path(")
        fragments += definition(source, "    inline constexpr std::array<std::string_view, 7> generated_child_runtime_assets", True)
        fragments += definition(source, "    [[nodiscard]] static std::string generated_child_runtime_assets_msbuild(")
        fragments += definition(source, "    [[nodiscard]] static std::string generated_child_runtime_assets_cmake(")
        start = source.index("    [[nodiscard]] static bool repair_generated_windows_dependency_script(")
        stop = source.index("    [[nodiscard]] static bool repair_generated_windows_child_project_build_files(", start)
        fragments += source[start:stop]
        fragments += definition(owned, "    [[nodiscard]] static std::string generated_child_project_link_dependencies(")
        project_expression = source.split("        const std::string windowsProjectText =", 1)[1].split(
            "        const std::string windowsBuildScriptText =", 1)[0]
        script_expression = source.split("        const std::string windowsBuildScriptText =", 1)[1].split(
            "        const std::string linuxBuildScriptText =", 1)[0]
        body = r'''
#include <cassert>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include "epoch.build_layout.hpp"
#undef assert
#define assert(...) do { if(!(__VA_ARGS__)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#__VA_ARGS__); std::exit(91); } } while(false)
namespace fs=std::filesystem;
std::string xml_escape(std::string s){for(auto pos=s.find('&');pos!=std::string::npos;pos=s.find('&',pos+5))s.replace(pos,1,"&amp;");return s;}
std::string powershell_escape_single_quoted(std::string s){for(auto pos=s.find('\'');pos!=std::string::npos;pos=s.find('\'',pos+2))s.insert(pos,1,'\'');return s;}
''' + fragments + r'''
int main(int argc,char**argv){
assert(argc==3);const fs::path repoRoot=fs::path{argv[1]},output=fs::path{argv[2]};
fs::create_directories(output);
const fs::path windowsProject=output/"DependencyFixture.vcxproj";
const auto repoRootWin=xml_escape(repoRoot.string());
const auto repoRootPowerShell=powershell_escape_single_quoted(repoRoot.string());
const auto vcpkgManifestRootPowerShell=powershell_escape_single_quoted((repoRoot/"Engine").string());
const auto repoEngineProjectWin=xml_escape((repoRoot/"Engine/examples/EpochEngine/EpochEngine.vcxproj").string());
const auto repoEpochGuiProjectWin=xml_escape((repoRoot/"Engine/dep/EpochGui/EpochGui.vcxproj").string());
const auto engineLibraryRelative=epochengine::build_layout::windows_relative_bin(repoRoot,"$(Configuration)").string()+"\\";
const std::string projectGuid="CCD20673-713B-4657-9844-10381F336490",sourceEdition=EPOCH_EDITION_PUBLIC?"epoch.public.github.v1":"epoch.development.site.v1",artifactStem="DependencyFixture";
struct{std::string project_name="DependencyFixture";}spec;
const std::string windowsProjectText=''' + project_expression + r'''
const std::string windowsBuildScriptText=''' + script_expression + r'''
std::ofstream(windowsProject,std::ios::binary)<<windowsProjectText;
std::ofstream(output/"build_project.ps1",std::ios::binary)<<windowsBuildScriptText;
std::ofstream(output/"runtime-assets.cmake",std::ios::binary)<<generated_child_runtime_assets_cmake();
std::ofstream(output/"runtime-assets.xml",std::ios::binary)<<generated_child_runtime_assets_msbuild();
auto current=windowsBuildScriptText;assert(repair_generated_windows_dependency_script(current));assert(current==windowsBuildScriptText);
auto previousTracking=windowsBuildScriptText;
replace_all(previousTracking,"/p:UseMultiToolTask=false","/p:UseMultiToolTask=true");
assert(previousTracking!=windowsBuildScriptText);
assert(repair_generated_windows_dependency_script(previousTracking));
assert(previousTracking==windowsBuildScriptText);
auto fixedHost=windowsBuildScriptText;
replace_all(fixedHost,generated_windows_msbuild_discovery(),"");
assert(fixedHost!=windowsBuildScriptText);
auto previousHost=fixedHost;
replace_all(previousHost,"        'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\amd64\\MSBuild.exe',\n","");
replace_all(previousHost,"        'C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Current\\Bin\\amd64\\MSBuild.exe',\n","");
replace_all(previousHost,"'/p:PreferredToolArchitecture=x64', ","");
assert(previousHost!=windowsBuildScriptText);
assert(repair_generated_windows_dependency_script(fixedHost));
assert(fixedHost==windowsBuildScriptText);
assert(repair_generated_windows_dependency_script(previousHost));
assert(previousHost==windowsBuildScriptText);
auto invalid=std::string{"custom script"};assert(!repair_generated_windows_dependency_script(invalid));assert(invalid=="custom script");
// Existing generated files have this exact legacy block. Migrate it with the
// production repair and prove a repeated repair does not churn script bytes.
const auto begin=current.find("    # EPOCH_INSTALLED_DEPENDENCIES_BEGIN\n"),end=current.find("    # EPOCH_INSTALLED_DEPENDENCIES_END\n");
assert(begin!=std::string::npos&&end!=std::string::npos);
current.replace(begin,end+std::string_view{"    # EPOCH_INSTALLED_DEPENDENCIES_END\n"}.size()-begin,R"OLD(    if (-not [string]::IsNullOrWhiteSpace($env:EPOCH_CANDIDATE_DEPENDENCY_ROOT)) {
        $dependencyRoot = $env:EPOCH_CANDIDATE_DEPENDENCY_ROOT.TrimEnd('\', '/') + '\'
        $tripletRoot = Join-Path $dependencyRoot 'x64-windows'
        if (-not (Test-Path -LiteralPath (Join-Path $tripletRoot 'include'))) { $tripletRoot = Join-Path $tripletRoot 'x64-windows' }
        if (-not (Test-Path -LiteralPath (Join-Path $tripletRoot 'include'))) { throw 'The admitted host dependency triplet is unavailable.' }
        $msbuildArgs += @('/p:VcpkgManifestInstall=false', '/p:VcpkgAutoBootstrap=false', ('/p:VcpkgInstalledDir=' + $dependencyRoot), ('/p:EpochVcpkgInstallRoot=' + $tripletRoot + '\'))
    }
)OLD");
assert(repair_generated_windows_dependency_script(current));assert(current==windowsBuildScriptText);
}
'''
        cpp, cls.emitter = cls.scratch / "emitter.cpp", cls.scratch / "emitter.exe"
        cpp.write_text(body, encoding="utf-8")
        tool, kit = tools[-1], kits[-1]
        result = subprocess.run([str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/EHsc",
            "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/I" + str(ROOT / "Engine/include"),
            "/Fo" + str(cls.scratch / "emitter.obj"), "/Fe" + str(cls.emitter), str(cpp), "/link",
            "/LIBPATH:" + str(tool / "lib/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"),
            "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")], cwd=cls.scratch, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.output = cls.scratch / "ordinary-project"
        result = subprocess.run([str(cls.emitter), str(ROOT), str(cls.output)], cwd=cls.scratch, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        script = cls.output / "build_project.ps1"
        emitted = script.read_text(encoding="utf-8")
        original_lock = "$repoBuildLockPath = Join-Path $repoRoot 'Artifacts\\generated-project-build.lock'"
        if emitted.count(original_lock) != 1:
            raise AssertionError("The production lock destination changed; review fixture isolation.")
        script.write_text(emitted.replace(original_lock,
            "$repoBuildLockPath = Join-Path $projectRoot 'Artifacts\\generated-project-build.lock'"), encoding="utf-8")
        cls.stub = cls.scratch / "record-msbuild.ps1"
        cls.stub.write_text("@{ arguments = @($args); concurrency = $env:VCPKG_MAX_CONCURRENCY; cmake = $env:CMAKE_BUILD_PARALLEL_LEVEL; nodeReuse = $env:MSBUILDDISABLENODEREUSE; buildRoot = $env:EpochBuildRoot } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $env:EPOCH_ARGUMENT_RECEIPT\n$global:LASTEXITCODE = 0\n", encoding="utf-8")
        cls.sdk_fixture = cls.scratch / "installed-sdk"
        cls.canonical_base = cls.sdk_fixture / "Artifacts/Dependencies/vcpkg_installed"
        cls.canonical_triplet = cls.make_cache(cls.canonical_base)

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "scratch"):
            assert cls.scratch.resolve().is_relative_to((ROOT / "build/contract-tests").resolve())
            shutil.rmtree(cls.scratch)

    def run_script(self, configuration="Debug", dependency_base=None, build_root=None):
        receipt = self.scratch / "arguments.json"
        receipt.unlink(missing_ok=True)
        environment = dict(os.environ, MSBUILD_EXE_PATH=str(self.stub), EPOCH_ARGUMENT_RECEIPT=str(receipt))
        environment.pop("EPOCH_CANDIDATE_DEPENDENCY_ROOT", None)
        if dependency_base is not None:
            environment["EPOCH_CANDIDATE_DEPENDENCY_ROOT"] = str(dependency_base)
        if build_root is not None:
            environment["EpochBuildRoot"] = str(build_root)
        configuration_args = [] if configuration is None else ["-Configuration", configuration]
        result = subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
            str(self.output / "build_project.ps1"), *configuration_args], cwd=self.output,
            env=environment, capture_output=True, text=True)
        return result, json.loads(receipt.read_text(encoding="utf-8-sig")) if receipt.exists() else None

    @staticmethod
    def make_cache(base, nested=False):
        root = base / "x64-windows"
        if nested:
            root /= "x64-windows"
        headers = ("asio.hpp curl/curl.h glad/glad.h GLFW/glfw3.h glm/glm.hpp openssl/ssl.h vulkan/vulkan.hpp volk.h shaderc/shaderc.hpp spirv-tools/libspirv.hpp SDL3/SDL.h SFML/Graphics.hpp raylib.h zlib.h nlohmann/json.hpp httplib.h sqlite3.h").split()
        common = "glad.lib SDL3.lib raylib.lib sqlite3.lib libcrypto.lib libssl.lib glfw3dll.lib volk.lib vulkan-1.lib shaderc.lib SPIRV-Tools.lib".split()
        paths = [root / "include" / header for header in headers]
        for directory, extra in (("debug/lib", "sfml-graphics-d.lib sfml-window-d.lib sfml-system-d.lib libcurl-d.lib zd.lib"),
                                 ("lib", "sfml-graphics.lib sfml-window.lib sfml-system.lib libcurl.lib z.lib")):
            paths += [root / directory / library for library in common + extra.split()]
        for path in paths:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture input")
        return root

    def assert_arguments(self, receipt, base, triplet):
        arguments = receipt["arguments"]
        for argument in ("/p:VcpkgManifestInstall=false", "/p:VcpkgAutoBootstrap=false", "/p:VcpkgTriplet=x64-windows",
                "/p:PreferredToolArchitecture=x64", "/p:UseMultiToolTask=false", "/p:CL_MPCount=2", "/p:EnforceProcessCountAcrossBuilds=true", "/m:1", "/nr:false"):
            self.assertIn(argument, arguments)
        self.assertIn("/v:minimal", arguments)
        self.assertNotIn("/clp:ErrorsOnly", arguments)
        escape = lambda value: (str(value) + "\\").replace("%", "%25").replace(";", "%3B")
        self.assertIn("/p:VcpkgInstalledDir=" + escape(base), arguments)
        self.assertIn("/p:EpochVcpkgInstallRoot=" + escape(triplet), arguments)
        self.assertEqual((receipt["concurrency"], receipt["cmake"], receipt["nodeReuse"]), ("2", "2", "1"))

    def test_ordinary_build_reuses_actual_canonical_cache_without_install(self):
        # Execute the actual argument builder and default dependency selection
        # against the isolated SDK tree. Full generated-script tests below still
        # run source admission against the unchanged physical source edition.
        source = (self.output / "build_project.ps1").read_text(encoding="utf-8-sig")
        start = source.index("    $msbuildArgs = @(\n")
        stop = source.index("    # EPOCH_INSTALLED_DEPENDENCIES_END", start)
        preflight = self.scratch / "canonical-preflight.ps1"
        preflight.write_text(
            "param($repoRoot, $Configuration, $projectFile, $receiptStub)\n"
            "$ErrorActionPreference = 'Stop'\n"
            "$Platform = 'x64'\n"
            "$solutionDir = $repoRoot + '\\'\n"
            "$vcpkgManifestRoot = Join-Path $repoRoot 'Engine'\n"
            "$logPath = Join-Path $PSScriptRoot 'canonical-preflight.log'\n"
            + source[start:stop] + "& $receiptStub @msbuildArgs\n", encoding="utf-8")
        receipt_path = self.scratch / "canonical-arguments.json"
        environment = dict(os.environ, EPOCH_ARGUMENT_RECEIPT=str(receipt_path))
        environment.pop("EPOCH_CANDIDATE_DEPENDENCY_ROOT", None)
        for configuration in ("Debug", "Release"):
            result = subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                str(preflight), str(self.sdk_fixture), configuration, str(self.output / "DependencyFixture.vcxproj"), str(self.stub)],
                cwd=self.scratch, env=environment, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            receipt = json.loads(receipt_path.read_text(encoding="utf-8-sig"))
            self.assertIn(f"/p:Configuration={configuration}", receipt["arguments"])
            self.assert_arguments(receipt, self.canonical_base, self.canonical_triplet)

    def test_default_release_and_real_build_output_are_preserved(self):
        original = self.stub.read_text(encoding="utf-8")
        messages = ("Scanning sources for module dependencies...", "Compiling...", "fixture.module.ixx", "Linking...")
        try:
            self.stub.write_text(original + "\n" + "\n".join("Write-Output '" + line + "'" for line in messages) + "\n", encoding="utf-8")
            result, receipt = self.run_script(configuration=None, dependency_base=self.canonical_base)
        finally:
            self.stub.write_text(original, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("/p:Configuration=Release", receipt["arguments"])
        self.assert_arguments(receipt, self.canonical_base, self.canonical_triplet)
        captured = (self.output / "Artifacts/Reports/build-release-x64.log").read_bytes()
        transcript = captured.decode("utf-16" if captured.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")
        for line in messages:
            self.assertIn(line, result.stdout)
            self.assertIn(line, transcript)

    def test_real_resolver_prefers_x64_and_preserves_explicit_selection(self):
        source = (self.output / "build_project.ps1").read_text(encoding="utf-8-sig")
        resolver = source.split("function Resolve-MSBuild", 1)[1].split("function Enter-EpochGeneratedBuildLock", 1)[0]
        environment = dict(os.environ)
        environment.pop("MSBUILD_EXE_PATH", None)
        result = subprocess.run(["powershell.exe", "-NoProfile", "-Command",
            "function Resolve-MSBuild" + resolver + "\nResolve-MSBuild"], env=environment,
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        selected = Path(result.stdout.strip())
        self.assertTrue(selected.is_file())
        self.assertEqual(selected.parent.name.lower(), "amd64")
        build_root = self.scratch / "isolated-build-root"
        result, receipt = self.run_script(dependency_base=self.canonical_base, build_root=build_root)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(str(self.stub), result.stdout)  # Explicit host selection wins.
        self.assertEqual(receipt["buildRoot"], str(build_root))
        self.assert_arguments(receipt, self.canonical_base, self.canonical_triplet)
        for configuration in ("Debug", "Release"):
            properties = subprocess.run([str(selected), str(self.output / "DependencyFixture.vcxproj"),
                "/nologo", "/nr:false", "/p:Platform=x64", f"/p:Configuration={configuration}",
                "/p:PreferredToolArchitecture=x64", "/getProperty:EpochBuildRoot,EpochEngineLibraryDirectory,PreferredToolArchitecture"],
                cwd=self.output, env=dict(environment, EpochBuildRoot=str(build_root)), capture_output=True, text=True)
            self.assertEqual(properties.returncode, 0, properties.stdout + properties.stderr)
            values = json.loads(properties.stdout)["Properties"]
            self.assertEqual(Path(values["EpochBuildRoot"]), build_root)
            edition = "public" if "public" in values["EpochEngineLibraryDirectory"] else "development"
            self.assertEqual(Path(values["EpochEngineLibraryDirectory"]), build_root / "Runtime/windows" / edition / "x64" / configuration)
            self.assertEqual(values["PreferredToolArchitecture"], "x64")

    def test_cache_preflight_prefers_canonical_and_rejects_incomplete_overrides(self):
        base = self.scratch / "candidate-cache"
        canonical = self.make_cache(base)
        self.make_cache(base, nested=True)
        result, receipt = self.run_script(dependency_base=base)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_arguments(receipt, base, canonical)
        shutil.rmtree(canonical / "include")
        result, receipt = self.run_script(dependency_base=base)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_arguments(receipt, canonical, canonical / "x64-windows")
        shutil.rmtree(canonical / "x64-windows/debug/lib")
        result, receipt = self.run_script(dependency_base=base)
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(receipt)
        self.assertIn("will not download or reinstall", result.stdout)
        result, receipt = self.run_script(dependency_base=self.scratch / "missing-cache")
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(receipt)  # An explicit invalid root never falls back to the working SDK.
        special = self.scratch / "cache;100%"
        triplet = self.make_cache(special)
        result, receipt = self.run_script(dependency_base=special)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_arguments(receipt, special, triplet)

    def test_actual_msbuild_evaluation_keeps_base_and_triplet_distinct(self):
        self.assertTrue(self.msbuild.is_file(), "Installed MSVC is missing x64 MSBuild")
        project = self.output / "DependencyFixture.vcxproj"
        # Hosted CI bootstraps vcpkg for CMake without installing user-wide
        # MSBuild integration. Evaluate its real targets inside this fixture.
        vcpkg_root = os.environ.get("EPOCH_VCPKG_ROOT") or os.environ.get("VCPKG_ROOT")
        if vcpkg_root:
            targets = Path(vcpkg_root).resolve() / "scripts/buildsystems/msbuild/vcpkg.targets"
            self.assertTrue(targets.is_file(), "Configured vcpkg root is missing its MSBuild targets")
            source = project.read_text(encoding="utf-8")
            body, closing, trailing = source.rpartition("</Project>")
            self.assertTrue(closing and not trailing.strip())
            project = self.output / "DependencyFixtureWithVcpkg.vcxproj"
            project.write_text(body + f"  <Import Project={quoteattr(str(targets))}"
                f" Condition=\"'$(_ZVcpkgInstalledDir)'==''\" />\n{closing}{trailing}",
                encoding="utf-8")
        for configuration in ("Debug", "Release"):
            result = subprocess.run([str(self.msbuild), str(project), "/nologo", "/nr:false",
                "/p:Platform=x64", f"/p:Configuration={configuration}",
                "/getProperty:VcpkgInstalledDir,_ZVcpkgInstalledDir,_ZVcpkgCurrentInstalledDir,VcpkgManifestInstall,VcpkgAutoBootstrap,EpochVcpkgInstallRoot,CL_MPCount,UseMultiToolTask"],
                cwd=self.output, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            properties = json.loads(result.stdout)["Properties"]
            base, triplet = ROOT / "Artifacts/Dependencies/vcpkg_installed", ROOT / "Artifacts/Dependencies/vcpkg_installed/x64-windows"
            for field in ("VcpkgInstalledDir", "_ZVcpkgInstalledDir"):
                self.assertEqual(Path(properties[field]), base, field)
            for field in ("EpochVcpkgInstallRoot", "_ZVcpkgCurrentInstalledDir"):
                self.assertEqual(Path(properties[field]), triplet, field)
            self.assertEqual(properties["VcpkgManifestInstall"].lower(), "false")
            self.assertEqual(properties["VcpkgAutoBootstrap"].lower(), "false")
            self.assertEqual(properties["CL_MPCount"], "2")
            self.assertEqual(properties["UseMultiToolTask"].lower(), "false")

    def test_required_runtime_assets_copy_exactly_with_both_build_owners(self):
        names = ("fonts/Roboto-Regular.ttf", "fonts/OFL.txt", "vulkan/texture.ppm", "vulkan/shaders/vert.spv",
            "vulkan/shaders/frag.spv", "vulkan/shaders/gui_vert.spv", "vulkan/shaders/gui_frag.spv")
        self.assertTrue(self.msbuild.is_file(), "Installed MSVC is missing x64 MSBuild")
        staged = self.scratch / "staged-msbuild"
        result = subprocess.run([str(self.msbuild), str(self.output / "DependencyFixture.vcxproj"), "/nologo", "/nr:false",
            "/t:EpochCopyRequiredRuntimeAssets", "/p:Configuration=Release", "/p:Platform=x64", f"/p:OutDir={staged}\\"],
            cwd=self.output, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
        for name in names:
            self.assertEqual(digest(staged / "assets" / name), digest(ROOT / "Engine/assets" / name))
        self.assertEqual(len(list(staged.rglob("*.*"))), len(names))
        # An ordinary CMake consumer uses the same explicit seven files. This
        # target runs only cmake -E copy/touch, not an Engine or GPU executable.
        consumer, build = self.scratch / "asset-consumer", self.scratch / "asset-build"
        consumer.mkdir()
        cmake = ("cmake_minimum_required(VERSION 3.26)\nproject(AssetClosure LANGUAGES NONE)\n"
            f'set(EPOCH_REPO_ROOT "{ROOT.as_posix()}")\nset(epoch_project_output_dir "{(self.scratch / "staged-cmake").as_posix()}")\n'
            'add_custom_target(epoch_project_runtime ALL COMMAND "${CMAKE_COMMAND}" -E true)\n'
            f'include("{(self.output / "runtime-assets.cmake").as_posix()}")\n')
        (consumer / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
        result = subprocess.run(["cmake", "-S", str(consumer), "-B", str(build), "-G", "Ninja"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run(["cmake", "--build", str(build), "--target", "epoch_project_runtime", "--parallel", "1"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for name in names:
            self.assertEqual(digest(self.scratch / "staged-cmake/assets" / name), digest(ROOT / "Engine/assets" / name))
        # Fail closed for a missing required resource, including from a build
        # tree that already exists. Never fill it from another checkout/root.
        bad = self.scratch / "missing-assets"
        bad.mkdir()
        fragment = (self.output / "runtime-assets.xml").read_text()
        project = bad / "missing.proj"
        project.write_text('<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><PropertyGroup>'
            f'<EpochRepoRoot>{bad}\\</EpochRepoRoot><OutDir>{bad}\\output\\</OutDir></PropertyGroup>'
            '<Target Name="EpochCopyProjectAssets" />' + fragment + '</Project>', encoding="utf-8")
        result = subprocess.run([str(self.msbuild), str(project), "/nologo", "/nr:false", "/t:EpochCopyRequiredRuntimeAssets"], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Required Engine runtime asset is missing", result.stdout)
        (consumer / "CMakeLists.txt").write_text(cmake.replace(
            f'set(EPOCH_REPO_ROOT "{ROOT.as_posix()}")', f'set(EPOCH_REPO_ROOT "{bad.as_posix()}")'), encoding="utf-8")
        result = subprocess.run(["cmake", "-S", str(consumer), "-B", str(build)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Required Engine runtime asset is missing", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
