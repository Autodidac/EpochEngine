// SPDX-License-Identifier: MPL-2.0
module;
#include "core.stl_types.hpp"
#include <atomic>
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif
module platform.window_attachment;
import core.error;
import core.log;
import platform.window;

namespace epochengine::platform::window_adapter
{
#if defined(_WIN32)
namespace
{
    constexpr wchar_t kExternalAttachmentProp[] = L"Epoch.ExternalProcessAttachment.v1";
    constexpr LONG_PTR kExternalFrameStyles = WS_POPUP | WS_OVERLAPPEDWINDOW | WS_MAXIMIZE | WS_MINIMIZE;
    constexpr LONG_PTR kExternalFrameExStyles = WS_EX_APPWINDOW | WS_EX_TOOLWINDOW
        | WS_EX_TOPMOST | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME;
    std::atomic<std::uintptr_t> g_externalAttachmentCookie{};

    [[nodiscard]] std::uintptr_t next_external_attachment_cookie() noexcept
    {
        auto current = g_externalAttachmentCookie.load(std::memory_order_relaxed);
        for (;;)
        {
            // Exhaustion is a refusal, never wraparound/reuse of a prior cookie.
            if (current == ~std::uintptr_t{0}) return 0;
            const auto next = current + 1;
            if (g_externalAttachmentCookie.compare_exchange_weak(
                    current, next, std::memory_order_relaxed))
                return next;
        }
    }

    [[nodiscard]] constexpr bool external_docked_styles(
        LONG_PTR style, LONG_PTR exStyle) noexcept
    {
        return (style & WS_CHILD) != 0 && (style & kExternalFrameStyles) == 0
            && (exStyle & kExternalFrameExStyles) == 0
            && (exStyle & WS_EX_NOPARENTNOTIFY) != 0;
    }
    static_assert(external_docked_styles(WS_CHILD, WS_EX_NOPARENTNOTIFY));
    static_assert(!external_docked_styles(WS_CHILD | WS_POPUP, WS_EX_NOPARENTNOTIFY));
    static_assert(!external_docked_styles(WS_CHILD | WS_CAPTION, WS_EX_NOPARENTNOTIFY));
    static_assert(!external_docked_styles(WS_CHILD | WS_MAXIMIZE, WS_EX_NOPARENTNOTIFY));
    static_assert(!external_docked_styles(WS_CHILD, WS_EX_NOPARENTNOTIFY | WS_EX_APPWINDOW));

    // This host owns an attachment lease, never the foreign window or renderer.
    // A retained process handle pins the process incarnation. The opaque HWND
    // cookie detects ordinary destruction/reuse without exposing a host address
    // or handle capability. A child can read/modify its window properties: this
    // is lifecycle correlation, not trusted identity or an isolation boundary.
    // No subclass callback/function pointer is installed in the other process.
    struct Win32WindowAttachment final : INativeWindowAttachment
    {
        HWND hwnd{};
        HWND parent{};
        HWND originalOwner{};
        HANDLE process{};
        DWORD processId{};
        DWORD threadId{};
        std::uintptr_t cookie{};
        LONG_PTR originalStyle{};
        LONG_PTR originalExStyle{};
        RECT originalRect{};
        RECT admittedSlot{};
        WINDOWPLACEMENT originalPlacement{ sizeof(WINDOWPLACEMENT) };
        bool propertyOwned{};
        bool nativeChanged{};
        bool slotVerified{};
        std::atomic<bool> hideDuringRetirement{false};

        WindowHandle window() const noexcept override { return {reinterpret_cast<std::uintptr_t>(hwnd)}; }
        WindowHandle host() const noexcept override { return {reinterpret_cast<std::uintptr_t>(parent)}; }
        std::uint64_t process_id() const noexcept override { return processId; }
        void hide_during_retirement(bool hide) noexcept override
        { hideDuringRetirement.store(hide, std::memory_order_release); }

        Win32WindowAttachment() = default;
        Win32WindowAttachment(const Win32WindowAttachment&) = delete;
        Win32WindowAttachment& operator=(const Win32WindowAttachment&) = delete;
        ~Win32WindowAttachment()
        {
            restore();
            if (process) ::CloseHandle(process);
        }

        [[nodiscard]] bool same_window() const noexcept override
        {
            DWORD observedPid{};
            return process && ::WaitForSingleObject(process, 0) == WAIT_TIMEOUT
                && ::GetProcessId(process) == processId
                && ::GetWindowThreadProcessId(hwnd, &observedPid) == threadId
                && observedPid == processId && threadId != 0
                && (!propertyOwned || ::GetPropW(hwnd, kExternalAttachmentProp)
                    == reinterpret_cast<HANDLE>(cookie));
        }

