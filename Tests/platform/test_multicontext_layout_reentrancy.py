"""Actual mixed-backend layout/resize/proxy owners with reentrant CPU ports.

No Win32, graphics or application is started. Native ports record HWND owner
identities and deliver resize callbacks synchronously; posted/asynchronous work
is delivered only while its recorded owner is current.
"""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition

ROOT = Path(__file__).resolve().parents[2]
OWNERS = (
    "Engine/src/renderers/host/host.context_win.cpp",
    "Engine/src/renderers/host/implementation/host.window_interaction.inl",
    "Engine/src/renderers/host/implementation/host.native_dispatch.inl",
    "Engine/modules/context.window.ixx",
)


PREFIX = r'''
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using HWND=void*;using HANDLE=void*;using UINT=unsigned;using DWORD=std::uint32_t;using WPARAM=std::uintptr_t;
using LPARAM=std::intptr_t;using LONG_PTR=std::intptr_t;using LRESULT=std::intptr_t;
struct RECT{int left{},top{},right{},bottom{};};struct POINT{int x{},y{};};struct SIZE{int cx{},cy{};};
constexpr int FALSE=0,SW_HIDE=0,SW_SHOWNA=8,GWL_STYLE=-16,GWL_EXSTYLE=-20;
constexpr LONG_PTR WS_CHILD=1,WS_POPUP=2,WS_OVERLAPPEDWINDOW=4,WS_VISIBLE=8;
constexpr LONG_PTR WS_CLIPSIBLINGS=16,WS_CLIPCHILDREN=32,WS_EX_APPWINDOW=64,WS_EX_TOOLWINDOW=128;
constexpr LONG_PTR WS_EX_TOPMOST=256,WS_EX_WINDOWEDGE=512,WS_EX_CLIENTEDGE=1024,WS_EX_DLGMODALFRAME=2048,WS_EX_NOPARENTNOTIFY=4096;
constexpr UINT SWP_NOZORDER=1,SWP_NOACTIVATE=2,SWP_FRAMECHANGED=4,SWP_SHOWWINDOW=8,SWP_ASYNCWINDOWPOS=16;
constexpr UINT RDW_INVALIDATE=1,RDW_ALLCHILDREN=2,RDW_NOERASE=4,WM_APP=0x8000;
constexpr UINT ERROR_SUCCESS=0;
constexpr HWND HWND_DESKTOP=nullptr;
HWND handle(unsigned value){return reinterpret_cast<HWND>(static_cast<std::uintptr_t>(value));}
struct TrackingMutex {
 static inline unsigned depth{};
 void lock(){++depth;}void unlock(){CHECK(depth);--depth;}
};
namespace ports {
struct Native {HWND parent{};RECT client{0,0,100,100};POINT position{};unsigned owner=1;
 bool backendOwned{},visible=true;LONG_PTR style=WS_CHILD,extended{};std::unordered_map<std::wstring,HANDLE> props;
 unsigned process=7;bool proxyShell{};};
struct Message{HWND target{};UINT code{};WPARAM w{};LPARAM l{};};
struct Call{std::string name;HWND target{};unsigned caller{},owner{};bool asynchronous{};int width{},height{};};
std::unordered_map<HWND,Native> windows;
std::deque<Message> messages;std::deque<std::pair<unsigned,std::function<void()>>> asynchronous;
std::vector<Call> calls;unsigned current=1,mutationsUnderLock{},foreignBackendMutations{},reentries{},delivered{},maxPending{};
unsigned registryGrowth{};bool growOnPlacement=true;UINT lastError{};
HWND capture{};unsigned shellOwnerViolations{};
bool captureFails{};unsigned captureNotifications{};
}
int IsWindow(HWND);HWND GetParent(HWND);int GetClientRect(HWND,RECT*);int GetWindowRect(HWND,RECT*);
int ClientToScreen(HWND,POINT*);int ScreenToClient(HWND,POINT*);int MapWindowPoints(HWND,HWND,POINT*,UINT);
LONG_PTR GetWindowLongPtrW(HWND,int);LONG_PTR SetWindowLongPtrW(HWND,int,LONG_PTR);
HWND SetParent(HWND,HWND);int SetWindowPos(HWND,HWND,int,int,int,int,UINT);int ShowWindow(HWND,int);
int IsWindowVisible(HWND);int RedrawWindow(HWND,void*,void*,UINT);int UpdateWindow(HWND);HWND SetFocus(HWND);
HANDLE GetPropW(HWND,const wchar_t*);int SetPropW(HWND,const wchar_t*,HANDLE);HANDLE RemovePropW(HWND,const wchar_t*);
int PostMessageW(HWND,UINT,WPARAM,LPARAM);void SetLastError(UINT);UINT GetLastError();
int ReleaseCapture();HWND SetCapture(HWND);HWND SetActiveWindow(HWND);int SetForegroundWindow(HWND);
DWORD GetCurrentThreadId();DWORD GetCurrentProcessId();DWORD GetWindowThreadProcessId(HWND,DWORD*);
HWND GetCapture();int InvalidateRect(HWND,void*,int);
namespace epochengine::logger {
enum class LogLevel{INFO};struct Log{template<class...T>void logf(T&&...){}};
Log& get(const char*){static Log value;return value;}
void error(const char*,const char*){}
}
constexpr const char* kLogSys="recording";
namespace epochengine::core {
enum class ContextType{None,RayLib,SDL,SFML,Vulkan,OpenGL,DirectX,Software};
enum class RoutedPanelDockTarget{none,left_tabs,left_context,right_tabs,right_context,bottom_left_tabs,bottom_right_tabs};
enum class BackendLifecycleState{ready,failed};
struct Queue {
 unsigned owner=1;std::deque<std::function<void()>> pending;
 void enqueue(std::function<void()> fn){CHECK(TrackingMutex::depth==0);pending.push_back(std::move(fn));}
 void drain(){const auto old=ports::current;ports::current=owner;
  unsigned bound{};while(!pending.empty()){CHECK(++bound<100);auto fn=std::move(pending.front());pending.pop_front();fn();}
  ports::current=old;}
 void clear(){pending.clear();}
};
struct Context {ContextType type{};int width=100,height=100;std::function<void(int,int)> onResize;};
struct WindowData {
 HWND hwnd{},host_hwnd{};std::atomic<HWND> hwndChild{};ContextType type{};bool isFloating{};
 int width=100,height=100;std::shared_ptr<Context> context;std::function<void(int,int)> onResize;
 std::atomic<bool> running{true},firstPresentComplete{true},should_close{},retirementQueued{};
 std::atomic<std::uint64_t> resizeGeneration{},successfulFrameGeneration{1};
 PROXY_GENERATION_FIELD
 std::atomic<unsigned> dockTargetPreference{};std::atomic<int> dockGridOrder{-1};
 BackendLifecycleState lifecycle=BackendLifecycleState::ready;Queue commandQueue,ownerThreadCommandQueue;
 bool backend_ready()const{return lifecycle==BackendLifecycleState::ready;}
 bool get_should_close()const{return should_close.load(std::memory_order_acquire);}
 void set_backend_lifecycle(BackendLifecycleState value){lifecycle=value;}
 void set_size(int w,int h){width=w;height=h;}
};
struct DragState{bool dragging{};HWND draggedWindow{},originalParent{};};
DragState& Drag(){static DragState value;return value;}
struct MultiContextManager {
 HWND parent=handle(1);mutable TrackingMutex windowsMutex;
 std::vector<std::unique_ptr<WindowData>> windows;
 void ArrangeDockedWindowsGrid();void HandleResize(HWND,int,int);WindowData* findWindowByHWND(HWND);
};
}
using namespace epochengine::core;
MultiContextManager* g_activeManager{};
std::shared_ptr<Context> typed_context(const std::shared_ptr<Context>& value){return value;}
namespace backend {void ResolveClientSize(HWND hwnd,int& width,int& height){RECT r{};GetClientRect(hwnd,&r);width=r.right;width=std::max(1,width);height=std::max(1,r.bottom);}}
namespace telemetry {
struct RendererTelemetryTags{ContextType type;std::uintptr_t window;const char* role{};};
template<class...T>void emit_counter(T&&...){}template<class...T>void emit_gauge(T&&...){}
}
struct Attachment {bool attached()const{return false;}bool place(int,int,int,int){CHECK(false);return false;}};
std::shared_ptr<Attachment> external_attachment(const WindowData*){return {};}
struct Presentation{bool in_viewport{};};Presentation external_presentation(const WindowData*){return {};}
bool place_external_viewport_attachment(HWND,Presentation){CHECK(false);return false;}
void position_top_level_shell(HWND,int,int,int,int);
void move_detached_top_level_shell(HWND,int,int,int,int){CHECK(false);}
void sync_drag_offset_to_host_window(HWND);
void restore_native_drag_opacity(DragState&){}
void clear_routed_panel_tab_drop(){}
'''


