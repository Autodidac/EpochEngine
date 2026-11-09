"""Run real per-context state/atlas routing against a recording GPU owner."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT=Path(__file__).resolve().parents[2]
class OpenGLContextUploadOwnership(unittest.TestCase):
 def test_selected_window_depth_format(self):
  if os.name!='nt':self.skipTest('Windows pixel-format CPU lane')
  from Tests.support.cpp import compile_and_run
  import time,json
  source=(ROOT/'Engine/src/renderers/opengl/opengl.context_init.cpp').read_text(encoding='utf-8')
  start=source.index('            // SetPixelFormat is one-time per HDC.')
  body=source[start:source.index('            // ---- WGL bootstrap:',start)]
  selected=source.index('        // The format may be inherited')
  body+=source[selected:source.index('        // Publish through PlatformGL',selected)]
  code=r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <source_location>
#include <stdexcept>
#include <cstdio>
int inherited{},chosenDepth=-1,chooseCount{},setCount{};bool describeFailure{},setFailure{};
PIXELFORMATDESCRIPTOR requested{};
int pixel_format(HDC){return inherited;}
int choose_format(HDC,const PIXELFORMATDESCRIPTOR* p){requested=*p;++chooseCount;return 7;}
BOOL set_format(HDC,int,const PIXELFORMATDESCRIPTOR*){++setCount;return setFailure?FALSE:TRUE;}
int describe_format(HDC,int,UINT,PIXELFORMATDESCRIPTOR* p){p->cDepthBits=BYTE(chosenDepth<0?requested.cDepthBits:chosenDepth);return describeFailure?0:1;}
#define GetPixelFormat pixel_format
#define ChoosePixelFormat choose_format
#define SetPixelFormat set_format
#define DescribePixelFormat describe_format
namespace logger{enum class LogLevel{INFO};struct Log{template<class... A>void logf(A&&...){} };Log& get(const char*){static Log value;return value;}}
struct State{HDC hdc{};};
void actual(State& glState){BODY}
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#__VA_ARGS__);return 93;}}while(false)
int main(){State glState;actual(glState);
 CHECK(chooseCount==1&&setCount==1&&requested.cDepthBits==24&&requested.cAccumRedBits==0);
 CHECK(requested.cColorBits==32&&requested.iLayerType==PFD_MAIN_PLANE);
 // The former initializer put24 in accumRed, leaving depth0: real SDK layout.
 PIXELFORMATDESCRIPTOR old{sizeof(PIXELFORMATDESCRIPTOR),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,PFD_TYPE_RGBA,32,0,0,0,0,0,0,0,0,0,24,0,0,0,0,0,0,0};
 CHECK(old.cDepthBits==0&&old.cAccumRedBits==24);
 inherited=4;chosenDepth=24;actual(glState);CHECK(chooseCount==1&&setCount==1);
 chosenDepth=0;bool refused{};try{actual(glState);}catch(const std::runtime_error&){refused=true;}CHECK(refused);
 chosenDepth=16;refused=false;try{actual(glState);}catch(const std::runtime_error&){refused=true;}CHECK(refused);
 chosenDepth=32;actual(glState);describeFailure=true;refused=false;try{actual(glState);}catch(const std::runtime_error&){refused=true;}CHECK(refused);
 return 0;
}
'''.replace('BODY',body)
  report=ROOT/'Artifacts/Reports/mesh-culling-oct31'/('depth-format-'+str(time.time_ns()));report.mkdir(parents=True)
  unit=report/'contract.cpp';unit.write_text(code,encoding='utf-8')
  sdk=windows_sdk_includes(Path('C:/Program Files (x86)/Windows Kits/10'))[-1]
  result=compile_and_run(self,unit,report,includes=[sdk/'um',sdk/'shared'])
  (report/'result.json').write_text(json.dumps({'scope':'Actual native pixel-format initialization and selected-format gate; installed Windows SDK field layout, ports only, no window/driver','records':result},indent=2),encoding='utf-8')

 def test_native_context_state_and_upload_dispatch(self):
  if os.name!='nt':self.skipTest('Windows native ownership CPU lane')
  source=(ROOT/'Engine/modules/opengl.textures.ixx').read_text(encoding='utf-8')
  functions='\n'.join(definition(source,marker) for marker in (
   '    [[nodiscard]] inline const void* platform_context_key(',
   '    inline void bind_platform_state(',
   '    [[nodiscard]] inline epochengine::openglstate::OpenGL4State& state_for_platform_context(',
   '    [[nodiscard]] inline AtlasGpuMap& atlas_map_for_platform_context(',
   '    inline void upload_atlas_to_gpu(',
   '    inline void ensure_uploaded('))
  init=(ROOT/'Engine/src/renderers/opengl/opengl.context_init.cpp').read_text(encoding='utf-8')
  first=init.index('        auto& backend =',init.index('    bool opengl_initialize('))
  init_selection=init[first:init.index('#if defined(_WIN32)',first)]
  scene=(ROOT/'Engine/src/renderers/opengl/opengl.scene_preview.cpp').read_text(encoding='utf-8')
  scene_selection=scene[scene.index('        auto& backend ='):scene.index('        openglpreview::render_scene_preview(')]
  queries=(ROOT/'Engine/src/renderers/opengl/opengl.context.cpp').read_text(encoding='utf-8')
  dimensions='\n'.join(definition(queries,marker) for marker in ('    int opengl_get_width()', '    int opengl_get_height()'))
  code=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
using HDC=void*;using HGLRC=void*;using HWND=void*;
namespace epochengine {
namespace openglcontext::PlatformGL {
 struct PlatformGLContext {void* device{};void* context{};bool valid()const{return device&&context;}};
 inline thread_local PlatformGLContext current{};
 PlatformGLContext get_current(){return current;}
}
namespace openglstate {struct OpenGL4State {HDC hdc{};HGLRC hglrc{};HWND hwnd{};unsigned width=1,height=1,pipeline{};};}
namespace core {
 namespace cli {inline int window_width=1600,window_height=1000;}
 enum class ContextType{OpenGL,SFML};
 struct Context {ContextType type{ContextType::OpenGL};void* native_window{};void* native_drawable{};void* native_gl_context{};int framebufferWidth=640,framebufferHeight=480;int get_width_safe()const{return framebufferWidth;}int get_height_safe()const{return framebufferHeight;}};
 struct Slot {std::shared_ptr<void> data;};
 inline std::map<ContextType,Slot> g_backends;
 inline std::shared_mutex g_backendsMutex;
 inline thread_local std::shared_ptr<Context> renderContext;
 auto get_current_render_context(){return renderContext;}
}
namespace logger {inline void error(std::string_view,std::string_view){assert(false);}}
namespace opengltextures {
 using PlatformGLContext=openglcontext::PlatformGL::PlatformGLContext;
 struct TextureAtlas {unsigned version=1;};
 struct AtlasGpu {unsigned version{},textureHandle{};};
 using AtlasGpuMap=std::unordered_map<const TextureAtlas*,AtlasGpu>;
 struct BackendData {
  AtlasGpuMap gpu_atlases;
  std::unordered_map<const void*,AtlasGpuMap> context_gpu_atlases;
  std::unordered_map<const void*,std::unique_ptr<openglstate::OpenGL4State>> context_gl_states;
  std::mutex gpuMutex;openglstate::OpenGL4State glState;
 };
 inline BackendData recordingBackend;
 inline std::atomic<unsigned> uploads{};
 BackendData& get_opengl_backend(){return recordingBackend;}
 namespace detail {PlatformGLContext to_platform_context(const openglstate::OpenGL4State& state){return {state.hdc,state.hglrc};}}
 bool upload_atlas_to_gpu_for_context(BackendData& value,openglstate::OpenGL4State& state,const PlatformGLContext& native,const TextureAtlas& atlas) {
  const auto current=openglcontext::PlatformGL::get_current();
  assert(current.context==native.context&&current.device==native.device&&state.hglrc==native.context);
  std::lock_guard lock(value.gpuMutex);++uploads;
  value.context_gpu_atlases[native.context][&atlas]={atlas.version,100+uploads.load()};return true;
 }
 FUNCTIONS
}
namespace openglcontext {
 DIMENSION_QUERIES
 namespace contextdetail {PlatformGL::PlatformGLContext context_to_platform_context(const core::Context* ctx){return {ctx->native_drawable,ctx->native_gl_context};}}
 openglstate::OpenGL4State& initialization_state(const std::shared_ptr<core::Context>& ctx){INITIAL_SELECTION
  return glState;
 }
 openglstate::OpenGL4State& scene_state(const std::shared_ptr<core::Context>& ctx){const auto native=PlatformGL::get_current();SCENE_SELECTION
  return glState;
 }
}
}
int main(){
 using namespace epochengine;
 auto make=[](std::uintptr_t key){auto ctx=std::make_shared<core::Context>();ctx->native_gl_context=reinterpret_cast<void*>(key);ctx->native_drawable=reinterpret_cast<void*>(key+100);ctx->native_window=reinterpret_cast<void*>(key+200);return ctx;};
 const auto a=make(10),b=make(20),legacy=make(30);
 auto& backend=opengltextures::recordingBackend;
 backend.glState.hdc=legacy->native_drawable;backend.glState.hglrc=legacy->native_gl_context;backend.glState.pipeline=99;
 core::g_backends[core::ContextType::OpenGL].data=std::shared_ptr<void>(&backend,[](void*){});
 auto& stateA=openglcontext::initialization_state(a);auto& stateB=openglcontext::initialization_state(b);
 assert(&stateA!=&stateB&&&stateA!=&backend.glState&&&stateB!=&backend.glState);
 stateA.pipeline=11;stateB.pipeline=22;
 assert(stateA.hglrc==a->native_gl_context&&stateB.hglrc==b->native_gl_context);
 assert(backend.glState.hglrc==legacy->native_gl_context&&backend.glState.pipeline==99);
 assert(&openglcontext::initialization_state(legacy)==&backend.glState);
 opengltextures::TextureAtlas atlas;
 const auto upload=[&](const auto& ctx,unsigned pipeline){
  core::renderContext=ctx;openglcontext::PlatformGL::current={ctx->native_drawable,ctx->native_gl_context};
  assert(openglcontext::opengl_get_width()==ctx->framebufferWidth&&openglcontext::opengl_get_height()==ctx->framebufferHeight);
  for(int i=0;i<1000;++i){auto& state=openglcontext::initialization_state(ctx);assert(state.pipeline==pipeline);assert(&openglcontext::scene_state(ctx)==&state);opengltextures::ensure_uploaded(atlas);}
 };
 std::thread first([&]{upload(a,11);}),second([&]{upload(b,22);});first.join();second.join();
 assert(opengltextures::uploads.load()==2&&backend.context_gpu_atlases.size()==2);
 assert(backend.context_gpu_atlases[a->native_gl_context].contains(&atlas));
 assert(backend.context_gpu_atlases[b->native_gl_context].contains(&atlas));
 // A CPU-only caller may not activate the last initialized render context.
 core::renderContext=a;openglcontext::PlatformGL::current={};++atlas.version;
 opengltextures::ensure_uploaded(atlas);opengltextures::upload_atlas_to_gpu(atlas);assert(opengltextures::uploads.load()==2);
 // A different GL-owning backend may not fulfill OpenGL's atlas queue.
 auto foreign=make(40);foreign->type=core::ContextType::SFML;core::renderContext=foreign;
 openglcontext::PlatformGL::current={foreign->native_drawable,foreign->native_gl_context};opengltextures::ensure_uploaded(atlas);assert(opengltextures::uploads.load()==2);
 upload(a,11);assert(opengltextures::uploads.load()==3);
 assert(backend.context_gpu_atlases[b->native_gl_context][&atlas].version==1);
 assert(backend.glState.pipeline==99);
}
'''.replace('FUNCTIONS',functions).replace('INITIAL_SELECTION',init_selection).replace('SCENE_SELECTION',scene_selection).replace('DIMENSION_QUERIES',dimensions)
  parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
  scratch=Path(tempfile.mkdtemp(prefix='opengl-context-upload-owner-',dir=parent))
  try:
   tools=msvc_2022_toolsets()
   sdk=Path('C:/Program Files (x86)/Windows Kits/10')
   kits=windows_sdk_includes(sdk)
   if not tools or not kits:self.skipTest('MSVC/SDK required')
   tool,kit=tools[-1],kits[-1];cpp=scratch/'contract.cpp';binary=scratch/'contract.exe';cpp.write_text(code,encoding='utf-8')
   args=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX','/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(scratch/'contract.obj'),'/Fe'+str(binary),str(cpp),'/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
   built=subprocess.run(args,cwd=scratch,capture_output=True,text=True,timeout=45);self.assertEqual(built.returncode,0,built.stdout[-6000:]+built.stderr)
   run=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=15);self.assertEqual(run.returncode,0,run.stdout+run.stderr)
  finally:
   self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()));shutil.rmtree(scratch)
