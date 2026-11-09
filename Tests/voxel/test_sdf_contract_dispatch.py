"""Link the actual SDF contract through the Engine's real dispatch expression."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "Engine/src/voxel/voxel.sdf_contract.cpp"
ENTRY = ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp"
DISPATCH = ROOT / "Engine/src/epoch/implementation/epoch.contracts_engine.inl"
NAME = "voxel.sdf.primitive_and_edit_contract"


class SdfContractDispatch(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "MSVC module-link regression")
    def test_actual_contract_pass_and_failed_expectation_reaches_engine_check(self):
        ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
        include = "$(MSBuildThisFileDirectory)src\\voxel\\voxel.sdf_contract.cpp"
        items = ET.parse(ROOT / "Engine/Engine.vcxitems").getroot()
        matches = [node for node in items.findall(".//m:ClCompile", ns)
                   if node.get("Include") == include]
        self.assertEqual(len(matches), 1, "MSVC must compile the actual contract once")
        self.assertFalse(matches[0].findall("m:ExcludedFromBuild", ns))
        filters = ET.parse(ROOT / "Engine/Engine.vcxitems.filters").getroot()
        self.assertEqual(sum(node.get("Include") == include
                             for node in filters.findall(".//m:ClCompile", ns)), 1)
        cmake = (ROOT / "Engine/CMakeLists.txt").read_text(encoding="utf-8-sig")
        sources = re.search(r"set\(EPOCH_SOURCES\s+(.*?)\n\)", cmake, re.S)
        self.assertIsNotNone(sources)
        self.assertEqual(sources[1].split().count("src/voxel/voxel.sdf_contract.cpp"), 1)
        self.assertIn("EPOCH_VOXEL_SDF_CONTRACT_MAIN=1", cmake)

        entry = ENTRY.read_text(encoding="utf-8-sig")
        declaration = re.search(
            r"namespace epochengine::voxel::sdf::contracts\s*\{\s*"
            r"\[\[nodiscard\]\] int run_contract\(\) noexcept;\s*\}", entry)
        self.assertIsNotNone(declaration)
        owner = DISPATCH.read_text(encoding="utf-8-sig")
        call = re.search(r'check\("' + re.escape(NAME)
                         + r'",\s*epochengine::voxel::sdf::contracts::run_contract\(\) == 0\);', owner)
        self.assertIsNotNone(call)
        self.assertEqual(owner.count('"' + NAME + '"'), 1)

        consumer = '''#include <string_view>
import voxel.field;
import voxel.sdf;
DECLARATION
int main() {
    bool failed = false;
    unsigned checks = 0;
    const auto check = [&](std::string_view name, bool passed) {
        ++checks;
        failed = failed || !passed || name != "CHECK_NAME";
    };
    ACTUAL_CALL
    return failed || checks != 1u ? 1 : 0;
}
'''.replace("DECLARATION", declaration[0]).replace("ACTUAL_CALL", call[0]).replace("CHECK_NAME", NAME)
        source = CONTRACT.read_text(encoding="utf-8-sig")
        expectation = "near(sample(sphere, {}), -2.0)"
        self.assertEqual(source.count(expectation), 1)
        # Only the expected value changes in this local fault fixture. Compile
        # the whole real contract; do not replace it with a stub return value.
        faulty = source.replace(expectation, "near(sample(sphere, {}), -3.0)", 1)
        identity = str(time.time_ns())
        report = ROOT / "Artifacts/Reports/voxel-sdf-dispatch-nov06" / identity
        report.mkdir(parents=True)
        (report / "consumer.cpp").write_text(consumer, encoding="utf-8")
        (report / "faulty-contract.cpp").write_text(faulty, encoding="utf-8")
        modules = ROOT / "Engine/modules"
        (report / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(SdfContractDispatch LANGUAGES CXX)
enable_testing()
foreach(lane IN ITEMS actual faulty)
  if(lane STREQUAL "actual")
    set(contract "{CONTRACT.as_posix()}")
  else()
    set(contract "${{CMAKE_CURRENT_SOURCE_DIR}}/faulty-contract.cpp")
  endif()
  add_executable(sdf_${{lane}} consumer.cpp "${{contract}}")
  target_compile_features(sdf_${{lane}} PRIVATE cxx_std_23)
  target_compile_options(sdf_${{lane}} PRIVATE /W3 /WX /utf-8 /EHsc)
  target_sources(sdf_${{lane}} PRIVATE FILE_SET CXX_MODULES
    BASE_DIRS "{modules.as_posix()}" FILES
    "{(modules/'voxel.field.ixx').as_posix()}"
    "{(modules/'voxel.sdf.ixx').as_posix()}")
endforeach()
''', encoding="utf-8")
        build = ROOT / "Artifacts/Intermediate/tests/voxel-sdf-dispatch" / identity
        runs = []
        receipt = {"scope": "Actual global-TU contract, real C++23 modules, Engine declaration/call; CPU only",
                   "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                     for path in (CONTRACT, ENTRY, DISPATCH)}, "commands": runs}

        def run(command, expected=0):
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
            log = report / f"lane-{len(runs)}.log"
            log.write_text(result.stdout + result.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": result.returncode,
                         "expected": expected, "log": str(log.relative_to(ROOT))})
            receipt["passed"] = False
            (report / "result.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
            self.assertEqual(result.returncode, expected, result.stdout[-5000:] + result.stderr[-2000:])

        run(["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"])
        for configuration in ("Debug", "Release"):
            run(["cmake", "--build", str(build), "--config", configuration, "--parallel", "1", "--",
                 "/nr:false", "/p:PreferredToolArchitecture=x64", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"])
            run([str(build / configuration / "sdf_actual.exe")])
            run([str(build / configuration / "sdf_faulty.exe")], expected=1)
        receipt["passed"] = True
        (report / "result.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