PORTS = r'''
void native_mutation(const char* name,HWND target,bool asynchronous=false){
 CHECK(ports::windows.contains(target));const auto& window=ports::windows.at(target);
 ports::calls.push_back({name,target,ports::current,window.owner,asynchronous});
 if(TrackingMutex::depth)++ports::mutationsUnderLock;
 if(window.backendOwned&&window.owner!=ports::current&&!asynchronous)++ports::foreignBackendMutations;
 if(window.proxyShell&&window.owner!=ports::current&&!asynchronous)++ports::shellOwnerViolations;
}
POINT screen_origin(HWND hwnd){POINT point{};unsigned bound{};
 while(hwnd){CHECK(++bound<20);const auto& w=ports::windows.at(hwnd);point.x+=w.position.x;point.y+=w.position.y;hwnd=w.parent;}return point;}
int IsWindow(HWND hwnd){return ports::windows.contains(hwnd);}
HWND GetParent(HWND hwnd){return IsWindow(hwnd)?ports::windows.at(hwnd).parent:nullptr;}
int GetClientRect(HWND hwnd,RECT* out){if(!IsWindow(hwnd))return 0;*out=ports::windows.at(hwnd).client;return 1;}
int GetWindowRect(HWND hwnd,RECT* out){if(!IsWindow(hwnd))return 0;const auto point=screen_origin(hwnd);const auto r=ports::windows.at(hwnd).client;
 *out={point.x,point.y,point.x+r.right,point.y+r.bottom};return 1;}
int ClientToScreen(HWND hwnd,POINT* point){const auto origin=screen_origin(hwnd);point->x+=origin.x;point->y+=origin.y;return 1;}
int ScreenToClient(HWND hwnd,POINT* point){const auto origin=screen_origin(hwnd);point->x-=origin.x;point->y-=origin.y;return 1;}
int MapWindowPoints(HWND from,HWND to,POINT* points,UINT count){const auto a=screen_origin(from),b=screen_origin(to);
 for(UINT i=0;i<count;++i){points[i].x+=a.x-b.x;points[i].y+=a.y-b.y;}return 1;}
LONG_PTR GetWindowLongPtrW(HWND hwnd,int kind){return kind==GWL_STYLE?ports::windows.at(hwnd).style:ports::windows.at(hwnd).extended;}
LONG_PTR SetWindowLongPtrW(HWND hwnd,int kind,LONG_PTR value){native_mutation("SetWindowLongPtrW",hwnd);
 auto& field=kind==GWL_STYLE?ports::windows.at(hwnd).style:ports::windows.at(hwnd).extended;return std::exchange(field,value);}
void reenter_resize(HWND hwnd){if(g_activeManager){++ports::reentries;const auto r=ports::windows.at(hwnd).client;g_activeManager->HandleResize(hwnd,r.right,r.bottom);}}
HWND SetParent(HWND hwnd,HWND parent){native_mutation("SetParent",hwnd);const auto previous=ports::windows.at(hwnd).parent;
 ports::windows.at(hwnd).parent=parent;reenter_resize(hwnd);return previous;}
int SetWindowPos(HWND hwnd,HWND,int x,int y,int width,int height,UINT flags){
 const auto owner=ports::windows.at(hwnd).owner;
 if(owner!=ports::current&&(flags&SWP_ASYNCWINDOWPOS)){
  native_mutation("SetWindowPos.queued",hwnd,true);
  ports::asynchronous.emplace_back(owner,[=]{SetWindowPos(hwnd,nullptr,x,y,width,height,flags&~SWP_ASYNCWINDOWPOS);});return 1;
 }
 native_mutation("SetWindowPos",hwnd);auto& w=ports::windows.at(hwnd);w.position={x,y};w.client={0,0,width,height};
 ports::calls.back().width=width;ports::calls.back().height=height;
 if(flags&SWP_SHOWWINDOW)w.visible=true;
 if(ports::growOnPlacement&&g_activeManager){ports::growOnPlacement=false;
  std::scoped_lock guard(g_activeManager->windowsMutex);
  g_activeManager->windows.reserve(g_activeManager->windows.capacity()+8);
  g_activeManager->windows.push_back(std::make_unique<WindowData>());++ports::registryGrowth;
 }
 reenter_resize(hwnd);return 1;
}
int ShowWindow(HWND hwnd,int mode){native_mutation("ShowWindow",hwnd);ports::windows.at(hwnd).visible=mode!=SW_HIDE;return 1;}
int IsWindowVisible(HWND hwnd){return IsWindow(hwnd)&&ports::windows.at(hwnd).visible;}
int RedrawWindow(HWND hwnd,void*,void*,UINT){native_mutation("RedrawWindow.invalidate",hwnd,true);return 1;}
int UpdateWindow(HWND hwnd){native_mutation("UpdateWindow",hwnd);return 1;}
HWND SetFocus(HWND hwnd){native_mutation("SetFocus",hwnd);return hwnd;}
HANDLE GetPropW(HWND hwnd,const wchar_t* key){return ports::windows.at(hwnd).props[key];}
int SetPropW(HWND hwnd,const wchar_t* key,HANDLE value){ports::windows.at(hwnd).props[key]=value;return 1;}
HANDLE RemovePropW(HWND hwnd,const wchar_t* key){auto& p=ports::windows.at(hwnd).props;const auto value=p[key];p.erase(key);return value;}
int PostMessageW(HWND hwnd,UINT code,WPARAM w,LPARAM l){CHECK(TrackingMutex::depth==0);ports::messages.push_back({hwnd,code,w,l});
 ports::maxPending=std::max(ports::maxPending,static_cast<unsigned>(ports::messages.size()));return 1;}
void SetLastError(UINT value){ports::lastError=value;}UINT GetLastError(){return ports::lastError;}
DWORD GetCurrentThreadId(){return ports::current;}DWORD GetCurrentProcessId(){return 7;}
DWORD GetWindowThreadProcessId(HWND hwnd,DWORD* process){if(!IsWindow(hwnd))return 0;if(process)*process=ports::windows.at(hwnd).process;return ports::windows.at(hwnd).owner;}
HWND GetCapture(){return ports::capture;}
int InvalidateRect(HWND,void*,int){return 1;}
int ReleaseCapture(){const auto hwnd=ports::capture;ports::capture=nullptr;if(hwnd)cancel_native_context_drag(hwnd);return 1;}
HWND SetCapture(HWND value){const auto old=ports::capture;ports::capture=ports::captureFails?nullptr:value;
 if(old&&old!=value){++ports::captureNotifications;cancel_native_context_drag(old);}return old;}
HWND SetActiveWindow(HWND value){native_mutation("SetActiveWindow",value);return value;}
int SetForegroundWindow(HWND value){native_mutation("SetForegroundWindow",value);return 1;}
void sync_drag_offset_to_host_window(HWND hwnd){CHECK(ports::current==ports::windows.at(hwnd).owner);}
void position_top_level_shell(HWND hwnd,int x,int y,int width,int height){
 native_mutation("position_top_level_shell.port",hwnd);CHECK(ports::current==ports::windows.at(hwnd).owner);
 auto& window=ports::windows.at(hwnd);window.parent=nullptr;window.position={x,y};window.client={0,0,width,height};
}
void pump(MultiContextManager& manager){
 unsigned bound{};
 for(;;){
  CHECK(++bound<100);
  bool work=false;
  while(!ports::messages.empty()){
   CHECK(++ports::delivered<100);auto message=ports::messages.front();ports::messages.pop_front();
   const auto previous=ports::current;ports::current=ports::windows.at(message.target).owner;
   if(message.code==WM_EPOCH_LAYOUT)parent_dispatch(manager,message.target,message.code);
   else proxy_dispatch(message.target,message.code,message.w,message.l);
   ports::current=previous;work=true;
  }
  for(auto& window:manager.windows){window->ownerThreadCommandQueue.drain();window->commandQueue.drain();}
  while(!ports::asynchronous.empty()){
   auto [owner,fn]=std::move(ports::asynchronous.front());ports::asynchronous.pop_front();
   const auto previous=ports::current;ports::current=owner;fn();ports::current=previous;work=true;
  }
  if(!work)break;
 }
}
int main(){
 MultiContextManager manager;g_activeManager=&manager;
 ports::windows[handle(1)]={nullptr,{0,0,1800,1200},{20,30},1,false,true};
 const ContextType types[]{ContextType::RayLib,ContextType::SDL,ContextType::SFML,ContextType::Vulkan,ContextType::OpenGL,ContextType::DirectX};
 std::vector<WindowData*> original;std::vector<std::pair<unsigned,SIZE>> callbacks;
 for(unsigned i=0;i<6;++i){
  const HWND host=handle(10+i),child=i<3?handle(20+i):host;const unsigned renderThread=2+i;
  ports::windows[host]={handle(1),{0,0,100,100},{0,0},1,false,true};
  ports::windows[host].proxyShell=i==1||i==2;
  if(i<3)ports::windows[child]={i==1?host:handle(1),{0,0,100,100},{0,0},renderThread,true,true};
  auto window=std::make_unique<WindowData>();window->type=types[i];window->hwnd=child;
  window->hwndChild=child;window->host_hwnd=i<3?host:nullptr;
  window->context=std::make_shared<Context>();window->context->type=types[i];
  window->commandQueue.owner=renderThread;window->ownerThreadCommandQueue.owner=renderThread;
  window->context->onResize=[&,i,renderThread](int width,int height){CHECK(ports::current==renderThread);callbacks.push_back({i,{width,height}});};
  original.push_back(window.get());manager.windows.push_back(std::move(window));
 }
 request_parent_layout(manager.parent);request_parent_layout(manager.parent);CHECK(ports::messages.size()==1);
 pump(manager);
 CHECK(ports::registryGrowth==1&&manager.windows.size()==7&&ports::reentries>0);
 for(unsigned i=0;i<6;++i)CHECK(manager.windows[i].get()==original[i]);
 CHECK(ports::messages.empty()&&ports::asynchronous.empty()&&ports::delivered<20);
 CHECK(ports::mutationsUnderLock==0);
 const auto delivered=ports::delivered;
 request_parent_layout(manager.parent);pump(manager);CHECK(ports::delivered-delivered==1);
 callbacks.clear();
 for(unsigned i=0;i<6;++i){auto* window=original[i];
  manager.HandleResize(window->hwnd,611,411);manager.HandleResize(window->hwnd,612,412);
  CHECK((i==0?window->ownerThreadCommandQueue.pending.size():window->commandQueue.pending.size())==2);
 }
 pump(manager);CHECK(callbacks.size()==6);
 for(const auto& [index,size]:callbacks){CHECK(index<6&&size.cx==612&&size.cy==412);}
 auto* proxy=original[1];const HWND child=proxy->hwndChild.load(),host=proxy->host_hwnd;
 const auto backend_calls_since=[&](std::size_t start){unsigned count{};for(auto i=start;i<ports::calls.size();++i)
  if(ports::calls[i].target==child)++count;return count;};
 // The newer undock transaction cancels an older redock before renderer drain.
 redock_sfml_proxy_window(proxy,manager.parent,20,30,501,401);
 undock_sfml_proxy_window(proxy,60,70,502,402);
 const auto undockStart=ports::calls.size();pump(manager);
 CHECK(GetParent(child)==host&&ports::windows.at(child).client.right==502);
 for(auto i=undockStart;i<ports::calls.size();++i)
  CHECK(!(ports::calls[i].target==child&&ports::calls[i].name=="SetWindowPos"&&ports::calls[i].width==501));
 redock_sfml_proxy_window(proxy,manager.parent,20,30,503,403);
 auto& drag=Drag();drag.dragging=true;drag.draggedWindow=host;drag.originalParent=manager.parent;ports::capture=host;
 const auto generation=proxy->proxyChildNativeGeneration.load();cancel_native_context_drag(host);
 CHECK(proxy->proxyChildNativeGeneration.load()==generation+1&&!drag.dragging&&!ports::capture);
 const auto cancelStart=ports::calls.size();pump(manager);CHECK(backend_calls_since(cancelStart)==0);
 // Existing queue lifetime remains safe across close, retirement and Stop.
 for(unsigned mode=0;mode<3;++mode){
  apply_child_fill_layout(child,host,510+mode,410+mode);const auto start=ports::calls.size();
  if(mode==0)proxy->should_close=true;if(mode==1)proxy->retirementQueued=true;if(mode==2)proxy->running=false;
  pump(manager);CHECK(backend_calls_since(start)==0);
  proxy->should_close=false;proxy->retirementQueued=false;proxy->running=true;
 }
 // A queued command never migrates to a replacement child/host or HWND owner.
 apply_child_fill_layout(child,host,520,420);const auto childReplacement=ports::calls.size();
 proxy->hwndChild=handle(90);pump(manager);CHECK(backend_calls_since(childReplacement)==0);proxy->hwndChild=child;
 apply_child_fill_layout(child,host,521,421);const auto hostReplacement=ports::calls.size();
 proxy->host_hwnd=handle(91);pump(manager);CHECK(backend_calls_since(hostReplacement)==0);proxy->host_hwnd=host;
 for(unsigned kind=0;kind<3;++kind){
  apply_child_fill_layout(child,host,522+kind,422+kind);const auto start=ports::calls.size();
  if(kind==0)ports::windows.at(child).owner=303;if(kind==1)ports::windows.at(child).process=99;if(kind==2)ports::windows.at(host).owner=404;
  pump(manager);CHECK(backend_calls_since(start)==0);
  ports::windows.at(child).owner=3;ports::windows.at(child).process=7;ports::windows.at(host).owner=1;
 }
 // Actual proxy Undock dispatch transfers capture. SetCapture synchronously
 // delivers the old child's real cancellation handler before returning.
 arm_proxy_drag(proxy,host,true);
 Drag().dragging=true;Drag().draggedWindow=child;Drag().originalParent=manager.parent;ports::capture=child;
 const auto notificationCount=ports::captureNotifications;const auto transferGeneration=proxy->proxyChildNativeGeneration.load();
 auto* request=new ProxyDockRequest{.sourceHwnd=child,.parentHwnd=manager.parent,.x=80,.y=90,.width=540,.height=440};
 const auto transferStart=ports::calls.size();
 proxy_dispatch(host,WM_EPOCH_PROXY_DOCKCMD,static_cast<WPARAM>(ProxyDockCmd::Undock),reinterpret_cast<LPARAM>(request));
 CHECK(ports::captureNotifications==notificationCount+1&&Drag().dragging&&Drag().draggedWindow==host&&ports::capture==host);
 CHECK(proxy->proxyChildNativeGeneration.load()==transferGeneration+1);
 pump(manager);CHECK(GetParent(child)==host&&ports::windows.at(child).client.right==540);
 bool focused{};for(auto i=transferStart;i<ports::calls.size();++i)
  if(ports::calls[i].name=="SetFocus"&&ports::calls[i].target==child){focused=true;CHECK(ports::calls[i].caller==3);}
 CHECK(focused);Drag()={};ports::capture=nullptr;
 // A failed transfer ends the drag, but the shell undock has committed. Its
 // child fill is freshly admitted after cancellation and the old parent settles.
 Drag().dragging=true;Drag().draggedWindow=child;Drag().originalParent=manager.parent;ports::capture=child;ports::captureFails=true;
 request=new ProxyDockRequest{.sourceHwnd=child,.parentHwnd=manager.parent,.x=80,.y=90,.width=541,.height=441};
 const auto failedTransferStart=ports::calls.size();
 const auto deliveredBeforeFailure=ports::delivered;
 proxy_dispatch(host,WM_EPOCH_PROXY_DOCKCMD,static_cast<WPARAM>(ProxyDockCmd::Undock),reinterpret_cast<LPARAM>(request));
 CHECK(!Drag().dragging&&!ports::capture&&!ports::messages.empty());pump(manager);
 CHECK(backend_calls_since(failedTransferStart)>0&&GetParent(child)==host&&ports::windows.at(child).client.right==541);
 CHECK(ports::delivered>deliveredBeforeFailure);
 for(auto i=failedTransferStart;i<ports::calls.size();++i)CHECK(!(ports::calls[i].target==child&&ports::calls[i].name=="SetFocus"));
 ports::captureFails=false;
 // FIFO geometry within one transaction retains latest-generation callbacks.
 callbacks.clear();apply_child_fill_layout(child,host,530,430);apply_child_fill_layout(child,host,531,431);pump(manager);
 CHECK(ports::windows.at(child).client.right==531&&callbacks.size()==1&&callbacks[0].first==1&&callbacks[0].second.cx==531);
 std::ofstream calls("native-calls.tsv");calls<<"call\thwnd\tcaller\towner\tasync\n";
 for(const auto& call:ports::calls)calls<<call.name<<'\t'<<reinterpret_cast<std::uintptr_t>(call.target)<<'\t'<<call.caller<<'\t'<<call.owner<<'\t'<<call.asynchronous<<'\n';
 calls.close();std::ofstream metrics("metrics.json");
 metrics<<"{\"mutations_under_lock\":"<<ports::mutationsUnderLock<<",\"foreign_backend_mutations\":"<<ports::foreignBackendMutations
  <<",\"resize_reentries\":"<<ports::reentries<<",\"delivered_messages\":"<<ports::delivered<<",\"resize_callbacks\":"<<callbacks.size()
  <<",\"proxy_shell_owner_violations\":"<<ports::shellOwnerViolations<<"}\n";metrics.close();
 for(const auto& call:ports::calls)if(ports::windows.at(call.target).backendOwned&&call.caller!=call.owner&&!call.asynchronous)
  std::fprintf(stderr,"Foreign backend mutation: %s hwnd=%llu caller=%u owner=%u\n",call.name.c_str(),
   static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(call.target)),call.caller,call.owner);
 CHECK(ports::foreignBackendMutations==0);
 CHECK(ports::shellOwnerViolations==0&&ports::mutationsUnderLock==0);
 return 0;
}
'''


