"""Compile actual ordered sprite submission against a recording GL driver."""
from pathlib import Path
import json
import unittest
import xml.etree.ElementTree as ET

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
    def test_diagnostic_define_is_private_to_process_implementation(self):
        namespace = '{http://schemas.microsoft.com/developer/msbuild/2003}'
        shared = ET.parse(ROOT/'Engine/Engine.vcxitems')
        define = 'EPOCH_OPENGL_STATIC_QUADS_DIAGNOSTIC'
        owners = [item for item in shared.iter(namespace+'ClCompile')
                  if any(define in (child.text or '') for child in item)]
        self.assertEqual(len(owners), 1)
        self.assertTrue(owners[0].attrib['Include'].endswith('opengl.context_process_impl.cpp'))
        props = ET.parse(ROOT/'Tools/MSVC/EpochDiagnostics.props')
        for defaults in props.iter(namespace+'ItemDefinitionGroup'):
            self.assertNotIn(define, ''.join(defaults.itertext()))
        self.assertEqual(props.find('.//'+namespace+'EpochOpenGLStaticQuads').text, 'false')

    def test_actual_geometry_order_capacity_upload_and_context_lifetime(self):
        from Tests.support.cpp import compile_and_run
        source=(ROOT/'Engine/modules/opengl.textures.ixx').read_text(encoding='utf-8')
        state=(ROOT/'Engine/modules/opengl.state.ixx').read_text(encoding='utf-8')
        fields='        GLuint spriteStreamVao'+state.split('        GLuint spriteStreamVao',1)[1].split('        GLuint sceneShader',1)[0]
        constants='\n'.join(line for line in source.splitlines() if 'inline constexpr std::size_t sprite_stream_' in line)
        structures='\n'.join(definition(source,'    struct '+name+' final',True) for name in ('SpriteVertex','SpriteBatchTexture','SpriteBatchState'))
        functions='\n'.join(definition(source,marker) for marker in (
            '    inline void prepare_sprite_frame(', '    inline void release_sprite_stream(',
            '    [[nodiscard]] inline bool ensure_sprite_stream(', '    inline void flush_sprite_batch(',
            '    inline void append_streamed_sprite(', '    inline void submit_resolved_sprite(',
            '    [[nodiscard]] inline bool begin_sprite_batch(', '    inline void end_sprite_batch(',
            '    inline void draw_sprite_in_active_batch('))
        process=(ROOT/'Engine/src/renderers/opengl/opengl.context_process_impl.cpp').read_text(encoding='utf-8')
        self.assertLess(process.index('frame_admission::admit('),process.index('opengltextures::prepare_sprite_frame(nativeState);'))
        self.assertLess(process.index('opengltextures::prepare_sprite_frame(nativeState);'),process.index('nativeState.guiSpritesSubmitted ='))
        self.assertLess(process.index('opengl_render_active_frame(ctx, queue'),process.index('nativeState.spriteStreamFramePrepared = false;'))
        gate_start=process.index('#if !defined(EPOCH_OPENGL_STATIC_QUADS_DIAGNOSTIC)')
        gate=process[gate_start:process.index('#endif',gate_start)+len('#endif')]
        gate=gate.replace('opengltextures::prepare_sprite_frame','prepare_sprite_frame')
        controls=('\nvoid prepare_default_control(epochengine::openglstate::OpenGL4State& nativeState) {\n'
                  +gate+'\n}\n#define EPOCH_OPENGL_STATIC_QUADS_DIAGNOSTIC 1\n'
                  +'void prepare_static_control(epochengine::openglstate::OpenGL4State& nativeState) {\n'
                  +gate+'\n}\n#undef EPOCH_OPENGL_STATIC_QUADS_DIAGNOSTIC\n')
        quad=(ROOT/'Engine/modules/opengl.quad.ixx').read_text(encoding='utf-8')
        quad_arrays='\n'.join(quad[quad.index(marker):quad.index(';',quad.index(marker))+1]
                              for marker in ('constexpr float quadVerts[]', 'constexpr unsigned int quadIdx[]'))
        prefix=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <iterator>
