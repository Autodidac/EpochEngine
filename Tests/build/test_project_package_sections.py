"""Project package/resource ownership, real registry contracts and UI wiring.

Compiles the actual registry namespace and Forest Factory identity aliases,
without starting the engine, a model or a download.
"""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProjectPackageSections(unittest.TestCase):
    def roots(self):
        result = [ROOT]
        public = ROOT / 'build/publication/EpochEngine'
        if (public / 'Engine/modules/package.registry.ixx').exists():
            result.append(public)
        return result

    def test_actual_registry_classification_and_admission_contracts(self):
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(parent.is_relative_to(ROOT.resolve()))
        scratch = Path(tempfile.mkdtemp(prefix='package-sections-', dir=parent))
        try:
            for index, root in enumerate(self.roots()):
                registry = read_owned_source(root / 'Engine/modules/package.registry.ixx', encoding='utf-8')
                forest = read_owned_source(root / 'Engine/modules/forest.factory.ixx', encoding='utf-8')
                forest_ids = []
                for name in ('kForestFactoryPackageId', 'kForestFactoryReferenceRepo'):
                    declaration = re.search(r'inline constexpr std::string_view ' + name + r'\s*=[^;]+;', forest)
                    self.assertIsNotNone(declaration)
                    forest_ids.append(declaration[0])
                namespace = registry[registry.index('export namespace epochengine::package_registry'):]
                namespace = namespace.replace('export namespace', 'namespace', 1)
                identities = 'namespace epochengine::forest {\n' + '\n'.join(forest_ids) + '\n}\n'
                registry_owns_identity = 'import forest.factory;' not in registry
                if root == ROOT:
                    self.assertTrue(registry_owns_identity)
                    self.assertIsNone(re.search(r'^import\s+', registry, re.MULTILINE),
                        'Registry must not require optional feature modules')
                    self.assertIn('import package.registry;', forest)
                declarations = (namespace + '\n' + identities if registry_owns_identity
                    else identities + namespace)
                source = scratch / f'registry-{index}.cpp'
                source.write_text('#include <array>\n#include <cstddef>\n#include <cstdint>\n'
                    '#include <span>\n#include <string_view>\n'
                    + declarations + '\n'
                    'static_assert(epochengine::package_registry::catalog_section_contract());\n'
                    'static_assert(epochengine::package_registry::validate_registry().ok);\n'
                    'static_assert(epochengine::forest::kForestFactoryPackageId == "engine_forest_factory");\n'
                    'static_assert(epochengine::forest::kForestFactoryPackageId == '
                    'epochengine::package_registry::kEngineForestFactoryPackageId);\n'
                    'static_assert(epochengine::forest::kForestFactoryReferenceRepo == '
                    'epochengine::package_registry::kForestFactoryReferenceRepo);\n', encoding='utf-8')
                if os.name == 'nt':
                    program = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))
                    toolsets = sorted((program / 'Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                    kits = sorted(Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')).glob(
                        'Windows Kits/10/Include/*'))
                    if not toolsets or not kits:
                        self.skipTest('MSVC headers not installed')
                    toolset, kit = toolsets[-1], kits[-1]
                    compiler = toolset / 'bin/Hostx64/x64/cl.exe'
                    args = [str(compiler), '/nologo', '/std:c++latest', '/Zs', '/TP',
                        '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'),
                        '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'), str(source)]
                else:
                    compiler = shutil.which('clang++') or shutil.which('g++')
                    if not compiler:
                        self.skipTest('C++23 compiler not installed')
                    args = [compiler, '-std=c++23', '-fsyntax-only', str(source)]
                result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, str(root) + '\n' + result.stdout + result.stderr)
                if root == ROOT and os.name == 'nt':
                    # Produce the real interface with no forest/morphology/voxel IFCs,
                    # then compile an importing consumer against only that IFC.
                    includes = [item for item in args if item.startswith('/I')]
                    ifc = scratch / 'package.registry.ifc'
                    result = subprocess.run([str(compiler), '/nologo', '/std:c++latest',
                        '/EHsc', '/c', '/interface', *includes,
                        '/ifcOutput' + str(ifc), '/Fo' + str(scratch / 'registry.obj'),
                        str(ROOT / 'Engine/modules/package.registry.ixx')],
                        cwd=scratch, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    consumer = scratch / 'consumer.cpp'
                    consumer.write_text('import package.registry;\n'
                        'static_assert(epochengine::package_registry::validate_registry().ok);\n'
                        'static_assert(epochengine::package_registry::kEngineForestFactoryPackageId '
                        '== "engine_forest_factory");\n', encoding='utf-8')
                    result = subprocess.run([str(compiler), '/nologo', '/std:c++latest',
                        '/EHsc', '/Zs', '/TP', *includes, '/reference',
                        'package.registry=' + str(ifc), str(consumer)],
                        cwd=scratch, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_lists_and_actions_use_the_same_registry_ownership(self):
        for root in self.roots():
            app = read_owned_source(root / 'Engine/src/editor/editor.application.cpp', encoding='utf-8')
            start = app.index('        if (editor.showPackageManagerModal)\n        {',
                app.index('Model Manager closed; passive scan may still finish.'))
            section = app[start:app.index('        if (editor.showAboutModal)', start)]
            for label in ('Project Packages', 'Project AI Resources', 'Model Manager'):
                self.assertIn('.label="' + label + '"', section)
            self.assertIn('CatalogSection::ProjectPackages', section)
            self.assertIn('CatalogSection::ProjectAiResources', section)
            install_guard = section.index('== package_registry::CatalogSection::ModelManager')
            self.assertLess(install_guard, section.index('stage_model_package_opt_in'))
            self.assertIn('!selectedPackage || !visible_resource(*selectedPackage)', section)
            self.assertIn('!selectedCatalog || !visible_in_catalog(*selectedCatalog)', section)
            self.assertIn('selectedPlan.has_value()', section)
            self.assertIn('Download and image generation are not available for this resource yet.', section)
            self.assertIn('Source archive installation is not connected in this build.', section)
            self.assertNotIn('Show Models Only', section)
            self.assertNotIn('Show All Packages', section)
            self.assertNotIn('Download/build gate staged', section)
            self.assertNotIn('epochLocalAi.ready() ? "installed"', section)

    def test_both_http_implementations_use_project_resource_endpoint(self):
        for root in self.roots():
            module = read_owned_source(root / 'Engine/modules/package.catalog.ixx', encoding='utf-8')
            source = read_owned_source(root / 'Engine/src/packages/package.catalog.cpp', encoding='utf-8')
            endpoint = '/api/epoch/project-resources/catalog'
            self.assertIn('https://epoch.adamrushford.chatgpt.site' + endpoint, module)
            self.assertIn('"' + endpoint + '"', module)
            self.assertIn('endpoint_path.begin(), endpoint_path.end()', source)
            self.assertIn('L"GET", requestPath.c_str()', source)
            self.assertIn('const std::string url{ endpoint };', source)
            self.assertNotIn('/api/epoch/packages/catalog', source)
            self.assertIn('WINHTTP_OPTION_REDIRECT_POLICY_NEVER', source)



if __name__ == '__main__':
    unittest.main()
