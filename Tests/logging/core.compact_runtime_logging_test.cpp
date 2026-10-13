// Standalone (no engine module build required):
//   g++ -std=c++23 -Wall -Wextra -Werror -pedantic core.compact_runtime_logging_test.cpp -o compact_logging_test
// Include the real engine string overloads first: this catches the MSVC
// C2593 ambiguity that a formatter-only compilation does not detect.
#include "../../Engine/include/core.stl_types.hpp"
#include "../../Engine/src/core/core.log_compact.hpp"
#include "../../Engine/src/renderers/host/host.log_policy.hpp"

#include <cassert>
#include <chrono>
#include <string>

namespace
{
    struct WindowState final
    {
        int activity{};
        int logical_width{1600};
        int logical_height{1000};
        int framebuffer_width{1600};
        int framebuffer_height{1000};
        int dpi_milli{1000};
        int resize_generation{};
        bool presentable{true};
        bool visible{true};
        bool focused{true};
        bool minimized{};
        bool occluded{};
        bool dpi_known{true};
        bool visibility_known{true};
        bool focus_known{true};
        bool minimized_known{true};
        bool occlusion_known{true};
        constexpr bool operator==(const WindowState&) const noexcept = default;
    };
}

int main()
{
    using epochengine::core::log::display::format_line;
    using epochengine::rendercontext::logging::WindowStateLogGate;
    using namespace std::chrono_literals;

    const auto compact = format_line(
        "2026-10-11 18:32:05", "INFO", "Context.Multiplexer.Win",
        "(host.context_win.cpp:5282) - Backend OpenGL pacing requested=target_hz configured_hz=120 effective=target_hz effective_hz=120 activity=foreground native_requested=false native_configured=false native_active=false cpu_wait=true.");
    assert(compact == "18:32:05 [INFO] Context.Win: OpenGL pacing target_hz @ 120 Hz");
    const auto window = format_line(
        "2026-10-11 18:32:09", "INFO", "Context.Multiplexer.Win",
        "(host.context_win.cpp:5237) - Backend OpenGL window state activity=foreground presentable=true logical=1600x1000 framebuffer=1600x1000 dpi_milli=1000 dpi_known=true visibility_known=true focus_known=true occlusion_known=true resize_generation=1.");
    assert(window == "18:32:09 [INFO] Context.Win: OpenGL window foreground 1600x1000 (fb 1600x1000)");
    const auto source = format_line("2026-10-11 18:36:55", "INFO", "Engine.AI.Source",
        "(editor.ai_development_panel.cpp:12583) - Resident source; path=Engine/modules/gui.engine.ixx; first_line=1; last_line=570; bytes=16337; content_sha256=d5d44d89166cc917a81ec1d3236a12156323b98766cf413ecd48652d8a8c1bd2");
    assert(source == "18:36:55 [INFO] AI.Source: Source Engine/modules/gui.engine.ixx:1-570");
    const auto warning = format_line("2026-10-11 18:32:11", "WARN", "ai",
        "(ai.engine.cpp:99) - WinHTTP: WinHttpSendRequest failed");
    assert(warning.find("ai.engine.cpp:99") != std::string::npos);
    assert(warning.find("WinHttpSendRequest failed") != std::string::npos);
    const std::string long_error(500, 'X');
    assert(format_line("x", "ERROR", "ai", long_error).size() > 500);
    const std::string long_info(500, 'Y');
    assert(format_line("x", "INFO", "ai", long_info).size() < 200);

    WindowStateLogGate<WindowState> gate;
    const WindowStateLogGate<WindowState>::clock::time_point t{};
    WindowState ws;
    assert(gate.should_report(ws, t));
    assert(!gate.should_report(ws, t + 100ms));
    for (int n = 0; n != 40; ++n)
    {
        ws.logical_width += 2;
        ws.framebuffer_width += 2;
        ws.resize_generation++;
        assert(!gate.should_report(ws, t + 150ms + n * 10ms));
    }
    assert(!gate.should_report(ws, t + 800ms));
    assert(gate.should_report(ws, t + 900ms)); // final settled dimensions
    assert(!gate.should_report(ws, t + 1000ms));
    ws.focused = false;
    ws.activity = 1;
    assert(gate.should_report(ws, t + 1010ms)); // important state transition
    ws.logical_width += 100;
    assert(!gate.should_report(ws, t + 1050ms));
    ws.minimized = true;
    ws.presentable = false;
    assert(gate.should_report(ws, t + 1060ms)); // important state transition
}
