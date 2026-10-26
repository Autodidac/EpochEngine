// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Real two-client X11 queue; production forwarding/routing bodies are inserted.
#include <X11/Xlib.h>
#undef None
#include <atomic>
#include <chrono>
#include <bitset>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

using HWND = void*;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)
namespace epochengine::input {
enum MouseButton { MouseLeft, MouseMiddle, MouseRight, MouseButton4, MouseButton5, MouseCount };
std::shared_mutex g_inputMutex;
std::bitset<8> keyDown;
std::bitset<MouseCount> mouseDown, mousePressed;
struct AtomicInt {
    int value{};
    void store(int next, std::memory_order) { value = next; }
    void fetch_add(int delta, std::memory_order) { value += delta; }
};
AtomicInt mouseX, mouseY, mouseWheel;
void set_mouse_coords_are_global(bool) {}
}
namespace epochengine::core {
enum class ContextType { SDL, SFML, OpenGL };
struct WindowData { ContextType type{}; };
struct MultiContextManager {
    std::unordered_map<HWND, WindowData> owned;
    WindowData* findWindowByHWND(HWND window) {
        const auto found = owned.find(window);
        return found == owned.end() ? nullptr : &found->second;
    }
};
MultiContextManager manager;
Display* global_display{};
MultiContextManager* GetActiveMultiContextManager() { return &manager; }
long manager_event_mask(ContextType type) {
    XSetWindowAttributes swa{};
    MANAGER_MASK
    return swa.event_mask;
}
std::optional<epochengine::input::MouseButton> map_mouse_button(unsigned button) {
    using namespace epochengine::input;
    switch (button) {
    case 1: return MouseLeft; case 2: return MouseMiddle; case 3: return MouseRight;
    case 8: return MouseButton4; case 9: return MouseButton5; default: return {};
    }
}
FORWARD_POINTER
}

void route(Display* display, const XEvent& event) {
    switch (event.type) {
    case ClientMessage: {
        ROUTE_POINTER
        break;
    }
    case FocusOut: {
        FOCUS_LOSS
        break;
    }
    default: break;
    }
}

void drain(Display* display) {
    XSync(display, False);
    while (XPending(display)) {
        XEvent event{};
        XNextEvent(display, &event);
        route(display, event);
    }
}

int main() {
    CHECK(XInitThreads());
    Display* managerDisplay = XOpenDisplay(nullptr);
    Display* sdkDisplay = XOpenDisplay(nullptr);
    CHECK(managerDisplay && sdkDisplay);
    epochengine::core::global_display = managerDisplay;
    const auto root = DefaultRootWindow(managerDisplay);
    for (const auto type : {epochengine::core::ContextType::SDL,
            epochengine::core::ContextType::SFML}) {
        const auto window = XCreateSimpleWindow(managerDisplay, root,
            0, 0, 100, 100, 0, 0, 0);
        const auto handle = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(window));
        epochengine::core::manager.owned.emplace(handle, epochengine::core::WindowData{type});
        XSelectInput(managerDisplay, window, epochengine::core::manager_event_mask(type));
        XMapWindow(managerDisplay, window);
        XSync(managerDisplay, False);
        bool viewable = false;
        for (int attempt = 0; attempt < 200; ++attempt) {
            XWindowAttributes attributes{};
            if (XGetWindowAttributes(managerDisplay, window, &attributes)
                && attributes.map_state == IsViewable) {
                viewable = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CHECK(viewable);
        // This would terminate with BadAccess if the host retained ButtonPressMask.
        XSelectInput(sdkDisplay, window, ButtonPressMask | ButtonReleaseMask);
        XSync(sdkDisplay, False);
        XSetInputFocus(managerDisplay, window, RevertToParent, CurrentTime);
        drain(managerDisplay);
        using namespace epochengine::input;
        mouseDown.reset(); mousePressed.reset(); mouseWheel.value = 0;
        // The real SDK client gets the physical press; the host gets none yet.
        XEvent physical{};
        physical.xbutton.type = ButtonPress;
        physical.xbutton.window = window;
        physical.xbutton.button = Button1;
        CHECK(XSendEvent(managerDisplay, window, False, ButtonPressMask, &physical));
        XSync(managerDisplay, False); XSync(sdkDisplay, False);
        CHECK(XPending(managerDisplay) == 0 && XPending(sdkDisplay) == 1);
        XEvent received{}; XNextEvent(sdkDisplay, &received);
        CHECK(received.type == ButtonPress);
        epochengine::core::QueueX11PointerInput(handle, 1, true, 10, 11, 0);
        epochengine::core::QueueX11PointerInput(handle, 0, false, 12, 13, 60);
        epochengine::core::QueueX11PointerInput(handle, 1, false, 14, 15, 0);
        drain(managerDisplay);
        CHECK(mousePressed.test(MouseLeft) && !mouseDown.test(MouseLeft));
        CHECK(mouseWheel.value == 60 && mouseX.value == 14 && mouseY.value == 15);
        CHECK(XPending(sdkDisplay) == 0); // No forwarding loop or wheel double count.

        // A late press cannot restore held input after native focus loss.
        mousePressed.reset(); mouseDown.set(MouseLeft);
        epochengine::core::QueueX11PointerInput(handle, 1, true, 20, 21, 0);
        XSetInputFocus(managerDisplay, root, RevertToParent, CurrentTime);
        drain(managerDisplay);
        CHECK(!mousePressed.test(MouseLeft) && !mouseDown.test(MouseLeft));
        mouseDown.set(MouseLeft);
        epochengine::core::QueueX11PointerInput(handle, 1, false, 22, 23, 0);
        drain(managerDisplay);
        CHECK(!mouseDown.test(MouseLeft));

        // Queued and later input from a retired/destroyed surface is rejected.
        epochengine::core::QueueX11PointerInput(handle, 0, false, 24, 25, 120);
        epochengine::core::manager.owned.erase(handle);
        XDestroyWindow(managerDisplay, window);
        epochengine::core::QueueX11PointerInput(handle, 0, false, 26, 27, 120);
        drain(managerDisplay);
        CHECK(mouseWheel.value == 60);
    }
    XCloseDisplay(sdkDisplay); XCloseDisplay(managerDisplay);
    return 0;
}
