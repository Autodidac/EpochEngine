// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once
// Per-render-thread stall breadcrumbs. Unlike a frame-completion timer, this
// watchdog reports when an editor frame *stops finishing*. It never samples
// GPU state, walks a filesystem, waits for the editor, or touches the GUI.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stop_token>
#include <string>
#include <thread>

extern "C" void core_log_write(std::uint32_t level, const char* category,
    const char* message);

namespace epoch_editor_perf {

enum class FrameStage : std::uint8_t {
    idle, host_setup, gui_begin, editor_entry, editor_after_chat, editor_ui,
    editor_persist, host_after_editor, gui_end, present
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
    default: return "Idle";
    }
}

// Atomic stage and its time are published together. The 56-bit millisecond
// clock has sufficient range for a steady-clock tick counter, and the reader
// cannot mistake a stage update for a partially-written timestamp.
class FrameStallWatch final {
    static constexpr std::uint64_t kClockMask = (std::uint64_t{1} << 56) - 1;
    std::atomic<std::uint64_t> snapshot_{0};
    std::jthread monitor_;

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
    void mark(FrameStage stage) noexcept {
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

// Keeps the watchdog disarmed if the frame exits through a different path.
class ScopedFrameStallWatch final {
    FrameStallWatch& watch_ = current_frame_watch();
public:
    ScopedFrameStallWatch() { watch_.mark(FrameStage::host_setup); }
    ~ScopedFrameStallWatch() { watch_.mark(FrameStage::idle); }
    ScopedFrameStallWatch(const ScopedFrameStallWatch&) = delete;
    ScopedFrameStallWatch& operator=(const ScopedFrameStallWatch&) = delete;
};

} // namespace epoch_editor_perf
