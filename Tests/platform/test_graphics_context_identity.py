"""Compile actual graphics declarations beside the canonical native descriptor."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class GraphicsContextIdentity(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Installed MSVC declaration proof')
    def test_native_and_graphics_descriptors_have_distinct_names(self):
        source = (ROOT/'Engine/modules/platform.context.ixx').read_text()
        declarations = source.split('export namespace epochengine::platform',1)[1].split('    class IGraphicsContext',1)[0] + '}\n'
        self.assertIn('struct GraphicsContextDesc', declarations)
        native = ROOT/'Engine/dep/EpochPlatformEngine/include'
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kits = sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').glob('*'))
        self.assertTrue(toolsets and kits, 'MSVC and SDK required')
        parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent,prefix='graphics-context-') as folder:
            folder=Path(folder)
            prefix='#include <epochplatformengine/runtime.hpp>\n#include <type_traits>\nnamespace epochengine::platform '
            checks='static_assert(!std::is_same_v<epochengine::platform::ContextDesc, epochengine::platform::GraphicsContextDesc>);\n'
            compiler=toolsets[-1]/'bin/Hostx64/x64/cl.exe'
            args=[str(compiler),'/nologo','/std:c++latest','/EHsc','/c','/DEPOCH_PLATFORM_ENGINE_STATIC=1','/I'+str(native),'/I'+str(toolsets[-1]/'include')]
            args += ['/I'+str(kits[-1]/part) for part in ('ucrt','shared','um')]
            for previous in (False,True):
                code=prefix+declarations+checks
                if previous: code=prefix+declarations.replace('GraphicsContextDesc','ContextDesc')
                path=folder/('previous.cpp' if previous else 'current.cpp');path.write_text(code)
                result=subprocess.run(args+[str(path)],cwd=folder,capture_output=True,text=True,timeout=60)
                if previous:
                    self.assertNotEqual(result.returncode,0,'Old duplicate declarations unexpectedly accepted')
                    self.assertIn('ContextDesc',result.stdout+result.stderr)
                else:self.assertEqual(result.returncode,0,result.stdout+result.stderr)
