/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
#include "core.format_text.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#endif

extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8);

namespace
{
    using Clock = std::chrono::system_clock;

    constexpr std::uint32_t kLogInfo = 1u;
    constexpr std::uint32_t kLogError = 3u;
    constexpr const char* kLogTag = "Epoch.RendererSmoke";

    void LogInfo(std::string_view message)
    {
        const std::string text{ message };
        core_log_write(kLogInfo, kLogTag, text.c_str());
    }

    void LogError(std::string_view message)
    {
        const std::string text{ message };
        core_log_write(kLogError, kLogTag, text.c_str());
    }

    template <typename... Args>
    void LogInfo(std::string_view fmt, Args&&... args)
    {
        LogInfo(epochengine::format_text(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void LogError(std::string_view fmt, Args&&... args)
    {
        LogError(epochengine::format_text(fmt, std::forward<Args>(args)...));
    }

    struct HarnessOptions
    {
        std::filesystem::path binary_path{};
        std::optional<std::string> backend_filter{};
        std::optional<std::string> mode_filter{};
        bool capture = false;
        bool legacy_only = false;
        bool show_help = false;
        int timeout_seconds = 45;
        int inter_scenario_delay_ms = 750;
        std::filesystem::path output_root = "Logs/smoke";
    };

    struct Scenario
    {
        std::string runtime;
        std::string backend;
        std::string mode;
    };

    struct ScenarioResult
    {
        Scenario scenario{};
        std::filesystem::path log_path{};
        std::filesystem::path manifest_path{};
        int exit_code = -1;
        bool timed_out = false;
        bool passed = false;
    };

    [[nodiscard]] std::string make_timestamp()
    {
        const auto now = Clock::now();
        const auto time_value = Clock::to_time_t(now);

        std::tm local_tm{};
#if defined(_WIN32)
        localtime_s(&local_tm, &time_value);
#else
        localtime_r(&time_value, &local_tm);
#endif

        std::ostringstream out;
        out << std::put_time(&local_tm, "%Y%m%d-%H%M%S");
        return out.str();
    }

    [[nodiscard]] std::string normalize_lower(std::string_view value)
    {
        std::string out(value.begin(), value.end());
        for (char& ch : out)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return out;
    }

    [[nodiscard]] std::vector<Scenario> build_matrix(const HarnessOptions& options)
    {
        static constexpr std::string_view backends[] = {
#if defined(_WIN32)
            "opengl", "sdl", "sfml", "raylib", "vulkan", "directx", "software"
#else
            "opengl", "sdl", "sfml", "raylib", "vulkan", "software"
#endif
        };

        static constexpr std::string_view modes[] = {
            "parented", "standalone"
        };

        std::vector<Scenario> scenarios;
        scenarios.reserve(48);

        for (std::string_view backend_name : backends)
        {
            if (options.backend_filter && normalize_lower(*options.backend_filter) != backend_name)
                continue;

            for (std::string_view mode_name : modes)
            {
                if (options.mode_filter && normalize_lower(*options.mode_filter) != mode_name)
                    continue;

                scenarios.push_back(Scenario{ "legacy", std::string(backend_name), std::string(mode_name) });

                if (!options.legacy_only)
                    scenarios.push_back(Scenario{ "epoch", std::string(backend_name), std::string(mode_name) });
            }
        }

        return scenarios;
    }

    [[nodiscard]] HarnessOptions parse_args(int argc, char** argv)
    {
        HarnessOptions options{};

#if defined(_WIN32)
        options.binary_path = "epoch.exe";
#else
        options.binary_path = "./epoch";
#endif

        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg{ argv[i] };

            auto read_value = [&](std::string_view name) -> std::string
                {
                    if (i + 1 >= argc)
                    {
                        LogError("Missing value for {}", name);
                        return {};
                    }
                    return std::string(argv[++i]);
                };

            if (arg == "--binary")
            {
                const auto value = read_value(arg);
                if (!value.empty())
                    options.binary_path = value;
            }
            else if (arg == "--backend")
            {
                const auto value = read_value(arg);
                if (!value.empty())
                    options.backend_filter = normalize_lower(value);
            }
            else if (arg == "--mode")
            {
                const auto value = read_value(arg);
                if (!value.empty())
                    options.mode_filter = normalize_lower(value);
            }
            else if (arg == "--capture")
            {
                options.capture = true;
            }
            else if (arg == "--legacy-only")
            {
                options.legacy_only = true;
            }
            else if (arg == "--timeout")
            {
                const auto value = read_value(arg);
                if (!value.empty())
                    options.timeout_seconds = (std::max)(5, std::stoi(value));
            }
            else if (arg == "--delay-ms")
            {
                const auto value = read_value(arg);
                if (!value.empty())
                    options.inter_scenario_delay_ms = std::clamp(std::stoi(value), 0, 10000);
            }
            else if (arg == "--out")
            {
                const auto value = read_value(arg);
                if (!value.empty())
                    options.output_root = value;
            }
            else if (arg == "--help" || arg == "-h")
            {
                options.show_help = true;
                LogInfo(
                    "Renderer smoke harness\n"
                    "  --binary <path>      Engine binary path (default: ./epoch or epoch.exe)\n"
                    "  --backend <name>     Limit to one backend\n"
                    "  --mode <name>        Limit to one mode (parented|standalone)\n"
                    "  --capture            Forward capture hint to runtime\n"
                    "  --legacy-only        Skip epoch-native mirror runs\n"
                    "  --timeout <seconds>  Per-scenario timeout (default 45)\n"
                    "  --delay-ms <ms>      Delay between scenarios (default 750)\n"
                    "  --out <path>         Output root for logs/manifests");
            }
        }

        return options;
    }

    [[nodiscard]] std::string make_runtime_args(const Scenario& scenario, bool capture)
    {
        std::ostringstream args;
        args
            << " --runtime " << scenario.runtime
            << " --renderer " << scenario.backend
            << " --window-mode " << scenario.mode
            << " --smoke"
            << " --scene smoke_matrix"
            << " --trace-menu-button0";

        if (capture)
            args << " --capture";

        return args.str();
    }

    void write_manifest(const ScenarioResult& result, const std::string& args)
    {
        std::ofstream out(result.manifest_path, std::ios::trunc);
        out << "{\n";
        out << "  \"runtime\": \"" << result.scenario.runtime << "\",\n";
        out << "  \"backend\": \"" << result.scenario.backend << "\",\n";
        out << "  \"mode\": \"" << result.scenario.mode << "\",\n";
        out << "  \"args\": \"" << args << "\",\n";
        out << "  \"log\": \"" << result.log_path.generic_string() << "\",\n";
        out << "  \"exit_code\": " << result.exit_code << ",\n";
        out << "  \"timed_out\": " << (result.timed_out ? "true" : "false") << ",\n";
        out << "  \"passed\": " << (result.passed ? "true" : "false") << "\n";
        out << "}\n";
    }

#if defined(_WIN32)
    [[nodiscard]] std::wstring widen(std::string_view value)
    {
        if (value.empty())
            return {};

        const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (needed <= 0)
            return {};

        std::wstring wide(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), needed);
        return wide;
    }

    [[nodiscard]] bool run_process_windows(
        const std::filesystem::path& binary_path,
        std::string_view args,
        const std::filesystem::path& log_path,
        int timeout_seconds,
        int& exit_code,
        bool& timed_out)
    {
        const std::wstring command_line =
            L"\"" + binary_path.wstring() + L"\"" + widen(args);

        const HANDLE log_file = CreateFileW(
            log_path.wstring().c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (log_file == INVALID_HANDLE_VALUE)
            return false;

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = log_file;
        startup.hStdError = log_file;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION process{};

        std::wstring mutable_command = command_line;
        const BOOL created = CreateProcessW(
            nullptr,
            mutable_command.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &startup,
            &process);

        CloseHandle(log_file);

        if (!created)
            return false;

        const DWORD wait_result = WaitForSingleObject(process.hProcess, static_cast<DWORD>(timeout_seconds * 1000));
        timed_out = (wait_result == WAIT_TIMEOUT);

        if (timed_out)
            TerminateProcess(process.hProcess, 124);

        DWORD proc_exit = 0;
        GetExitCodeProcess(process.hProcess, &proc_exit);
        exit_code = static_cast<int>(proc_exit);

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }
#else
    [[nodiscard]] bool run_process_posix(
        const std::filesystem::path& binary_path,
        std::string_view args,
        const std::filesystem::path& log_path,
        int timeout_seconds,
        int& exit_code,
        bool& timed_out)
    {
        std::ostringstream command;
        command
            << "timeout " << timeout_seconds << "s "
            << '"' << binary_path.string() << '"'
            << args
            << " > \"" << log_path.string() << "\" 2>&1";

        const int status = std::system(command.str().c_str());
        exit_code = status;
        timed_out = (status == 124 * 256) || (status == 124);
        return true;
    }
#endif

    [[nodiscard]] ScenarioResult run_scenario(
        const HarnessOptions& options,
        const Scenario& scenario,
        std::string_view stamp,
        int index)
    {
        ScenarioResult result{};
        result.scenario = scenario;

        std::ostringstream stem;
        stem << std::setfill('0') << std::setw(2) << index
             << '-' << scenario.runtime
             << '-' << scenario.backend
             << '-' << scenario.mode;

        const auto run_dir = options.output_root / stamp;
        std::filesystem::create_directories(run_dir);

        result.log_path = run_dir / (stem.str() + ".log");
        result.manifest_path = run_dir / (stem.str() + ".json");

        const std::string args = make_runtime_args(scenario, options.capture);

        bool launched = false;
#if defined(_WIN32)
        launched = run_process_windows(
            options.binary_path,
            args,
            result.log_path,
            options.timeout_seconds,
            result.exit_code,
            result.timed_out);
#else
        launched = run_process_posix(
            options.binary_path,
            args,
            result.log_path,
            options.timeout_seconds,
            result.exit_code,
            result.timed_out);
#endif

        if (!launched)
        {
            result.exit_code = -1;
            result.timed_out = false;
            result.passed = false;
            std::ofstream fail_log(result.log_path, std::ios::app);
            fail_log << "[Harness] Failed to launch process.\n";
        }
        else
        {
            result.passed = (!result.timed_out && result.exit_code == 0);
        }

        write_manifest(result, args);
        return result;
    }
}

int main(int argc, char** argv)
{
    const HarnessOptions options = parse_args(argc, argv);
    if (options.show_help)
        return 0;

    const auto scenarios = build_matrix(options);

    if (scenarios.empty())
    {
        LogError("No scenarios selected.");
        return 2;
    }

    if (!std::filesystem::exists(options.binary_path))
    {
        LogError("Binary not found: {}", options.binary_path.string());
        return 2;
    }

    const std::string stamp = make_timestamp();
    std::vector<ScenarioResult> results;
    results.reserve(scenarios.size());

    LogInfo("Running {} scenarios", scenarios.size());

    int index = 0;
    for (const Scenario& scenario : scenarios)
    {
        ++index;
        LogInfo("({}/{}) {} {} {}",
            index,
            scenarios.size(),
            scenario.runtime,
            scenario.backend,
            scenario.mode);

        results.push_back(run_scenario(options, scenario, stamp, index));

        if (options.inter_scenario_delay_ms > 0 && index < static_cast<int>(scenarios.size()))
        {
            LogInfo("Waiting {} ms before the next renderer scenario.", options.inter_scenario_delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(options.inter_scenario_delay_ms));
        }
    }

    int failures = 0;
    for (const auto& result : results)
    {
        if (!result.passed)
            ++failures;

        LogInfo("{} runtime={} backend={} mode={} exit={}{} log={}",
            result.passed ? "PASS" : "FAIL",
            result.scenario.runtime,
            result.scenario.backend,
            result.scenario.mode,
            result.exit_code,
            result.timed_out ? " timeout" : "",
            result.log_path.generic_string());
    }

    if (failures > 0)
    {
        LogError("Completed with {} failures.", failures);
        return 1;
    }

    LogInfo("All scenarios passed.");
    return 0;
}


