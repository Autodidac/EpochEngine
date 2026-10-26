"""Actual output-graph evaluation and artifact admission, no GUI/model execution."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class OutputLayout(unittest.TestCase):
    def setUp(self):
        self.parent = (ROOT / 'build/contract-tests').resolve()
        self.parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(self.parent.is_relative_to(ROOT.resolve()))
        self.scratch = Path(tempfile.mkdtemp(prefix='output-layout-', dir=self.parent))

    def tearDown(self):
        self.assertTrue(self.scratch.resolve().is_relative_to(self.parent))
        shutil.rmtree(self.scratch)

    def edition_fixture(self, name, value):
        root = self.scratch / name
        for relative in ('Tools/MSVC/EpochOutputLayout.props',
                         'Engine/include/epoch.build_layout.hpp', 'Engine/include/epoch.edition.hpp'):
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            source = read_owned_source(ROOT / relative, encoding='utf-8')
            if relative.endswith('epoch.edition.hpp'):
                source = source.replace('#define EPOCH_SOURCE_EDITION_PUBLIC 0',
                                        '#define EPOCH_SOURCE_EDITION_PUBLIC ' + str(value))
            target.write_text(source, encoding='utf-8')
        return root

    def test_actual_msbuild_paths_every_project_and_configuration(self):
        msbuild = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe')
        if not msbuild.exists():
            self.skipTest('MSBuild unavailable')
        projects = [ROOT / 'Engine/examples' / name / (name + '.vcxproj')
                    for name in ('EpochEngine', 'EpochEditor', 'HeadlessCI', 'AiTransportContract')]
        projects.extend(ROOT / 'Engine/dep' / name / (name + '.vcxproj')
                        for name in ('EpochGui', 'EpochPlatformEngine'))
        edition = 'public' if re.search(r'^#define EPOCH_SOURCE_EDITION_PUBLIC 1$',
            read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp'), re.M) else 'development'
        for project in projects:
            configs = ('Debug', 'Release') if project.stem in ('EpochGui', 'AiTransportContract') else ('Debug', 'Release', 'ReleaseWithDebugInfo')
            for configuration in configs:
                result = subprocess.run([str(msbuild), str(project), '/nologo',
                    '/p:Platform=x64', '/p:Configuration=' + configuration,
                    '-getProperty:OutDir,IntDir,EpochOutputEdition'], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-2000:] + result.stderr)
                values = json.loads(result.stdout)['Properties']
                self.assertEqual(Path(values['OutDir']), ROOT / 'build/bin/windows' / edition / 'x64' / configuration)
                self.assertEqual(Path(values['IntDir']), ROOT / 'build/obj/msvc' / edition / 'x64' / configuration / project.stem)
            isolated = self.scratch / 'isolated-products'
            result = subprocess.run([str(msbuild), str(project), '/nologo',
                '/p:Platform=x64', '/p:Configuration=Debug', '/p:EpochBuildRoot=' + str(isolated),
                '-getProperty:OutDir,IntDir'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            values = json.loads(result.stdout)['Properties']
            self.assertEqual(Path(values['OutDir']), isolated / 'bin/windows' / edition / 'x64/Debug')
            self.assertEqual(Path(values['IntDir']), isolated / 'obj/msvc' / edition / 'x64/Debug' / project.stem)
        # Evaluate the SAME production props against both physical edition markers.
        for edition, value in (('development', 0), ('public', 1)):
            root = self.edition_fixture(edition, value)
            project = root / 'output.proj'
            project.write_text('<Project><PropertyGroup><Platform>x64</Platform>'
                '<Configuration>Release</Configuration><ProjectName>Actual</ProjectName>'
                '<ConfigurationType>Application</ConfigurationType></PropertyGroup>'
                '<Import Project="Tools/MSVC/EpochOutputLayout.props" /></Project>', encoding='utf-8')
            result = subprocess.run([str(msbuild), str(project), '/nologo',
                '-getProperty:OutDir,IntDir,EpochOutputEdition'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-2000:] + result.stderr)
            values = json.loads(result.stdout)['Properties']
            self.assertEqual(values['EpochOutputEdition'], edition)
            self.assertEqual(Path(values['OutDir']), root / 'build/bin/windows' / edition / 'x64/Release')
            # Run the actual asset target, not a modeled XML transformation.
            assets = ('fonts/Roboto-Regular.ttf', 'vulkan/shaders/vert.spv')
            for asset in assets:
                target = root / 'Engine/assets' / asset
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / 'Engine/assets' / asset, target)
            result = subprocess.run([str(msbuild), str(project), '/nologo',
                '/t:EpochStageSharedRuntimeAssets', '/verbosity:quiet'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-2000:] + result.stderr)
            for asset in assets:
                self.assertEqual((Path(values['OutDir']) / 'assets' / asset).read_bytes(),
                                 (ROOT / 'Engine/assets' / asset).read_bytes())

    def test_actual_evaluated_link_metadata_and_library_targets(self):
        msbuild = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe')
        if not msbuild.exists():
            self.skipTest('MSBuild unavailable')
        inspector = self.scratch / 'inspect.targets'
        inspector.write_text('<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">'
            '<Target Name="EpochInspectEvaluatedLink"><ItemGroup>'
            '<Link Include="__epoch_link_metadata_probe__" />'
            '<Lib Include="__epoch_lib_metadata_probe__" />'
            '</ItemGroup></Target></Project>', encoding='utf-8')
        edition = 'public' if re.search(r'^#define EPOCH_SOURCE_EDITION_PUBLIC 1$',
            read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp'), re.M) else 'development'
        names = ('EpochEngine', 'EpochEditor', 'HeadlessCI', 'AiTransportContract', 'EpochGui', 'EpochPlatformEngine')
        for name in names:
            project = ROOT / ('Engine/dep/' + name + '/' + name + '.vcxproj' if name in ('EpochGui', 'EpochPlatformEngine')
                              else 'Engine/examples/' + name + '/' + name + '.vcxproj')
            configurations = ('Debug', 'Release') if name in ('EpochGui', 'AiTransportContract') else ('Debug', 'Release', 'ReleaseWithDebugInfo')
            for configuration in configurations:
                result = subprocess.run([str(msbuild), str(project), '/nologo', '/verbosity:quiet',
                    '/p:Platform=x64', '/p:Configuration=' + configuration,
                    '/p:ForceImportAfterCppTargets=' + str(inspector), '/t:EpochInspectEvaluatedLink',
                    '-getProperty:OutDir,IntDir,EpochVcpkgInstallRoot',
                    '-getItem:Link,Lib,ProjectReference'], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-3000:] + result.stderr)
                evaluated = json.loads(result.stdout)
                expected = ROOT / 'build/bin/windows' / edition / 'x64' / configuration
                legacy = str(ROOT / 'x64' / configuration).casefold()
                for item in evaluated['Items']['Link'] + evaluated['Items']['Lib']:
                    directories = [value.rstrip('\\/').casefold() for value in
                        item.get('AdditionalLibraryDirectories', '').split(';') if value]
                    self.assertNotIn(legacy, directories, name + ':' + configuration)
                    if name in ('EpochEditor', 'AiTransportContract') and item['Identity'] == '__epoch_link_metadata_probe__':
                        self.assertTrue(directories, name + ':' + configuration)
                        self.assertEqual(directories[0], str(expected).casefold())
                if name == 'EpochEditor':
                    item = evaluated['Items']['Link'][0]
                    dependencies = item.get('AdditionalDependencies', '').split(';')
                    engine_library = str(expected / 'EpochEngine.lib')
                    gui_library = str(expected / 'EpochGui.lib')
                    self.assertEqual(dependencies.count(engine_library), 1)
                    self.assertEqual(dependencies.count(gui_library), 1)
                    self.assertNotIn('EpochEngine.lib', dependencies)
                    self.assertNotIn('EpochGui.lib', dependencies)
                    # Preserve the established library order. Moving these
                    # after inherited GLAD/raylib dependencies changes archive
                    # resolution and causes a real gladLoadGL duplicate.
                    engine_index = dependencies.index(engine_library)
                    self.assertEqual(dependencies[engine_index + 1], gui_library)
                    self.assertLess(engine_index, dependencies.index('glad.lib'))
                    self.assertLess(dependencies.index(gui_library), dependencies.index('opengl32.lib'))
                    self.assertEqual(dependencies.count('glad.lib'), 1)
                    # References still build both libraries, but must not
                    # append another copy at a different linker position.
                    references = evaluated['Items']['ProjectReference']
                    by_name = {Path(ref['Identity']).stem: ref for ref in references}
                    expected_references = {'EpochEngine', 'EpochGui', 'EpochPlatformEngine',
                                           'EpochAudioEngine', 'EpochMediaEngine'}
                    if edition == 'development':
                        expected_references.add('EpochSelfCoding')
                    self.assertEqual(set(by_name), expected_references)
                    for ordered in ('EpochEngine', 'EpochGui'):
                        self.assertEqual(by_name[ordered].get('LinkLibraryDependencies'), 'false')
                    # Platform is independently linked through the real project reference.
                    self.assertNotEqual(by_name['EpochPlatformEngine'].get('LinkLibraryDependencies'), 'false')
                    third_party = str(Path(evaluated['Properties']['EpochVcpkgInstallRoot']) /
                        ('debug/lib' if configuration == 'Debug' else 'lib')).casefold()
                    link_directories = [value.rstrip('\\/').casefold() for value in
                        item.get('AdditionalLibraryDirectories', '').split(';') if value]
                    self.assertIn(third_party, link_directories)
                if name in ('EpochEngine', 'EpochGui', 'EpochPlatformEngine'):
                    result = subprocess.run([str(msbuild), str(project), '/nologo', '/verbosity:quiet',
                        '/p:Platform=x64', '/p:Configuration=' + configuration,
                        '/t:GetTargetPath', '-getTargetResult:GetTargetPath'], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout[-2000:] + result.stderr)
                    targets = json.loads(result.stdout)['TargetResults']['GetTargetPath']['Items']
                    self.assertEqual(Path(targets[0]['Identity']), expected / (name + '.lib'))

    def test_compiled_path_policy_and_real_ledger_reject_stale_layout(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        module = read_owned_source(ROOT / 'Engine/dep/EpochSelfCoding/integration/modules/editor.candidate_artifact.ixx', encoding='utf-8')
        body = module[module.index('export namespace epochengine::editor::candidate_artifact'):].replace('export namespace', 'namespace', 1)
        identity = '''namespace epochengine::platform::child_process {
struct ExecutableIdentity { unsigned long long size_bytes{12}; std::string sha256{"abc"};
bool valid() const { return size_bytes && !sha256.empty(); }
friend bool operator==(const ExecutableIdentity&, const ExecutableIdentity&) = default; }; }
'''
        path_source = read_owned_source(ROOT / 'Engine/src/platform/core.path.cpp', encoding='utf-8')
        path_body = path_source[path_source.index('    path log_output_dir()'):path_source.index('    path capture_output_dir()')]
        path_policy = '''namespace path_policy {
using path = std::filesystem::path;
path candidate, override_path, executable, runtime;
path candidate_data_root() { return candidate; }
path env_override_path(const char*) { return override_path; }
path executable_dir() { return executable; }
path runtime_root_dir() { return runtime; }
path normalize(const path& value) { return value.lexically_normal(); }
''' + path_body + '}\n'
        main = r'''
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    namespace fs = std::filesystem;
    namespace b = epochengine::build_layout;
    namespace a = epochengine::editor::candidate_artifact;
    const fs::path root = fs::absolute(argv[1]);
    const std::string edition = argv[2];
    path_policy::executable = root / "build/bin/windows" / edition / "x64/Release";
    path_policy::runtime = root;
    if (path_policy::log_output_dir() != path_policy::executable / "logs") return 20;
    path_policy::override_path = root / "operator-logs";
    if (path_policy::log_output_dir() != path_policy::override_path) return 21;
    path_policy::candidate = root / "private-candidate-data";
    if (path_policy::log_output_dir() != path_policy::candidate / "logs") return 22;
    path_policy::candidate.clear(); path_policy::override_path.clear();
    path_policy::executable.clear();
    if (path_policy::log_output_dir() != root / "logs") return 23;
    path_policy::runtime.clear();
    if (!path_policy::log_output_dir().empty()) return 24;
    if (b::edition != edition) return 2;
    fs::create_directories(root);
    if (b::source_layout(root) != b::Layout::Legacy) return 3;
    if (a::relative_executable(a::Lane::DebugEditor, root) != "x64/Debug/EpochEditor.exe") return 4;
    const auto marker = root / "Tools/MSVC/EpochOutputLayout.props";
    fs::create_directories(marker.parent_path());
    std::ofstream{marker} << "<!-- epoch.build-output-layout/v1 -->";
    // A stale legacy binary exists, but is NOT considered a new-graph product.
    fs::create_directories(root / "x64/Debug");
    std::ofstream{root / "x64/Debug/EpochEditor.exe"} << "stale";
    const auto expected = "build/bin/windows/" + edition + "/x64/Debug/EpochEditor.exe";
    if (a::relative_executable(a::Lane::DebugEditor, root) != expected) return 5;
    a::Ledger ledger;
    if (!ledger.reset(root.generic_string(), 8, true, false)) return 6;
    a::Binding binding{root.generic_string(), 8, a::Lane::DebugEditor,
        (root / "x64/Debug/EpochEditor.exe").generic_string(), {}, ledger.begin_build(a::Lane::DebugEditor)};
    if (ledger.accept_build(binding)) return 7;
    binding.executable_path = (root / expected).generic_string();
    if (!ledger.accept_build(binding)) return 8;
    if (b::linux_cmake(root, "Clang-Release") != root / "build/cmake/linux" / edition / "Clang-Release") return 9;
    std::ofstream{marker} << "unsupported layout";
    if (b::source_layout(root) != b::Layout::Invalid) return 10;
    if (!a::relative_executable(a::Lane::DebugEditor, root).empty()) return 11;
    if (a::valid_binding(binding)) return 12;
    return 0;
}
'''
        toolset, kit = toolsets[-1], kits[-1]
        for edition, value in (('development', 0), ('public', 1)):
            root = self.edition_fixture(edition, value)
            cpp = root / 'contract.cpp'
            cpp.write_text('#include "epoch.build_layout.hpp"\n#include <limits>\n#include <optional>\n'
                '#include <cstdint>\n' + identity + body + path_policy + main, encoding='utf-8')
            binary = root / 'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc',
                '/I' + str(root / 'Engine/include'), '/I' + str(toolset / 'include'),
                '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                '/Fo' + str(root / 'contract.obj'), '/Fe' + str(binary), str(cpp), '/link',
                '/LIBPATH:' + str(toolset / 'lib/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
            result = subprocess.run(args, cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-3000:] + result.stderr)
            result = subprocess.run([str(binary), str(root / 'workspace'), edition], cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, 'Artifact layout case ' + str(result.returncode))
            if value == 0:
                # Actual ZIP/timeline helper contract refuses an existing test
                # fixture and never resets active or retained evidence.
                diagnostic_binary = root / 'diagnostic.exe'
                diagnostic_args = [str(ROOT / 'Tests/ai/ai_engine_diagnostics_contract.cpp')
                    if arg == str(cpp) else '/Fe' + str(diagnostic_binary)
                    if arg.startswith('/Fe') else '/Fo' + str(root / 'diagnostic.obj')
                    if arg.startswith('/Fo') else arg for arg in args]
                result = subprocess.run(diagnostic_args, cwd=root, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-3000:] + result.stderr)
                result = subprocess.run([str(diagnostic_binary), str(self.scratch / 'diagnostics')],
                    cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, 'Diagnostic layout case ' + str(result.returncode))

    def test_all_configuration_producers_and_consumers_share_paths(self):
        root_presets = json.loads(read_owned_source(ROOT / 'CMakePresets.json'))
        self.assertEqual(root_presets['configurePresets'][0]['binaryDir'], '${sourceDir}/build/cmake/${presetName}')
        engine_presets = json.loads(read_owned_source(ROOT / 'Engine/CMakePresets.json'))
        for preset in engine_presets['configurePresets']:
            if 'binaryDir' in preset:
                self.assertEqual(preset['binaryDir'], '${sourceDir}/../build/cmake/${presetName}')
        for relative in ('Engine/build.sh', 'Engine/run.sh', 'Engine/install.sh'):
            source = read_owned_source(ROOT / relative)
            self.assertIn('build/cmake/linux/${EPOCH_OUTPUT_EDITION}', source)
            self.assertNotIn('${SCRIPT_DIR}/Bin', source)
        updater = read_owned_source(ROOT / 'Engine/src/updater/updater.system.cpp', encoding='utf-8')
        self.assertIn('build_layout::windows_bin(source_root, SOURCE_BUILD_CONFIGURATION(), SOURCE_BUILD_PLATFORM())', updater)
        self.assertIn('build_layout::linux_cmake(source_root, "Clang-Release")', updater)
        app = read_owned_source(ROOT / 'Engine/src/editor/editor.application.cpp', encoding='utf-8')
        self.assertNotIn('workspace / "x64" / configuration', app)


if __name__ == '__main__':
    unittest.main()
