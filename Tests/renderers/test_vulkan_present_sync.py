"""Compile actual Vulkan semaphore allocation, cleanup and draw functions on CPU.
Only Vulkan/device boundaries are faked; no window/GPU/driver is started.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class VulkanPresentSync(unittest.TestCase):
    def test_actual_allocator_cleanup_and_frame_submission(self):
        commands = (ROOT / 'Engine/modules/vulkan.context_commands.ixx').read_text(encoding='utf-8')
        swap = (ROOT / 'Engine/modules/vulkan.context_swapchain.ixx').read_text(encoding='utf-8')
        allocator = 'void Application::createPresentSyncObjects()' + commands.split('void Application::createPresentSyncObjects()',1)[1].split('    void Application::createSyncObjects()',1)[0]
        cleanup = 'void Application::cleanupSwapChain()' + swap.split('void Application::cleanupSwapChain()',1)[1].split('    void Application::recreateSwapChain()',1)[0]
        draw = 'void Application::drawFrame()' + commands.split('void Application::drawFrame()',1)[1].rsplit('} // namespace',1)[0]
        code = (ROOT/'Tests/renderers/vulkan.present_sync_contract.cpp').read_text(encoding='utf-8-sig').replace('ALLOCATOR',allocator).replace('CLEANUP',cleanup).replace('DRAW_FRAME',draw)
        if os.name != 'nt': self.skipTest('Installed Windows MSVC CPU lane')
        toolsets=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((kitroot/'Include').glob('*'))
        if not toolsets or not kits:self.skipTest('MSVC/SDK required')
        parent=(ROOT/'build/contract-tests').resolve();parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='vulkan-present-sync-',dir=parent))
        try:
            source=scratch/'contract.cpp';source.write_text(code,encoding='utf-8')
            toolset,kit=toolsets[-1],kits[-1];binary=scratch/'contract.exe'
            args=[str(toolset/'bin/Hostx64/x64/cl.exe'),'/nologo','/std:c++latest','/EHsc','/WX','/O2',
                '/I'+str(toolset/'include'),'/I'+str(kit/'ucrt'),'/I'+str(kit/'shared'),'/I'+str(kit/'um'),
                '/Fo'+str(scratch/'contract.obj'),'/Fe'+str(binary),str(source),'/link',
                '/LIBPATH:'+str(toolset/'lib/x64'),'/LIBPATH:'+str(kitroot/'Lib'/kit.name/'um/x64'),
                '/LIBPATH:'+str(kitroot/'Lib'/kit.name/'ucrt/x64')]
            compiled=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=90)
            self.assertEqual(compiled.returncode,0,compiled.stdout+compiled.stderr)
            executed=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
            lines=code.splitlines();failure=lines[executed.returncode-1] if 0<executed.returncode<=len(lines) else ''
            self.assertEqual(executed.returncode,0,'Actual Vulkan contract: '+failure)
            # The former cleanup order must fail this same ownership fixture.
            # Otherwise a passing semaphore test could hide a destroyed-pool free.
            previous = code.replace(
                'descriptorSets.clear();\n        descriptorPool.reset();',
                'descriptorPool.reset();\n        descriptorSets.clear();', 1)
            self.assertNotEqual(previous, code)
            source.write_text(previous, encoding='utf-8')
            compiled=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=90)
            self.assertEqual(compiled.returncode,0,compiled.stdout+compiled.stderr)
            executed=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
            self.assertNotEqual(executed.returncode,0,'Destroyed descriptor pool was accepted')
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent));shutil.rmtree(scratch)

    def test_production_recreation_orders_idle_cleanup_and_image_allocation(self):
        source=(ROOT/'Engine/modules/vulkan.context_swapchain.ixx').read_text(encoding='utf-8')
        recreation=source.split('void Application::recreateSwapChain()',1)[1]
        self.assertLess(recreation.index('device->waitIdle()'),recreation.index('cleanupSwapChain();'))
        self.assertLess(recreation.index('cleanupSwapChain();'),recreation.index('createSwapChain();'))
        create=source.split('void Application::createSwapChain()',1)[1].split('void Application::createImageViews()',1)[0]
        self.assertLess(create.index('swapChainImages = std::move(images);'),create.index('createPresentSyncObjects();'))

if __name__=='__main__':unittest.main()
