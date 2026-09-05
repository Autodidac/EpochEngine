// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Explicit native, non-renderer contract. Never part of automatic CTest.
#include "core.stl_types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

#if !defined(_WIN32)
#error This contract requires the native Win32 window adapter.
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

import core.error;
import core.log;
import platform.window;

namespace
{
    namespace platform = epochengine::platform;
    unsigned failures{};

    void check(bool passed, std::string_view label)
    {
        if (!passed) ++failures;
        epochengine::core::log::write(passed
            ? epochengine::core::log::level::info : epochengine::core::log::level::error,
            "Epoch.NativeWindowContract", { label.data(), label.size() });
    }

    HWND native(platform::WindowHandle handle) noexcept
    {
        return reinterpret_cast<HWND>(handle.value);
    }

    bool title_is(HWND hwnd, std::wstring_view expected) noexcept
    {
        std::array<wchar_t, 256> title{};
        const int length = ::GetWindowTextW(hwnd, title.data(), static_cast<int>(title.size()));
        return length >= 0 && std::wstring_view{ title.data(), static_cast<std::size_t>(length) } == expected
            && ::GetWindowTextLengthW(hwnd) == length;
    }

    struct Events
    {
        platform::WindowHandle expected{};
        unsigned close_count{};
        unsigned resize_count{};
        std::int32_t width{};
        std::int32_t height{};

        void consume(const platform::WindowEvent& event) noexcept
        {
            if (event.handle.value != expected.value) return;
            if (event.type == platform::WindowEventType::close) ++close_count;
            if (event.type == platform::WindowEventType::resized)
            {
                ++resize_count;
                width = event.width;
                height = event.height;
            }
        }
    };
}

