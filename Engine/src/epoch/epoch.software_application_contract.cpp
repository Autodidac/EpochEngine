// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "epoch.software_application_internal.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

import core.log;

namespace
{
    namespace software = epochengine::software;
    namespace platform = epochengine::platform;
    namespace error = epochengine::core::error;

    struct State
    {
        unsigned factories{};
        unsigned systems_destroyed{};
        unsigned windows_created{};
        unsigned windows_destroyed{};
        unsigned pumps{};
        unsigned init_calls{};
        unsigned ticks{};
        unsigned shutdown_calls{};
        unsigned observed{};
        int init_result{};
        unsigned quit_after{ 100 };
        bool factory_failure{};
        bool null_system{};
        bool create_failure{};
        bool invalid_handle{};
        bool close_on_second_pump{};
        bool lose_primary{};
        bool throw_init{};
        bool throw_tick{};
        bool throw_quit{};
        bool throw_shutdown{};
        bool throw_created{};
        bool throw_resize{};
        bool window_alive{};
        bool ordering_ok{ true };
        bool window_profile{};
        bool request_nested_window{};
        bool nested_window_refused{};
        std::vector<software::WindowEventKind> events{};
    };

    State* factory_state{};

    class ContractWindowSystem final : public platform::IWindowSystem
    {
    public:
        explicit ContractWindowSystem(State& state) : state_(state) {}
        ~ContractWindowSystem() noexcept override { ++state_.systems_destroyed; }

        error::result<platform::WindowHandle> create_window(const platform::WindowDesc&) noexcept override
        {
            if (state_.create_failure)
                return epochengine::unexpected(error::failed("contract create failure"));
            if (state_.invalid_handle) return platform::WindowHandle{};
            state_.window_alive = true;
            ++state_.windows_created;
            return platform::WindowHandle{ 17 };
        }

        void destroy_window(platform::WindowHandle handle) noexcept override
        {
            state_.ordering_ok = state_.ordering_ok && state_.window_alive && handle.value == 17;
            state_.window_alive = false;
            ++state_.windows_destroyed;
        }

        void pump_events(const epochengine::function_ref<void(const platform::WindowEvent&)>& handler) noexcept override
        {
            ++state_.pumps;
            // An unrelated window's close must never terminate this application.
            handler({ platform::WindowEventType::close, { 999 }, 0, 0 });
            if (state_.pumps == 1)
                handler({ platform::WindowEventType::resized, { 17 }, 800, 600 });
            if (state_.close_on_second_pump && state_.pumps == 2)
                handler({ platform::WindowEventType::close, { 17 }, 0, 0 });
        }

        void request_close(platform::WindowHandle) noexcept override {}
        void set_title(platform::WindowHandle, epochengine::string_view) noexcept override {}
        platform::WindowHandle primary_window() const noexcept override
        {
            return state_.lose_primary && state_.pumps != 0
                ? platform::WindowHandle{} : platform::WindowHandle{ 17 };
        }

    private:
        State& state_;
    };

    error::result<std::unique_ptr<platform::IWindowSystem>> factory() noexcept
    {
        ++factory_state->factories;
        if (factory_state->factory_failure)
            return epochengine::unexpected(error::failed("contract factory failure"));
        if (factory_state->null_system)
            return std::unique_ptr<platform::IWindowSystem>{};
        return std::make_unique<ContractWindowSystem>(*factory_state);
    }

    int APP_CALL initialize(void* user)
    {
        auto& state = *static_cast<State*>(user);
        ++state.init_calls;
        state.ordering_ok = state.ordering_ok && (!state.window_profile
            || (state.window_alive && state.events.size() == 1
                && state.events.front() == software::WindowEventKind::created));
        if (state.request_nested_window)
        {
            software::LaunchOptions nested{};
            nested.profile = software::Profile::platform_window;
            app_callbacks_v1 api{};
            api.version = APP_API_VERSION;
            api.size = sizeof(app_callbacks_v1);
            api.on_tick = +[](void*, std::uint64_t, double) {};
            const auto refused = software::detail::run_with_window_factory(nested, api, factory, true);
            state.nested_window_refused = refused.reason == software::ExitReason::window_host_busy
                && !refused.init_entered && !refused.window_created;
        }
        if (state.throw_init) throw 1;
        return state.init_result;
    }

    void APP_CALL tick(void* user, std::uint64_t frame, double elapsed)
    {
        auto& state = *static_cast<State*>(user);
        state.ordering_ok = state.ordering_ok && state.init_calls == 1
            && state.shutdown_calls == 0 && frame == state.ticks && elapsed >= 0.0;
        if (state.throw_tick) throw 2;
        ++state.ticks;
    }

