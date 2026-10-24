// SPDX-License-Identifier: MPL-2.0
#if defined(__linux__) && defined(EPOCH_PLATFORM_HAS_X11)

#include <epochplatformengine/driver.hpp>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef Status
#    undef Status
#endif

#include <array>
#include <climits>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace epochengine::platform
{
    namespace
    {
        struct X11WindowRecord final
        {
            ::Window window{};
            ::Window parent{};
            WindowDesc desc{};
        };

        class X11Driver final : public IPlatformDriver
        {
        public:
            [[nodiscard]] DriverKind kind() const noexcept override { return DriverKind::x11; }
            [[nodiscard]] std::string_view name() const noexcept override { return "EpochPlatformEngine X11"; }
            [[nodiscard]] DriverCapabilities capabilities() const noexcept override
            {
                return {
                    .native_windows = true,
                    .multiple_windows = true,
                    .parented_windows = true,
                    .detached_windows = true,
                    .high_dpi = false,
                    .file_drop = false,
                    .supports_dedicated_thread = true,
                    .requires_main_thread = false
                };
            }

            [[nodiscard]] Status initialize(const DriverInit&) noexcept override
            {
                static std::once_flag x11_threads_once;
                std::call_once(x11_threads_once, [] { XInitThreads(); });

                display_ = XOpenDisplay(nullptr);
                if (display_ == nullptr)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "XOpenDisplay failed; DISPLAY is unavailable" });

                screen_ = DefaultScreen(display_);
                wm_delete_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
                initialized_ = true;
                return {};
            }

            void shutdown() noexcept override
            {
                if (display_ == nullptr)
                    return;

                for (const auto& [context, record] : windows_)
                {
                    (void)context;
                    if (record.window != 0)
                        XDestroyWindow(display_, record.window);
                }
                windows_.clear();
                reverse_.clear();
                XFlush(display_);
                XCloseDisplay(display_);
                display_ = nullptr;
                initialized_ = false;
            }

            [[nodiscard]] Result<NativeWindowHandle> create_window(const ContextId context, const WindowDesc& desc) noexcept override
            {
                if (!initialized_ || display_ == nullptr)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "X11 driver is not initialized" });

                ::Window parent = RootWindow(display_, screen_);
                if (desc.parent.kind == NativeHandleKind::x11 && desc.parent.value != 0)
                    parent = static_cast<::Window>(desc.parent.value);

                const int x = desc.x >= 0 ? desc.x : 0;
                const int y = desc.y >= 0 ? desc.y : 0;
                const unsigned int width = static_cast<unsigned int>(desc.width);
                const unsigned int height = static_cast<unsigned int>(desc.height);

                ::Window window = XCreateSimpleWindow(
                    display_, parent, x, y, width, height, 0,
                    BlackPixel(display_, screen_), WhitePixel(display_, screen_));
                if (window == 0)
                    return std::unexpected(Error{ ErrorCode::window_creation_failed, "XCreateSimpleWindow failed" });

                const long event_mask = StructureNotifyMask | FocusChangeMask | KeyPressMask | KeyReleaseMask |
                    PointerMotionMask | ButtonPressMask | ButtonReleaseMask | ExposureMask;
                XSelectInput(display_, window, event_mask);
                XStoreName(display_, window, desc.title.c_str());

                if (desc.mode != WindowMode::child)
                    XSetWMProtocols(display_, window, &wm_delete_, 1);

                if (!desc.resizable)
                {
                    XSizeHints hints{};
                    hints.flags = PMinSize | PMaxSize;
                    hints.min_width = hints.max_width = desc.width;
                    hints.min_height = hints.max_height = desc.height;
                    XSetWMNormalHints(display_, window, &hints);
                }

                windows_.emplace(context, X11WindowRecord{ .window = window, .parent = parent, .desc = desc });
                reverse_.emplace(window, context);

                if (desc.visible)
                    XMapWindow(display_, window);
                XFlush(display_);

                return NativeWindowHandle{
                    .kind = NativeHandleKind::x11,
                    .display = display_,
                    .parent = reinterpret_cast<void*>(static_cast<std::uintptr_t>(parent)),
                    .value = static_cast<std::uintptr_t>(window)
                };
            }

            void destroy_window(const ContextId context) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end() || display_ == nullptr)
                    return;
                const ::Window window = iterator->second.window;
                reverse_.erase(window);
                windows_.erase(iterator);
                XDestroyWindow(display_, window);
                XFlush(display_);
                queued_events_.push_back(WindowEvent{ .type = WindowEventType::destroyed, .context = context });
            }

            void pump_events(const EventSink& sink) noexcept override
            {
                if (display_ == nullptr)
                    return;

                while (XPending(display_) > 0)
                {
                    XEvent native{};
                    XNextEvent(display_, &native);

                    const auto reverse_iterator = reverse_.find(native.xany.window);
                    if (reverse_iterator == reverse_.end())
                        continue;
                    const ContextId context = reverse_iterator->second;

                    switch (native.type)
                    {
                    case ClientMessage:
                        if (static_cast<Atom>(native.xclient.data.l[0]) == wm_delete_)
                            queued_events_.push_back(WindowEvent{ .type = WindowEventType::close_requested, .context = context });
                        break;
                    case DestroyNotify:
                        queued_events_.push_back(WindowEvent{ .type = WindowEventType::destroyed, .context = context });
                        break;
                    case ConfigureNotify:
                        queued_events_.push_back(WindowEvent{
                            .type = WindowEventType::resized,
                            .context = context,
                            .x = native.xconfigure.x,
                            .y = native.xconfigure.y,
                            .width = native.xconfigure.width,
                            .height = native.xconfigure.height
                        });
                        break;
                    case FocusIn:
                        queued_events_.push_back(WindowEvent{ .type = WindowEventType::focus_gained, .context = context });
                        break;
                    case FocusOut:
                        queued_events_.push_back(WindowEvent{ .type = WindowEventType::focus_lost, .context = context });
                        break;
                    case MotionNotify:
                        queued_events_.push_back(WindowEvent{
                            .type = WindowEventType::mouse_moved,
                            .context = context,
                            .x = native.xmotion.x,
                            .y = native.xmotion.y
                        });
                        break;
                    case ButtonPress:
                        if (native.xbutton.button == Button4 || native.xbutton.button == Button5)
                        {
                            queued_events_.push_back(WindowEvent{
                                .type = WindowEventType::mouse_wheel,
                                .context = context,
                                .x = native.xbutton.x,
                                .y = native.xbutton.y,
                                .wheel_delta = native.xbutton.button == Button4 ? 120 : -120
                            });
                        }
                        else
                        {
                            queued_events_.push_back(WindowEvent{
                                .type = WindowEventType::mouse_button_pressed,
                                .context = context,
                                .x = native.xbutton.x,
                                .y = native.xbutton.y,
                                .button = static_cast<std::int32_t>(native.xbutton.button)
                            });
                        }
                        break;
                    case ButtonRelease:
                        if (native.xbutton.button != Button4 && native.xbutton.button != Button5)
                        {
                            queued_events_.push_back(WindowEvent{
                                .type = WindowEventType::mouse_button_released,
                                .context = context,
                                .x = native.xbutton.x,
                                .y = native.xbutton.y,
                                .button = static_cast<std::int32_t>(native.xbutton.button)
                            });
                        }
                        break;
                    case KeyPress:
                    case KeyRelease:
                    {
                        std::array<char, 64> text{};
                        KeySym symbol{};
                        XComposeStatus compose{};
                        const int length = XLookupString(&native.xkey, text.data(), static_cast<int>(text.size() - 1), &symbol, &compose);
                        queued_events_.push_back(WindowEvent{
                            .type = native.type == KeyPress ? WindowEventType::key_pressed : WindowEventType::key_released,
                            .context = context,
                            .key = static_cast<std::int32_t>(symbol)
                        });
                        if (native.type == KeyPress && length > 0)
                        {
                            queued_events_.push_back(WindowEvent{
                                .type = WindowEventType::text_input,
                                .context = context,
                                .text = std::string(text.data(), static_cast<std::size_t>(length))
                            });
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                std::deque<WindowEvent> local;
                local.swap(queued_events_);
                for (const auto& event : local)
                    sink(event);
            }

            [[nodiscard]] NativeWindowHandle native_handle(const ContextId context) const noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return {};
                return {
                    .kind = NativeHandleKind::x11,
                    .display = display_,
                    .parent = reinterpret_cast<void*>(static_cast<std::uintptr_t>(iterator->second.parent)),
                    .value = static_cast<std::uintptr_t>(iterator->second.window)
                };
            }

            [[nodiscard]] Status set_title(const ContextId context, const std::string_view title) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "X11 context not found" });
                const std::string value(title);
                XStoreName(display_, iterator->second.window, value.c_str());
                XFlush(display_);
                iterator->second.desc.title = value;
                return {};
            }

            [[nodiscard]] Status set_visible(const ContextId context, const bool visible) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "X11 context not found" });
                if (visible)
                    XMapWindow(display_, iterator->second.window);
                else
                    XUnmapWindow(display_, iterator->second.window);
                XFlush(display_);
                iterator->second.desc.visible = visible;
                return {};
            }

            [[nodiscard]] Status resize(const ContextId context, const std::int32_t width, const std::int32_t height) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "X11 context not found" });
                XResizeWindow(display_, iterator->second.window, static_cast<unsigned int>(width), static_cast<unsigned int>(height));
                XFlush(display_);
                iterator->second.desc.width = width;
                iterator->second.desc.height = height;
                return {};
            }

            [[nodiscard]] Status reparent(const ContextId context, const NativeWindowHandle parent, const WindowMode mode) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "X11 context not found" });

                ::Window native_parent = RootWindow(display_, screen_);
                if (parent.kind == NativeHandleKind::x11 && parent.value != 0)
                    native_parent = static_cast<::Window>(parent.value);
                XReparentWindow(display_, iterator->second.window, native_parent, 0, 0);
                XFlush(display_);
                iterator->second.parent = native_parent;
                iterator->second.desc.parent = parent;
                iterator->second.desc.mode = mode;
                return {};
            }

            void request_close(const ContextId context) noexcept override
            {
                if (windows_.contains(context))
                    queued_events_.push_back(WindowEvent{ .type = WindowEventType::close_requested, .context = context });
            }

        private:
            Display* display_{};
            int screen_{};
            Atom wm_delete_{};
            bool initialized_{};
            std::unordered_map<ContextId, X11WindowRecord> windows_{};
            std::unordered_map<::Window, ContextId> reverse_{};
            std::deque<WindowEvent> queued_events_{};
        };
    }

    std::unique_ptr<IPlatformDriver> create_x11_driver()
    {
        return std::make_unique<X11Driver>();
    }
}

#endif