        [[nodiscard]] bool write_long(int index, LONG_PTR value) const noexcept
        {
            if (!same_window()) return false;
            ::SetLastError(ERROR_SUCCESS);
            const LONG_PTR previous = ::SetWindowLongPtrW(hwnd, index, value);
            return (previous != 0 || ::GetLastError() == ERROR_SUCCESS)
                && same_window() && ::GetWindowLongPtrW(hwnd, index) == value;
        }

        [[nodiscard]] bool attached() const noexcept override
        {
            RECT client{};
            return propertyOwned && same_window() && ::IsWindow(parent)
                && ::GetParent(hwnd) == parent
                && external_docked_styles(::GetWindowLongPtrW(hwnd, GWL_STYLE),
                    ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE))
                && ::GetClientRect(hwnd, &client)
                && client.right > client.left && client.bottom > client.top;
        }

        [[nodiscard]] bool place(int x, int y, int width, int height) noexcept override
        {
            slotVerified = false;
            if (!attached() || ::IsHungAppWindow(hwnd) != FALSE
                || x < 0 || y < 0 || width <= 0 || height <= 0)
                return false;
            RECT parentClient{};
            if (!::GetClientRect(parent, &parentClient)
                || x > parentClient.right - width || y > parentClient.bottom - height)
                return false;
            RECT previous{};
            POINT previousOrigin{};
            const bool haveRectangle = ::GetWindowRect(hwnd, &previous) != FALSE;
            if (haveRectangle) previousOrigin = {previous.left, previous.top};
            const bool unchanged = haveRectangle
                && ::ScreenToClient(parent, &previousOrigin) != FALSE
                && previousOrigin.x == x && previousOrigin.y == y
                && previous.right - previous.left == width
                && previous.bottom - previous.top == height
                && ::IsWindowVisible(hwnd) != FALSE;
            // Style changes are handled at attachment, not every slot refresh.
            // Repeating FRAMECHANGED needlessly invalidates compositor surfaces.
            if ((!unchanged && !::SetWindowPos(hwnd, nullptr, x, y, width, height,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW))
                || !attached())
                return false;
            RECT client{}, outer{};
            POINT origin{};
            if (!::GetClientRect(hwnd, &client) || !::GetWindowRect(hwnd, &outer)
                || !::ClientToScreen(parent, &origin))
                return false;
            // Child chrome is removed: both the client area and native bounds
            // must agree with the admitted slot, not merely be nonzero.
            slotVerified = client.right - client.left == width && client.bottom - client.top == height
                && outer.left == origin.x + x && outer.top == origin.y + y
                && outer.right - outer.left == width && outer.bottom - outer.top == height
                && attached();
            if (slotVerified) admittedSlot = { x, y, x + width, y + height };
            return slotVerified;
        }

        [[nodiscard]] bool placement_current() const noexcept override
        {
            RECT client{}, outer{};
            POINT origin{};
            return slotVerified && attached()
                && ::GetClientRect(hwnd, &client) && ::GetWindowRect(hwnd, &outer)
                && ::ClientToScreen(parent, &origin)
                && client.right - client.left == admittedSlot.right - admittedSlot.left
                && client.bottom - client.top == admittedSlot.bottom - admittedSlot.top
                && outer.left == origin.x + admittedSlot.left
                && outer.top == origin.y + admittedSlot.top
                && outer.right == origin.x + admittedSlot.right
                && outer.bottom == origin.y + admittedSlot.bottom && attached();
        }

        [[nodiscard]] bool attach(HWND candidate, HWND host, DWORD pid) noexcept
        {
            // Reject an already stalled child before attempting synchronous
            // cross-process parenting. This is a bounded health probe, not a
            // guarantee that a foreign window cannot hang later.
            if (!candidate || ::IsHungAppWindow(candidate) != FALSE)
                return false;
            DWORD_PTR probe{};
            if (::SendMessageTimeoutW(candidate, WM_NULL, 0, 0,
                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 150u, &probe) == 0)
                return false;
            hwnd = candidate;
            parent = host;
            processId = pid;
            DWORD observedPid{};
            threadId = ::GetWindowThreadProcessId(hwnd, &observedPid);
            if (threadId == 0 || observedPid != pid || pid == ::GetCurrentProcessId())
                return false;
            process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
            if (!same_window() || ::GetPropW(hwnd, kExternalAttachmentProp))
                return false;
            originalStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
            originalExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            originalOwner = ::GetWindow(hwnd, GW_OWNER);
            // The child-process supervisor enumerates unowned top-level HWNDs.
            // Do not steal a child or dialog from another ownership hierarchy.
            if ((originalStyle & WS_CHILD) != 0 || originalOwner != nullptr
                || !::GetWindowRect(hwnd, &originalRect)
                || !::GetWindowPlacement(hwnd, &originalPlacement))
                return false;
            RECT hostClient{};
            if (!::GetClientRect(parent, &hostClient)
                || hostClient.right <= 0 || hostClient.bottom <= 0 || !same_window())
                return false;
            cookie = next_external_attachment_cookie();
            if (cookie == 0 || !::SetPropW(hwnd, kExternalAttachmentProp, reinterpret_cast<HANDLE>(cookie)))
                return false;
            propertyOwned = true;
            if (!same_window()) return false;
            nativeChanged = true;
            if (!write_long(GWL_STYLE, (originalStyle & ~kExternalFrameStyles) | WS_CHILD)
                || !write_long(GWL_EXSTYLE,
                    (originalExStyle & ~kExternalFrameExStyles) | WS_EX_NOPARENTNOTIFY))
                return false;
            if (!same_window()) return false;
            ::SetLastError(ERROR_SUCCESS);
            const HWND previousParent = ::SetParent(hwnd, parent);
            if ((!previousParent && ::GetLastError() != ERROR_SUCCESS)
                || !attached())
                return false;
            return place(0, 0, hostClient.right, hostClient.bottom);
        }

