"""Compile the actual atlas with texture/logger boundaries stubbed, in Debug mode."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class AtlasPacking(unittest.TestCase):
    def test_actual_atlas_against_original_first_fit(self):
        module = (ROOT/'Engine/modules/atlas.texture.ixx').read_text(encoding='utf-8')
        module = module.replace('module;', '').replace('export module atlas.texture;', '')
        module = module.replace('#include "../include/engine.config.hpp"', '')
        module = module.replace('import texture.core;', '').replace('import core.logger;', '')
        module = module.replace('export namespace epochengine', 'namespace epochengine')
        code = (ROOT/'Tests/renderers/atlas.packing_contract.cpp').read_text(encoding='utf-8').replace('ATLAS_PRODUCTION', module)
        if os.name != 'nt': self.skipTest('Installed Windows MSVC CPU lane')
        toolsets=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((kitroot/'Include').glob('*'))
        if not toolsets or not kits:self.skipTest('MSVC/SDK required')
        parent=(ROOT/'build/contract-tests').resolve();parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='atlas-packing-',dir=parent))
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
            self.assertEqual(executed.returncode,0,'Actual atlas contract: '+failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent));shutil.rmtree(scratch)

if __name__=='__main__':unittest.main()
