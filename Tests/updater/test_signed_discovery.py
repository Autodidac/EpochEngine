"""Execute production JSON/edition admission; crypto is an explicit test boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class SignedDiscovery(unittest.TestCase):
    def test_actual_public_and_development_admission(self):
        from Tests.ai.test_epoch_90146_embedded_preview_input import definition
        source=(ROOT/'Engine/src/updater/implementation/updater.release_integrity.inl').read_text(encoding='utf-8')
        parser=source[:source.index('        [[nodiscard]] inline std::optional<std::vector<unsigned char>>')]
        admission=definition(source,'[[nodiscard]] inline std::optional<std::string> verified_release_integrity_payload(')
        code=(ROOT/'Tests/updater/updater.signed_discovery_contract.cpp').read_text().replace('PRODUCTION_PARSER',parser).replace('PRODUCTION_ADMISSION',admission)
        config=(ROOT/'Engine/modules/updater.config.ixx').read_text(encoding='utf-8')
        url=definition(config, 'export inline std::string PROJECT_RELEASE_INTEGRITY_URL(').replace('export ', '')
        policy='inline constexpr bool PROJECT_PACKAGED_UPDATES_ENABLED =' + config.split('export inline constexpr bool PROJECT_PACKAGED_UPDATES_ENABLED =',1)[1].split(';',1)[0] + ';'
        policies=''
        for name, public in [('development_policy', 'false'), ('public_policy', 'true')]:
            policies += 'namespace '+name+' { constexpr bool PUBLIC_DISTRIBUTION='+public+'; constexpr bool SITE_DISTRIBUTION=!PUBLIC_DISTRIBUTION; constexpr auto GITHUB_BASE="https://github.com/"; '+policy+url+' static_assert(PROJECT_PACKAGED_UPDATES_ENABLED == PUBLIC_DISTRIBUTION); }'
        types=source[source.index('        struct IntegrityAssetInfo'):source.index('        [[nodiscard]] inline std::optional<std::string> download_expected_sha256(')]
        resolver=definition(source, '[[nodiscard]] inline ResolvedPackagedRelease resolve_packaged_release(')
        code=code.replace('PRODUCTION_RELEASE_TYPES',types).replace('PRODUCTION_RESOLVER',resolver).replace('CONFIG_POLICIES',policies)
        system=(ROOT/'Engine/src/updater/updater.system.cpp').read_text(encoding='utf-8')
        command=definition(system,'UpdateCommandResult run_update_command(')
        self.assertIn('result.packaged_release_checked = PROJECT_PACKAGED_UPDATES_ENABLED;',command)
        self.assertIn('!PROJECT_PACKAGED_UPDATES_ENABLED || packaged_status.ok;',command)
        self.assertIn('run_source_update_command(channel, false, false, honor_env_silent)',command)
        self.assertIn('check_source_version_once_per_run(',command)
        version=(ROOT/'Engine/modules/epoch.version.ixx').read_text(encoding='utf-8').split('export module epoch.version;',1)[0].replace('module;', '').replace('../include/epoch.edition.hpp', (ROOT/'Engine/include/epoch.edition.hpp').as_posix())
        code = version + '\nstatic_assert(EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE == EPOCH_VERSION_REVISION_VALUE);\nstatic_assert(EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE == EPOCH_VERSION_REVISION_VALUE);\nstatic_assert(EPOCH_MACOS_PACKAGED_VERSION_REVISION_VALUE == (EPOCH_EDITION_PUBLIC ? EPOCH_VERSION_REVISION_VALUE : 30));\n' + code
        code += '\n#undef EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE\n#undef EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE\n#define EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_REVISION 191\n#define EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_REVISION 192\n' + version + '\nstatic_assert(EPOCH_WINDOWS_PACKAGED_VERSION_REVISION_VALUE == 191);\nstatic_assert(EPOCH_LINUX_PACKAGED_VERSION_REVISION_VALUE == 192);\n'
        if os.name != 'nt': self.skipTest('Installed Windows MSVC CPU lane')
        toolsets=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((kitroot/'Include').glob('*'))
        if not toolsets or not kits:self.skipTest('MSVC/SDK required')
        parent=(ROOT/'build/contract-tests').resolve();parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='updater-integrity-',dir=parent))
        try:
            source=scratch/'contract.cpp';source.write_text(code,encoding='utf-8')
            toolset,kit=toolsets[-1],kits[-1];binary=scratch/'contract.exe'
            args=[str(toolset/'bin/Hostx64/x64/cl.exe'),'/nologo','/std:c++latest','/EHsc','/WX','/Od', '/D_DEBUG', '/MDd',
                '/I'+str(toolset/'include'),'/I'+str(kit/'ucrt'),'/I'+str(kit/'shared'),'/I'+str(kit/'um'),
                '/Fo'+str(scratch/'contract.obj'),'/Fe'+str(binary),str(source),'/link',
                '/LIBPATH:'+str(toolset/'lib/x64'),'/LIBPATH:'+str(kitroot/'Lib'/kit.name/'um/x64'),
                '/LIBPATH:'+str(kitroot/'Lib'/kit.name/'ucrt/x64')]
            compiled=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=90)
            self.assertEqual(compiled.returncode,0,compiled.stdout+compiled.stderr)
            executed=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=60)
            lines=code.splitlines();failure=lines[executed.returncode-1] if 0<executed.returncode<=len(lines) else ''
            self.assertEqual(executed.returncode,0,'Actual signed discovery policy: '+failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent));shutil.rmtree(scratch)

if __name__=="__main__": unittest.main()
