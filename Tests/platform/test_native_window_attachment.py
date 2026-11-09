"""Compile the production native attachment lease with mocked OS APIs only."""
from pathlib import Path
import os
import re
import hashlib
import json
import subprocess
import unittest
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]

class NativeWindowAttachment(unittest.TestCase):
    def test_actual_native_lease_lifetime(self):
        dependency = ROOT / 'Engine/dep/EpochPlatformEngine'
        source = (dependency / 'src/platform/platform.window_attachment.cpp').read_text(encoding='utf-8')
        owner = source.split('namespace\n{\n', 1)[1].split('} // namespace', 1)[0]
        interface = (dependency / 'modules/platform.window_attachment.ixx').read_text(encoding='utf-8')
        interface = '    class INativeWindowAttachment' + interface.split('    class INativeWindowAttachment', 1)[1].split('    // Caller', 1)[0]
        window = (dependency / 'modules/platform.window.ixx').read_text(encoding='utf-8')
        handle = '    struct WindowHandle' + window.split('    struct WindowHandle', 1)[1].split('    enum class NativeInputKind', 1)[0]
        apis = sorted(set(re.findall(r'::([A-Z][A-Za-z0-9_]+)', owner)))
        redirects = '\n'.join('#undef '+api+'\n#define '+api+' mock_'+api for api in apis)
        code = (ROOT / 'Tests/platform/platform.window_attachment_contract.cpp').read_text(encoding='utf-8')
        code = code.replace('// API_REDIRECTS', redirects).replace('// PRODUCTION_HANDLE', handle).replace('// PRODUCTION_INTERFACE', interface).replace('// PRODUCTION_OWNER', owner)
        if os.name != 'nt':
            # The test executes the Windows adapter's real conditional path.
            self.skipTest('Installed MSVC CPU lane')
        toolsets = msvc_2022_toolsets()
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = windows_sdk_includes(kitroot)
        if not toolsets or not kits:
            self.skipTest('Installed MSVC/SDK required')
        proof = ROOT/'Artifacts/Reports/runtime-viewport-stall-oct29/native-lease'/hashlib.sha256(code.encode()).hexdigest()[:16]
        proof.mkdir(parents=True,exist_ok=True)
        source = proof/'contract.cpp'
        source.write_text(code,encoding='utf-8')
        toolset, kit = toolsets[-1], kits[-1]
        results=[]
        for config in ('Debug','Release'):
            scratch=proof/config
            scratch.mkdir(exist_ok=True)
            binary=scratch/'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                '/MDd' if config=='Debug' else '/MD','/Z7' if config=='Debug' else '/O2',
                '/I'+str(dependency/'include'),
                '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
                '/I' + str(kit / 'um'), '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary),
                str(source), '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
            compiled = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=90)
            (scratch/'compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            lines = code.splitlines()
            failure = lines[executed.returncode - 1] if 0 < executed.returncode <= len(lines) else ''
            self.assertEqual(executed.returncode, 0, 'Actual native attachment CPU contract: ' + failure)
            results.append(dict(configuration=config,exit=executed.returncode))
        # The former permissive logger port accepted a std::string that the
        # actual ABI rejects. Keep this as a compiler-negative regression.
        negative=code.replace('epochengine::string_view{message.data(), message.size()}', 'message')
        self.assertNotEqual(negative,code)
        negative_source=proof/'abi-negative.cpp'
        negative_source.write_text(negative,encoding='utf-8')
        negative_args=args[:args.index('/link')]
        negative_args[negative_args.index(str(source))]=str(negative_source)
        negative_args += ['/c']
        rejected=subprocess.run(negative_args,cwd=scratch,capture_output=True,text=True,timeout=45)
        (proof/'abi-negative.log').write_text(rejected.stdout+rejected.stderr,encoding='utf-8')
        self.assertNotEqual(rejected.returncode,0,'Real custom string-view ABI must reject std::string')
        self.assertIn('C2664',rejected.stdout+rejected.stderr)
        results.append(dict(case='former_std_string_log_abi',compile_exit=rejected.returncode,expected='C2664'))
        (proof/'result.json').write_text(json.dumps(dict(passed=True,cases=results,
            source_sha256=hashlib.sha256((dependency/'src/platform/platform.window_attachment.cpp').read_bytes()).hexdigest(),
            scope='Actual native attachment owner with recorded Windows ports; stage/native-error/geometry recorded before rollback, bounded32 refusals; no native windows/GPU'),indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