#include <map>
#include <mutex>
#include <span>
#include <vector>
using GLuint=unsigned; using GLenum=unsigned; using GLint=int; using GLsizei=int;
using GLsizeiptr=std::ptrdiff_t;
constexpr unsigned GL_ARRAY_BUFFER=1,GL_FLOAT=2,GL_FALSE=0,GL_STREAM_DRAW=3,
 GL_TEXTURE_2D=4,GL_TRIANGLES=5,GL_BLEND=6,GL_TEXTURE_MIN_FILTER=7,
 GL_TEXTURE_MAG_FILTER=8,GL_NEAREST=9,GL_TEXTURE_WRAP_S=10,GL_TEXTURE_WRAP_T=11,
 GL_CLAMP_TO_EDGE=12,GL_UNSIGNED_INT=13,GL_SCISSOR_TEST=14,GL_DEPTH_TEST=15,
 GL_CULL_FACE=16,GL_SRC_ALPHA=17,GL_ONE_MINUS_SRC_ALPHA=18,GL_TEXTURE0=19;
namespace epochengine::openglstate { struct OpenGL4State {
 int nativeTag{},width=1280,height=800; GLuint vao{},shader=15;
 int uUVRegionLoc=1,uTransformLoc=2;
'''+fields+r'''
}; }
namespace core { struct Context { int nativeTag{},framebufferWidth=1280,framebufferHeight=800; }; }
namespace epochengine::openglcontext::PlatformGL {
 struct PlatformGLContext {int identity{};bool valid() const {return identity>0;}
  friend bool operator==(PlatformGLContext,PlatformGLContext)=default;};
 inline PlatformGLContext current;
 auto get_current(){return current;}
}
namespace epochengine::openglquad {
 inline epochengine::openglstate::OpenGL4State* current{};
 auto& quad_pipeline_state(){return *current;}
}
namespace detail {
 auto context_to_platform_context(const core::Context* c){return epochengine::openglcontext::PlatformGL::PlatformGLContext{c?c->nativeTag:0};}
}
struct AtlasRegion {float u1=.2f,v1=.3f,u2=.8f,v2=.9f;};
struct TextureAtlas {std::uint64_t version=1;AtlasRegion region{};
 bool try_get_entry_info(int index,AtlasRegion& out) const {if(index<0)return false;out=region;return true;}};
struct SpriteHandle {unsigned atlasIndex{},localIndex{};};
struct AtlasGPU {std::uint64_t version{};GLuint textureHandle{};};
struct BackendData {std::mutex gpuMutex;std::map<int,std::map<const TextureAtlas*,AtlasGPU>> textures;
 std::map<int,epochengine::openglstate::OpenGL4State*> states;};
