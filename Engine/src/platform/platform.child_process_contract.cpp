#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

import platform.child_process;

#if defined(EPOCH_PLATFORM_CHILD_PROCESS_CONTRACT_MAIN)
#if defined(_WIN32)
namespace
{
    constexpr std::string_view capture_input = "epoch-stdin-sentinel";
    constexpr std::string_view capture_stdout = "epoch-stdout-captured\n";
    constexpr std::string_view capture_stderr = "epoch-stderr-captured\n";

    struct CaptureInheritanceFixture final
    {
        HANDLE sentinel{};
        HANDLE input_read{};
        HANDLE input_write{};
        HANDLE previous_input{};
        bool input_replaced{};

        void restore_input() noexcept
        {
            if (input_replaced)
            {
                (void)::SetStdHandle(STD_INPUT_HANDLE, previous_input);
                input_replaced = false;
            }
        }

        ~CaptureInheritanceFixture() noexcept
        {
            restore_input();
            for (HANDLE handle : {sentinel, input_read, input_write})
            {
                if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
                    (void)::CloseHandle(handle);
            }
        }

        [[nodiscard]] bool prepare() noexcept
        {
            SECURITY_ATTRIBUTES security{};
            security.nLength = sizeof(security);
            security.bInheritHandle = TRUE;
            sentinel = ::CreateEventW(&security, TRUE, FALSE, nullptr);
            if (sentinel == nullptr
                || ::CreatePipe(&input_read, &input_write, &security, 0u) == FALSE)
            {
                return false;
            }
            // A captured launch must duplicate stdin deliberately; neither pipe
            // handle may leak merely because the parent owns it.
            if (::SetHandleInformation(input_read, HANDLE_FLAG_INHERIT, 0u) == FALSE
                || ::SetHandleInformation(input_write, HANDLE_FLAG_INHERIT, 0u) == FALSE)
            {
                return false;
            }
            DWORD written{};
            if (::WriteFile(input_write, capture_input.data(),
                    static_cast<DWORD>(capture_input.size()), &written, nullptr) == FALSE
                || written != capture_input.size())
            {
                return false;
            }
            previous_input = ::GetStdHandle(STD_INPUT_HANDLE);
            input_replaced = ::SetStdHandle(STD_INPUT_HANDLE, input_read) != FALSE;
            return input_replaced;
        }
    };

    [[nodiscard]] int run_capture_inheritance_child(const char* sentinelText)
    {
        const HANDLE forbidden = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(std::stoull(sentinelText)));
        if (::SetEvent(forbidden) != FALSE)
            return 91;

        std::string input(capture_input.size(), '\0');
        DWORD received{};
        if (::ReadFile(::GetStdHandle(STD_INPUT_HANDLE), input.data(),
                static_cast<DWORD>(input.size()), &received, nullptr) == FALSE
            || received != input.size() || input != capture_input)
        {
            return 92;
        }
        DWORD written{};
        if (::WriteFile(::GetStdHandle(STD_OUTPUT_HANDLE), capture_stdout.data(),
                static_cast<DWORD>(capture_stdout.size()), &written, nullptr) == FALSE
            || written != capture_stdout.size())
        {
            return 93;
        }
        if (::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), capture_stderr.data(),
                static_cast<DWORD>(capture_stderr.size()), &written, nullptr) == FALSE
            || written != capture_stderr.size())
        {
            return 94;
        }
        return 17;
    }
}
#endif

