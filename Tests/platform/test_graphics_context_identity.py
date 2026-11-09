"""Compile actual graphics declarations beside the canonical native descriptor."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run
ROOT = Path(__file__).resolve().parents[2]

class GraphicsContextIdentity(unittest.TestCase):
    def test_actual_raylib_owner_capture_and_precise_presentation_guards(self):
        linux = (ROOT/'Engine/modules/raylib.context_linux.ixx').read_text(encoding='utf-8')
        helper = linux[linux.index('export namespace epochengine::raylibcontext::linux'):linux.rindex('#endif')]
        helper = helper.replace('export namespace', 'namespace', 1)
        self.assertNotIn('get_window_handle()', helper)
        context = (ROOT/'Engine/modules/raylib.context.ixx').read_text(encoding='utf-8')
        initialize_start=context.index('    export inline bool raylib_initialize(')
        initialize = definition(context[context.index('\n    {', initialize_start):], '\n    {')
        self.assertLess(initialize.index('epochengine::raylib_api::init_window('), initialize.index('linux::capture_native_context()'))
        self.assertLess(initialize.index('if (!epochengine::raylib_api::is_window_ready())'), initialize.index('linux::capture_native_context()'))
        self.assertIn('return fail_initialization("Failed to capture Raylib GLFW context', initialize)
        self.assertIn('st = {};', initialize)
        cleanup = definition(context, '        inline void raylib_cleanup_owner_thread(')
        self.assertLess(cleanup.index('epochengine::raylib_api::close_window();'), cleanup.index('st = {};'))
        device = (ROOT/'Engine/modules/render.device_raylib.ixx').read_text(encoding='utf-8')
        methods = '\n'.join(definition(device, '        [[nodiscard]] '+signature) for signature in (
            'const char* native_resource_context_refusal(', 'bool native_resource_context_available(',
            'const char* native_presentation_context_refusal(', 'bool native_presentation_context_available('))
        renderer = (ROOT/'Engine/modules/raylib.renderer.ixx').read_text(encoding='utf-8')
        diagnostic = definition(renderer, '        inline void log_no_context_refusal(')
        prefix = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
struct GLFWwindow { int identity; };
GLFWwindow first{1}, second{2}; GLFWwindow* currentNative{};
unsigned nativeHandleCalls{}, makeCurrentCalls{}; unsigned long x11NativeHandle=0x200002;
GLFWwindow* glfwGetCurrentContext() { return currentNative; }
void glfwMakeContextCurrent(GLFWwindow* value) { ++makeCurrentCalls; currentNative=value; }
namespace epochengine {
 namespace core {
  enum class ContextType { RayLib, Other };
  struct Context { ContextType type{ContextType::RayLib}; };
  std::shared_ptr<Context> currentEngine;
  auto get_current_render_context() { return currentEngine; }
 }
 namespace raylib_api {
  bool ready{};
  bool is_window_ready() { return ready; }
  void* get_window_handle() { ++nativeHandleCalls; return &x11NativeHandle; }
 }
 namespace raylibstate {
  struct State { core::Context* owner_ctx{}; std::thread::id owner_thread{}; void* hglrc{};
   bool running{}, renderingActive{}, frameActive{}, frameInTextureMode{}; };
  State s_raylibstate;
 }
 namespace logger {
  std::vector<std::string> warnings;
  void warn(const char*, const std::string& message) { warnings.push_back(message); }
 }
}
'''
        suffix = r'''
namespace epochengine::raylibcontext::win {
 bool native_context_is_current() { return epochengine::raylibcontext::linux::native_context_is_current(); }
}
namespace epochengine {
 struct Device {
'''+methods+r'''
 };
 namespace raylibrenderer::detail {
  std::atomic_uint32_t s_canvasNoContextRefusals{};
'''+diagnostic+r'''
 }
}
int main() {
 using namespace epochengine;
 namespace native=raylibcontext::linux;
 auto& state=raylibstate::s_raylibstate;
 auto owner=std::make_shared<core::Context>(), other=std::make_shared<core::Context>();
 currentNative=&first; raylib_api::ready=true;
 assert(!native::capture_native_context());
 state.owner_ctx=owner.get(); state.owner_thread=std::this_thread::get_id();
 currentNative=nullptr; assert(!native::capture_native_context() && !state.hglrc);
 currentNative=&first; raylib_api::ready=false; assert(!native::capture_native_context());
 raylib_api::ready=true;
 bool foreignAccepted=true;
 std::thread foreign([&]{ foreignAccepted=native::capture_native_context(); }); foreign.join();
 assert(!foreignAccepted && !state.hglrc);
 // The old 6.0-handle cast rejects a valid owner: X11's XID address is not GLFWwindow*.
 assert(static_cast<GLFWwindow*>(raylib_api::get_window_handle())!=glfwGetCurrentContext());
 nativeHandleCalls=0;
 assert(native::capture_native_context() && state.hglrc==&first);
 assert(native::native_context_is_current() && nativeHandleCalls==0);
 currentNative=&second;
 assert(!native::capture_native_context() && state.hglrc==&first);
 assert(!native::native_context_is_current());
 assert(native::make_native_context_current() && currentNative==&first && makeCurrentCalls==1);
 assert(native::make_native_context_current() && makeCurrentCalls==1);
 Device device;
 state.running=state.renderingActive=state.frameActive=true;
 core::currentEngine=owner;
 assert(device.native_resource_context_available(owner.get()));
 assert(device.native_presentation_context_available(owner.get()));
 auto reject=[&](std::string_view expected) {
  assert(!device.native_presentation_context_available(owner.get()));
  const char* reason=device.native_presentation_context_refusal(owner.get());
  assert(reason && std::string_view{reason}==expected);
 };
 state.renderingActive=false; reject("renderer is inactive"); state.renderingActive=true;
 state.owner_ctx=nullptr; reject("owner context is missing"); state.owner_ctx=owner.get();
 raylib_api::ready=false; reject("native window is not ready"); raylib_api::ready=true;
 assert(std::string_view{device.native_resource_context_refusal(other.get())}=="requested owner does not match");
 core::currentEngine.reset(); reject("current engine context is missing");
 core::currentEngine=other; reject("current engine context does not match owner");
 core::currentEngine=owner; owner->type=core::ContextType::Other;
 reject("current engine context is not Raylib"); owner->type=core::ContextType::RayLib;
 currentNative=&second; reject("native GL context is not current"); currentNative=&first;
 state.running=false; reject("renderer is stopped"); state.running=true;
 state.frameActive=false; reject("drawing frame is not active"); state.frameActive=true;
 state.frameInTextureMode=true; reject("drawing frame is targeting a texture"); state.frameInTextureMode=false;
 assert(device.native_presentation_context_available(owner.get()));
 for(int i=0;i<12;++i) raylibrenderer::detail::log_no_context_refusal("scene preparation", "drawing frame is not active");
 assert(logger::warnings.size()==8);
 for(const auto& warning:logger::warnings) {
  assert(warning.find("drawing frame is not active")!=std::string::npos);
  assert(warning.find("owner GL context is not current")==std::string::npos);
 }
 // Existing lifecycle reset clears the captured pointer; never bind a stale/deleted window.
 state={}; currentNative=&second;
 assert(!native::native_context_is_current());
 assert(!native::make_native_context_current() && currentNative==&second && makeCurrentCalls==1);
 assert(nativeHandleCalls==0);
}
'''
        report=ROOT/'build/contract-tests/raylib-native-owner'
        report.mkdir(parents=True,exist_ok=True)
        path=report/'contract.cpp';path.write_text(prefix+helper+suffix,encoding='utf-8')
        compile_and_run(self,path,report)

    @unittest.skipUnless(os.name == 'nt', 'Installed MSVC declaration proof')
    def test_native_and_graphics_descriptors_have_distinct_names(self):
        source = (ROOT/'Engine/modules/platform.context.ixx').read_text()
        declarations = source.split('export namespace epochengine::platform',1)[1].split('    class IGraphicsContext',1)[0] + '}\n'
        self.assertIn('struct GraphicsContextDesc', declarations)
        native = ROOT/'Engine/dep/EpochPlatformEngine/include'
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kits = sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').glob('*'))
        self.assertTrue(toolsets and kits, 'MSVC and SDK required')
        parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent,prefix='graphics-context-') as folder:
            folder=Path(folder)
            prefix='#include <epochplatformengine/runtime.hpp>\n#include <type_traits>\nnamespace epochengine::platform '
            checks='static_assert(!std::is_same_v<epochengine::platform::ContextDesc, epochengine::platform::GraphicsContextDesc>);\n'
            compiler=toolsets[-1]/'bin/Hostx64/x64/cl.exe'
            args=[str(compiler),'/nologo','/std:c++latest','/EHsc','/c','/DEPOCH_PLATFORM_ENGINE_STATIC=1','/I'+str(native),'/I'+str(toolsets[-1]/'include')]
            args += ['/I'+str(kits[-1]/part) for part in ('ucrt','shared','um')]
            for previous in (False,True):
                code=prefix+declarations+checks
                if previous: code=prefix+declarations.replace('GraphicsContextDesc','ContextDesc')
                path=folder/('previous.cpp' if previous else 'current.cpp');path.write_text(code)
                result=subprocess.run(args+[str(path)],cwd=folder,capture_output=True,text=True,timeout=60)
                if previous:
                    self.assertNotEqual(result.returncode,0,'Old duplicate declarations unexpectedly accepted')
                    self.assertIn('ContextDesc',result.stdout+result.stderr)
                else:self.assertEqual(result.returncode,0,result.stdout+result.stderr)
