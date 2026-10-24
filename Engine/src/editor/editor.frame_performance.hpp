// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once
// Low-overhead CPU-side editor frame timing. No telemetry process, OS sampler,
// GPU query, or logging runs on the hot path. Emit one aggregate every 5s.
#include "editor.frame_stall_watch.hpp"
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <optional>
#include <string_view>

namespace epoch_editor_perf {
[[nodiscard]] inline bool trace_enabled_in_environment() noexcept {
#if defined(_MSC_VER)
    // MSVC treats getenv as C4996 under secure CRT settings. Read this
    // once, at first use; release the allocated copy on every path.
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "EPOCH_EDITOR_PERF_TRACE") != 0)
        return true; // A failed optional diagnostics lookup is nonfatal.
    const bool enabled = (value == nullptr || std::string_view{value} != "0");
    std::free(value);
    return enabled;
#else
    if (const char* value = std::getenv("EPOCH_EDITOR_PERF_TRACE"))
        return std::string_view{value} != "0";
    return true;
#endif
}

struct Report final {
    unsigned count{};
    double average_ms{};
    double maximum_ms{};
    double initial_average_ms{};
    double pre_ui_average_ms{};
    double ui_average_ms{};
    double persist_average_ms{};
};
class FrameTimer final {
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    Time started_ = Clock::now();
    Time chatReady_ = started_;
    Time uiStarted_ = started_;
    Time uiDone_ = started_;
public:
    void mark_chat_ready() noexcept { chatReady_ = Clock::now(); current_frame_watch().mark(FrameStage::editor_after_chat); }
    void mark_ui_start() noexcept { uiStarted_ = Clock::now(); current_frame_watch().mark(FrameStage::editor_ui); }
    void mark_ui_done() noexcept { uiDone_ = Clock::now(); current_frame_watch().mark(FrameStage::editor_persist); }

    [[nodiscard]] std::optional<Report> finish() const noexcept {
        static const bool enabled = trace_enabled_in_environment();
        if (!enabled) return std::nullopt;
        const auto now = Clock::now();
        const auto millis = [](Time first, Time last) noexcept {
            return std::chrono::duration<double, std::milli>{last - first}.count();
        };
        struct Accum final {
            Time window_start = Clock::now();
            unsigned count{};
            double total{}, maximum{}, initial{}, pre_ui{}, ui{}, persist{};
        };
        static thread_local Accum rolling{};
        ++rolling.count;
        rolling.total += millis(started_, now);
        rolling.maximum = (std::max)(rolling.maximum, millis(started_, now));
        rolling.initial += millis(started_, chatReady_);
        rolling.pre_ui += millis(chatReady_, uiStarted_);
        rolling.ui += millis(uiStarted_, uiDone_);
        rolling.persist += millis(uiDone_, now);
        if (now - rolling.window_start < std::chrono::seconds{5})
            return std::nullopt;
        const double divisor = static_cast<double>(rolling.count);
        Report report{rolling.count, rolling.total / divisor, rolling.maximum,
            rolling.initial / divisor, rolling.pre_ui / divisor,
            rolling.ui / divisor, rolling.persist / divisor};
        rolling = Accum{.window_start = now};
        return report;
    }
};
} // namespace epoch_editor_perf
