// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/epoch_platform_engine.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{
    [[noreturn]] void fail(const char* message)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

namespace {
    using namespace epochengine::platform;
    Runtime* shutdown_runtime{};
    std::atomic_bool shutdown_observed{};
    std::atomic_bool application_alive{}, late_shutdown{};
    bool lifetime_probe{};
    int application_mode{}, stop_calls{}, destroy_calls{};
    class ShutdownProbeDriver final : public IPlatformDriver {
        std::unique_ptr<IPlatformDriver> inner = create_headless_driver();
    public:
        DriverKind kind() const noexcept override { return inner->kind(); }
        std::string_view name() const noexcept override { return inner->name(); }
        DriverCapabilities capabilities() const noexcept override { return inner->capabilities(); }
        Status initialize(const DriverInit& init) noexcept override { return inner->initialize(init); }
        void shutdown() noexcept override {
            // Driver-thread teardown may still read its owning runtime registry.
            (void)shutdown_runtime->context_count();
            if (lifetime_probe && !application_alive.load()) late_shutdown.store(true);
            inner->shutdown();
            shutdown_observed.store(true);
        }
        Result<NativeWindowHandle> create_window(ContextId id, const WindowDesc& desc) noexcept override { return inner->create_window(id, desc); }
        void destroy_window(ContextId id) noexcept override { inner->destroy_window(id); }
        void pump_events(const EventSink& sink) noexcept override { inner->pump_events(sink); }
        NativeWindowHandle native_handle(ContextId id) const noexcept override { return inner->native_handle(id); }
        Status set_title(ContextId id, std::string_view title) noexcept override { return inner->set_title(id, title); }
        Status set_visible(ContextId id, bool visible) noexcept override { return inner->set_visible(id, visible); }
        Status resize(ContextId id, std::int32_t width, std::int32_t height) noexcept override { return inner->resize(id, width, height); }
        Status reparent(ContextId id, NativeWindowHandle parent, WindowMode mode) noexcept override { return inner->reparent(id, parent, mode); }
        void request_close(ContextId id) noexcept override { inner->request_close(id); }
    };
    class LifetimeApplication final : public IApplication {
    public:
        Status on_start(Runtime& runtime, const ApplicationLaunchInfo&) override {
            shutdown_runtime = &runtime;
            auto registered = runtime.register_driver(DriverKind::headless, +[]() -> std::unique_ptr<IPlatformDriver> { return std::make_unique<ShutdownProbeDriver>(); });
            if (!registered) return registered;
            ContextDesc desc{};
            desc.driver = DriverKind::headless;
            desc.thread_policy = ThreadPolicy::dedicated_driver_thread;
            desc.window.visible = false;
            auto context = runtime.create_context(desc);
            if (!context) return std::unexpected(context.error());
            if (application_mode == 1) return std::unexpected(Error{ErrorCode::initialization_failed, "intentional start failure"});
            if (application_mode == 2) throw 42;
            return {};
        }
        bool on_frame(Runtime&, std::chrono::nanoseconds) override { return false; }
        void on_stop(Runtime&) noexcept override { ++stop_calls; }
    };
    void test_application_outlives_runtime() {
        lifetime_probe = true;
        bool any_late = false;
        for (application_mode = 0; application_mode != 3; ++application_mode) {
            shutdown_observed = false; late_shutdown = false;
            stop_calls = 0; destroy_calls = 0;
            ApplicationFactory factory{
                .create = +[](const ApplicationLaunchInfo&) noexcept -> IApplication* { application_alive = true; return new LifetimeApplication; },
                .destroy = +[](IApplication* app) noexcept { application_alive = false; ++destroy_calls; delete app; }
            };
            const int result = run_application(factory, {});
            const int expected = application_mode == 0 ? 0 : application_mode == 1 ? 4 : 5;
            if (result != expected || destroy_calls != 1 || stop_calls != (application_mode == 0 ? 1 : 0)) fail("application exit/on_stop semantics changed");
            if (!shutdown_observed.load()) fail("application driver shutdown skipped");
            if (late_shutdown.load()) { std::cerr << "late driver shutdown path=" << application_mode << '\n'; any_late = true; }
        }
        lifetime_probe = false;
        shutdown_runtime = nullptr;
        if (any_late) fail("application destroyed before runtime driver shutdown");
    }
    void test_driver_join_releases_registry() {
        std::jthread watchdog([](std::stop_token stop) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!stop.stop_requested()) fail("driver shutdown blocked on runtime registry");
        });
        {
            Runtime probe;
            shutdown_runtime = &probe;
            if (!probe.register_driver(DriverKind::headless, +[]() -> std::unique_ptr<IPlatformDriver> { return std::make_unique<ShutdownProbeDriver>(); }))
                fail("probe driver registration");
            ContextDesc desc{};
            desc.driver = DriverKind::headless;
            desc.thread_policy = ThreadPolicy::dedicated_driver_thread;
            desc.window.visible = false;
            if (!probe.create_context(desc)) fail("probe context creation");
        }
        watchdog.request_stop();
        if (!shutdown_observed.load()) fail("driver shutdown was skipped");
        shutdown_runtime = nullptr;
    }
}

