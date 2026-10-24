// SPDX-License-Identifier: MPL-2.0
#if defined(__APPLE__)

#include <epochplatformengine/driver.hpp>

#import <Cocoa/Cocoa.h>

#include <deque>
#include <memory>
#include <unordered_map>

struct EpochPlatformCocoaBridge final
{
    void* object{};
    void (*close)(void*, unsigned long long) noexcept{};
    void (*resize)(void*, unsigned long long, int, int) noexcept{};
};

extern "C" void epoch_platform_cocoa_queue_close(void* owner, unsigned long long context) noexcept;
extern "C" void epoch_platform_cocoa_queue_resize(void* owner, unsigned long long context, int width, int height) noexcept;

@interface EpochPlatformWindowDelegate : NSObject<NSWindowDelegate>
{
@public
    void* _owner;
    unsigned long long _context;
}
@end

@implementation EpochPlatformWindowDelegate
- (BOOL)windowShouldClose:(id)sender
{
    (void)sender;
    epoch_platform_cocoa_queue_close(_owner, _context);
    return NO;
}

- (void)windowDidResize:(NSNotification*)notification
{
    NSWindow* window = (NSWindow*)[notification object];
    NSRect bounds = [[window contentView] bounds];
    epoch_platform_cocoa_queue_resize(_owner, _context, (int)bounds.size.width, (int)bounds.size.height);
}
@end

namespace epochengine::platform
{
    namespace
    {
        struct CocoaWindowRecord final
        {
            NSWindow* window{};
            EpochPlatformWindowDelegate* delegate{};
            WindowDesc desc{};
        };

        class CocoaDriver final : public IPlatformDriver
        {
        public:
            CocoaDriver() noexcept
            {
                bridge_.object = this;
                bridge_.close = [](void* object, const unsigned long long context) noexcept
                {
                    static_cast<CocoaDriver*>(object)->queue_close(ContextId{ context });
                };
                bridge_.resize = [](void* object, const unsigned long long context, const int width, const int height) noexcept
                {
                    static_cast<CocoaDriver*>(object)->queue_resize(ContextId{ context }, width, height);
                };
            }

            [[nodiscard]] DriverKind kind() const noexcept override { return DriverKind::cocoa; }
            [[nodiscard]] std::string_view name() const noexcept override { return "EpochPlatformEngine Cocoa"; }
            [[nodiscard]] DriverCapabilities capabilities() const noexcept override
            {
                return {
                    .native_windows = true,
                    .multiple_windows = true,
                    .parented_windows = false,
                    .detached_windows = true,
                    .high_dpi = true,
                    .file_drop = false,
                    .supports_dedicated_thread = false,
                    .requires_main_thread = true
                };
            }

            [[nodiscard]] Status initialize(const DriverInit&) noexcept override
            {
                @autoreleasepool
                {
                    application_ = [NSApplication sharedApplication];
                    [application_ setActivationPolicy:NSApplicationActivationPolicyRegular];
                    [application_ finishLaunching];
                    initialized_ = true;
                }
                return {};
            }

            void shutdown() noexcept override
            {
                while (!windows_.empty())
                    destroy_window(windows_.begin()->first);
                initialized_ = false;
            }

            [[nodiscard]] Result<NativeWindowHandle> create_window(const ContextId context, const WindowDesc& desc) noexcept override
            {
                if (!initialized_)
                    return std::unexpected(Error{ ErrorCode::initialization_failed, "Cocoa driver is not initialized" });

                @autoreleasepool
                {
                    NSUInteger style = desc.decorated
                        ? (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                        : NSWindowStyleMaskBorderless;
                    if (desc.resizable)
                        style |= NSWindowStyleMaskResizable;

                    NSRect rectangle = NSMakeRect(
                        desc.x >= 0 ? desc.x : 100,
                        desc.y >= 0 ? desc.y : 100,
                        desc.width,
                        desc.height);
                    NSWindow* window = [[NSWindow alloc]
                        initWithContentRect:rectangle
                        styleMask:style
                        backing:NSBackingStoreBuffered
                        defer:NO];
                    if (window == nil)
                        return std::unexpected(Error{ ErrorCode::window_creation_failed, "NSWindow allocation failed" });

                    NSString* title = [[NSString alloc] initWithBytes:desc.title.data()
                        length:desc.title.size()
                        encoding:NSUTF8StringEncoding];
                    [window setTitle:title != nil ? title : @"EpochPlatformEngine"];
                    [title release];
                    [window setReleasedWhenClosed:NO];
                    [window setAcceptsMouseMovedEvents:YES];

                    auto* delegate = [[EpochPlatformWindowDelegate alloc] init];
                    delegate->_owner = &bridge_;
                    delegate->_context = context.value;
                    [window setDelegate:delegate];

                    if (desc.parent.kind == NativeHandleKind::cocoa && desc.parent.window != nullptr)
                        [(NSWindow*)desc.parent.window addChildWindow:window ordered:NSWindowAbove];
                    if (desc.visible)
                    {
                        [window makeKeyAndOrderFront:nil];
                        [application_ activateIgnoringOtherApps:YES];
                    }

                    windows_.emplace(context, CocoaWindowRecord{ .window = window, .delegate = delegate, .desc = desc });
                    return make_handle(windows_.at(context));
                }
            }

            void destroy_window(const ContextId context) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return;
                @autoreleasepool
                {
                    [iterator->second.window setDelegate:nil];
                    [iterator->second.window orderOut:nil];
                    [iterator->second.window close];
                    [iterator->second.delegate release];
                    [iterator->second.window release];
                }
                windows_.erase(iterator);
                events_.push_back(WindowEvent{ .type = WindowEventType::destroyed, .context = context });
            }

