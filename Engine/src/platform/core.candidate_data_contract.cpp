#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif

import core.path;
import platform.child_process;

#if defined(EPOCH_CORE_CANDIDATE_DATA_CONTRACT_MAIN)
namespace
{
    namespace paths = epochengine::core::path;
    namespace children = epochengine::platform::child_process;
    namespace fs = std::filesystem;

    std::string utf8(const fs::path& value)
    {
        const auto bytes = value.u8string();
        return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
    }

    fs::path unicode_leaf()
    {
        return fs::path{ u8"candidate data_\u65e5\u672c_\u00e9_\U0001f680" };
    }

    struct Lookups final
    {
        fs::path runtime{ paths::runtime_root_dir() };
        fs::path engine_assets{ paths::engine_asset_dir() };
        fs::path example_assets{ paths::example_asset_dir() };
        fs::path includes{ paths::engine_include_dir() };
        fs::path executable{ paths::executable_path() };
        fs::path workspace{ paths::example_console_workspace_dir() };
        fs::path logs{ paths::log_output_dir() };
        fs::path captures{ paths::capture_output_dir() };

        bool immutable_equal(const Lookups& other) const
        {
            return runtime == other.runtime && engine_assets == other.engine_assets
                && example_assets == other.example_assets && includes == other.includes
                && executable == other.executable;
        }

        bool all_equal(const Lookups& other) const
        {
            return immutable_equal(other) && workspace == other.workspace
                && logs == other.logs && captures == other.captures;
        }
    };

    bool direct_refusals(const fs::path& fixture)
    {
        const auto valid = fixture / unicode_leaf();
        std::vector<fs::path> invalid{
            {}, "relative", fixture.root_path(), fixture / "absent",
            fixture / "ordinary-file", fixture / "." / unicode_leaf(),
            fixture / "other" / ".." / unicode_leaf(),
            fixture / "link" / "inside", valid / ""
        };
        auto nul = valid.native();
        nul.push_back(fs::path::value_type{});
        nul.append(fs::path{"suffix"}.native());
        invalid.emplace_back(nul);
#if defined(_WIN32)
        invalid.emplace_back(L"\\\\?\\" + valid.native());
        invalid.emplace_back(L"\\\\.\\" + valid.native());
        invalid.emplace_back(L"C:relative");
        invalid.emplace_back(valid.native() + L".");
        invalid.emplace_back(valid.native() + L" ");
        invalid.emplace_back(valid.native() + L":stream");
#endif
        for (const auto& value : invalid)
            if (paths::configure_candidate_data_root(value)
                || !paths::candidate_data_root().empty())
                return false;
        return paths::configure_candidate_data_root(valid)
            && paths::candidate_data_root() == valid
            && !paths::configure_candidate_data_root(fixture / "other")
            && !paths::configure_candidate_data_root(valid)
            && paths::candidate_data_root() == valid;
    }

    int run_child(int argc, char** argv)
    {
        if (argc < 2 || !argv[1])
            return 10;
        const std::string_view mode{ argv[1] };
        const auto fixture = fs::current_path();
        const auto before = Lookups{};
        const auto result = paths::preflight_candidate_data_arguments(argc, argv);
        if (mode == "--case=invalid")
            return !result.ok && paths::candidate_data_root().empty()
                && before.all_equal(Lookups{}) ? 0 : 11;
        if (mode == "--case=default")
            return result.ok && result.option_index == 0
                && paths::candidate_data_root().empty() && before.all_equal(Lookups{})
                && paths::log_output_dir() == fixture / "ambient-logs"
                && paths::capture_output_dir() == fixture / "ambient-captures" ? 0 : 12;
        if (mode == "--case=direct")
            return result.ok && result.option_index == 0 && direct_refusals(fixture)
                && before.immutable_equal(Lookups{}) ? 0 : 13;
        if (mode != "--case=valid" || !result.ok || result.option_index != 2)
            return 14;
        const auto expected = fixture / unicode_leaf();
        if (paths::candidate_data_root() != expected
            || paths::example_console_workspace_dir() != expected / "workspace"
            || paths::log_output_dir() != expected / "logs"
            || paths::capture_output_dir() != expected / "logs" / "captures"
            || !before.immutable_equal(Lookups{}) || !fs::is_empty(expected)
            || fs::current_path() != fixture)
            return 15;
        if (paths::configure_candidate_data_root(fixture / "other")
            || paths::configure_candidate_data_root(expected)
            || paths::candidate_data_root() != expected)
            return 16;
        return 0;
    }

