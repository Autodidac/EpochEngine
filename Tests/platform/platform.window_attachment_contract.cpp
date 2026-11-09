// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Test template: the Python runner inserts the unchanged production lease body.
#include <windows.h>
#include "core.stl_types.hpp"
#include <atomic>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
static_assert(!std::is_convertible_v<std::string,epochengine::string_view>);
namespace mock
{
    HWND child = reinterpret_cast<HWND>(1), host = reinterpret_cast<HWND>(2);
    HANDLE process = reinterpret_cast<HANDLE>(3);
    HWND parent{};
    DWORD childPid=20, childThread=200, hostPid=10, hostThread=100, error{};
    LONG_PTR style=WS_OVERLAPPEDWINDOW, exStyle=WS_EX_APPWINDOW;
    RECT rect{10,20,310,220}, hostRect{100,100,900,700};
    HANDLE property{};
    bool alive=true,hung=false,responsive=true,visible=true,failStyle=false,failParent=false,failProperty=false;
    bool failPosition{}, wrongClient{}, failZeroExStyle{}, mismatchExReadback{};
    DWORD applyExError{};bool failOwnerNoop{},loseAfterEx{},failReadAfterEx{},readExFailure{};unsigned ownerWrites{},exWrites{};
    std::string failureMessage;
    unsigned closes{},positions{},frames{},hides{},warnings{};
    UINT show=SW_SHOWNORMAL;
    void reset()
    {
        parent=nullptr;childPid=20;childThread=200;hostPid=10;hostThread=100;error=0;
        style=WS_OVERLAPPEDWINDOW;exStyle=WS_EX_APPWINDOW;rect={10,20,310,220};property=nullptr;
        alive=true;hung=false;responsive=true;visible=true;failStyle=false;failParent=false;failProperty=false;
        failPosition=wrongClient=failZeroExStyle=mismatchExReadback=false;failureMessage.clear();
        applyExError=0;failOwnerNoop=loseAfterEx=failReadAfterEx=readExFailure=false;ownerWrites=exWrites=0;
        closes=positions=frames=hides=warnings=0;show=SW_SHOWNORMAL;
    }
}
DWORD mock_GetCurrentProcessId(){return 10;}
DWORD mock_GetCurrentThreadId(){return 100;}
DWORD mock_GetWindowThreadProcessId(HWND hwnd,DWORD* pid)
{*pid=hwnd==mock::host?mock::hostPid:mock::childPid;return hwnd==mock::host?mock::hostThread:mock::childThread;}
HANDLE mock_OpenProcess(DWORD,BOOL,DWORD pid){return pid==mock::childPid?mock::process:nullptr;}
BOOL mock_CloseHandle(HANDLE handle){if(handle==mock::process)++mock::closes;return TRUE;}
DWORD mock_GetProcessId(HANDLE){return mock::childPid;}
DWORD mock_WaitForSingleObject(HANDLE,DWORD){return mock::alive?WAIT_TIMEOUT:WAIT_OBJECT_0;}
BOOL mock_IsWindow(HWND hwnd){return hwnd==mock::child||hwnd==mock::host;}
BOOL mock_IsHungAppWindow(HWND){return mock::hung;}
BOOL mock_IsWindowVisible(HWND){return mock::visible;}
LRESULT mock_SendMessageTimeoutW(HWND,UINT,WPARAM,LPARAM,UINT,UINT,DWORD_PTR*){return mock::responsive?1:0;}
HANDLE mock_GetPropW(HWND,LPCWSTR){
    if(mock::mismatchExReadback&&mock::exStyle==WS_EX_NOPARENTNOTIFY)mock::error=ERROR_INVALID_PARAMETER;
    return mock::property;}
BOOL mock_SetPropW(HWND,LPCWSTR,HANDLE value){if(mock::failProperty)return FALSE;mock::property=value;return TRUE;}
HANDLE mock_RemovePropW(HWND,LPCWSTR){auto old=mock::property;mock::property=nullptr;return old;}
void mock_SetLastError(DWORD error){mock::error=error;}
DWORD mock_GetLastError(){return mock::error;}
LONG_PTR mock_GetWindowLongPtrW(HWND,int index){
    if(index==GWL_EXSTYLE&&mock::readExFailure){mock::error=ERROR_INVALID_PARAMETER;return 0;}
    if(index==GWL_EXSTYLE&&mock::mismatchExReadback&&mock::exStyle==WS_EX_NOPARENTNOTIFY)return WS_EX_TOPMOST;
    return index==GWL_STYLE?mock::style:index==GWL_EXSTYLE?mock::exStyle:0;}
