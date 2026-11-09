"""Actual manager destructor/StopAll keeps production queues alive through join."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest
from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class ContextManagerExceptionRetirement(unittest.TestCase):
    def test_real_render_thread_retires_before_window_member_teardown(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC C++23 CPU owner lane')
        owner = ROOT/'Engine/src/renderers/host/host.context_win.cpp'
        source = owner.read_text(encoding='utf-8')
        destructor = definition(source,'    MultiContextManager::~MultiContextManager() noexcept')
        stop = definition(source,'    void MultiContextManager::StopAll()')
        api = (ROOT/'Engine/modules/context.multiplexer.ixx').read_text(encoding='utf-8')
        self.assertEqual(api.count('~MultiContextManager() noexcept;'),2)
        self.assertIn('~MultiContextManager() noexcept = default;',api)
        linux = (ROOT/'Engine/src/renderers/host/host.context_linux.cpp').read_text(encoding='utf-8')
        self.assertIn('StopAll();',definition(linux,'    MultiContextManager::~MultiContextManager() noexcept'))
        engine = (ROOT/'Engine/src/epoch/epoch.engine_legacy.cpp').read_text(encoding='utf-8')
        interface = definition(engine,'    void RunEditorInterface()\n    {')
        catch = interface.index('catch (const std::exception& ex)')
        self.assertLess(interface.index('logger::get(engine::kEditorLog).log(',catch),interface.index('MessageBoxA(',catch))
        code = r'''
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
import context.commandqueue;
using HWND=void*;
namespace core {enum class ContextType{None};}
struct WindowData;
struct Context{WindowData* windowData{};};
struct Proof{std::atomic_bool alive{true},go{},used_live_queue{},bad_access{},retired{};};
struct WindowData {
 HWND hwnd{reinterpret_cast<HWND>(1)},host_hwnd{},hwndChild{};
 std::atomic_bool running{true};std::shared_ptr<Context> context;
 epochengine::core::CommandQueue commandQueue,ownerThreadCommandQueue;
 std::shared_ptr<Proof> proof;core::ContextType type{};std::string titleNarrow{"contract"};
 explicit WindowData(std::shared_ptr<Proof> p):proof(std::move(p)){}
 void set_should_close(bool){proof->go.store(true,std::memory_order_release);}
 ~WindowData(){proof->alive.store(false,std::memory_order_release);}
};
namespace epochengine::logger {enum class LogLevel{INFO};struct Log{template<class...T>void logf(T&&...)const{}};Log get(std::string_view){return{};}}
constexpr std::string_view kLogSys="contract";
auto typed_context(const std::shared_ptr<Context>& c){return c;}
bool matches_window_handle(const WindowData* w,HWND h){return w&&w->hwnd==h;}
void forget_native_title_frame_source(const WindowData*){}
void cleanup_window_resources(std::unique_ptr<WindowData>& w){w.reset();}
void join_thread_with_message_pump(std::thread& t)noexcept{if(t.joinable())t.join();}
struct PendingWindowCleanup{HWND hwnd{};std::thread thread;std::unique_ptr<WindowData> window;};
std::mutex g_threadStateMutex;std::unordered_map<HWND,std::thread> g_threads;std::vector<PendingWindowCleanup>g_pendingCleanups;
class MultiContextManager;MultiContextManager* g_activeManager{};
class MultiContextManager {
public:
 ~MultiContextManager()noexcept;void StopAll();
 std::vector<std::unique_ptr<WindowData>>windows;std::atomic_bool running{};std::mutex windowsMutex;
 inline static MultiContextManager* s_activeInstance{};
 void start(const std::shared_ptr<Proof>& proof){
  s_activeInstance=this;g_activeManager=this;running=true;
  auto window=std::make_unique<WindowData>(proof);auto* raw=window.get();
  window->ownerThreadCommandQueue.enqueue([]{});windows.push_back(std::move(window));
  g_threads.emplace(raw->hwnd,std::thread([raw,proof]{
   while(!proof->go.load(std::memory_order_acquire))std::this_thread::yield();
   if(!proof->alive.load(std::memory_order_acquire))proof->bad_access=true;
   else {raw->ownerThreadCommandQueue.clear();raw->commandQueue.clear();proof->used_live_queue=true;}
   proof->retired=true;
  }));
 }
};
DESTRUCTOR
STOP
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(){
 const auto state=std::make_shared<Proof>();
 try{MultiContextManager owner;owner.start(state);
  {MultiContextManager unused;} // Its destructor must not stop the active owner's registry.
  CHECK(!g_threads.empty()&&state->alive&&!state->retired);
  throw std::runtime_error("original session exception");
 }catch(const std::runtime_error& error){
  // Release the former/default-destructor negative safely; it never dereferences
  // a dead WindowData. The positive path has already joined before catch.
  state->go=true;for(auto& [key,thread]:g_threads)if(thread.joinable())thread.join();g_threads.clear();
  CHECK(std::string_view(error.what())=="original session exception");
 }
 CHECK(state->retired&&state->used_live_queue&&!state->bad_access&&!state->alive);
 CHECK(g_activeManager==nullptr&&MultiContextManager::s_activeInstance==nullptr);
 const auto explicitStop=std::make_shared<Proof>();
 {MultiContextManager owner;owner.start(explicitStop);owner.StopAll();owner.StopAll();}
 CHECK(explicitStop->retired&&explicitStop->used_live_queue&&!explicitStop->bad_access&&!explicitStop->alive);
 {MultiContextManager neverStarted;}CHECK(g_threads.empty());
}
'''.replace('DESTRUCTOR',destructor).replace('STOP',stop)
        negative=code.replace(destructor,'MultiContextManager::~MultiContextManager() noexcept {}')
        self.assertNotEqual(code,negative)
        proof=ROOT/'Artifacts/Reports/planet-runtime-attachment-rebuild-oct28/exception-retirement'
        build=ROOT/'Artifacts/Intermediate/tests/context-manager-exception-retirement'
        proof.mkdir(parents=True,exist_ok=True)
        (proof/'retirement.contract.cpp').write_text(code,encoding='utf-8')
        (proof/'former.contract.cpp').write_text(negative,encoding='utf-8')
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ContextExceptionRetirement LANGUAGES CXX)
add_library(queue STATIC "{(ROOT/'Engine/src/renderers/core/core.context_command_queue.cpp').as_posix()}")
target_compile_features(queue PUBLIC cxx_std_23)
target_sources(queue PUBLIC FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES "{(ROOT/'Engine/modules/context.commandqueue.ixx').as_posix()}")
add_executable(retirement retirement.contract.cpp)
add_executable(former former.contract.cpp)
target_link_libraries(retirement PRIVATE queue)
target_link_libraries(former PRIVATE queue)
enable_testing()
add_test(NAME exception_retirement COMMAND retirement)
add_test(NAME former_default_destructor COMMAND former)
set_tests_properties(former_default_destructor PROPERTIES WILL_FAIL TRUE)
set_tests_properties(exception_retirement former_default_destructor PROPERTIES TIMEOUT 10)
''',encoding='utf-8')
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64',
                   '-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],
                         ['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']]
        records=[]
        for number,command in enumerate(commands):
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90,
                                  env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'))
            (proof/f'lane-{number}.log').write_text(result.stdout+result.stderr,encoding='utf-8')
            records.append({'command':command,'exit':result.returncode})
            self.assertEqual(result.returncode,0,(result.stdout+result.stderr)[-7000:])
        (proof/'result.json').write_text(json.dumps({'passed':True,'commands':records,
            'ownerSha256':hashlib.sha256(owner.read_bytes()).hexdigest(),
            'scope':'Actual Windows manager destructor and StopAll with real std::thread and production CommandQueue. Recorded native-resource/message-pump ports; exception, unused owner, explicit StopAll repetition. Former default destructor fails safely without touching freed memory. No GUI/GPU.'},indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    unittest.main()
