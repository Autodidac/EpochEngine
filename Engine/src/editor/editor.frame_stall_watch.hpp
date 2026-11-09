// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once
// Per-owner-thread stall breadcrumbs. Unlike a frame-completion timer, this
// watchdog reports when an editor frame *stops finishing*. It never samples
// GPU state, walks a filesystem, waits for the editor, or touches the GUI.
#include <atomic>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

extern "C" void core_log_write(std::uint32_t level, const char* category,
    const char* message);

namespace epoch_editor_perf {

enum class FrameStage : std::uint8_t {
    idle, host_setup, gui_begin, editor_entry, editor_after_chat, editor_ui,
    editor_persist, host_after_editor, gui_end, present,
    session_pump, session_input, session_controller, session_update, session_wait,
    count
};

[[nodiscard]] constexpr const char* stage_name(FrameStage stage) noexcept {
    switch (stage) {
    case FrameStage::host_setup: return "Editor service / host frame preparation";
    case FrameStage::gui_begin: return "EpochGui begin_frame / atlas upload";
    case FrameStage::editor_entry: return "Editor setup / chat pump";
    case FrameStage::editor_after_chat: return "Editor pre-UI work";
    case FrameStage::editor_ui: return "Editor and EpochGui controls";
    case FrameStage::editor_persist: return "Editor layout persistence / finalization";
    case FrameStage::host_after_editor: return "Editor host scene / commands";
    case FrameStage::gui_end: return "EpochGui end_frame / draw submission";
    case FrameStage::present: return "Renderer present / backend queue";
    case FrameStage::session_pump: return "Native event pump";
    case FrameStage::session_input: return "Input state poll";
    case FrameStage::session_controller: return "Physical controller poll";
    case FrameStage::session_update: return "Session update / context management";
    case FrameStage::session_wait: return "Session pacing wait";
    default: return "Idle";
    }
}

// Opt into completed-stage evidence through the existing performance setting.
// Ordinary editor frame averages and the existing stall watchdog are unchanged.
[[nodiscard]] inline bool stage_trace_enabled() noexcept {
    static const bool enabled = []() noexcept {
#if defined(_MSC_VER)
        char* value = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&value, &length, "EPOCH_EDITOR_PERF_TRACE") != 0)
            return false;
        const bool selected = value && std::string_view{value} == "stages";
        std::free(value);
        return selected;
#else
        const char* value = std::getenv("EPOCH_EDITOR_PERF_TRACE");
        return value && std::string_view{value} == "stages";
#endif
    }();
    return enabled;
}

// Atomic stage and its time are published together. The 56-bit millisecond
// clock has sufficient range for a steady-clock tick counter, and the reader
// cannot mistake a stage update for a partially-written timestamp.
class FrameStallWatch final {
    static constexpr std::uint64_t kClockMask = (std::uint64_t{1} << 56) - 1;
    std::atomic<std::uint64_t> snapshot_{0};
    // Only the thread calling mark() owns these completed-stage aggregates.
    // The monitor continues to read only the atomic stall breadcrumb.
    using Clock = std::chrono::steady_clock;
    struct StageSample { std::uint64_t count{}; double total{}, maximum{}; };
    std::array<StageSample, static_cast<std::size_t>(FrameStage::count)> samples_{};
    FrameStage stage_{FrameStage::idle};
    Clock::time_point lastMark_{Clock::now()}, reportAt_{lastMark_};
    unsigned slowReports_{};
    std::jthread monitor_;

