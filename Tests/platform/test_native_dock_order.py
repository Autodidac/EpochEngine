"""Compile the actual atlas with texture/logger boundaries stubbed, in Debug mode."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class NativeDockOrder(unittest.TestCase):
    def test_actual_order_cancel_and_drag_route(self):
        from Tools.source.owned_source_text import definition
        host=(ROOT/'Engine/src/renderers/host/host.context_win.cpp').read_text(encoding='utf-8')
        interaction=(ROOT/'Engine/src/renderers/host/implementation/host.window_interaction.inl').read_text(encoding='utf-8')
        order=definition(host,'static int native_dock_order(')
        move=definition(host,'void MultiContextManager::ReorderDockedWindow(')
        owns=definition(interaction,'[[nodiscard]] inline bool native_class_owns_gui_input(')
        cancel=definition(interaction,'inline void cancel_native_context_drag(')
        dock=interaction.split('LRESULT CALLBACK DockableProc(',1)[1]
        arm='case WM_LBUTTONDOWN:'+dock.split('case WM_LBUTTONDOWN:',1)[1].split('case WM_RBUTTONDOWN:',1)[0]
        native=(ROOT/'Engine/src/renderers/host/implementation/host.native_dispatch.inl').read_text(encoding='utf-8')
        release='const bool wantsRedock ='+native.split('const bool wantsRedock =',1)[1].split('#if defined(_DEBUG)',1)[0]
        caption='case WM_NCLBUTTONDOWN:'+dock.split('case WM_NCLBUTTONDOWN:',1)[1].split('case WM_LBUTTONDOWN:',1)[0]
        code=(ROOT/'Tests/platform/host.dock_order_contract.cpp').read_text().replace('ORDER_FUNCTION',order).replace('MOVE_FUNCTION',move).replace('CANCEL_FUNCTION',cancel).replace('DOCK_ARM',arm).replace('CAPTION_ARM',caption).replace('RELEASE_POLICY',release).replace('OWNS_FUNCTION',owns)
        if os.name != 'nt': self.skipTest('Installed Windows MSVC CPU lane')
        toolsets=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((kitroot/'Include').glob('*'))
        if not toolsets or not kits:self.skipTest('MSVC/SDK required')
        parent=(ROOT/'build/contract-tests').resolve();parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='native-docking-',dir=parent))
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