    bool APP_CALL should_quit(void* user)
    {
        auto& state = *static_cast<State*>(user);
        if (state.throw_quit) throw 3;
        return state.ticks >= state.quit_after;
    }

    void APP_CALL shutdown(void* user)
    {
        auto& state = *static_cast<State*>(user);
        ++state.shutdown_calls;
        state.ordering_ok = state.ordering_ok && state.init_calls == 1
            && (!state.window_profile || state.window_alive);
        if (state.throw_shutdown) throw 4;
    }

    void observe(void* user, const software::WindowEvent& event)
    {
        auto& state = *static_cast<State*>(user);
        ++state.observed;
        state.ordering_ok = state.ordering_ok && event.native_window == 17 && state.window_alive;
        state.events.push_back(event.kind);
        if ((state.throw_created && event.kind == software::WindowEventKind::created)
            || (state.throw_resize && event.kind == software::WindowEventKind::resized)) throw 5;
    }

    app_callbacks_v1 callbacks(State& state)
    {
        return { APP_API_VERSION, sizeof(app_callbacks_v1), &state,
            initialize, tick, should_quit, shutdown };
    }

    software::LaunchOptions options(State& state, bool window = false)
    {
        software::LaunchOptions request{};
        request.profile = window ? software::Profile::platform_window : software::Profile::cli;
        request.maximum_ticks = 3;
        request.window.on_event = observe;
        request.window.observer_user = &state;
        state.window_profile = window;
        return request;
    }

    software::RunReport run(State& state, bool window = false, bool supported = true)
    {
        factory_state = &state;
        return software::detail::run_with_window_factory(options(state, window), callbacks(state), factory, supported);
    }

    unsigned failures{};

    void check(bool passed, std::string_view label)
    {
        if (!passed) ++failures;
        epochengine::core::log::write(passed
            ? epochengine::core::log::level::info : epochengine::core::log::level::error,
            "Epoch.SoftwareContract", { label.data(), label.size() });
    }
}