BackendData backend;
auto& get_opengl_backend(){return backend;}
auto& atlas_map_for_platform_context(BackendData& b,auto c){return b.textures[c.identity];}
auto& state_for_platform_context(BackendData& b,auto c,const core::Context* ctx){assert(c.identity==ctx->nativeTag);return *b.states.at(c.identity);}
bool ensure_created_pipeline(epochengine::openglstate::OpenGL4State& state){epochengine::openglquad::current=&state;return true;}
inline std::vector<unsigned> events;
void ensure_uploaded_for_context(BackendData& b,auto&,auto c,const TextureAtlas& a){
 events.push_back(1000);auto& map=b.textures[c.identity];auto& t=map[&a];
 if(!t.textureHandle)t.textureHandle=unsigned(map.size());t.version=a.version;
}
inline unsigned nextHandle=100,created{},deleted{},uniforms{},currentTexture{},currentVao{},currentVbo{},frame{};
inline unsigned failHandleAfter=~0u;
struct Store {int owner{};std::vector<float> vertices;};
std::map<unsigned,int> vaos;std::map<unsigned,Store> buffers;
struct Allocation {unsigned frame,buffer;std::size_t bytes;};std::vector<Allocation> allocations;
struct Write {unsigned frame,buffer;std::size_t offset,bytes;};std::vector<Write> writes;
struct Draw {unsigned texture,vao;int first,count;bool immediate;std::vector<float> data;};
std::vector<Draw> draws;
std::array<float,4> uv{},transform{};
inline auto glGenVertexArrays=+[](int,GLuint* h){*h=created++>=failHandleAfter?0:nextHandle++;if(*h)vaos[*h]=epochengine::openglcontext::PlatformGL::current.identity;};
inline auto glGenBuffers=+[](int,GLuint* h){*h=created++>=failHandleAfter?0:nextHandle++;if(*h)buffers[*h].owner=epochengine::openglcontext::PlatformGL::current.identity;};
inline auto glDeleteVertexArrays=+[](int,const GLuint* h){assert(vaos.at(*h)==epochengine::openglcontext::PlatformGL::current.identity);vaos.erase(*h);++deleted;};
inline auto glDeleteBuffers=+[](int,const GLuint* h){assert(buffers.at(*h).owner==epochengine::openglcontext::PlatformGL::current.identity);buffers.erase(*h);++deleted;};
inline auto glBindVertexArray=+[](GLuint h){currentVao=h;};
inline auto glBindBuffer=+[](GLenum,GLuint h){currentVbo=h;};
inline auto glBufferData=+[](GLenum,GLsizeiptr n,const void* p,GLenum){
 assert(n>=8192*6*16 && n<=65536*6*16 && !p);assert(currentVbo);
 auto& s=buffers.at(currentVbo);assert(s.owner==epochengine::openglcontext::PlatformGL::current.identity);
 assert(std::none_of(allocations.begin(),allocations.end(),[](const auto& a){return a.frame==frame && a.buffer==currentVbo;}));
 s.vertices.assign(n/sizeof(float),0);allocations.push_back({frame,currentVbo,std::size_t(n)});
};
inline auto glBufferSubData=+[](GLenum,GLsizeiptr offset,GLsizeiptr n,const void* p){
 assert(offset>=0 && n>0 && n<=512*6*16);auto& s=buffers.at(currentVbo);
 assert(std::size_t(offset+n)<=s.vertices.size()*sizeof(float));
 for(const auto& w:writes)if(w.frame==frame && w.buffer==currentVbo)assert(std::size_t(offset)>=w.offset+w.bytes);
 std::memcpy(reinterpret_cast<char*>(s.vertices.data())+offset,p,n);writes.push_back({frame,currentVbo,std::size_t(offset),std::size_t(n)});
};
inline auto glDrawArrays=+[](GLenum,int first,int count){
 const auto& s=buffers.at(currentVbo);assert(first>=0 && count>0 && (first+count)%6==0);
 assert(s.owner==epochengine::openglcontext::PlatformGL::current.identity && vaos.at(currentVao)==s.owner);
 assert(std::size_t(first+count)*4<=s.vertices.size());events.push_back(currentTexture);
 draws.push_back({currentTexture,currentVao,first,count,false,{s.vertices.begin()+first*4,s.vertices.begin()+(first+count)*4}});
};
inline auto glDrawElements=+[](GLenum,int count,GLenum,const void*){
 assert(currentVao==epochengine::openglquad::current->vao);assert(count==6);events.push_back(currentTexture);
 draws.push_back({currentTexture,currentVao,0,count,true,{uv[0],uv[1],uv[2],uv[3],transform[0],transform[1],transform[2],transform[3]}});
};
inline auto glUseProgram=+[](GLuint){};
inline auto glBindTexture=+[](GLenum,GLuint h){currentTexture=h;};
inline auto glUniform4f=+[](GLint location,float x,float y,float z,float w){++uniforms;(location==1?uv:transform)={x,y,z,w};};
inline auto glEnableVertexAttribArray=+[](GLuint){};
inline auto glVertexAttribPointer=+[](GLuint,int,GLenum,bool,int,const void*){};
inline auto glDisable=+[](GLenum){};
inline auto glEnable=+[](GLenum){};
inline auto glTexParameteri=+[](GLenum,GLenum,int){};
inline auto glViewport=+[](int,int,int,int){};
inline auto glBlendFunc=+[](GLenum,GLenum){};
inline auto glActiveTexture=+[](GLenum){};
'''
        main=r'''
