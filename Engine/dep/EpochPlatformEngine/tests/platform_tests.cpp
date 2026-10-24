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

int main()
{
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