    void record_completed_stage(Clock::time_point now) noexcept {
        const double elapsed = std::chrono::duration<double, std::milli>(now - lastMark_).count();
        lastMark_ = now;
        if (stage_ != FrameStage::idle && stage_ != FrameStage::count) {
            auto& sample = samples_[static_cast<std::size_t>(stage_)];
            ++sample.count;
            sample.total += elapsed;
            if (elapsed > sample.maximum) sample.maximum = elapsed;
            if (elapsed > 50.0 && slowReports_ < 8u) {
                ++slowReports_;
                try {
                    const std::string message = std::string{"Completed stage: "} + stage_name(stage_)
                        + " elapsed_ms=" + std::to_string(elapsed)
                        + "; wall time includes blocking/waits, not CPU execution time.";
                    core_log_write(2u, "Engine.Editor.Stages", message.c_str());
                } catch (...) { /* Diagnostics cannot change session control. */ }
            }
        }
        if (now - reportAt_ < std::chrono::seconds{1}) return;
        try {
            // The console INFO formatter retains only 170 payload bytes.
            // One populated-stage record preserves every field on that path;
            // the fixed stage array bounds this to 14 records per second.
            for (std::size_t index = 1; index < samples_.size(); ++index) {
                const auto& sample = samples_[index];
                if (!sample.count) continue;
                const std::string message = std::string{"stage="} + stage_name(static_cast<FrameStage>(index))
                    + " count=" + std::to_string(sample.count)
                    + " avg_wall_ms=" + std::to_string(sample.total / static_cast<double>(sample.count))
                    + " max_wall_ms=" + std::to_string(sample.maximum);
                core_log_write(1u, "Engine.Editor.Stages", message.c_str());
            }
        } catch (...) { /* Drop this optional report, never retry per frame. */ }
        samples_ = {};
        reportAt_ = now;
    }

    [[nodiscard]] static std::uint64_t time_ms() noexcept {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()) & kClockMask;
    }

public:
    FrameStallWatch() : monitor_([this](std::stop_token stop) noexcept {
        std::uint64_t previously_reported{};
        std::uint64_t last_report_at{};
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{250});
            const auto value = snapshot_.load(std::memory_order_acquire);
            if (!value) { previously_reported = 0; continue; }
            const auto now = time_ms();
            const auto since = value & kClockMask;
            if (now < since || now - since < 2500) continue;
            if (value == previously_reported && now - last_report_at < 5000) continue;
            previously_reported = value;
            last_report_at = now;
            const auto stage = static_cast<FrameStage>(value >> 56);
            try {
                const std::string message = std::string{"Editor frame stalled for "}
                    + std::to_string(now - since) + " ms at stage: " + stage_name(stage)
                    + ". Model scan completion does not prove this stage is healthy.";
                core_log_write(3u, "Engine.Editor.Stall", message.c_str());
            } catch (...) { /* Diagnostics cannot terminate the process. */ }
        }
    }) {}
    FrameStallWatch(const FrameStallWatch&) = delete;
    FrameStallWatch& operator=(const FrameStallWatch&) = delete;
    ~FrameStallWatch() {
        monitor_.request_stop();
        if (monitor_.joinable()) monitor_.join();
    }
    [[nodiscard]] FrameStage stage() const noexcept { return stage_; }
    void mark(FrameStage stage) noexcept {
        if (stage_trace_enabled()) record_completed_stage(Clock::now());
        stage_ = stage;
        if (stage == FrameStage::idle) {
            snapshot_.store(0, std::memory_order_release);
        } else {
            snapshot_.store((static_cast<std::uint64_t>(stage) << 56) | time_ms(),
                std::memory_order_release);
        }
    }
};

[[nodiscard]] inline FrameStallWatch& current_frame_watch() {
    static thread_local FrameStallWatch watchdog{};
    return watchdog;
}

// Restore the surrounding session stage on every exit, or disarm a root frame.
class ScopedFrameStallWatch final {
    FrameStallWatch& watch_ = current_frame_watch();
    FrameStage previous_ = watch_.stage();
public:
    ScopedFrameStallWatch() { watch_.mark(FrameStage::host_setup); }
    ~ScopedFrameStallWatch() { watch_.mark(previous_); }
    ScopedFrameStallWatch(const ScopedFrameStallWatch&) = delete;
    ScopedFrameStallWatch& operator=(const ScopedFrameStallWatch&) = delete;
};

} // namespace epoch_editor_perf