int main()
{
    using software::ExitReason;
    {
        State state{};
        const auto report = run(state, false, false);
        check(report.succeeded() && report.profile_preflight_passed && report.ticks == 3 && state.ticks == 3
            && state.init_calls == 1 && state.shutdown_calls == 1 && state.ordering_ok
            && state.factories == 0 && !report.window_created,
            "CLI: first tick executes, no window factory or warmup, one shutdown");
    }
    {
        State state{};
        const auto report = software::run(options(state), callbacks(state));
        check(report.succeeded() && report.ticks == 3 && !report.window_created,
            "Public CLI composition: callback lifecycle without native window");
    }
    for (unsigned invalid = 0; invalid < 5; ++invalid)
    {
        State state{};
        factory_state = &state;
        auto request = options(state);
        auto api = callbacks(state);
        if (invalid == 0) request.profile = static_cast<software::Profile>(255);
        if (invalid == 1) request.tick_interval_ms = 1001;
        if (invalid == 2) api.version = APP_API_VERSION + 1;
        if (invalid == 3) api.size = sizeof(app_callbacks_v1) - 1;
        if (invalid == 4) api.on_tick = nullptr;
        const auto report = software::detail::run_with_window_factory(request, api, factory, true);
        check(!report.succeeded() && state.init_calls == 0 && state.factories == 0,
            "Invalid profile/interval/callback ABI rejected before initialization");
    }
    {
        State state{};
        state.quit_after = 0;
        const auto report = run(state);
        check(report.succeeded() && report.reason == ExitReason::application_quit
            && report.ticks == 0 && state.shutdown_calls == 1,
            "Immediate quit still owns one init/shutdown pair");
    }
    {
        State state{};
        state.init_result = 41;
        const auto report = run(state);
        check(report.reason == ExitReason::application_init_failed && !report.initialized
            && report.application_init_code == 41 && state.ticks == 0 && state.shutdown_calls == 1,
            "Partial initialization failure keeps original result and calls shutdown");
    }
    for (unsigned throwing = 0; throwing < 4; ++throwing)
    {
        State state{};
        state.throw_init = throwing == 0;
        state.throw_tick = throwing == 1;
        state.throw_quit = throwing == 2;
        state.throw_shutdown = throwing == 3;
        const auto report = run(state);
        check(report.reason == ExitReason::callback_failed && !report.succeeded()
            && state.shutdown_calls == 1 && report.shutdown_entered,
            "Callback exception is contained and shutdown executes exactly once");
    }
    {
        State state{};
        state.init_result = 41;
        state.throw_shutdown = true;
        const auto report = run(state);
        check(report.reason == ExitReason::application_init_failed && report.application_init_code == 41
            && report.shutdown_failed && !report.succeeded(),
            "Shutdown failure does not hide the primary initialization failure");
    }
    {
        State state{};
        state.close_on_second_pump = true;
        const auto report = run(state, true);
        check(report.reason == ExitReason::window_closed && report.succeeded()
            && report.ticks == 1 && state.events.size() == 3 && state.ordering_ok
            && state.windows_destroyed == 1 && state.systems_destroyed == 1
            && report.window_released && !state.window_alive,
            "Window lease: created/init/resize/tick/close/shutdown/release ordering");
    }
    {
        State state{};
        const auto report = run(state, true);
        check(report.succeeded() && report.ticks == 3 && state.ordering_ok
            && state.observed == 2 && state.windows_destroyed == 1,
            "Events from other window identities do not terminate this application");
    }
    {
        State state{};
        state.lose_primary = true;
        const auto report = run(state, true);
        check(report.reason == ExitReason::window_closed && report.ticks == 0
            && state.windows_destroyed == 1 && state.shutdown_calls == 1,
            "Lost primary window stops ticking and releases its lease");
    }
    {
        State state{};
        const auto report = run(state, true, false);
        check(report.reason == ExitReason::unsupported_platform
            && !report.profile_preflight_passed && state.factories == 0 && state.init_calls == 0,
            "Unsupported platform refuses before any null/native factory call");
    }
    {
        State state{};
        state.request_nested_window = true;
        const auto report = run(state, true);
        check(report.succeeded() && state.nested_window_refused && state.factories == 1
            && state.windows_destroyed == 1,
            "Native window host rejects overlapping sessions before second factory/initialization");
    }
    for (unsigned invalid = 0; invalid < 4; ++invalid)
    {
        State state{};
        factory_state = &state;
        auto request = options(state, true);
        if (invalid == 0) request.window.width = 0;
        if (invalid == 1) request.window.height = 16385;
        if (invalid == 2) request.window.title = std::string(1025, 'x');
        if (invalid == 3) request.window.title = std::string("bad\0title", 9);
        const auto report = software::detail::run_with_window_factory(request, callbacks(state), factory, true);
        check(report.reason == ExitReason::invalid_options && state.factories == 0 && state.init_calls == 0,
            "Window dimensions/title are validated before acquiring native resources");
    }
    for (unsigned failed = 0; failed < 4; ++failed)
    {
        State state{};
        state.factory_failure = failed == 0;
        state.null_system = failed == 1;
        state.create_failure = failed == 2;
        state.invalid_handle = failed == 3;
        const auto report = run(state, true);
        check(report.reason == ExitReason::window_creation_failed && state.init_calls == 0
            && state.windows_destroyed == 0 && state.systems_destroyed == (failed >= 2 ? 1u : 0u),
            "Factory/create/invalid-handle failures release only acquired resources");
    }
    for (unsigned phase = 0; phase < 3; ++phase)
    {
        State state{};
        state.throw_created = phase == 0;
        state.throw_resize = phase == 1;
        state.throw_shutdown = phase == 2;
        const auto report = run(state, true);
        check(report.reason == ExitReason::callback_failed && !report.succeeded()
            && state.windows_destroyed == 1 && state.systems_destroyed == 1
            && state.shutdown_calls == (phase == 0 ? 0u : 1u) && !state.window_alive,
            "Window observer/shutdown exception cannot escape noexcept event pump or retain window");
    }
    {
        State first{};
        State second{};
        const auto a = run(first, true);
        const auto b = run(second, true);
        check(a.succeeded() && b.succeeded() && first.windows_destroyed == 1
            && second.windows_destroyed == 1 && !first.window_alive && !second.window_alive,
            "Sequential application sessions retain no previous owned window");
    }
#if !defined(_WIN32)
    {
        State state{};
        const auto report = software::run(options(state, true), callbacks(state));
        check(report.reason == ExitReason::unsupported_platform && !report.window_created,
            "Public non-Windows PlatformWindow reports unsupported, not simulated native success");
    }
#endif
    check(failures == 0, "software_application.contract=complete (in-memory lifecycle; no native presentation proof)");
    return failures == 0 ? 0 : 1;
}
