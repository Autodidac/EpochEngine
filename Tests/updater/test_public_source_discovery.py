"""Exercise the actual release routes against local ZIPs and test ports."""
from pathlib import Path
import json
import os
import re
import subprocess
import time
import unittest
from zipfile import ZipFile

from Tools.source.owned_source_text import definition

from Tests.support.cpp import dependency_include, msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]
VERSIONS = ('0.91.13', '0.91.14')


def write_route_fixtures(output):
    fixtures = output / 'release-fixtures'
    fixtures.mkdir()
    for version in VERSIONS:
        for edition in ('public', 'development'):
            edition_id = ('epoch.public.github.v1' if edition == 'public'
                          else 'epoch.development.site.v1')
            marker = json.dumps({'schema': 'epoch.distribution.v1', 'edition': edition_id,
                                 'version': version}).encode()
            runtime = fixtures / f'{edition}-runtime-{version}'
            runtime.mkdir()
            with ZipFile(runtime / 'runtime.zip', 'w') as archive:
                archive.writestr('epoch-distribution.json', marker)
                archive.writestr('EpochEditor.exe', b'inert test content, never executed')
            with ZipFile(runtime / 'runtime.zip') as archive:
                archive.extract('epoch-distribution.json', runtime / 'extracted')
            source = fixtures / f'{edition}-source-{version}.zip'
            with ZipFile(source, 'w') as archive:
                archive.writestr('Engine.sln', 'fixture')
                archive.writestr('Engine/vcpkg.json', '{}')
                archive.writestr('Engine/include/epoch.edition.hpp',
                                 f"#define EPOCH_SOURCE_EDITION_PUBLIC {int(edition == 'public')}\n")
                archive.writestr('Engine/modules/epoch.version.ixx',
                                 '#define EPOCH_VERSION_MAJOR_VALUE 0\n#define EPOCH_VERSION_MINOR_VALUE 91\n'
                                 f'#define EPOCH_VERSION_REVISION_VALUE {version.split(".")[-1]}\n')
                archive.writestr('Tools/build/get_epoch_version.ps1', "throw 'Archived version helper must never run'\n")
                if edition == 'public':
                    archive.writestr('source-projection.json', '{}')
            with ZipFile(source) as archive:
                archive.extractall(fixtures / source.stem)


