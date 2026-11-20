"""Actual per-context texture retirement with a recording GL/registry boundary."""
from pathlib import Path
import os
import shutil
import tempfile
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class OpenGLContextRetirement(unittest.TestCase):
    def test_actual_texture_retirement_preserves_sibling_context(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        source = (ROOT/'Engine/modules/opengl/opengl.textures.ixx').read_text(encoding='utf-8')
        structures = '\n'.join(definition(source, '    struct '+name, True)
            for name in ('AtlasGPU', 'NativeRenderTextureGPU', 'NativeTextureGPU',
                         'NativeTextureKey final', 'NativeTextureKeyHash final'))
        backend = definition(source, '    struct BackendData', True)
        key = definition(source, '    [[nodiscard]] inline const void* platform_context_key(')
        converter = definition(source, '        inline epochengine::openglcontext::PlatformGL::PlatformGLContext')
        delete_render_objects = definition(source, '    inline void delete_native_render_texture_objects(')
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
enum class GLObject { texture, framebuffer, renderbuffer, sampler };
using GLName=std::pair<GLObject,GLuint>;
inline std::vector<std::pair<const void*,GLName>> deletions;
void record_delete(GLObject kind,int count,const GLuint* names) {
 assert(count==1 && names && *names);
 deletions.emplace_back(activeContext,GLName{kind,*names});
}
void glDeleteTextures(int count,const GLuint* names) {record_delete(GLObject::texture,count,names);}
void glDeleteFramebuffers(int count,const GLuint* names) {record_delete(GLObject::framebuffer,count,names);}
void glDeleteRenderbuffers(int count,const GLuint* names) {record_delete(GLObject::renderbuffer,count,names);}
void glDeleteSamplers(int count,const GLuint* names) {record_delete(GLObject::sampler,count,names);}
namespace epochengine::opengltextures {
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
 backend->native_render_textures[{&a,31}]={.context_key=&a,.framebuffer=32,.color=31,.depth=33,.sampler=34,.active=true};
 backend->native_render_textures[{&a,0}]={.context_key=&a,.active=true};
 backend->native_render_textures[{&b,31}]={.context_key=&b,.framebuffer=32,.color=31,.depth=33,.sampler=34,.active=true};
 const auto retire=[&](auto context) {
  activeContext=context.context;
  clear_gpu_atlases(context);
 };
 using enum GLObject;
 const auto expectDeleted=[&](const void* context,std::vector<GLName> expected) {
  std::vector<GLName> actual;
  for(const auto& [owner,name]:deletions) { assert(owner==context); actual.push_back(name); }
  std::sort(actual.begin(),actual.end()); std::sort(expected.begin(),expected.end());
  assert(actual==expected);
 };
 // Null contexts do nothing, including the global handle-generation counter.
 retire(openglcontext::PlatformGL::PlatformGLContext{});
 assert(deletions.empty() && s_generation.load()==1);
 assert(backend->context_gpu_atlases.size()==2 && backend->native_textures.size()==3);
 assert(backend->native_render_textures.size()==3);
 retire(contextA);
 expectDeleted(&a,{{texture,17},{texture,23},{texture,31},{framebuffer,32},{renderbuffer,33},{sampler,34}});
 assert(s_generation.load()==2);
 assert(!backend->context_gpu_atlases.contains(&a));
 assert(backend->context_gpu_atlases.at(&b).at(&atlasB).textureHandle==17);
 assert(backend->native_textures.size()==1);
 assert(backend->native_textures.at({&b,23}).texture==23);
 assert(backend->native_render_textures.size()==1);
 const auto& siblingRenderTexture=backend->native_render_textures.at({&b,31});
 assert(siblingRenderTexture.active && siblingRenderTexture.framebuffer==32
    && siblingRenderTexture.depth==33 && siblingRenderTexture.sampler==34);
 assert(backend->gpu_atlases.at(&fallback).textureHandle==91); // Default B is still live.
 deletions.clear();
 // An absent/repeated context invalidates stale handles but deletes no sibling resource.
 retire(contextUnknown); retire(contextA);
 assert(deletions.empty() && s_generation.load()==4);
 assert(backend->context_gpu_atlases.size()==1 && backend->gpu_atlases.size()==1);
 assert(backend->native_textures.size()==1);
 assert(backend->native_render_textures.size()==1);
 retire(contextB);
 expectDeleted(&b,{{texture,17},{texture,23},{texture,31},{framebuffer,32},{renderbuffer,33},{sampler,34},{texture,91}});
 assert(s_generation.load()==5);
 assert(backend->context_gpu_atlases.empty() && backend->gpu_atlases.empty());
 assert(backend->native_textures.empty() && backend->native_render_textures.empty());
 deletions.clear();
 // A matching default context with no per-context map still owns its fallback atlas.
 backend->glState.hglrc=&a; backend->gpu_atlases[&fallback].textureHandle=72;
 retire(contextB);
 assert(deletions.empty() && backend->gpu_atlases.size()==1 && s_generation.load()==6);
 retire(contextA); expectDeleted(&a,{{texture,72}});
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
            cpp.write_text('\n'.join((prefix, structures,
                'using AtlasGpuMap=std::unordered_map<const TextureAtlas*,AtlasGPU>;',
                backend, 'inline std::atomic_uint8_t s_generation{1};',
                'namespace detail {', converter, '}', key, delete_render_objects, retirement, main)),
                encoding='utf-8')
            compile_and_run(self, cpp, scratch)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