            void pump_events(const EventSink& sink) noexcept override
            {
                @autoreleasepool
                {
                    NSEvent* event{};
                    do
                    {
                        event = [application_ nextEventMatchingMask:NSEventMaskAny
                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0.0]
                            inMode:NSDefaultRunLoopMode
                            dequeue:YES];
                        if (event != nil)
                            [application_ sendEvent:event];
                    }
                    while (event != nil);
                    [application_ updateWindows];
                }

                std::deque<WindowEvent> local;
                local.swap(events_);
                for (const auto& event : local)
                    sink(event);
            }

            [[nodiscard]] NativeWindowHandle native_handle(const ContextId context) const noexcept override
            {
                const auto iterator = windows_.find(context);
                return iterator == windows_.end() ? NativeWindowHandle{} : make_handle(iterator->second);
            }

            [[nodiscard]] Status set_title(const ContextId context, const std::string_view title) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Cocoa context not found" });
                @autoreleasepool
                {
                    NSString* value = [[NSString alloc] initWithBytes:title.data() length:title.size() encoding:NSUTF8StringEncoding];
                    [iterator->second.window setTitle:value != nil ? value : @"EpochPlatformEngine"];
                    [value release];
                }
                iterator->second.desc.title.assign(title);
                return {};
            }

            [[nodiscard]] Status set_visible(const ContextId context, const bool visible) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Cocoa context not found" });
                if (visible)
                    [iterator->second.window makeKeyAndOrderFront:nil];
                else
                    [iterator->second.window orderOut:nil];
                iterator->second.desc.visible = visible;
                return {};
            }

            [[nodiscard]] Status resize(const ContextId context, const std::int32_t width, const std::int32_t height) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Cocoa context not found" });
                NSRect frame = [iterator->second.window frame];
                const NSRect content = NSMakeRect(0, 0, width, height);
                const NSRect outer = [iterator->second.window frameRectForContentRect:content];
                frame.size = outer.size;
                [iterator->second.window setFrame:frame display:YES animate:NO];
                iterator->second.desc.width = width;
                iterator->second.desc.height = height;
                return {};
            }

            [[nodiscard]] Status reparent(const ContextId context, const NativeWindowHandle parent, const WindowMode mode) noexcept override
            {
                const auto iterator = windows_.find(context);
                if (iterator == windows_.end())
                    return std::unexpected(Error{ ErrorCode::context_not_found, "Cocoa context not found" });
                if (NSWindow* existing_parent = [iterator->second.window parentWindow])
                    [existing_parent removeChildWindow:iterator->second.window];
                if (parent.kind == NativeHandleKind::cocoa && parent.window != nullptr)
                    [(NSWindow*)parent.window addChildWindow:iterator->second.window ordered:NSWindowAbove];
                iterator->second.desc.parent = parent;
                iterator->second.desc.mode = mode;
                return {};
            }

            void request_close(const ContextId context) noexcept override
            {
                queue_close(context);
            }

            void queue_close(const ContextId context) noexcept
            {
                if (windows_.contains(context))
                    events_.push_back(WindowEvent{ .type = WindowEventType::close_requested, .context = context });
            }

            void queue_resize(const ContextId context, const int width, const int height) noexcept
            {
                if (windows_.contains(context))
                    events_.push_back(WindowEvent{ .type = WindowEventType::resized, .context = context, .width = width, .height = height });
            }

        private:
            [[nodiscard]] static NativeWindowHandle make_handle(const CocoaWindowRecord& record) noexcept
            {
                return {
                    .kind = NativeHandleKind::cocoa,
                    .window = record.window,
                    .parent = [record.window parentWindow],
                    .value = reinterpret_cast<std::uintptr_t>(record.window)
                };
            }

            EpochPlatformCocoaBridge bridge_{};
            NSApplication* application_{};
            bool initialized_{};
            std::unordered_map<ContextId, CocoaWindowRecord> windows_{};
            std::deque<WindowEvent> events_{};
        };
    }

    std::unique_ptr<IPlatformDriver> create_cocoa_driver()
    {
        return std::make_unique<CocoaDriver>();
    }
}

extern "C" void epoch_platform_cocoa_queue_close(void* owner, const unsigned long long context) noexcept
{
    auto* bridge = static_cast<EpochPlatformCocoaBridge*>(owner);
    if (bridge != nullptr && bridge->close != nullptr)
        bridge->close(bridge->object, context);
}

extern "C" void epoch_platform_cocoa_queue_resize(void* owner, const unsigned long long context, const int width, const int height) noexcept
{
    auto* bridge = static_cast<EpochPlatformCocoaBridge*>(owner);
    if (bridge != nullptr && bridge->resize != nullptr)
        bridge->resize(bridge->object, context, width, height);
}

#endif
