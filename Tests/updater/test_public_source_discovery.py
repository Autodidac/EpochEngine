"""Actual signed source resolver/download/root owners; transport and crypto are ports."""
from pathlib import Path
import importlib.util
import json
import os
import re
import subprocess
import time
import unittest

from Tools.source.owned_source_text import definition

from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class PublicSourceDiscovery(unittest.TestCase):
    def test_actual_signed_source_and_windows_admission(self):
        if os.name != 'nt':
            self.skipTest('Installed Windows MSVC CPU lane')
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
        sha = (ROOT / 'Engine/modules/core.sha256.ixx').read_text(encoding='utf-8').replace('module;', '').replace('export module core.sha256;', '').replace('export namespace', 'namespace')
        distribution = (ROOT / 'Engine/include/updater.distribution.hpp').read_text(encoding='utf-8').replace('#pragma once', '').replace('#include "epoch.edition.hpp"', '#define EPOCH_SOURCE_EDITION_PUBLIC 1\n#define EPOCH_EDITION_PUBLIC 1')
        config = (ROOT / 'Engine/modules/updater.config.ixx').read_text(encoding='utf-8')
        policies = ''
        signatures = ('PROJECT_RELEASE_INTEGRITY_URL', 'PROJECT_SOURCE_URL', 'PROJECT_SOURCE_CHECKSUM_URL', 'PROJECT_SOURCE_VERSION_URL', 'PROJECT_SOURCE_CHANNEL_ALLOWED')
        for name, public in (('public_policy', 'true'), ('development_policy', 'false')):
            policies += 'namespace ' + name + ' { constexpr bool PUBLIC_DISTRIBUTION=' + public + '; constexpr bool SITE_DISTRIBUTION=!PUBLIC_DISTRIBUTION; constexpr bool SOURCE_UPDATE_FALLBACK_ENABLED=true; constexpr bool AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED=SITE_DISTRIBUTION; constexpr auto GITHUB_BASE="https://github.com/"; constexpr auto EPOCH_SITE_BASE="https://epoch.adamrushford.chatgpt.site";\n'
            policies += '\n'.join(definition(config, 'export inline ' + ('bool ' if item.endswith('ALLOWED') else 'std::string ') + item + '(').replace('export ', '') for item in signatures)
            policies += '\n}\n'
        fragments = {
            'PRODUCTION_SHA': sha,
            'PRODUCTION_DISTRIBUTION': distribution,
            'PRODUCTION_POLICIES': policies,
            'PRODUCTION_VERSION': formatter + '\n'.join(definition(system, '[[' + 'nodiscard]] inline ' + sig) for sig in (
                'std::string strip_utf8_bom(', 'std::string normalize_line_endings(',
                'std::string trim_ascii(', 'int parse_int_or_zero(', 'std::string canonicalize_semver(',
                'std::string extract_version_string(', 'std::array<int, 3> parse_version_triplet(', 'int compare_versions(')),
            'PRODUCTION_PARSER': integrity[:integrity.index('        [[nodiscard]] inline std::optional<std::vector<unsigned char>>')],
            'PRODUCTION_ADMISSION': definition(integrity, '[[nodiscard]] inline std::optional<std::string> verified_release_integrity_payload('),
            'PRODUCTION_TYPES': integrity[integrity.index('        struct IntegrityAssetInfo'):integrity.index('        [[nodiscard]] inline std::optional<std::string> download_expected_sha256(')],
            'PRODUCTION_SOURCE': integrity[integrity.index('        struct ResolvedSourceArchive'):integrity.index('        [[nodiscard]] inline bool text_matches_ci_job(')],
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
            binary = output / 'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/W4', '/WX', '/Z7',
                    *(['/Od', '/MDd', '/D_DEBUG'] if configuration == 'Debug' else ['/O2', '/MD', '/DNDEBUG']),
                    '/I' + str(toolset / 'include'), *('/I' + str(kit / name) for name in ('ucrt', 'shared', 'um')),
                    '/Fo' + str(output / 'consumer.obj'), '/Fe' + str(binary), str(source), '/link',
                    '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(kits / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(kits / 'Lib' / kit.name / 'ucrt/x64')]
            compiled = subprocess.run(args, cwd=output, capture_output=True, text=True, timeout=90)
            (output / 'compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
            self.assertEqual(compiled.returncode, 0, str(output / 'compile.log') + '\n' + compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary), str(output)], cwd=output, capture_output=True, text=True, timeout=25)
            (output / 'run.log').write_text(executed.stdout + executed.stderr, encoding='utf-8')
            self.assertEqual(executed.returncode, 0, f'{configuration} actual owner exit {executed.returncode}; {output}')
            # The compiled owner emits the exact script fragment used by the worker.
            script = output / 'admission.ps1'
            powershell = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                         str(ROOT / 'Tests/updater/updater.source_admission_contract.ps1'),
                                         '-Report', str(output), '-OwnerScript', str(script), '-Repo', str(ROOT)],
                                        cwd=output, env=ps_environment, capture_output=True, text=True, timeout=30)
            (output / 'powershell.log').write_text(powershell.stdout + powershell.stderr, encoding='utf-8')
            self.assertEqual(powershell.returncode, 0, powershell.stdout + powershell.stderr)
            outcomes.append({'configuration': configuration, 'compile': compiled.returncode, 'run': executed.returncode,
                             'windows_script_syntax_and_semantics': powershell.returncode})
        layout = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(ROOT / 'Tests/updater/package_layout_contract.ps1')],
                                cwd=ROOT, env=ps_environment, capture_output=True, text=True, timeout=20)
        (report / 'package-layout.log').write_text(layout.stdout + layout.stderr, encoding='utf-8')
        self.assertEqual(layout.returncode, 0, layout.stdout + layout.stderr)
        (report / 'result.json').write_text(json.dumps({'results': outcomes, 'package_layout': layout.returncode,
            'crypto_transport_ports': True, 'real_sha256_and_filesystem': True, 'network_gui_models': False}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
