// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
using HWND=int; using UINT=unsigned; using WPARAM=unsigned; using LPARAM=int;
struct POINT {long x{},y{};}; struct RECT {long left{},top{},right{},bottom{};};
constexpr int FALSE=0;
enum class ContextType {None,RayLib,SDL,SFML,Vulkan,OpenGL,DirectX,Software};
enum class RoutedPanelDockTarget {none,left_tabs,left_context,right_tabs,right_context,bottom_left_tabs,bottom_right_tabs};
struct Context {ContextType type;};
struct WindowData {int hwnd; std::string guiRoute; bool isFloating=false;
std::atomic<unsigned> dockTargetPreference{0};std::atomic<int> dockGridOrder{-1};std::shared_ptr<Context> context;};
auto typed_context(const std::shared_ptr<Context>& context){return context;}
RECT screen_client_rect(HWND){return {0,0,600,400};}
bool point_in_rect(RECT r,POINT p){return p.x>=r.left&&p.x<r.right&&p.y>=r.top&&p.y<r.bottom;}
int clamp_positive(int x){return std::max(x,1);}
int dock_slot_handle(WindowData* window,HWND){return window->isFloating?0:window->hwnd;}
class MultiContextManager {public:
int parent=10;std::recursive_mutex windowsMutex;std::vector<std::unique_ptr<WindowData>> windows;
WindowData* findWindowByHWND(int id){for(auto& w:windows)if(w->hwnd==id)return w.get();return nullptr;}
void ReorderDockedWindow(HWND,POINT);
};
ORDER_FUNCTION
MOVE_FUNCTION
struct DragState {bool dragging=false;HWND draggedWindow=0,originalParent=0;int opacity=0;};
DragState state;int capture=0,releases=0,clears=0,invalidates=0,restores=0;
namespace epochengine::core {DragState& Drag(){return state;}}
void cancel_native_context_drag(HWND) noexcept;
void restore_native_drag_opacity(DragState& drag){++restores;drag.opacity=0;}
void clear_routed_panel_tab_drop(){++clears;}
int GetCapture(){return capture;}
void ReleaseCapture(){++releases;const auto old=capture;capture=0;cancel_native_context_drag(old);}
bool IsWindow(HWND h){return h!=0;}
void InvalidateRect(HWND,void*,int){++invalidates;}
CANCEL_FUNCTION
enum {WM_LBUTTONDOWN,WM_MOUSEMOVE,WM_LBUTTONUP,WM_NCLBUTTONDOWN,WM_NCMOUSEMOVE,WM_NCLBUTTONUP};
constexpr unsigned HTCAPTION=2;
int guiEvents=0,dragEvents=0;bool hotspot=false;
bool native_class_owns_gui_input(HWND hwnd) noexcept;
bool is_dock_drag_hotspot(HWND,LPARAM){return hotspot;}
void forward_gui_input_message(HWND,UINT,WPARAM,LPARAM){++guiEvents;}
int DefSubclassProc(HWND,UINT,WPARAM,LPARAM){return 7;}
namespace epochengine::core {struct MultiContextManager {
static int ChildProc(HWND,UINT,WPARAM,LPARAM){++dragEvents;return 9;}};}
using LONG_PTR=std::intptr_t;constexpr int GCLP_WNDPROC=-24;
LONG_PTR GetClassLongPtrW(HWND hwnd,int){return hwnd==77?reinterpret_cast<LONG_PTR>(epochengine::core::MultiContextManager::ChildProc):0;}
OWNS_FUNCTION
int route(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){DOCK_ARM CAPTION_ARM default:return 0;}}
bool admits_release(bool routedPane,bool hasDockTarget,bool releaseInsideParent){ RELEASE_POLICY return wantsRedock; }
#define VERIFY(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(){
VERIFY(native_class_owns_gui_input(77));VERIFY(!native_class_owns_gui_input(3));
MultiContextManager manager;
for(int i=0;i<6;++i){auto w=std::make_unique<WindowData>();w->hwnd=i+1;
w->context=std::make_shared<Context>(Context{static_cast<ContextType>(i+1)});manager.windows.push_back(std::move(w));}
auto ordered=[&]{std::vector<WindowData*> result;for(auto& w:manager.windows)result.push_back(w.get());
std::stable_sort(result.begin(),result.end(),[](auto* a,auto* b){return native_dock_order(a)<native_dock_order(b);});return result;};
VERIFY(ordered()[0]->hwnd==1 && ordered()[5]->hwnd==6);
manager.ReorderDockedWindow(1,{590,390});
VERIFY(ordered()[5]->hwnd==1 && ordered()[0]->hwnd==2);
for(int i=0;i<6;++i)VERIFY(manager.windows[i]->hwnd==i+1);
manager.ReorderDockedWindow(1,{201,1});
VERIFY(ordered()[1]->hwnd==1 && ordered()[0]->hwnd==2);
manager.ReorderDockedWindow(1,{-1,1});VERIFY(ordered()[1]->hwnd==1);
manager.findWindowByHWND(6)->isFloating=true;
manager.ReorderDockedWindow(6,{1,1});VERIFY(ordered()[0]->hwnd==6);
state={true,3,10,1};capture=3;cancel_native_context_drag(4);VERIFY(state.dragging&&releases==0);
cancel_native_context_drag(3);VERIFY(!state.dragging&&state.draggedWindow==0&&releases==1&&restores==1&&clears==1&&invalidates==1);
cancel_native_context_drag(3);VERIFY(releases==1);
hotspot=true;VERIFY(route(3,WM_LBUTTONDOWN,0,0)==9&&guiEvents==0&&dragEvents==1);
state={true,3,10,0};hotspot=false;VERIFY(route(3,WM_LBUTTONUP,0,0)==9&&guiEvents==0&&dragEvents==2);
state={};VERIFY(route(3,WM_LBUTTONDOWN,0,0)==7&&guiEvents==1);
VERIFY(route(3,WM_NCLBUTTONDOWN,HTCAPTION,0)==9&&guiEvents==1);
VERIFY(route(3,WM_NCLBUTTONDOWN,99,0)==7&&guiEvents==1);
VERIFY(admits_release(false,false,true));
VERIFY(!admits_release(false,true,false));
VERIFY(!admits_release(true,false,true));
VERIFY(admits_release(true,true,true));
return 0;
}
