// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Recorded native calls only: no OS window, process, renderer or model starts.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace epochengine::core::cli { inline bool trace_menu_button0_rect{}; }
unsigned placement_trace_calls{};
std::string last_placement_trace;
namespace epochengine::logger { void info(std::string_view tag, std::string_view message) {
 if(tag=="Context.ExternalViewport"){++placement_trace_calls;last_placement_trace=message;} } }
using HWND = void*;
using DWORD = unsigned long;
struct RECT { long left{}, top{}, right{}, bottom{}; };
struct POINT { long x{}, y{}; };
constexpr int SW_HIDE=0, GW_CHILD=5;
constexpr unsigned SWP_NOMOVE=1, SWP_NOSIZE=2, SWP_NOACTIVATE=4, SWP_ASYNCWINDOWPOS=8;
const HWND HWND_TOP{};
HWND handle(std::uintptr_t id) { return reinterpret_cast<HWND>(id); }
struct NativeWindow { HWND parent{}; RECT client{}; POINT screen{}; bool visible{true}; DWORD pid{7}, thread{1}; };
std::unordered_map<HWND, NativeWindow> native;
unsigned hide_calls{}, raise_calls{}, synchronous_foreign_raises{};
HWND first_child{};
bool IsWindowVisible(HWND h) { return native.contains(h) && native[h].visible; }
bool IsChild(HWND p, HWND h) { return native.contains(h) && native[h].parent==p; }
bool GetClientRect(HWND h, RECT* r) { if(!native.contains(h))return false; *r=native[h].client; return true; }
bool ClientToScreen(HWND h, POINT* p) { if(!native.contains(h))return false; p->x+=native[h].screen.x;p->y+=native[h].screen.y;return true; }
bool ScreenToClient(HWND h, POINT* p) { if(!native.contains(h))return false;p->x-=native[h].screen.x;p->y-=native[h].screen.y;return true; }
bool ShowWindowAsync(HWND h,int) { ++hide_calls;native[h].visible=false;return true; }
HWND GetWindow(HWND,int) { return first_child; }
bool SetWindowPos(HWND h, HWND,int,int,int,int,unsigned flags) {
 ++raise_calls;if(!(flags&SWP_ASYNCWINDOWPOS))++synchronous_foreign_raises;
 first_child=h;return true; }
