namespace
{
    [[nodiscard]] inline epochengine::gui::Vec2 client_mouse_position(
        HWND hwnd,
        LPARAM lParam,
        bool screenCoordinates = false) noexcept
    {
        POINT pt{
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        if (screenCoordinates)
            ::ScreenToClient(hwnd, &pt);

        return {
            static_cast<float>(pt.x),
            static_cast<float>(pt.y)
        };
    }

    struct MouseFocusEvidence
    {
        DWORD callerThread{}, error{}, initialError{}, attachError{};
        bool attached{}, detached{};
        HWND target{}, before{}, previous{}, after{}, parent{}, root{};
        bool targetEnabled{}, parentEnabled{};
    };
    static thread_local MouseFocusEvidence mouseFocusEvidence{};

    inline void focus_gui_mouse_target(HWND hwnd) noexcept
    {
        const DWORD priorError = ::GetLastError();
        MouseFocusEvidence evidence{};
        evidence.target = hwnd;
        evidence.callerThread = ::GetCurrentThreadId();
        evidence.before = ::GetFocus();
        ::SetLastError(0);
        evidence.previous = ::SetFocus(hwnd);
        evidence.error = ::GetLastError();
        evidence.after = ::GetFocus();
        evidence.parent = ::GetParent(hwnd);
        evidence.root = ::GetAncestor(hwnd, GA_ROOT);
        evidence.targetEnabled = ::IsWindowEnabled(hwnd) != FALSE;
        evidence.parentEnabled = !evidence.parent || ::IsWindowEnabled(evidence.parent) != FALSE;
        evidence.initialError = evidence.error;
        if (evidence.error == ERROR_INVALID_PARAMETER && evidence.after != hwnd
            && evidence.targetEnabled && evidence.parentEnabled)
        {
            DWORD ownerPid{}, foregroundPid{};
            const DWORD ownerThread = ::GetWindowThreadProcessId(hwnd, &ownerPid);
            const HWND foreground = ::GetForegroundWindow();
            const DWORD foregroundThread = ::GetWindowThreadProcessId(foreground, &foregroundPid);
            // Retry only a real click's failed acquisition inside our active
            // parent. Never attach unrelated/external queues or steal focus.
            if (ownerThread == evidence.callerThread && foregroundThread != 0
                && foregroundThread != ownerThread
                && ownerPid == ::GetCurrentProcessId() && foregroundPid == ownerPid
                && foreground == evidence.root && ::IsChild(foreground, hwnd) != FALSE)
            {
                ::SetLastError(0);
                evidence.attached = ::AttachThreadInput(ownerThread, foregroundThread, TRUE) != FALSE;
                evidence.attachError = ::GetLastError();
                if (evidence.attached)
                {
                    struct InputQueueAttachment
                    {
                        DWORD owner, foreground;
                        bool& detached;
                        ~InputQueueAttachment() noexcept
                        { detached = ::AttachThreadInput(owner, foreground, FALSE) != FALSE; }
                    };
                    {
                        InputQueueAttachment attachment{ownerThread, foregroundThread, evidence.detached};
                        ::SetLastError(0);
                        evidence.previous = ::SetFocus(hwnd);
                        evidence.error = ::GetLastError();
                    }
                    // Detachment must not turn an apparent acquisition into
                    // silently accepted input; the existing focus gate remains.
                    evidence.after = ::GetFocus();
                }
            }
        }
        if (epochengine::core::cli::trace_menu_button0_rect)
            mouseFocusEvidence = evidence;
        // Keep a native failure's error, or the incoming error when successful
        // SetFocus left it untouched. Observation calls must not leak errors.
        ::SetLastError(evidence.error ? evidence.error : priorError);
    }

    inline void push_gui_mouse_event(
        const epochengine::core::Context* ctx,
        HWND hwnd,
        epochengine::gui::EventType type,
        LPARAM lParam,
        int wheelDelta = 0,
        bool screenCoordinates = false,
        int mouseButton = 0) noexcept
    {
        if (!ctx)
            return;

        // Opt-in admission evidence on real native button delivery, not hover.
        // No coordinates, key values or text payloads are logged.
        static thread_local unsigned focusReceipts = 0;
        if (epochengine::core::cli::trace_menu_button0_rect
            && type == epochengine::gui::EventType::MouseDown && focusReceipts < 24u)
        {
            ++focusReceipts;
            const HWND foreground = ::GetForegroundWindow();
            DWORD foregroundPid{}, ownerPid{};
            const DWORD foregroundThread = ::GetWindowThreadProcessId(foreground, &foregroundPid);
            const DWORD ownerThread = ::GetWindowThreadProcessId(hwnd, &ownerPid);
            GUITHREADINFO foregroundInfo{}, ownerInfo{};
            foregroundInfo.cbSize = sizeof(foregroundInfo);
            ownerInfo.cbSize = sizeof(ownerInfo);
            const bool foregroundKnown = foregroundThread
                && ::GetGUIThreadInfo(foregroundThread, &foregroundInfo) != FALSE;
            const bool ownerKnown = ownerThread
                && ::GetGUIThreadInfo(ownerThread, &ownerInfo) != FALSE;
            try
            {
                epochengine::logger::info("InputAdmission", "backend=" + ctx->backendName
                    + " target=" + std::to_string(reinterpret_cast<std::uintptr_t>(hwnd))
                    + " focus_attempt_target=" + std::to_string(reinterpret_cast<std::uintptr_t>(mouseFocusEvidence.target))
                    + " caller_thread=" + std::to_string(mouseFocusEvidence.callerThread)
                    + " focus_initial_error=" + std::to_string(mouseFocusEvidence.initialError)
                    + " input_attached=" + std::to_string(mouseFocusEvidence.attached)
                    + " input_detached=" + std::to_string(mouseFocusEvidence.detached)
                    + " attach_error=" + std::to_string(mouseFocusEvidence.attachError)
                    + " focus_error=" + std::to_string(mouseFocusEvidence.error)
                    + " focus_before=" + std::to_string(reinterpret_cast<std::uintptr_t>(mouseFocusEvidence.before))
                    + " focus_previous=" + std::to_string(reinterpret_cast<std::uintptr_t>(mouseFocusEvidence.previous))
                    + " focus_after=" + std::to_string(reinterpret_cast<std::uintptr_t>(mouseFocusEvidence.after))
                    + " parent=" + std::to_string(reinterpret_cast<std::uintptr_t>(mouseFocusEvidence.parent))
                    + " root=" + std::to_string(reinterpret_cast<std::uintptr_t>(mouseFocusEvidence.root))
                    + " target_enabled=" + std::to_string(mouseFocusEvidence.targetEnabled)
                    + " parent_enabled=" + std::to_string(mouseFocusEvidence.parentEnabled)
                    + " foreground=" + std::to_string(reinterpret_cast<std::uintptr_t>(foreground))
                    + " foreground_thread=" + std::to_string(foregroundThread)
                    + " owner_thread=" + std::to_string(ownerThread)
                    + " same_pid=" + std::to_string(foregroundPid == ownerPid)
                    + " foreground_known=" + std::to_string(foregroundKnown)
                    + " owner_known=" + std::to_string(ownerKnown)
                    + " foreground_focus=" + std::to_string(reinterpret_cast<std::uintptr_t>(foregroundInfo.hwndFocus))
                    + " owner_focus=" + std::to_string(reinterpret_cast<std::uintptr_t>(ownerInfo.hwndFocus))
                    + " owner_capture=" + std::to_string(reinterpret_cast<std::uintptr_t>(ownerInfo.hwndCapture))
                    + " owns_foreground_focus=" + std::to_string(ctx->owns_input_window_safe(foregroundInfo.hwndFocus))
                    + " owns_owner_focus=" + std::to_string(ctx->owns_input_window_safe(ownerInfo.hwndFocus))
                    + " focus_admitted=" + std::to_string(ctx->has_input_focus_safe())
                    + " pointer_admitted=" + std::to_string(ctx->has_pointer_input_safe()));
            }
            catch (...) { /* Diagnostics cannot change native input delivery. */ }
            mouseFocusEvidence = {};
        }

        auto mousePosition = client_mouse_position(
            hwnd, lParam, screenCoordinates);
        if (ctx->normalize_mouse_position)
        {
            int x = static_cast<int>(mousePosition.x);
            int y = static_cast<int>(mousePosition.y);
            ctx->normalize_mouse_position(x, y);
            mousePosition = {
                static_cast<float>(x),
                static_cast<float>(y)
            };
        }

        epochengine::gui::push_input_for_context(ctx, epochengine::gui::InputEvent{
            .type = type,
            .mouse_pos = mousePosition,
            .mouse_button = mouseButton,
            .wheel_delta = wheelDelta
        });
    }

    inline void push_gui_key_event(const epochengine::core::Context* ctx, int key) noexcept
    {
        if (!ctx)
            return;

        epochengine::gui::push_input_for_context(ctx, epochengine::gui::InputEvent{
            .type = epochengine::gui::EventType::KeyDown,
            .key = key,
            .ctrl_down = (::GetKeyState(VK_CONTROL) & 0x8000) != 0,
            .shift_down = (::GetKeyState(VK_SHIFT) & 0x8000) != 0,
            .alt_down = (::GetKeyState(VK_MENU) & 0x8000) != 0
        });
    }

    [[nodiscard]] inline std::string utf8_from_codepoint(char32_t codepoint) noexcept
    {
        std::string out{};
        if (codepoint <= 0x7Fu)
        {
            out.push_back(static_cast<char>(codepoint));
        }
        else if (codepoint <= 0x7FFu)
        {
            out.push_back(static_cast<char>(0xC0u | ((codepoint >> 6) & 0x1Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        else if (codepoint <= 0xFFFFu)
        {
            out.push_back(static_cast<char>(0xE0u | ((codepoint >> 12) & 0x0Fu)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        else if (codepoint <= 0x10FFFFu)
        {
            out.push_back(static_cast<char>(0xF0u | ((codepoint >> 18) & 0x07u)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
        return out;
    }

    [[nodiscard]] constexpr bool is_gui_text_codepoint(char32_t codepoint) noexcept
    {
        return codepoint >= U' '
            && codepoint != char32_t{0x7f}
            && codepoint <= char32_t{0x10ffff}
            && !(codepoint >= char32_t{0xd800}
                && codepoint <= char32_t{0xdfff});
    }

    static_assert(!is_gui_text_codepoint(U'\b'));
    static_assert(is_gui_text_codepoint(U' '));

    inline void push_gui_text_event(const epochengine::core::Context* ctx, char32_t codepoint) noexcept
    {
        if (!ctx || !is_gui_text_codepoint(codepoint))
            return;

        const std::string utf8 = utf8_from_codepoint(codepoint);
        if (utf8.empty())
            return;

        epochengine::gui::push_input_for_context(ctx, epochengine::gui::InputEvent{
            .type = epochengine::gui::EventType::TextInput,
            .text = utf8
        });
    }

    [[nodiscard]] inline std::shared_ptr<epochengine::core::Context> typed_context(
        const epochengine::core::OpaqueContextHandle& opaque) noexcept
    {
        return opaque
            ? std::reinterpret_pointer_cast<epochengine::core::Context>(opaque)
            : nullptr;
    }

    // TU-owned globals.
    std::unordered_map<HWND, std::thread> g_threads;
    std::mutex g_threadStateMutex;
    epochengine::core::DragState       g_drag;
    epochengine::core::MultiContextManager* g_activeManager = nullptr;
    struct RoutedPanelTabDropState
    {
        std::mutex mutex{};
        std::string route{};
        epochengine::core::RoutedPanelDockTarget target{
            epochengine::core::RoutedPanelDockTarget::none};
        std::uint32_t insertionIndex{0xffffffffu};
    };
    RoutedPanelTabDropState g_routedPanelTabDrop{};
    struct PendingWindowCleanup
    {
        HWND hwnd{};
        std::thread thread{};
        std::unique_ptr<epochengine::core::WindowData> window{};
    };
    std::vector<PendingWindowCleanup> g_pendingCleanups;
    constexpr std::string_view kLogSys = "Context.Multiplexer.Win";
    constexpr UINT_PTR kThemeRefreshTimer = 0xE090u;
    constexpr UINT kThemeRefreshIntervalMs = 1000u;
    constexpr wchar_t kWindowThemeProperty[] = L"Epoch.Chrome.Palette.v2";
    constexpr auto kRenderThreadStartupStepDelay = std::chrono::milliseconds(250);

    std::mutex g_externalAttachmentsMutex;
    std::unordered_map<const epochengine::core::WindowData*,
        std::shared_ptr<epochengine::platform::window_adapter::INativeWindowAttachment>> g_externalAttachments;

    [[nodiscard]] std::shared_ptr<epochengine::platform::window_adapter::INativeWindowAttachment> external_attachment(
        const epochengine::core::WindowData* window)
    {
        std::scoped_lock lock(g_externalAttachmentsMutex);
        const auto found = g_externalAttachments.find(window);
        return found == g_externalAttachments.end() ? nullptr : found->second;
    }

    [[nodiscard]] bool release_external_attachment(const epochengine::core::WindowData* window)
    {
        std::shared_ptr<epochengine::platform::window_adapter::INativeWindowAttachment> attachment;
        {
            std::scoped_lock lock(g_externalAttachmentsMutex);
            const auto found = g_externalAttachments.find(window);
            if (found == g_externalAttachments.end()) return false;
            attachment = std::move(found->second);
            g_externalAttachments.erase(found);
        }
        attachment->restore();
        return true;
    }

    // DWM expects COLORREF (BGR), whereas the GUI palette stores RGB.
    [[nodiscard]] constexpr COLORREF dwm_rgb(std::uint32_t value) noexcept
    {
        return RGB((value >> 16u) & 0xFFu, (value >> 8u) & 0xFFu, value & 0xFFu);
    }

    [[nodiscard]] inline HBRUSH parent_background_brush() noexcept
    {
        // Eight immutable process-lifetime brushes; no allocation while painting.
        static const auto brushes = []
        {
            std::array<HBRUSH, 8u> result{};
            constexpr std::array<COLORREF, 8u> fallback{
                RGB(0x16,0x17,0x19), RGB(0xF0,0xF1,0xF3),
                RGB(0x24,0x25,0x27), RGB(0x10,0x17,0x20),
                RGB(0x20,0x1A,0x17), RGB(0x11,0x1B,0x17),
                RGB(0x17,0x1C,0x21), RGB(0x0E,0x0F,0x11)};
            for (std::size_t i = 0; i < result.size(); ++i)
                result[i] = ::CreateSolidBrush(fallback[i]);
            return result;
        }();
        const auto theme = epochengine::gui::native_window_chrome_palette();
        const auto index = static_cast<std::size_t>(theme.paletteId);
        return index < brushes.size() && brushes[index]
            ? brushes[index] : brushes[0];
    }

    inline void apply_native_window_chrome(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        const auto palette = epochengine::gui::native_window_chrome_palette();
        // Unique values 1..8 also invalidate when switching between dark themes.
        const HANDLE state = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(palette.paletteId) + 1u);
        if (::GetPropW(hwnd, kWindowThemeProperty) == state)
            return;

        const BOOL useDark = palette.isLight ? FALSE : TRUE;
        const HRESULT modernResult = ::DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
        if (FAILED(modernResult))
            (void)::DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));

        // Windows 11: border=34, caption=35, text=36. Unsupported versions
        // harmlessly ignore the calls; no renderer or HWND ownership changes.
        constexpr DWORD borderAttribute = 34u;
        constexpr DWORD captionAttribute = 35u;
        constexpr DWORD textAttribute = 36u;
        const COLORREF border = dwm_rgb(palette.borderRgb);
        const COLORREF caption = dwm_rgb(palette.captionRgb);
        const COLORREF text = dwm_rgb(palette.textRgb);
        (void)::DwmSetWindowAttribute(hwnd, borderAttribute, &border, sizeof(border));
        (void)::DwmSetWindowAttribute(hwnd, captionAttribute, &caption, sizeof(caption));
        (void)::DwmSetWindowAttribute(hwnd, textAttribute, &text, sizeof(text));
        (void)::SetPropW(hwnd, kWindowThemeProperty, state);

        // Invalidate just the non-client frame on a theme switch, never force
        // synchronous whole-window redraws from the host thread.
        (void)::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    inline void apply_native_drag_opacity(
        epochengine::core::DragState& drag,
        HWND hwnd) noexcept
    {
        drag.opacityWindow = nullptr;
        drag.opacityApplied = false;
        drag.opacityWasLayered = false;
        if (!hwnd || ::IsWindow(hwnd) == FALSE || ::GetParent(hwnd) != nullptr)
            return;

        const LONG_PTR extendedStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        drag.opacityWasLayered = (extendedStyle & WS_EX_LAYERED) != 0;
        if (drag.opacityWasLayered)
            return;

        ::SetLastError(ERROR_SUCCESS);
        if (::SetWindowLongPtrW(
                hwnd,
                GWL_EXSTYLE,
                extendedStyle | WS_EX_LAYERED) == 0
            && ::GetLastError() != ERROR_SUCCESS)
        {
            return;
        }
        if (::SetLayeredWindowAttributes(hwnd, 0, 128, LWA_ALPHA) == FALSE)
        {
            ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, extendedStyle);
            return;
        }

        drag.opacityWindow = hwnd;
        drag.opacityApplied = true;
    }

    inline void restore_native_drag_opacity(
        epochengine::core::DragState& drag) noexcept
    {
        const HWND hwnd = drag.opacityWindow;
        if (drag.opacityApplied && hwnd && ::IsWindow(hwnd) != FALSE)
        {
            static_cast<void>(::SetLayeredWindowAttributes(
                hwnd, 0, 255, LWA_ALPHA));
            if (!drag.opacityWasLayered)
            {
                const LONG_PTR extendedStyle =
                    ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                ::SetWindowLongPtrW(
                    hwnd,
                    GWL_EXSTYLE,
                    extendedStyle & ~WS_EX_LAYERED);
            }
            ::RedrawWindow(
                hwnd,
                nullptr,
                nullptr,
                RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
        }
        drag.opacityWindow = nullptr;
        drag.opacityApplied = false;
        drag.opacityWasLayered = false;
    }

    inline void clear_routed_panel_tab_drop() noexcept
    {
        std::scoped_lock lock(g_routedPanelTabDrop.mutex);
        g_routedPanelTabDrop.route.clear();
        g_routedPanelTabDrop.target =
            epochengine::core::RoutedPanelDockTarget::none;
        g_routedPanelTabDrop.insertionIndex = 0xffffffffu;
    }

    inline void cancel_native_context_drag(HWND hwnd) noexcept
    {
        auto& drag = epochengine::core::Drag();
        if (!drag.dragging || drag.draggedWindow != hwnd) return;
        const HWND parent = drag.originalParent;
        restore_native_drag_opacity(drag);
        // Clear before releasing capture: WM_CAPTURECHANGED can re-enter.
        drag = {};
        clear_routed_panel_tab_drop();
        if (::GetCapture() == hwnd) ::ReleaseCapture();
        if (parent && ::IsWindow(parent) != FALSE) ::InvalidateRect(parent, nullptr, FALSE);
    }

    [[nodiscard]] inline bool routed_panel_tab_drop_for(
        std::string_view route,
        epochengine::core::RoutedPanelDockTarget& target,
        std::uint32_t& insertionIndex) noexcept
    {
        std::scoped_lock lock(g_routedPanelTabDrop.mutex);
        if (route.empty()
            || route != g_routedPanelTabDrop.route
            || g_routedPanelTabDrop.target
                == epochengine::core::RoutedPanelDockTarget::none)
        {
            return false;
        }
        target = g_routedPanelTabDrop.target;
        insertionIndex = g_routedPanelTabDrop.insertionIndex;
        return true;
    }

    [[nodiscard]] inline bool should_draw_opengl_startup_placeholder(
        const epochengine::core::WindowData* window) noexcept
    {
        return window
            && window->type == epochengine::core::ContextType::OpenGL
            && !window->firstPresentComplete.load(std::memory_order_acquire);
    }
    // Some Windows SDK setups don't expose WGL_ARB_create_context declarations here.
    // Provide local fallbacks so this TU can request modern core contexts without extra headers.
#if !defined(WGL_CONTEXT_MAJOR_VERSION_ARB)
    constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
#endif
#if !defined(WGL_CONTEXT_MINOR_VERSION_ARB)
    constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
#endif
#if !defined(WGL_CONTEXT_PROFILE_MASK_ARB)
    constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
#endif
#if !defined(WGL_CONTEXT_CORE_PROFILE_BIT_ARB)
    constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
#endif
#if !defined(PFNWGLCREATECONTEXTATTRIBSARBPROC)
    using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
#endif

   // [[nodiscard]] inline int clamp_positive(int v) noexcept { return (v < 1) ? 1 : v; }
    [[nodiscard]] inline int clamp_positive(int v) noexcept { return (v < 1) ? 1 : v; }

    [[nodiscard]] inline bool native_class_owns_gui_input(HWND hwnd) noexcept
    {
        return ::GetClassLongPtrW(hwnd, GCLP_WNDPROC)
            == reinterpret_cast<LONG_PTR>(epochengine::core::MultiContextManager::ChildProc);
    }
#if EPOCH_SINGLE_PARENT
    struct SubCtx { HWND originalParent{}; };
    constexpr wchar_t kEpochDockParentProp[] = L"EpochDockParent";
    constexpr int kDockDragStripHeight = 28;
    static thread_local HWND g_guiInputOwner = nullptr;

    [[nodiscard]] inline bool is_dock_drag_hotspot(HWND hwnd, LPARAM lp) noexcept
    {
        if (!hwnd)
            return false;

        RECT clientRect{};
        if (!::GetClientRect(hwnd, &clientRect))
            return false;

        const int width = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
        const int height = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));
        const int hotspotHeight = (std::min)(kDockDragStripHeight, height);

        const int x = GET_X_LPARAM(lp);
        const int y = GET_Y_LPARAM(lp);
        // The backend child can host real GUI controls in this same top strip.
        // Never steal ordinary left-clicks from those controls. Native context
        // dragging is an explicit Alt+drag gesture instead.
        const bool dragModifier = (::GetKeyState(VK_MENU) & 0x8000) != 0;
        return dragModifier
            && x >= 0 && x < width
            && y >= 0 && y < hotspotHeight;
    }

    [[nodiscard]] inline RECT screen_client_rect(HWND hwnd) noexcept
    {
        RECT clientRect{};
        if (!hwnd || !::GetClientRect(hwnd, &clientRect))
            return clientRect;

        POINT topLeft{ 0, 0 };
        ::ClientToScreen(hwnd, &topLeft);
        ::OffsetRect(&clientRect, topLeft.x, topLeft.y);
        return clientRect;
    }

    [[nodiscard]] inline bool point_in_rect(const RECT& rect, const POINT& pt) noexcept
    {
        return pt.x >= rect.left
            && pt.x < rect.right
            && pt.y >= rect.top
            && pt.y < rect.bottom;
    }

    [[nodiscard]] inline bool rect_within_rect(const RECT& outer, const RECT& inner) noexcept
    {
        return inner.left >= outer.left
            && inner.top >= outer.top
            && inner.right <= outer.right
            && inner.bottom <= outer.bottom;
    }

    [[nodiscard]] inline bool rects_intersect(const RECT& a, const RECT& b) noexcept
    {
        return a.left < b.right
            && a.right > b.left
            && a.top < b.bottom
            && a.bottom > b.top;
    }

    [[nodiscard]] inline bool should_redock_to_parent(
        HWND parent,
        const POINT& screenPoint,
        const RECT& windowRect) noexcept
    {
        if (!parent || ::IsWindow(parent) == FALSE)
            return false;

        const RECT parentClientRect = screen_client_rect(parent);
        RECT parentWindowRect{};
        ::GetWindowRect(parent, &parentWindowRect);
        if (point_in_rect(parentClientRect, screenPoint)
            || point_in_rect(parentWindowRect, screenPoint))
            return true;

        const POINT windowCenter{
            windowRect.left + ((windowRect.right - windowRect.left) / 2),
            windowRect.top + ((windowRect.bottom - windowRect.top) / 2)
        };
        return point_in_rect(parentClientRect, windowCenter)
            || point_in_rect(parentWindowRect, windowCenter);
    }

    [[nodiscard]] inline bool pointer_inside_parent_client(HWND parent, const POINT& screenPoint) noexcept
    {
        if (!parent || ::IsWindow(parent) == FALSE)
            return false;

        const RECT parentClientRect = screen_client_rect(parent);
        RECT parentWindowRect{};
        ::GetWindowRect(parent, &parentWindowRect);
        return point_in_rect(parentClientRect, screenPoint)
            || point_in_rect(parentWindowRect, screenPoint);
    }

    [[nodiscard]] inline bool pointer_inside_parent_dock_region(HWND parent, const POINT& screenPoint) noexcept
    {
        if (!parent || ::IsWindow(parent) == FALSE)
            return false;

        return point_in_rect(screen_client_rect(parent), screenPoint);
    }

    [[nodiscard]] inline POINT screen_drag_point(HWND hwnd, LPARAM lParam) noexcept
    {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (hwnd && ::IsWindow(hwnd) != FALSE && ::ClientToScreen(hwnd, &pt) != FALSE)
            return pt;

        if (::GetCursorPos(&pt) != FALSE)
            return pt;

        return pt;
    }

    [[nodiscard]] inline bool is_nonclient_mouse_message(UINT msg) noexcept
    {
        return msg == WM_NCMOUSEMOVE
            || msg == WM_NCLBUTTONUP
            || msg == WM_NCLBUTTONDOWN;
    }

    [[nodiscard]] inline POINT screen_mouse_point(HWND hwnd, UINT msg, LPARAM lParam) noexcept
    {
        if (is_nonclient_mouse_message(msg))
            return POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        return screen_drag_point(hwnd, lParam);
    }

    [[nodiscard]] inline HWND stored_dock_parent(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return nullptr;

        return static_cast<HWND>(::GetPropW(hwnd, kEpochDockParentProp));
    }

    [[nodiscard]] inline bool drag_distance_exceeded(
        const POINT& start,
        const POINT& current,
        int thresholdPx = 10) noexcept
    {
        const long dx = static_cast<long>(current.x) - static_cast<long>(start.x);
        const long dy = static_cast<long>(current.y) - static_cast<long>(start.y);
        const long threshold = static_cast<long>(thresholdPx);
        return (dx * dx) + (dy * dy) >= (threshold * threshold);
    }

    [[nodiscard]] inline POINT force_proxy_shell_outside_parent(
        HWND parent,
        const POINT& cursorScreen,
        int proposedClientLeft,
        int proposedClientTop,
        int clientW,
        int clientH) noexcept
    {
        POINT adjusted{
            proposedClientLeft,
            proposedClientTop
        };

        if (!parent || ::IsWindow(parent) == FALSE)
            return adjusted;

        constexpr int kEscapeMargin = 18;
        RECT parentWindowRect{};
        ::GetWindowRect(parent, &parentWindowRect);

        RECT adjustedShellRect{ 0, 0, clientW, clientH };
        constexpr DWORD kDetachedShellStyle =
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        constexpr DWORD kDetachedShellExStyle = WS_EX_APPWINDOW;
        if (::AdjustWindowRectEx(
            &adjustedShellRect,
            kDetachedShellStyle,
            FALSE,
            kDetachedShellExStyle) == FALSE)
        {
            adjustedShellRect.left = 0;
            adjustedShellRect.top = 0;
            adjustedShellRect.right = clientW;
            adjustedShellRect.bottom = clientH;
        }

        auto compute_shell_rect = [&](int clientLeft, int clientTop) noexcept
        {
            const int hostLeft = clientLeft + adjustedShellRect.left;
            const int hostTop = clientTop + adjustedShellRect.top;
            return RECT{
                hostLeft,
                hostTop,
                hostLeft + clamp_positive(adjustedShellRect.right - adjustedShellRect.left),
                hostTop + clamp_positive(adjustedShellRect.bottom - adjustedShellRect.top)
            };
        };

        auto escaped_to_right = [&]() noexcept
        {
            adjusted.x = parentWindowRect.right + kEscapeMargin - adjustedShellRect.left;
        };

        auto escaped_to_left = [&]() noexcept
        {
            adjusted.x = parentWindowRect.left - kEscapeMargin - adjustedShellRect.right;
        };

        auto escaped_to_bottom = [&]() noexcept
        {
            adjusted.y = parentWindowRect.bottom + kEscapeMargin - adjustedShellRect.top;
        };

        auto escaped_to_top = [&]() noexcept
        {
            adjusted.y = parentWindowRect.top - kEscapeMargin - adjustedShellRect.bottom;
        };

        const RECT proposedShellRect = compute_shell_rect(proposedClientLeft, proposedClientTop);
        const int overflowLeft = (std::max)(0, static_cast<int>(parentWindowRect.left - proposedShellRect.left));
        const int overflowRight = (std::max)(0, static_cast<int>(proposedShellRect.right - parentWindowRect.right));
        const int overflowTop = (std::max)(0, static_cast<int>(parentWindowRect.top - proposedShellRect.top));
        const int overflowBottom = (std::max)(0, static_cast<int>(proposedShellRect.bottom - parentWindowRect.bottom));

        if (overflowRight > 0 || overflowLeft > 0 || overflowBottom > 0 || overflowTop > 0)
        {
            const int maxOverflow = (std::max)((std::max)(overflowLeft, overflowRight), (std::max)(overflowTop, overflowBottom));
            if (maxOverflow == overflowRight)
                escaped_to_right();
            else if (maxOverflow == overflowLeft)
                escaped_to_left();
            else if (maxOverflow == overflowBottom)
                escaped_to_bottom();
            else
                escaped_to_top();
        }
        else if (rect_within_rect(parentWindowRect, proposedShellRect)
            || rects_intersect(parentWindowRect, proposedShellRect))
        {
            const int distLeft = std::abs(proposedShellRect.left - parentWindowRect.left);
            const int distRight = std::abs(parentWindowRect.right - proposedShellRect.right);
            const int distTop = std::abs(proposedShellRect.top - parentWindowRect.top);
            const int distBottom = std::abs(parentWindowRect.bottom - proposedShellRect.bottom);
            const int minDistance = (std::min)((std::min)(distLeft, distRight), (std::min)(distTop, distBottom));
            if (minDistance == distRight)
                escaped_to_right();
            else if (minDistance == distLeft)
                escaped_to_left();
            else if (minDistance == distBottom)
                escaped_to_bottom();
            else
                escaped_to_top();
        }
        else if (cursorScreen.x >= parentWindowRect.right)
            escaped_to_right();
        else if (cursorScreen.x < parentWindowRect.left)
            escaped_to_left();
        else if (cursorScreen.y >= parentWindowRect.bottom)
            escaped_to_bottom();
        else if (cursorScreen.y < parentWindowRect.top)
            escaped_to_top();

        return adjusted;
    }

    inline void dock_host_window_to_parent(
        HWND hwnd,
        HWND parent,
        int desiredScreenX,
        int desiredScreenY,
        int clientW,
        int clientH,
        bool asynchronous = true) noexcept
    {
        if (!hwnd || !parent || ::IsWindow(parent) == FALSE)
            return;

        const HWND currentParent = ::GetParent(hwnd);
        LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
        const bool needsChildStyle = (style & WS_CHILD) == 0 || (style & (WS_POPUP | WS_OVERLAPPEDWINDOW)) != 0;
        if (needsChildStyle)
        {
            style &= ~(WS_POPUP | WS_OVERLAPPEDWINDOW);
            style |= WS_CHILD;
            ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        }

        LONG_PTR exStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        LONG_PTR desiredExStyle = exStyle;
        desiredExStyle &= ~static_cast<LONG_PTR>(
            WS_EX_APPWINDOW
            | WS_EX_TOOLWINDOW
            | WS_EX_TOPMOST
            | WS_EX_WINDOWEDGE
            | WS_EX_CLIENTEDGE
            | WS_EX_DLGMODALFRAME);
        desiredExStyle |= WS_EX_NOPARENTNOTIFY;
        if (desiredExStyle != exStyle)
            ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, desiredExStyle);

        if (currentParent != parent)
            ::SetParent(hwnd, parent);
        ::SetPropW(hwnd, kEpochDockParentProp, parent);

        RECT parentClient{};
        ::GetClientRect(parent, &parentClient);
        POINT clientPos{ desiredScreenX, desiredScreenY };
        ::ScreenToClient(parent, &clientPos);

        const int maxX = (std::max)(0, static_cast<int>(parentClient.right - clientW));
        const int maxY = (std::max)(0, static_cast<int>(parentClient.bottom - clientH));
        clientPos.x = (std::clamp)(static_cast<int>(clientPos.x), 0, maxX);
        clientPos.y = (std::clamp)(static_cast<int>(clientPos.y), 0, maxY);

        UINT flags = SWP_NOZORDER
            | SWP_NOACTIVATE
            | (needsChildStyle || currentParent != parent ? SWP_FRAMECHANGED : 0)
            | SWP_SHOWWINDOW;
        if (asynchronous)
            flags |= SWP_ASYNCWINDOWPOS;

        ::SetWindowPos(
            hwnd,
            nullptr,
            clientPos.x,
            clientPos.y,
            clientW,
            clientH,
            flags);
    }

    [[nodiscard]] inline bool make_parent_dock_guide_layout(
        HWND parent,
        POINT screenPoint,
        bool routedPanel,
        epochengine::gui::DockGuideLayout& layout) noexcept
    {
        if (!parent || ::IsWindow(parent) == FALSE)
            return false;

        RECT client{};
        if (::GetClientRect(parent, &client) == FALSE)
            return false;

        POINT local = screenPoint;
        if (::ScreenToClient(parent, &local) == FALSE)
            return false;

        const int width = (std::max)(1L, client.right - client.left);
        const int height = (std::max)(1L, client.bottom - client.top);
        if (local.x < 0 || local.y < 0
            || local.x >= width || local.y >= height)
        {
            return false;
        }

        const float halfWidth = static_cast<float>(width) * 0.5f;
        layout = epochengine::gui::make_dock_guide_layout(
            epochengine::gui::DockGuideOptions{
                .guide_bounds = {
                    { 0.0f, 0.0f },
                    { static_cast<float>(width), static_cast<float>(height) } },
                .left_tabs_preview = {},
                .right_tabs_preview = {},
                .bottom_left_tabs_preview = {},
                .bottom_right_tabs_preview = {},
                .left_context_preview = routedPanel
                    ? epochengine::gui::WidgetBounds{}
                    : epochengine::gui::WidgetBounds{
                        { 0.0f, 0.0f },
                        { halfWidth, static_cast<float>(height) } },
                .right_context_preview = routedPanel
                    ? epochengine::gui::WidgetBounds{}
                    : epochengine::gui::WidgetBounds{
                        { halfWidth, 0.0f },
                        { static_cast<float>(width) - halfWidth,
                          static_cast<float>(height) } },
                .floating_preview = {},
                .pointer = {
                    static_cast<float>(local.x),
                    static_cast<float>(local.y) },
                .guide_extent = 94.0f,
                .guide_gap = 8.0f,
                .allow_side_tabs = routedPanel,
                .allow_bottom_tabs = routedPanel,
                .allow_contexts = true,
                .allow_float = false,
                .center_context_guides_in_previews = !routedPanel
            });
        return true;
    }

    [[nodiscard]] inline epochengine::core::RoutedPanelDockTarget
        routed_panel_target_for_parent(
            HWND parent,
            POINT screenPoint,
            bool routedPanel) noexcept
    {
        epochengine::gui::DockGuideLayout guideLayout{};
        if (!make_parent_dock_guide_layout(
                parent, screenPoint, routedPanel, guideLayout))
        {
            return epochengine::core::RoutedPanelDockTarget::none;
        }

        switch (guideLayout.hovered_target)
        {
        case epochengine::gui::DockGuideTarget::left_tabs:
            return epochengine::core::RoutedPanelDockTarget::left_tabs;
        case epochengine::gui::DockGuideTarget::right_tabs:
            return epochengine::core::RoutedPanelDockTarget::right_tabs;
        case epochengine::gui::DockGuideTarget::bottom_left_tabs:
            return epochengine::core::RoutedPanelDockTarget::bottom_left_tabs;
        case epochengine::gui::DockGuideTarget::bottom_right_tabs:
            return epochengine::core::RoutedPanelDockTarget::bottom_right_tabs;
        case epochengine::gui::DockGuideTarget::left_context:
            return epochengine::core::RoutedPanelDockTarget::left_context;
        case epochengine::gui::DockGuideTarget::right_context:
            return epochengine::core::RoutedPanelDockTarget::right_context;
        case epochengine::gui::DockGuideTarget::float_window:
        case epochengine::gui::DockGuideTarget::none:
        default:
            return epochengine::core::RoutedPanelDockTarget::none;
        }
    }
    inline void request_routed_panel_redock_close(epochengine::core::WindowData* window) noexcept
    {
        if (!window || window->guiRoute.empty())
            return;

        window->routedRedockRequested.store(true);
    }

    [[nodiscard]] inline bool child_window_matches_parent_slot(
        HWND hwnd,
        HWND parent,
        int x,
        int y,
        int width,
        int height) noexcept
    {
        if (!hwnd
            || !parent
            || ::IsWindow(hwnd) == FALSE
            || ::IsWindow(parent) == FALSE
            || ::GetParent(hwnd) != parent)
        {
            return false;
        }

        RECT rect{};
        if (::GetWindowRect(hwnd, &rect) == FALSE)
            return false;

        POINT points[2]{
            { rect.left, rect.top },
            { rect.right, rect.bottom }
        };
        ::SetLastError(ERROR_SUCCESS);
        if (::MapWindowPoints(HWND_DESKTOP, parent, points, 2) == 0
            && (::GetLastError() != ERROR_SUCCESS))
        {
            return false;
        }

        constexpr int kDockSlotTolerance = 2;
        const int actualX = points[0].x;
        const int actualY = points[0].y;
        const int actualW = points[1].x - points[0].x;
        const int actualH = points[1].y - points[0].y;

        return std::abs(actualX - x) <= kDockSlotTolerance
            && std::abs(actualY - y) <= kDockSlotTolerance
            && std::abs(actualW - width) <= kDockSlotTolerance
            && std::abs(actualH - height) <= kDockSlotTolerance;
    }

    // Dock/undock requests must be processed on the window's owning thread.
    // GLFW/raylib windows are owned by the thread that created them (typically the render thread).
    // Cross-thread SetParent/SetWindowLongPtr/SetWindowPos can deadlock.
    constexpr UINT WM_EPOCH_DOCKCMD = WM_APP + 0x4A11;
    constexpr UINT WM_EPOCH_LAYOUT = WM_APP + 0x4A12;
    constexpr UINT WM_EPOCH_PROXY_DOCKCMD = WM_APP + 0x4A13;
    constexpr UINT WM_EPOCH_OPEN_DETACHED_CONTEXT = WM_APP + 0x4A14;
    constexpr UINT WM_EPOCH_RETIRE_CONTEXT_WINDOW = WM_APP + 0x4A15;
    constexpr wchar_t kEpochLayoutPendingProp[] = L"EpochLayoutPending";
    enum class DockCmd : WPARAM
    {
        Undock = 1,
    };
    enum class ProxyDockCmd : WPARAM
    {
        Undock = 1,
        MoveDetached = 2,
        Redock = 3,
    };
    struct ProxyDockRequest
    {
        HWND sourceHwnd{};
        HWND parentHwnd{};
        int x{};
        int y{};
        int width{};
        int height{};
    };

    inline void request_parent_layout(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        if (::GetPropW(hwnd, kEpochLayoutPendingProp))
            return;

        ::SetPropW(hwnd, kEpochLayoutPendingProp, reinterpret_cast<HANDLE>(1));
        ::PostMessageW(hwnd, WM_EPOCH_LAYOUT, 0, 0);
    }

    [[nodiscard]] inline bool backend_window_exposable(
        const epochengine::core::WindowData* window) noexcept
    {
        if (const auto attachment = external_attachment(window))
            return window->backend_ready() && attachment->attached();
        return window
            && window->backend_ready()
            && window->successfulFrameGeneration.load(std::memory_order_acquire) > 0u;
    }

    [[nodiscard]] inline std::shared_ptr<epochengine::core::Context> resolve_gui_context_for_hwnd(HWND hwnd) noexcept
    {
        auto* mgr = g_activeManager;
        if (!mgr)
            return {};

        if (auto* win = mgr->findWindowByHWND(hwnd);
            backend_window_exposable(win))
        {
            return typed_context(win->context);
        }

        return {};
    }

    [[nodiscard]] inline epochengine::core::WindowData* resolve_window_data_for_hwnd(HWND hwnd) noexcept
    {
        auto* mgr = g_activeManager;
        if (!mgr)
            return nullptr;

        return mgr->findWindowByHWND(hwnd);
    }

    [[nodiscard]] inline RECT native_rect(
        const epochengine::gui::WidgetBounds& bounds) noexcept
    {
        return RECT{
            static_cast<LONG>(bounds.position.x),
            static_cast<LONG>(bounds.position.y),
            static_cast<LONG>(bounds.position.x + bounds.size.x),
            static_cast<LONG>(bounds.position.y + bounds.size.y)
        };
    }

    inline void paint_parent_context_dock_overlay(HWND parent, HDC hdc) noexcept
    {
        const auto& drag = g_drag;
        if (!hdc || !drag.dragging || !drag.draggedWindow)
            return;

        const auto* window = resolve_window_data_for_hwnd(drag.draggedWindow);
        if (!window)
            return;

        HWND targetParent = drag.originalParent;
        if (!targetParent || ::IsWindow(targetParent) == FALSE)
            targetParent = stored_dock_parent(drag.draggedWindow);
        if (targetParent != parent)
            return;

        epochengine::gui::DockGuideLayout layout{};
        const bool routedPanel = !window->guiRoute.empty();
        if (!make_parent_dock_guide_layout(
                parent, drag.lastMousePos, routedPanel, layout))
        {
            return;
        }

        const int savedDc = ::SaveDC(hdc);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, RGB(0xFF, 0xF3, 0xDF));

        static HBRUSH previewBrush = ::CreateHatchBrush(
            HS_DIAGCROSS, RGB(0xF2, 0x98, 0x32));
        static HBRUSH previewFrameBrush = ::CreateSolidBrush(
            RGB(0xF2, 0x98, 0x32));
        static HBRUSH guideBrush = ::CreateSolidBrush(
            RGB(0x9A, 0x58, 0x24));
        static HBRUSH hoveredGuideBrush = ::CreateSolidBrush(
            RGB(0xE2, 0x83, 0x2C));

        if (layout.hovered_preview.size.x > 1.0f
            && layout.hovered_preview.size.y > 1.0f)
        {
            RECT preview = native_rect(layout.hovered_preview);
            ::FillRect(hdc, &preview, previewBrush);
            ::FrameRect(hdc, &preview, previewFrameBrush);
        }

        for (std::uint32_t i = 0; i < layout.count; ++i)
        {
            const auto& guide = layout.guides[i];
            RECT rect = native_rect(guide.target_bounds);
            ::FillRect(hdc, &rect, guide.hovered ? hoveredGuideBrush : guideBrush);
            ::FrameRect(hdc, &rect, previewFrameBrush);

            const wchar_t* label =
                guide.target == epochengine::gui::DockGuideTarget::left_tabs
                    ? L"Left Tabs"
                : guide.target == epochengine::gui::DockGuideTarget::right_tabs
                    ? L"Right Tabs"
                : guide.target == epochengine::gui::DockGuideTarget::bottom_left_tabs
                    ? L"Bottom Left"
                : guide.target == epochengine::gui::DockGuideTarget::bottom_right_tabs
                    ? L"Bottom Right"
                : guide.target == epochengine::gui::DockGuideTarget::left_context
                    ? L"Left Context"
                : guide.target == epochengine::gui::DockGuideTarget::right_context
                    ? L"Right Context"
                    : L"Dock";
            ::DrawTextW(
                hdc,
                label,
                -1,
                &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        ::RestoreDC(hdc, savedDc);
    }

    inline void remember_gui_input_owner(HWND hwnd) noexcept
    {
        if (hwnd && ::IsWindow(hwnd) != FALSE)
            g_guiInputOwner = hwnd;
    }

    inline void forget_gui_input_owner(HWND hwnd, HWND successor = nullptr) noexcept
    {
        if (g_guiInputOwner == hwnd)
            g_guiInputOwner = nullptr;
        const auto ctx = resolve_gui_context_for_hwnd(hwnd);
        const auto next = resolve_gui_context_for_hwnd(successor);
        if (ctx && ctx != next)
            epochengine::gui::push_input_for_context(ctx.get(), {
                .type = epochengine::gui::EventType::FocusLost });
    }

    [[nodiscard]] inline bool same_gui_input_surface(HWND lhs, HWND rhs) noexcept
    {
        if (!lhs || !rhs)
            return false;

        if (lhs == rhs)
            return true;

        auto* mgr = g_activeManager;
        if (!mgr)
            return false;

        const auto* owner = mgr->findWindowByHWND(lhs);
        return owner && owner == mgr->findWindowByHWND(rhs);
    }

    [[nodiscard]] inline bool accepts_gui_keyboard_input(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return false;

        const auto ctx = resolve_gui_context_for_hwnd(hwnd);
        if (!ctx || !ctx->has_input_focus_safe())
            return false;

        const HWND foreground = ::GetForegroundWindow();
        DWORD foregroundPid{};
        const DWORD foregroundThread = ::GetWindowThreadProcessId(foreground, &foregroundPid);
        const bool externallyParented = foregroundPid != ::GetCurrentProcessId();
        DWORD windowPid{};
        const bool ownedChildQueue = externallyParented
            || (::GetAncestor(hwnd, GA_ROOT) == foreground && ::IsChild(foreground, hwnd) != FALSE);
        const DWORD inputThread = ownedChildQueue
            ? ::GetWindowThreadProcessId(hwnd, &windowPid) : foregroundThread;
        if (!inputThread || (ownedChildQueue &&
                (windowPid != ::GetCurrentProcessId()
                    || ::IsChild(foreground, hwnd) == FALSE)))
            return false;
        GUITHREADINFO info{};
        info.cbSize = sizeof(info);
        if (!::GetGUIThreadInfo(inputThread, &info))
            return false;
        // A separately launched candidate is parented into the host process.
        // Query its owning GUI thread, not the foreign foreground host thread.
        // Require actual focus on an Epoch-owned child: hover alone is never
        // permission to inject key/text events.
        if (ownedChildQueue)
            return ctx->owns_input_window_safe(info.hwndFocus);
        const HWND focused = info.hwndFocus ? info.hwndFocus : info.hwndActive;
        return same_gui_input_surface(focused, hwnd);
    }

    [[nodiscard]] inline bool has_proxy_shell_pair(
        const epochengine::core::WindowData* window) noexcept
    {
        return window
            && (window->type == epochengine::core::ContextType::SDL
                || window->type == epochengine::core::ContextType::SFML)
            && window->hwndChild
            && window->host_hwnd
            && window->hwndChild != window->host_hwnd
            && ::IsWindow(window->hwndChild) != FALSE
            && ::IsWindow(window->host_hwnd) != FALSE;
    }

    [[nodiscard]] inline bool has_proxy_host(
        const epochengine::core::WindowData* window) noexcept
    {
        return has_proxy_shell_pair(window)
            && (::GetParent(window->hwndChild) == window->host_hwnd
                || ::GetParent(window->host_hwnd) == nullptr);
    }

    [[nodiscard]] inline bool uses_visible_proxy_host(
        const epochengine::core::WindowData* window) noexcept
    {
        return has_proxy_host(window)
            && (::GetParent(window->hwndChild) == window->host_hwnd
                || ::GetParent(window->host_hwnd) == nullptr);
    }

    inline void apply_child_fill_layout(HWND child, HWND parent, int clientW, int clientH) noexcept;

    inline void hide_associated_host_window(
        const epochengine::core::WindowData* window,
        HWND activeHwnd,
        HWND expectedParent) noexcept
    {
        if (!window || !window->host_hwnd || window->host_hwnd == activeHwnd)
            return;

        if (has_proxy_host(window))
        {
            if (expectedParent
                && ::GetParent(window->host_hwnd) == expectedParent)
            {
                ::ShowWindow(window->host_hwnd, SW_HIDE);
            }
            return;
        }

        if (::IsWindow(window->host_hwnd) == FALSE)
            return;

        if (expectedParent
            && ::GetParent(window->host_hwnd) == expectedParent)
        {
            ::ShowWindow(window->host_hwnd, SW_HIDE);
        }
    }

    inline void restore_associated_host_window(
        const epochengine::core::WindowData* window,
        HWND parent,
        int desiredScreenX,
        int desiredScreenY,
        int clientW,
        int clientH) noexcept
    {
        if (!window)
            return;

        if (window->type == epochengine::core::ContextType::SDL
            && uses_visible_proxy_host(window))
        {
            if (!parent
                || ::IsWindow(parent) == FALSE
                || !window->host_hwnd
                || !window->hwndChild
                || ::IsWindow(window->host_hwnd) == FALSE
                || ::IsWindow(window->hwndChild) == FALSE)
            {
                return;
            }

            dock_host_window_to_parent(
                window->host_hwnd,
                parent,
                desiredScreenX,
                desiredScreenY,
                clientW,
                clientH);
            apply_child_fill_layout(window->hwndChild, window->host_hwnd, clientW, clientH);
            ::ShowWindow(window->host_hwnd, SW_SHOWNA);
            return;
        }

        if (!window
            || !parent
            || ::IsWindow(parent) == FALSE
            || !window->host_hwnd
            || window->host_hwnd == window->hwnd
            || window->host_hwnd == window->hwndChild
            || ::IsWindow(window->host_hwnd) == FALSE)
        {
            return;
        }

        dock_host_window_to_parent(
            window->host_hwnd,
            parent,
            desiredScreenX,
            desiredScreenY,
            clientW,
            clientH);
        ::ShowWindow(window->host_hwnd, SW_HIDE);
    }

    [[nodiscard]] inline bool is_sfml_proxy_candidate(
        const epochengine::core::WindowData* window) noexcept
    {
        return has_proxy_shell_pair(window);
    }

    [[nodiscard]] inline bool backend_uses_proxy_child(epochengine::core::ContextType type) noexcept
    {
        return type == epochengine::core::ContextType::SDL
            || type == epochengine::core::ContextType::SFML;
    }

    [[nodiscard]] inline bool backend_adopts_native_child(epochengine::core::ContextType type) noexcept
    {
        return type == epochengine::core::ContextType::RayLib;
    }

    [[nodiscard]] inline bool is_proxy_host_hwnd(
        const epochengine::core::WindowData* window,
        HWND hwnd) noexcept
    {
        return window
            && hwnd
            && window->host_hwnd == hwnd
            && window->hwndChild
            && window->hwndChild != hwnd
            && ::IsWindow(window->hwndChild) != FALSE;
    }

    [[nodiscard]] inline bool is_sfml_proxy_detached(
        const epochengine::core::WindowData* window) noexcept
    {
        return has_proxy_shell_pair(window)
            && ::GetParent(window->hwndChild) == window->host_hwnd
            && ::GetParent(window->host_hwnd) == nullptr;
    }

    [[nodiscard]] inline bool is_proxy_child_directly_docked(
        const epochengine::core::WindowData* window,
        HWND expectedParent = nullptr) noexcept
    {
        if (!has_proxy_shell_pair(window))
            return false;

        const HWND childParent = ::GetParent(window->hwndChild);
        const HWND hostParent = ::GetParent(window->host_hwnd);
        if (!childParent || childParent == window->host_hwnd)
            return false;

        if (hostParent && hostParent != childParent)
            return false;

        return !expectedParent || childParent == expectedParent;
    }

    [[nodiscard]] inline bool uses_hidden_proxy_shell(
        const epochengine::core::WindowData* window) noexcept
    {
        return has_proxy_shell_pair(window)
            && ::IsWindowVisible(window->host_hwnd) == FALSE;
    }

    [[nodiscard]] inline HWND proxy_drag_frame(
        const epochengine::core::WindowData* window,
        HWND dockParent,
        HWND fallback) noexcept
    {
        if (!window || !is_sfml_proxy_candidate(window))
            return fallback;

        if (is_sfml_proxy_detached(window))
            return window->host_hwnd ? window->host_hwnd : fallback;

        if (is_proxy_child_directly_docked(window, dockParent))
        {
            const HWND child = window->hwndChild.load(std::memory_order_acquire);
            return child ? child : fallback;
        }

        if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
            return window->host_hwnd;

        return fallback;
    }

    [[nodiscard]] inline bool proxy_drag_owns_host(
        const epochengine::core::WindowData* window) noexcept
    {
        if (!is_sfml_proxy_candidate(window))
            return false;

        const auto& drag = epochengine::core::Drag();
        return drag.dragging
            && (drag.draggedWindow == window->host_hwnd
                || drag.draggedWindow == window->hwndChild);
    }

    inline void apply_child_fill_layout(HWND child, HWND parent, int clientW, int clientH) noexcept
    {
        if (!child || !parent)
            return;

        LONG_PTR style = ::GetWindowLongPtrW(child, GWL_STYLE);
        style &= ~(WS_POPUP | WS_OVERLAPPEDWINDOW);
        style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        ::SetWindowLongPtrW(child, GWL_STYLE, style);
        if (::GetParent(child) != parent)
            ::SetParent(child, parent);

        ::SetWindowPos(
            child,
            nullptr,
            0,
            0,
            clientW,
            clientH,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    inline void position_top_level_shell(
        HWND hwnd,
        int desiredClientLeft,
        int desiredClientTop,
        int clientW,
        int clientH) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        LONG_PTR desiredExStyle = exStyle;
        desiredExStyle &= ~static_cast<LONG_PTR>(
            WS_EX_NOPARENTNOTIFY
            | WS_EX_TOOLWINDOW
            | WS_EX_TOPMOST);
        desiredExStyle |= WS_EX_APPWINDOW;
        if (desiredExStyle != exStyle)
            ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, desiredExStyle);

        if (const HWND currentParent = ::GetParent(hwnd);
            currentParent != nullptr)
        {
            ::SetPropW(hwnd, kEpochDockParentProp, currentParent);
            ::SetParent(hwnd, nullptr);
        }
        ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, 0);

        const DWORD adjustedExStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        RECT adjusted{ 0, 0, clientW, clientH };
        int hostX = desiredClientLeft;
        int hostY = desiredClientTop;
        int hostW = clientW;
        int hostH = clientH;
        if (::AdjustWindowRectEx(&adjusted, static_cast<DWORD>(style), FALSE, adjustedExStyle))
        {
            hostX += adjusted.left;
            hostY += adjusted.top;
            hostW = clamp_positive(adjusted.right - adjusted.left);
            hostH = clamp_positive(adjusted.bottom - adjusted.top);
        }

        ::ShowWindow(hwnd, SW_SHOWNORMAL);
        ::SetWindowPos(
            hwnd,
            nullptr,
            hostX,
            hostY,
            hostW,
            hostH,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        RECT actualRect{};
        if (::GetWindowRect(hwnd, &actualRect) != FALSE
            && (std::abs(actualRect.left - hostX) > 2
                || std::abs(actualRect.top - hostY) > 2
                || std::abs((actualRect.right - actualRect.left) - hostW) > 2
                || std::abs((actualRect.bottom - actualRect.top) - hostH) > 2))
        {
            ::MoveWindow(hwnd, hostX, hostY, hostW, hostH, TRUE);
            ::SetWindowPos(
                hwnd,
                HWND_TOP,
                hostX,
                hostY,
                hostW,
                hostH,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }

        apply_native_window_chrome(hwnd);
        ::BringWindowToTop(hwnd);
        ::SetActiveWindow(hwnd);
        ::SetForegroundWindow(hwnd);
    }

    inline void move_detached_top_level_shell(
        HWND hwnd,
        int desiredClientLeft,
        int desiredClientTop,
        int clientW,
        int clientH) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        const DWORD style = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE));
        const DWORD exStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        RECT adjusted{ 0, 0, clientW, clientH };
        int hostX = desiredClientLeft;
        int hostY = desiredClientTop;
        int hostW = clientW;
        int hostH = clientH;
        if (::AdjustWindowRectEx(&adjusted, style, FALSE, exStyle))
        {
            hostX += adjusted.left;
            hostY += adjusted.top;
            hostW = clamp_positive(adjusted.right - adjusted.left);
            hostH = clamp_positive(adjusted.bottom - adjusted.top);
        }

        ::SetWindowPos(
            hwnd,
            nullptr,
            hostX,
            hostY,
            hostW,
            hostH,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

        RECT actualRect{};
        if (::GetWindowRect(hwnd, &actualRect) != FALSE
            && (std::abs(actualRect.left - hostX) > 2
                || std::abs(actualRect.top - hostY) > 2
                || std::abs((actualRect.right - actualRect.left) - hostW) > 2
                || std::abs((actualRect.bottom - actualRect.top) - hostH) > 2))
        {
            ::MoveWindow(hwnd, hostX, hostY, hostW, hostH, TRUE);
            ::SetWindowPos(
                hwnd,
                HWND_TOP,
                hostX,
                hostY,
                hostW,
                hostH,
                SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        }

        apply_native_window_chrome(hwnd);
        ::BringWindowToTop(hwnd);
        const auto& drag = epochengine::core::Drag();
        if (drag.dragging && drag.draggedWindow == hwnd)
        {
            ::SetActiveWindow(hwnd);
            ::SetForegroundWindow(hwnd);
            ::SetFocus(hwnd);
        }
    }

    inline void sync_drag_offset_to_host_window(HWND hwnd) noexcept
    {
        if (!hwnd || ::IsWindow(hwnd) == FALSE)
            return;

        POINT cursor{};
        RECT hostRect{};
        if (::GetCursorPos(&cursor) == FALSE || ::GetWindowRect(hwnd, &hostRect) == FALSE)
            return;

        auto& drag = epochengine::core::Drag();
        drag.dragWindowOffset.x = cursor.x - hostRect.left;
        drag.dragWindowOffset.y = cursor.y - hostRect.top;
    }

    inline void undock_sfml_proxy_window(
        epochengine::core::WindowData* window,
        int desiredClientLeft,
        int desiredClientTop,
        int clientW,
        int clientH) noexcept
    {
        if (!is_sfml_proxy_candidate(window))
            return;

        window->isFloating = true;
        position_top_level_shell(
            window->host_hwnd,
            desiredClientLeft,
            desiredClientTop,
            clientW,
            clientH);
        apply_child_fill_layout(window->hwndChild, window->host_hwnd, clientW, clientH);
        window->set_size(clientW, clientH);
        if (proxy_drag_owns_host(window))
            ::SetFocus(window->host_hwnd);
        else
        {
            const HWND child = window->hwndChild.load(std::memory_order_acquire);
            ::SetFocus(child ? child : window->host_hwnd);
        }
    }

    inline void redock_sfml_proxy_window(
        epochengine::core::WindowData* window,
        HWND parent,
        int desiredScreenX,
        int desiredScreenY,
        int clientW,
        int clientH) noexcept
    {
        if (!is_sfml_proxy_candidate(window) || !parent || ::IsWindow(parent) == FALSE)
            return;

        window->isFloating = false;
#if defined(_DEBUG)
        epochengine::logger::get(kLogSys).logf(
            epochengine::logger::LogLevel::INFO,
            std::source_location::current(),
            "SFML redock begin host={} child={} hostParentBefore={} childParentBefore={} targetParent={}",
            static_cast<void*>(window->host_hwnd),
            static_cast<void*>(window->hwndChild),
            static_cast<void*>(::GetParent(window->host_hwnd)),
            static_cast<void*>(::GetParent(window->hwndChild)),
            static_cast<void*>(parent));
#endif

        if (uses_visible_proxy_host(window))
        {
            dock_host_window_to_parent(
                window->host_hwnd,
                parent,
                desiredScreenX,
                desiredScreenY,
                clientW,
                clientH);
            apply_child_fill_layout(window->hwndChild, window->host_hwnd, clientW, clientH);
            ::ShowWindow(window->host_hwnd, SW_SHOWNA);
            ::ShowWindow(window->hwndChild, SW_SHOWNA);
            window->set_size(clientW, clientH);
            if (proxy_drag_owns_host(window))
            {
                const HWND child = window->hwndChild.load(std::memory_order_acquire);
                ::SetFocus(child ? child : window->host_hwnd);
            }
        }
        else
        {
            dock_host_window_to_parent(
                window->hwndChild,
                parent,
                desiredScreenX,
                desiredScreenY,
                clientW,
                clientH);
            ::ShowWindow(window->hwndChild, SW_SHOWNA);
            if (window->host_hwnd
                && window->host_hwnd != window->hwndChild
                && ::IsWindow(window->host_hwnd) != FALSE)
            {
                dock_host_window_to_parent(
                    window->host_hwnd,
                    parent,
                    desiredScreenX,
                    desiredScreenY,
                    clientW,
                    clientH);
                ::ShowWindow(window->host_hwnd, SW_HIDE);
            }
            window->set_size(clientW, clientH);
            if (proxy_drag_owns_host(window))
                ::SetFocus(window->hwndChild);
        }

#if defined(_DEBUG)
        epochengine::logger::get(kLogSys).logf(
            epochengine::logger::LogLevel::INFO,
            std::source_location::current(),
            "SFML redock end host={} child={} hostParentAfter={} childParentAfter={} hostVisible={}",
            static_cast<void*>(window->host_hwnd),
            static_cast<void*>(window->hwndChild),
            static_cast<void*>(::GetParent(window->host_hwnd)),
            static_cast<void*>(::GetParent(window->hwndChild)),
            ::IsWindowVisible(window->host_hwnd) != FALSE);
#endif
    }

    inline void post_proxy_host_command(
        const epochengine::core::WindowData* window,
        ProxyDockCmd command,
        HWND parent,
        int x,
        int y,
        int width,
        int height) noexcept
    {
        if (!is_sfml_proxy_candidate(window))
            return;

        const HWND child = window->hwndChild.load(std::memory_order_acquire);
        auto* request = new (std::nothrow) ProxyDockRequest{
            .sourceHwnd = child ? child : window->host_hwnd,
            .parentHwnd = parent,
            .x = x,
            .y = y,
            .width = width,
            .height = height
        };
        if (!request)
            return;

        if (!::PostMessageW(
            window->host_hwnd,
            WM_EPOCH_PROXY_DOCKCMD,
            static_cast<WPARAM>(command),
            reinterpret_cast<LPARAM>(request)))
        {
            delete request;
        }
#if defined(_DEBUG)
        else
        {
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Posted SFML proxy command={} host={} child={} hostParent={} childParent={} targetParent={}",
                static_cast<int>(command),
                static_cast<void*>(window->host_hwnd),
                static_cast<void*>(window->hwndChild),
                static_cast<void*>(::GetParent(window->host_hwnd)),
                static_cast<void*>(::GetParent(window->hwndChild)),
                static_cast<void*>(parent));
        }
#endif
    }

    [[nodiscard]] inline bool backend_requires_owner_thread_dock_commands(
        const epochengine::core::WindowData* window) noexcept
    {
        return window
            && window->type == epochengine::core::ContextType::RayLib;
    }

    [[nodiscard]] inline HWND owner_thread_dock_handle(
        const epochengine::core::WindowData* window) noexcept
    {
        if (!window)
            return nullptr;

        if (window->hwndChild && ::IsWindow(window->hwndChild) != FALSE)
            return window->hwndChild;
        if (window->hwnd && ::IsWindow(window->hwnd) != FALSE)
            return window->hwnd;
        if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
            return window->host_hwnd;
        return nullptr;
    }

    [[nodiscard]] inline bool post_owner_thread_dock_command(
        epochengine::core::WindowData* window,
        ProxyDockCmd command,
        HWND parent,
        int x,
        int y,
        int width,
        int height) noexcept
    {
        if (!backend_requires_owner_thread_dock_commands(window))
            return false;

        const HWND target = owner_thread_dock_handle(window);
        if (!target)
            return false;

        if (command == ProxyDockCmd::Redock
            && (!parent || ::IsWindow(parent) == FALSE))
        {
            return false;
        }

        window->firstPresentComplete.store(false, std::memory_order_release);
        window->ownerThreadCommandQueue.enqueue([window, target, command, parent, x, y, width, height]()
            {
                if (!window || !target || ::IsWindow(target) == FALSE)
                    return;

                switch (command)
                {
                case ProxyDockCmd::Undock:
                    window->isFloating = true;
                    if (parent && ::IsWindow(parent) != FALSE)
                        ::SetPropW(target, kEpochDockParentProp, parent);
                    position_top_level_shell(target, x, y, width, height);
                    break;

                case ProxyDockCmd::MoveDetached:
                    window->isFloating = true;
                    move_detached_top_level_shell(target, x, y, width, height);
                    break;

                case ProxyDockCmd::Redock:
                    window->isFloating = false;
                    // This command already runs on the Raylib/GLFW owner thread.
                    // Place it synchronously before accepting the next present.
                    dock_host_window_to_parent(target, parent, x, y, width, height, false);
                    hide_associated_host_window(window, target, parent);
                    ::ShowWindow(target, SW_SHOWNA);
                    ::UpdateWindow(target);
                    ::SetFocus(target);
                    break;
                }

                window->hwnd = target;
                window->hwndChild = target;
                window->set_size(width, height);
            });
        return true;
    }

    [[nodiscard]] bool route_context_close_shortcut(HWND hwnd, UINT msg,
        WPARAM key, LPARAM flags) noexcept
    {
        if (msg != WM_SYSKEYDOWN || key != VK_F4
            || (static_cast<std::uintptr_t>(flags) & (std::uintptr_t{1} << 29u)) == 0)
            return false;
        // Holding the shortcut must not cascade through every context.
        if ((static_cast<std::uintptr_t>(flags) & (std::uintptr_t{1} << 30u)) != 0)
            return true;
        if (auto* manager = epochengine::core::GetActiveMultiContextManager())
            return manager->CloseHoveredContext(hwnd);
        return false;
    }

    inline void forward_gui_input_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        const auto ctx = resolve_gui_context_for_hwnd(hwnd);
        if (!ctx)
            return;

        switch (msg)
        {
        case WM_LBUTTONDOWN:
            remember_gui_input_owner(hwnd);
            focus_gui_mouse_target(hwnd);
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseDown, lParam);
            break;
        case WM_RBUTTONDOWN:
            remember_gui_input_owner(hwnd);
            focus_gui_mouse_target(hwnd);
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseDown, lParam, 0, false, 1);
            break;
        case WM_MOUSEMOVE:
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseMove, lParam);
            break;
        case WM_LBUTTONUP:
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseUp, lParam);
            break;
        case WM_RBUTTONUP:
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseUp, lParam, 0, false, 1);
            break;
        case WM_MOUSEWHEEL:
            push_gui_mouse_event(
                ctx.get(),
                hwnd,
                epochengine::gui::EventType::MouseWheel,
                lParam,
                GET_WHEEL_DELTA_WPARAM(wParam),
                true);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (!accepts_gui_keyboard_input(hwnd))
                break;
            push_gui_key_event(ctx.get(), static_cast<int>(wParam));
            break;
        case WM_CHAR:
        case WM_SYSCHAR:
            if (!accepts_gui_keyboard_input(hwnd))
                break;
            if (wParam >= 0x20u || wParam == 13u)
                push_gui_text_event(ctx.get(), static_cast<char32_t>(wParam));
            break;
        default:
            break;
        }
    }

