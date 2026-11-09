"""Actual runtime asset lookup and shader-target proof without Engine/GUI builds."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class RuntimeResourceClosure(unittest.TestCase):
    def fixture(self):
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='runtime-resources-', dir=parent)).resolve()
        self.addCleanup(lambda: shutil.rmtree(scratch) if scratch.is_relative_to(parent.resolve()) else None)
        return scratch

    def test_complete_executable_assets_precede_checkout_without_rebinding_source(self):
        source = (ROOT / 'Engine/src/platform/core.path.cpp').read_text(encoding='utf-8')
        functions = '\n'.join(definition(source, marker) for marker in (
            '        [[nodiscard]] bool exists_noerr(',
            '    bool is_epoch_repo_root(',
            '    path runtime_root_dir(',
            '    path engine_asset_dir('))
        prefix = r'''
#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
namespace fs=std::filesystem;
using path=fs::path;
path fixtureExecutableRoot,repo;
std::map<std::string,path> overrides;
path normalize(const path& p){return p.lexically_normal();}
path env_override_path(const char* name){return overrides[name];}
path executable_dir(){return fixtureExecutableRoot;}
path executable_path(){return fixtureExecutableRoot/"Game.exe";}
path find_epoch_repo_root(const path&){return repo;}
path resolve_runtime_root_from(const path& p){return p;}
'''
        main = r'''
void put(const path& p){fs::create_directories(p.parent_path());std::ofstream(p)<<"fixture";}
int main(){
 const auto root=fs::current_path();repo=root/"repo";
 for(const auto* name:{"Engine/CMakeLists.txt","Engine/include/epoch.engine.hpp",
     "Engine/src/epoch/epoch.main.cpp","Engine/examples/EpochEngine/EpochEngine.vcxproj"})put(repo/name);
 fixtureExecutableRoot=repo/"Projects/Game/build/bin";fs::create_directories(fixtureExecutableRoot);
 fs::create_directories(repo/"Engine/assets");
 const auto staged=fixtureExecutableRoot/"assets";
 assert(runtime_root_dir()==repo);assert(engine_asset_dir()==repo/"Engine/assets");
 const char* resources[]={"fonts/Roboto-Regular.ttf","fonts/OFL.txt","vulkan/texture.ppm",
     "vulkan/shaders/vert.spv","vulkan/shaders/frag.spv","vulkan/shaders/gui_vert.spv","vulkan/shaders/gui_frag.spv"};
 for(const auto* name:resources){put(staged/name);}
 assert(engine_asset_dir()==staged);assert(runtime_root_dir()==repo);
 for(const auto* name:resources){
  fs::remove(staged/name);assert(engine_asset_dir()==repo/"Engine/assets");put(staged/name);
 }
 fs::remove(staged/"fonts/OFL.txt");fs::create_directory(staged/"fonts/OFL.txt");
 assert(engine_asset_dir()==repo/"Engine/assets");fs::remove(staged/"fonts/OFL.txt");put(staged/"fonts/OFL.txt");
 const auto explicitAssets=root/"explicit-assets";fs::create_directories(explicitAssets);
 overrides["EPOCH_ASSET_ROOT"]=explicitAssets;assert(engine_asset_dir()==explicitAssets);
 overrides["EPOCH_ASSET_ROOT"]=repo;assert(engine_asset_dir()==repo/"Engine/assets");overrides.clear();
 const auto explicitRuntime=root/"runtime-override";fs::create_directories(explicitRuntime/"assets");
 overrides["EPOCH_RUNTIME_ROOT"]=explicitRuntime;
 assert(runtime_root_dir()==explicitRuntime);assert(engine_asset_dir()==explicitRuntime/"assets");overrides.clear();
 // An unavailable executable must not silently adopt CWD/assets.
 fs::current_path(fixtureExecutableRoot);fixtureExecutableRoot.clear();assert(engine_asset_dir()==repo/"Engine/assets");fs::current_path(root);
 fixtureExecutableRoot=staged.parent_path();
 fs::remove(staged/"fonts/OFL.txt");
 std::error_code ec;fs::create_symlink(repo/"Engine/CMakeLists.txt",staged/"fonts/OFL.txt",ec);
 if(!ec)assert(engine_asset_dir()==repo/"Engine/assets");
}
'''
        scratch = self.fixture()
        cpp = scratch / 'contract.cpp'
        cpp.write_text(prefix + functions + main, encoding='utf-8')
        if os.name == 'nt':
            tools = msvc_2022_toolsets()
            sdk = Path('C:/Program Files (x86)/Windows Kits/10')
            kits = windows_sdk_includes(sdk)
            if not tools or not kits:
                self.skipTest('Installed MSVC/SDK required')
            tool, kit = tools[-1], kits[-1]
            binary = scratch / 'contract.exe'
            command = [str(tool / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/W4', '/WX',
                       '/I' + str(tool / 'include'), '/I' + str(kit / 'ucrt'),
                       '/Fo' + str(scratch / 'contract.obj'), '/Fe' + str(binary), str(cpp), '/link',
                       '/LIBPATH:' + str(tool / 'lib/x64'), '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'um/x64'),
                       '/LIBPATH:' + str(sdk / 'Lib' / kit.name / 'ucrt/x64')]
        else:
            compiler = shutil.which('clang++') or shutil.which('g++')
            if not compiler:
                self.skipTest('C++23 compiler required')
            binary = scratch / 'contract'
            command = [compiler, '-std=c++23', str(cpp), '-o', str(binary)]
        built = subprocess.run(command, cwd=scratch, capture_output=True, text=True, timeout=60)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        run = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_actual_shader_target_compiles_all_four_with_installed_glslc(self):
        compiler = ROOT / 'Engine/vcpkg_installed/x64-windows/tools/shaderc/glslc.exe'
        if os.name != 'nt':
            compiler = Path(shutil.which('glslc') or '')
        cmake, ninja = shutil.which('cmake'), shutil.which('ninja')
        if not compiler.is_file() or not cmake or not ninja:
            self.skipTest('Existing glslc/CMake/Ninja required; never download tools')
        source = (ROOT / 'Engine/CMakeLists.txt').read_text(encoding='utf-8')
        block = source[source.index('if(GLSLANG_VALIDATOR OR GLSLC)'):source.index('\nepoch_apply_cxx_dialect(epoch)', source.index('if(GLSLANG_VALIDATOR OR GLSLC)'))]
        scratch = self.fixture()
        project, build = scratch / 'source', scratch / 'build'
        project.mkdir()
        text = ('cmake_minimum_required(VERSION 3.28)\nproject(ShaderClosure NONE)\n'
                'add_custom_target(epoch)\nadd_custom_target(epoch_renderer_smoke)\n'
                'set(GLSLANG_VALIDATOR GLSLANG_VALIDATOR-NOTFOUND)\n'
                f'set(GLSLC "{compiler.as_posix()}")\n'
                f'set(CMAKE_CURRENT_SOURCE_DIR "{(ROOT / "Engine").as_posix()}")\n'
                'set(EPOCH_VULKAN_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets/vulkan/shaders")\n'
                'file(MAKE_DIRECTORY "${EPOCH_VULKAN_SHADER_DIR}")\n' + block)
        (project / 'CMakeLists.txt').write_text(text, encoding='utf-8')
        configure = subprocess.run([cmake, '-S', str(project), '-B', str(build), '-G', 'Ninja',
                                    '-DCMAKE_MAKE_PROGRAM=' + ninja], capture_output=True, text=True, timeout=30)
        self.assertEqual(configure.returncode, 0, configure.stdout + configure.stderr)
        built = subprocess.run([cmake, '--build', str(build), '--target', 'epoch_vulkan_shaders', '--parallel', '1'],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        outputs = {}
        for name in ('vert', 'frag', 'gui_vert', 'gui_frag'):
            data = (build / 'assets/vulkan/shaders' / (name + '.spv')).read_bytes()
            self.assertGreater(len(data), 20)
            self.assertEqual(struct.unpack_from('<I', data)[0], 0x07230203)
            outputs[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        receipt = ROOT / 'build/validation/runtime-resources-oct26/shader-target-result.json'
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text(json.dumps({'scope': 'Actual extracted production CMake target, real local glslc; no Engine/Linux/GPU build',
                                       'compiler': str(compiler), 'configure_exit': configure.returncode,
                                       'build_exit': built.returncode, 'outputs': outputs}, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