int main(int argc, char** argv)
{
    if (argc != 2 || std::string_view{ argv[1] } != "--native-window-contract")
    {
        epochengine::core::log::error("Epoch.NativeWindowContract",
            "No windows created. Explicit --native-window-contract is required.");
        return 2;
    }

    auto created_system = platform::create_window_system();
    check(created_system.has_value() && *created_system != nullptr, "Native Win32 factory created.");
    if (!created_system || !*created_system) return 1;
    auto system = std::move(*created_system);

    platform::WindowDesc desc{};
    desc.visible = false;
    desc.title = "Epoch Software";
    auto created = system->create_window(desc);
    check(created.has_value() && created->valid(), "Native hidden window created.");
    if (!created || !created->valid()) return 1;
    const auto handle = *created;
    const HWND hwnd = native(handle);
    check(::IsWindow(hwnd) != FALSE && ::IsWindowVisible(hwnd) == FALSE,
        "Window is real and hidden; no renderer or visible preview is exercised.");
    check(title_is(hwnd, L"Epoch Software"), "Initial caption survives WM_NCCREATE default processing.");

    system->set_title(handle, "Epoch Renamed \xCE\xA9 Window");
    check(title_is(hwnd, L"Epoch Renamed \u03A9 Window"), "UTF-8 title changes round-trip through native window text.");

    Events events{ .expected = handle };
    const auto observe = [&](const platform::WindowEvent& event) noexcept { events.consume(event); };
    system->pump_events(observe);
    events.resize_count = 0;
    const bool positioned = ::SetWindowPos(hwnd, nullptr, 0, 0, 640, 420,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    system->pump_events(observe);
    RECT client{};
    const bool measured = ::GetClientRect(hwnd, &client) != FALSE;
    check(positioned && measured && events.resize_count != 0
        && events.width == client.right - client.left && events.height == client.bottom - client.top
        && events.width > 0 && events.height > 0,
        "Native resize event reports the actual client size, not requested outer dimensions.");

    const bool queued_first_resize = ::SetWindowPos(hwnd, nullptr, 0, 0, 720, 480,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    RECT first_client{};
    const bool measured_first = ::GetClientRect(hwnd, &first_client) != FALSE;
    Events callback_events{ .expected = handle };
    unsigned callback_resizes{};
    bool callback_resize_succeeded = false;
    const auto resize_once = [&](const platform::WindowEvent& event) noexcept
    {
        callback_events.consume(event);
        if (event.handle.value == handle.value && event.type == platform::WindowEventType::resized
            && callback_resizes == 0)
        {
            ++callback_resizes;
            callback_resize_succeeded = ::SetWindowPos(hwnd, nullptr, 0, 0, 800, 520,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
        }
    };
    system->pump_events(resize_once);
    RECT second_client{};
    const bool measured_second = ::GetClientRect(hwnd, &second_client) != FALSE;
    check(queued_first_resize && measured_first && callback_resize_succeeded && measured_second
        && callback_resizes == 1 && callback_events.resize_count == 1
        && callback_events.width == first_client.right - first_client.left
        && callback_events.height == first_client.bottom - first_client.top,
        "Synchronous resize inside a callback cannot extend or invalidate the current notification batch.");
    system->pump_events(resize_once);
    check(callback_resizes == 1 && callback_events.resize_count == 2
        && callback_events.width == second_client.right - second_client.left
        && callback_events.height == second_client.bottom - second_client.top,
        "The next pump delivers the callback-generated resize exactly once with its new client dimensions.");
    system->pump_events(resize_once);
    check(callback_resizes == 1 && callback_events.resize_count == 2,
        "A third pump does not duplicate either resize notification.");

    system->request_close(handle);
    system->pump_events(observe);
    check(events.close_count == 1 && ::IsWindow(hwnd) != FALSE,
        "Close is reported once and remains owner-controlled until destruction.");
    system->destroy_window(handle);
    check(::IsWindow(hwnd) == FALSE && !system->primary_window().valid(),
        "Explicit owner release destroys the native handle and clears primary window.");

    auto cancelled = system->create_window(desc);
    check(cancelled.has_value() && cancelled->valid(), "Window created for callback destruction regression.");
    if (!cancelled || !cancelled->valid()) return 1;
    const HWND cancelled_hwnd = native(*cancelled);
    const auto ignore = [](const platform::WindowEvent&) noexcept {};
    system->pump_events(ignore);
    const bool cancellation_first_resize = ::SetWindowPos(cancelled_hwnd, nullptr, 0, 0, 620, 400,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    const bool cancellation_second_resize = ::SetWindowPos(cancelled_hwnd, nullptr, 0, 0, 640, 420,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    unsigned callbacks_before_release{};
    bool cancellation_nested_resize = false;
    const auto destroy_during_callback = [&](const platform::WindowEvent& event) noexcept
    {
        if (event.handle.value != cancelled->value || event.type != platform::WindowEventType::resized) return;
        if (++callbacks_before_release != 1) return;
        // Queue another synchronous notification, then cancel this window.
        cancellation_nested_resize = ::SetWindowPos(cancelled_hwnd, nullptr, 0, 0, 660, 440,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
        system->destroy_window(*cancelled);
    };
    system->pump_events(destroy_during_callback);
    system->pump_events(destroy_during_callback);
    check(cancellation_first_resize && cancellation_second_resize && cancellation_nested_resize
        && callbacks_before_release == 1 && ::IsWindow(cancelled_hwnd) == FALSE,
        "Destruction during delivery suppresses pending and newly queued notifications for the retired window.");

    // More than one outstanding window covers destructor reentrancy: each
    // DestroyWindow synchronously removes its own entry through WM_DESTROY.
    platform::WindowDesc default_desc{};
    default_desc.visible = false;
    auto first = system->create_window(default_desc);
    check(first.has_value() && first->valid(), "Native factory can create again after explicit release.");
    if (!first || !first->valid()) return 1;
    check(title_is(native(*first), L"Epoch"), "Default native title is not blank.");
    default_desc.title = "Epoch Created \xCE\xA9 Window";
    auto second = system->create_window(default_desc);
    check(second.has_value() && second->valid(), "Second owned native window created for cleanup regression.");
    if (!second || !second->valid()) return 1;
    check(title_is(native(*second), L"Epoch Created \u03A9 Window"), "UTF-8 initial caption survives default creation.");
    const HWND first_hwnd = native(*first);
    const HWND second_hwnd = native(*second);
    system.reset();
    check(::IsWindow(first_hwnd) == FALSE && ::IsWindow(second_hwnd) == FALSE,
        "Window-system destruction retires every owned handle without invalidating its traversal.");

    auto recreated_system = platform::create_window_system();
    check(recreated_system.has_value() && *recreated_system != nullptr,
        "Native window system can be reconstructed after complete teardown.");
    if (!recreated_system || !*recreated_system) return 1;
    auto final_window = (*recreated_system)->create_window(default_desc);
    check(final_window.has_value() && final_window->valid(), "Window class is usable after teardown and registration.");
    if (!final_window || !final_window->valid()) return 1;
    const HWND final_hwnd = native(*final_window);
    (*recreated_system)->destroy_window(*final_window);
    recreated_system->reset();
    check(::IsWindow(final_hwnd) == FALSE, "Final native handle is retired.");

    check(failures == 0,
        "native_window.contract=complete; real hidden title/resize/close/lifecycle only, no renderer/pixel proof.");
    return failures == 0 ? 0 : 1;
}