int main(int argc, char** argv)
{
    namespace child_process = epochengine::platform::child_process;

#if defined(_WIN32)
    if (argc == 3
        && std::string_view{argv[1]} == "--epoch-capture-inheritance-contract")
    {
        return run_capture_inheritance_child(argv[2]);
    }
#endif

    if (argc >= 3 && std::string_view{argv[1]} == "--epoch-child-contract")
    {
        const int duration = std::stoi(argv[2]);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        return argc >= 4 ? std::stoi(argv[3]) : 0;
    }

    if (argc < 1 || argv[0] == nullptr)
        return 1;

    child_process::LaunchRequest invalid{};
    invalid.executable = "missing-child-process-contract-executable";
    invalid.correlation_key = "invalid";
    if (child_process::launch_or_focus(invalid).code
        != child_process::LaunchCode::invalid_request)
    {
        return 2;
    }

    const std::filesystem::path self = std::filesystem::absolute(argv[0]);
    child_process::LaunchRequest first{};
    first.executable = self;
    first.working_directory = self.parent_path();
    first.arguments = {"--epoch-child-contract", "30000", "17"};
    first.correlation_key = "contract:first";
    first.exclusive_group = "contract:exclusive";
    first.display_name = "Child Process Contract";
    first.window_mode = child_process::WindowMode::hidden;

    const child_process::LaunchResult launched =
        child_process::launch_or_focus(first);
    if (launched.code != child_process::LaunchCode::started
        || !launched.handle.valid())
    {
        return 3;
    }
    const auto running = child_process::snapshot(launched.handle);
    if (!running || !running->active()
        || running->correlation_key != first.correlation_key
        || running->platform_process_id == 0u
        || running->platform_window_id != 0u)
    {
        return 4;
    }

    const child_process::LaunchResult duplicate =
        child_process::launch_or_focus(first);
    if (duplicate.code != child_process::LaunchCode::focused_existing
        || duplicate.handle != launched.handle)
    {
        return 5;
    }

    child_process::LaunchRequest competing = first;
    competing.correlation_key = "contract:competing";
    const child_process::LaunchResult busy =
        child_process::launch_or_focus(competing);
    if (busy.code != child_process::LaunchCode::active_group_busy
        || busy.handle != launched.handle)
    {
        return 6;
    }

    const child_process::SupervisorMetrics activeMetrics =
        child_process::metrics();
    if (activeMetrics.launches != 1u
        || activeMetrics.active_processes != 1u
        || activeMetrics.peak_active_processes != 1u)
    {
        return 7;
    }

    std::stop_source cancellation{};
    (void)cancellation.request_stop();
    const child_process::WaitResult cancelled = child_process::wait(
        launched.handle,
        cancellation.get_token(),
        5'000'000'000ull);
    if (cancelled.code != child_process::WaitCode::cancelled
        || !cancelled.process
        || cancelled.process->active()
        || cancelled.process->state != child_process::ProcessState::exited
        || !cancelled.process->exit_code_valid)
    {
        return 8;
    }
    if (!child_process::release(launched.handle)
        || child_process::snapshot(launched.handle))
    {
        return 9;
    }

    child_process::LaunchRequest natural = first;
    natural.arguments = {"--epoch-child-contract", "20", "17"};
    natural.correlation_key = "contract:natural";
    const std::filesystem::path outputPath =
        self.parent_path() / "epoch-child-process-contract.log";
    std::error_code outputError{};
    std::filesystem::remove(outputPath, outputError);
    natural.merged_output_path = outputPath;
#if defined(_WIN32)
    CaptureInheritanceFixture capture{};
    if (!capture.prepare())
        return 15;
    natural.arguments = {"--epoch-capture-inheritance-contract",
        std::to_string(reinterpret_cast<std::uintptr_t>(capture.sentinel))};
#endif
    const child_process::LaunchResult second =
        child_process::launch_or_focus(natural);
#if defined(_WIN32)
    capture.restore_input();
#endif
    if (!second || !second.handle.valid()
        || second.handle == launched.handle)
    {
        return 10;
    }
    const child_process::WaitResult completed = child_process::wait(
        second.handle,
        {},
        5'000'000'000ull);
    if (!completed || !completed.process
        || completed.process->state != child_process::ProcessState::exited
        || !completed.process->exit_code_valid
        || completed.process->exit_code != 17)
    {
        return 11;
    }

    const child_process::SupervisorMetrics finalMetrics =
        child_process::metrics();
    if (finalMetrics.launches != 2u
        || finalMetrics.exits != 2u
        || finalMetrics.active_processes != 0u)
    {
        return 12;
    }
    if (!std::filesystem::is_regular_file(outputPath, outputError)
        || outputError)
    {
        return 13;
    }
#if defined(_WIN32)
    if (::WaitForSingleObject(capture.sentinel, 0u) != WAIT_TIMEOUT)
        return 16;
    {
        std::ifstream output{outputPath, std::ios::binary};
        const std::string bytes{
            std::istreambuf_iterator<char>{output}, std::istreambuf_iterator<char>{}};
        if (bytes != std::string{capture_stdout} + std::string{capture_stderr})
            return 17;
    }
#endif
    std::filesystem::remove(outputPath, outputError);
    return child_process::release(second.handle) ? 0 : 14;
}
#endif