    epochengine::platform::window_adapter::NativeInputResult backend_input_callback(void* owner,
        epochengine::platform::window_adapter::NativeInputAction action,
        const epochengine::platform::window_adapter::NativeInputMessage& message) noexcept
    {
        // Registration belongs to the manager active when this surface was
        // admitted. Never route a stale callback through a replacement manager.
        if (owner != epochengine::core::GetActiveMultiContextManager()) return {};
        using epochengine::platform::window_adapter::NativeInputAction;
        const auto hwnd = reinterpret_cast<HWND>(message.window.value);
        const auto msg = static_cast<UINT>(message.message);
        const auto wp = static_cast<WPARAM>(message.key);
        const auto lp = static_cast<LPARAM>(message.flags);
        switch (action)
        {
        case NativeInputAction::close_shortcut:
            return {route_context_close_shortcut(hwnd, msg, wp, lp), 0};
        case NativeInputAction::focus_gained:
            remember_gui_input_owner(hwnd);
            return {};
        case NativeInputAction::focus_lost:
            forget_gui_input_owner(hwnd, reinterpret_cast<HWND>(wp));
            return {};
        case NativeInputAction::retiring:
            forget_gui_input_owner(hwnd);
            return {};
        case NativeInputAction::input:
            forward_gui_input_message(hwnd, msg, wp, lp);
            return {};
        case NativeInputAction::proxy_drag:
        {
            auto* const window = resolve_window_data_for_hwnd(hwnd);
            const auto& drag = epochengine::core::Drag();
            const bool backendDockableChild =
                window
                && (window->type == epochengine::core::ContextType::SDL
                    || window->type == epochengine::core::ContextType::SFML)
                && hwnd == window->hwndChild;
            const bool proxyDragStart =
                backendDockableChild
                && msg == WM_LBUTTONDOWN
                && is_dock_drag_hotspot(hwnd, lp);
            const bool proxyContinueDrag =
                backendDockableChild
                && drag.dragging
                && (drag.draggedWindow == hwnd
                    || drag.draggedWindow == window->host_hwnd);
            if (proxyDragStart || proxyContinueDrag)
            {
                return {true, epochengine::core::MultiContextManager::ChildProc(hwnd, msg, wp, lp)};
            }
            return {};
        }
        default: return {};
        }
    }


