from Tools.source.owned_source_text import read_owned_source
"""Guard the real Platform compilation/source boundary, not file-count progress."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
DEP = ROOT/'Engine/dep/EpochPlatformEngine'


class PlatformDependencyBoundary(unittest.TestCase):
    def test_module_import_closure_has_one_owner(self):
        modules = {}
        for path in (DEP/'modules').glob('*.ixx'):
            source = read_owned_source(path, encoding='utf-8')
            declared = re.search(r'^export module\s+([^;]+);', source, re.M)
            self.assertIsNotNone(declared, str(path))
            name = declared.group(1)
            self.assertEqual(path.stem, name.replace(':', '_'))
            self.assertNotIn(name, modules)
            self.assertFalse((ROOT/'Engine/modules'/path.name).exists(), name)
            modules[name] = source
        for required in ('core.env', 'core.log', 'core.time', 'perf.tier', 'perf.select',
                         'platform.capabilities', 'platform.budgets', 'platform.window',
                         'platform.engine', 'platform.application', 'context.type', 'context.admission'):
            self.assertIn(required, modules)
        for name, source in modules.items():
            for imported in re.findall(r'^\s*(?:export\s+)?import\s+([^;]+);', source, re.M):
                self.assertTrue(imported in modules or imported == 'std', (name, imported))

    def test_owned_quoted_includes_do_not_reach_engine_implementation(self):
        owners = [*(DEP/'modules').glob('*.ixx'), *(DEP/'src').rglob('*.cpp'),
                  *(DEP/'src').rglob('*.hpp'), *(DEP/'include').glob('*.hpp'),
                  *(DEP/'include').glob('*.h')]
        for path in owners:
            source = read_owned_source(path, encoding='utf-8')
            for included in re.findall(r'^\s*#\s*include\s+"([^"\n]+)"', source, re.M):
                choices = [path.parent/included, DEP/'include'/included]
                resolved = next((p.resolve() for p in choices if p.is_file()), None)
                self.assertIsNotNone(resolved, (str(path), included))
                self.assertTrue(resolved.is_relative_to(DEP.resolve()), (str(path), included))
            self.assertNotRegex(source, re.compile(r'^\s*(?:export\s+)?import\s+(?:ai|editor|gui|render|epoch)\.', re.M), str(path))

    def test_direct_msvc_compilation_owner_is_the_dependency(self):
        project = ET.parse(DEP/'EpochPlatformEngine.vcxproj')
        compiled = [item.attrib['Include'].replace('\\', '/') for item in project.iter()
                    if item.tag.rsplit('}', 1)[-1] == 'ClCompile' and 'Include' in item.attrib]
        for path in (DEP/'modules').glob('*.ixx'):
            self.assertEqual(compiled.count('modules/'+path.name), 1, path.name)
        self.assertEqual(compiled.count('src/core/core.env.cpp'), 1)
        self.assertNotIn('src/platform/platform.entry.cpp', compiled)
        self.assertFalse((ROOT/'Engine/src/core/core.env.cpp').exists())
        shared = read_owned_source(ROOT/'Engine/Engine.vcxitems', encoding='utf-8')
        for path in (DEP/'modules').glob('*.ixx'):
            self.assertNotIn('modules\\'+path.name+'"', shared, path.name)

    def test_native_entry_is_opt_in_and_cxx23_is_minimum(self):
        cmake = read_owned_source(DEP/'CMakeLists.txt', encoding='utf-8')
        self.assertIn('add_library(Epoch::PlatformEngine ALIAS EpochPlatformModuleStatic)', cmake)
        self.assertIn('add_library(epoch_platform_entry OBJECT EXCLUDE_FROM_ALL', cmake)
        self.assertIn('add_library(Epoch::PlatformEntry ALIAS epoch_platform_entry)', cmake)
        self.assertIn('target_compile_features(EpochPlatformModule${linkage} PUBLIC cxx_std_23)', cmake)
        self.assertNotIn('cxx_std_26', cmake)
        entry = read_owned_source(DEP/'tests/platform.entry_contract.cpp', encoding='utf-8')
        self.assertIn('import platform.application;', entry)
        self.assertIn('app_get_callbacks', entry)
        self.assertIn('app_get_launch_options', entry)
        self.assertIsNone(re.search(r'^\s*int\s+main\s*\(', entry, re.M))
