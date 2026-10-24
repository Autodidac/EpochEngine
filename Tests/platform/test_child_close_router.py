"""Exercise actual Platform child-close selection without windows or GUI input."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def definition(source, marker, semicolon=False):
    start = source.index(marker)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + (';' if semicolon else '')


class ChildCloseRouter(unittest.TestCase):
    def test_actual_native_selection_is_owned_live_and_remembers_hover(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk / 'Include').glob('*'))
        if not tools or not kits:
            self.skipTest('MSVC/SDK unavailable')
        api = (ROOT / 'Engine/dep/EpochPlatformEngine/modules/platform.window.ixx').read_text(encoding='utf-8')
        source = (ROOT / 'Engine/dep/EpochPlatformEngine/src/platform/platform.window.cpp').read_text(encoding='utf-8')
        selected = '\n'.join(definition(api, '    struct '+n, True)
                             for n in ('WindowHandle', 'NativeChildWindow'))
        selected += '\nstd::mutex childCloseMutex; std::unordered_map<std::uintptr_t,std::uintptr_t> childCloseTargets;\n'
        selected += definition(source, '    WindowHandle native_child_close_target(')
        selected += definition(source, '    void forget_native_child_close_group(')
        prefix = r'''
#include <cassert>
#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_map>
using HWND=void*;using DWORD=unsigned long;
struct POINT{int x{},y{};};
inline HWND pointer{};
inline std::unordered_map<HWND,DWORD> owners;
inline std::unordered_map<HWND,HWND> parents;
bool GetCursorPos(POINT*){return true;}
HWND WindowFromPoint(POINT){return pointer;}
bool IsWindow(HWND h){return owners.contains(h);}
DWORD GetWindowThreadProcessId(HWND h,DWORD* process){*process=owners[h];return *process?1:0;}
DWORD GetCurrentProcessId(){return 42;}
bool IsChild(HWND ancestor,HWND target){
 for(unsigned i=0;target && i<20;++i){
  if(!parents.contains(target)) return false;
  target=parents[target]; if(target==ancestor)return true;
 } return false;
}
HWND h(unsigned n){return reinterpret_cast<HWND>(static_cast<std::uintptr_t>(n));}
'''
        main = r'''
int main(){
 owners={{h(11),42},{h(12),42},{h(21),42},{h(31),99},{h(111),42}};
 parents[h(111)]=h(11);
 const NativeChildWindow children[]={{{1},{11}},{{1},{12}},{{2},{21}},{{3},{31}}};
 pointer=h(111);
 assert(native_child_close_target({900},children,{900}).value==1);
 pointer=h(999); // Mouse outside the engine still retains its last hovered child.
 assert(native_child_close_target({900},children,{21}).value==1);
 forget_native_child_close_group({900});
 assert(native_child_close_target({900},children,{21}).value==2);
 pointer=h(31); // A foreign process surface never becomes a valid close target.
 const NativeChildWindow foreign[]={{{3},{31}}};
 assert(!native_child_close_target({901},foreign,{31}).valid());
 pointer=h(12);
 assert(native_child_close_target({902},children,{900}).value==1);
 owners.erase(h(11)); owners.erase(h(12));
 assert(native_child_close_target({902},children,{900}).value==2);
 assert(!native_child_close_target({},children,{900}).valid());
 assert(!native_child_close_target({900},{},{900}).valid());
 forget_native_child_close_group({900});forget_native_child_close_group({901});forget_native_child_close_group({902});
 assert(childCloseTargets.empty());
}
'''
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='child-close-', dir=parent))
        try:
            tool, kit = tools[-1], kits[-1]
            cpp = scratch/'contract.cpp'
            cpp.write_text(prefix+selected+main, encoding='utf-8')
            binary=scratch/'contract.exe'
            result=subprocess.run([str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/std:c++latest','/EHsc',
                '/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(scratch/'contract.obj'),
                '/Fe'+str(binary),str(cpp),'/link','/LIBPATH:'+str(tool/'lib/x64'),
                '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')],
                cwd=scratch,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout[-4000:]+result.stderr)
            result=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
