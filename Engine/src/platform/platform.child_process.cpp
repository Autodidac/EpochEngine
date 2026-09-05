module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

module platform.child_process;

import core.sha256;

namespace epochengine::platform::child_process
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        constexpr std::uint64_t maximum_executable_bytes = 512u * 1024u * 1024u;

        [[nodiscard]] bool valid_executable_path(const std::filesystem::path& path)
        {
            const auto& native = path.native();
            return !native.empty() && native.size() <= 32'767u
                && native.find(std::filesystem::path::value_type{})
                    == std::filesystem::path::string_type::npos;
        }

#if defined(_WIN32)
        // The image and each directory component stay non-inheritable and
        // deny replacement until CreateProcess/ResumeThread have completed.
        // These short-lived file locks are not a candidate execution sandbox.
        struct ExecutableReadLock final
        {
            HANDLE image{INVALID_HANDLE_VALUE};
            std::array<HANDLE, 128u> directories{};
            std::size_t directory_count{};
            std::filesystem::path canonical_path{};
            std::optional<ExecutableIdentity> identity{};

            ExecutableReadLock() = default;
            ExecutableReadLock(const ExecutableReadLock&) = delete;
            ExecutableReadLock& operator=(const ExecutableReadLock&) = delete;

            ~ExecutableReadLock() noexcept
            {
                if (image != INVALID_HANDLE_VALUE)
                    (void)::CloseHandle(image);
                while (directory_count != 0u)
                    (void)::CloseHandle(directories[--directory_count]);
            }

            [[nodiscard]] bool lock_directory(const std::filesystem::path& path)
            {
                if (directory_count == directories.size())
                    return false;
                const HANDLE handle = ::CreateFileW(path.c_str(),
                    FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
                if (handle == INVALID_HANDLE_VALUE)
                    return false;
                directories[directory_count++] = handle;
                BY_HANDLE_FILE_INFORMATION information{};
                return ::GetFileType(handle) == FILE_TYPE_DISK
                    && ::GetFileInformationByHandle(handle, &information) != FALSE
                    && (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u
                    && (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0u;
            }

            [[nodiscard]] bool open(const std::filesystem::path& path)
            {
                if (!valid_executable_path(path))
                    return false;
                std::error_code error{};
                const auto absolute = std::filesystem::absolute(path, error).lexically_normal();
                if (error || !absolute.is_absolute() || absolute.filename().empty())
                    return false;

                // Lock root-to-leaf before opening the image, so a renamed
                // ancestor cannot redirect the subsequent path-based OS launch.
                auto directory = absolute.root_path();
                if (!lock_directory(directory))
                    return false;
                for (const auto& component : absolute.parent_path().relative_path())
                {
                    directory /= component;
                    if (!lock_directory(directory))
                        return false;
                }
                image = ::CreateFileW(absolute.c_str(), GENERIC_READ,
                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                if (image == INVALID_HANDLE_VALUE || ::GetFileType(image) != FILE_TYPE_DISK)
                    return false;
                BY_HANDLE_FILE_INFORMATION information{};
                if (::GetFileInformationByHandle(image, &information) == FALSE
                    || (information.dwFileAttributes
                        & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0u)
                    return false;
                // Attribute writes are not excluded by Win32 sharing flags.
                // Recheck after the entire chain is pinned: each ordinary
                // directory now contains its next no-delete child, and cannot
                // become the empty directory required for a new reparse point.
                for (std::size_t index = 0u; index < directory_count; ++index)
                {
                    BY_HANDLE_FILE_INFORMATION directoryInformation{};
                    if (::GetFileType(directories[index]) != FILE_TYPE_DISK
                        || ::GetFileInformationByHandle(
                            directories[index], &directoryInformation) == FALSE
                        || (directoryInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u
                        || (directoryInformation.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
                        return false;
                }
                const std::uint64_t size = (static_cast<std::uint64_t>(
                    information.nFileSizeHigh) << 32u) | information.nFileSizeLow;
                if (size == 0u || size > maximum_executable_bytes)
                    return false;

                std::wstring finalPath(32'768u, L'\0');
                const DWORD pathLength = ::GetFinalPathNameByHandleW(image,
                    finalPath.data(), static_cast<DWORD>(finalPath.size()),
                    FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                if (pathLength == 0u || pathLength >= finalPath.size())
                    return false;
                finalPath.resize(pathLength);
                if (finalPath.starts_with(L"\\\\?\\UNC\\"))
                    finalPath = L"\\\\" + finalPath.substr(8u);
                else if (finalPath.starts_with(L"\\\\?\\"))
                    finalPath.erase(0u, 4u);
                canonical_path = std::filesystem::path{finalPath}.lexically_normal();
                if (!canonical_path.is_absolute())
                    return false;

                core::sha256::Hasher hasher{};
                std::array<std::uint8_t, 64u * 1024u> buffer{};
                std::uint64_t consumed{};
                while (consumed < size)
                {
                    const DWORD wanted = static_cast<DWORD>((std::min)(
                        size - consumed, static_cast<std::uint64_t>(buffer.size())));
                    DWORD received{};
                    if (::ReadFile(image, buffer.data(), wanted, &received, nullptr) == FALSE
                        || received == 0u || received > wanted)
                        return false;
                    hasher.update(std::span<const std::uint8_t>{buffer.data(), received});
                    consumed += received;
                }
                DWORD extra{};
                if (::ReadFile(image, buffer.data(), 1u, &extra, nullptr) == FALSE || extra != 0u)
                    return false;
                identity = ExecutableIdentity{size, core::sha256::hex(hasher.finish())};
                return identity->valid();
            }
        };
#else
        [[nodiscard]] std::optional<ExecutableIdentity> inspect_regular_executable(
            const std::filesystem::path& path)
        {
            if (!valid_executable_path(path))
                return std::nullopt;
            std::error_code error{};
            if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path, error))
                || error)
                return std::nullopt;
            const auto size = std::filesystem::file_size(path, error);
            if (error || size == 0u || size > maximum_executable_bytes)
                return std::nullopt;
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return std::nullopt;
            core::sha256::Hasher hasher{};
            std::array<char, 64u * 1024u> buffer{};
            std::uint64_t consumed{};
            while (consumed < size)
            {
                const auto wanted = static_cast<std::streamsize>((std::min)(
                    size - consumed, static_cast<std::uint64_t>(buffer.size())));
                input.read(buffer.data(), wanted);
                if (input.gcount() != wanted)
                    return std::nullopt;
                hasher.update(std::string_view{buffer.data(), static_cast<std::size_t>(wanted)});
                consumed += static_cast<std::uint64_t>(wanted);
            }
            if (input.peek() != std::char_traits<char>::eof() || input.bad()
                || std::filesystem::file_size(path, error) != size || error)
                return std::nullopt;
            ExecutableIdentity result{size, core::sha256::hex(hasher.finish())};
            return result.valid() ? std::optional{std::move(result)} : std::nullopt;
        }
#endif

        struct ProcessSlot final
        {
            bool occupied{};
            std::uint32_t generation{};
            ProcessState state{ProcessState::idle};
            std::filesystem::path executable{};
            std::optional<ExecutableIdentity> verified_executable{};
            std::string correlation_key{};
            std::string exclusive_group{};
            std::string display_name{};
            WindowMode window_mode{WindowMode::normal};
            std::uint64_t started_tick_ns{};
            std::uint64_t finished_tick_ns{};
            std::int32_t exit_code{};
            bool exit_code_valid{};
            bool focus_pending{};
            std::uint64_t focus_deadline_ns{};
            std::string message{};
#if defined(_WIN32)
            HANDLE process{};
            HANDLE job{};
            DWORD process_id{};
#else
            pid_t process_id{-1};
#endif
        };

        struct SupervisorStorage final
        {
            std::mutex mutex{};
            std::array<ProcessSlot, maximum_processes> slots{};
            SupervisorMetrics metrics{};

            ~SupervisorStorage() noexcept
            {
#if defined(_WIN32)
                for (ProcessSlot& slot : slots)
                {
                    if (slot.process != nullptr)
                    {
                        ::CloseHandle(slot.process);
                        slot.process = nullptr;
                    }
                    if (slot.job != nullptr)
                    {
                        ::CloseHandle(slot.job);
                        slot.job = nullptr;
                    }
                }
#endif
            }
        };

        [[nodiscard]] SupervisorStorage& storage() noexcept
        {
            static SupervisorStorage value{};
            return value;
        }

        [[nodiscard]] std::uint64_t now_ns() noexcept
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now().time_since_epoch()).count());
        }

        [[nodiscard]] bool active(ProcessState state) noexcept
        {
            return state == ProcessState::starting
                || state == ProcessState::running
                || state == ProcessState::stop_requested;
        }

        void revise(SupervisorStorage& value) noexcept
        {
            if (value.metrics.revision
                != (std::numeric_limits<std::uint64_t>::max)())
            {
                ++value.metrics.revision;
            }
        }

        [[nodiscard]] std::uint32_t next_generation(
            std::uint32_t current) noexcept
        {
            return current == (std::numeric_limits<std::uint32_t>::max)()
                ? 1u
                : current + 1u;
        }

        [[nodiscard]] ProcessSlot* resolve_slot(
            SupervisorStorage& value,
            ProcessHandle handle) noexcept
        {
            if (!handle.valid() || handle.slot >= value.slots.size())
                return nullptr;
            ProcessSlot& slot = value.slots[handle.slot];
            return slot.occupied && slot.generation == handle.generation
                ? &slot
                : nullptr;
        }

        [[nodiscard]] ProcessHandle handle_for(
            const SupervisorStorage& value,
            const ProcessSlot& slot) noexcept
        {
            const auto* first = value.slots.data();
            const auto* current = &slot;
            return {
                static_cast<std::uint32_t>(current - first),
                slot.generation};
        }

        void finish_slot(
            SupervisorStorage& value,
            ProcessSlot& slot,
            ProcessState terminal,
            std::int32_t exitCode,
            bool exitCodeValid,
            std::string message)
        {
            if (active(slot.state) && value.metrics.active_processes > 0u)
                --value.metrics.active_processes;
            slot.state = terminal;
            slot.finished_tick_ns = now_ns();
            slot.exit_code = exitCode;
            slot.exit_code_valid = exitCodeValid;
            slot.focus_pending = false;
            slot.focus_deadline_ns = 0u;
            slot.message = std::move(message);
            if (terminal == ProcessState::exited)
                ++value.metrics.exits;
#if defined(_WIN32)
            if (slot.process != nullptr)
            {
                ::CloseHandle(slot.process);
                slot.process = nullptr;
            }
            if (slot.job != nullptr)
            {
                ::CloseHandle(slot.job);
                slot.job = nullptr;
            }
#else
            slot.process_id = -1;
#endif
            revise(value);
        }

#if defined(_WIN32)
        struct CapturedStartupResources final
        {
            CapturedStartupResources() = default;
            CapturedStartupResources(const CapturedStartupResources&) = delete;
            CapturedStartupResources& operator=(const CapturedStartupResources&) = delete;

            HANDLE input{INVALID_HANDLE_VALUE};
            HANDLE output{INVALID_HANDLE_VALUE};
            LPPROC_THREAD_ATTRIBUTE_LIST attributes{};
            bool attributes_initialized{};

            ~CapturedStartupResources() noexcept
            {
                if (attributes != nullptr)
                {
                    if (attributes_initialized)
                        ::DeleteProcThreadAttributeList(attributes);
                    (void)::HeapFree(::GetProcessHeap(), 0u, attributes);
                }
                if (input != INVALID_HANDLE_VALUE && input != nullptr)
                    (void)::CloseHandle(input);
                if (output != INVALID_HANDLE_VALUE && output != nullptr)
                    (void)::CloseHandle(output);
            }
        };

        struct WindowSearch final
        {
            DWORD process_id{};
            HWND window{};
        };

        BOOL CALLBACK find_process_window(HWND window, LPARAM data) noexcept
        {
            auto* search = reinterpret_cast<WindowSearch*>(data);
            if (search == nullptr || search->window != nullptr
                || ::IsWindowVisible(window) == FALSE
                || ::GetWindow(window, GW_OWNER) != nullptr)
            {
                return TRUE;
            }
            DWORD processId{};
            (void)::GetWindowThreadProcessId(window, &processId);
            if (processId == search->process_id)
            {
                search->window = window;
                return FALSE;
            }
            return TRUE;
        }

        BOOL CALLBACK close_process_window(HWND window, LPARAM data) noexcept
        {
            auto* processId = reinterpret_cast<DWORD*>(data);
            if (processId == nullptr)
                return TRUE;
            DWORD candidate{};
            (void)::GetWindowThreadProcessId(window, &candidate);
            if (candidate == *processId && ::GetWindow(window, GW_OWNER) == nullptr)
                (void)::PostMessageW(window, WM_CLOSE, 0u, 0);
            return TRUE;
        }

        [[nodiscard]] std::uint64_t process_window_id(
            const DWORD processId) noexcept
        {
            if (processId == 0u)
                return 0u;
            WindowSearch search{.process_id = processId};
            (void)::EnumWindows(
                find_process_window,
                reinterpret_cast<LPARAM>(&search));
            return static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(search.window));
        }

        [[nodiscard]] FocusCode focus_slot(ProcessSlot& slot) noexcept
        {
            if (!active(slot.state))
                return FocusCode::not_running;
            if (slot.window_mode == WindowMode::hidden)
                return FocusCode::unsupported;

            WindowSearch search{.process_id = slot.process_id};
            (void)::EnumWindows(find_process_window, reinterpret_cast<LPARAM>(&search));
            if (search.window == nullptr)
            {
                slot.focus_pending = true;
                if (slot.focus_deadline_ns == 0u)
                    slot.focus_deadline_ns = now_ns() + 5'000'000'000ull;
                return FocusCode::pending;
            }

            (void)::ShowWindow(search.window, SW_SHOWNORMAL);
            (void)::BringWindowToTop(search.window);
            const bool foreground = ::SetForegroundWindow(search.window) != FALSE;
            (void)::SetActiveWindow(search.window);
            slot.focus_pending = !foreground;
            if (foreground)
            {
                slot.focus_deadline_ns = 0u;
                return FocusCode::focused;
            }
            return FocusCode::failed;
        }

        [[nodiscard]] std::optional<std::wstring> utf8_to_wide(
            std::string_view text)
        {
            if (text.empty())
                return std::wstring{};
            if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
                return std::nullopt;
            const int required = ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0);
            if (required <= 0)
                return std::nullopt;
            std::wstring result(static_cast<std::size_t>(required), L'\0');
            if (::MultiByteToWideChar(
                    CP_UTF8,
                    MB_ERR_INVALID_CHARS,
                    text.data(),
                    static_cast<int>(text.size()),
                    result.data(),
                    required) != required)
            {
                return std::nullopt;
            }
            return result;
        }

        [[nodiscard]] std::wstring quote_windows_argument(
            std::wstring_view value)
        {
            if (!value.empty()
                && value.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
            {
                return std::wstring{value};
            }

            std::wstring result{L'"'};
            std::size_t slashes{};
            for (const wchar_t character : value)
            {
                if (character == L'\\')
                {
                    ++slashes;
                    continue;
                }
                if (character == L'"')
                {
                    result.append(slashes * 2u + 1u, L'\\');
                    result.push_back(L'"');
                    slashes = 0u;
                    continue;
                }
                result.append(slashes, L'\\');
                slashes = 0u;
                result.push_back(character);
            }
            result.append(slashes * 2u, L'\\');
            result.push_back(L'"');
            return result;
        }

        [[nodiscard]] bool spawn_process(
            ProcessSlot& slot,
            const LaunchRequest& request,
            std::string& error)
        {
            std::vector<std::wstring> arguments{};
            arguments.reserve(request.arguments.size());
            for (const std::string& argument : request.arguments)
            {
                auto wide = utf8_to_wide(argument);
                if (!wide)
                {
                    error = "Process argument is not valid UTF-8.";
                    return false;
                }
                arguments.emplace_back(std::move(*wide));
            }

            std::wstring commandLine = quote_windows_argument(slot.executable.wstring());
            for (const std::wstring& argument : arguments)
            {
                commandLine.push_back(L' ');
                commandLine += quote_windows_argument(argument);
            }

            HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
            if (job == nullptr)
            {
                error = "CreateJobObjectW failed with error "
                    + std::to_string(static_cast<unsigned long>(::GetLastError()))
                    + ".";
                return false;
            }
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags =
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (::SetInformationJobObject(
                    job,
                    JobObjectExtendedLimitInformation,
                    &limits,
                    sizeof(limits)) == FALSE)
            {
                const DWORD failure = ::GetLastError();
                ::CloseHandle(job);
                error = "SetInformationJobObject failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }

            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(STARTUPINFOW);
            startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
            startup.StartupInfo.wShowWindow = request.window_mode == WindowMode::hidden
                ? SW_HIDE
                : SW_SHOWNORMAL;
            CapturedStartupResources captured{};
            std::array<HANDLE, 2u> inheritedHandles{};
            if (!request.merged_output_path.empty())
            {
                SECURITY_ATTRIBUTES security{};
                security.nLength = sizeof(security);
                security.bInheritHandle = TRUE;
                captured.output = ::CreateFileW(
                    request.merged_output_path.c_str(),
                    GENERIC_WRITE,
                    FILE_SHARE_READ,
                    &security,
                    request.append_output ? OPEN_ALWAYS : CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);
                if (captured.output == INVALID_HANDLE_VALUE)
                {
                    const DWORD failure = ::GetLastError();
                    ::CloseHandle(job);
                    error = "Output capture could not be opened: Win32 error "
                        + std::to_string(static_cast<unsigned long>(failure)) + ".";
                    return false;
                }
                if (request.append_output)
                {
                    LARGE_INTEGER end{};
                    if (::SetFilePointerEx(captured.output, end, nullptr, FILE_END) == FALSE)
                    {
                        const DWORD failure = ::GetLastError();
                        ::CloseHandle(job);
                        error = "Output capture could not seek to the end: Win32 error "
                            + std::to_string(static_cast<unsigned long>(failure)) + ".";
                        return false;
                    }
                }
                const HANDLE parentInput = ::GetStdHandle(STD_INPUT_HANDLE);
                if (parentInput == nullptr || parentInput == INVALID_HANDLE_VALUE)
                {
                    captured.input = ::CreateFileW(
                        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                }
                else if (::DuplicateHandle(
                        ::GetCurrentProcess(), parentInput,
                        ::GetCurrentProcess(), &captured.input,
                        0u, TRUE, DUPLICATE_SAME_ACCESS) == FALSE)
                {
                    captured.input = INVALID_HANDLE_VALUE;
                }
                if (captured.input == INVALID_HANDLE_VALUE
                    || captured.input == nullptr)
                {
                    const DWORD failure = ::GetLastError();
                    ::CloseHandle(job);
                    error = "Captured process stdin could not be prepared: Win32 error "
                        + std::to_string(static_cast<unsigned long>(failure)) + ".";
                    return false;
                }

                SIZE_T attributeBytes{};
                const BOOL measured = ::InitializeProcThreadAttributeList(
                    nullptr, 1u, 0u, &attributeBytes);
                const DWORD measureFailure = ::GetLastError();
                if (measured != FALSE || attributeBytes == 0u
                    || measureFailure != ERROR_INSUFFICIENT_BUFFER)
                {
                    ::CloseHandle(job);
                    error = "Captured process handle list could not be sized: Win32 error "
                        + std::to_string(static_cast<unsigned long>(measureFailure)) + ".";
                    return false;
                }
                captured.attributes = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
                    ::HeapAlloc(::GetProcessHeap(), 0u, attributeBytes));
                if (captured.attributes == nullptr)
                {
                    ::CloseHandle(job);
                    error = "Captured process handle list allocation failed.";
                    return false;
                }
                if (::InitializeProcThreadAttributeList(
                        captured.attributes, 1u, 0u, &attributeBytes) == FALSE)
                {
                    const DWORD failure = ::GetLastError();
                    ::CloseHandle(job);
                    error = "Captured process handle list initialization failed: Win32 error "
                        + std::to_string(static_cast<unsigned long>(failure)) + ".";
                    return false;
                }
                captured.attributes_initialized = true;
                inheritedHandles = {captured.input, captured.output};
                if (::UpdateProcThreadAttribute(
                        captured.attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                        inheritedHandles.data(), inheritedHandles.size() * sizeof(HANDLE),
                        nullptr, nullptr) == FALSE)
                {
                    const DWORD failure = ::GetLastError();
                    ::CloseHandle(job);
                    error = "Captured process handle allowlist failed: Win32 error "
                        + std::to_string(static_cast<unsigned long>(failure)) + ".";
                    return false;
                }
                // TRUE inheritance is required by HANDLE_LIST, but only these
                // standard-stream handles are admitted, never unrelated handles.
                startup.StartupInfo.cb = sizeof(startup);
                startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
                startup.StartupInfo.hStdInput = captured.input;
                startup.StartupInfo.hStdOutput = captured.output;
                startup.StartupInfo.hStdError = captured.output;
                startup.lpAttributeList = captured.attributes;
            }
            PROCESS_INFORMATION process{};
            DWORD flags = CREATE_SUSPENDED;
            if (request.window_mode == WindowMode::hidden)
                flags |= CREATE_NO_WINDOW;
            if (captured.attributes_initialized)
                flags |= EXTENDED_STARTUPINFO_PRESENT;
            const std::wstring working = request.working_directory.wstring();
            const BOOL created = ::CreateProcessW(
                slot.executable.c_str(),
                commandLine.data(),
                nullptr,
                nullptr,
                captured.attributes_initialized,
                flags,
                nullptr,
                working.empty() ? nullptr : working.c_str(),
                &startup.StartupInfo,
                &process);
            if (created == FALSE)
            {
                const DWORD failure = ::GetLastError();
                ::CloseHandle(job);
                error = "CreateProcessW failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }

            if (::AssignProcessToJobObject(job, process.hProcess) == FALSE)
            {
                const DWORD failure = ::GetLastError();
                (void)::TerminateProcess(process.hProcess, 1u);
                ::CloseHandle(process.hThread);
                ::CloseHandle(process.hProcess);
                ::CloseHandle(job);
                error = "AssignProcessToJobObject failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }
            if (::ResumeThread(process.hThread) == static_cast<DWORD>(-1))
            {
                const DWORD failure = ::GetLastError();
                (void)::TerminateJobObject(job, 1u);
                ::CloseHandle(process.hThread);
                ::CloseHandle(process.hProcess);
                ::CloseHandle(job);
                error = "ResumeThread failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }

            ::CloseHandle(process.hThread);
            slot.process = process.hProcess;
            slot.job = job;
            slot.process_id = process.dwProcessId;
            if (request.window_mode == WindowMode::normal)
            {
                (void)::AllowSetForegroundWindow(process.dwProcessId);
                slot.focus_pending = true;
                slot.focus_deadline_ns = now_ns() + 5'000'000'000ull;
            }
            return true;
        }
#else
        [[nodiscard]] FocusCode focus_slot(ProcessSlot&) noexcept
        {
            return FocusCode::unsupported;
        }

        [[nodiscard]] bool spawn_process(
            ProcessSlot& slot,
            const LaunchRequest& request,
            std::string& error)
        {
            std::vector<std::string> argumentStorage{};
            argumentStorage.reserve(request.arguments.size() + 1u);
            argumentStorage.emplace_back(slot.executable.string());
            argumentStorage.insert(
                argumentStorage.end(),
                request.arguments.begin(),
                request.arguments.end());
            std::vector<char*> arguments{};
            arguments.reserve(argumentStorage.size() + 1u);
            for (std::string& argument : argumentStorage)
                arguments.push_back(argument.data());
            arguments.push_back(nullptr);

            int errorPipe[2]{-1, -1};
            if (::pipe(errorPipe) != 0)
            {
                error = "pipe failed with errno " + std::to_string(errno) + ".";
                return false;
            }
            (void)::fcntl(errorPipe[1], F_SETFD, FD_CLOEXEC);

            int nullFile = -1;
            if (request.window_mode == WindowMode::hidden)
                nullFile = ::open("/dev/null", O_RDWR);
            int outputFile = -1;
            if (!request.merged_output_path.empty())
            {
                const int flags = O_CREAT | O_WRONLY
                    | (request.append_output ? O_APPEND : O_TRUNC);
                outputFile = ::open(
                    request.merged_output_path.c_str(), flags, 0644);
                if (outputFile < 0)
                {
                    const int failure = errno;
                    (void)::close(errorPipe[0]);
                    (void)::close(errorPipe[1]);
                    if (nullFile >= 0)
                        (void)::close(nullFile);
                    error = "Output capture could not be opened: errno "
                        + std::to_string(failure) + ".";
                    return false;
                }
            }

            const pid_t child = ::fork();
            if (child == 0)
            {
                (void)::close(errorPipe[0]);
                (void)::setpgid(0, 0);
                if (nullFile >= 0)
                {
                    (void)::dup2(nullFile, STDIN_FILENO);
                    if (outputFile < 0)
                    {
                        (void)::dup2(nullFile, STDOUT_FILENO);
                        (void)::dup2(nullFile, STDERR_FILENO);
                    }
                    if (nullFile > STDERR_FILENO)
                        (void)::close(nullFile);
                }
                if (outputFile >= 0)
                {
                    (void)::dup2(outputFile, STDOUT_FILENO);
                    (void)::dup2(outputFile, STDERR_FILENO);
                    if (outputFile > STDERR_FILENO)
                        (void)::close(outputFile);
                }
                if (::chdir(request.working_directory.c_str()) != 0)
                {
                    const int failure = errno;
                    (void)::write(errorPipe[1], &failure, sizeof(failure));
                    ::_exit(126);
                }
                ::execv(slot.executable.c_str(), arguments.data());
                const int failure = errno;
                (void)::write(errorPipe[1], &failure, sizeof(failure));
                ::_exit(127);
            }

            (void)::close(errorPipe[1]);
            if (nullFile >= 0)
                (void)::close(nullFile);
            if (outputFile >= 0)
                (void)::close(outputFile);
            if (child < 0)
            {
                const int failure = errno;
                (void)::close(errorPipe[0]);
                error = "fork failed with errno " + std::to_string(failure) + ".";
                return false;
            }

            int childFailure{};
            ssize_t received{};
            do
            {
                received = ::read(errorPipe[0], &childFailure, sizeof(childFailure));
            } while (received < 0 && errno == EINTR);
            (void)::close(errorPipe[0]);
            if (received > 0)
            {
                int status{};
                (void)::waitpid(child, &status, 0);
                error = "exec failed with errno " + std::to_string(childFailure) + ".";
                return false;
            }

            (void)::setpgid(child, child);
            slot.process_id = child;
            return true;
        }
#endif

        void poll_locked(SupervisorStorage& value)
        {
            const std::uint64_t tick = now_ns();
            for (ProcessSlot& slot : value.slots)
            {
                if (!slot.occupied || !active(slot.state))
                    continue;
#if defined(_WIN32)
                if (slot.process == nullptr)
                {
                    finish_slot(
                        value,
                        slot,
                        ProcessState::failed,
                        0,
                        false,
                        "Native process handle was lost.");
                    continue;
                }
                DWORD code{};
                if (::GetExitCodeProcess(slot.process, &code) == FALSE)
                {
                    finish_slot(
                        value,
                        slot,
                        ProcessState::failed,
                        0,
                        false,
                        "GetExitCodeProcess failed.");
                    continue;
                }
                if (code != STILL_ACTIVE)
                {
                    finish_slot(
                        value,
                        slot,
                        ProcessState::exited,
                        static_cast<std::int32_t>(code),
                        true,
                        "Process exited.");
                    continue;
                }
                if (slot.focus_pending)
                {
                    const FocusCode result = focus_slot(slot);
                    if (result == FocusCode::focused)
                    {
                        ++value.metrics.focus_successes;
                        revise(value);
                    }
                    else if (slot.focus_deadline_ns != 0u
                        && tick >= slot.focus_deadline_ns)
                    {
                        slot.focus_pending = false;
                        slot.focus_deadline_ns = 0u;
                    }
                }
#else
                int status{};
                const pid_t result = ::waitpid(slot.process_id, &status, WNOHANG);
                if (result == 0)
                    continue;
                if (result < 0)
                {
                    if (errno == EINTR)
                        continue;
                    finish_slot(
                        value,
                        slot,
                        ProcessState::failed,
                        0,
                        false,
                        "waitpid failed with errno " + std::to_string(errno) + ".");
                    continue;
                }
                const std::int32_t code = WIFEXITED(status)
                    ? static_cast<std::int32_t>(WEXITSTATUS(status))
                    : (WIFSIGNALED(status)
                        ? static_cast<std::int32_t>(128 + WTERMSIG(status))
                        : 0);
                finish_slot(
                    value,
                    slot,
                    ProcessState::exited,
                    code,
                    WIFEXITED(status) || WIFSIGNALED(status),
                    "Process exited.");
#endif
            }
        }

        [[nodiscard]] bool valid_text(
            std::string_view text,
            std::size_t maximum,
            bool allowEmpty) noexcept
        {
            return (allowEmpty || !text.empty())
                && text.size() <= maximum
                && text.find('\0') == std::string_view::npos;
        }

        [[nodiscard]] std::optional<std::string> validate_request(
            const LaunchRequest& request)
        {
            if (request.executable.empty())
                return "Executable path is empty.";
            if (!valid_executable_path(request.executable))
                return "Executable path is invalid or exceeds its bound.";
            if (request.expected_executable && !request.expected_executable->valid())
                return "Expected executable identity is invalid.";
#if !defined(_WIN32)
            if (request.expected_executable)
                return "Required executable identity launch is unsupported on this platform; atomic image enforcement is unavailable.";
#endif
            if (!valid_text(request.correlation_key, 256u, false))
                return "Correlation key is empty or too large.";
            if (!valid_text(request.exclusive_group, 128u, true))
                return "Exclusive group is too large.";
            if (!valid_text(request.display_name, 256u, true))
                return "Display name is too large.";
            if (request.arguments.size() > maximum_arguments)
                return "Process argument count exceeds the bound.";
            if (request.merged_output_path.generic_string().size() > 4096u)
                return "Merged output path exceeds the byte bound.";
            std::size_t total{};
            for (const std::string& argument : request.arguments)
            {
                if (!valid_text(argument, 4096u, true))
                    return "A process argument is invalid or too large.";
                total += argument.size();
                if (total > 32u * 1024u)
                    return "Process arguments exceed the total byte bound.";
            }
            return std::nullopt;
        }

        [[nodiscard]] ProcessSnapshot make_snapshot(
            const SupervisorStorage& value,
            const ProcessSlot& slot)
        {
            const std::uint64_t finish = slot.finished_tick_ns != 0u
                ? slot.finished_tick_ns
                : now_ns();
            return {
                .handle = handle_for(value, slot),
                .state = slot.state,
#if defined(_WIN32)
                .platform_process_id = static_cast<std::uint64_t>(slot.process_id),
                .platform_window_id = process_window_id(slot.process_id),
#else
                .platform_process_id = slot.process_id > 0
                    ? static_cast<std::uint64_t>(slot.process_id)
                    : 0u,
                .platform_window_id = 0u,
#endif
                .executable = slot.executable,
                .correlation_key = slot.correlation_key,
                .exclusive_group = slot.exclusive_group,
                .display_name = slot.display_name,
                .started_tick_ns = slot.started_tick_ns,
                .finished_tick_ns = slot.finished_tick_ns,
                .runtime_ns = slot.started_tick_ns != 0u && finish >= slot.started_tick_ns
                    ? finish - slot.started_tick_ns
                    : 0u,
                .exit_code = slot.exit_code,
                .exit_code_valid = slot.exit_code_valid,
#if defined(_WIN32)
                .focus_supported = slot.window_mode == WindowMode::normal,
#else
                .focus_supported = false,
#endif
                .focus_pending = slot.focus_pending,
                .stop_supported = true,
                .message = slot.message,
                .verified_executable = slot.verified_executable};
        }
    }

    std::string_view process_state_name(ProcessState state) noexcept
    {
        switch (state)
        {
        case ProcessState::idle: return "Idle";
        case ProcessState::starting: return "Starting";
        case ProcessState::running: return "Running";
        case ProcessState::stop_requested: return "Stop requested";
        case ProcessState::exited: return "Exited";
        case ProcessState::failed: return "Failed";
        }
        return "Unknown";
    }

    std::string_view launch_code_name(LaunchCode code) noexcept
    {
        switch (code)
        {
        case LaunchCode::started: return "Started";
        case LaunchCode::focused_existing: return "Focused existing";
        case LaunchCode::active_group_busy: return "Active group busy";
        case LaunchCode::invalid_request: return "Invalid request";
        case LaunchCode::capacity_exhausted: return "Capacity exhausted";
        case LaunchCode::spawn_failed: return "Spawn failed";
        }
        return "Unknown";
    }

    std::string_view focus_code_name(FocusCode code) noexcept
    {
        switch (code)
        {
        case FocusCode::focused: return "Focused";
        case FocusCode::pending: return "Window pending";
        case FocusCode::unsupported: return "Unsupported";
        case FocusCode::stale_handle: return "Stale handle";
        case FocusCode::not_running: return "Not running";
        case FocusCode::failed: return "Failed";
        }
        return "Unknown";
    }

    std::string_view stop_code_name(StopCode code) noexcept
    {
        switch (code)
        {
        case StopCode::requested: return "Requested";
        case StopCode::forced: return "Forced";
        case StopCode::unsupported: return "Unsupported";
        case StopCode::stale_handle: return "Stale handle";
        case StopCode::not_running: return "Not running";
        case StopCode::failed: return "Failed";
        }
        return "Unknown";
    }

    std::string_view wait_code_name(WaitCode code) noexcept
    {
        switch (code)
        {
        case WaitCode::exited: return "Exited";
        case WaitCode::cancelled: return "Cancelled";
        case WaitCode::timed_out: return "Timed out";
        case WaitCode::stale_handle: return "Stale handle";
        case WaitCode::failed: return "Failed";
        }
        return "Unknown";
    }

    std::optional<ExecutableIdentity> inspect_executable(
        const std::filesystem::path& executable) noexcept
    {
        try
        {
#if defined(_WIN32)
            ExecutableReadLock locked{};
            if (!locked.open(executable))
                return std::nullopt;
            return locked.identity;
#else
            // Inspection is informational here. It is never substituted for
            // an atomic expected-identity launch on platforms without one.
            return inspect_regular_executable(executable);
#endif
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    LaunchResult launch_or_focus(const LaunchRequest& request) noexcept
    {
        SupervisorStorage& value = storage();
        try
        {
            std::scoped_lock lock{value.mutex};
            ++value.metrics.launch_requests;
            revise(value);
            if (const auto invalid = validate_request(request))
            {
                ++value.metrics.launch_failures;
                return {
                    .code = LaunchCode::invalid_request,
                    .message = *invalid};
            }
            poll_locked(value);

            std::error_code error{};
            std::filesystem::path resolvedExecutable{};
#if defined(_WIN32)
            ExecutableReadLock executableLock{};
            if (request.expected_executable)
            {
                if (!executableLock.open(request.executable))
                {
                    ++value.metrics.launch_failures;
                    return {.code = LaunchCode::invalid_request,
                        .message = "Executable identity could not be read under a non-reparse, no-write/delete image and namespace lock."};
                }
                if (executableLock.identity != request.expected_executable)
                {
                    ++value.metrics.launch_failures;
                    return {.code = LaunchCode::invalid_request,
                        .message = "Executable size or SHA-256 differs from the required artifact identity."};
                }
                resolvedExecutable = executableLock.canonical_path;
            }
            else
#endif
            {
                resolvedExecutable = std::filesystem::weakly_canonical(
                    std::filesystem::absolute(request.executable, error), error);
                if (error || resolvedExecutable.empty())
                {
                    ++value.metrics.launch_failures;
                    return {.code = LaunchCode::invalid_request,
                        .message = "Executable path could not be resolved."};
                }
            }

            for (ProcessSlot& slot : value.slots)
            {
                if (!slot.occupied || !active(slot.state))
                    continue;
                if (slot.correlation_key == request.correlation_key)
                {
                    if (slot.executable != resolvedExecutable
                        || slot.verified_executable != request.expected_executable)
                    {
                        ++value.metrics.launch_failures;
                        return {.code = LaunchCode::invalid_request,
                            .message = "The correlation key is already bound to a different executable path or artifact identity."};
                    }
                    ++value.metrics.focus_requests;
                    const FocusCode focused = focus_slot(slot);
                    if (focused == FocusCode::focused)
                        ++value.metrics.focus_successes;
                    revise(value);
                    return {
                        .code = LaunchCode::focused_existing,
                        .handle = handle_for(value, slot),
                        .focus = focused,
                        .message = "Matching process is already active."};
                }
                if (!request.exclusive_group.empty()
                    && slot.exclusive_group == request.exclusive_group)
                {
                    return {
                        .code = LaunchCode::active_group_busy,
                        .handle = handle_for(value, slot),
                        .message = "Another process owns the exclusive group."};
                }
            }

            ProcessSlot* slot{};
            for (ProcessSlot& candidate : value.slots)
            {
                if (!candidate.occupied)
                {
                    slot = &candidate;
                    break;
                }
            }
            if (slot == nullptr)
            {
                for (ProcessSlot& candidate : value.slots)
                {
                    if (!active(candidate.state))
                    {
                        slot = &candidate;
                        break;
                    }
                }
            }
            if (slot == nullptr)
            {
                ++value.metrics.launch_failures;
                return {
                    .code = LaunchCode::capacity_exhausted,
                    .message = "All bounded process slots are active."};
            }

            const std::uint32_t generation = next_generation(slot->generation);
#if defined(_WIN32)
            if (slot->process != nullptr)
                ::CloseHandle(slot->process);
            if (slot->job != nullptr)
                ::CloseHandle(slot->job);
#endif
            *slot = ProcessSlot{};
            slot->occupied = true;
            slot->generation = generation;
            slot->state = ProcessState::starting;
            slot->correlation_key = request.correlation_key;
            slot->exclusive_group = request.exclusive_group;
            slot->display_name = request.display_name;
            slot->window_mode = request.window_mode;

            slot->executable = std::move(resolvedExecutable);
            if (error || slot->executable.empty()
                || !std::filesystem::is_regular_file(slot->executable, error)
                || error)
            {
                slot->state = ProcessState::failed;
                slot->message = "Executable is not a readable regular file.";
                ++value.metrics.launch_failures;
                revise(value);
                return {
                    .code = LaunchCode::invalid_request,
                    .handle = handle_for(value, *slot),
                    .message = slot->message};
            }

            LaunchRequest resolved = request;
            resolved.executable = slot->executable;
            if (resolved.working_directory.empty())
                resolved.working_directory = slot->executable.parent_path();
            resolved.working_directory = std::filesystem::weakly_canonical(
                std::filesystem::absolute(resolved.working_directory, error),
                error);
            if (error || resolved.working_directory.empty()
                || !std::filesystem::is_directory(resolved.working_directory, error)
                || error)
            {
                slot->state = ProcessState::failed;
                slot->message = "Working directory is not a readable directory.";
                ++value.metrics.launch_failures;
                revise(value);
                return {
                    .code = LaunchCode::invalid_request,
                    .handle = handle_for(value, *slot),
                    .message = slot->message};
            }
            if (!resolved.merged_output_path.empty())
            {
                if (resolved.merged_output_path.is_relative())
                {
                    resolved.merged_output_path = resolved.working_directory
                        / resolved.merged_output_path;
                }
                resolved.merged_output_path = std::filesystem::weakly_canonical(
                    std::filesystem::absolute(
                        resolved.merged_output_path, error),
                    error);
                const std::filesystem::path outputParent =
                    resolved.merged_output_path.parent_path();
                error.clear();
                const bool outputParentReady = !outputParent.empty()
                    && std::filesystem::is_directory(outputParent, error);
                const bool outputExists = !error
                    && std::filesystem::exists(
                        resolved.merged_output_path, error);
                const bool outputIsDirectory = !error && outputExists
                    && std::filesystem::is_directory(
                        resolved.merged_output_path, error);
                if (error || resolved.merged_output_path.empty()
                    || !outputParentReady || outputIsDirectory)
                {
                    slot->state = ProcessState::failed;
                    slot->message =
                        "Merged output path is not a writable file location.";
                    ++value.metrics.launch_failures;
                    revise(value);
                    return {
                        .code = LaunchCode::invalid_request,
                        .handle = handle_for(value, *slot),
                        .message = slot->message};
                }
            }

            std::string spawnError{};
            if (!spawn_process(*slot, resolved, spawnError))
            {
                slot->state = ProcessState::failed;
                slot->message = std::move(spawnError);
                ++value.metrics.launch_failures;
                revise(value);
                return {
                    .code = LaunchCode::spawn_failed,
                    .handle = handle_for(value, *slot),
                    .message = slot->message};
            }

            slot->verified_executable = request.expected_executable;
            slot->state = ProcessState::running;
            slot->started_tick_ns = now_ns();
            slot->message = "Process started.";
            ++value.metrics.launches;
            ++value.metrics.active_processes;
            value.metrics.peak_active_processes = (std::max)(
                value.metrics.peak_active_processes,
                value.metrics.active_processes);
            revise(value);
            return {
                .code = LaunchCode::started,
                .handle = handle_for(value, *slot),
                .focus = slot->focus_pending
                    ? FocusCode::pending
                    : FocusCode::unsupported,
                .message = slot->message};
        }
        catch (const std::exception& error)
        {
            return {
                .code = LaunchCode::spawn_failed,
                .message = error.what()};
        }
        catch (...)
        {
            return {
                .code = LaunchCode::spawn_failed,
                .message = "Unknown process launch failure."};
        }
    }

    FocusCode focus_impl(ProcessHandle handle)
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        ProcessSlot* slot = resolve_slot(value, handle);
        if (slot == nullptr)
            return FocusCode::stale_handle;
        ++value.metrics.focus_requests;
        const FocusCode result = focus_slot(*slot);
        if (result == FocusCode::focused)
            ++value.metrics.focus_successes;
        revise(value);
        return result;
    }

    StopCode stop_impl(ProcessHandle handle, StopMode mode)
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        ProcessSlot* slot = resolve_slot(value, handle);
        if (slot == nullptr)
            return StopCode::stale_handle;
        if (!active(slot->state))
            return StopCode::not_running;
        ++value.metrics.stop_requests;
#if defined(_WIN32)
        if (mode == StopMode::force)
        {
            const BOOL terminated = slot->job != nullptr
                ? ::TerminateJobObject(slot->job, 1u)
                : (slot->process != nullptr
                    ? ::TerminateProcess(slot->process, 1u)
                    : FALSE);
            if (terminated == FALSE)
            {
                revise(value);
                return StopCode::failed;
            }
            slot->state = ProcessState::stop_requested;
            slot->message = "Forced process stop requested.";
            revise(value);
            return StopCode::forced;
        }
        DWORD processId = slot->process_id;
        WindowSearch search{.process_id = processId};
        (void)::EnumWindows(find_process_window, reinterpret_cast<LPARAM>(&search));
        if (search.window == nullptr)
        {
            revise(value);
            return StopCode::unsupported;
        }
        (void)::EnumWindows(close_process_window, reinterpret_cast<LPARAM>(&processId));
#else
        const int signal = mode == StopMode::force ? SIGKILL : SIGTERM;
        if (slot->process_id <= 0 || ::kill(-slot->process_id, signal) != 0)
        {
            revise(value);
            return StopCode::failed;
        }
#endif
        slot->state = ProcessState::stop_requested;
        slot->message = mode == StopMode::force
            ? "Forced process stop requested."
            : "Graceful process stop requested.";
        revise(value);
        return mode == StopMode::force ? StopCode::forced : StopCode::requested;
    }

    void poll_impl()
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
    }

    std::optional<ProcessSnapshot> snapshot_impl(ProcessHandle handle)
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        const ProcessSlot* slot = resolve_slot(value, handle);
        return slot == nullptr
            ? std::nullopt
            : std::optional<ProcessSnapshot>{make_snapshot(value, *slot)};
    }

    WaitResult wait_impl(
        ProcessHandle handle,
        std::stop_token cancellation,
        std::uint64_t timeoutNs)
    {
        constexpr std::uint64_t forcedStopSettlementNs = 5'000'000'000ull;
        const std::uint64_t started = now_ns();
        std::uint64_t stopIssuedAt{};
        bool cancellationIssued{};
        bool timeoutIssued{};

        for (;;)
        {
            std::optional<ProcessSnapshot> observed = snapshot_impl(handle);
            if (!observed)
            {
                return {
                    .code = WaitCode::stale_handle,
                    .message = "Process handle became stale while waiting."};
            }
            if (!observed->active())
            {
                if (cancellationIssued)
                {
                    return {
                        .code = WaitCode::cancelled,
                        .process = std::move(observed),
                        .message = "Process tree stopped after cancellation."};
                }
                if (timeoutIssued)
                {
                    return {
                        .code = WaitCode::timed_out,
                        .process = std::move(observed),
                        .message = "Process tree stopped after timeout."};
                }
                const WaitCode terminalCode = observed->state == ProcessState::exited
                    ? WaitCode::exited
                    : WaitCode::failed;
                std::string terminalMessage = observed->message;
                return {
                    .code = terminalCode,
                    .process = std::move(observed),
                    .message = std::move(terminalMessage)};
            }

            const std::uint64_t tick = now_ns();
            const bool timedOut = timeoutNs != 0u
                && tick >= started
                && tick - started >= timeoutNs;
            if (!cancellationIssued && !timeoutIssued
                && (cancellation.stop_requested() || timedOut))
            {
                const StopCode stopped = stop_impl(handle, StopMode::force);
                if (stopped != StopCode::forced
                    && stopped != StopCode::requested
                    && stopped != StopCode::not_running)
                {
                    return {
                        .code = WaitCode::failed,
                        .process = std::move(observed),
                        .message = "Process tree could not be stopped while waiting."};
                }
                cancellationIssued = cancellation.stop_requested();
                timeoutIssued = !cancellationIssued && timedOut;
                stopIssuedAt = tick;
            }
            else if (stopIssuedAt != 0u
                && tick >= stopIssuedAt
                && tick - stopIssuedAt >= forcedStopSettlementNs)
            {
                return {
                    .code = WaitCode::failed,
                    .process = std::move(observed),
                    .message = "Forced process-tree stop did not settle within the bound."};
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }
    std::optional<ProcessSnapshot> active_in_group_impl(std::string_view group)
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        for (const ProcessSlot& slot : value.slots)
        {
            if (slot.occupied && active(slot.state)
                && slot.exclusive_group == group)
            {
                return make_snapshot(value, slot);
            }
        }
        return std::nullopt;
    }

    std::vector<ProcessSnapshot> snapshots_impl()
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        try
        {
            std::vector<ProcessSnapshot> result{};
            result.reserve(value.slots.size());
            for (const ProcessSlot& slot : value.slots)
            {
                if (slot.occupied)
                    result.emplace_back(make_snapshot(value, slot));
            }
            return result;
        }
        catch (...)
        {
            return {};
        }
    }

    SupervisorMetrics metrics_impl()
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        return value.metrics;
    }

    bool release_impl(ProcessHandle handle)
    {
        SupervisorStorage& value = storage();
        std::scoped_lock lock{value.mutex};
        poll_locked(value);
        ProcessSlot* slot = resolve_slot(value, handle);
        if (slot == nullptr || active(slot->state))
            return false;
        const std::uint32_t generation = slot->generation;
        *slot = ProcessSlot{};
        slot->generation = generation;
        revise(value);
        return true;
    }
    FocusCode focus(ProcessHandle handle) noexcept
    {
        try
        {
            return focus_impl(handle);
        }
        catch (...)
        {
            return FocusCode::failed;
        }
    }

    StopCode stop(ProcessHandle handle, StopMode mode) noexcept
    {
        try
        {
            return stop_impl(handle, mode);
        }
        catch (...)
        {
            return StopCode::failed;
        }
    }

    WaitResult wait(
        ProcessHandle handle,
        std::stop_token cancellation,
        std::uint64_t timeoutNs) noexcept
    {
        try
        {
            return wait_impl(handle, cancellation, timeoutNs);
        }
        catch (const std::exception& error)
        {
            return {
                .code = WaitCode::failed,
                .message = error.what()};
        }
        catch (...)
        {
            return {
                .code = WaitCode::failed,
                .message = "Unknown process wait failure."};
        }
    }
    void poll() noexcept
    {
        try
        {
            poll_impl();
        }
        catch (...)
        {
        }
    }

    std::optional<ProcessSnapshot> snapshot(ProcessHandle handle) noexcept
    {
        try
        {
            return snapshot_impl(handle);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<ProcessSnapshot> active_in_group(
        std::string_view group) noexcept
    {
        try
        {
            return active_in_group_impl(group);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::vector<ProcessSnapshot> snapshots() noexcept
    {
        try
        {
            return snapshots_impl();
        }
        catch (...)
        {
            return {};
        }
    }

    SupervisorMetrics metrics() noexcept
    {
        try
        {
            return metrics_impl();
        }
        catch (...)
        {
            return {};
        }
    }

    bool release(ProcessHandle handle) noexcept
    {
        try
        {
            return release_impl(handle);
        }
        catch (...)
        {
            return false;
        }
    }
}
