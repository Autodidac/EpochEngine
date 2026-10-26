"""Compile the real Engine update-policy module against canonical Platform."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class UpdaterPlatformPolicy(unittest.TestCase):
    def test_engine_consumers_use_engine_owner_and_both_build_graphs_include_it(self):
        for name in ('core.commandline.ixx', 'updater.config.ixx'):
            source = (ROOT/'Engine/modules'/name).read_text(encoding='utf-8')
            self.assertIn('import updater.platform_policy;', source)
            for api in ('updater_shell_backend_name', 'source_snapshot_archive_extension', 'source_snapshot_archive_label'):
                self.assertNotIn('epochengine::platform::policy::'+api+'(', source)
        module = (ROOT/'Engine/modules/updater.platform_policy.ixx').read_text(encoding='utf-8')
        self.assertIn('export module updater.platform_policy;', module)
        self.assertIn('import platform.native;', module)
        self.assertNotIn('import platform.engine;', module)
        for name in ('Engine.vcxitems', 'Engine.vcxitems.filters'):
            self.assertIn('modules\\updater.platform_policy.ixx', (ROOT/'Engine'/name).read_text(encoding='utf-8'))
        cmake = (ROOT/'Engine/CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('file(GLOB EPOCH_MODULE_FILES CONFIGURE_DEPENDS', cmake)
        self.assertIn('add_test(NAME epoch_updater_platform_policy_contract', cmake)

    @unittest.skipUnless(os.name == 'nt', 'Installed MSVC module-consumer proof')
    def test_actual_native_and_engine_modules_compile_link_and_run(self):
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kits = sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').glob('*'))
        self.assertTrue(toolsets and kits, 'MSVC and SDK are required')
        tools, sdk = toolsets[-1], kits[-1]
        compiler = tools/'bin/Hostx64/x64/cl.exe'
        common = [str(compiler), '/nologo', '/std:c++latest', '/EHsc', '/MD', '/DEPOCH_PLATFORM_ENGINE_STATIC=1',
                  '/I'+str(ROOT/'Engine/dep/EpochPlatformEngine/include'), '/I'+str(tools/'include')]
        common += ['/I'+str(sdk/part) for part in ('ucrt','shared','um')]
        parent = ROOT/'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix='updater-policy-module-') as folder:
            folder = Path(folder)
            native = ROOT/'Engine/dep/EpochPlatformEngine/modules/platform.native.ixx'
            policy = ROOT/'Engine/modules/updater.platform_policy.ixx'
            contract = ROOT/'Tests/updater/updater.platform_policy_contract.cpp'
            native_ref = ['/reference', 'platform.native='+str(folder/'native.ifc')]
            policy_ref = ['/reference', 'updater.platform_policy='+str(folder/'policy.ifc')]
            compatibility_ref = ['/reference', 'platform.engine='+str(folder/'compatibility.ifc')]
            commands = [
                common+['/c','/interface',str(native),'/ifcOutput'+str(folder/'native.ifc'),'/Fo'+str(folder/'native.obj')],
                common+native_ref+['/c','/interface',str(ROOT/'Engine/dep/EpochPlatformEngine/modules/platform.engine.ixx'),'/ifcOutput'+str(folder/'compatibility.ifc'),'/Fo'+str(folder/'compatibility.obj')],
                common+native_ref+['/c','/interface',str(policy),'/ifcOutput'+str(folder/'policy.ifc'),'/Fo'+str(folder/'policy.obj')],
                common+native_ref+policy_ref+compatibility_ref+[str(contract),str(folder/'native.obj'),str(folder/'policy.obj'),str(folder/'compatibility.obj'),
                    '/Fo'+str(folder/'consumer.obj'),'/Fe'+str(folder/'consumer.exe'),'/link',
                    '/LIBPATH:'+str(tools/'lib/x64'),
                    '/LIBPATH:'+str(sdk.parent.parent/'Lib'/sdk.name/'ucrt/x64'),
                    '/LIBPATH:'+str(sdk.parent.parent/'Lib'/sdk.name/'um/x64')],
                [str(folder/'consumer.exe')],
            ]
            for command in commands:
                result = subprocess.run(command,cwd=folder,capture_output=True,text=True,timeout=90)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