DWORD GetCurrentThreadId() { return 1; }
DWORD GetCurrentProcessId() { return 7; }
DWORD GetWindowThreadProcessId(HWND h,DWORD* p) { if(!native.contains(h))return 0;*p=native[h].pid;return native[h].thread; }
constexpr std::uint64_t MAXDWORD=0xffffffffu;
namespace epochengine::platform::window_adapter {
struct WindowHandle { std::uintptr_t value{}; };
struct INativeWindowAttachment {
 virtual ~INativeWindowAttachment() = default;
 virtual bool attached() const = 0;
 virtual WindowHandle window() const = 0;
 virtual WindowHandle host() const = 0;
 virtual std::uint64_t process_id() const = 0;
 virtual bool place(int,int,int,int) = 0;
 virtual bool placement_current() const = 0;
 virtual void restore() = 0;
};
}
namespace epochengine::core {
enum class ScenePreviewMode {None,Editor,Runtime};
enum class BackendLifecycleState {ready,failed};
struct RenderViewport { int x{},y{},width{},height{}; };
struct Context {
 void* native_window{}; int width{1000},height{700}; RenderViewport viewport{100,80,600,420};
 bool overlay{}; ScenePreviewMode mode{ScenePreviewMode::Editor};
 bool gui_overlay_priority() const {return overlay;}
 ScenePreviewMode scene_preview_mode() const {return mode;}
 RenderViewport scene_viewport() const {return viewport;}
};
struct WindowData {
 HWND hwnd{};std::string guiRoute;
 std::atomic<bool> running{true},should_close{};
 BackendLifecycleState state{BackendLifecycleState::ready};
 bool backend_ready() const {return state==BackendLifecycleState::ready;}
 void set_backend_lifecycle(BackendLifecycleState s) {state=s;}
};
struct MultiContextManager {
 HWND parent{handle(1)};DWORD uiThreadId{1};std::atomic<bool> running{true};
 mutable std::recursive_mutex windowsMutex;
 std::vector<std::unique_ptr<WindowData>> windows;
 bool IsExternalProcessWindowAttached(std::uintptr_t,std::uint64_t,std::string_view) const noexcept;
 bool UpdateExternalProcessWindowViewport(std::uintptr_t,std::uint64_t,std::string_view,bool);
};
}
using namespace epochengine::core;
std::mutex g_externalAttachmentsMutex;
PRESENTATION_TYPE
std::unordered_map<const WindowData*,ExternalProcessPresentation> g_externalAttachments;
PRESENTATION_LOOKUP
ATTACHMENT_LOOKUP
VIEWPORT_PLACEMENT
ATTACHMENT_CHECK
VIEWPORT_UPDATE
void actual_grid_filter(HWND parent,std::vector<WindowData*>& dockedWindows) {
GRID_FILTER
 placeViewportWindows();
}
struct Lease final:epochengine::platform::window_adapter::INativeWindowAttachment {
 HWND child{handle(3)},parent{handle(1)};std::uint64_t pid{42};
 RECT placed{};unsigned places{},restores{};bool live{true},current{true},accept{true};
 bool attached() const override{return live;}
 epochengine::platform::window_adapter::WindowHandle window() const override{return{reinterpret_cast<std::uintptr_t>(child)};}
 epochengine::platform::window_adapter::WindowHandle host() const override{return{reinterpret_cast<std::uintptr_t>(parent)};}
 std::uint64_t process_id() const override{return pid;}
 bool place(int x,int y,int width,int height) override {
  ++places;if(!accept)return false;placed={x,y,x+width,y+height};native[child].visible=true;current=true;return true;}
 bool placement_current() const override{return current;}
 void restore() override{++restores;}
};
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main() {
 const auto parent=handle(1),owner=handle(2),child=handle(3);
 native[parent]={{},{0,0,1600,1100},{30,40},true};
 native[owner]={parent,{0,0,1250,875},{150,115},true};
 native[child]={parent,{0,0,20,20},{30,40},true,42};
 auto context=std::make_shared<Context>();context->native_window=owner;
 auto lease=std::make_shared<Lease>();
 ExternalProcessPresentation presentation{.attachment=lease,.viewport_context=context,.in_viewport=true,.visible=true};
 CHECK(place_external_viewport_attachment(parent,presentation));
 CHECK(lease->placed.left==245&&lease->placed.top==175&&lease->placed.right==995&&lease->placed.bottom==700);
 CHECK(raise_calls==1&&first_child==child); // Nonzero host origin +125% native scale.
 CHECK(synchronous_foreign_raises==0); // Foreign UI must never be joined for z-order refresh.
 CHECK(place_external_viewport_attachment(parent,presentation)&&raise_calls==1);
 CHECK(placement_trace_calls==0);
 epochengine::core::cli::trace_menu_button0_rect=true;
 for(unsigned i=0;i<40;++i)CHECK(place_external_viewport_attachment(parent,presentation));
 CHECK(placement_trace_calls==32&&last_placement_trace.find("stages=5")!=std::string::npos);
 CHECK(last_placement_trace.find("placement_ms=")!=std::string::npos
  &&last_placement_trace.find("z_order_ms=")!=std::string::npos
  &&last_placement_trace.find("verification_ms=")!=std::string::npos
  &&last_placement_trace.find("outcome=placed")!=std::string::npos);
 epochengine::core::cli::trace_menu_button0_rect=false;
 // Compressed client retains the same logical viewport and cannot cover its tools.
 native[owner].client={0,0,300,210};
 CHECK(place_external_viewport_attachment(parent,presentation));
 CHECK(lease->placed.left==150&&lease->placed.top==99&&lease->placed.right==330&&lease->placed.bottom==225);
 // Clip to source client and then parent, including a partly offscreen owner.
 native[owner].screen={-20,10};context->viewport={-50,-20,3000,2000};
 CHECK(place_external_viewport_attachment(parent,presentation));
 CHECK(lease->placed.left==0&&lease->placed.top==0&&lease->placed.right==250&&lease->placed.bottom==180);
 const auto places=lease->places;
 context->viewport={1,1,0,100};CHECK(place_external_viewport_attachment(parent,presentation));
 CHECK(!native[child].visible&&lease->places==places);
 context->viewport={100,80,600,420};native[owner].screen={150,115};
 CHECK(place_external_viewport_attachment(parent,presentation)&&native[child].visible);
 context->overlay=true;CHECK(place_external_viewport_attachment(parent,presentation)&&!native[child].visible);
 context->overlay=false;CHECK(place_external_viewport_attachment(parent,presentation)&&native[child].visible);
 // Covered Editor stops drawing its preview. Its admitted project still owns
 // the viewport: repeated host/frame observations must not hide/show it.
 context->mode=ScenePreviewMode::None;
 const auto coveredHideCalls=hide_calls;
 for(unsigned frame=0;frame<32;++frame){
  CHECK(place_external_viewport_attachment(parent,presentation)&&native[child].visible);
  CHECK(hide_calls==coveredHideCalls);
 }
 context->overlay=true;CHECK(place_external_viewport_attachment(parent,presentation)&&!native[child].visible);
 context->overlay=false;CHECK(place_external_viewport_attachment(parent,presentation)&&native[child].visible);
 context->mode=ScenePreviewMode::Editor;presentation.visible=false;
 CHECK(place_external_viewport_attachment(parent,presentation)&&!native[child].visible);
 presentation.visible=true;CHECK(place_external_viewport_attachment(parent,presentation)&&native[child].visible);
 native[owner].parent={};CHECK(place_external_viewport_attachment(parent,presentation)&&!native[child].visible);
 native[owner].parent=parent;lease->accept=false;CHECK(!place_external_viewport_attachment(parent,presentation));lease->accept=true;
 MultiContextManager manager;auto foreign=std::make_unique<WindowData>();foreign->hwnd=child;foreign->guiRoute="project_runtime";
 auto* projectWindow=foreign.get();manager.windows.push_back(std::move(foreign));
 g_externalAttachments[projectWindow]=presentation;
 CHECK(manager.UpdateExternalProcessWindowViewport(3,42,"project_runtime",true)&&native[child].visible);
 auto before=lease->places;
 CHECK(!manager.UpdateExternalProcessWindowViewport(3,43,"project_runtime",true));
 CHECK(!manager.UpdateExternalProcessWindowViewport(3,42,"wrong_route",true));
 CHECK(!manager.UpdateExternalProcessWindowViewport(4,42,"project_runtime",true)&&lease->places==before);
 CHECK(manager.UpdateExternalProcessWindowViewport(3,42,"project_runtime",false)&&!native[child].visible);
 CHECK(manager.UpdateExternalProcessWindowViewport(3,42,"project_runtime",true)&&native[child].visible);
 // Only viewport attachments leave the grid. A self-iteration candidate keeps
 // its independent tile; an expired viewport never silently becomes one.
 WindowData editor,candidate;candidate.hwnd=handle(4);
 auto candidateLease=std::make_shared<Lease>();candidateLease->child=handle(4);
 g_externalAttachments[&candidate]={.attachment=candidateLease};
 std::vector<WindowData*> grid{&editor,projectWindow,&candidate};
 actual_grid_filter(parent,grid);CHECK(grid.size()==2&&grid[0]==&editor&&grid[1]==&candidate);
 presentation.viewport_context.reset();g_externalAttachments[projectWindow]=presentation;
 grid={&editor,projectWindow,&candidate};actual_grid_filter(parent,grid);
 CHECK(grid.size()==2&&!native[child].visible&&candidateLease->places==0);
 CHECK(projectWindow->backend_ready());
 lease->live=false;CHECK(!manager.UpdateExternalProcessWindowViewport(3,42,"project_runtime",true));
 return 0;
}
