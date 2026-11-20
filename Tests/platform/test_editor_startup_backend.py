"""Exercise the production Editor startup decision and host lifecycle ordering."""

import json
import os
from pathlib import Path
import subprocess
import unittest
from uuid import uuid4

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp"


class EditorStartupBackend(unittest.TestCase):
    def test_automatic_native_fallback_retires_first_and_explicit_choices_win(self):
        owner = OWNER.read_text(encoding="utf-8")
        policy = definition(owner, "    [[nodiscard]] constexpr bool prefer_editor_startup_vulkan(")
        recovery = definition(owner, "    class AutomaticVulkanStartupAttempt final", True)
        orchestration = definition(owner, "    template <typename Instance, typename PumpEvents>")
        code = r'''
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define EPOCH_USING_VULKAN 1
#define EPOCH_USING_OPENGL 1
enum class BackendLifecycleState { pending, initializing, ready, failed, stopped };
enum class EditorStartupResult { started, cancelled, failed };
enum class Attempt { gl, vulkan, software, arrange, launch, retire };

struct LegacyLaunchConfig {
    int raylib_count{}, sdl_count{}, sfml_count{}, vulkan_count{}, opengl_count{1},
        directx_count{}, software_count{};
    bool parented{};
} defaults;
LegacyLaunchConfig resolve_legacy_launch_config() { return defaults; }

namespace cli {
    bool backend_selection_explicit{}, updater_shell_requested{}, capture_requested{},
        smoke_requested{}, smoke_context_switch_requested{};
}
namespace epochengine {
    std::string_view platform = "Windows";
    std::string_view GetRuntimePlatformDisplayString() { return platform; }
}
namespace input { void designate_polling_thread_to_current() {} }
namespace logger {
enum class LogLevel { INFO, WARN };
    struct Logger { template<typename... Args> void log(Args&&...) {} };
    Logger get(std::string_view) { return {}; }
}
namespace engine { constexpr std::string_view kEditorLog = "Editor"; }
#if defined(_WIN32)
namespace path {
    std::filesystem::path root;
    std::filesystem::path executable_path() { return root / "Editor.exe"; }
    std::filesystem::path log_output_dir() { return root; }
}
namespace platform::child_process {
    struct Identity { std::string sha256; bool valid() const { return sha256.size() == 64; } };
    char image = 'a';
    std::optional<Identity> inspect_executable(const std::filesystem::path& executable) {
        if (executable.filename() != "Editor.exe") return {};
        return Identity{std::string(64, image)};
    }
}
RECOVERY
#endif

struct Window {
    std::atomic_uint64_t successfulFrameGeneration{};
    BackendLifecycleState lifecycle{BackendLifecycleState::pending};
    bool close{};
    BackendLifecycleState backend_lifecycle() const { return lifecycle; }
    bool get_should_close() const { return close; }
};
struct MultiContextManager {
    std::vector<Attempt> trace;
    std::array<int, 7> lastCounts{};
    Window window;
    std::vector<Window*> windows;
    bool failInitialization{}, ownsWindow{};
    bool Initialize(void*, int raylib, int sdl, int sfml, int vulkan, int gl, int directx, int software, bool) {
        if (ownsWindow) return false;
        lastCounts = {raylib, sdl, sfml, vulkan, gl, directx, software};
        trace.push_back(vulkan ? Attempt::vulkan : gl ? Attempt::gl : Attempt::software);
        ownsWindow = true;
        windows.push_back(&window);
        return !(vulkan && failInitialization);
    }
    void ArrangeDockedWindowsGrid() { trace.push_back(Attempt::arrange); }
    void StartRenderThreads() { trace.push_back(Attempt::launch); }
    const auto& GetWindows() const { return windows; }
    void StopAll() { trace.push_back(Attempt::retire); ownsWindow = false; windows.clear(); }
};
POLICY
ORCHESTRATION
#define CHECK(...) do { if (!(__VA_ARGS__)) return __LINE__; } while (false)

int main(int argc, char** argv) {
    using Result = EditorStartupResult;
    using Step = Attempt;
    const std::vector gl{Step::gl, Step::arrange, Step::launch};
    const std::vector vk{Step::vulkan, Step::arrange, Step::launch};
    const std::vector retry{Step::vulkan, Step::arrange, Step::launch, Step::retire,
        Step::gl, Step::arrange, Step::launch};
#if defined(_WIN32)
    using Recovery = AutomaticVulkanStartupAttempt;
    using RecoveryState = Recovery::State;
    CHECK(argc >= 2);
    path::root = std::filesystem::path(argv[1]);
    if (argc == 3 && std::string_view{argv[2]} == "interrupt") {
        Recovery attempt;
        CHECK(attempt.state() == RecoveryState::armed);
        // Simulate a terminal Windows status without running a renderer or WER.
        ExitProcess(0xc0000409u);
    }
    if (argc == 3 && std::string_view{argv[2]} == "recover") {
        MultiContextManager previous;
        CHECK(start_editor_contexts(previous, nullptr, [] { return false; }, 20) == Result::started);
        CHECK(previous.trace == gl);
        MultiContextManager repeat;
        CHECK(start_editor_contexts(repeat, nullptr, [] { return false; }, 20) == Result::started);
        CHECK(repeat.trace == gl);
        cli::backend_selection_explicit = true;
        defaults.vulkan_count = 1;
        defaults.opengl_count = 0;
        MultiContextManager explicitVulkan;
        CHECK(start_editor_contexts(explicitVulkan, nullptr, [] { return false; }, 20) == Result::started);
        CHECK(explicitVulkan.trace == vk);
        platform::child_process::image = 'b';
        Recovery otherBuild;
        CHECK(otherBuild.state() == RecoveryState::armed);
        otherBuild.complete();
        return 0;
    }
    {
        Recovery owner;
        CHECK(owner.state() == RecoveryState::armed);
        Recovery competing;
        CHECK(competing.state() == RecoveryState::unavailable);
        MultiContextManager secondLaunch;
        CHECK(start_editor_contexts(secondLaunch, nullptr, [] { return false; }, 20) == Result::started);
        CHECK(secondLaunch.trace == gl);
        owner.complete();
    }
#else
    (void)argc;
    (void)argv;
#endif
    static_assert(prefer_editor_startup_vulkan(false, false, true, true, 10));
    static_assert(!prefer_editor_startup_vulkan(false, false, true, false, 12));
    MultiContextManager windows;
    unsigned polls{};
    CHECK(start_editor_contexts(windows, nullptr, [&] {
        if (++polls == 1) windows.window.lifecycle = BackendLifecycleState::ready;
        else windows.window.successfulFrameGeneration = 1;
        return true;
    }, 10) == Result::started);
    CHECK(polls == 2 && windows.trace == vk);
    CHECK(windows.lastCounts == std::array{0, 0, 0, 1, 0, 0, 0});

    MultiContextManager linuxHost;
    epochengine::platform = "Linux";
    CHECK(start_editor_contexts(linuxHost, nullptr, [&] {
        linuxHost.window.lifecycle = BackendLifecycleState::failed;
        return true;
    }, 10) == Result::started && linuxHost.trace == retry);
    CHECK(linuxHost.lastCounts == std::array{0, 0, 0, 0, 1, 0, 0});

    MultiContextManager initFailure;
    initFailure.failInitialization = true;
    CHECK(start_editor_contexts(initFailure, nullptr, [] { return false; }, 10) == Result::started);
    CHECK(initFailure.trace == std::vector{Step::vulkan, Step::retire, Step::gl,
        Step::arrange, Step::launch});

    MultiContextManager userClose;
    CHECK(start_editor_contexts(userClose, nullptr, [&] {
        userClose.window.close = true;
        return true;
    }, 10) == Result::cancelled);
    CHECK(userClose.trace == std::vector{Step::vulkan, Step::arrange, Step::launch, Step::retire});

    MultiContextManager wsl;
    epochengine::platform = "WSL Linux";
    CHECK(start_editor_contexts(wsl, nullptr, [] { return false; }, 20) == Result::started && wsl.trace == gl);
    MultiContextManager restricted;
    epochengine::platform = "Windows";
    cli::smoke_requested = true;
    CHECK(start_editor_contexts(restricted, nullptr, [] { return false; }, 20) == Result::started
        && restricted.trace == gl);
    cli::smoke_requested = false;

    MultiContextManager lowerTier;
    CHECK(start_editor_contexts(lowerTier, nullptr, [] { return false; }, 9) == Result::started
        && lowerTier.trace == gl);
    cli::backend_selection_explicit = true;
    defaults.opengl_count = 0;
    defaults.software_count = 1;
    MultiContextManager explicitSoftware;
    CHECK(start_editor_contexts(explicitSoftware, nullptr, [] { return false; }, 20) == Result::started);
    CHECK(explicitSoftware.trace == std::vector{Step::software, Step::arrange, Step::launch});
    defaults = {1, 1, 1, 1, 1, 1, 0, true};
    MultiContextManager explicitGrid;
    CHECK(start_editor_contexts(explicitGrid, nullptr, [] { return false; }, 20) == Result::started);
    CHECK(explicitGrid.lastCounts == std::array{1, 1, 1, 1, 1, 1, 0} && explicitGrid.trace == vk);
}
'''
        report = ROOT / "Artifacts/Reports/editor-startup-backend" / uuid4().hex[:8]
        report.mkdir(parents=True)
        fixture = report / "startup.cpp"
        fixture.write_text(code.replace("POLICY", policy).replace("ORCHESTRATION", orchestration)
                           .replace("RECOVERY", recovery), encoding="utf-8")
        kits = ()
        if os.name == "nt":
            from Tests.support.cpp import windows_sdk_includes
            sdk = windows_sdk_includes(Path("C:/Program Files (x86)/Windows Kits/10"))
            if sdk:
                kits = (sdk[-1] / "um", sdk[-1] / "shared")
        proof = compile_and_run(self, fixture, report, includes=kits,
                                args=lambda mode: (report / mode / "normal",))
        if os.name == "nt":
            for config in proof:
                mode = config["configuration"]
                executable = report / f"{mode}.exe"
                directory = report / mode / "interrupted"
                crash = subprocess.run([executable, directory, "interrupt"], cwd=report, timeout=15)
                self.assertEqual(crash.returncode & 0xffffffff, 0xc0000409)
                recovered = subprocess.run([executable, directory, "recover"], cwd=report, timeout=15)
                self.assertEqual(recovered.returncode, 0)
                config.update(simulated_fatal_exit=crash.returncode & 0xffffffff, recovered=recovered.returncode)
        (report / "result.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