    LRESULT CALLBACK DockableProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dw)
    {
        if (route_context_close_shortcut(hwnd, msg, wp, lp)) return 0;
        auto* ctx = reinterpret_cast<SubCtx*>(dw);

        switch (msg)
        {
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            cancel_native_context_drag(hwnd);
            return DefSubclassProc(hwnd, msg, wp, lp);
        case WM_SETFOCUS:
            remember_gui_input_owner(hwnd);
            return DefSubclassProc(hwnd, msg, wp, lp);
        case WM_KILLFOCUS:
            forget_gui_input_owner(hwnd, reinterpret_cast<HWND>(wp));
            return DefSubclassProc(hwnd, msg, wp, lp);
        case WM_NCDESTROY:
            forget_gui_input_owner(hwnd);
            ::RemovePropW(hwnd, kEpochDockParentProp);
            delete ctx;
            return DefSubclassProc(hwnd, msg, wp, lp);

        case WM_EPOCH_DOCKCMD:
        {
            if (static_cast<DockCmd>(wp) == DockCmd::Undock)
            {
                // Convert to a top-level window, preserving client size.
                RECT clientRect{};
                ::GetClientRect(hwnd, &clientRect);
                const int clientW = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
                const int clientH = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));

                LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
                style &= ~WS_CHILD;
                style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE;
                ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);

                const DWORD exStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
                RECT adjusted{ 0, 0, clientW, clientH };
                if (::AdjustWindowRectEx(&adjusted, static_cast<DWORD>(style), FALSE, exStyle))
                {
                    const int wndW = clamp_positive(adjusted.right - adjusted.left);
                    const int wndH = clamp_positive(adjusted.bottom - adjusted.top);
                    ::SetParent(hwnd, nullptr);
                    ::SetWindowPos(hwnd, nullptr, 0, 0, wndW, wndH,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }
                else
                {
                    ::SetParent(hwnd, nullptr);
                    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }
                return 0;
            }
            break;
        }

        case WM_CLOSE:
        {
            // If this is a GLFW-owned HWND (raylib), do NOT DestroyWindow here.
            // Let GLFW/raylib handle the close.
            wchar_t cls[64]{};
            ::GetClassNameW(hwnd, cls, static_cast<int>(std::size(cls)));
            if (wcsncmp(cls, L"GLFW", 4) == 0)
                return DefSubclassProc(hwnd, msg, wp, lp);

            if (auto* const manager = g_activeManager;
                manager && resolve_window_data_for_hwnd(hwnd))
            {
                // Backend-owned children release their renderer and native
                // surface on the renderer thread. The manager destroys only
                // the host after that thread has completed.
                manager->RemoveWindow(hwnd);
            }
            else
            {
                ::DestroyWindow(hwnd);
            }
            return 0;
        }

        case WM_NCLBUTTONDOWN:
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONUP:
        {
            const auto& drag = epochengine::core::Drag();
            if ((msg == WM_NCLBUTTONDOWN && static_cast<UINT>(wp) == HTCAPTION)
                || (drag.dragging && drag.draggedWindow == hwnd))
                return epochengine::core::MultiContextManager::ChildProc(hwnd, msg, wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);
        }

        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
        {
            // Epoch's class procedure already owns pointer delivery. Preserve
            // its full route instead of forwarding once here and once below.
            if (native_class_owns_gui_input(hwnd))
                return DefSubclassProc(hwnd, msg, wp, lp);
            const auto& dragState = epochengine::core::Drag();
            const bool continueDrag = dragState.dragging && dragState.draggedWindow == hwnd;
            const bool dragStart = (msg == WM_LBUTTONDOWN) && is_dock_drag_hotspot(hwnd, lp);
            if (dragStart || continueDrag)
                return epochengine::core::MultiContextManager::ChildProc(hwnd, msg, wp, lp);
            forward_gui_input_message(hwnd, msg, wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);
        }

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEWHEEL:
            if (native_class_owns_gui_input(hwnd))
                return DefSubclassProc(hwnd, msg, wp, lp);
            forward_gui_input_message(hwnd, msg, wp, lp);
            return DefSubclassProc(hwnd, msg, wp, lp);

        case WM_CHAR:
        case WM_SYSCHAR:
            forward_gui_input_message(hwnd, msg, wp, lp);
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            forward_gui_input_message(hwnd, msg, wp, lp);
            return 0;
        }

        return DefSubclassProc(hwnd, msg, wp, lp);
    }