LONG_PTR mock_SetWindowLongPtrW(HWND,int index,LONG_PTR value)
{
    if(index==GWL_EXSTYLE)++mock::exWrites;
    if(mock::failStyle){mock::failStyle=false;mock::error=ERROR_ACCESS_DENIED;return 0;}
    if(index==GWL_EXSTYLE&&mock::failZeroExStyle){mock::failZeroExStyle=false;mock::error=ERROR_INVALID_PARAMETER;return 0;}
    if(index==GWLP_HWNDPARENT){++mock::ownerWrites;if(mock::failOwnerNoop)mock::error=ERROR_INVALID_WINDOW_HANDLE;return 0;}
    if(index==GWL_EXSTYLE&&mock::applyExError){auto old=mock::exStyle;mock::exStyle=value;mock::error=mock::applyExError;mock::applyExError=0;
        if(mock::loseAfterEx)++mock::childThread;if(mock::failReadAfterEx)mock::readExFailure=true;return old;}
    auto& target=index==GWL_STYLE?mock::style:mock::exStyle;auto old=target;target=value;return old;
}
HWND mock_GetWindow(HWND,UINT){return nullptr;}
HWND mock_GetParent(HWND){return mock::parent;}
HWND mock_SetParent(HWND,HWND parent)
{
    if(mock::failParent){mock::failParent=false;mock::error=ERROR_ACCESS_DENIED;return nullptr;}
    auto old=mock::parent;mock::parent=parent;return old;
}
BOOL mock_GetWindowRect(HWND hwnd,RECT* rect){*rect=hwnd==mock::host?mock::hostRect:mock::rect;return TRUE;}
BOOL mock_GetClientRect(HWND hwnd,RECT* rect)
{auto source=hwnd==mock::host?mock::hostRect:mock::rect;*rect={0,0,source.right-source.left+(hwnd==mock::child&&mock::wrongClient?1:0),source.bottom-source.top};return TRUE;}
BOOL mock_GetWindowPlacement(HWND,WINDOWPLACEMENT* placement)
{placement->showCmd=mock::show;placement->rcNormalPosition=mock::rect;return TRUE;}
BOOL mock_SetWindowPlacement(HWND,const WINDOWPLACEMENT* placement){mock::show=placement->showCmd;return TRUE;}
BOOL mock_SetWindowPos(HWND,HWND,int x,int y,int w,int h,UINT flags)
{
    if(mock::failPosition){mock::failPosition=false;mock::error=ERROR_INVALID_PARAMETER;return FALSE;}
    ++mock::positions;if(flags&SWP_FRAMECHANGED)++mock::frames;
    if(mock::parent){x+=mock::hostRect.left;y+=mock::hostRect.top;}
    mock::rect={x,y,x+w,y+h};if(flags&SWP_SHOWWINDOW)mock::visible=true;return TRUE;
}
BOOL mock_ScreenToClient(HWND,POINT* point){point->x-=mock::hostRect.left;point->y-=mock::hostRect.top;return TRUE;}
BOOL mock_ClientToScreen(HWND,POINT* point){point->x+=mock::hostRect.left;point->y+=mock::hostRect.top;return TRUE;}
BOOL mock_EqualRect(const RECT* a,const RECT* b){return a->left==b->left&&a->top==b->top&&a->right==b->right&&a->bottom==b->bottom;}
BOOL mock_ShowWindowAsync(HWND,int){++mock::hides;mock::visible=false;return TRUE;}
namespace epochengine::core::log { void warn(epochengine::string_view,epochengine::string_view message){
    const std::string_view text{message.data,message.size};
    ++mock::warnings;if(text.starts_with("Attachment failure stage=")||text.starts_with("Attachment attribute reconciled stage="))mock::failureMessage=text;} }
