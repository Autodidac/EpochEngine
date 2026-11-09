// SPDX-License-Identifier: MPL-2.0
module;
#include "core.stl_types.hpp"
#include <atomic>
#include <charconv>
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

    [[nodiscard]] bool attachment_failure(std::string_view stage, DWORD nativeError,
        HWND child, HWND host, DWORD processId, int x = 0, int y = 0,
        int width = 0, int height = 0, RECT client = {}, RECT outer = {},
        std::string_view attribute = {}, bool reconciled = false) noexcept
    {
        static thread_local unsigned records{};
        if (records >= 32u) return false;
        ++records;
        try
        {
            const std::string message = (reconciled ? "Attachment attribute reconciled stage="
                : "Attachment failure stage=") + std::string{stage}
                + " native_error=" + std::to_string(nativeError)
                + " child=" + std::to_string(reinterpret_cast<std::uintptr_t>(child))
                + " host=" + std::to_string(reinterpret_cast<std::uintptr_t>(host))
                + " pid=" + std::to_string(processId)
                + " slot=" + std::to_string(x) + "," + std::to_string(y)
                + "," + std::to_string(width) + "," + std::to_string(height)
                + " client=" + std::to_string(client.right - client.left)
                + "," + std::to_string(client.bottom - client.top)
                + " outer=" + std::to_string(outer.left) + "," + std::to_string(outer.top)
                + "," + std::to_string(outer.right) + "," + std::to_string(outer.bottom)
                + (attribute.empty() ? std::string{} : " " + std::string{attribute});
            core::log::warn("Context.Multiplexer.Win",
                epochengine::string_view{message.data(), message.size()});
        }
        catch (...) {} // Diagnostic allocation must not change a native refusal.
        return false;
    }

    [[nodiscard]] std::string attribute_hex(LONG_PTR value)
    {
        char bytes[2u + 2u * sizeof(std::uintptr_t)]{'0', 'x'};
        const auto converted = std::to_chars(bytes + 2u, bytes + sizeof(bytes),
            static_cast<std::uintptr_t>(value), 16);
        return std::string{bytes, converted.ptr};
    }

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

        [[nodiscard]] bool write_long(int index, LONG_PTR value,
            std::string_view stage = "restore_attribute") const noexcept
        {
            if (!same_window()) return attachment_failure(stage, ERROR_INVALID_WINDOW_HANDLE,
                hwnd, parent, processId, 0, 0, 0, 0, {}, {}, "failure=identity_before_write");
            ::SetLastError(ERROR_SUCCESS);
            const LONG_PTR before = ::GetWindowLongPtrW(hwnd, index);
            const DWORD beforeError = ::GetLastError();
            const bool beforeRead = before != 0 || beforeError == ERROR_SUCCESS;
            if (!same_window()) return attachment_failure(stage, ERROR_INVALID_WINDOW_HANDLE,
                hwnd, parent, processId, 0, 0, 0, 0, {}, {}, "failure=identity_after_pre_read");
            if (!beforeRead) return attachment_failure(stage, beforeError,
                hwnd, parent, processId, 0, 0, 0, 0, {}, {}, "failure=pre_read");
            // Already-correct owner/style state needs no native mutation. In
            // particular, restoring an absent top-level owner is a no-op.
            if (beforeRead && before == value) return true;
            ::SetLastError(ERROR_SUCCESS);
            const LONG_PTR previous = ::SetWindowLongPtrW(hwnd, index, value);
            // The previous value can legitimately be zero. Capture its error
            // immediately: identity/readback APIs may change the thread error.
            const DWORD setterError = ::GetLastError();
            const bool setterSucceeded = previous != 0 || setterError == ERROR_SUCCESS;
            bool identity = same_window();
            LONG_PTR observed{};
            DWORD readError{};
            if (identity)
            {
                ::SetLastError(ERROR_SUCCESS);
                observed = ::GetWindowLongPtrW(hwnd, index);
                readError = ::GetLastError();
            }
            identity = identity && same_window();
            const bool readSucceeded = identity && (observed != 0 || readError == ERROR_SUCCESS);
            if (setterSucceeded && readSucceeded && observed == value) return true;
            // A native style change can report error87 after applying its bits.
            // Reconcile only that observed changed postcondition; access denial,
            // other errors and unreadable/replaced windows remain refusals.
            const bool reconciled = (index == GWL_STYLE || index == GWL_EXSTYLE)
                && beforeRead && before != value && previous == 0
                && setterError == ERROR_INVALID_PARAMETER && readSucceeded && observed == value;
            const char* failure = !setterSucceeded ? "setter"
                : !identity ? "identity_after_write" : !readSucceeded ? "readback" : "readback_mismatch";
            const DWORD error = !setterSucceeded ? setterError
                : !identity ? ERROR_INVALID_WINDOW_HANDLE : !readSucceeded ? readError : ERROR_INVALID_DATA;
            try
            {
                const std::string detail = "failure=" + std::string{failure}
                    + " attribute_index=" + std::to_string(index)
                    + " before=" + attribute_hex(before)
                    + " before_error=" + std::to_string(beforeError)
                    + " previous=" + attribute_hex(previous)
                    + " requested=" + attribute_hex(value)
                    + " observed=" + attribute_hex(observed)
                    + " setter_error=" + std::to_string(setterError)
                    + " read_error=" + std::to_string(readError)
                    + " identity_after_write=" + std::to_string(identity);
                (void)attachment_failure(stage, error, hwnd, parent, processId,
                    0, 0, 0, 0, {}, {}, detail, reconciled);
                return reconciled;
            }
            catch (...) { return false; }
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
            RECT client{}, outer{};
            const auto refuse = [&](std::string_view stage, DWORD error = 0)
            { return attachment_failure(stage, error, hwnd, parent, processId,
                x, y, width, height, client, outer); };
            if (!attached()) return refuse("placement_identity");
            if (::IsHungAppWindow(hwnd) != FALSE) return refuse("placement_hung_window");
            if (x < 0 || y < 0 || width <= 0 || height <= 0)
                return refuse("placement_invalid_slot");
            RECT parentClient{};
            ::SetLastError(ERROR_SUCCESS);
            if (!::GetClientRect(parent, &parentClient))
                return refuse("placement_parent_client", ::GetLastError());
            if (x > parentClient.right - width || y > parentClient.bottom - height)
                return refuse("placement_outside_parent");
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
            if (!unchanged)
            {
                ::SetLastError(ERROR_SUCCESS);
                if (!::SetWindowPos(hwnd, nullptr, x, y, width, height,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW))
                    return refuse("placement_set_window_pos", ::GetLastError());
            }
            if (!attached()) return refuse("placement_identity_after_move");
            POINT origin{};
            ::SetLastError(ERROR_SUCCESS);
            if (!::GetClientRect(hwnd, &client))
                return refuse("placement_child_client", ::GetLastError());
            if (!::GetWindowRect(hwnd, &outer))
                return refuse("placement_outer_rectangle", ::GetLastError());
            if (!::ClientToScreen(parent, &origin))
                return refuse("placement_parent_origin", ::GetLastError());
            // Child chrome is removed: both the client area and native bounds
            // must agree with the admitted slot, not merely be nonzero.
            slotVerified = client.right - client.left == width && client.bottom - client.top == height
                && outer.left == origin.x + x && outer.top == origin.y + y
                && outer.right - outer.left == width && outer.bottom - outer.top == height
                && attached();
            if (slotVerified) admittedSlot = { x, y, x + width, y + height };
            return slotVerified || refuse("placement_geometry_mismatch");
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
            const auto refuse = [&](std::string_view stage, DWORD error = 0)
            { return attachment_failure(stage, error, candidate, host, pid); };
            // Reject an already stalled child before attempting synchronous
            // cross-process parenting. This is a bounded health probe, not a
            // guarantee that a foreign window cannot hang later.
            if (!candidate) return refuse("admission_missing_window");
            if (::IsHungAppWindow(candidate) != FALSE)
                return refuse("admission_hung_window");
            DWORD_PTR probe{};
            ::SetLastError(ERROR_SUCCESS);
            if (::SendMessageTimeoutW(candidate, WM_NULL, 0, 0,
                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 150u, &probe) == 0)
                return refuse("admission_health_probe", ::GetLastError());
            hwnd = candidate;
            parent = host;
            processId = pid;
            DWORD observedPid{};
            threadId = ::GetWindowThreadProcessId(hwnd, &observedPid);
            if (threadId == 0 || observedPid != pid || pid == ::GetCurrentProcessId())
                return refuse("admission_thread_process_identity");
            process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
            if (!process) return refuse("admission_open_process", ::GetLastError());
            if (!same_window()) return refuse("admission_process_incarnation");
            if (::GetPropW(hwnd, kExternalAttachmentProp))
                return refuse("admission_existing_lease");
            originalStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
            originalExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            originalOwner = ::GetWindow(hwnd, GW_OWNER);
            // The child-process supervisor enumerates unowned top-level HWNDs.
            // Do not steal a child or dialog from another ownership hierarchy.
            if ((originalStyle & WS_CHILD) != 0 || originalOwner != nullptr)
                return refuse("admission_foreign_hierarchy");
            ::SetLastError(ERROR_SUCCESS);
            if (!::GetWindowRect(hwnd, &originalRect))
                return refuse("admission_original_rectangle", ::GetLastError());
            if (!::GetWindowPlacement(hwnd, &originalPlacement))
                return refuse("admission_original_placement", ::GetLastError());
            RECT hostClient{};
            if (!::GetClientRect(parent, &hostClient))
                return refuse("admission_host_client", ::GetLastError());
            if (hostClient.right <= 0 || hostClient.bottom <= 0 || !same_window())
                return refuse("admission_host_geometry_or_identity");
            cookie = next_external_attachment_cookie();
            if (cookie == 0) return refuse("admission_cookie_exhausted");
            ::SetLastError(ERROR_SUCCESS);
            if (!::SetPropW(hwnd, kExternalAttachmentProp, reinterpret_cast<HANDLE>(cookie)))
                return refuse("admission_set_lease_property", ::GetLastError());
            propertyOwned = true;
            if (!same_window()) return refuse("admission_identity_after_property");
            nativeChanged = true;
            // Match engine-created child contexts: neither sibling paints nor
            // descendant paints may overwrite another hosted surface. Restore
            // the exact saved top-level style when this attachment retires.
            if (!write_long(GWL_STYLE, (originalStyle & ~kExternalFrameStyles)
                    | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                    "admission_child_style"))
                return false;
            if (!write_long(GWL_EXSTYLE,
                    (originalExStyle & ~kExternalFrameExStyles) | WS_EX_NOPARENTNOTIFY,
                    "admission_child_extended_style"))
                return false;
            if (!same_window()) return refuse("admission_identity_before_parent");
            ::SetLastError(ERROR_SUCCESS);
            const HWND previousParent = ::SetParent(hwnd, parent);
            if (!previousParent && ::GetLastError() != ERROR_SUCCESS)
                return refuse("admission_set_parent", ::GetLastError());
            if (!attached()) return refuse("admission_identity_after_parent");
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