#else
    struct SubCtx { HWND originalParent{}; };
    constexpr wchar_t kEpochDockParentProp[] = L"EpochDockParent";
    constexpr wchar_t kEpochLayoutPendingProp[] = L"EpochLayoutPending";
    constexpr UINT WM_EPOCH_LAYOUT = WM_APP + 0x4A12;
    constexpr UINT WM_EPOCH_PROXY_DOCKCMD = WM_APP + 0x4A13;
    constexpr UINT WM_EPOCH_OPEN_DETACHED_CONTEXT = WM_APP + 0x4A14;
    constexpr UINT WM_EPOCH_RETIRE_CONTEXT_WINDOW = WM_APP + 0x4A15;

    enum class ProxyDockCmd : WPARAM
    {
        Undock = 1,
        MoveDetached = 2,
        Redock = 3,
    };

    struct ProxyDockRequest
    {
        HWND sourceHwnd{};
        HWND parentHwnd{};
        int x{};
        int y{};
        int width{};
        int height{};
    };

    inline void request_parent_layout(HWND) noexcept {}
    inline void remember_gui_input_owner(HWND) noexcept {}
    inline void forget_gui_input_owner(HWND) noexcept {}
    [[nodiscard]] inline bool accepts_gui_keyboard_input(HWND) noexcept { return true; }

    [[nodiscard]] inline HWND stored_dock_parent(HWND) noexcept { return nullptr; }

    [[nodiscard]] inline RECT screen_client_rect(HWND) noexcept { return RECT{}; }
    [[nodiscard]] inline bool point_in_rect(const RECT&, const POINT&) noexcept { return false; }
    [[nodiscard]] inline bool should_redock_to_parent(HWND, const POINT&, const RECT&) noexcept { return false; }

    [[nodiscard]] inline POINT screen_mouse_point(HWND hwnd, UINT msg, LPARAM lParam) noexcept
    {
        (void)hwnd;
        if (msg == WM_NCMOUSEMOVE || msg == WM_NCLBUTTONUP || msg == WM_NCLBUTTONDOWN)
            return POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        POINT pt{};
        if (::GetCursorPos(&pt) != FALSE)
            return pt;
        return POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    }

    [[nodiscard]] inline epochengine::core::WindowData* resolve_window_data_for_hwnd(HWND hwnd) noexcept
    {
        auto* mgr = g_activeManager;
        return (mgr && hwnd) ? mgr->findWindowByHWND(hwnd) : nullptr;
    }

    [[nodiscard]] inline bool is_dock_drag_hotspot(HWND, LPARAM) noexcept { return false; }
    [[nodiscard]] inline bool pointer_inside_parent_client(HWND, const POINT&) noexcept { return false; }
    [[nodiscard]] inline bool pointer_inside_parent_dock_region(HWND, const POINT&) noexcept { return false; }
    [[nodiscard]] inline bool drag_distance_exceeded(const POINT&, const POINT&, int = 10) noexcept { return false; }
    [[nodiscard]] inline bool has_proxy_shell_pair(const epochengine::core::WindowData*) noexcept { return false; }
    [[nodiscard]] inline bool is_sfml_proxy_candidate(const epochengine::core::WindowData*) noexcept { return false; }
    [[nodiscard]] inline bool is_proxy_host_hwnd(const epochengine::core::WindowData*, HWND) noexcept { return false; }
    [[nodiscard]] inline bool is_sfml_proxy_detached(const epochengine::core::WindowData*) noexcept { return false; }
    [[nodiscard]] inline bool is_proxy_child_directly_docked(const epochengine::core::WindowData*, HWND = nullptr) noexcept { return false; }
    [[nodiscard]] inline bool backend_uses_proxy_child(epochengine::core::ContextType) noexcept { return false; }
    [[nodiscard]] inline bool backend_adopts_native_child(epochengine::core::ContextType) noexcept { return false; }
    [[nodiscard]] inline HWND proxy_drag_frame(const epochengine::core::WindowData*, HWND, HWND fallback) noexcept { return fallback; }

    [[nodiscard]] inline POINT force_proxy_shell_outside_parent(
        HWND,
        const POINT&,
        int proposedClientLeft,
        int proposedClientTop,
        int,
        int) noexcept
    {
        return POINT{ proposedClientLeft, proposedClientTop };
    }

    inline void dock_host_window_to_parent(HWND, HWND, int, int, int, int) noexcept {}
    inline void apply_child_fill_layout(HWND, HWND, int, int) noexcept {}
    inline void move_detached_top_level_shell(HWND, int, int, int, int) noexcept {}
    inline void sync_drag_offset_to_host_window(HWND) noexcept {}
    inline void hide_associated_host_window(const epochengine::core::WindowData*, HWND, HWND) noexcept {}
    inline void restore_associated_host_window(const epochengine::core::WindowData*, HWND, int, int, int, int) noexcept {}
    inline void undock_sfml_proxy_window(epochengine::core::WindowData*, int, int, int, int) noexcept {}
    inline void redock_sfml_proxy_window(epochengine::core::WindowData*, HWND, int, int, int, int) noexcept {}
    inline void post_proxy_host_command(const epochengine::core::WindowData*, ProxyDockCmd, HWND, int, int, int, int) noexcept {}

    epochengine::platform::window_adapter::NativeInputResult backend_input_callback(void*,
        epochengine::platform::window_adapter::NativeInputAction,
        const epochengine::platform::window_adapter::NativeInputMessage&) noexcept
    {
        return {};
    }
