"""Actual per-context texture retirement with a recording GL/registry boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class OpenGLContextRetirement(unittest.TestCase):
    def test_actual_texture_retirement_preserves_sibling_context(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        tools = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk/'Include').glob('*'))
        if not tools or not kits:
            self.skipTest('MSVC/SDK unavailable')
        source = (ROOT/'Engine/modules/opengl.textures.ixx').read_text(encoding='utf-8')
        structures = '\n'.join(definition(source, '    struct '+name, True)
            for name in ('AtlasGPU', 'NativeTextureGPU', 'NativeTextureKey final', 'NativeTextureKeyHash final'))
        key = definition(source, '    [[nodiscard]] inline const void* platform_context_key(')
        converter = definition(source, '        inline epochengine::openglcontext::PlatformGL::PlatformGLContext')
        retirement = definition(source, '    inline void clear_gpu_atlases(')
        prefix = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
using GLuint=unsigned; using u64=std::uint64_t; using u32=std::uint32_t;
struct TextureAtlas {};
namespace epochengine {
 struct TextureDesc {};
 namespace openglstate { struct OpenGL4State { void* hdc{}; void* hglrc{}; }; }
 namespace openglcontext::PlatformGL { struct PlatformGLContext { void* device{}; void* context{}; }; }
 namespace core {
  enum class ContextType { OpenGL, Other };
  struct Backend { std::shared_ptr<void> data; };
  inline std::map<ContextType,Backend> g_backends;
  inline std::shared_mutex g_backendsMutex;
 }
}
inline const void* activeContext{};
inline std::vector<std::pair<const void*,GLuint>> deletions;
void glDeleteTextures(int count,const GLuint* textures) {
 assert(count==1 && textures && *textures);
 deletions.emplace_back(activeContext,*textures);
}
namespace epochengine::opengltextures {
'''
        backend = r'''
using AtlasGpuMap=std::unordered_map<const TextureAtlas*,AtlasGPU>;
// Only the registry, storage and native driver are substituted. The exact
// production context-key, state conversion, key/hash and retirement body run.
struct BackendData {
 AtlasGpuMap gpu_atlases;
 std::unordered_map<const void*,AtlasGpuMap> context_gpu_atlases;
 std::unordered_map<NativeTextureKey,NativeTextureGPU,NativeTextureKeyHash> native_textures;
 std::mutex gpuMutex;
 epochengine::openglstate::OpenGL4State glState;
};
inline std::atomic_uint8_t s_generation{1};
'''
        main = r'''
}
int main() {
 using namespace epochengine;
 using namespace epochengine::opengltextures;
 int a{},b{},unknown{};
 const auto contextA=openglcontext::PlatformGL::PlatformGLContext{nullptr,&a};
 const auto contextB=openglcontext::PlatformGL::PlatformGLContext{nullptr,&b};
 const auto contextUnknown=openglcontext::PlatformGL::PlatformGLContext{nullptr,&unknown};
 const auto backend=std::make_shared<BackendData>();
 core::g_backends[core::ContextType::OpenGL].data=backend;
 TextureAtlas atlasA,atlasB,emptyAtlas,fallback;
 backend->glState.hglrc=&b;
 backend->context_gpu_atlases[&a][&atlasA].textureHandle=17;
 backend->context_gpu_atlases[&a][&emptyAtlas].textureHandle=0;
 backend->context_gpu_atlases[&b][&atlasB].textureHandle=17; // Legal same GLuint in another namespace.
 backend->gpu_atlases[&fallback].textureHandle=91;
 backend->native_textures[{&a,23}]={.texture=23,.context_key=&a,.active=true};
 backend->native_textures[{&a,0}]={.texture=0,.context_key=&a,.active=true};
 backend->native_textures[{&b,23}]={.texture=23,.context_key=&b,.active=true};
 const auto retire=[&](auto context) {
  activeContext=context.context;
  clear_gpu_atlases(context);
 };
 const auto expectDeleted=[&](const void* context,std::vector<GLuint> expected) {
  std::vector<GLuint> actual;
  for(const auto& [owner,name]:deletions) { assert(owner==context); actual.push_back(name); }
  std::sort(actual.begin(),actual.end()); std::sort(expected.begin(),expected.end());
  assert(actual==expected);
 };
 // Null contexts do nothing, including the global handle-generation counter.
 retire(openglcontext::PlatformGL::PlatformGLContext{});
 assert(deletions.empty() && s_generation.load()==1);
 assert(backend->context_gpu_atlases.size()==2 && backend->native_textures.size()==3);
 retire(contextA);
 expectDeleted(&a,{17,23});
 assert(s_generation.load()==2);
 assert(!backend->context_gpu_atlases.contains(&a));
 assert(backend->context_gpu_atlases.at(&b).at(&atlasB).textureHandle==17);
 assert(backend->native_textures.size()==1);
 assert(backend->native_textures.at({&b,23}).texture==23);
 assert(backend->gpu_atlases.at(&fallback).textureHandle==91); // Default B is still live.
 deletions.clear();
 // An absent/repeated context invalidates stale handles but deletes no sibling resource.
 retire(contextUnknown); retire(contextA);
 assert(deletions.empty() && s_generation.load()==4);
 assert(backend->context_gpu_atlases.size()==1 && backend->gpu_atlases.size()==1);
 assert(backend->native_textures.size()==1);
 retire(contextB);
 expectDeleted(&b,{17,23,91});
 assert(s_generation.load()==5);
 assert(backend->context_gpu_atlases.empty() && backend->gpu_atlases.empty());
 assert(backend->native_textures.empty());
 deletions.clear();
 // A matching default context with no per-context map still owns its fallback atlas.
 backend->glState.hglrc=&a; backend->gpu_atlases[&fallback].textureHandle=72;
 retire(contextB);
 assert(deletions.empty() && backend->gpu_atlases.size()==1 && s_generation.load()==6);
 retire(contextA); expectDeleted(&a,{72});
 assert(backend->gpu_atlases.empty() && s_generation.load()==7);
 deletions.clear();
 // Missing registry entry and an empty backend are harmless valid-context retirements.
 core::g_backends.clear(); retire(contextA);
 assert(deletions.empty() && s_generation.load()==8);
 core::g_backends[core::ContextType::OpenGL]={}; retire(contextB);
 assert(deletions.empty() && s_generation.load()==9);
 retire(openglcontext::PlatformGL::PlatformGLContext{});
 assert(s_generation.load()==9);
}
'''
        parent=ROOT/'build/contract-tests'
        parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='opengl-context-retirement-',dir=parent))
        try:
            cpp=scratch/'contract.cpp'
            cpp.write_text(prefix+structures+backend+'namespace detail {\n'+converter+'\n}\n'+key+retirement+main,encoding='utf-8')
            tool,kit=tools[-1],kits[-1]
            binary=scratch/'contract.exe'
            args=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc',
                  '/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(scratch/'contract.obj'),
                  '/Fe'+str(binary),str(cpp),'/link','/LIBPATH:'+str(tool/'lib/x64'),
                  '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
            result=subprocess.run(args,cwd=scratch,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout[-6000:]+result.stderr)
            result=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
