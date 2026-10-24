"""Run the actual shared projection and Vulkan clip conversion without a GPU."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class VulkanPreviewProjection(unittest.TestCase):
    def test_perspective_and_asymmetric_orthographic_clip_coordinates(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk / 'Include').glob('*'))
        if not tools or not kits:
            self.skipTest('MSVC/SDK unavailable')
        shared = (ROOT / 'Engine/modules/render.preview_grid.ixx').read_text(encoding='utf-8')
        native = (ROOT / 'Engine/modules/vulkan.context_descriptor.ixx').read_text(encoding='utf-8')
        functions = '\n'.join(definition(shared, marker).replace('export ', '') for marker in (
            '    export [[nodiscard]] inline Mat4 identity_matrix(',
            '    export [[nodiscard]] inline Mat4 perspective(',
            '    export [[nodiscard]] inline Mat4 orthographic('))
        conversion = '\n'.join(definition(native, marker) for marker in (
            '        [[nodiscard]] inline glm::mat4 previewgrid_to_glm(',
            '        [[nodiscard]] inline glm::mat4 previewgrid_to_vulkan_projection('))
        code = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
namespace epochengine::previewgrid {
using Mat4=std::array<float,16>;
''' + functions + r'''
}
namespace glm {
struct mat4 {
 float values[4][4]{};
 explicit mat4(float diagonal) {for(int n=0;n<4;++n)values[n][n]=diagonal;}
 float* operator[](int column){return values[column];}
 const float* operator[](int column)const{return values[column];}
};
}
''' + conversion + r'''
int main(){
 using namespace epochengine::previewgrid;
 const auto check=[](const Mat4& source){
  const auto native=previewgrid_to_vulkan_projection(source);
  for(float z:{-.05f,-.1f,-1.f,-20.f,-1000.f}){
   const std::array<float,4> point{.37f,-.62f,z,1.f};
   std::array<float,4> gl{},vk{};
   for(int row=0;row<4;++row)for(int col=0;col<4;++col){
    gl[row]+=source[col*4+row]*point[col];
    vk[row]+=native[col][row]*point[col];
   }
   const auto near=[](float a,float b){return std::abs(a-b)<.0001f;};
   assert(near(vk[0],gl[0]) && near(vk[1],-gl[1]));
   assert(near(vk[2],.5f*(gl[2]+gl[3])) && near(vk[3],gl[3]));
  }
 };
 const auto perspectiveMatrix=perspective(1.1f,1.6f,.05f,1000.f);
 const auto orthographicMatrix=orthographic(-3.f,7.f,-2.f,9.f,.05f,1000.f);
 check(perspectiveMatrix); check(orthographicMatrix);
 for(const auto source:{perspectiveMatrix,orthographicMatrix}){
  const auto native=previewgrid_to_vulkan_projection(source);
  const auto depth=[&](float z){return (native[2][2]*z+native[3][2])/(native[2][3]*z+native[3][3]);};
  assert(std::abs(depth(-.05f))<.00001f);
  assert(std::abs(depth(-1000.f)-1.f)<.00001f);
 }
 // Asymmetric ortho includes a Y translation; flipping only [1][1] is insufficient.
 const auto ortho=previewgrid_to_vulkan_projection(orthographicMatrix);
 assert(ortho[3][1]==-orthographicMatrix[13]);
}
'''
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='vulkan-projection-', dir=parent))
        try:
            cpp, binary = scratch / 'contract.cpp', scratch / 'contract.exe'
            cpp.write_text(code, encoding='utf-8')
            tool, kit = tools[-1], kits[-1]
            result = subprocess.run([
                str(tool / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc',
                '/I'+str(tool/'include'), '/I'+str(kit/'ucrt'),
                '/Fo'+str(scratch/'contract.obj'), '/Fe'+str(binary), str(cpp),
                '/link', '/LIBPATH:'+str(tool/'lib/x64'),
                '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),
                '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')],
                cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-5000:]+result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
