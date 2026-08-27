#include <chrono>
#include <filesystem>
#include <stop_token>
#include <string>
#include <thread>

import platform.child_process;

#if defined(EPOCH_PLATFORM_CHILD_PROCESS_CONTRACT_MAIN)
int main(int argc, char** argv)
{
    namespace child_process = epochengine::platform::child_process;

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
        || running->platform_process_id == 0u)
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
    const child_process::LaunchResult second =
        child_process::launch_or_focus(natural);
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
    std::filesystem::remove(outputPath, outputError);
    return child_process::release(second.handle) ? 0 : 14;
}
#endif