void start_frame(epochengine::openglstate::OpenGL4State& s,core::Context& c){
 assert(!sprite_batch_state.active);++frame;epochengine::openglcontext::PlatformGL::current={c.nativeTag};
 backend.states[c.nativeTag]=&s;prepare_sprite_frame(s);s.guiSpritesSubmitted=s.guiDrawCallsSubmitted=0;
 draws.clear();events.clear();writes.clear();allocations.clear();uniforms=0;
}
void finish_frame(epochengine::openglstate::OpenGL4State& s){assert(!sprite_batch_state.active);s.spriteStreamFramePrepared=false;}
std::size_t sprites_drawn(){std::size_t n=0;for(const auto& d:draws)n+=d.count/6;return n;}
int main(){
 epochengine::openglstate::OpenGL4State state{},other{};state.nativeTag=1;state.vao=10;other.nativeTag=2;other.vao=20;
 core::Context c{1},foreign{2};TextureAtlas a,b;const TextureAtlas* atlas[]={&a,&b};
 start_frame(state,c);assert(allocations.empty()&&writes.empty()&&created==0);
 for(int i=0;i<2;++i){assert(begin_sprite_batch(&c));end_sprite_batch();}
 assert(allocations.empty()&&writes.empty()&&created==0);finish_frame(state);
 start_frame(state,c);assert(begin_sprite_batch(&c));
 for(unsigned i:{0u,0u,1u,0u})draw_sprite_in_active_batch({i,0},atlas,10,20,30,40);
 end_sprite_batch();assert(draws.size()==3 && draws[0].count==12 && draws[1].count==6 && draws[2].count==6);
 assert(draws[0].first==0 && draws[1].first==12 && draws[2].first==18);
 assert(events==std::vector<unsigned>({1000,1,1000,2,1}));
 assert(state.guiSpritesSubmitted==4 && state.guiDrawCallsSubmitted==3);
 auto near=[](float a,float b){return std::abs(a-b)<.00001f;};const auto v=draws[0].data;
 auto verify_unit_sequence=[&](unsigned expected){unsigned index=0;
  for(const auto& draw:draws)for(int sprite=0;sprite<draw.count/6;++sprite,++index){
   if(draw.immediate){assert(near(draw.data[4],((float(index)+.5f)/1280)*2-1));
    assert(near(draw.data[5],((800.f-.5f)/800)*2-1));
    assert(near(draw.data[6],2.f/1280)&&near(draw.data[7],2.f/800));
    assert(near(draw.data[0],.2f)&&near(draw.data[1],.7f)&&near(draw.data[2],.6f)&&near(draw.data[3],-.6f));
   }else{const auto at=std::size_t(sprite)*24;
    assert(near(draw.data[at],float(index)*2/1280-1)&&near(draw.data[at+1],1-2.f/800));
    assert(near(draw.data[at+4],(float(index)+1)*2/1280-1)&&near(draw.data[at+9],1));
    assert(near(draw.data[at+2],.2f)&&near(draw.data[at+3],.7f)&&near(draw.data[at+10],.8f)&&near(draw.data[at+11],.1f));
   }
  }assert(index==expected);
 };
 assert(near(v[0],10.f*2/1280-1)&&near(v[1],1-60.f*2/800));
 assert(near(v[2],.2f)&&near(v[3],.7f)&&near(v[8],40.f*2/1280-1)&&near(v[9],1-20.f*2/800));
 assert(near(v[10],.8f)&&near(v[11],.1f)&&v[12]==v[8]&&v[20]==v[0]);
 // Second protected deferred replay plus top layer shares the same native frame store.
 assert(begin_sprite_batch(&c));for(unsigned i:{0u,0u,1u,0u,1u,0u})draw_sprite_in_active_batch({i,0},atlas,-10,5,20,30);
 end_sprite_batch();assert(state.guiSpritesSubmitted==10 && state.guiDrawCallsSubmitted==8 && sprites_drawn()==10);
 assert(allocations.size()==1 && allocations[0].bytes==8192*96 && writes.size()==8);
 assert(draws[3].first==24 && near(draws[3].data[0],-10.f*2/1280-1));
 const auto objects=created,buffer=state.spriteStreamVbo,vao=state.spriteStreamVao;finish_frame(state);
 // CPU staging remains512 sprites, preserving exact tail data and native objects.
 start_frame(state,c);assert(begin_sprite_batch(&c));for(unsigned i=0;i<1025;++i)draw_sprite_in_active_batch({0,0},atlas,float(i),0,1,1);
 end_sprite_batch();assert(created==objects&&allocations.size()==1&&draws.size()==3);
 assert(draws[0].count==3072&&draws[1].first==3072&&draws[2].first==6144&&draws[2].count==6);
 assert(near(draws[2].data[0],1024.f*2/1280-1));finish_frame(state);
 // Retained workload shape:5630sprites in182ordered texture runs across both passes.
 start_frame(state,c);unsigned placed=0;
 for(unsigned pass=0;pass<2;++pass){assert(begin_sprite_batch(&c));
  for(unsigned run=pass*91;run<(pass+1)*91;++run){
   const unsigned count=run<170?31:30;
   for(unsigned i=0;i<count;++i)draw_sprite_in_active_batch({run%2,0},atlas,float(placed++),0,1,1);
  }end_sprite_batch();}
 std::size_t bytesWritten=0;for(const auto& write:writes)bytesWritten+=write.bytes;
 assert(placed==5630&&sprites_drawn()==5630&&draws.size()==182&&writes.size()==182&&bytesWritten==540480);
 assert(allocations.size()==1&&allocations[0].bytes==786432);finish_frame(state);
 // Atlas mutation submits old vertices before upload of its replacement bytes.
 start_frame(state,c);assert(begin_sprite_batch(&c));draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);
 ++a.version;draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);end_sprite_batch();
 assert(events==std::vector<unsigned>({1000,1,1000,1})&&allocations.size()==1);finish_frame(state);
 // Every excess sprite and every later batch falls back without overwriting the frame's stream.
 start_frame(state,c);assert(begin_sprite_batch(&c));for(unsigned i=0;i<8195;++i)draw_sprite_in_active_batch({0,0},atlas,float(i),0,1,1);
 end_sprite_batch();assert(sprites_drawn()==8195&&allocations.size()==1&&draws.size()==19);
 assert(std::count_if(draws.begin(),draws.end(),[](const auto& d){return d.immediate;})==3);
 assert(near(draws.back().data[0],.2f)&&near(draws.back().data[1],.7f));
 assert(near(draws.back().data[4],((8194.f+.5f)/1280)*2-1));
 assert(near(draws.back().data[5],((800.f-.5f)/800)*2-1));
 assert(near(draws.back().data[6],2.f/1280)&&near(draws.back().data[7],2.f/800));
 verify_unit_sequence(8195);
 assert(begin_sprite_batch(&c));for(int i=0;i<2;++i)draw_sprite_in_active_batch({1,0},atlas,0,0,1,1);end_sprite_batch();
 assert(sprites_drawn()==8197&&allocations.size()==1&&draws.back().immediate);finish_frame(state);
 start_frame(state,c);assert(state.spriteStreamCapacitySprites==16384&&allocations.empty());
 assert(begin_sprite_batch(&c));draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);end_sprite_batch();
 assert(allocations.size()==1&&allocations[0].bytes==16384*96&&state.spriteStreamVbo==buffer&&state.spriteStreamVao==vao);finish_frame(state);
 // Explicit6MiB storage ceiling does not limit content: all above-budget sprites remain visible.
 state.guiSpritesSubmitted=(std::numeric_limits<std::uint64_t>::max)();state.spriteStreamCapacitySprites=65535;
 start_frame(state,c);assert(state.spriteStreamCapacitySprites==65536&&allocations.empty());
 assert(begin_sprite_batch(&c));for(unsigned i=0;i<65538;++i)draw_sprite_in_active_batch({0,0},atlas,float(i),0,1,1);
 end_sprite_batch();assert(sprites_drawn()==65538&&allocations.size()==1&&allocations[0].bytes==65536*96);
 verify_unit_sequence(65538);
 assert(draws.back().immediate&&draws[draws.size()-2].immediate);finish_frame(state);
 // Empty next native frame retains high-water metadata but allocates/uploads nothing.
 start_frame(state,c);assert(begin_sprite_batch(&c));end_sprite_batch();assert(allocations.empty()&&writes.empty());finish_frame(state);
 // Distinct native context identities own distinct buffers. Foreign/missing current owners refuse batching.
 start_frame(other,foreign);assert(!begin_sprite_batch(&c)&&allocations.empty()&&writes.empty());
 epochengine::openglcontext::PlatformGL::current={};assert(!begin_sprite_batch(&foreign));
 epochengine::openglcontext::PlatformGL::current={2};assert(begin_sprite_batch(&foreign));
 draw_sprite_in_active_batch({99,0},atlas,0,0,1,1);end_sprite_batch();assert(allocations.empty()&&writes.empty());
 assert(begin_sprite_batch(&foreign));draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);end_sprite_batch();
 assert(other.spriteStreamVbo!=buffer&&other.spriteStreamVao!=vao&&allocations.size()==1);finish_frame(other);
 auto before=deleted;release_sprite_stream(state,false);
 assert(deleted==before&&!state.spriteStreamVbo&&!state.spriteStreamVao&&!state.spriteStreamCapacitySprites&&!state.spriteStreamFramePrepared);
 release_sprite_stream(other,true);assert(deleted==before+2&&!other.spriteStreamCapacitySprites);
 // Missing streaming capabilities keep all existing static-quad output and issue no buffer store.
 auto saved=glBufferSubData;glBufferSubData=nullptr;start_frame(state,c);assert(begin_sprite_batch(&c));
 draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);draw_sprite_in_active_batch({1,0},atlas,0,0,1,1);end_sprite_batch();
 assert(sprites_drawn()==2&&draws[0].immediate&&draws[1].immediate&&allocations.empty()&&writes.empty());finish_frame(state);glBufferSubData=saved;
 // Partial native object creation retires only allocated owner objects and preserves all sprites.
 state.spriteStreamUnavailable=false;failHandleAfter=created+1;start_frame(state,c);assert(begin_sprite_batch(&c));
 draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);end_sprite_batch();const auto attempts=created;
 assert(draws.size()==1&&draws[0].immediate&&allocations.empty());assert(!ensure_sprite_stream(state)&&created==attempts);finish_frame(state);
 // A batch outside an explicitly prepared native frame never reuses another frame's bytes.
 failHandleAfter=~0u;state.spriteStreamUnavailable=false;assert(begin_sprite_batch(&c));draw_sprite_in_active_batch({0,0},atlas,0,0,1,1);end_sprite_batch();
 assert(draws.back().immediate&&allocations.empty());
 // Compare the actual production diagnostic gate, not a replacement submitter.
 // Canonical vertices combine recorded uniforms with the production static quad.
 struct SpriteProof {unsigned texture;std::array<float,24> vertices;};
 auto canonical=[](){std::vector<SpriteProof> result;
  for(const auto& draw:draws)for(int sprite=0;sprite<draw.count/6;++sprite){
   SpriteProof proof{draw.texture,{}};
   for(unsigned vertex=0;vertex<6;++vertex){const auto at=vertex*4;
    if(draw.immediate){const auto q=quadIdx[vertex]*4;
     proof.vertices[at]=quadVerts[q]*draw.data[6]+draw.data[4];
     proof.vertices[at+1]=quadVerts[q+1]*draw.data[7]+draw.data[5];
     proof.vertices[at+2]=quadVerts[q+2]*draw.data[2]+draw.data[0];
     proof.vertices[at+3]=quadVerts[q+3]*draw.data[3]+draw.data[1];
    }else for(unsigned component=0;component<4;++component)
     proof.vertices[at+component]=draw.data[std::size_t(sprite)*24+at+component];
   }result.push_back(proof);
  }return result;
 };
 auto workload=[&](bool staticQuads){
  epochengine::openglstate::OpenGL4State control{};control.nativeTag=staticQuads?4:3;control.vao=staticQuads?40:30;
  core::Context context{control.nativeTag};assert(!sprite_batch_state.active);++frame;
  epochengine::openglcontext::PlatformGL::current={context.nativeTag};backend.states[context.nativeTag]=&control;
  draws.clear();events.clear();writes.clear();allocations.clear();const auto objectsBefore=created;
  if(staticQuads)prepare_static_control(control);else prepare_default_control(control);
  assert(control.spriteStreamFramePrepared==!staticQuads);
  unsigned placed=0;
  auto runs=[&](unsigned first,unsigned last){for(unsigned run=first;run<last;++run){
   const unsigned count=run<170?31:30;
   for(unsigned i=0;i<count;++i,++placed)draw_sprite_in_active_batch({run%2,0},atlas,
    float(placed%83)*13.25f-5.f,float(placed%47)*9.5f,1.f+float(placed%11)*3.25f,1.f+float(placed%7)*2.5f);
  }};
  assert(begin_sprite_batch(&context));runs(0,91);end_sprite_batch();
  assert(placed==2821&&sprites_drawn()==2821); // First base replay completes before scene.
  events.push_back(2000);glBindVertexArray(0);glBindTexture(GL_TEXTURE_2D,999); // Scene changes native bindings.
  assert(begin_sprite_batch(&context));runs(91,171);assert(placed==5300); // Second base replay.
  runs(171,182);end_sprite_batch();assert(placed==5630&&sprites_drawn()==5630); // Top layer, same batch.
  assert(control.guiSpritesSubmitted==5630);
  if(staticQuads){assert(created==objectsBefore&&allocations.empty()&&writes.empty());
   assert(draws.size()==5630&&control.guiDrawCallsSubmitted==5630);
   assert(std::all_of(draws.begin(),draws.end(),[](const auto& draw){return draw.immediate;}));
  }else{assert(created==objectsBefore+2&&allocations.size()==1&&writes.size()==182);
   assert(draws.size()==182&&control.guiDrawCallsSubmitted==182);
  }
  auto proof=canonical();assert(proof.size()==5630);finish_frame(control);
  // A second frame selects the same mode; empty layers add no stream work.
  allocations.clear();writes.clear();const auto nextObjects=created;
  if(staticQuads)prepare_static_control(control);else prepare_default_control(control);
  assert(begin_sprite_batch(&context));end_sprite_batch();assert(created==nextObjects&&allocations.empty()&&writes.empty());
  finish_frame(control);release_sprite_stream(control,true);backend.states.erase(context.nativeTag);
  return proof;
 };
 const auto streamed=workload(false),immediate=workload(true);
 for(std::size_t sprite=0;sprite<streamed.size();++sprite){
  assert(streamed[sprite].texture==immediate[sprite].texture);
  for(unsigned component=0;component<24;++component)
   assert(near(streamed[sprite].vertices[component],immediate[sprite].vertices[component]));
 }
}
'''
        report=ROOT/'Artifacts/Intermediate/tests/opengl-frame-stream-reuse';report.mkdir(parents=True,exist_ok=True)
        cpp=report/'contract.cpp';cpp.write_text(prefix+constants+'\n'+structures+'\ninline thread_local SpriteBatchState sprite_batch_state{};\n'+functions+controls+'\n'+quad_arrays+main,encoding='utf-8')
        records=compile_and_run(self,cpp,report,compile_timeout=90,run_timeout=30)
        (report/'result.json').write_text(json.dumps({'passed':True,'configurations':records,
            'scope':'Actual ordered stream/frame owners and production static diagnostic gate with recorded GL calls; no GPU or native window.',
            'static_comparison':{'sprites':5630,'stream_draws':182,'static_draws':5630,
                'same_texture_geometry_uv_order':True,'static_stream_object_creations':0,
                'static_buffer_store_calls':0,'static_buffer_upload_calls':0,
                'atlas_uploads_excluded_from_zero_claim':True,'native_120_hz_feasibility':'not measured'}},indent=2)+'\n',encoding='utf-8')
