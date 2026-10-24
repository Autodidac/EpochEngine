    LRESULT CALLBACK MultiContextManager::ParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCDESTROY)
        {
            forget_native_title_window(hwnd);
            epochengine::platform::window_adapter::forget_native_child_close_group(
                {reinterpret_cast<std::uintptr_t>(hwnd)});
        }
        if (route_context_close_shortcut(hwnd, msg, wParam, lParam)) return 0;
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        auto* mgr = reinterpret_cast<MultiContextManager*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!mgr) return ::DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_EPOCH_REFRESH_FPS_TITLE:
            apply_pending_native_title(hwnd, static_cast<std::uintptr_t>(wParam));
            return 0;
        case WM_CREATE:
        {
            (void)::SetTimer(hwnd, kThemeRefreshTimer, kThemeRefreshIntervalMs, nullptr);
            apply_native_window_chrome(hwnd);
            return 0;
        }
        case WM_SHOWWINDOW:
        case WM_ACTIVATE:
            apply_native_window_chrome(hwnd);
            return 0;
        case WM_TIMER:
            if (wParam == kThemeRefreshTimer)
            {
                try { (void)mgr->ResolveHoveredContext(hwnd); } catch (...) {}
                apply_native_window_chrome(hwnd);
                return 0;
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_THEMECHANGED:
        case WM_SETTINGCHANGE:
        case WM_SYSCOLORCHANGE:
        case WM_DWMCOMPOSITIONCHANGED:
        {
            apply_native_window_chrome(hwnd);
            const auto result = ::DefWindowProcW(hwnd, msg, wParam, lParam);
            return result;
        }

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
                request_parent_layout(hwnd);
            return 0;

        case WM_EXITSIZEMOVE:
            request_parent_layout(hwnd);
            return 0;

        case WM_EPOCH_LAYOUT:
            ::RemovePropW(hwnd, kEpochLayoutPendingProp);
            mgr->ArrangeDockedWindowsGrid();
            return 0;

        case WM_EPOCH_OPEN_DETACHED_CONTEXT:
        {
            std::unique_ptr<DetachedContextWindowRequest> request{
                reinterpret_cast<DetachedContextWindowRequest*>(lParam)
            };
            if (request)
                static_cast<void>(mgr->CreateDetachedContextWindowOnOwnerThread(*request));
            return 0;
        }

        case WM_EPOCH_RETIRE_CONTEXT_WINDOW:
            mgr->RemoveWindow(reinterpret_cast<HWND>(lParam));
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = ::BeginPaint(hwnd, &ps);
            ::FillRect(hdc, &ps.rcPaint, parent_background_brush());
            paint_parent_context_dock_overlay(hwnd, hdc);
            ::EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DROPFILES:
            mgr->HandleDropFiles(hwnd, reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_CLOSE:
        {
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Parent WM_CLOSE received hwnd={} - beginning orderly child shutdown.",
                static_cast<void*>(hwnd));
            std::vector<HWND> children;
            {
                std::scoped_lock lock(mgr->windowsMutex);
                children.reserve(mgr->windows.size());
                for (const auto& win : mgr->windows)
                {
                    if (!win)
                        continue;

                    if (external_attachment(win.get()))
                    {
                        // The supervisor owns the candidate process lifetime;
                        // this manager owns only its native attachment lease.
                        // Do not post WM_CLOSE to a foreign numeric HWND that
                        // may have died/reused since registration. Editor
                        // teardown retires the supervised process by handle.
                        win->running = false;
                        win->set_should_close(true);
                        continue;
                    }

                    HWND closeTarget = nullptr;
                    if (win->host_hwnd
                        && ::IsWindow(win->host_hwnd) != FALSE)
                    {
                        closeTarget = win->host_hwnd;
                    }
                    else
                    {
                        closeTarget = primary_window_handle(win.get());
                    }

                    if (!closeTarget || closeTarget == hwnd || ::IsWindow(closeTarget) == FALSE)
                        continue;

                    win->running = false;
                    win->set_should_close(true);

                    if (auto liveContext = typed_context(win->context);
                        liveContext && liveContext->windowData == win.get())
                    {
                        liveContext->windowData->set_should_close(true);
                    }

                    children.push_back(closeTarget);
                }
            }

            std::sort(children.begin(), children.end());
            children.erase(std::unique(children.begin(), children.end()), children.end());

            // Parent shutdown should close the live child windows in place.
            // Undocking here is the wrong behavior: it visibly tears panes out of the
            // parent host during shutdown and can confuse backend-owned child HWNDs.
            for (HWND child : children)
            {
                ::PostMessageW(child, WM_CLOSE, 0, 0);
            }

            mgr->parent = nullptr;
            if (auto* mgr = s_activeInstance)
                mgr->RemoveWindow(hwnd);
            return 0;
        }

        case WM_DESTROY:
            (void)::KillTimer(hwnd, kThemeRefreshTimer);
            (void)::RemovePropW(hwnd, kWindowThemeProperty);
            forget_native_title_window(hwnd);
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Parent WM_DESTROY received hwnd={} - parent window destroyed cleanly.",
                static_cast<void*>(hwnd));
            ::RemovePropW(hwnd, kEpochLayoutPendingProp);
            return 0;
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK MultiContextManager::ChildProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (route_context_close_shortcut(hwnd, msg, wParam, lParam)) return 0;
        static DragState& drag = Drag();
        const auto resolveGuiContext = [hwnd]() -> std::shared_ptr<epochengine::core::Context>
        {
            auto* mgr = s_activeInstance;
            if (!mgr)
                return {};

            if (auto* win = mgr->findWindowByHWND(hwnd))
                return typed_context(win->context);

            return {};
        };
        const auto resolveWindowData = [hwnd]() noexcept -> epochengine::core::WindowData*
        {
            return resolve_window_data_for_hwnd(hwnd);
        };

        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        switch (msg)
        {
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            cancel_native_context_drag(hwnd);
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_EPOCH_REFRESH_FPS_TITLE:
            apply_pending_native_title(hwnd, static_cast<std::uintptr_t>(wParam));
            return 0;
        case WM_CREATE:
            (void)::SetTimer(hwnd, kThemeRefreshTimer, kThemeRefreshIntervalMs, nullptr);
            apply_native_window_chrome(hwnd);
            return 0;
        case WM_ACTIVATE:
            apply_native_window_chrome(hwnd);
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_TIMER:
            if (wParam == kThemeRefreshTimer)
            {
                // Docked HWNDs may become top-level again on undocking.
                apply_native_window_chrome(hwnd);
                return 0;
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_THEMECHANGED:
        case WM_SETTINGCHANGE:
        case WM_SYSCOLORCHANGE:
            apply_native_window_chrome(hwnd);
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_EPOCH_OPEN_DETACHED_CONTEXT:
        {
            std::unique_ptr<DetachedContextWindowRequest> request{
                reinterpret_cast<DetachedContextWindowRequest*>(lParam)
            };
            if (request)
            {
                if (auto* mgr = s_activeInstance)
                    static_cast<void>(mgr->CreateDetachedContextWindowOnOwnerThread(*request));
            }
            return 0;
        }
        case WM_SETFOCUS:
            remember_gui_input_owner(hwnd);
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_KILLFOCUS:
            forget_gui_input_owner(hwnd, reinterpret_cast<HWND>(wParam));
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_NCDESTROY:
            forget_native_title_window(hwnd);
            forget_gui_input_owner(hwnd);
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_RBUTTONDOWN:
        {
            remember_gui_input_owner(hwnd);
            ::SetFocus(hwnd);
            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseDown, lParam, 0, false, 1);
            return 0;
        }
        case WM_RBUTTONUP:
        {
            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseUp, lParam, 0, false, 1);
            return 0;
        }
        case WM_NCLBUTTONDOWN:
        case WM_LBUTTONDOWN:
        {
            auto* const window = resolve_window_data_for_hwnd(hwnd);

            const bool detachedProxyHost =
                window
                && is_proxy_host_hwnd(window, hwnd)
                && is_sfml_proxy_detached(window);
            const bool detachedRoutedHost =
                window
                && window->hwnd == hwnd
                && (::GetParent(hwnd) == nullptr || window->isFloating);
            const bool detachedNativeTitleHost = detachedProxyHost || detachedRoutedHost;

            if (msg == WM_NCLBUTTONDOWN)
            {
                if (!detachedNativeTitleHost || static_cast<UINT>(wParam) != HTCAPTION)
                    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            }
            else
            {
                remember_gui_input_owner(hwnd);
                ::SetFocus(hwnd);
                const auto ctx = resolveGuiContext();
                if (!is_dock_drag_hotspot(hwnd, lParam))
                {
                    push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseDown, lParam);
                    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
                }
            }

            if (detachedProxyHost && window->hwndChild && ::IsWindow(window->hwndChild) != FALSE)
                ::SetFocus(window->hwndChild);
            else if (!detachedProxyHost)
                ::SetFocus(hwnd);

            ::SetCapture(hwnd);
            drag.dragging = true;
            drag.draggedWindow = hwnd;
            if (window && is_sfml_proxy_candidate(window) && window->host_hwnd)
            {
                drag.originalParent = ::GetParent(window->host_hwnd);
                if (!drag.originalParent)
                    drag.originalParent = stored_dock_parent(window->host_hwnd);
                if (!drag.originalParent && window->hwndChild)
                    drag.originalParent = stored_dock_parent(window->hwndChild);
            }
            else
            {
                drag.originalParent = ::GetParent(hwnd);
            }
            if (!drag.originalParent)
                drag.originalParent = stored_dock_parent(hwnd);
            drag.lastMousePos = screen_mouse_point(hwnd, msg, lParam);
            drag.dragStartMousePos = drag.lastMousePos;
            if (drag.originalParent && window)
                ::InvalidateRect(drag.originalParent, nullptr, FALSE);

            HWND dragFrame = hwnd;
            if (!detachedProxyHost && window && is_sfml_proxy_candidate(window))
            {
                if (is_proxy_child_directly_docked(window, drag.originalParent))
                {
                    const HWND child = window->hwndChild.load(std::memory_order_acquire);
                    dragFrame = child ? child : hwnd;
                }
                else if (window->host_hwnd)
                    dragFrame = window->host_hwnd;
            }

            RECT dragFrameRect{};
            ::GetWindowRect(dragFrame, &dragFrameRect);
            drag.dragWindowOffset.x = drag.lastMousePos.x - dragFrameRect.left;
            drag.dragWindowOffset.y = drag.lastMousePos.y - dragFrameRect.top;
            drag.proxyUndockPending = false;
            drag.proxyRedockPending = false;
            clear_routed_panel_tab_drop();
            if (window && window->isFloating)
                apply_native_drag_opacity(drag, dragFrame);
