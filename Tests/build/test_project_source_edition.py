"""Actual generated-source edition admission; CPU/files only, no Editor builds."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import importlib.util
import json
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


gate = load('actual_generated_source_gate', ROOT / 'Tools/build/verify_epoch_project_source.py')
public_gate = load('actual_public_gate', ROOT / 'Tools/build/verify_public_source.py')
PUBLIC = 'epoch.public.github.v1'
DEV = 'epoch.development.site.v1'


def definition(source, marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class ProjectSourceEdition(unittest.TestCase):
    def setUp(self):
        self.parent = ROOT / 'build/contract-tests'
        self.parent.mkdir(parents=True, exist_ok=True)
        self.scratch = Path(tempfile.mkdtemp(prefix='project-edition-', dir=self.parent)).resolve()
        self.addCleanup(self.remove_fixture)

    def remove_fixture(self):
        self.assertTrue(self.scratch.is_relative_to(self.parent.resolve()))
        shutil.rmtree(self.scratch)

    def fixture(self, public):
        root = self.scratch / ('public' if public else 'development')
        files = {
            'Engine/include/epoch.edition.hpp': ('#define EPOCH_SOURCE_EDITION_PUBLIC '
                                                + ('1' if public else '0') + '\n').encode(),
            'Engine/CMakeLists.txt': b'cmake_minimum_required(VERSION 4.4)\n',
        }
        if public:
            template = ROOT / 'Tools/build/public_ci_workflow.yml'
            files[public_gate.WORKFLOW] = (template if template.is_file()
                                          else ROOT / public_gate.WORKFLOW).read_bytes()
            files['Tools/build/verify_public_source.py'] = (ROOT / 'Tools/build/verify_public_source.py').read_bytes()
        for name, data in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        if public:
            manifest = {
                'schema': 'epoch.source_projection.v1', 'edition': 'public',
                'file_hash_algorithm': public_gate.SOURCE_HASH_ALGORITHM,
                'file_hash_normalization': public_gate.SOURCE_HASH_NORMALIZATION,
                'files': {name: public_gate.source_digest(name, data) for name, data in files.items()},
            }
            (root / 'source-projection.json').write_text(json.dumps(manifest), encoding='utf-8')
        return root

    def test_actual_admission_accepts_both_exact_editions_and_rejects_cross_source(self):
        for public, expected, foreign in ((True, PUBLIC, DEV), (False, DEV, PUBLIC)):
            root = self.fixture(public)
            gate.check_source(root, expected)
            with self.assertRaisesRegex(RuntimeError, 'edition differs'):
                gate.check_source(root, foreign)

    def test_missing_malformed_and_conflicting_physical_identity_refuse(self):
        root = self.fixture(False)
        marker = root / 'Engine/include/epoch.edition.hpp'
        for contents in ('', '#define EPOCH_SOURCE_EDITION_PUBLIC 2\n',
                         '#define EPOCH_SOURCE_EDITION_PUBLIC 0\n#define EPOCH_SOURCE_EDITION_PUBLIC 1\n',
                         '#define EPOCH_SOURCE_EDITION_PUBLIC 0\n# define EPOCH_SOURCE_EDITION_PUBLIC 1\n'):
            marker.write_text(contents, encoding='utf-8')
            with self.subTest(contents=contents), self.assertRaises(RuntimeError):
                gate.check_source(root, DEV)
        marker.unlink()
        with self.assertRaises(RuntimeError):
            gate.check_source(root, DEV)

    def test_public_requires_exact_audit_and_rejects_added_or_changed_implementation(self):
        root = self.fixture(True)
        gate.check_source(root, PUBLIC)
        for name in ('Engine/src/project/project.unreviewed.cpp',
                     'Engine/dep/EpochPlatformEngine/src/platform.unreviewed.cpp',
                     'Engine/dep/EpochAudioEngine/modules/audio.unreviewed.ixx',
                     'Engine/dep/EpochMediaEngine/include/media.unreviewed.inl',
                     'Engine/examples/OrdinaryTool/example.unreviewed.cpp'):
            extra = root/name
            extra.parent.mkdir(parents=True, exist_ok=True)
            extra.write_text('int unreviewed;\n', encoding='utf-8')
            with self.subTest(name=name), self.assertRaisesRegex(RuntimeError, 'unmanifested'):
                gate.check_source(root, PUBLIC)
            extra.unlink()
        (root / 'Engine/CMakeLists.txt').write_text('foreign content\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'content differs'):
            gate.check_source(root, PUBLIC)
        (root / 'source-projection.json').unlink()
        with self.assertRaisesRegex(RuntimeError, 'audited Public'):
            gate.check_source(root, PUBLIC)

    def test_generated_builds_admit_before_engine_configuration_and_link(self):
        source = read_owned_source(ROOT / 'Engine/src/editor/editor.scene.cpp', encoding='utf-8')
        creation = definition(source, '[[nodiscard]] static EditorProjectCreationResult write_project_shell(')
        self.assertLess(creation.index('generated_project_source_edition_allowed(repoRoot)'),
                        creation.index('write_text_file_if_allowed('))
        cmake = creation[creation.index('const std::string sourceAdmissionCmake'):creation.index('const std::string cmakeListsText')]
        self.assertLess(cmake.index('execute_process(COMMAND'), cmake.index('add_subdirectory('))
        self.assertIn('add_dependencies(epoch epoch_generated_source_guard)', cmake)
        self.assertIn('InitialTargets=\\"EpochVerifyGeneratedSource\\"', creation)
        self.assertIn('--edition ', cmake)
        self.assertIn('xml_escape(sourceEdition)', creation)
        for start, end, dispatch in (
                ('const std::string windowsBuildScriptText', 'const std::string linuxBuildScriptText', 'Resolve-MSBuild'),
                ('const std::string linuxBuildScriptText', 'bool ok', '--check-toolchain clang')):
            section = creation[creation.index(start):]
            if end in section:
                section = section[:section.index(end)]
            self.assertLess(section.index('verify_epoch_project_source.py'), section.index(dispatch))
        self.assertIn('static_assert(EPOCH_SOURCE_EDITION_PUBLIC == ', creation)
        self.assertIn('"engine_edition', creation.replace('\\"', '"'))

    def test_actual_creation_preflight_cpu(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk / 'Include').glob('*'))
        if not tools or not kits:
            self.skipTest('MSVC/SDK unavailable')
        source = read_owned_source(ROOT / 'Engine/src/editor/editor.scene.cpp', encoding='utf-8')
        production = definition(source, '[[nodiscard]] static std::string read_text_file(')
        production += '\n' + definition(source, '[[nodiscard]] static bool generated_project_source_edition_allowed(')
        roots = {False: self.fixture(False), True: self.fixture(True)}
        toolset, kit = tools[-1], kits[-1]
        for public in (False, True):
            include = self.scratch / ('headers-public' if public else 'headers-development')
            include.mkdir()
            (include / 'updater.distribution.hpp').write_bytes((ROOT / 'Engine/include/updater.distribution.hpp').read_bytes())
            edition = read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp', encoding='utf-8')
            if public:
                edition = edition.replace('#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_EDITION_PUBLIC 1')
            (include / 'epoch.edition.hpp').write_text(edition, encoding='utf-8')
            code = self.scratch / ('preflight-public.cpp' if public else 'preflight-development.cpp')
            code.write_text('#include <filesystem>\n#include <fstream>\n#include <iterator>\n#include <string>\n'
                            '#include "updater.distribution.hpp"\nnamespace fs=std::filesystem;\n' + production
                            + '\nint main(int argc,char** argv) { return argc == 2 && generated_project_source_edition_allowed(fs::path{argv[1]}) ? 0 : 1; }\n', encoding='utf-8')
            binary = code.with_suffix('.exe')
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(include), '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'),
                    '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                    '/Fo' + str(code.with_suffix('.obj')), '/Fe' + str(binary), str(code), '/link',
                    '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'ucrt/x64')]
            result = subprocess.run(args, cwd=self.scratch, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for source_public, root in roots.items():
                result = subprocess.run([str(binary), str(root)], capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0 if source_public == public else 1, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