    bool make_directory_link(const fs::path& link, const fs::path& target)
    {
#if defined(_WIN32)
        if (!fs::create_directory(link))
            return false;
        const HANDLE handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        struct Junction final
        {
            DWORD tag;
            WORD length;
            WORD reserved;
            WORD substitute_offset;
            WORD substitute_length;
            WORD print_offset;
            WORD print_length;
            wchar_t names[4096];
        } buffer{};
        const std::wstring substitute = L"\\??\\" + target.native();
        const std::wstring printable = target.native();
        const auto count = substitute.size() + 1u + printable.size() + 1u;
        if (count >= 4096u)
        {
            CloseHandle(handle);
            return false;
        }
        buffer.tag = IO_REPARSE_TAG_MOUNT_POINT;
        buffer.substitute_length = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
        buffer.print_offset = static_cast<WORD>((substitute.size() + 1u) * sizeof(wchar_t));
        buffer.print_length = static_cast<WORD>(printable.size() * sizeof(wchar_t));
        std::memcpy(buffer.names, substitute.c_str(), (substitute.size() + 1u) * sizeof(wchar_t));
        std::memcpy(reinterpret_cast<std::byte*>(buffer.names) + buffer.print_offset,
            printable.c_str(), (printable.size() + 1u) * sizeof(wchar_t));
        buffer.length = static_cast<WORD>(8u + count * sizeof(wchar_t));
        DWORD returned{};
        const bool result = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &buffer,
            8u + buffer.length, nullptr, 0, &returned, nullptr) != FALSE;
        CloseHandle(handle);
        return result;
#else
        std::error_code error;
        fs::create_directory_symlink(target, link, error);
        return !error;
#endif
    }

    bool host_workspace_alias_contract(const fs::path& fixture)
    {
        const auto host = fixture / "physical-host";
        const auto alias = fixture / "EpochEngine";
        const auto executable = host / "x64" / "Release" / "EpochEditor.exe";
        const auto workspace = host / "Engine" / "examples" / "EpochEditor" / "workspace";
        for (const auto& relative : {
                fs::path{"Engine/CMakeLists.txt"},
                fs::path{"Engine/include/epoch.engine.hpp"},
                fs::path{"Engine/src/epoch.main.cpp"},
                fs::path{"Engine/examples/EpochEngine/EpochEngine.vcxproj"},
                fs::path{"x64/Release/EpochEditor.exe"}})
        {
            fs::create_directories((host / relative).parent_path());
            std::ofstream file{host / relative, std::ios::binary};
            file << "host path fixture";
            if (!file) return false;
        }
        fs::create_directories(workspace);
        if (!make_directory_link(alias, host))
            return false;

        const auto expected = fs::canonical(workspace);
        const auto aliasExecutable = alias / "x64" / "Release" / "EpochEditor.exe";
        const auto throughAlias = paths::example_console_workspace_dir(aliasExecutable);
        if (throughAlias != expected
            || throughAlias != fs::canonical(throughAlias)
            || paths::example_console_workspace_dir(executable) != expected
            || aliasExecutable == fs::canonical(aliasExecutable)
            || !paths::example_console_workspace_dir(fs::path{}).empty()
            || !paths::example_console_workspace_dir("relative/EpochEditor.exe").empty()
            || !paths::example_console_workspace_dir(host / "missing.exe").empty()
            || !paths::example_console_workspace_dir(host).empty())
            return false;

        // Host launch alias resolution must not turn candidate-owned paths
        // into trusted aliases. The production candidate admission is tested
        // against this interior redirect by its own fresh subprocess below.
        if (!make_directory_link(workspace / "candidate-output", fixture / "target"))
            return false;

        const auto installed = fixture / "physical-install";
        const auto installAlias = fixture / "InstalledEpoch";
        fs::create_directories(installed / "workspace");
        {
            std::ofstream file{installed / "EpochEditor.exe", std::ios::binary};
            file << "packaged host path fixture";
            if (!file) return false;
        }
        if (!make_directory_link(installAlias, installed))
            return false;
        return paths::example_console_workspace_dir(installAlias / "EpochEditor.exe")
                == fs::canonical(installed / "workspace")
            && paths::candidate_data_root().empty();
    }

    bool run_case(const fs::path& fixture, std::vector<std::string> arguments, unsigned number)
    {
        children::LaunchRequest request;
        request.executable = paths::executable_path();
        request.working_directory = fixture;
        request.arguments = std::move(arguments);
        request.correlation_key = "core-candidate-data-" + std::to_string(number);
        request.window_mode = children::WindowMode::hidden;
        request.merged_output_path = fixture / ("child-" + std::to_string(number) + ".log");
        request.disconnect_standard_input = true;
        request.environment = std::vector<children::EnvironmentVariable>{
            { "EPOCH_LOG_DIR", utf8(fixture / "ambient-logs") },
            { "EPOCH_CAPTURE_DIR", utf8(fixture / "ambient-captures") }
        };
#if defined(_WIN32)
        std::wstring windows(32768u, L'\0');
        const auto count = GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
        if (count == 0u || count >= windows.size())
            return false;
        windows.resize(count);
        request.environment->push_back({ "SystemRoot", utf8(fs::path{ windows }) });
#endif
        const auto launched = children::launch_or_focus(request);
        if (!launched.owns_new_process())
            return false;
        auto completed = children::wait(launched.handle, {}, 10'000'000'000ull);
        if (completed.code != children::WaitCode::exited)
        {
            (void)children::stop(launched.handle, children::StopMode::force);
            completed = children::wait(launched.handle, {}, 5'000'000'000ull);
        }
        const bool passed = launched.code == children::LaunchCode::started
            && completed.code == children::WaitCode::exited && completed.process
            && completed.process->exit_code_valid && completed.process->exit_code == 0;
        const bool released = children::release(launched.handle);
        return passed && released;
    }

    int run_parent()
    {
        const auto temporary = fs::temp_directory_path();
        fs::path fixture;
        const auto token = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 16u && fixture.empty(); ++attempt)
        {
            const auto proposed = temporary / ("epoch-candidate-data-"
                + std::to_string(token) + "-" + std::to_string(attempt));
            if (fs::create_directory(proposed))
                fixture = proposed;
        }
        if (fixture.empty())
            return 20;
        const auto valid = fixture / unicode_leaf();
        fs::create_directory(valid);
        fs::create_directory(fixture / "other");
        fs::create_directories(fixture / "target" / "inside");
        { std::ofstream file{ fixture / "ordinary-file" }; file << "fixture"; }
        if (!make_directory_link(fixture / "link", fixture / "target"))
            return 21;
        if (!host_workspace_alias_contract(fixture))
            return 22;
        const std::string option = "--candidate-data-root";
        const auto root = utf8(valid);
        std::vector<std::vector<std::string>> cases{
            { "--case=default" },
            { "--case=valid", option, root },
            { "--case=direct" },
            { "--case=invalid", option },
            { "--case=invalid", option, "" },
            { "--case=invalid", option, "relative" },
            { "--case=invalid", option, utf8(fixture / "absent") },
            { "--case=invalid", option, utf8(fixture.root_path()) },
            { "--case=invalid", option, utf8(fixture / "ordinary-file") },
            { "--case=invalid", option, utf8(fixture / "link" / "inside") },
            { "--case=invalid", option, root, option, root },
            { "--case=invalid", option + "=" + root },
            { "--case=invalid", option + "=" },
            { "--case=invalid", option, root, option + "=" + root },
            { "--case=invalid", option + "-unexpected", root },
            { "--case=invalid", option, utf8(fixture / "." / unicode_leaf()) },
            { "--case=invalid", option, utf8(fixture / "physical-host" / "Engine"
                / "examples" / "EpochEditor" / "workspace" / "candidate-output" / "inside") }
        };
        for (std::size_t index = 0u; index < cases.size(); ++index)
            if (!run_case(fixture, std::move(cases[index]), static_cast<unsigned>(index))
                || !fs::is_empty(valid) || !fs::is_empty(fixture / "other")
                || fs::exists(fixture / "absent") || fs::exists(fixture / "ambient-logs")
                || fs::exists(fixture / "ambient-captures"))
                return 30 + static_cast<int>(index);
        // Only this fresh, explicitly owned fixture is removed. A failed case
        // retains it for inspection; no child may remain active on success.
        fs::remove_all(fixture);
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc > 1)
            return run_child(argc, argv);
        return run_parent();
    }
    catch (...)
    {
        return 90;
    }
}
#endif
