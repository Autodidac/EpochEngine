"""Execute the complete production self-test in both physically marked editions."""
from pathlib import Path
import json
import os
import re
import subprocess
import time
import unittest

from Tools.source.owned_source_text import definition

from Tests.support.cpp import msvc_2022_toolsets

ROOT = Path(__file__).resolve().parents[2]


class DiscoverySelfTest(unittest.TestCase):
    def test_complete_owner_both_physical_editions(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC/Windows SDK CPU lane')
        toolset = msvc_2022_toolsets()[-1]
        kits = Path('C:/Program Files (x86)/Windows Kits/10')
        kit = sorted((kits / 'Include').glob('*'))[-1]
        report = ROOT / 'Artifacts/Reports/public-discovery-selftest-oct29' / str(time.time_ns())
        report.mkdir(parents=True)
        system = (ROOT / 'Engine/src/updater/updater.system.cpp').read_text(encoding='utf-8')
        config = (ROOT / 'Engine/modules/updater.config.ixx').read_text(encoding='utf-8')
        integrity = (ROOT / 'Engine/src/updater/implementation/updater.release_integrity.inl').read_text(encoding='utf-8')
        version = (ROOT / 'Engine/modules/epoch.version.ixx').read_text(encoding='utf-8')
        version = version[:version.index('    export std::string GetRuntimePlatformDisplayString(')] + '}\n'
        version = version.replace('module;', '').replace('export module epoch.version;', '').replace('export ', '')
        version = version.replace('import platform.engine;', '')
        version = version.replace('#include "../include/epoch.edition.hpp"', '#include "epoch.edition.hpp"')
        declaration_names = ('SITE_DISTRIBUTION', 'PUBLIC_DISTRIBUTION', 'GITHUB_BASE', 'GITHUB_RAW_BASE', 'GITHUB_API_BASE', 'EPOCH_SITE_BASE')
        declarations = ''
        for name in declaration_names:
            start = config.index('export inline constexpr ' + ('bool ' if name.endswith('DISTRIBUTION') else 'std::string_view ') + name)
            declarations += config[start:config.index(';', start) + 1].replace('export ', '') + '\n'
        start = config.index('#if defined(EPOCH_BINARY_ONLY_DISTRIBUTION)')
        finish = config.index('export inline constexpr std::string_view OWNER')
        declarations += config[start:finish].replace('export ', '')
        start = config.index('export inline const std::string PROJECT_VERSION')
        finish = config.index('export inline constexpr std::string_view WINDOWS_RUNTIME_BINARY_PREFIX')
        declarations += config[start:finish].replace('export ', '')
        functions = ''
        for result, name in (
            ('bool', 'PROJECT_RELEASE_DOWNLOAD_ALLOWED'), ('bool', 'PROJECT_SOURCE_EDITION_MATCHES'),
            ('std::string', 'SOURCE_VERSION_FILE_NAME'), ('std::string', 'PROJECT_RELEASE_INTEGRITY_URL'),
            ('std::string', 'PROJECT_SOURCE_VERSION_URL'), ('std::string', 'PROJECT_SOURCE_ARCHIVE_EXTENSION'),
            ('std::string', 'PROJECT_SOURCE_URL'), ('std::string', 'PROJECT_SOURCE_CHECKSUM_URL'),
            ('std::vector<std::string>', 'PROJECT_SOURCE_FALLBACK_URLS'), ('bool', 'PROJECT_SOURCE_CHANNEL_ALLOWED'),
            ('std::string', 'PROJECT_ACTION_RUNS_API_URL'), ('std::string', 'PROJECT_RELEASE_DOWNLOAD_PREFIX')):
            functions += definition(config, 'export inline ' + result + ' ' + name + '(').replace('export ', '') + '\n'
        helpers = ''
        for result, name in (
            ('std::string', 'strip_utf8_bom'), ('std::string', 'normalize_line_endings'), ('std::string', 'trim_ascii'),
            ('int', 'parse_int_or_zero'), ('std::string', 'canonicalize_semver'), ('std::string', 'extract_version_string'),
            ('std::array<int, 3>', 'parse_version_triplet'), ('int', 'compare_versions')):
            helpers += definition(system, '[[nodiscard]] inline ' + result + ' ' + name + '(') + '\n'
        helpers += definition(system, 'struct VersionCheckResult') + ';\n'
        helpers += definition(system, '[[nodiscard]] inline bool prefer_source_release(')
        helpers += integrity[:integrity.index('        [[nodiscard]] inline std::optional<std::vector<unsigned char>>')]
        owner = definition(system, 'bool update_discovery_contract_self_test(')
        old_condition = 'PROJECT_SOURCE_URL().ends_with(PROJECT_SOURCE_ARCHIVE_EXTENSION())'
        new_condition = 'PROJECT_SOURCE_URL() == PROJECT_RELEASE_INTEGRITY_URL()\n                           && PROJECT_SOURCE_CHECKSUM_URL().empty()\n                           && PROJECT_SOURCE_ARCHIVE_EXTENSION() == ".zip"'
        self.assertIn(new_condition, owner)
        before = owner.replace(new_condition, old_condition)
        recorded_before = report.parent / 'before-selftest.cpp'
        if recorded_before.exists():
            self.assertEqual(before, recorded_before.read_text(encoding='utf-8'))
        outcomes = []
        edition_header = (ROOT / 'Engine/include/epoch.edition.hpp').read_text(encoding='utf-8')
        for edition, marker in (('development', '0'), ('public', '1')):
            for configuration in ('Debug', 'Release'):
                folder = report / (edition + '-' + configuration)
                folder.mkdir()
                header, count = re.subn(r'^#define EPOCH_SOURCE_EDITION_PUBLIC [01]$',
                                        f'#define EPOCH_SOURCE_EDITION_PUBLIC {marker}',
                                        edition_header, flags=re.M)
                self.assertEqual(count, 1)
                (folder / 'epoch.edition.hpp').write_text(header, encoding='utf-8')
                (folder / 'updater.distribution.hpp').write_bytes((ROOT / 'Engine/include/updater.distribution.hpp').read_bytes())
                prelude = '#include <optional>\n#include <regex>\n#include <vector>\n#include "updater.distribution.hpp"\n'
                prelude += '#include "' + (ROOT / 'Engine/dep/EpochPlatformEngine/include/epochplatformengine/types.hpp').as_posix() + '"\n' + version
                prelude += '\nnamespace epochengine::updater { namespace platform_policy { inline constexpr auto source_snapshot_archive_extension() { return ".tar.gz"; } }\n'
                prelude += declarations + functions + 'namespace system_detail {\n' + helpers + '\n}\n'
                for stage, body in (('before', before), ('after', owner)):
                    source = folder / (stage + '.cpp')
                    source.write_text(prelude + body + '\n}\nint main() { return epochengine::updater::update_discovery_contract_self_test() ? 0 : 156; }\n', encoding='utf-8')
                    binary = folder / (stage + '.exe')
                    args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX', '/Z7',
                            *(['/Od', '/MDd', '/D_DEBUG'] if configuration == 'Debug' else ['/O2', '/MD', '/DNDEBUG']),
                            '/I' + str(folder), '/I' + str(toolset / 'include'),
                            *('/I' + str(kit / part) for part in ('ucrt', 'shared', 'um')),
                            '/Fo' + str(folder / (stage + '.obj')), '/Fe' + str(binary), str(source), '/link',
                            '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(kits / 'Lib' / kit.name / 'um/x64'),
                            '/LIBPATH:' + str(kits / 'Lib' / kit.name / 'ucrt/x64')]
                    build = subprocess.run(args, cwd=folder, capture_output=True, text=True, timeout=60)
                    (folder / (stage + '-build.log')).write_text(build.stdout + build.stderr)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(binary)], cwd=folder, capture_output=True, text=True, timeout=10)
                    expected = 156 if stage == 'before' and edition == 'public' else 0
                    self.assertEqual(run.returncode, expected, str(folder) + ' ' + stage)
                    outcomes.append({'edition': edition, 'configuration': configuration, 'stage': stage, 'exit': run.returncode})
        (report / 'result.json').write_text(json.dumps({'actual_complete_self_test': True, 'physical_edition_headers': True,
            'platform_archive_extension_port': 'tar.gz (Public must independently select its one shared ZIP)',
            'results': outcomes, 'network_models_gui': False}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