// API_REDIRECTS
namespace epochengine::platform
{
// PRODUCTION_HANDLE
// PRODUCTION_INTERFACE
namespace
{
// PRODUCTION_OWNER
}
}
#define CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;}while(false)
int main()
{
    using namespace epochengine::platform;
    // A managed child needs the same sibling/descendant paint isolation as
    // engine-created contexts, regardless of its former top-level flags.
    for (const LONG_PTR inherited : {LONG_PTR{}, LONG_PTR{WS_CLIPSIBLINGS},
            LONG_PTR{WS_CLIPCHILDREN}, LONG_PTR{WS_CLIPSIBLINGS | WS_CLIPCHILDREN}})
    {
        mock::reset();
        const LONG_PTR original = WS_OVERLAPPEDWINDOW | inherited;
        mock::style = original;
        {
            Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
            constexpr LONG_PTR childFlags = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            CHECK((mock::style & childFlags) == childFlags);
            CHECK((mock::style & WS_OVERLAPPEDWINDOW) == 0);
            CHECK(lease.place(4,5,100,80) && (mock::style & childFlags) == childFlags);
            lease.restore();CHECK(mock::style == original);
        }
        CHECK(mock::closes == 1);
    }
    mock::reset();
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        const auto writes=mock::exWrites;mock::readExFailure=true;
        CHECK(!lease.write_long(GWL_EXSTYLE,0));
        CHECK(mock::exWrites==writes&&mock::exStyle==WS_EX_NOPARENTNOTIFY); // Failed pre-read never mutates.
        CHECK(mock::failureMessage.find("failure=pre_read")!=std::string::npos);
        mock::readExFailure=false;
    }
    // Actual native evidence: the changed style is present despite a zero
    // setter return/error87. Only this narrow style postcondition is reconciled.
    mock::reset();mock::exStyle=0;mock::applyExError=ERROR_INVALID_PARAMETER;
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        CHECK(lease.placement_current()&&mock::exStyle==WS_EX_NOPARENTNOTIFY);
        CHECK(mock::failureMessage.starts_with("Attachment attribute reconciled stage="));
        CHECK(mock::failureMessage.find("setter_error=87")!=std::string::npos);
    }
    // Matching readback never clears access denial, another error, lost
    // process/thread/cookie identity, or an unreadable postcondition.
    for(int failure=0;failure<4;++failure){
        mock::reset();mock::exStyle=0;mock::applyExError=failure==0?ERROR_ACCESS_DENIED:failure==1?ERROR_INVALID_WINDOW_HANDLE:ERROR_INVALID_PARAMETER;
        mock::loseAfterEx=failure==2;mock::failReadAfterEx=failure==3;
        {Win32WindowAttachment lease;CHECK(!lease.attach(mock::child,mock::host,20));}
    }
    mock::reset();mock::failOwnerNoop=true;
    {Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));lease.restore();
     CHECK(mock::ownerWrites==0&&!mock::property&&!mock::parent);}
    CHECK(attribute_hex(0)=="0x0"&&attribute_hex(WS_EX_NOPARENTNOTIFY)=="0x4");
    if constexpr(sizeof(LONG_PTR)==8u)
        CHECK(attribute_hex(static_cast<LONG_PTR>(std::uint64_t{0xffffffff80000000ull}))=="0xffffffff80000000");
    mock::reset();
    {
        Win32WindowAttachment lease;
        CHECK(lease.attach(mock::child,mock::host,20));
        CHECK(lease.same_window()&&lease.attached()&&lease.placement_current());
        CHECK(lease.window().value==1&&lease.host().value==2&&lease.process_id()==20);
        auto positions=mock::positions;
        CHECK(lease.place(0,0,800,600)&&mock::positions==positions&&mock::frames==0);
        CHECK(!lease.place(-1,0,10,10)&&!lease.placement_current());
        CHECK(lease.place(4,5,100,80)&&lease.placement_current());
        ++mock::rect.right;CHECK(!lease.placement_current());--mock::rect.right;
        lease.restore();CHECK(!mock::property&&!mock::parent&&mock::style==WS_OVERLAPPEDWINDOW);
        CHECK(mock::rect.left==10&&mock::rect.right==310&&mock::frames==1);
        positions=mock::positions;lease.restore();CHECK(mock::positions==positions);
    }
    CHECK(mock::closes==1);
    mock::reset();mock::exStyle=0;
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        CHECK(mock::exStyle==WS_EX_NOPARENTNOTIFY&&lease.placement_current());
        lease.restore();CHECK(mock::exStyle==0&&!mock::parent&&!mock::property);
    }
    for(bool mismatch:{false,true})
    {
        mock::reset();mock::exStyle=0;
        mock::failZeroExStyle=!mismatch;mock::mismatchExReadback=mismatch;
        {Win32WindowAttachment lease;CHECK(!lease.attach(mock::child,mock::host,20));
         CHECK(mock::failureMessage.find("admission_child_extended_style")!=std::string::npos);
         CHECK(mock::failureMessage.find(mismatch?"failure=readback_mismatch":"failure=setter")!=std::string::npos);
         CHECK(mock::failureMessage.find(mismatch?"setter_error=0":"setter_error=87")!=std::string::npos);
         CHECK(mock::failureMessage.find("requested=0x4")!=std::string::npos);
         if(mismatch)CHECK(mock::failureMessage.find("observed=0x8")!=std::string::npos);
        }
        CHECK(mock::exStyle==0&&!mock::parent&&!mock::property);
    }
    for(int failure=0;failure<7;++failure)
    {
        mock::reset();
        {
            Win32WindowAttachment lease;
            if(failure==0)mock::hung=true;
            if(failure==1)mock::responsive=false;
            if(failure==2)mock::childPid=10;
            if(failure==3)mock::failStyle=true;
            if(failure==4)mock::failParent=true;
            if(failure==5)mock::failProperty=true;
            if(failure==6)mock::style=WS_CHILD;
            CHECK(!lease.attach(mock::child,mock::host,failure==2?10:20));
            constexpr std::string_view stages[]={"admission_hung_window","admission_health_probe",
                "admission_thread_process_identity","admission_child_style","admission_set_parent",
                "admission_set_lease_property","admission_foreign_hierarchy"};
            CHECK(mock::failureMessage.find(stages[failure])!=std::string::npos);
            if(failure==3||failure==4)CHECK(mock::failureMessage.find("native_error=5")!=std::string::npos);
        }
        CHECK(mock::property==nullptr&&mock::parent==nullptr);
        CHECK(mock::closes==(failure<3?0u:1u));
        if(failure!=6)CHECK(mock::style==WS_OVERLAPPEDWINDOW);
    }
    mock::reset();
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        lease.hide_during_retirement(true);lease.restore();CHECK(mock::show==SW_HIDE&&mock::hides==1);
    }
    mock::reset();
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        mock::property=reinterpret_cast<HANDLE>(999);
        auto positions=mock::positions;lease.restore();
        CHECK(mock::positions==positions&&mock::property==reinterpret_cast<HANDLE>(999));
    }
    CHECK(mock::closes==1);
    mock::reset();
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        mock::failPosition=true;
        CHECK(!lease.place(4,5,100,80)&&!lease.placement_current());
        CHECK(mock::failureMessage.find("placement_set_window_pos native_error=87")!=std::string::npos);
        CHECK(mock::failureMessage.find("slot=4,5,100,80")!=std::string::npos);
        const auto originalFailure=mock::failureMessage;lease.restore();
        CHECK(mock::failureMessage==originalFailure&&!mock::property&&!mock::parent);
    }
    mock::reset();
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        mock::wrongClient=true;
        CHECK(!lease.place(4,5,100,80)&&!lease.placement_current());
        CHECK(mock::failureMessage.find("placement_geometry_mismatch native_error=0")!=std::string::npos);
        CHECK(mock::failureMessage.find("client=101,80")!=std::string::npos);
        mock::wrongClient=false;
        for(unsigned i=0;i<64;++i)CHECK(!lease.place(-1,0,10,10));
        const auto warnings=mock::warnings;
        CHECK(warnings>0&&warnings<=32);
        CHECK(!lease.place(-1,0,10,10)&&mock::warnings==warnings); // Bounded even on repeated refusals.
    }
    mock::reset();
    {
        Win32WindowAttachment lease;CHECK(lease.attach(mock::child,mock::host,20));
        mock::alive=false;auto positions=mock::positions;lease.restore();CHECK(mock::positions==positions);
    }
    CHECK(mock::closes==1);
    mock::reset();
    g_externalAttachmentCookie.store(~std::uintptr_t{0});
    {Win32WindowAttachment lease;CHECK(!lease.attach(mock::child,mock::host,20));}
    CHECK(mock::property==nullptr&&mock::closes==1);
    return 0;
}