class MulticontextLayoutReentrancy(unittest.TestCase):
    def test_actual_six_backend_layout_resize_and_proxy_owners(self):
        host, interaction, dispatch, context = [(ROOT / path).read_text(encoding="utf-8") for path in OWNERS]
        proof = ROOT / "Artifacts/Reports/multicontext-layout-reentrancy-oct31/repair-capture-final-cpu"
        proof.mkdir(parents=True, exist_ok=True)
        before = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS}
        (proof / "source-before.json").write_text(json.dumps(before, indent=2) + "\n", encoding="utf-8")
        field = next(line for line in context.splitlines() if "std::atomic_uint64_t proxyChildNativeGeneration" in line)
        chunks = [PREFIX.replace("PROXY_GENERATION_FIELD", field)]
        for name in ("kEpochDockParentProp", "WM_EPOCH_LAYOUT", "WM_EPOCH_PROXY_DOCKCMD", "kEpochLayoutPendingProp"):
            line = next(line for line in interaction.splitlines() if "constexpr " in line and name in line)
            chunks.append(line)
        chunks += [definition(interaction, "    enum class ProxyDockCmd") + ";",
                   definition(interaction, "    struct ProxyDockRequest") + ";"]
        for marker in (
            "    [[nodiscard]] inline int clamp_positive(",
            "    [[nodiscard]] inline bool has_proxy_shell_pair(",
            "    [[nodiscard]] inline bool has_proxy_host(",
            "    [[nodiscard]] inline bool uses_visible_proxy_host(",
            "    [[nodiscard]] inline bool is_sfml_proxy_candidate(",
            "    [[nodiscard]] inline bool proxy_drag_owns_host(",
            "    [[nodiscard]] inline bool backend_requires_owner_thread_dock_commands(",
            "    [[nodiscard]] inline HWND owner_thread_dock_handle(",
            "    [[nodiscard]] inline HWND primary_window_handle(",
            "    [[nodiscard]] inline HWND dock_slot_handle(",
            "    [[nodiscard]] inline bool matches_window_handle(",
            "    [[nodiscard]] inline bool child_window_matches_parent_slot(",
        ):
            chunks.append(definition(interaction, marker))
        chunks.append(definition(interaction, "    template<class Command>"))
        chunks.append(definition(interaction, "    inline void show_proxy_child_window("))
        chunks.append(definition(interaction, "    inline void focus_proxy_child_window("))
        chunks.append(definition(interaction, "    inline void dock_host_window_to_parent("))
        # The first occurrence is a forward declaration; select the real body.
        fill = interaction.index("    inline void apply_child_fill_layout(HWND child, HWND parent, int clientW, int clientH) noexcept\n    {")
        chunks.append(definition(interaction[fill:], "    inline void apply_child_fill_layout("))
        for marker in (
            "    inline void hide_associated_host_window(",
            "    inline void restore_associated_host_window(",
            "    inline void undock_sfml_proxy_window(",
            "    inline void redock_sfml_proxy_window(",
            "    inline void post_proxy_host_command(",
            "    [[nodiscard]] inline bool post_owner_thread_dock_command(",
            "    [[nodiscard]] inline bool backend_window_exposable(",
            "    inline void request_parent_layout(",
        ):
            chunks.append(definition(interaction, marker))
        chunks.append(definition(host, "    WindowData* MultiContextManager::findWindowByHWND("))
        chunks.append(definition(interaction, "    [[nodiscard]] inline epochengine::core::WindowData* resolve_window_data_for_hwnd("))
        chunks.append(definition(interaction, "    inline void cancel_native_context_drag("))
        chunks.append(definition(host, "    static int native_dock_order("))
        chunks.append(definition(host, "    void MultiContextManager::HandleResize("))
        chunks.append(definition(host, "    void MultiContextManager::ArrangeDockedWindowsGrid("))
        arm_start = dispatch.index("            if (window && is_sfml_proxy_candidate(window))\n                window->proxyChildNativeGeneration.fetch_add")
        arm_end = dispatch.index("            ::SetCapture(hwnd);", arm_start)
        chunks.append("void arm_proxy_drag(WindowData* window,HWND hwnd,bool detachedProxyHost){\n" + dispatch[arm_start:arm_end] + "\n}")
        proxy = definition(dispatch, "        case WM_EPOCH_PROXY_DOCKCMD:")
        chunks.append("LRESULT proxy_dispatch(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam){switch(msg){\n" + proxy + "\ndefault:CHECK(false);return 0;}}")
        parent = dispatch.split("        case WM_EPOCH_LAYOUT:", 1)[1].split("        case WM_EPOCH_OPEN_DETACHED_CONTEXT:", 1)[0]
        chunks.append("void parent_dispatch(MultiContextManager& manager,HWND hwnd,UINT msg){auto* mgr=&manager;switch(msg){case WM_EPOCH_LAYOUT:\n" + parent.replace("return 0;", "return;") + "\ndefault:CHECK(false);}}")
        chunks.append(PORTS)
        fixture = proof / "mixed_layout_contract.cpp"
        fixture.write_text("\n".join(chunks), encoding="utf-8")
        try:
            results = compile_and_run(self, fixture, proof, windows_flags=("/WX",))
            (proof / "result.json").write_text(json.dumps(dict(configurations=results, source_before=before,
                scope="Actual layout/resize/proxy owners; native ports only. A cross-owner call is not proof of an OS deadlock."), indent=2) + "\n", encoding="utf-8")
        finally:
            after = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in OWNERS}
            (proof / "source-after.json").write_text(json.dumps(after, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(before, after, "Production source changed during CPU proof")

    def test_actual_non_parented_visibility_helpers(self):
        path = ROOT / OWNERS[1]
        interaction = path.read_text(encoding="utf-8")
        show = definition(interaction, "    inline void show_proxy_child_window(HWND child, int mode) noexcept {")
        focus = definition(interaction, "    inline void focus_proxy_child_window(HWND child) noexcept {")
        code = r'''
#include <cstdlib>
using HWND=void*;
unsigned shows{},focuses{};int lastMode{};HWND lastShow{},lastFocus{};
void ShowWindow(HWND child,int mode){++shows;lastShow=child;lastMode=mode;}
void SetFocus(HWND child){++focuses;lastFocus=child;}
''' + show + "\n" + focus + r'''
int main(){
 const HWND child=reinterpret_cast<HWND>(1);
 show_proxy_child_window(child,8);focus_proxy_child_window(child);
 return shows==1&&focuses==1&&lastMode==8&&lastShow==child&&lastFocus==child?0:1;
}
'''
        report = ROOT / "Artifacts/Reports/multicontext-layout-reentrancy-oct31/non-parented-capture-final-cpu"
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / "non_parented_visibility_contract.cpp"
        fixture.write_text(code, encoding="utf-8")
        results = compile_and_run(self, fixture, report, windows_flags=("/WX",))
        (report / "result.json").write_text(json.dumps(dict(configurations=results,
            source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            scope="Actual EPOCH_SINGLE_PARENT=false helper definitions with native-call recording ports; not full Engine profile integration."
        ), indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
