"""Compile the production native attachment lease with mocked OS APIs only."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

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
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('Installed MSVC/SDK required')
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='native-attachment-', dir=parent))
        try:
            source = scratch / 'contract.cpp'
            source.write_text(code, encoding='utf-8')
            toolset, kit = toolsets[-1], kits[-1]
            binary = scratch / 'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
                '/I' + str(kit / 'um'), '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary),
                str(source), '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
            compiled = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=90)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            lines = code.splitlines()
            failure = lines[executed.returncode - 1] if 0 < executed.returncode <= len(lines) else ''
            self.assertEqual(executed.returncode, 0, 'Actual native attachment CPU contract: ' + failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()
