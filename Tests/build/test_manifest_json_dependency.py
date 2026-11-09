"""Configure the actual manifest JSON dependency with native AI disabled."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ManifestJsonDependency(unittest.TestCase):
    def test_manifest_json_is_an_independent_engine_usage_requirement(self):
        cmake = shutil.which('cmake')
        package = ROOT / 'Engine/vcpkg_installed/x64-windows/share/nlohmann_json'
        if not cmake or not (package / 'nlohmann_jsonConfig.cmake').is_file():
            self.skipTest('Existing CMake and installed JSON package required; never install')
        production = (ROOT / 'Engine/CMakeLists.txt').read_text(encoding='utf-8')
        start = production.index('# Project manifests and update identities use JSON')
        end = production.index('    find_package(httplib', start)
        discovery = production[start:end] + '\nendif()\n'
        link = next(line for line in production.splitlines()
                    if line.startswith('target_link_libraries(epoch PRIVATE epoch_gui '))
        # A configure-only interface consumer checks the exact production link
        # list without pretending to compile the full Engine or install tools.
        link = link.replace(' PRIVATE ', ' INTERFACE ', 1)
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        fixture = Path(tempfile.mkdtemp(prefix='manifest-json-', dir=parent))
        self.addCleanup(shutil.rmtree, fixture)
        (fixture / 'dummy.cpp').write_text('// configure-only target\n', encoding='utf-8')
        (fixture / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.28)\nproject(ManifestJson NONE)\n'
            'set(EPOCH_ENABLE_NATIVE_AI OFF)\n'
            f'set(nlohmann_json_DIR "{package.as_posix()}")\n' + discovery
            + 'add_library(epoch INTERFACE)\n'
            + 'add_library(epoch_gui INTERFACE)\nadd_library(Epoch::AudioEngine INTERFACE IMPORTED)\n'
            + 'add_library(Epoch::MediaEngine INTERFACE IMPORTED)\n' + link
            + '\nget_target_property(jsonLinks epoch INTERFACE_LINK_LIBRARIES)\n'
            + 'if(NOT "nlohmann_json::nlohmann_json" IN_LIST jsonLinks)\n'
            + 'message(FATAL_ERROR "Engine manifest JSON must be independent of AI")\nendif()\n'
            + 'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/includes.txt" CONTENT "$<TARGET_PROPERTY:epoch,INTERFACE_INCLUDE_DIRECTORIES>")\n',
            encoding='utf-8')
        result = subprocess.run([cmake, '-S', str(fixture), '-B', str(fixture / 'build'), '-G', 'Ninja'],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        includes = (fixture / 'build/includes.txt').read_text(encoding='utf-8')
        self.assertIn('vcpkg_installed/x64-windows/include', includes.replace('\\', '/'))


if __name__ == '__main__':
    unittest.main()
