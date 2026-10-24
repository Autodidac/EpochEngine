"""Compile actual ordered sprite submission against a recording GL driver."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def definition(source, marker, semicolon=False):
    start = source.index(marker)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + (';' if semicolon else '')


class SpriteStream(unittest.TestCase):
    def test_actual_geometry_order_capacity_upload_and_context_lifetime(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((sdk / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        source = (ROOT / 'Engine/modules/opengl.textures.ixx').read_text(encoding='utf-8')
        structures = '\n'.join(definition(source, '    struct '+name+' final', True)
                               for name in ('SpriteVertex', 'SpriteBatchTexture', 'SpriteBatchState'))
        functions = '\n'.join(definition(source, marker) for marker in (
            '    inline void release_sprite_stream(', '    [[nodiscard]] inline bool ensure_sprite_stream(',
            '    inline void flush_sprite_batch(', '    inline void append_streamed_sprite(',
            '    inline void submit_resolved_sprite(', '    inline void end_sprite_batch(',
            '    inline void draw_sprite_in_active_batch('))
        # These are the production functions, not another implementation of
        # their ordering/geometry algorithm. Only native GL/atlas admission is fake.
        prefix = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <span>
#include <vector>
using GLuint=unsigned; using GLenum=unsigned; using GLint=int; using GLsizei=int;
using GLsizeiptr=std::ptrdiff_t;
constexpr unsigned GL_ARRAY_BUFFER=1,GL_FLOAT=2,GL_FALSE=0,GL_STREAM_DRAW=3,
 GL_TEXTURE_2D=4,GL_TRIANGLES=5,GL_BLEND=6,GL_TEXTURE_MIN_FILTER=7,
 GL_TEXTURE_MAG_FILTER=8,GL_NEAREST=9,GL_TEXTURE_WRAP_S=10,GL_TEXTURE_WRAP_T=11,
 GL_CLAMP_TO_EDGE=12,GL_UNSIGNED_INT=13;
namespace epochengine::openglstate { struct OpenGL4State {
 GLuint spriteStreamVao{},spriteStreamVbo{},shader=15;
 bool spriteStreamUnavailable{};
 int uUVRegionLoc=1,uTransformLoc=2;
 std::uint64_t guiSpritesSubmitted{},guiDrawCallsSubmitted{};
}; }
namespace core { struct Context {}; }
namespace epochengine::openglcontext::PlatformGL { struct PlatformGLContext {}; }
namespace epochengine::openglquad {
 inline epochengine::openglstate::OpenGL4State* current{};
 auto& quad_pipeline_state(){return *current;}
}
struct AtlasRegion { float u1=.2f,v1=.3f,u2=.8f,v2=.9f; };
struct TextureAtlas { std::uint64_t version=1; AtlasRegion region{};
 bool try_get_entry_info(int index,AtlasRegion& out) const {
  if(index<0) return false; out=region; return true;
 }};
struct SpriteHandle { unsigned atlasIndex{},localIndex{}; };
struct AtlasGPU { std::uint64_t version{}; GLuint textureHandle{}; };
struct BackendData {std::mutex gpuMutex; std::map<const TextureAtlas*,AtlasGPU> textures;};
auto& atlas_map_for_platform_context(BackendData& b, auto){return b.textures;}
inline std::vector<unsigned> events;
void ensure_uploaded_for_context(BackendData& b,auto&,auto,const TextureAtlas& a){
 events.push_back(1000u);
 auto& t=b.textures[&a]; if(!t.textureHandle)t.textureHandle=unsigned(b.textures.size());
 t.version=a.version;
}
inline unsigned nextHandle=100,created{},deleted{},uniforms{},currentTexture{},currentVao{},currentVbo{};
inline unsigned failHandleAfter=~0u;
inline std::vector<float> uploaded;
struct Draw {unsigned texture{},vao{};int count{};std::vector<float> data;};
inline std::vector<Draw> draws;
inline auto glGenVertexArrays=+[](int,GLuint* h){*h=created++>=failHandleAfter?0:nextHandle++;};
inline auto glGenBuffers=+[](int,GLuint* h){*h=created++>=failHandleAfter?0:nextHandle++;};
inline auto glDeleteVertexArrays=+[](int,const GLuint*){++deleted;};
inline auto glDeleteBuffers=+[](int,const GLuint*){++deleted;};
inline auto glBindVertexArray=+[](GLuint h){currentVao=h;};
inline auto glBindBuffer=+[](GLenum,GLuint h){currentVbo=h;};
inline auto glBufferData=+[](GLenum,GLsizeiptr n,const void* p,GLenum){
 assert(n==512*6*4*sizeof(float)); assert(!p); assert(currentVbo);
};
inline auto glBufferSubData=+[](GLenum,GLsizeiptr offset,GLsizeiptr n,const void* p){
 assert(!offset); assert(n>0 && n<=512*6*4*sizeof(float));
 uploaded.resize(n/sizeof(float)); std::memcpy(uploaded.data(),p,n);
};
inline auto glDrawArrays=+[](GLenum,int start,int count){
 assert(!start); assert(currentTexture && currentVao); events.push_back(currentTexture);
 draws.push_back({currentTexture,currentVao,count,uploaded});
};
inline auto glDrawElements=+[](GLenum,int count,GLenum,const void*){
 draws.push_back({currentTexture,currentVao,count,{}});
};
inline auto glUseProgram=+[](GLuint){};
inline auto glBindTexture=+[](GLenum,GLuint h){currentTexture=h;};
inline auto glUniform4f=+[](GLint,float,float,float,float){++uniforms;};
inline auto glEnableVertexAttribArray=+[](GLuint){};
inline auto glVertexAttribPointer=+[](GLuint,int,GLenum,bool,int,const void*){};
inline auto glDisable=+[](GLenum){};
inline auto glTexParameteri=+[](GLenum,GLenum,int){};
inline constexpr std::size_t sprite_stream_capacity=512u;
'''
        main = r'''
void reset(epochengine::openglstate::OpenGL4State& state,BackendData& backend) {
 sprite_batch_state={}; auto& b=sprite_batch_state;
 b.gl_state=&state; b.backend=&backend; b.framebuffer_width=1280;b.framebuffer_height=800;
 b.streaming=ensure_sprite_stream(state); b.active=true;
 epochengine::openglquad::current=&state;
 draws.clear(); events.clear(); uniforms=0;
}
int main() {
 epochengine::openglstate::OpenGL4State state{},other{}; BackendData backend;
 TextureAtlas a,b; const TextureAtlas* atlas[]={&a,&b};
 reset(state,backend);
 for(unsigned i:{0u,0u,1u,0u}) draw_sprite_in_active_batch({i,0},atlas,10,20,30,40);
 end_sprite_batch();
 assert(draws.size()==3 && draws[0].count==12 && draws[1].count==6 && draws[2].count==6);
 assert(draws[0].texture==draws[2].texture && draws[0].texture!=draws[1].texture);
 assert(events==std::vector<unsigned>({1000,1,1000,2,1}));
 assert(uniforms==6 && state.guiSpritesSubmitted==4 && state.guiDrawCallsSubmitted==3);
 const auto& v=draws[0].data;
 auto near=[](float a,float b){return std::abs(a-b)<0.00001f;};
 assert(near(v[0],10.f*2/1280-1) && near(v[1],1-60.f*2/800));
 assert(near(v[2],.2f) && near(v[3],.7f));
 assert(near(v[8],40.f*2/1280-1) && near(v[9],1-20.f*2/800));
 assert(near(v[10],.8f) && near(v[11],.1f));
 assert(v[12]==v[8] && v[13]==v[9] && v[20]==v[0] && v[21]==v[1]);
 // Exact capacity boundaries, no tail loss and no additional allocations of GL objects.
 const auto objects=created; reset(state,backend);
 for(unsigned i=0;i<1025;++i) draw_sprite_in_active_batch({0,0},atlas,float(i),0,1,1);
 end_sprite_batch();
 assert(created==objects && draws.size()==3);
 assert(draws[0].count==3072 && draws[1].count==3072 && draws[2].count==6);
 assert(near(draws[2].data[0],1024.f*2/1280-1));
 // Atlas version updates must follow all draws using its preceding bytes.
 reset(state,backend); draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);
 ++a.version; draw_sprite_in_active_batch({0,0},atlas,0,0,1,1); end_sprite_batch();
 assert(events==std::vector<unsigned>({1000,1,1000,1}));
 // Distinct contexts have distinct VAOs/VBOs. Invalid input submits nothing.
 const auto firstVao=state.spriteStreamVao; reset(other,backend);
 assert(other.spriteStreamVao!=firstVao);
 draw_sprite_in_active_batch({99,0},atlas,0,0,1,1); end_sprite_batch(); assert(draws.empty());
 auto before=deleted; release_sprite_stream(state,false);
 assert(deleted==before && !state.spriteStreamVao && !state.spriteStreamVbo);
 release_sprite_stream(other,true); assert(deleted==before+2);
 // Missing native capability retains the existing immediate-quad path.
 auto saved=glBufferSubData; glBufferSubData=nullptr;
 reset(state,backend); assert(!sprite_batch_state.streaming);
 draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);end_sprite_batch();
 assert(draws.size()==1 && draws[0].count==6); glBufferSubData=saved;
 // Partial creation retires the one allocated object and stops retrying.
 state.spriteStreamUnavailable=false; failHandleAfter=created+1;
 assert(!ensure_sprite_stream(state)); const auto attempts=created;
 assert(!ensure_sprite_stream(state) && created==attempts);
 return 0;
}
'''
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='sprite-stream-', dir=parent))
        try:
            toolset, kit = toolsets[-1], kits[-1]
            cpp = scratch / 'contract.cpp'
            cpp.write_text(prefix + structures + '\ninline thread_local SpriteBatchState sprite_batch_state{};\n'
                           + functions + main, encoding='utf-8')
            binary = scratch / 'contract.exe'
            args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc',
                    '/I'+str(toolset/'include'), '/I'+str(kit/'ucrt'), '/Fo'+str(scratch/'contract.obj'),
                    '/Fe'+str(binary), str(cpp), '/link', '/LIBPATH:'+str(toolset/'lib/x64'),
                    '/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'), '/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
            result = subprocess.run(args, cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-5000:]+result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
