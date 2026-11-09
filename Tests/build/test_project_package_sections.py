"""Project package/resource ownership, real registry contracts and UI wiring.

Compiles the actual registry namespace and Forest Factory identity aliases,
without starting the engine, a model or a download.
"""
from Tools.source.owned_source_text import read_owned_source
from Tests.build.test_opengl_sprite_stream import definition
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class ProjectPackageSections(unittest.TestCase):
    def roots(self):
        result = [ROOT]
        public = ROOT / 'build/publication/EpochEngine'
        if (public / 'Engine/modules/package.registry.ixx').exists():
            result.append(public)
        return result

    def test_actual_catalog_labels_preserve_payload_and_unknown_identities(self):
        registry = read_owned_source(ROOT / 'Engine/modules/package.registry.ixx', encoding='utf-8')
        namespace = registry[registry.index('export namespace epochengine::package_registry'):]
        namespace = namespace.replace('export namespace', 'namespace', 1)
        catalog = read_owned_source(ROOT / 'Engine/modules/package.catalog.ixx', encoding='utf-8')
        catalog = catalog[catalog.index('export namespace epochengine::package_catalog'):]
        catalog = catalog.replace('export namespace', 'namespace', 1)
        writer = read_owned_source(ROOT / 'Engine/src/editor/application/editor.application_packages_creation.inl', encoding='utf-8')
        actual = definition(writer, '            for (auto& entry : snapshot.entries)')
        source = r'''
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
REGISTRY
CATALOG
namespace package_registry = epochengine::package_registry;
namespace package_catalog = epochengine::package_catalog;
bool same_payload(const package_catalog::Entry& a,const package_catalog::Entry& b) {
 return a.id==b.id && a.kind==b.kind && a.activation==b.activation && a.scope==b.scope
  && a.availability==b.availability && a.source_url==b.source_url
  && a.immutable_revision==b.immutable_revision && a.sha256==b.sha256
  && a.size_bytes==b.size_bytes && a.license_url==b.license_url && a.platforms==b.platforms
  && a.identity_required==b.identity_required && a.automatic_execution==b.automatic_execution
  && a.local_admission_required==b.local_admission_required && a.signed_payload==b.signed_payload
  && a.content_addressed==b.content_addressed;
}
int main() {
 package_catalog::Snapshot snapshot;
 package_catalog::Entry known;
 known.id="research_planetary_terrain";known.display_name="Old experimental title";
 known.summary="Old prototype purpose";known.kind="source";known.activation="review";
 known.availability=package_catalog::Availability::descriptor_only;
 known.source_url="https://example.invalid/immutable-source.zip";known.immutable_revision="exact-revision";
 known.sha256=std::string(64,'a');known.size_bytes=8192;
 known.license_url="https://example.invalid/license";known.platforms={"windows","linux"};
 known.identity_required=true;known.local_admission_required=true;known.signed_payload=true;
 auto unknown=known;unknown.id="third_party_terrain";unknown.display_name="Third Party Terrain";
 unknown.summary="Unchanged external description";
 auto manifestId=known;manifestId.id="planetary_terrain";manifestId.display_name="Separate manifest-ID row";
 auto conflicting=known;conflicting.source_url="https://example.invalid/another-source.zip";
 snapshot.entries={known,unknown,manifestId,conflicting};
 const auto before=snapshot.entries;
 ACTUAL
 if(snapshot.entries.size()!=before.size())return 1;
 for(std::size_t i=0;i<before.size();++i)if(!same_payload(snapshot.entries[i],before[i]))return 2;
 for(const auto i:{0u,3u}) {
  if(snapshot.entries[i].display_name!="Planetary Terrain")return 3;
  if(snapshot.entries[i].summary!=package_registry::find("research_planetary_terrain")->summary)return 4;
 }
 for(const auto i:{1u,2u})if(snapshot.entries[i].display_name!=before[i].display_name
  ||snapshot.entries[i].summary!=before[i].summary)return 5;
 const auto* mapping=package_registry::source_manifest_binding("planetary_terrain");
 if(!mapping ||mapping->registry_id!="research_planetary_terrain")return 6;
 constexpr std::array pairs{
  std::pair{"research_voxel_planetoid","Voxel Planetoid LOD"},
  std::pair{"research_voxel_authoring","Voxel Authoring Tools"},
  std::pair{"research_forest_temporal_graph","Forest Graph Source"},
  std::pair{"research_fft_ocean","Water (FFT Ocean)"}};
 for(const auto& pair:pairs) {
  const auto* entry=package_registry::find(pair.first);
  if(!entry ||entry->displayName!=pair.second)return 7;
  if(entry->summary.find("not connected yet")==std::string_view::npos
   &&entry->summary.find("no connected project controls yet")==std::string_view::npos)return 8;
 }
 return 0;
}
'''.replace('REGISTRY', namespace).replace('CATALOG', catalog).replace('ACTUAL', actual)
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='package-labels-', dir=parent))
        passed = False
        try:
            (scratch / 'labels.cpp').write_text(source, encoding='utf-8')
            (scratch / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.28)\n'
                'project(package_labels LANGUAGES CXX)\n'
                'add_executable(package_labels labels.cpp)\n'
                'target_compile_features(package_labels PRIVATE cxx_std_23)\n'
                'enable_testing()\nadd_test(NAME package_labels COMMAND package_labels)\n', encoding='utf-8')
            command = ['cmake', '-S', str(scratch), '-B', str(scratch / 'cmake')]
            if os.name == 'nt':
                command += ['-G', 'Visual Studio 17 2022', '-A', 'x64']
            else:
                command += ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release']
            commands = [('configure', command)]
            for config in ('Debug', 'Release') if os.name == 'nt' else ('Release',):
                build = ['cmake', '--build', str(scratch / 'cmake'), '--config', config, '--parallel', '1']
                if os.name == 'nt':
                    build += ['--', '/nr:false', '/p:UseMultiToolTask=false', '/p:CL_MPCount=1']
                commands.extend([(config + '-build', build), (config + '-contract',
                    ['ctest', '--test-dir', str(scratch / 'cmake'), '-C', config, '--output-on-failure'])])
            for label, command in commands:
                env = dict(os.environ, MSBUILDDISABLENODEREUSE='1')
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120, env=env)
                (scratch / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
                self.assertEqual(result.returncode, 0, str(scratch) + '\n' + (result.stdout + result.stderr)[-12000:])
            passed = True
        finally:
            if passed:
                shutil.rmtree(scratch)

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
                    toolsets = msvc_2022_toolsets()
                    kitroot = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Windows Kits/10'
                    kits = windows_sdk_includes(kitroot)
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
            if 'Registered engine models' in section:
                for token in ('native::snapshot()', 'model.label.empty() ? model.id : model.label',
                        'Enable Project AI', 'Disable Project AI', 'ProjectAiProviderMode::engine_selected',
                        'write_project_ai_profile(editor', 'open_editor_surface(EditorMainSurface::Project',
                        'visible_local_package', 'open_package_source_review(editor'):
                    self.assertTrue(token in section, token)
                ai_start = section.index('                namespace native = epochengine::ai::native_runtime;')
                ai_end = section.index('            else\n            {\n            gui::wrapped_label(projectAiResources', ai_start)
                resource_view = section[ai_start:ai_end]
                for forbidden in ('native::load(', 'native::prepare(', 'native::install(', 'start_model_package_install('):
                    self.assertFalse(forbidden in resource_view, forbidden)
                self.assertTrue('Weights are not copied; image support is not implied.' in resource_view)
            else:
                # The retained, physically separate Public checkpoint still has
                # the prior resource UI until its next explicit projection.
                self.assertIn('!selectedPackage || !visible_resource(*selectedPackage)', section)
                self.assertIn('!selectedCatalog || !visible_in_catalog(*selectedCatalog)', section)
                self.assertIn('selectedPlan.has_value()', section)
                self.assertIn('Download and image generation are not available for this resource yet.', section)
                self.assertIn('Source staging is available offline; building and project activation are separate.', section)
                self.assertIn('open_package_source_review(editor, std::string(package.id))', section)
                self.assertIn('sourceInstallUnavailable ? "Review Local Source..."', section)
            self.assertNotIn('Source archive installation is not connected in this build.', section)
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
