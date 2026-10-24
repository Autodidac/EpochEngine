"""Preprocess actual edition policies and evaluate their build wiring.

No renderer, engine runtime, model or network service is started.
"""
from Tools.source.owned_source_text import read_owned_source
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FLAG = 'EPOCH_ENABLE_TEXTURE_DIAGNOSTICS'


class TextureDiagnosticPolicy(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(parent.resolve().is_relative_to(ROOT.resolve()))
        self.scratch = Path(tempfile.mkdtemp(prefix='texture-policy-', dir=parent))
        self.assertTrue(self.scratch.resolve().is_relative_to(parent.resolve()))

    def tearDown(self):
        # The exact resolved disposable path was checked against this checkout.
        parent = (ROOT / 'build/contract-tests').resolve()
        self.assertTrue(parent.is_relative_to(ROOT.resolve()))
        self.assertTrue(self.scratch.resolve().is_relative_to(parent))
        shutil.rmtree(self.scratch)

    def compiler(self):
        if os.name == 'nt':
            base = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))
            choices = sorted((base / 'Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob(
                '*/bin/Hostx64/x64/cl.exe'))
            if choices:
                return str(choices[-1]), True
        for name in ('clang++', 'g++', 'c++'):
            found = shutil.which(name)
            if found:
                return found, False
        self.skipTest('No C++ preprocessor is installed')

    def edition_roots(self):
        roots = [(ROOT, 0)]
        public = ROOT / 'build/edition-split/public-source'
        if (public / 'Engine/include/epoch.edition.hpp').exists():
            roots.append((public, 1))
        for root, expected in roots:
            marker = re.search(r'^#define EPOCH_SOURCE_EDITION_PUBLIC ([01])$',
                read_owned_source(root / 'Engine/include/epoch.edition.hpp', encoding='utf-8'), re.M)
            self.assertIsNotNone(marker)
            self.assertEqual(int(marker[1]), expected)
        return roots

    def preprocess(self, root, expected_edition, checks, defines=(), preamble=''):
        compiler, msvc = self.compiler()
        source = self.scratch / 'policy.cpp'
        source.write_text(preamble + '\n'
            f'#include "{(root / "Engine/include/epoch.edition.hpp").as_posix()}"\n'
            f'#include "{(root / "Engine/include/engine.config.hpp").as_posix()}"\n'
            f'#if EPOCH_EDITION_PUBLIC != {expected_edition}\n#error Edition changed\n#endif\n'
            + checks, encoding='utf-8')
        args = [compiler, '/nologo', '/EP', '/TP'] if msvc else [compiler, '-E', '-x', 'c++']
        args += [('/D' if msvc else '-D') + name + (('=' + value) if value else '')
            for name, value in defines]
        return subprocess.run(args + [str(source)], cwd=ROOT, capture_output=True, text=True)

    def expect_policy(self, root, edition, checks, defines=(), preamble=''):
        result = self.preprocess(root, edition, checks, defines, preamble)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    @staticmethod
    def value_checks(values):
        return ''.join(f'#if ({name}) != {value}\n#error {name} mismatch\n#endif\n'
            for name, value in values.items())

    def active_guard(self, root, relative, expression):
        # Extract a real production guard, excluding commented legacy examples.
        source = read_owned_source(root / relative, encoding='utf-8')
        match = re.search(r'^\s*#if\s+(' + re.escape(expression) + r')\s*$', source, re.M)
        self.assertIsNotNone(match, relative)
        return match[1]

    def test_actual_header_numeric_and_legacy_matrix(self):
        matrix = [([], 0), ([(FLAG, '0')], 0), ([(FLAG, '1')], 1),
            ([('DEBUG_TEXTURE_RENDERING_VERBOSE', '')], 1),
            ([('DEBUG_TEXTURE_RENDERING_VERBOSE', '0')], 1),
            ([(FLAG, '0'), ('DEBUG_TEXTURE_RENDERING_VERBOSE', '1')], 0),
            ([(FLAG, '1'), ('DEBUG_TEXTURE_RENDERING_VERBOSE', '0')], 1)]
        for root, edition in self.edition_roots():
            for defines, expected in matrix:
                with self.subTest(edition=edition, defines=defines):
                    self.expect_policy(root, edition, self.value_checks({FLAG: expected}), defines)
            result = self.preprocess(root, edition, '', [(FLAG, '2')])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('must be 0 or 1', result.stderr + result.stdout)

    def test_confirmation_overrides_reach_actual_context_and_upload_guards(self):
        master = 'EPOCH_ENABLE_BACKEND_CONFIRMATION_LOGS'
        context = 'EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS'
        upload = 'EPOCH_ENABLE_BACKEND_UPLOAD_CONFIRMATION_LOGS'
        raylib = 'EPOCH_ENABLE_RAYLIB_CONFIRMATION_LOGS'
        opengl = 'EPOCH_ENABLE_OPENGL_CONFIRMATION_LOGS'
        matrix = [([], 0, 0), ([(master, '1')], 1, 1),
            ([(master, '1'), (context, '0')], 0, 1),
            ([(master, '1'), (opengl, '0')], 1, 0),
            ([(master, '0'), (context, '1'), (raylib, '1')], 1, 0),
            ([(master, '0'), (upload, '1'), (opengl, '1')], 0, 1)]
        for root, edition in self.edition_roots():
            context_guard = self.active_guard(root, 'Engine/modules/raylib.context.ixx', context + ' && ' + raylib)
            upload_guard = self.active_guard(root, 'Engine/modules/opengl.textures.ixx', upload + ' && ' + opengl)
            for defines, context_value, upload_value in matrix:
                with self.subTest(edition=edition, defines=defines):
                    self.expect_policy(root, edition,
                        self.value_checks({context_guard: context_value, upload_guard: upload_value}), defines)

    def test_slow_frame_and_runtime_diagnostic_controls(self):
        slow = 'EPOCH_ENABLE_RENDERER_SLOW_FRAME_LOGS'
        grace = 'EPOCH_SLOW_FRAME_LOG_STARTUP_GRACE_MS'
        throttle = 'EPOCH_SLOW_FRAME_LOG_THROTTLE_MS'
        trace = 'EPOCH_VULKAN_RUNTIME_DIAGNOSTICS'
        for root, edition in self.edition_roots():
            self.active_guard(root, 'Engine/modules/diagnostics.engine.ixx', '!' + slow)
            self.active_guard(root, 'Engine/src/renderers/vulkan/vulkan.platform_context.cpp', trace)
            self.expect_policy(root, edition, self.value_checks({slow: 1, grace: 5000, throttle: 5000, trace: 0}))
            self.expect_policy(root, edition, self.value_checks({slow: 0, grace: 0, throttle: 0, trace: 1}),
                [(slow, '0'), (grace, '0'), (throttle, '0'), (trace, '1')])
            for interval in (grace, throttle):
                with self.subTest(edition=edition, negative=interval):
                    result = self.preprocess(root, edition, '', [(interval, '-1')])
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('nonnegative milliseconds', result.stderr + result.stdout)

    def test_force_presence_precedence_and_platform_policy(self):
        backends = ('OPENGL', 'SDL', 'SFML', 'RAYLIB', 'SOFTWARE_RENDERER', 'VULKAN', 'DIRECTX')
        for root, edition in self.edition_roots():
            # These exercise the real policy's OS predicates; they are not a
            # Linux compiler/backend build or runtime-parity claim.
            for platform in ('windows', 'linux'):
                preamble = '#undef _WIN32\n#undef __linux__\n#define ' + (
                    '_WIN32 1\n' if platform == 'windows' else '__linux__ 1\n')
                cases = [('default', []), ('disable', [('DISABLE', '')]),
                    ('enable', [('ENABLE', '')]), ('both', [('DISABLE', ''), ('ENABLE', '')]),
                    ('zero_still_present', [('DISABLE', '0')])]
                for case, forces in cases:
                    defines = [('EPOCH_FORCE_' + force + '_' + backend, value)
                        for backend in backends for force, value in forces]
                    checks = self.value_checks({'EPOCH_SINGLE_PARENT': int(platform == 'windows')})
                    for backend in backends:
                        enabled = case not in ('disable', 'zero_still_present') and (
                            backend != 'DIRECTX' or platform == 'windows')
                        name = 'EPOCH_USING_' + backend
                        checks += (f'#if !defined({name}) || {name} != 1\n#error {name} missing\n#endif\n'
                            if enabled else f'#ifdef {name}\n#error {name} unexpectedly present\n#endif\n')
                    with self.subTest(edition=edition, platform=platform, case=case):
                        self.expect_policy(root, edition, checks, defines, preamble)

    def test_connected_authoring_and_device_guards(self):
        platform = 'EPOCH_ENABLE_AUTHORING_PLATFORM'
        slices = [('Engine/src/project/project.contracts.cpp', 'EPOCH_ENABLE_TEXTURE_EDITOR'),
            ('Engine/src/project/project.contracts.cpp', 'EPOCH_ENABLE_TILEMAP_EDITOR'),
            ('Engine/src/project/project.gameplay2d_runtime.cpp', 'EPOCH_ENABLE_ANIMATION_EDITOR')]
        for root, edition in self.edition_roots():
            expressions = [self.active_guard(root, path, platform + ' && ' + flag) for path, flag in slices]
            input_guard = self.active_guard(root, 'Engine/src/input/input.controller_sdl.cpp', 'EPOCH_ENABLE_PHYSICAL_INPUT')
            audio_guard = self.active_guard(root, 'Engine/src/epoch/epoch.engine_legacy.cpp', 'EPOCH_ENABLE_PHYSICAL_AUDIO')
            matrix = [([], [1, 1, 1, 1, 1]), ([(platform, '0')], [0, 0, 0, 1, 1]),
                ([(flag, '0') for _, flag in slices], [0, 0, 0, 1, 1]),
                ([(input_guard, '0'), (audio_guard, '0')], [1, 1, 1, 0, 0])]
            for defines, expected in matrix:
                with self.subTest(edition=edition, defines=defines):
                    self.expect_policy(root, edition,
                        self.value_checks(dict(zip(expressions + [input_guard, audio_guard], expected))), defines)
            # Build choices cannot turn either physical edition into the other.
            result = self.preprocess(root, edition, '', [('EPOCH_EDITION_PUBLIC', str(1 - edition))])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('conflicts with physical source edition', result.stderr + result.stdout)

    def test_dead_sample_config_has_no_remaining_build_or_source_references(self):
        dead = re.compile(r'\b(?:EPOCH_BACKEND_(?:VULKAN|D3D12|METAL|NULL)|'
            r'EPOCH_ENABLE_(?:GPU_FEEDBACK|VISIBILITY_BUFFER|DIRECTX_CONFIRMATION_LOGS))\b')
        for root, edition in self.edition_roots():
            with self.subTest(edition=edition):
                self.assertFalse((root / 'Engine/include/epoch.config.hpp').exists())
                for directory in ('Engine/include', 'Engine/src', 'Engine/modules'):
                    for path in (root / directory).rglob('*'):
                        if path.suffix not in ('.hpp', '.cpp', '.ixx'):
                            continue
                        source = read_owned_source(path, encoding='utf-8-sig')
                        self.assertNotIn('epoch.config.hpp', source, str(path.relative_to(root)))
                        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
                        self.assertIsNone(dead.search(source), str(path.relative_to(root)))
                for relative in ('Engine/Engine.vcxitems', 'Engine/Engine.vcxitems.filters',
                        'Engine/docs/engine/modules.txt'):
                    self.assertNotIn('epoch.config.hpp', read_owned_source(root / relative, encoding='utf-8'))

    def test_all_verbose_consumers_read_numeric_policy(self):
        for root, edition in self.edition_roots():
            total = 0
            for filename in ('atlas.texture.ixx', 'sprite.registry.ixx', 'sprite.pool.ixx'):
                source = read_owned_source(root / 'Engine/modules' / filename, encoding='utf-8')
                self.assertLess(source.index('#include "../include/engine.config.hpp"'), source.index('export module'))
                self.assertNotIn('defined(DEBUG_TEXTURE_RENDERING_VERBOSE)', source)
                total += source.count('#if EPOCH_ENABLE_TEXTURE_DIAGNOSTICS')
            self.assertEqual(total, 5, f'edition={edition}')

    def test_cmake_emits_numeric_values_from_real_option(self):
        cmake = shutil.which('cmake')
        if not cmake:
            self.skipTest('CMake not installed')
        for root, edition in self.edition_roots():
            source = read_owned_source(root / 'Engine/CMakeLists.txt', encoding='utf-8')
            option = re.search(r'^option\(EPOCH_ENABLE_TEXTURE_DIAGNOSTICS [^\n]+\)$', source, re.M)
            self.assertIsNotNone(option)
            definitions = source.split('\ntarget_compile_definitions(epoch PRIVATE\n', 1)[1].split('\n)', 1)[0]
            definition = re.search(r'^\s*(EPOCH_ENABLE_TEXTURE_DIAGNOSTICS=\$<BOOL:\$\{EPOCH_ENABLE_TEXTURE_DIAGNOSTICS\}>)$',
                definitions, re.M)
            self.assertIsNotNone(definition)
            # Exercise each physical edition's actual option and generator
            # expression without loading engine dependencies or building it.
            project = self.scratch / f'cmake-{edition}'
            project.mkdir()
            (project / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\n'
                'project(DiagnosticPolicy LANGUAGES NONE)\n' + option[0] + '\n'
                'add_library(diag INTERFACE)\n'
                'target_compile_definitions(diag INTERFACE ' + definition[1] + ')\n'
                'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/definitions.txt" '
                'CONTENT "$<JOIN:$<TARGET_PROPERTY:diag,INTERFACE_COMPILE_DEFINITIONS>,;>")\n', encoding='utf-8')
            for value, expected in [('OFF', 0), ('ON', 1)]:
                output = project / value
                result = subprocess.run([cmake, '-S', str(project), '-B', str(output),
                    '-G', 'Ninja', '-D' + FLAG + '=' + value], cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(read_owned_source(output / 'definitions.txt', encoding='utf-8'), f'{FLAG}={expected}')

    @unittest.skipUnless(os.name == 'nt', 'MSVC property evaluation is Windows-specific')
    def test_msvc_projects_emit_shared_property(self):
        msbuild = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / (
            'Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe')
        if not msbuild.exists():
            self.skipTest('MSBuild not installed')
        for root, edition in self.edition_roots():
            for name in ('EpochEngine', 'EpochEditor', 'HeadlessCI', 'AiTransportContract'):
                project = root / 'Engine/examples' / name / (name + '.vcxproj')
                for value, expected in [('false', 0), ('true', 1), ('0', 0), ('1', 1)]:
                    with self.subTest(edition=edition, project=name, value=value):
                        result = subprocess.run([str(msbuild), str(project), '/nologo',
                            '/p:Configuration=Release', '/p:Platform=x64',
                            '/p:EpochTextureDiagnostics=' + value,
                            '/getProperty:EpochTextureDiagnosticsDefine', '/getItem:ClCompile'],
                            cwd=ROOT, capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        evaluated = json.loads(result.stdout)
                        self.assertEqual(evaluated['Properties']['EpochTextureDiagnosticsDefine'], str(expected))
                        items = evaluated['Items']['ClCompile']
                        self.assertTrue(items)
                        for item in items:
                            self.assertIn(f'{FLAG}={expected}', item['PreprocessorDefinitions'].split(';'))
            result = subprocess.run([str(msbuild), str(project), '/nologo',
                '/p:Configuration=Release', '/p:Platform=x64', '/p:EpochTextureDiagnostics=invalid',
                '/t:EpochValidateTextureDiagnostics'], cwd=ROOT, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('must be false, true, 0 or 1', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
