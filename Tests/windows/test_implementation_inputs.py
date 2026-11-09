"""Opt-in, real MSBuild proof for implementation-fragment invalidation.

Run: python Tests/windows/test_implementation_inputs.py
The tiny console fixture and all logs stay under this checkout's build/validation.
"""
from pathlib import Path
import json
import os
import subprocess
import time
import unittest
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[2]
MSBUILD = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"


@unittest.skipUnless(os.name == "nt" and MSBUILD.is_file(), "requires installed MSVC")
class ImplementationInputs(unittest.TestCase):
    def test_actual_engine_owner_metadata(self):
        project = ROOT / "Engine/examples/EpochEngine/EpochEngine.vcxproj"
        command = [str(MSBUILD), str(project), "/nologo", "/t:EpochApplyImplementationInvalidation", "/p:Configuration=Debug", "/p:Platform=x64", "/p:UseMultiToolTask=true", "/getItem:ClCompile,_EpochChangedImplementationOwner,EpochImplementationFragment", "/getProperty:IntDir"]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout[-6000:])
        data = json.loads(result.stdout)
        items = data["Items"]
        changed = {item["FullPath"] for item in items["_EpochChangedImplementationOwner"]}
        for item in items["ClCompile"]:
            if item["FullPath"] in changed:
                self.assertEqual(item["MinimalRebuildFromTracking"], "false", item["FullPath"])
            elif item["FullPath"].endswith("gui.engine.cpp"):
                self.assertEqual(item["MinimalRebuildFromTracking"], "true", "unrelated owner invalidated")
        for item in items["EpochImplementationFragment"]:
            self.assertEqual(Path(item["ObjectFile"]).parent, Path(data["Properties"]["IntDir"]))
        receipt = ROOT / "build/validation/implementation-inputs-oct26/production-invalidation-final.json"
        receipt.write_text(json.dumps(data, indent=2))
        print("Actual Engine metadata proof:", len(changed), "stale owners correctly invalidated")

    def test_changed_fragment_rebuilds_only_its_owner(self):
        fixture = ROOT / "build/validation/implementation-inputs-oct26" / str(time.time_ns())
        fixture.mkdir(parents=True)
        (fixture / "main.cpp").write_text('int value(); int second(); int main() { return value() + second(); }\n')
        (fixture / "owner.cpp").write_text('#include "value.inl"\nint value() { return expected; }\n')
        (fixture / "second.cpp").write_text('#include "second.inl"\nint second() { return extra; }\n')
        second_fragment = fixture / "second.inl"
        second_fragment.write_text("constexpr int extra = 0;\n")
        fragment = fixture / "value.inl"
        fragment.write_text("constexpr int expected = 1;\n")
        target = escape(str(ROOT / "Tools/MSVC/EpochImplementationInputs.targets"))
        project = fixture / "fixture.vcxproj"
        project.write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
  <PropertyGroup Label="Globals"><ProjectGuid>{06312F51-A679-4450-AD38-07B59B26F902}</ProjectGuid><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset></PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <PropertyGroup><OutDir>$(ProjectDir)bin\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir><TargetName>fixture</TargetName><VcpkgEnabled>false</VcpkgEnabled></PropertyGroup>
  <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp23</LanguageStandard><PrecompiledHeader>NotUsing</PrecompiledHeader><DebugInformationFormat>ProgramDatabase</DebugInformationFormat><MinimalRebuildFromTracking>true</MinimalRebuildFromTracking></ClCompile><Link><SubSystem>Console</SubSystem></Link></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="$(ProjectDir)main.cpp" /><ClCompile Include="$(ProjectDir)owner.cpp" /><ClCompile Include="$(ProjectDir)second.cpp" />
    <EpochImplementationFragment Include="$(ProjectDir)value.inl"><Owner>$(ProjectDir)owner.cpp</Owner><ObjectFile>$(IntDir)owner.obj</ObjectFile></EpochImplementationFragment>
    <EpochImplementationFragment Include="$(ProjectDir)second.inl"><Owner>$(ProjectDir)second.cpp</Owner><ObjectFile>$(IntDir)second.obj</ObjectFile></EpochImplementationFragment></ItemGroup>
  <Import Project="''' + target + '''" />
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>''')

        def build(name, multitool):
            command = [str(MSBUILD), str(project), "/nologo", "/m:1", "/p:Configuration=Debug", "/p:Platform=x64", f"/p:UseMultiToolTask={str(multitool).lower()}", "/p:CL_MPCount=2"]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            (fixture / (name + ".log")).write_text(result.stdout)
            self.assertEqual(result.returncode, 0, result.stdout[-6000:])
            return result.stdout

        build("initial", False)
        # The tool-mode change itself refreshes command tracking. Establish its
        # normal baseline before measuring fragment-only invalidation.
        build("multitool-baseline", True)
        exe = fixture / "bin/fixture.exe"
        self.assertEqual(subprocess.run([str(exe)]).returncode, 1)
        owner = fixture / "obj/owner.obj"
        other = fixture / "obj/main.obj"
        old_owner, old_other = owner.stat().st_mtime_ns, other.stat().st_mtime_ns
        time.sleep(1.05)
        fragment.write_text("constexpr int expected = 2;\n")
        log = build("fragment-multitool", True)
        self.assertIn("Implementation changed:", log)
        self.assertGreater(owner.stat().st_mtime_ns, old_owner)
        self.assertEqual(other.stat().st_mtime_ns, old_other, "unrelated TU recompiled")
        self.assertEqual(subprocess.run([str(exe)]).returncode, 2)
        changed = owner.stat().st_mtime_ns
        build("unchanged-multitool", True)
        self.assertEqual(owner.stat().st_mtime_ns, changed)
        self.assertEqual(other.stat().st_mtime_ns, old_other)
        time.sleep(1.05)
        fragment.write_text("constexpr int expected = 4;\n")
        second_fragment.write_text("constexpr int extra = 3;\n")
        build("two-owners-changed", True)
        self.assertEqual(subprocess.run([str(exe)]).returncode, 7)
        self.assertEqual(other.stat().st_mtime_ns, old_other)
        (fixture / "result.json").write_text(json.dumps({"passed": True, "initial_exit": 1, "changed_exit": 2, "two_changed_owners_exit": 7, "unrelated_object_unchanged": True, "unchanged_build_recompiled": False}, indent=2))
        print("Receipt:", fixture / "result.json")


if __name__ == "__main__":
    unittest.main()
