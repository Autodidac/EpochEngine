"""Actual RTT allocator, device record and retirement bodies; no GPU/window."""
from pathlib import Path
import hashlib
import json
import unittest
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


def definition(source, start):
    begin = source.index(start)
    brace = source.index("{", begin)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


class RttContextOwnership(unittest.TestCase):
    def test_actual_two_device_and_two_context_lifetimes(self):
        device_path = ROOT / "Engine/modules/render.device_opengl_family.ixx"
        native_path = ROOT / "Engine/modules/opengl.textures.ixx"
        device = device_path.read_text(encoding="utf-8")
        native = native_path.read_text(encoding="utf-8")
        record_types = "\n".join(definition(device, "    struct " + name) + ";" for name in (
            "OpenGLFamilyRenderTextureRecord", "OpenGLFamilyNativeRenderTextureAllocation",
            "OpenGLFamilyNativeRenderTextureHooks"))
        native_types = "\n".join(definition(native, "    struct " + name) + ";" for name in (
            "NativeRenderTextureGPU", "NativeTextureKey final", "NativeTextureKeyHash final"))
        native_bodies = "\n".join(definition(native, marker) for marker in (
            "    inline void delete_native_render_texture_objects(",
            "    [[nodiscard]] inline epochengine::OpenGLFamilyNativeRenderTextureAllocation allocate_native_render_texture(",
            "    inline void destroy_native_render_texture(",
            "    [[nodiscard]] inline bool begin_native_render_texture_pass(",
            "    inline void end_native_render_texture_pass("))
        retirement = definition(native[native.index("    inline void clear_gpu_atlases("):],
            "            for (auto it = oglData->native_render_textures.begin();")
        create = definition(device, "        RenderTextureAssetHandles create_render_texture_asset(").replace(" override", "")
        allocate_slot = definition(device, "        [[nodiscard]] u32 allocate_render_texture_slot()")
        map_decl = native[native.index("        std::unordered_map<NativeTextureKey, NativeRenderTextureGPU,"):]
        map_decl = map_decl[:map_decl.index(";")+1]
        fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <source_location>
#include <string_view>
#include <unordered_map>
#include <vector>
using u32=std::uint32_t;using GLuint=u32;using GLenum=u32;using GLbitfield=u32;using GLsizei=int;
enum:unsigned{GL_FRAMEBUFFER=1,GL_TEXTURE_2D,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,GL_TEXTURE_MIN_FILTER,
 GL_TEXTURE_MAG_FILTER,GL_LINEAR,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE,GL_COLOR_ATTACHMENT0,
 GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,GL_DEPTH_STENCIL_ATTACHMENT,GL_TEXTURE_MIN_LOD,
 GL_COLOR_BUFFER_BIT=0x100,GL_DEPTH_BUFFER_BIT=0x200,GL_STENCIL_BUFFER_BIT=0x400,GL_FRAMEBUFFER_COMPLETE=0x500};