#endif

    inline void cleanup_window_resources(std::unique_ptr<epochengine::core::WindowData>& window) noexcept
    {
        if (!window)
            return;

        window->ownerThreadCommandQueue.clear();
        window->commandQueue.clear();

        if (release_external_attachment(window.get()))
        {
            // The child process owns this HWND/DC/renderer. Retiring only the
            // host lease must not call DestroyWindow on a foreign or reused HWND.
            window->set_backend_lifecycle(epochengine::core::BackendLifecycleState::stopped);
            window->host_hwnd = nullptr;
            window->hwnd = nullptr;
            window->hwndChild = nullptr;
            return;
        }

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        if (window->ownsNativeGlContext && window->glContext)
        {
            ::wglMakeCurrent(nullptr, nullptr);
            ::wglDeleteContext(window->glContext);
            window->glContext = nullptr;
            window->ownsNativeGlContext = false;
        }
#endif
        if (window->ownsNativeDc && window->hdc)
        {
            HWND releaseTarget = nullptr;
            if (window->hwndChild && ::IsWindow(window->hwndChild) != FALSE)
                releaseTarget = window->hwndChild;
            else if (window->hwnd && ::IsWindow(window->hwnd) != FALSE)
                releaseTarget = window->hwnd;
            else if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
                releaseTarget = window->host_hwnd;

            if (releaseTarget)
                ::ReleaseDC(releaseTarget, window->hdc);
            window->hdc = nullptr;
            window->ownsNativeDc = false;
        }

        if (auto context = typed_context(window->context);
            context && context->windowData == window.get())
        {
            context->windowData = nullptr;
            context->hwnd = nullptr;
            context->hdc = nullptr;
            context->hglrc = nullptr;
            context->native_window = nullptr;
            context->native_drawable = nullptr;
            context->native_gl_context = nullptr;
        }

        const HWND host = window->host_hwnd ? window->host_hwnd : window->hwnd;
        if (host && ::IsWindow(host) != FALSE)
            ::DestroyWindow(host);

        window->host_hwnd = nullptr;
        window->hwnd = nullptr;
        window->hwndChild = nullptr;
    }

    [[nodiscard]] inline HWND primary_window_handle(const epochengine::core::WindowData* window) noexcept
    {
        if (!window)
            return nullptr;

        if (window->hwnd && ::IsWindow(window->hwnd) != FALSE)
            return window->hwnd;
        const HWND child = window->hwndChild.load(std::memory_order_acquire);
        if (child && ::IsWindow(child) != FALSE)
            return child;
        if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
            return window->host_hwnd;
        return window->hwnd ? window->hwnd : (child ? child : window->host_hwnd);
    }

    [[nodiscard]] inline HWND render_thread_key(const epochengine::core::WindowData* window) noexcept
    {
        if (!window)
            return nullptr;
        return window->host_hwnd ? window->host_hwnd : window->hwnd;
    }

    [[nodiscard]] inline HWND dock_slot_handle(
        const epochengine::core::WindowData* window,
        HWND dockParent) noexcept
    {
        if (!window || !dockParent || ::IsWindow(dockParent) == FALSE)
            return nullptr;

        if (window->hwndChild
            && ::IsWindow(window->hwndChild) != FALSE
            && ::GetParent(window->hwndChild) == dockParent)
        {
            return window->hwndChild;
        }

        if (window->host_hwnd
            && ::IsWindow(window->host_hwnd) != FALSE
            && ::GetParent(window->host_hwnd) == dockParent)
        {
            return window->host_hwnd;
        }

        if (window->hwnd
            && ::IsWindow(window->hwnd) != FALSE
            && ::GetParent(window->hwnd) == dockParent)
        {
            return window->hwnd;
        }

        return nullptr;
    }

    [[nodiscard]] inline bool matches_window_handle(
        const epochengine::core::WindowData* window,
        HWND hwnd) noexcept
    {
        return window
            && hwnd
            && (window->hwnd == hwnd
                || window->hwndChild == hwnd
                || window->host_hwnd == hwnd);
    }

    [[nodiscard]] inline bool thread_finished(std::thread& thread) noexcept
    {
        if (!thread.joinable()) return true;
        HANDLE handle = static_cast<HANDLE>(thread.native_handle());
        if (!handle) return false;
        return ::WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
    }

    inline void pump_pending_host_messages() noexcept
    {
        MSG msg{};
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
        {
            if (msg.message == WM_QUIT)
                continue;

            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    inline void join_thread_with_message_pump(std::thread& thread) noexcept
    {
        while (thread.joinable() && !thread_finished(thread))
        {
            pump_pending_host_messages();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (thread.joinable())
            thread.join();
    }

    struct NativeTitleFpsState
    {
        std::wstring baseTitle{};
        epochengine::renderhost::frame_rate::Sample frameRate{};
        double fps = 0.0;
    };

    std::mutex g_nativeTitleFpsMutex;
    std::unordered_map<const epochengine::core::WindowData*, NativeTitleFpsState> g_nativeTitleFps;

    // The HWND owner applies ordinary native caption text. No custom GDI
    // painting or non-client repaint scheduling belongs to this cache.
    struct NativeTitleUpdate
    {
        std::wstring title{};
        const epochengine::core::WindowData* source{};
        std::uintptr_t notification{};
        bool pending{};
    };
    std::mutex g_nativeFpsTitlesMutex;
    std::unordered_map<HWND, NativeTitleUpdate> g_nativeFpsTitles;
    std::uintptr_t g_nativeFpsTitleNotification{};
    constexpr UINT WM_EPOCH_REFRESH_FPS_TITLE = WM_APP + 0x4A16;

    [[nodiscard]] DWORD native_title_owner_thread(HWND hwnd) noexcept
    {
        DWORD process{};
        const DWORD thread = hwnd ? ::GetWindowThreadProcessId(hwnd, &process) : 0u;
        if (!thread || process != ::GetCurrentProcessId()) return 0u;
        const auto procedure = reinterpret_cast<WNDPROC>(::GetClassLongPtrW(hwnd, GCLP_WNDPROC));
        // Backend-owned SDL/SFML/GLFW windows do not dispatch our message.
        // Their generic Epoch proxy host and parent remain eligible targets.
        return procedure == epochengine::core::MultiContextManager::ParentProc
            || procedure == epochengine::core::MultiContextManager::ChildProc ? thread : 0u;
    }

    void forget_native_title_window(HWND hwnd) noexcept
    {
        std::scoped_lock lock(g_nativeFpsTitlesMutex);
        g_nativeFpsTitles.erase(hwnd);
    }

    void queue_native_title_if_owned(HWND hwnd, const std::wstring& title,
        const epochengine::core::WindowData* source = nullptr)
    {
        if (!native_title_owner_thread(hwnd)) return;
        std::uintptr_t notification{};
        {
            std::scoped_lock lock(g_nativeFpsTitlesMutex);
            if (g_nativeFpsTitleNotification == ~std::uintptr_t{0}) return;
            auto& update = g_nativeFpsTitles[hwnd];
            update.title = title;
            update.source = source;
            if (update.pending) return; // Latest text replaces the queued value.
            notification = ++g_nativeFpsTitleNotification;
            update.notification = notification;
            update.pending = true;
        }
        // No pointer/heap payload and no synchronous fallback from a renderer.
        if (!::PostMessageW(hwnd, WM_EPOCH_REFRESH_FPS_TITLE,
                static_cast<WPARAM>(notification), 0))
        {
            std::scoped_lock lock(g_nativeFpsTitlesMutex);
            if (const auto found = g_nativeFpsTitles.find(hwnd);
                found != g_nativeFpsTitles.end() && found->second.notification == notification)
                found->second.pending = false;
        }
    }

    void apply_pending_native_title(HWND hwnd, std::uintptr_t notification)
    {
        if (native_title_owner_thread(hwnd) != ::GetCurrentThreadId()) return;
        std::wstring title;
        {
            std::scoped_lock lock(g_nativeFpsTitlesMutex);
            const auto found = g_nativeFpsTitles.find(hwnd);
            if (found == g_nativeFpsTitles.end() || !found->second.pending
                || found->second.notification != notification) return;
            title = found->second.title;
            found->second.pending = false;
        }
        // This callback runs in the HWND owner, outside every cache lock.
        (void)::SetWindowTextW(hwnd, title.c_str());
    }

    [[nodiscard]] std::wstring make_native_fps_title(
        std::wstring_view base,
        double fps)
    {
        std::wstring title{ base };
        title += L" | ";
        title += std::to_wstring(static_cast<long long>(fps + 0.5));
        title += L" FPS avg";
        return title;
    }

    void record_native_title_frame(
        epochengine::core::MultiContextManager* manager,
        epochengine::core::WindowData& window)
    {
        const auto now = std::chrono::steady_clock::now();
        std::wstring childTitle{};
        std::wstring parentTitle{};
        bool shouldUpdate = false;

        {
            std::scoped_lock lock(g_nativeTitleFpsMutex);
            auto& state = g_nativeTitleFps[&window];
            if (state.baseTitle.empty())
            {
                state.baseTitle = window.titleWide.empty()
                    ? L"Epoch Context"
                    : window.titleWide;
            }

            const double seconds = std::chrono::duration<double>(now.time_since_epoch()).count();
            if (!state.frameRate.observe(
                    window.successfulFrameGeneration.load(std::memory_order_acquire),
                    seconds, &window))
                return;
            state.fps = state.frameRate.average_rate();

            childTitle = make_native_fps_title(
                state.baseTitle, state.fps);
            parentTitle = L"";

            for (const auto& [_, sampled] : g_nativeTitleFps)
            {
                if (sampled.baseTitle.empty() || sampled.fps <= 0.0)
                    continue;

                if (!parentTitle.empty())
                {
                    parentTitle += L" | ";
                }
                parentTitle += sampled.baseTitle;
                parentTitle += L" | ";
                const auto fpsText = std::to_wstring(static_cast<long long>(sampled.fps + 0.5)) + L" FPS";
                parentTitle += fpsText;
                parentTitle += L" avg";
            }

            shouldUpdate = true;
        }

        if (!shouldUpdate)
            return;

        queue_native_title_if_owned(window.hwnd, childTitle, &window);
        if (window.hwndChild != window.hwnd)
            queue_native_title_if_owned(window.hwndChild, childTitle, &window);
        if (window.host_hwnd != window.hwnd && window.host_hwnd != window.hwndChild)
            queue_native_title_if_owned(window.host_hwnd, childTitle, &window);

        if (manager)
        {
            HWND parent = manager->GetParentWindow();
            if (parent)
            {
                queue_native_title_if_owned(parent, parentTitle);
            }
        }
    }

    void forget_native_title_frame_source(const epochengine::core::WindowData* window) noexcept
    {
        if (!window)
            return;

        {
            std::scoped_lock lock(g_nativeTitleFpsMutex);
            g_nativeTitleFps.erase(window);
        }
        std::scoped_lock lock(g_nativeFpsTitlesMutex);
        std::erase_if(g_nativeFpsTitles, [window](const auto& item)
            { return item.second.source == window; });
    }
}

