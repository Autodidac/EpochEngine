"""Actual init-owned GL debug callback with CPU-only GL/logger substitutes."""
from pathlib import Path
import json
import unittest
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class OpenGLDriverDebug(unittest.TestCase):
    def test_actual_callback_and_opt_in_admission(self):
        owner = (ROOT / "Engine/src/renderers/opengl/opengl.context_init.cpp").read_text(encoding="utf-8")
        body = owner.split("    namespace detail\n    {\n", 1)[1].split("        inline const wchar_t* gl_child_class_name()", 1)[0]
        body = body.rsplit("#if defined(_WIN32)", 1)[0]
        self.assertLess(owner.index("gladLoadGLLoader("), owner.index("detail::configure_driver_debug_output();"))
        self.assertLess(owner.index("detail::configure_driver_debug_output();"), owner.index("openglquad::ensure_quad_pipeline("))
        code = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "core.format_text.hpp"
#include "core.log_compact.hpp"
#include "core.stl_types.hpp"
#if defined(_WIN32)
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#define GL_VERSION_4_3 1
#define GL_KHR_debug 1
using GLenum=unsigned;using GLuint=unsigned;using GLsizei=int;using GLchar=char;
constexpr unsigned GL_DEBUG_SEVERITY_HIGH=0x9146,GL_DEBUG_SEVERITY_MEDIUM=0x9147;
constexpr unsigned GL_DEBUG_OUTPUT_SYNCHRONOUS=0x8242,GL_DEBUG_OUTPUT=0x92E0;
constexpr unsigned GL_DONT_CARE=0x1100,GL_FALSE=0,GL_TRUE=1;
int GLAD_GL_VERSION_4_3{},GLAD_GL_KHR_debug{};
using Callback=void(APIENTRY*)(GLenum,GLenum,GLuint,GLenum,GLsizei,const GLchar*,const void*);
Callback callback{};const void* userdata=reinterpret_cast<void*>(1);
std::vector<unsigned> glCalls;
void glEnable(unsigned value){glCalls.push_back(value);}
void registerCallback(Callback value,const void* user){callback=value;userdata=user;glCalls.push_back(1);}
void messageControl(unsigned,unsigned,unsigned severity,int,const unsigned*,unsigned enable){glCalls.push_back(severity);glCalls.push_back(enable);}
auto glDebugMessageCallback=&registerCallback;
auto glDebugMessageControl=&messageControl;
namespace core::env {
epochengine::optional<epochengine::string> value;
epochengine::optional<epochengine::string> get(epochengine::string_view){return value;}
}
namespace logger {
enum class LogLevel{INFO,WARN};
std::mutex lock;std::vector<std::string> lines;bool fail{};
struct Logger {
 void log(LogLevel,std::string_view message,std::source_location){
  std::lock_guard guard(lock);if(fail)throw 7;lines.emplace_back(message);
 }
 template<class... Args> void logf(LogLevel level,std::source_location loc,std::string_view format,Args&&... args){
  log(level,epochengine::format_text(format,args...),loc);
 }
};
Logger& get(const char*){static Logger result;return result;}
}
namespace actual {
''' + body + r'''
}
#undef GL_VERSION_4_3
#undef GL_KHR_debug
namespace no_debug_headers {
''' + body + r'''
}
int main(){
 using actual::configure_driver_debug_output;
 // No request or a non-exact request makes no GL calls, even with support.
 GLAD_GL_VERSION_4_3=1;
 configure_driver_debug_output();assert(glCalls.empty()&&logger::lines.empty());
 for(const char* value:{"0","true","","10","01"," 1","1 "}){core::env::value=value;configure_driver_debug_output();}
 assert(glCalls.empty()&&logger::lines.empty());
 core::env::value="1";GLAD_GL_VERSION_4_3=0;
 configure_driver_debug_output();assert(glCalls.empty()&&logger::lines.size()==1);
 no_debug_headers::configure_driver_debug_output();assert(glCalls.empty()&&logger::lines.size()==2);
 GLAD_GL_VERSION_4_3=1;glDebugMessageCallback=nullptr;
 configure_driver_debug_output();assert(glCalls.empty());
 glDebugMessageCallback=&registerCallback;glDebugMessageControl=nullptr;
 configure_driver_debug_output();assert(glCalls.empty());
 glDebugMessageControl=&messageControl;GLAD_GL_VERSION_4_3=0;GLAD_GL_KHR_debug=1;
 configure_driver_debug_output();
 assert(callback&&userdata==nullptr&&glCalls.size()==9);
 assert(glCalls.front()==GL_DEBUG_OUTPUT_SYNCHRONOUS&&glCalls.back()==GL_DEBUG_OUTPUT);
 assert(glCalls[2]==GL_DONT_CARE&&glCalls[3]==GL_FALSE);
 assert(glCalls[4]==GL_DEBUG_SEVERITY_HIGH&&glCalls[5]==GL_TRUE);
 assert(glCalls[6]==GL_DEBUG_SEVERITY_MEDIUM&&glCalls[7]==GL_TRUE);
 const auto calls=glCalls.size();logger::lines.clear();
 callback(1,2,3,0,4,"skip",nullptr);assert(logger::lines.empty());
 std::string longText(4096,'x');
 callback(~0u,~0u,0x1234,GL_DEBUG_SEVERITY_HIGH,4096,longText.data(),nullptr);
 assert(logger::lines.size()==1);
 const auto& longLine=logger::lines.back();
 assert(longLine.find("src=FFFFFFFF type=FFFFFFFF id=1234 sev=9146 cut=true:")!=std::string::npos);
 assert(std::count(longLine.begin(),longLine.end(),'x')==96&&longLine.size()<170);
 assert(epochengine::core::log::display::compact_message("WARN","OpenGL.Debug",longLine)==longLine);
 assert(longLine.substr(0,512)==longLine);
 callback(1,2,4,GL_DEBUG_SEVERITY_MEDIUM,999,nullptr,nullptr);
 callback(1,2,5,GL_DEBUG_SEVERITY_HIGH,-1,"unused",nullptr);
 assert(logger::lines[1].find("<no message>")!=std::string::npos);
 assert(logger::lines[2].find("<no message>")!=std::string::npos);
 logger::fail=true;callback(1,2,6,GL_DEBUG_SEVERITY_HIGH,1,"x",nullptr);logger::fail=false;
 for(unsigned i=0;i<100;++i)callback(1,2,i,GL_DEBUG_SEVERITY_HIGH,1,"x",nullptr);
 assert(actual::driverDebugReports==8&&logger::lines.size()==7&&glCalls.size()==calls);
 // Reinitializing a context must not reset the process budget.
 configure_driver_debug_output();const auto count=logger::lines.size();
 callback(1,2,9,GL_DEBUG_SEVERITY_HIGH,1,"x",nullptr);assert(logger::lines.size()==count);
 // Actual saturating budget remains bounded across simultaneous context callbacks.
 actual::driverDebugReports=0;logger::lines.clear();const auto beforeThreads=glCalls.size();
 std::vector<std::thread> threads;
 for(unsigned i=0;i<8;++i)threads.emplace_back([]{
  for(unsigned j=0;j<1000;++j)callback(1,2,j,GL_DEBUG_SEVERITY_HIGH,1,"x",nullptr);
 });
 for(auto& thread:threads)thread.join();
 assert(actual::driverDebugReports==8&&logger::lines.size()==8&&glCalls.size()==beforeThreads);
 return 0;
}
'''
        report = ROOT / "Artifacts/Reports/opengl-driver-debug-oct30/cpu"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "opengl_driver_debug_contract.cpp"
        source.write_text(code, encoding="utf-8")
        result = compile_and_run(self, source, report,
            includes=(ROOT / "Engine/include", ROOT / "Engine/dep/EpochPlatformEngine/include"),
            windows_flags=("/WX",))
        (report / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