class PublicSourceDiscovery(unittest.TestCase):
    def test_actual_signed_source_and_windows_admission(self):
        if os.name != 'nt':
            self.skipTest('Installed Windows MSVC CPU lane')
        try:
            json_include = dependency_include(ROOT)
        except FileNotFoundError as error:
            self.skipTest(str(error))
        toolset = msvc_2022_toolsets()[-1]
        kits = Path('C:/Program Files (x86)/Windows Kits/10')
        kit = windows_sdk_includes(kits)[-1]
        report = ROOT / 'Artifacts/Reports/public-source-contract-oct29' / str(time.time_ns())
        report.mkdir(parents=True)
        # The Codex Python launcher prepends PowerShell7 modules. The production
        # worker is Windows PowerShell5; bind that interpreter's own SDK modules.
        ps_environment = dict(os.environ)
        ps_environment['PSModulePath'] = str(Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/Modules')
        integrity = (ROOT / 'Engine/src/updater/implementation/updater.release_integrity.inl').read_text(encoding='utf-8')
        system = (ROOT / 'Engine/src/updater/updater.system.cpp').read_text(encoding='utf-8')
        version = (ROOT / 'Engine/modules/epoch.version.ixx').read_text(encoding='utf-8')
        formatter = definition(version, 'export [[nodiscard]] std::array<char, 64> format_version_components(')
        formatter = 'namespace epochengine {\n' + formatter.replace('export ', '', 1) + '\n}\n'
        worker = (ROOT / 'Engine/src/updater/implementation/updater.source_worker.inl').read_text(encoding='utf-8')
        windows = (ROOT / 'Engine/src/updater/updater.source_worker_win.cpp').read_text(encoding='utf-8')
        sha = (ROOT / 'Engine/modules/core/core.sha256.ixx').read_text(encoding='utf-8').replace('module;', '').replace('export module core.sha256;', '').replace('export namespace', 'namespace')
        distribution = (ROOT / 'Engine/include/updater.distribution.hpp').read_text(encoding='utf-8')
        distribution = distribution.replace('#pragma once', '').replace('#include "epoch.edition.hpp"', '')
        distribution += '\n' + distribution.replace('namespace epochengine::updater::distribution',
            'namespace epochengine::updater::development_distribution').replace('EPOCH_EDITION_PUBLIC != 0', 'false')
        distribution = '#define EPOCH_SOURCE_EDITION_PUBLIC 1\n#define EPOCH_EDITION_PUBLIC 1\n' + distribution
        config = (ROOT / 'Engine/modules/updater/updater.config.ixx').read_text(encoding='utf-8')
        policies = ''
        signatures = ('PROJECT_RELEASE_INTEGRITY_URL', 'PROJECT_SOURCE_VERSION_URL', 'PROJECT_SOURCE_URL', 'PROJECT_SOURCE_CHECKSUM_URL', 'PROJECT_SOURCE_CHANNEL_ALLOWED', 'PROJECT_ACTION_RUNS_API_URL')
        packaged_enabled = config.split('export inline constexpr bool PROJECT_PACKAGED_UPDATES_ENABLED =', 1)[1].split(';', 1)[0]
        fallback_start = config.index('#if defined(EPOCH_BINARY_ONLY_DISTRIBUTION)')
        project_download = config.index('    export inline constexpr bool SOURCE_PROJECT_DOWNLOAD_ENABLED', fallback_start)
        fallback = config[fallback_start:config.index(';', project_download) + 1].replace('export ', '')
        for name, public, binary_only in (('public_policy', 'true', 0), ('development_policy', 'false', 0),
                                          ('binary_only_policy', 'true', 1)):
            policies += ('namespace ' + name + ' { constexpr bool PUBLIC_DISTRIBUTION=' + public
                         + '; constexpr bool SITE_DISTRIBUTION=!PUBLIC_DISTRIBUTION;'
                         ' constexpr auto GITHUB_BASE="https://github.com/";'
                         ' constexpr auto GITHUB_API_BASE="https://api.github.com/repos/";'
                         ' constexpr auto EPOCH_SITE_BASE="https://epoch.adamrushford.chatgpt.site";\n')
            policies += f'#define EPOCH_BINARY_ONLY_DISTRIBUTION {binary_only}\n{fallback}\n#undef EPOCH_BINARY_ONLY_DISTRIBUTION\n'
            policies += 'constexpr bool PROJECT_PACKAGED_UPDATES_ENABLED =' + packaged_enabled + ';\n'
            policies += '\n'.join(definition(config, 'export inline ' + ('bool ' if item.endswith('ALLOWED') else 'std::string ') + item + '(').replace('export ', '') for item in signatures)
            policies += '\n}\n'
        fragments = {
            'PRODUCTION_SHA': sha,
            'PRODUCTION_DISTRIBUTION': distribution,
            'PRODUCTION_POLICIES': policies,
            'PRODUCTION_VERSION': formatter + '\n'.join(definition(system, '[[' + 'nodiscard]] inline ' + sig) for sig in (
                'std::string strip_utf8_bom(', 'std::string normalize_line_endings(',
                'std::string trim_ascii(', 'std::string lower_ascii(', 'int parse_int_or_zero(', 'std::string canonicalize_semver(',
                'std::string extract_version_string(', 'std::array<int, 3> parse_version_triplet(', 'int compare_versions(')),
            'PRODUCTION_PARSER': integrity[:integrity.index('        [[nodiscard]] inline std::optional<std::vector<unsigned char>>')],
            'PRODUCTION_ADMISSION': definition(integrity, '[[nodiscard]] inline std::optional<std::string> verified_release_integrity_payload('),
            'PRODUCTION_TYPES': integrity[integrity.index('        struct IntegrityAssetInfo'):integrity.index('        [[nodiscard]] inline std::optional<std::string> download_expected_sha256(')],
            'PRODUCTION_PACKAGE': '\n'.join((
                definition(system, '[[nodiscard]] inline bool starts_with_ascii('),
                definition(system, '[[nodiscard]] inline bool ends_with_ascii('),
                definition(system, 'struct VersionCheckResult') + ';',
                definition(system, '[[nodiscard]] inline bool prefer_source_release('),
                definition(integrity, '[[nodiscard]] inline ResolvedPackagedRelease resolve_packaged_release('),
                definition(system, '[[nodiscard]] inline bool markerless_runtime_allowed('),
                definition(system, '[[nodiscard]] inline bool runtime_payload_matches_edition('),
            )),
            'PRODUCTION_SOURCE': integrity[integrity.index('        struct ResolvedSourceArchive'):integrity.index('        [[nodiscard]] inline bool text_matches_ci_job(')],
            'PRODUCTION_HOSTED_BUILD': definition(system, 'struct BuildStatusResult') + ';\n' + integrity[
                integrity.index('        [[nodiscard]] inline bool text_matches_ci_job('):
                integrity.index('        [[nodiscard]] inline std::string capture_process_output(')],
            'PRODUCTION_DOWNLOAD': definition(system, '[[nodiscard]] bool download_source_archive_with_fallbacks('),
            'PRODUCTION_ROOT': definition(worker, '[[nodiscard]] inline std::filesystem::path source_snapshot_root('),
            'PRODUCTION_ROOT_ADMISSION': definition(worker, '[[nodiscard]] inline bool source_snapshot_matches('),
            'PRODUCTION_EDITION': definition(system, '[[nodiscard]] inline bool source_tree_matches_edition('),
            'PRODUCTION_WINDOWS': definition(windows, '[[nodiscard]] std::string_view source_snapshot_admission_script(')
                + definition(windows, '[[nodiscard]] std::string_view source_archive_digest_script('),
        }
        code = (ROOT / 'Tests/updater/updater.public_source_contract.cpp').read_text(encoding='utf-8')
        code = re.sub(r'\bpayload\b', 'fixture_payload', code)
        for token in sorted(fragments, key=len, reverse=True):
            code = code.replace(token, fragments[token])
        source = report / 'consumer.cpp'
        source.write_text(code, encoding='utf-8')
        outcomes = []
        for configuration in ('Debug', 'Release'):
            output = report / configuration
            output.mkdir()
            write_route_fixtures(output)
            temporary = output / 'tmp'
            temporary.mkdir()
            environment = dict(os.environ, TEMP=str(temporary), TMP=str(temporary))
            binary = output / 'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/W4', '/WX', '/Z7',
                    *(['/Od', '/MDd', '/D_DEBUG'] if configuration == 'Debug' else ['/O2', '/MD', '/DNDEBUG']),
                    '/I' + str(toolset / 'include'), '/I' + str(ROOT / 'Engine/include'), '/I' + str(json_include),
                    *('/I' + str(kit / name) for name in ('ucrt', 'shared', 'um')),
                    '/Fo' + str(output / 'consumer.obj'), '/Fe' + str(binary), str(source), '/link',
                    '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(kits / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(kits / 'Lib' / kit.name / 'ucrt/x64')]
            compiled = subprocess.run(args, cwd=output, env=environment, capture_output=True, text=True, timeout=90)
            (output / 'compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
            self.assertEqual(compiled.returncode, 0, str(output / 'compile.log') + '\n' + compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary), str(output)], cwd=output, env=environment,
                                      capture_output=True, text=True, timeout=25)
            (output / 'run.log').write_text(executed.stdout + executed.stderr, encoding='utf-8')
            self.assertEqual(executed.returncode, 0, f'{configuration} actual owner exit {executed.returncode}; {output}\n'
                             + executed.stdout + executed.stderr)
            # The compiled owner emits the exact script fragment used by the worker.
            script = output / 'admission.ps1'
            powershell = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                         str(ROOT / 'Tests/updater/updater.source_admission_contract.ps1'),
                                         '-Report', str(output), '-OwnerScript', str(script), '-Repo', str(ROOT),
                                         '-ChannelFixtures', str(output / 'release-fixtures')],
                                        cwd=output, env=environment | {'PSModulePath': ps_environment['PSModulePath']},
                                        capture_output=True, text=True, timeout=30)
            (output / 'powershell.log').write_text(powershell.stdout + powershell.stderr, encoding='utf-8')
            self.assertEqual(powershell.returncode, 0, powershell.stdout + powershell.stderr)
            outcomes.append({'configuration': configuration, 'compile': compiled.returncode, 'run': executed.returncode,
                             'windows_script_syntax_and_semantics': powershell.returncode})
        layout = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(ROOT / 'Tests/updater/package_layout_contract.ps1')],
                                cwd=ROOT, env=ps_environment, capture_output=True, text=True, timeout=20)
        (report / 'package-layout.log').write_text(layout.stdout + layout.stderr, encoding='utf-8')
        self.assertEqual(layout.returncode, 0, layout.stdout + layout.stderr)
        (report / 'result.json').write_text(json.dumps({'results': outcomes, 'package_layout': layout.returncode,
            'release_route_fixture_versions': VERSIONS,
            'crypto_transport_ports': True, 'real_sha256_and_filesystem': True, 'network_gui_models': False}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