namespace mock {
 void* current=reinterpret_cast<void*>(1);bool activate=true,complete=true;
 std::map<const void*,GLuint> sequence;
 std::set<std::pair<const void*,GLuint>> alive;unsigned calls{},deletes{};GLuint bound{};
 void gen(int count,GLuint* ids){for(int n=0;n<count;++n){ids[n]=++sequence[current];alive.emplace(current,ids[n]);++calls;}}
 void destroy(int count,const GLuint* ids){for(int n=0;n<count;++n){assert(alive.erase({current,ids[n]})==1);++deletes;++calls;}}
}
void glGenFramebuffers(int n,GLuint* p){mock::gen(n,p);}void glGenTextures(int n,GLuint* p){mock::gen(n,p);}
void glGenRenderbuffers(int n,GLuint* p){mock::gen(n,p);}void glGenSamplers(int n,GLuint* p){mock::gen(n,p);}
void glDeleteFramebuffers(int n,const GLuint* p){mock::destroy(n,p);}void glDeleteTextures(int n,const GLuint* p){mock::destroy(n,p);}
void glDeleteRenderbuffers(int n,const GLuint* p){mock::destroy(n,p);}void glDeleteSamplers(int n,const GLuint* p){mock::destroy(n,p);}
void glBindFramebuffer(GLenum,GLuint value){++mock::calls;mock::bound=value;}
void glBindTexture(GLenum,GLuint){++mock::calls;}void glBindRenderbuffer(GLenum,GLuint){++mock::calls;}
void glTexImage2D(GLenum,int,GLenum,GLsizei,GLsizei,int,GLenum,GLenum,const void*){++mock::calls;}
void glTexParameteri(GLenum,GLenum,int){++mock::calls;}void glFramebufferTexture2D(GLenum,GLenum,GLenum,GLuint,int){++mock::calls;}
void glRenderbufferStorage(GLenum,GLenum,GLsizei,GLsizei){++mock::calls;}
void glFramebufferRenderbuffer(GLenum,GLenum,GLenum,GLuint){++mock::calls;}
void glSamplerParameteri(GLuint,GLenum,int){++mock::calls;}
GLenum glCheckFramebufferStatus(GLenum){++mock::calls;return mock::complete?GL_FRAMEBUFFER_COMPLETE:0;}
void glViewport(int,int,GLsizei,GLsizei){++mock::calls;}void glDrawBuffer(GLenum){++mock::calls;}
void glClearColor(float,float,float,float){++mock::calls;}void glClearDepth(double){++mock::calls;}
void glClear(GLbitfield){++mock::calls;}
namespace logger{template<class... T>void warnf_loc(T&&...){}}
namespace epochengine{
 enum class RendererBackendKind{opengl};
 struct RenderTextureAssetDesc{u32 width=128,height=64;bool has_depth=true,sampled_after_render=true;const char* debug_name{};};
 struct RenderTextureBackendRequirements{bool color_attachment=true,sampled_color=true,sampler=true,offscreen_target=true,presentable_surface=true,depth_attachment=true;};
 struct RenderTextureAssetPlan{RenderTextureBackendRequirements backend_requirements;};
 RenderTextureAssetPlan make_render_texture_asset_plan(const RenderTextureAssetDesc& desc){RenderTextureAssetPlan p;p.backend_requirements.depth_attachment=desc.has_depth;p.backend_requirements.sampler=desc.sampled_after_render;return p;}
 struct RenderPassDesc{bool clear_color=true,clear_depth=true;float clear[4]{};};
 struct Handle{u32 value{};explicit operator bool()const{return value!=0;}};
 struct RenderTextureAssetHandles{Handle color_texture,sampler,render_target;};
 RECORD_TYPES
 namespace openglcontext::PlatformGL{
 struct Context{const void* context{};};
 struct ScopedContext{Context target()const{return {mock::current};}};
 }
}
namespace actual{
 NATIVE_TYPES
 struct BackendData{std::mutex gpuMutex;MAP_DECL};
 BackendData& resolve_backend_data(void* user){return *static_cast<BackendData*>(user);}
 bool activate_backend_context(BackendData&,epochengine::openglcontext::PlatformGL::ScopedContext&,std::string_view){return mock::activate&&mock::current;}
 const void* platform_context_key(epochengine::openglcontext::PlatformGL::Context c){return c.context;}
 NATIVE_BODIES
 void retire(BackendData* oglData,const void* retiringKey){std::lock_guard lock(oglData->gpuMutex);RETIREMENT}
}
struct Device{
 epochengine::RendererBackendKind m_backend=epochengine::RendererBackendKind::opengl;
 std::vector<epochengine::OpenGLFamilyRenderTextureRecord> m_render_textures;
 epochengine::OpenGLFamilyNativeRenderTextureHooks m_native_hooks;
 using RenderTextureAssetHandles=epochengine::RenderTextureAssetHandles;
 using RenderTextureAssetDesc=epochengine::RenderTextureAssetDesc;
 using RenderTextureAssetPlan=epochengine::RenderTextureAssetPlan;
 using OpenGLFamilyRenderTextureRecord=epochengine::OpenGLFamilyRenderTextureRecord;
 using OpenGLFamilyNativeRenderTextureAllocation=epochengine::OpenGLFamilyNativeRenderTextureAllocation;
 using TextureHandle=epochengine::Handle;using SamplerHandle=epochengine::Handle;using RenderTargetHandle=epochengine::Handle;
 CREATE
 ALLOCATE_SLOT
};
int main(){
 using namespace epochengine;using namespace actual;
 const auto backendKind=RendererBackendKind::opengl;
 BackendData backend;
 auto hooks=OpenGLFamilyNativeRenderTextureHooks{&backend,&allocate_native_render_texture,&destroy_native_render_texture,&begin_native_render_texture_pass,&end_native_render_texture_pass};
 Device first{backendKind,{},hooks},second{backendKind,{},hooks},other{backendKind,{},hooks};
 const auto a=first.create_render_texture_asset({});const auto recordA=first.m_render_textures[0];
 const auto b=second.create_render_texture_asset({});const auto recordB=second.m_render_textures[0];
 assert(a.render_target.value==1&&b.render_target.value==1);
 assert(recordA.color_object!=recordB.color_object&&backend.native_render_textures.size()==2&&mock::deletes==0);
 assert(recordA.native_context_key==mock::current&&recordA.native_allocation_ready);
 assert(begin_native_render_texture_pass(&backend,backendKind,recordA,{}));
 end_native_render_texture_pass(&backend,backendKind,recordA);assert(mock::bound==0);
 auto forged=recordA;++forged.framebuffer_object;auto calls=mock::calls;
 assert(!begin_native_render_texture_pass(&backend,backendKind,forged,{}));
 destroy_native_render_texture(&backend,backendKind,forged);assert(mock::calls==calls&&backend.native_render_textures.size()==2);
 void* contextA=mock::current;mock::current=reinterpret_cast<void*>(2);
 const auto c=other.create_render_texture_asset({});const auto recordC=other.m_render_textures[0];
 assert(c.render_target.value==1&&recordC.color_object==recordA.color_object&&backend.native_render_textures.size()==3);
 calls=mock::calls;assert(!begin_native_render_texture_pass(&backend,backendKind,recordA,{}));
 destroy_native_render_texture(&backend,backendKind,recordA);end_native_render_texture_pass(&backend,backendKind,recordA);
 assert(mock::calls==calls&&backend.native_render_textures.size()==3);
 assert(begin_native_render_texture_pass(&backend,backendKind,recordC,{}));
 retire(&backend,mock::current);assert(backend.native_render_textures.size()==2);
 calls=mock::calls;assert(!begin_native_render_texture_pass(&backend,backendKind,recordC,{}));assert(mock::calls==calls);
 mock::current=contextA;assert(begin_native_render_texture_pass(&backend,backendKind,recordB,{}));
 destroy_native_render_texture(&backend,backendKind,recordA);assert(backend.native_render_textures.size()==1);
 assert(begin_native_render_texture_pass(&backend,backendKind,recordB,{}));
 destroy_native_render_texture(&backend,backendKind,recordA);assert(backend.native_render_textures.size()==1);
 mock::complete=false;auto failed=allocate_native_render_texture(&backend,backendKind,{},RenderTextureBackendRequirements{},1);
 assert(!failed.ready&&backend.native_render_textures.size()==1);mock::complete=true;
 mock::activate=false;calls=mock::calls;assert(!allocate_native_render_texture(&backend,backendKind,{},RenderTextureBackendRequirements{},1).ready);
 assert(!begin_native_render_texture_pass(&backend,backendKind,recordB,{}));assert(mock::calls==calls);mock::activate=true;
 retire(&backend,contextA);assert(backend.native_render_textures.empty()&&mock::alive.empty());
 calls=mock::calls;destroy_native_render_texture(&backend,backendKind,recordB);assert(mock::calls==calls);
 return 0;
}
'''
        for marker, body in (("RECORD_TYPES", record_types), ("NATIVE_TYPES", native_types),
                             ("MAP_DECL", map_decl), ("NATIVE_BODIES", native_bodies),
                             ("RETIREMENT", retirement), ("CREATE", create), ("ALLOCATE_SLOT", allocate_slot)):
            fixture = fixture.replace(marker, body)
        identity = hashlib.sha256(fixture.encode()).hexdigest()[:12]
        report = ROOT / "Artifacts/Reports/rtt-context-ownership-nov02" / identity
        report.mkdir(parents=True, exist_ok=True)
        source = report / "rtt_context_contract.cpp"
        source.write_text(fixture, encoding="utf-8")
        runs = compile_and_run(self, source, report)
        (report / "result.json").write_text(json.dumps({"pass": True, "native": False,
            "scope": "Actual device allocator, native RTT hooks and context retirement loop with GL/context ports; two devices in one context and repeated object IDs in two contexts.",
            "runs": runs, "production_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (device_path, native_path)}}, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