        void restore() noexcept override
        {
            if (!propertyOwned) return;
            // A lost/replaced property or exited process is not permission to
            // touch a newly reused HWND, including removing another lease.
            if (same_window())
            {
                bool restored = true;
                if (nativeChanged)
                {
                    restored = write_long(GWL_STYLE, originalStyle);
                    restored = write_long(GWL_EXSTYLE, originalExStyle) && restored;
                    if (same_window())
                    {
                        ::SetLastError(ERROR_SUCCESS);
                        const HWND previousParent = ::SetParent(hwnd, nullptr);
                        restored = (previousParent || ::GetLastError() == ERROR_SUCCESS) && restored;
                        restored = write_long(GWLP_HWNDPARENT,
                            reinterpret_cast<LONG_PTR>(originalOwner)) && restored;
                        if (same_window())
                        {
                            restored = ::SetWindowPos(hwnd, nullptr, originalRect.left, originalRect.top,
                                originalRect.right - originalRect.left, originalRect.bottom - originalRect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) && restored;
                            WINDOWPLACEMENT placement = originalPlacement;
                            if (hideDuringRetirement.load(std::memory_order_acquire))
                                placement.showCmd = SW_HIDE;
                            restored = same_window() && ::SetWindowPlacement(hwnd, &placement) && restored;
                            if (hideDuringRetirement.load(std::memory_order_acquire) && same_window())
                                (void)::ShowWindowAsync(hwnd, SW_HIDE);
                            RECT restoredRect{};
                            restored = same_window()
                                && ::GetWindowLongPtrW(hwnd, GWL_STYLE) == originalStyle
                                && ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) == originalExStyle
                                && ::GetWindow(hwnd, GW_OWNER) == originalOwner
                                && ::GetWindowRect(hwnd, &restoredRect)
                                && ::EqualRect(&restoredRect, &originalRect) && restored;
                        }
                    }
                    if (!restored)
                        core::log::warn("Context.Multiplexer.Win",
                            "Foreign context detach could not restore every native window attribute; process ownership remains external.");
                }
                if (same_window())
                    (void)::RemovePropW(hwnd, kExternalAttachmentProp);
            }
            propertyOwned = false;
        }
    };


} // namespace
#endif
    core::error::result<std::shared_ptr<INativeWindowAttachment>>
        attach_native_window(WindowHandle candidate, WindowHandle host,
            std::uint64_t process_id) noexcept
    {
#if defined(_WIN32)
        if (!candidate.valid() || !host.valid() || process_id == 0 || process_id > MAXDWORD)
            return epochengine::unexpected(core::error::invalid_argument("Invalid native attachment identity."));
        const auto parent = reinterpret_cast<HWND>(host.value);
        DWORD hostProcess{};
        if (::GetWindowThreadProcessId(parent, &hostProcess) != ::GetCurrentThreadId()
            || hostProcess != ::GetCurrentProcessId())
            return epochengine::unexpected(core::error::invalid_argument("Native attachment requires the owned host thread."));
        try
        {
            auto attachment = std::make_shared<Win32WindowAttachment>();
            if (!attachment->attach(reinterpret_cast<HWND>(candidate.value), parent,
                    static_cast<DWORD>(process_id)))
                return epochengine::unexpected(core::error::failed("Native attachment admission or placement failed."));
            return std::shared_ptr<INativeWindowAttachment>{std::move(attachment)};
        }
        catch (...)
        {
            return epochengine::unexpected(core::error::failed("Native attachment allocation failed."));
        }
#else
        (void)candidate; (void)host; (void)process_id;
        return epochengine::unexpected(core::error::make(
            {core::error::core_domain::id, core::error::core_domain::unsupported},
            "Native window attachment is unavailable on this platform."));
#endif
    }
}