#if defined(_DEBUG)
            if (window && is_sfml_proxy_candidate(window))
            {
                epochengine::logger::get(kLogSys).logf(
                    epochengine::logger::LogLevel::INFO,
                    std::source_location::current(),
                    "Proxy drag start hwnd={} host={} child={} originalParent={} hostParent={} childParent={} dragFrame={}",
                    static_cast<void*>(hwnd),
                    static_cast<void*>(window->host_hwnd),
                    static_cast<void*>(window->hwndChild),
                    static_cast<void*>(drag.originalParent),
                    static_cast<void*>(window->host_hwnd ? ::GetParent(window->host_hwnd) : nullptr),
                    static_cast<void*>(window->hwndChild ? ::GetParent(window->hwndChild) : nullptr),
                    static_cast<void*>(dragFrame));
            }
#endif
            return 0;
        }

        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
        {
            auto* const window = resolve_window_data_for_hwnd(hwnd);
            const bool detachedProxyHostDrag =
                drag.dragging
                && window
                && is_proxy_host_hwnd(window, hwnd)
                && drag.draggedWindow == hwnd;
            const bool detachedRoutedHostDrag =
                drag.dragging
                && window
                && window->hwnd == hwnd
                && drag.draggedWindow == hwnd;
            if (msg == WM_NCMOUSEMOVE && !detachedProxyHostDrag && !detachedRoutedHostDrag)
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            if (msg == WM_MOUSEMOVE && !drag.dragging)
            {
                const auto ctx = resolveGuiContext();
                push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseMove, lParam);
            }
            const bool proxyChildContinuingHostDrag =
                drag.dragging
                && window
                && is_sfml_proxy_candidate(window)
                && hwnd == window->hwndChild
                && drag.draggedWindow == window->host_hwnd;
            if (!drag.dragging || (drag.draggedWindow != hwnd && !proxyChildContinuingHostDrag))
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            POINT pt = screen_mouse_point(hwnd, msg, lParam);
            if (proxyChildContinuingHostDrag || detachedProxyHostDrag || detachedRoutedHostDrag || is_sfml_proxy_detached(window))
            {
                POINT cursor{};
                if (::GetCursorPos(&cursor) != FALSE)
                    pt = cursor;
            }

            drag.lastMousePos = pt;
            if (drag.originalParent && window)
                ::InvalidateRect(drag.originalParent, nullptr, FALSE);
            const bool proxyDragActivated =
                window
                && is_sfml_proxy_candidate(window)
                && drag_distance_exceeded(drag.dragStartMousePos, pt);

            RECT wndRect{};
            const HWND dragFrame = proxy_drag_frame(window, drag.originalParent, hwnd);
            ::GetWindowRect(dragFrame, &wndRect);

            const int newX = pt.x - drag.dragWindowOffset.x;
            const int newY = pt.y - drag.dragWindowOffset.y;

            RECT clientRect{};
            const HWND sizeFrame = proxy_drag_frame(window, drag.originalParent, hwnd);
            ::GetClientRect(sizeFrame, &clientRect);
            const int clientW = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
            const int clientH = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));

            // A routed editor pane stays a distinct native window while held.
            // Admission to a tab group occurs only on the release transaction,
            // so moving across the host cannot prematurely merge the pane.
            if (drag.originalParent
                && window
                && !window->guiRoute.empty()
                && window->isFloating)
            {
                ::SetWindowPos(
                    hwnd,
                    nullptr,
                    newX,
                    newY,
                    0,
                    0,
                    SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                ::InvalidateRect(drag.originalParent, nullptr, FALSE);
                return 0;
            }

            if (drag.originalParent)
            {
                int wndW = clientW;
                int wndH = clientH;
                bool inside = false;
                if (window && is_sfml_proxy_candidate(window))
                {
                    inside = pointer_inside_parent_client(drag.originalParent, pt);
                    if (!is_sfml_proxy_detached(window))
                    {
                        inside = !proxyDragActivated
                            || pointer_inside_parent_dock_region(drag.originalParent, pt);
                    }
                    else
                    {
                        inside = pointer_inside_parent_dock_region(drag.originalParent, pt);
                    }
                }
                else
                {
                    RECT parentRect{};
                    ::GetClientRect(drag.originalParent, &parentRect);
                    POINT topLeft{ 0, 0 };
                    ::ClientToScreen(drag.originalParent, &topLeft);
                    ::OffsetRect(&parentRect, topLeft.x, topLeft.y);
                    const int currentW = clamp_positive(static_cast<int>(wndRect.right - wndRect.left));
                    const int currentH = clamp_positive(static_cast<int>(wndRect.bottom - wndRect.top));
                    inside =
                        newX >= parentRect.left && newY >= parentRect.top &&
                        (newX + currentW) <= parentRect.right && (newY + currentH) <= parentRect.bottom;
                }

                if (inside)
                {
                    if (is_sfml_proxy_detached(window))
                    {
                        drag.proxyUndockPending = false;
                        drag.proxyRedockPending = false;
                        post_proxy_host_command(
                            window,
                            ProxyDockCmd::MoveDetached,
                            drag.originalParent,
                            newX,
                            newY,
                            clientW,
                            clientH);
                        ::SetFocus(window->host_hwnd);
                    }
                    else if (window && is_sfml_proxy_candidate(window))
                    {
                        drag.proxyUndockPending = false;
                        drag.proxyRedockPending = false;
                        if (is_proxy_child_directly_docked(window, drag.originalParent))
                        {
                            dock_host_window_to_parent(
                                window->hwndChild,
                                drag.originalParent,
                                newX,
                                newY,
                                clientW,
                                clientH);
                            if (window->host_hwnd
                                && window->host_hwnd != window->hwndChild
                                && ::IsWindow(window->host_hwnd) != FALSE
                                && ::GetParent(window->host_hwnd) == drag.originalParent)
                            {
                                ::ShowWindow(window->host_hwnd, SW_HIDE);
                            }
                            ::SetFocus(window->hwndChild);
                        }
                        else
                        {
                            dock_host_window_to_parent(
                                window->host_hwnd,
                                drag.originalParent,
                                newX,
                                newY,
                                clientW,
                                clientH);
                            apply_child_fill_layout(window->hwndChild, window->host_hwnd, clientW, clientH);
                            ::SetFocus(hwnd);
                        }
                    }
                    else if (window && backend_requires_owner_thread_dock_commands(window))
                    {
                        drag.proxyUndockPending = false;
                        drag.proxyRedockPending = false;
                        if (::GetParent(hwnd) != drag.originalParent || window->isFloating)
                        {
                            window->isFloating = false;
                            static_cast<void>(post_owner_thread_dock_command(
                                window,
                                ProxyDockCmd::Redock,
                                drag.originalParent,
                                newX,
                                newY,
                                wndW,
                                wndH));
                            if (auto* mgr = s_activeInstance)
                                mgr->HandleResize(hwnd, wndW, wndH);
                        }
                    }
                    else if (::GetParent(hwnd) != drag.originalParent)
                    {
                        dock_host_window_to_parent(
                            hwnd,
                            drag.originalParent,
                            newX,
                            newY,
                            wndW,
                            wndH);
                        restore_associated_host_window(
                            window,
                            drag.originalParent,
                            newX,
                            newY,
                            wndW,
                            wndH);
                        if (window)
                            window->isFloating = false;
                        if (auto* mgr = s_activeInstance)
                            mgr->HandleResize(hwnd, wndW, wndH);
                    }
                    else
                    {
                        dock_host_window_to_parent(
                            hwnd,
                            drag.originalParent,
                            newX,
                            newY,
                            wndW,
                            wndH);
                        restore_associated_host_window(
                            window,
                            drag.originalParent,
                            newX,
                            newY,
                            wndW,
                            wndH);
                        if (window)
                            window->isFloating = false;
                    }

                    hide_associated_host_window(
                        resolve_window_data_for_hwnd(hwnd),
                        hwnd,
                        drag.originalParent);
                    ::SetFocus(hwnd);
                }
                else
                {
                    if (window && is_sfml_proxy_candidate(window))
                    {
                        if (!is_sfml_proxy_detached(window))
                        {
                            if (::IsZoomed(drag.originalParent) != FALSE)
                                ::ShowWindow(drag.originalParent, SW_RESTORE);
                            drag.proxyRedockPending = false;
                            const POINT escaped = force_proxy_shell_outside_parent(
                                drag.originalParent,
                                pt,
                                newX,
                                newY,
                                clientW,
                                clientH);
#if defined(_DEBUG)
                            epochengine::logger::get(kLogSys).logf(
                                epochengine::logger::LogLevel::INFO,
                                std::source_location::current(),
                                "Proxy undock move host={} child={} pt=({}, {}) new=({}, {}) escaped=({}, {}) parent={} hostParentBefore={} childParentBefore={}",
                                static_cast<void*>(window->host_hwnd),
                                static_cast<void*>(window->hwndChild),
                                pt.x,
                                pt.y,
                                newX,
                                newY,
                                escaped.x,
                                escaped.y,
                                static_cast<void*>(drag.originalParent),
                                static_cast<void*>(window->host_hwnd ? ::GetParent(window->host_hwnd) : nullptr),
                                static_cast<void*>(window->hwndChild ? ::GetParent(window->hwndChild) : nullptr));
#endif
                            if (!drag.proxyUndockPending)
                            {
                                post_proxy_host_command(
                                    window,
                                    ProxyDockCmd::Undock,
                                    drag.originalParent,
                                    escaped.x,
                                    escaped.y,
                                    clientW,
                                    clientH);
                                drag.proxyUndockPending = true;
                            }
                        }
                        else
                        {
                            drag.proxyUndockPending = false;
                            post_proxy_host_command(
                                window,
                                ProxyDockCmd::MoveDetached,
                                drag.originalParent,
                                newX,
                                newY,
                                clientW,
                                clientH);
                            ::SetFocus(window->host_hwnd);
                        }
                    }
                    else if (window
                        && backend_requires_owner_thread_dock_commands(window)
                        && (::GetParent(hwnd) == drag.originalParent || !window->isFloating))
                    {
                        if (::IsZoomed(drag.originalParent) != FALSE)
                            ::ShowWindow(drag.originalParent, SW_RESTORE);
                        const POINT escaped = force_proxy_shell_outside_parent(
                            drag.originalParent,
                            pt,
                            newX,
                            newY,
                            clientW,
                            clientH);
                        if (!drag.proxyUndockPending)
                        {
                            static_cast<void>(post_owner_thread_dock_command(
                                window,
                                ProxyDockCmd::Undock,
                                drag.originalParent,
                                escaped.x,
                                escaped.y,
                                clientW,
                                clientH));
                            drag.proxyUndockPending = true;
                        }
                    }
                    else if (::GetParent(hwnd) == drag.originalParent)
                    {
                        if (::IsZoomed(drag.originalParent) != FALSE)
                            ::ShowWindow(drag.originalParent, SW_RESTORE);
                        const POINT escaped = force_proxy_shell_outside_parent(
                            drag.originalParent,
                            pt,
                            newX,
                            newY,
                            clientW,
                            clientH);
                        LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
                        style &= ~WS_CHILD;
                        style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE;
                        ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
                        ::SetParent(hwnd, nullptr);
                        ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, 0);
                        RECT adjusted{ 0, 0, clientW, clientH };
                        const DWORD exStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
                        if (::AdjustWindowRectEx(&adjusted, static_cast<DWORD>(style), FALSE, exStyle))
                        {
                            wndW = clamp_positive(adjusted.right - adjusted.left);
                            wndH = clamp_positive(adjusted.bottom - adjusted.top);
                        }
                        ::SetWindowPos(hwnd, HWND_TOPMOST, escaped.x, escaped.y, wndW, wndH,
                            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                        ::SetWindowPos(hwnd, HWND_NOTOPMOST, escaped.x, escaped.y, wndW, wndH,
                            SWP_SHOWWINDOW);
                        apply_native_window_chrome(hwnd);
                        if (window)
                            window->isFloating = true;
                        ::BringWindowToTop(hwnd);
                        ::SetActiveWindow(hwnd);
                        ::SetForegroundWindow(hwnd);
                        ::SetFocus(hwnd);
                    }
                    else if (window && backend_requires_owner_thread_dock_commands(window))
                    {
                        static_cast<void>(post_owner_thread_dock_command(
                            window,
                            ProxyDockCmd::MoveDetached,
                            drag.originalParent,
                            newX,
                            newY,
                            clientW,
                            clientH));
                    }
                    else
                    {
                        if (window)
                            window->isFloating = true;
                        ::SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                    }
                }
            }
            else
            {
                if (window && backend_requires_owner_thread_dock_commands(window))
                {
                    static_cast<void>(post_owner_thread_dock_command(
                        window,
                        ProxyDockCmd::MoveDetached,
                        nullptr,
                        newX,
                        newY,
                        clientW,
                        clientH));
                }
                else
                {
                    if (window)
                        window->isFloating = true;
                    ::SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }

            return 0;
        }

        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED) return 0;

            auto* mgr = s_activeInstance;
            if (!mgr) break;

            const int width = clamp_positive(static_cast<int>(LOWORD(lParam)));
            const int height = clamp_positive(static_cast<int>(HIWORD(lParam)));
            mgr->HandleResize(hwnd, width, height);
            return 0;
        }

        case WM_EPOCH_PROXY_DOCKCMD:
        {
            std::unique_ptr<ProxyDockRequest> request{
                reinterpret_cast<ProxyDockRequest*>(lParam)
            };
            auto* const window = resolve_window_data_for_hwnd(
                request && request->sourceHwnd ? request->sourceHwnd : hwnd);
            if (!request || !is_sfml_proxy_candidate(window))
                return 0;
            const auto command = static_cast<ProxyDockCmd>(wParam);

            switch (command)
            {
            case ProxyDockCmd::Undock:
                undock_sfml_proxy_window(
                    window,
                    request->x,
                    request->y,
                    request->width,
                    request->height);
#if defined(_DEBUG)
                if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
                {
                    RECT hostRect{};
                    ::GetWindowRect(window->host_hwnd, &hostRect);
                    epochengine::logger::get(kLogSys).logf(
                        epochengine::logger::LogLevel::INFO,
                        std::source_location::current(),
                        "Proxy undock applied host={} child={} hostParentAfter={} childParentAfter={} hostRect=({}, {}, {}, {})",
                        static_cast<void*>(window->host_hwnd),
                        static_cast<void*>(window->hwndChild),
                        static_cast<void*>(::GetParent(window->host_hwnd)),
                        static_cast<void*>(::GetParent(window->hwndChild)),
                        hostRect.left,
                        hostRect.top,
                        hostRect.right,
                        hostRect.bottom);
                }
#endif
                {
                    auto& dragState = epochengine::core::Drag();
                    if (dragState.dragging
                        && window->host_hwnd
                        && (dragState.draggedWindow == window->hwndChild
                            || dragState.draggedWindow == window->host_hwnd)
                        && ::IsWindow(window->host_hwnd) != FALSE)
                    {
                        ::ReleaseCapture();
                        ::SetCapture(window->host_hwnd);
                        dragState.draggedWindow = window->host_hwnd;
                        sync_drag_offset_to_host_window(window->host_hwnd);
                        ::SetActiveWindow(window->host_hwnd);
                        ::SetForegroundWindow(window->host_hwnd);
                        const HWND child = window->hwndChild.load(std::memory_order_acquire);
                        ::SetFocus(
                            (child && ::IsWindow(child) != FALSE)
                            ? child
                            : window->host_hwnd);
                    }
                }
                if (request->parentHwnd && ::IsWindow(request->parentHwnd) != FALSE)
                    request_parent_layout(request->parentHwnd);
                return 0;

            case ProxyDockCmd::MoveDetached:
                move_detached_top_level_shell(
                    window->host_hwnd,
                    request->x,
                    request->y,
                    request->width,
                    request->height);
                window->set_size(request->width, request->height);
#if defined(_DEBUG)
                if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
                {
                    RECT hostRect{};
                    ::GetWindowRect(window->host_hwnd, &hostRect);
                    epochengine::logger::get(kLogSys).logf(
                        epochengine::logger::LogLevel::INFO,
                        std::source_location::current(),
                        "Proxy move applied host={} child={} hostParent={} childParent={} hostRect=({}, {}, {}, {})",
                        static_cast<void*>(window->host_hwnd),
                        static_cast<void*>(window->hwndChild),
                        static_cast<void*>(::GetParent(window->host_hwnd)),
                        static_cast<void*>(::GetParent(window->hwndChild)),
                        hostRect.left,
                        hostRect.top,
                        hostRect.right,
                        hostRect.bottom);
                }
#endif
                return 0;

            case ProxyDockCmd::Redock:
                redock_sfml_proxy_window(
                    window,
                    request->parentHwnd,
                    request->x,
                    request->y,
                    request->width,
                    request->height);
                // Parent layout either requested this redock or follows the drag release.
                // Re-requesting it here can loop SDL/SFML proxy placement.
                return 0;
            }

            return 0;
        }

        case WM_NCLBUTTONUP:
        case WM_LBUTTONUP:
        {
            auto* const window = resolve_window_data_for_hwnd(hwnd);
            const bool detachedProxyHostDrag =
                drag.dragging
                && window
                && is_proxy_host_hwnd(window, hwnd)
                && drag.draggedWindow == hwnd;
            const bool detachedRoutedHostDrag =
                drag.dragging
                && window
                && window->hwnd == hwnd
                && drag.draggedWindow == hwnd;
            if (msg == WM_NCLBUTTONUP && !detachedProxyHostDrag && !detachedRoutedHostDrag)
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            if (msg == WM_LBUTTONUP && !drag.dragging)
            {
                const auto ctx = resolveGuiContext();
                push_gui_mouse_event(ctx.get(), hwnd, epochengine::gui::EventType::MouseUp, lParam);
            }
            const bool proxyChildContinuingHostDrag =
                drag.dragging
                && window
                && is_sfml_proxy_candidate(window)
                && hwnd == window->hwndChild
                && drag.draggedWindow == window->host_hwnd;
            if (drag.dragging && (drag.draggedWindow == hwnd || proxyChildContinuingHostDrag))
            {
                const HWND originalParent = drag.originalParent;
                POINT releasePoint = screen_mouse_point(hwnd, msg, lParam);
                if (proxyChildContinuingHostDrag || detachedProxyHostDrag || detachedRoutedHostDrag || is_sfml_proxy_detached(window))
                {
                    POINT cursor{};
                    if (::GetCursorPos(&cursor) != FALSE)
                        releasePoint = cursor;
                }
                const bool proxyDragActivated =
                    window
                    && is_sfml_proxy_candidate(window)
                    && drag_distance_exceeded(drag.dragStartMousePos, releasePoint);
                RECT wndRect{};
                const HWND releaseFrame = proxy_drag_frame(window, originalParent, hwnd);
                ::GetWindowRect(releaseFrame, &wndRect);
                restore_native_drag_opacity(drag);
                const POINT dragWindowOffset = drag.dragWindowOffset;
                drag.dragging = false;
                drag.draggedWindow = nullptr;
                drag.originalParent = nullptr;
                drag.dragStartMousePos = POINT{};
                drag.proxyUndockPending = false;
                drag.proxyRedockPending = false;
                ::ReleaseCapture();
                if (originalParent && ::IsWindow(originalParent) != FALSE)
                    ::InvalidateRect(originalParent, nullptr, FALSE);

                if (originalParent
                    && ::IsWindow(originalParent) != FALSE)
                {
                    RECT clientRect{};
                    const HWND sizeFrame = proxy_drag_frame(window, originalParent, hwnd);
                    ::GetClientRect(sizeFrame, &clientRect);
                    const int clientW = clamp_positive(static_cast<int>(clientRect.right - clientRect.left));
                    const int clientH = clamp_positive(static_cast<int>(clientRect.bottom - clientRect.top));
                    const RECT parentRect = screen_client_rect(originalParent);
                    const bool releaseInsideParent = point_in_rect(parentRect, releasePoint);
                    const POINT windowCenter{
                        wndRect.left + ((wndRect.right - wndRect.left) / 2),
                        wndRect.top + ((wndRect.bottom - wndRect.top) / 2)
                    };
                    const bool centerInsideParent = point_in_rect(parentRect, windowCenter);
                    const bool releaseOutsideParent = !should_redock_to_parent(originalParent, releasePoint, wndRect);
                    [[maybe_unused]] const bool detachedProxy = window && is_sfml_proxy_detached(window);
                    RoutedPanelDockTarget dockTarget =
                        routed_panel_target_for_parent(
                            originalParent,
                            releasePoint,
                            window && !window->guiRoute.empty());
                    std::uint32_t tabInsertionIndex = 0xffffffffu;
                    if (window && !window->guiRoute.empty())
                    {
                        RoutedPanelDockTarget directTarget{
                            RoutedPanelDockTarget::none};
                        if (routed_panel_tab_drop_for(
                                window->guiRoute,
                                directTarget,
                                tabInsertionIndex))
                        {
                            dockTarget = directTarget;
                        }
                    }
                    const bool hasDockTarget = dockTarget != RoutedPanelDockTarget::none;
                    const bool routedPane = window && !window->guiRoute.empty();
                    const bool wantsRedock = routedPane
                        ? (hasDockTarget && releaseInsideParent)
                        : releaseInsideParent;

#if defined(_DEBUG)
                    if (window && is_sfml_proxy_candidate(window))
                    {
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Proxy release host={} child={} detached={} release=({}, {}) parentRect=({}, {}, {}, {}) wndRect=({}, {}, {}, {}) releaseInside={} centerInside={} detachedProxy={} wantsRedock={}",
                            static_cast<void*>(window->host_hwnd),
                            static_cast<void*>(window->hwndChild),
                            is_sfml_proxy_detached(window),
                            releasePoint.x,
                            releasePoint.y,
                            parentRect.left,
                            parentRect.top,
                            parentRect.right,
                            parentRect.bottom,
                            wndRect.left,
                            wndRect.top,
                            wndRect.right,
                            wndRect.bottom,
                            releaseInsideParent,
                            centerInsideParent,
                            detachedProxy,
                            wantsRedock);
                    }
#endif

                    if (wantsRedock)
                    {
                        const bool restoreRoutedPaneAsTab = window
                            && !window->guiRoute.empty()
                            && dockTarget != RoutedPanelDockTarget::left_context
                            && dockTarget != RoutedPanelDockTarget::right_context;
                        if (restoreRoutedPaneAsTab)
                        {
                            window->routedDockTarget.store(
                                static_cast<std::uint8_t>(dockTarget),
                                std::memory_order_release);
                            window->routedDockTabInsertion.store(
                                tabInsertionIndex,
                                std::memory_order_release);
                            request_routed_panel_redock_close(window);
                        }
                        else if (window && is_sfml_proxy_detached(window))
                        {
                            post_proxy_host_command(
                                window,
                                ProxyDockCmd::Redock,
                                originalParent,
                                wndRect.left,
                                wndRect.top,
                                clientW,
                                clientH);
                        }
                        else if (window && backend_requires_owner_thread_dock_commands(window))
                        {
                            window->isFloating = false;
                            static_cast<void>(post_owner_thread_dock_command(
                                window,
                                ProxyDockCmd::Redock,
                                originalParent,
                                wndRect.left,
                                wndRect.top,
                                clientW,
                                clientH));
                        }
                        else
                        {
                            dock_host_window_to_parent(
                                hwnd,
                                originalParent,
                                wndRect.left,
                                wndRect.top,
                                clientW,
                                clientH);
                            restore_associated_host_window(
                                window,
                                originalParent,
                                wndRect.left,
                                wndRect.top,
                                clientW,
                                clientH);
                            if (window)
                                window->isFloating = false;
                        }
                        if (!restoreRoutedPaneAsTab)
                        {
                            if (auto* mgr = s_activeInstance)
                                mgr->HandleResize(hwnd, clientW, clientH);
                            if (window)
                                window->dockTargetPreference.store(
                                    static_cast<std::uint8_t>(dockTarget),
                                    std::memory_order_release);
                        }
                    }
                    else if (window
                        && is_sfml_proxy_candidate(window)
                        && !is_sfml_proxy_detached(window)
                        && proxyDragActivated
                        && releaseOutsideParent)
                    {
                        if (::IsZoomed(originalParent) != FALSE)
                            ::ShowWindow(originalParent, SW_RESTORE);
                        const POINT escaped = force_proxy_shell_outside_parent(
                            originalParent,
                            releasePoint,
                            releasePoint.x - dragWindowOffset.x,
                            releasePoint.y - dragWindowOffset.y,
                            clientW,
                            clientH);
#if defined(_DEBUG)
                        epochengine::logger::get(kLogSys).logf(
                            epochengine::logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Proxy undock release host={} child={} release=({}, {}) escaped=({}, {}) parent={} hostParentBefore={} childParentBefore={}",
                            static_cast<void*>(window->host_hwnd),
                            static_cast<void*>(window->hwndChild),
                            releasePoint.x,
                            releasePoint.y,
                            escaped.x,
                            escaped.y,
                            static_cast<void*>(originalParent),
                            static_cast<void*>(window->host_hwnd ? ::GetParent(window->host_hwnd) : nullptr),
                            static_cast<void*>(window->hwndChild ? ::GetParent(window->hwndChild) : nullptr));
#endif
                        post_proxy_host_command(
                            window,
                            ProxyDockCmd::Undock,
                            originalParent,
                            escaped.x,
                            escaped.y,
                            clientW,
                            clientH);
                        request_parent_layout(originalParent);
                    }
                }

                if (originalParent && window && window->guiRoute.empty()
                    && pointer_inside_parent_client(originalParent, releasePoint))
                    if (auto* manager = s_activeInstance)
                        manager->ReorderDockedWindow(hwnd, releasePoint);
                drag.dragWindowOffset = POINT{};
                clear_routed_panel_tab_drop();

                if (!(window && is_sfml_proxy_candidate(window)))
                {
                    hide_associated_host_window(
                        resolve_window_data_for_hwnd(hwnd),
                        hwnd,
                        originalParent);
                }
                HWND focusTarget = hwnd;
                if (window
                    && is_sfml_proxy_candidate(window)
                    && window->hwndChild
                    && ::IsWindow(window->hwndChild) != FALSE)
                {
                    if (is_sfml_proxy_detached(window)
                        && window->host_hwnd
                        && ::IsWindow(window->host_hwnd) != FALSE)
                    {
                        ::BringWindowToTop(window->host_hwnd);
                        ::SetActiveWindow(window->host_hwnd);
                        ::SetForegroundWindow(window->host_hwnd);
                    }
                    focusTarget = window->hwndChild;
                }
                ::SetFocus(focusTarget);

                if (originalParent && ::IsWindow(originalParent) != FALSE)
                    ::PostMessageW(originalParent, WM_SIZE, 0, 0);
                return 0;
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSEWHEEL:
        {
            if (is_proxy_host_hwnd(resolveWindowData(), hwnd))
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            const auto ctx = resolveGuiContext();
            push_gui_mouse_event(
                ctx.get(),
                hwnd,
                epochengine::gui::EventType::MouseWheel,
                lParam,
                GET_WHEEL_DELTA_WPARAM(wParam),
                true);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            if (is_proxy_host_hwnd(resolveWindowData(), hwnd))
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            if (!accepts_gui_keyboard_input(hwnd))
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            const auto ctx = resolveGuiContext();
            push_gui_key_event(ctx.get(), static_cast<int>(wParam));
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_CHAR:
        case WM_SYSCHAR:
        {
            if (is_proxy_host_hwnd(resolveWindowData(), hwnd))
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);

            if (!accepts_gui_keyboard_input(hwnd))
                return 0;

            const auto ctx = resolveGuiContext();
            if (wParam >= 0x20u || wParam == 13u)
                push_gui_text_event(ctx.get(), static_cast<char32_t>(wParam));
            return 0;
        }

        case WM_DROPFILES:
        {
            // Do not forward the same owning HDROP to the parent: that used
            // to free it once in ParentProc and again here.
            platform_file_drop::OwnedDrop drop{reinterpret_cast<HDROP>(wParam)};
            try
            {
                const auto ctx = resolveGuiContext();
                POINT point{};
                if (!ctx || !::DragQueryPoint(drop.get(), &point)) return 0;
                int x = point.x, y = point.y;
                if (ctx->normalize_mouse_position) ctx->normalize_mouse_position(x, y);
                auto files = drop.files();
                if (!files.empty())
                    gui::push_input_for_context(ctx.get(), gui::InputEvent{
                        .type = gui::EventType::FileDrop,
                        .mouse_pos = {static_cast<float>(x), static_cast<float>(y)},
                        .files = std::move(files)});
            }
            catch (...) { /* Never unwind a native window procedure. */ }
            return 0;
        }

        case WM_ERASEBKGND:
            if (auto* const window = resolveWindowData();
                should_draw_opengl_startup_placeholder(window))
            {
                RECT clientRect{};
                if (::GetClientRect(hwnd, &clientRect))
                {
                    ::FillRect(reinterpret_cast<HDC>(wParam), &clientRect, parent_background_brush());
                }
            }
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = ::BeginPaint(hwnd, &ps);
            if (auto* const window = resolveWindowData();
                should_draw_opengl_startup_placeholder(window))
            {
                ::FillRect(hdc, &ps.rcPaint, parent_background_brush());
            }
            ::EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Child WM_CLOSE received hwnd={} parent={}",
                static_cast<void*>(hwnd),
                static_cast<void*>(::GetParent(hwnd)));
            if (auto* mgr = s_activeInstance;
                mgr && mgr->findWindowByHWND(hwnd))
            {
                // Keep the native surface alive until the renderer has stopped
                // and released its swapchain/context. CleanupFinishedWindows
                // performs the final HWND destruction after joining the thread.
                mgr->RemoveWindow(hwnd);
            }
            else
            {
                ::DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            forget_native_title_window(hwnd);
            (void)::KillTimer(hwnd, kThemeRefreshTimer);
            (void)::RemovePropW(hwnd, kWindowThemeProperty);
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Child WM_DESTROY received hwnd={} parentAfter={}",
                static_cast<void*>(hwnd),
                static_cast<void*>(::GetParent(hwnd)));
            if (auto* mgr = s_activeInstance)
            {
                mgr->RemoveWindow(hwnd);
            }
            return 0;

        default:
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        return 0;
    }