int main()
{
    test_driver_join_releases_registry();
    test_application_outlives_runtime();
    using namespace epochengine::platform;

    Runtime runtime(RuntimeDesc{
        .dedicated_driver_threads = true,
        .auto_register_builtin_drivers = true,
        .idle_wait = std::chrono::milliseconds(1)
    });

    if (!runtime.has_driver(DriverKind::headless))
        fail("headless driver was not registered");

    std::atomic_uint32_t initialized{};
    std::atomic_uint32_t frames{};
    std::atomic_uint32_t shutdown{};
    std::atomic_uint32_t commands{};

    ContextId first{};
    for (int index = 0; index < 3; ++index)
    {
        auto created = runtime.create_context(
            ContextDesc{
                .driver = DriverKind::headless,
                .type = ContextType::noop,
                .window = WindowDesc{
                    .title = "test",
                    .width = 320,
                    .height = 180,
                    .visible = false
                },
                .route = "test-" + std::to_string(index)
            },
            ContextCallbacks{
                .initialize = [&initialized](const ContextSnapshot&) -> Status
                {
                    ++initialized;
                    return {};
                },
                .frame = [&frames](const ContextSnapshot&, std::chrono::nanoseconds)
                {
                    ++frames;
                    return true;
                },
                .shutdown = [&shutdown](const ContextSnapshot&) { ++shutdown; }
            });
        if (!created)
            fail(created.error().message.c_str());
        if (index == 0)
            first = *created;
    }

    if (initialized.load() != 3)
        fail("not all contexts initialized");
    if (runtime.active_context_count() != 3)
        fail("active context count is wrong");

    auto dispatch_status = runtime.dispatch(first, [&commands](const ContextSnapshot& snapshot)
    {
        if (!snapshot.native.valid())
            return;
        ++commands;
    });
    if (!dispatch_status)
        fail(dispatch_status.error().message.c_str());

    auto resize_status = runtime.resize(first, 800, 600);
    if (!resize_status)
        fail(resize_status.error().message.c_str());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((commands.load() == 0 || frames.load() == 0) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (commands.load() == 0)
        fail("driver-thread command did not execute");
    if (frames.load() == 0)
        fail("frame callbacks did not execute");

    WindowEvent event{};
    bool saw_resize = false;
    const auto event_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!saw_resize && std::chrono::steady_clock::now() < event_deadline)
    {
        while (runtime.poll_event(first, event))
            saw_resize = saw_resize || event.type == WindowEventType::resized;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!saw_resize)
        fail("resize event was not delivered");

    for (const auto& context : runtime.contexts())
    {
        const auto status = runtime.destroy_context(context.id);
        if (!status)
            fail(status.error().message.c_str());
    }

    if (runtime.context_count() != 0)
        fail("contexts were not removed");
    if (shutdown.load() != 3)
        fail("not all shutdown callbacks executed");

    std::cout << "EpochPlatformEngine tests passed\n";
    return 0;
}
