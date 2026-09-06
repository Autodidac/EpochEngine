module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "platform.child_isolation.hpp"
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
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

        [[nodiscard]] bool ordinary_workspace_directory(const std::filesystem::path& path)
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (error || !std::filesystem::is_directory(status)
                || std::filesystem::is_symlink(status)) return false;
#if defined(_WIN32)
            const DWORD attributes = ::GetFileAttributesW(path.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES
                || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) return false;
#endif
            return true;
        }

        [[nodiscard]] bool ordinary_workspace_tree(const std::filesystem::path& path)
        {
            if (!valid_executable_path(path) || !path.is_absolute()
                || path == path.root_path()) return false;
            auto current = path.root_path();
            if (!ordinary_workspace_directory(current)) return false;
            for (const auto& component : path.relative_path())
            {
                if (component.empty() || component == "." || component == "..") return false;
                current /= component;
                if (!ordinary_workspace_directory(current)) return false;
            }
            std::error_code error{};
            return std::filesystem::canonical(path, error) == path.lexically_normal() && !error;
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

#if defined(_WIN32)
        struct RetirementMember final
        {
            DWORD process_id{};
            HANDLE process{};

            RetirementMember(DWORD id, HANDLE handle) noexcept
                : process_id{id}, process{handle} {}
            RetirementMember(const RetirementMember&) = delete;
            RetirementMember& operator=(const RetirementMember&) = delete;
            RetirementMember(RetirementMember&& other) noexcept
                : process_id{std::exchange(other.process_id, 0u)},
                  process{std::exchange(other.process, nullptr)} {}
            RetirementMember& operator=(RetirementMember&& other) noexcept
            {
                if (this != &other)
                {
                    if (process != nullptr) (void)::CloseHandle(process);
                    process_id = std::exchange(other.process_id, 0u);
                    process = std::exchange(other.process, nullptr);
                }
                return *this;
            }
            ~RetirementMember() noexcept
            {
                if (process != nullptr) (void)::CloseHandle(process);
            }
        };
#endif

        struct ProcessSlot final
        {
            bool occupied{};
            bool active_counted{};
            bool failed_launch{};
            std::uint32_t generation{};
            ProcessState state{ProcessState::idle};
            std::filesystem::path executable{};
            std::optional<ExecutableIdentity> verified_executable{};
            std::optional<std::vector<EnvironmentVariable>> environment{};
            bool disconnect_standard_input{};
            std::optional<WorkspaceIsolation> isolation{};
            std::vector<std::string> arguments{};
            std::filesystem::path requested_working_directory{};
            std::filesystem::path requested_output_path{};
            bool append_output{};
            bool restricted_token_verified{};
            bool isolation_retired{};
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
            bool job_assigned{};
            bool job_retirement_requested{};
            DWORD retirement_observation_error{};
            DWORD retirement_query_assigned{};
            DWORD retirement_query_returned{};
            DWORD retirement_query_native_error{};
            std::vector<RetirementMember> retirement_members{};
            std::unique_ptr<native_isolation::Lease> isolation_lease{};
            bool isolation_cleanup_pending{};
            // Discovery is top-level only, but an admitted preview may later
            // become a child window. Retain and revalidate native identity;
            // the context host separately owns its attachment/property lease.
            mutable HWND observed_window{};
            mutable DWORD observed_window_thread{};
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
            if (slot.active_counted && value.metrics.active_processes > 0u)
                --value.metrics.active_processes;
            slot.active_counted = false;
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
            slot.retirement_members.clear();
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
        // Job accounting can reach zero before the member process objects
        // become signalled. Hold real member handles across TerminateJobObject
        // so group succession also waits for that final lifecycle evidence.
        [[nodiscard]] DWORD observe_retirement_members(ProcessSlot& slot) noexcept
        {
            constexpr std::size_t maximum_retirement_members = 4096u;
            struct ProcessIdBuffer final
            {
                DWORD assigned{};
                DWORD count{};
                std::array<ULONG_PTR, maximum_retirement_members> ids{};
            };
            static_assert(offsetof(ProcessIdBuffer, ids)
                == offsetof(JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList));
            if (slot.job == nullptr) return ERROR_INVALID_HANDLE;
            try
            {
                const auto queryMembers = [&slot](ProcessIdBuffer& result) -> DWORD
                {
                    // Membership can change during the native enumeration.
                    // Retry a short/incomplete list before recording an
                    // irreversible observation failure; never consume partial
                    // IDs or clear a previously failed retirement observation.
                    for (unsigned retry = 0u; retry != 3u; ++retry)
                    {
                        // Yield between retries: a successful native call was
                        // observed reporting assigned=21/returned=20 while
                        // terminated members were still leaving the job. Three
                        // immediate queries can all see that same transition.
                        if (retry != 0u)
                            std::this_thread::sleep_for(std::chrono::milliseconds{1});
                        result = {};
                        const BOOL queried = ::QueryInformationJobObject(slot.job,
                            JobObjectBasicProcessIdList, &result,
                            static_cast<DWORD>(sizeof(result)), nullptr);
                        const DWORD failure = queried ? ERROR_SUCCESS : ::GetLastError();
                        if (failure != ERROR_SUCCESS || result.count != result.assigned
                            || result.assigned > result.ids.size())
                        {
                            slot.retirement_query_assigned = result.assigned;
                            slot.retirement_query_returned = result.count;
                            slot.retirement_query_native_error = failure;
                        }
                        if (failure != ERROR_SUCCESS && failure != ERROR_MORE_DATA)
                            return failure;
                        if (result.assigned > result.ids.size() || result.count > result.ids.size())
                            return ERROR_MORE_DATA;
                        if (failure == ERROR_SUCCESS && result.count == result.assigned)
                            return ERROR_SUCCESS;
                    }
                    return ERROR_MORE_DATA;
                };
                ProcessIdBuffer members{};
                if (const DWORD queried = queryMembers(members); queried != ERROR_SUCCESS)
                    return queried;
                for (DWORD index = 0u; index < members.count; ++index)
                {
                    const ULONG_PTR rawId = members.ids[index];
                    if (rawId == 0u || rawId > (std::numeric_limits<DWORD>::max)())
                        return ERROR_INVALID_DATA;
                    const DWORD id = static_cast<DWORD>(rawId);
                    if (id == slot.process_id
                        || std::ranges::any_of(slot.retirement_members,
                            [id](const RetirementMember& member)
                            { return member.process_id == id; }))
                        continue;
                    for (unsigned retry = 0u; ; ++retry)
                    {
                        RetirementMember member{id, ::OpenProcess(
                            SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id)};
                        DWORD identityChanged{};
                        if (member.process == nullptr)
                        {
                            identityChanged = ::GetLastError();
                            if (identityChanged != ERROR_INVALID_PARAMETER)
                                return identityChanged;
                        }
                        else
                        {
                            BOOL inJob{};
                            if (::IsProcessInJob(member.process, slot.job, &inJob) == FALSE)
                                return ::GetLastError();
                            if (inJob == FALSE) identityChanged = ERROR_INVALID_DATA;
                        }
                        if (identityChanged != ERROR_SUCCESS)
                        {
                            // Snapshot IDs can disappear or be recycled before
                            // OpenProcess. Require a fresh complete job list to
                            // confirm departure; never wait on a nonmember handle
                            // or treat access/query errors as an ordinary exit.
                            ProcessIdBuffer current{};
                            if (const DWORD queried = queryMembers(current); queried != ERROR_SUCCESS)
                                return queried;
                            const std::span<const ULONG_PTR> currentIds{
                                current.ids.data(), current.count};
                            if (std::ranges::find(currentIds, rawId) == currentIds.end())
                                break;
                            if (retry >= 2u) return identityChanged;
                            continue;
                        }
                        const DWORD signalled = ::WaitForSingleObject(member.process, 0u);
                        if (signalled == WAIT_OBJECT_0) break;
                        if (signalled != WAIT_TIMEOUT)
                            return signalled == WAIT_FAILED ? ::GetLastError() : ERROR_INVALID_DATA;
                        if (slot.retirement_members.size() >= maximum_retirement_members)
                            return ERROR_MORE_DATA;
                        slot.retirement_members.emplace_back(std::move(member));
                        break;
                    }
                }
                return ERROR_SUCCESS;
            }
            catch (...)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }
        }

        [[nodiscard]] DWORD settle_retirement_members(ProcessSlot& slot) noexcept
        {
            DWORD failure{};
            std::erase_if(slot.retirement_members, [&failure](const RetirementMember& member)
            {
                const DWORD signalled = ::WaitForSingleObject(member.process, 0u);
                if (signalled != WAIT_OBJECT_0 && signalled != WAIT_TIMEOUT
                    && failure == ERROR_SUCCESS)
                    failure = signalled == WAIT_FAILED ? ::GetLastError() : ERROR_INVALID_DATA;
                return signalled == WAIT_OBJECT_0;
            });
            return failure;
        }

        [[nodiscard]] DWORD request_job_retirement(ProcessSlot& slot) noexcept
        {
            // A failed job assignment leaves an owned, never-resumed root PID,
            // not a member of our job. Stopping the empty job cannot retire it.
            if (!slot.job_assigned)
                return slot.process == nullptr ? ERROR_INVALID_HANDLE
                    : (::TerminateProcess(slot.process, 1u) != FALSE
                        ? ERROR_SUCCESS : ::GetLastError());
            const DWORD observed = observe_retirement_members(slot);
            if (observed != ERROR_SUCCESS && slot.retirement_observation_error == ERROR_SUCCESS)
                slot.retirement_observation_error = observed;
            // An observation failure must not prevent cancellation. Preserve
            // the missing evidence even if subsequent accounting becomes zero.
            const BOOL terminated = slot.job != nullptr
                ? ::TerminateJobObject(slot.job, 1u)
                : (slot.process != nullptr ? ::TerminateProcess(slot.process, 1u) : FALSE);
            if (terminated == FALSE)
                return slot.job == nullptr && slot.process == nullptr
                    ? ERROR_INVALID_HANDLE : ::GetLastError();
            slot.job_retirement_requested = slot.job != nullptr;
            return ERROR_SUCCESS;
        }

        struct ScopedNativeHandle final
        {
            HANDLE value{};
            explicit ScopedNativeHandle(HANDLE handle) noexcept : value(handle) {}
            ScopedNativeHandle(const ScopedNativeHandle&) = delete;
            ScopedNativeHandle& operator=(const ScopedNativeHandle&) = delete;
            ~ScopedNativeHandle() noexcept
            {
                if (value != nullptr && value != INVALID_HANDLE_VALUE)
                    (void)::CloseHandle(value);
            }
            [[nodiscard]] HANDLE release() noexcept
            {
                return std::exchange(value, nullptr);
            }
        };

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
            const ProcessSlot& slot) noexcept
        {
            if (!active(slot.state) || slot.window_mode == WindowMode::hidden
                || !slot.process || slot.process_id == 0u
                || ::WaitForSingleObject(slot.process, 0u) != WAIT_TIMEOUT
                || ::GetProcessId(slot.process) != slot.process_id)
                return 0u;
            if (slot.observed_window)
            {
                DWORD pid{};
                const DWORD thread = ::GetWindowThreadProcessId(slot.observed_window, &pid);
                if (::IsWindow(slot.observed_window) && pid == slot.process_id
                    && thread != 0u && thread == slot.observed_window_thread)
                    return static_cast<std::uint64_t>(
                        reinterpret_cast<std::uintptr_t>(slot.observed_window));
                slot.observed_window = nullptr;
                slot.observed_window_thread = 0u;
            }
            WindowSearch search{.process_id = slot.process_id};
            (void)::EnumWindows(
                find_process_window,
                reinterpret_cast<LPARAM>(&search));
            DWORD pid{};
            const DWORD thread = ::GetWindowThreadProcessId(search.window, &pid);
            if (!search.window || thread == 0u || pid != slot.process_id) return 0u;
            slot.observed_window = search.window;
            slot.observed_window_thread = thread;
            return static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(search.window));
        }

        [[nodiscard]] FocusCode focus_slot(ProcessSlot& slot) noexcept
        {
            if (!active(slot.state))
                return FocusCode::not_running;
            if (slot.window_mode == WindowMode::hidden)
                return FocusCode::unsupported;

            WindowSearch search{.process_id = slot.process_id,
                .window = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(process_window_id(slot)))};
            if (search.window == nullptr)
            {
                slot.focus_pending = true;
                if (slot.focus_deadline_ns == 0u)
                    slot.focus_deadline_ns = now_ns() + 5'000'000'000ull;
                return FocusCode::pending;
            }

            // Docked foreign HWND focus belongs to the parent context host.
            // Never try to turn its child HWND into a top-level foreground.
            if ((::GetWindowLongPtrW(search.window, GWL_STYLE) & WS_CHILD) != 0)
            {
                slot.focus_pending = false;
                slot.focus_deadline_ns = 0u;
                return FocusCode::unsupported;
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
            if (request.isolation)
            {
                slot.isolation_lease = std::make_unique<native_isolation::Lease>();
                if (!slot.isolation_lease->prepare(request.isolation->owned_root,
                        request.isolation->read_only, request.isolation->writable, error))
                    return false;
            }
            // A non-null, separately owned block replaces the environment;
            // never set/unset the host process environment to prepare a child.
            std::vector<wchar_t> environmentBlock{};
            if (request.environment)
            {
                auto variables = *request.environment;
                const auto folded = [](std::string_view name)
                {
                    std::string result{name};
                    for (char& ch : result)
                        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
                    return result;
                };
                std::ranges::sort(variables, {}, [&](const EnvironmentVariable& variable)
                    { return folded(variable.name); });
                for (const auto& variable : variables)
                {
                    const auto wide = utf8_to_wide(variable.name + "=" + variable.value);
                    if (!wide || environmentBlock.size() + wide->size() + 2u > 32'767u)
                    {
                        error = "Explicit process environment is not valid bounded UTF-8/UTF-16.";
                        return false;
                    }
                    environmentBlock.insert(environmentBlock.end(), wide->begin(), wide->end());
                    environmentBlock.push_back(L'\0');
                }
                if (environmentBlock.empty()) environmentBlock.push_back(L'\0');
                environmentBlock.push_back(L'\0');
            }

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

            ScopedNativeHandle job{::CreateJobObjectW(nullptr, nullptr)};
            if (job.value == nullptr)
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
                    job.value,
                    JobObjectExtendedLimitInformation,
                    &limits,
                    sizeof(limits)) == FALSE)
            {
                const DWORD failure = ::GetLastError();
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
            const DWORD attributeCount = request.isolation ? 3u : 1u;
            DWORD applicationPackagesPolicy = PROCESS_CREATION_ALL_APPLICATION_PACKAGES_OPT_OUT;
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
                        error = "Output capture could not seek to the end: Win32 error "
                            + std::to_string(static_cast<unsigned long>(failure)) + ".";
                        return false;
                    }
                }
                const HANDLE parentInput = ::GetStdHandle(STD_INPUT_HANDLE);
                if (request.disconnect_standard_input
                    || parentInput == nullptr || parentInput == INVALID_HANDLE_VALUE)
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
                    error = "Captured process stdin could not be prepared: Win32 error "
                        + std::to_string(static_cast<unsigned long>(failure)) + ".";
                    return false;
                }

                SIZE_T attributeBytes{};
                const BOOL measured = ::InitializeProcThreadAttributeList(
                    nullptr, attributeCount, 0u, &attributeBytes);
                const DWORD measureFailure = ::GetLastError();
                if (measured != FALSE || attributeBytes == 0u
                    || measureFailure != ERROR_INSUFFICIENT_BUFFER)
                {
                    error = "Captured process handle list could not be sized: Win32 error "
                        + std::to_string(static_cast<unsigned long>(measureFailure)) + ".";
                    return false;
                }
                captured.attributes = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
                    ::HeapAlloc(::GetProcessHeap(), 0u, attributeBytes));
                if (captured.attributes == nullptr)
                {
                    error = "Captured process handle list allocation failed.";
                    return false;
                }
                if (::InitializeProcThreadAttributeList(
                        captured.attributes, attributeCount, 0u, &attributeBytes) == FALSE)
                {
                    const DWORD failure = ::GetLastError();
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
                if (slot.isolation_lease
                    && (::UpdateProcThreadAttribute(captured.attributes, 0u,
                            PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
                            slot.isolation_lease->capabilities(), sizeof(SECURITY_CAPABILITIES),
                            nullptr, nullptr) == FALSE
                        || ::UpdateProcThreadAttribute(captured.attributes, 0u,
                            PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY,
                            &applicationPackagesPolicy, sizeof(applicationPackagesPolicy),
                            nullptr, nullptr) == FALSE))
                {
                    const DWORD failure = ::GetLastError();
                    error = "Restricted process attributes failed: Win32 "
                        + std::to_string(failure) + ".";
                    return false;
                }
            }
            PROCESS_INFORMATION process{};
            DWORD flags = CREATE_SUSPENDED;
            if (request.environment) flags |= CREATE_UNICODE_ENVIRONMENT;
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
                !request.merged_output_path.empty(),
                flags,
                request.environment ? environmentBlock.data() : nullptr,
                working.empty() ? nullptr : working.c_str(),
                &startup.StartupInfo,
                &process);
            if (created == FALSE)
            {
                const DWORD failure = ::GetLastError();
                error = "CreateProcessW failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }

            // Publish native ownership before any verification or diagnostic
            // can allocate. The reservation failure path stops this exact PID
            // and retains its lease until native retirement is observed.
            ScopedNativeHandle initialThread{process.hThread};
            slot.process = process.hProcess;
            slot.job = job.release();
            slot.process_id = process.dwProcessId;
            if (::AssignProcessToJobObject(slot.job, slot.process) == FALSE)
            {
                const DWORD failure = ::GetLastError();
                error = "AssignProcessToJobObject failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }
            slot.job_assigned = true;
            if (slot.isolation_lease && !slot.isolation_lease->verify(slot.process, error))
                return false;
            slot.restricted_token_verified = slot.isolation_lease != nullptr;
            if (::ResumeThread(initialThread.value) == static_cast<DWORD>(-1))
            {
                const DWORD failure = ::GetLastError();
                error = "ResumeThread failed with error "
                    + std::to_string(static_cast<unsigned long>(failure)) + ".";
                return false;
            }
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

            // Build allocations before fork; execve receives only the explicit
            // allowlist. The legacy execv path remains unchanged when absent.
            std::vector<std::string> environmentStorage{};
            std::vector<char*> environmentPointers{};
            if (request.environment)
            {
                environmentStorage.reserve(request.environment->size());
                for (const auto& variable : *request.environment)
                    environmentStorage.push_back(variable.name + "=" + variable.value);
                environmentPointers.reserve(environmentStorage.size() + 1u);
                for (auto& variable : environmentStorage)
                    environmentPointers.push_back(variable.data());
                environmentPointers.push_back(nullptr);
            }

            int errorPipe[2]{-1, -1};
            if (::pipe(errorPipe) != 0)
            {
                error = "pipe failed with errno " + std::to_string(errno) + ".";
                return false;
            }
            // Keep the handshake outside stdio even when the caller entered
            // with closed standard descriptors. A later dup2 must not replace
            // the only path that can report a failed input/environment setup.
            const auto reserveDescriptor = [](int& descriptor)
            {
                if (descriptor > STDERR_FILENO) return true;
                const int replacement = ::fcntl(descriptor, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
                if (replacement < 0) return false;
                (void)::close(descriptor);
                descriptor = replacement;
                return true;
            };
            if (!reserveDescriptor(errorPipe[0]) || !reserveDescriptor(errorPipe[1])
                || ::fcntl(errorPipe[1], F_SETFD, FD_CLOEXEC) < 0)
            {
                const int failure = errno;
                (void)::close(errorPipe[0]);
                (void)::close(errorPipe[1]);
                error = "Process setup channel could not be protected: errno "
                    + std::to_string(failure) + ".";
                return false;
            }

            int nullFile = -1;
            if (request.window_mode == WindowMode::hidden || request.disconnect_standard_input)
                nullFile = ::open("/dev/null", O_RDWR);
            if (request.disconnect_standard_input && nullFile < 0)
            {
                const int failure = errno;
                (void)::close(errorPipe[0]);
                (void)::close(errorPipe[1]);
                error = "Dedicated process stdin could not be opened: errno "
                    + std::to_string(failure) + ".";
                return false;
            }
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

            struct SetupFailure final
            {
                int error{};
                int descriptor_cleanup{};
            };
            const pid_t child = ::fork();
            const int forkFailure = child < 0 ? errno : 0;
            if (child == 0)
            {
                (void)::close(errorPipe[0]);
                const auto failSetup = [&](int failure, bool descriptorCleanup = false,
                                           int exitCode = 126)
                {
                    const SetupFailure report{failure, descriptorCleanup ? 1 : 0};
                    const auto* bytes = reinterpret_cast<const char*>(&report);
                    std::size_t offset{};
                    while (offset < sizeof(report))
                    {
                        const auto written = ::write(errorPipe[1], bytes + offset,
                            sizeof(report) - offset);
                        if (written < 0 && errno == EINTR) continue;
                        if (written <= 0) break;
                        offset += static_cast<std::size_t>(written);
                    }
                    ::_exit(exitCode);
                };
                const auto redirect = [&](int descriptor, int target)
                {
                    if (::dup2(descriptor, target) < 0) failSetup(errno);
                };
                if (::setpgid(0, 0) < 0) failSetup(errno);
                if (nullFile >= 0)
                {
                    redirect(nullFile, STDIN_FILENO);
                    if (outputFile < 0)
                    {
                        redirect(nullFile, STDOUT_FILENO);
                        redirect(nullFile, STDERR_FILENO);
                    }
                    if (nullFile > STDERR_FILENO)
                        (void)::close(nullFile);
                }
                if (outputFile >= 0)
                {
                    redirect(outputFile, STDOUT_FILENO);
                    redirect(outputFile, STDERR_FILENO);
                    if (outputFile > STDERR_FILENO)
                        (void)::close(outputFile);
                }
#if defined(__linux__)
                // fork owns a private descriptor table. Retain only stdio and
                // the CLOEXEC setup writer; sparse descriptors above a lowered
                // RLIMIT_NOFILE must not survive either execv or execve.
                // Direct syscalls avoid allocator/stdio/directory-walk state
                // inherited from other threads. Unsupported or denied cleanup
                // is a launch failure, never an incomplete close-loop fallback.
#if defined(SYS_close_range)
                const auto setupWriter = static_cast<unsigned int>(errorPipe[1]);
                if (setupWriter > static_cast<unsigned int>(STDERR_FILENO) + 1u
                    && ::syscall(SYS_close_range,
                        static_cast<unsigned int>(STDERR_FILENO) + 1u,
                        setupWriter - 1u, 0u) < 0)
                    failSetup(errno, true);
                if (::syscall(SYS_close_range, setupWriter + 1u,
                        std::numeric_limits<unsigned int>::max(), 0u) < 0)
                    failSetup(errno, true);
#else
                failSetup(ENOSYS, true);
#endif
#endif
                if (::chdir(request.working_directory.c_str()) != 0)
                    failSetup(errno);
                if (request.environment)
                    ::execve(slot.executable.c_str(), arguments.data(), environmentPointers.data());
                else
                    ::execv(slot.executable.c_str(), arguments.data());
                failSetup(errno, false, 127);
            }

            (void)::close(errorPipe[1]);
            if (nullFile >= 0)
                (void)::close(nullFile);
            if (outputFile >= 0)
                (void)::close(outputFile);
            if (child < 0)
            {
                (void)::close(errorPipe[0]);
                error = "fork failed with errno " + std::to_string(forkFailure) + ".";
                return false;
            }

            SetupFailure childFailure{};
            std::size_t received{};
            int readFailure{};
            while (received < sizeof(childFailure))
            {
                const auto count = ::read(errorPipe[0],
                    reinterpret_cast<char*>(&childFailure) + received,
                    sizeof(childFailure) - received);
                if (count < 0 && errno == EINTR) continue;
                if (count < 0) readFailure = errno;
                if (count <= 0) break;
                received += static_cast<std::size_t>(count);
            }
            (void)::close(errorPipe[0]);
            if (readFailure != 0 || (received != 0u && received != sizeof(childFailure)))
            {
                (void)::kill(child, SIGKILL);
                while (::waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
                error = "Process setup channel failed before exec admission: errno "
                    + std::to_string(readFailure != 0 ? readFailure : EIO) + ".";
                return false;
            }
            if (received != 0u)
            {
                while (::waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
                error = childFailure.descriptor_cleanup != 0
                    ? "Linux inherited-descriptor cleanup failed with errno "
                        + std::to_string(childFailure.error)
                        + ". close_range support and permission are required."
                    : "exec failed with errno " + std::to_string(childFailure.error) + ".";
                return false;
            }

            (void)::setpgid(child, child);
            slot.process_id = child;
            return true;
        }
#endif

        void count_active_slot(SupervisorStorage& value, ProcessSlot& slot) noexcept
        {
            if (slot.active_counted) return;
            slot.active_counted = true;
            ++value.metrics.active_processes;
            value.metrics.peak_active_processes = (std::max)(
                value.metrics.peak_active_processes, value.metrics.active_processes);
        }

        // Called while the supervisor lock is still held, including stack
        // unwinding. Losing a diagnostic allocation must not lose native or
        // security-grant ownership, nor make an unsettled slot reusable.
        void fail_reserved_launch(SupervisorStorage& value, ProcessSlot& slot) noexcept
        {
            if (!slot.failed_launch) ++value.metrics.launch_failures;
            slot.failed_launch = true;
            slot.state = ProcessState::failed;
            slot.focus_pending = false;
            slot.focus_deadline_ns = 0u;
#if defined(_WIN32)
            if (slot.process != nullptr || slot.isolation_lease)
            {
                slot.state = ProcessState::stop_requested;
                count_active_slot(value, slot);
                if (slot.process != nullptr)
                {
                    (void)request_job_retirement(slot);
                }
                else
                {
                    slot.isolation_cleanup_pending = true;
                    try
                    {
                        std::string cleanupError{};
                        if (slot.isolation_lease->retire(cleanupError))
                        {
                            slot.isolation_retired = true;
                            slot.isolation_lease.reset();
                            slot.isolation_cleanup_pending = false;
                            slot.state = ProcessState::failed;
                        }
                        else if (!cleanupError.empty())
                            slot.message += " Restricted cleanup retained: " + cleanupError;
                    }
                    catch (...) { /* The pending lease remains owned. */ }
                }
            }
#else
            if (slot.process_id > 0)
            {
                slot.state = ProcessState::stop_requested;
                count_active_slot(value, slot);
                (void)::kill(-slot.process_id, SIGKILL);
                (void)::kill(slot.process_id, SIGKILL);
            }
#endif
            if (!active(slot.state))
            {
                if (slot.active_counted && value.metrics.active_processes > 0u)
                    --value.metrics.active_processes;
                slot.active_counted = false;
                slot.finished_tick_ns = now_ns();
            }
            revise(value);
        }

        struct LaunchReservation final
        {
            SupervisorStorage& value;
            ProcessSlot& slot;
            bool armed{true};
            ~LaunchReservation() noexcept
            {
                if (!armed || !active(slot.state)) return;
                try { slot.message = "Process launch failed during host preparation."; }
                catch (...) {}
                fail_reserved_launch(value, slot);
            }
        };

        void poll_locked(SupervisorStorage& value)
        {
            const std::uint64_t tick = now_ns();
            for (ProcessSlot& slot : value.slots)
            {
                if (!slot.occupied || !active(slot.state))
                    continue;
#if defined(_WIN32)
                const auto retainOwnership = [&](std::string message)
                {
                    const bool changed = slot.state != ProcessState::stop_requested
                        || slot.message != message;
                    slot.state = ProcessState::stop_requested;
                    slot.focus_pending = false;
                    slot.focus_deadline_ns = 0u;
                    slot.message = std::move(message);
                    if (changed) revise(value);
                };
                if (slot.isolation_cleanup_pending && slot.process == nullptr)
                {
                    std::string cleanupError{};
                    if (!slot.isolation_lease || slot.isolation_lease->retire(cleanupError))
                    {
                        slot.isolation_retired = slot.isolation_lease != nullptr;
                        slot.isolation_lease.reset();
                        slot.isolation_cleanup_pending = false;
                        finish_slot(value, slot, ProcessState::failed, 0, false,
                            "Process launch failed; restricted identity and grants retired.");
                    }
                    else retainOwnership("Restricted launch cleanup is pending: " + cleanupError);
                    continue;
                }
                // A status-query failure is not retirement evidence. Retain
                // the process/job lease so release and group succession cannot
                // forget an unobserved child tree.
                if (slot.process == nullptr || slot.job == nullptr)
                {
                    retainOwnership("Native process/job handle is unavailable; retirement is unverified.");
                    continue;
                }
                const DWORD signalled = ::WaitForSingleObject(slot.process, 0u);
                if (signalled != WAIT_OBJECT_0 && signalled != WAIT_TIMEOUT)
                {
                    retainOwnership("Process exit could not be observed; retirement is unverified (Win32 "
                        + std::to_string(static_cast<unsigned long>(::GetLastError())) + ").");
                    continue;
                }
                if (!slot.job_assigned)
                {
                    // CreateProcess succeeded but job admission did not. This
                    // root was never resumed, so no child code or descendants
                    // ran; still require its actual process object to signal.
                    if (signalled != WAIT_OBJECT_0)
                    {
                        (void)request_job_retirement(slot);
                        retainOwnership("Retiring rejected suspended process before releasing its isolation lease.");
                        continue;
                    }
                    DWORD code{};
                    const bool codeValid = ::GetExitCodeProcess(slot.process, &code) != FALSE;
                    if (slot.isolation_lease)
                    {
                        std::string cleanupError{};
                        if (!slot.isolation_lease->retire(cleanupError))
                        {
                            retainOwnership("Rejected process exited; restricted cleanup is pending: " + cleanupError);
                            continue;
                        }
                        slot.isolation_retired = true;
                        slot.isolation_lease.reset();
                    }
                    finish_slot(value, slot, ProcessState::failed,
                        static_cast<std::int32_t>(code), codeValid,
                        "Rejected suspended process exited; native and isolation ownership retired.");
                    continue;
                }
                if (signalled == WAIT_TIMEOUT && slot.failed_launch
                    && !slot.job_retirement_requested)
                {
                    (void)request_job_retirement(slot);
                    retainOwnership("Retiring rejected process tree before releasing its isolation lease.");
                    continue;
                }
                if (signalled == WAIT_TIMEOUT && slot.job_retirement_requested)
                {
                    const DWORD observed = observe_retirement_members(slot);
                    const DWORD settled = settle_retirement_members(slot);
                    if (slot.retirement_observation_error == ERROR_SUCCESS)
                        slot.retirement_observation_error = observed != ERROR_SUCCESS
                            ? observed : settled;
                    retainOwnership(slot.retirement_observation_error == ERROR_SUCCESS
                        ? "Retiring owned process tree."
                        : "Process-tree member retirement is unverified; ownership is retained (Win32 "
                            + std::to_string(static_cast<unsigned long>(slot.retirement_observation_error))
                            + "; assigned=" + std::to_string(slot.retirement_query_assigned)
                            + "; returned=" + std::to_string(slot.retirement_query_returned)
                            + "; query_error=" + std::to_string(slot.retirement_query_native_error) + ").");
                    continue;
                }
                if (signalled == WAIT_OBJECT_0)
                {
                    // 259 is also a possible application exit code. The
                    // signalled process handle, not STILL_ACTIVE, owns liveness.
                    DWORD code{};
                    if (::GetExitCodeProcess(slot.process, &code) == FALSE)
                    {
                        retainOwnership("Process exit code could not be read; retirement is unverified.");
                        continue;
                    }
                    if (!slot.job_retirement_requested)
                    {
                        const DWORD stopped = request_job_retirement(slot);
                        if (stopped != ERROR_SUCCESS)
                        {
                            retainOwnership("Descendant stop failed; process-tree ownership is retained (Win32 "
                                + std::to_string(static_cast<unsigned long>(stopped)) + ").");
                            continue;
                        }
                        retainOwnership("Retiring remaining process-tree descendants.");
                        continue;
                    }
                    const DWORD observed = observe_retirement_members(slot);
                    const DWORD settled = settle_retirement_members(slot);
                    if (slot.retirement_observation_error == ERROR_SUCCESS)
                        slot.retirement_observation_error = observed != ERROR_SUCCESS
                            ? observed : settled;
                    if (slot.retirement_observation_error != ERROR_SUCCESS)
                    {
                        retainOwnership("Process-tree member retirement is unverified; ownership is retained (Win32 "
                            + std::to_string(static_cast<unsigned long>(slot.retirement_observation_error))
                            + "; assigned=" + std::to_string(slot.retirement_query_assigned)
                            + "; returned=" + std::to_string(slot.retirement_query_returned)
                            + "; query_error=" + std::to_string(slot.retirement_query_native_error) + ").");
                        continue;
                    }
                    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
                    if (::QueryInformationJobObject(slot.job,
                            JobObjectBasicAccountingInformation, &accounting,
                            sizeof(accounting), nullptr) == FALSE)
                    {
                        retainOwnership("Process-tree exit could not be observed; retirement is unverified (Win32 "
                            + std::to_string(static_cast<unsigned long>(::GetLastError())) + ").");
                        continue;
                    }
                    if (accounting.ActiveProcesses != 0u || !slot.retirement_members.empty())
                    {
                        // Zero job accounting alone is not synchronization.
                        // Retained member handles must also become signalled.
                        retainOwnership("Retiring remaining process-tree descendants.");
                        continue;
                    }
                    if (slot.isolation_lease)
                    {
                        std::string cleanupError{};
                        if (!slot.isolation_lease->retire(cleanupError))
                        {
                            retainOwnership("Process tree exited; restricted identity cleanup is pending: " + cleanupError);
                            continue;
                        }
                        slot.isolation_retired = true;
                        slot.isolation_lease.reset();
                    }
                    finish_slot(
                        value,
                        slot,
                        slot.failed_launch ? ProcessState::failed : ProcessState::exited,
                        static_cast<std::int32_t>(code),
                        true,
                        "Process exited; owned job is empty and observed descendants are signalled.");
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

        [[nodiscard]] bool valid_environment_utf8(std::string_view text) noexcept
        {
            std::uint32_t code{}, minimum{};
            unsigned remaining{};
            for (const unsigned char ch : text)
            {
                if (remaining != 0u)
                {
                    if ((ch & 0xc0u) != 0x80u) return false;
                    code = (code << 6u) | (ch & 0x3fu);
                    if (--remaining == 0u
                        && (code < minimum || code > 0x10ffffu
                            || (code >= 0xd800u && code <= 0xdfffu))) return false;
                }
                else if (ch <= 0x7fu) continue;
                else if (ch >= 0xc2u && ch <= 0xdfu)
                { code = ch & 0x1fu; remaining = 1u; minimum = 0x80u; }
                else if (ch >= 0xe0u && ch <= 0xefu)
                { code = ch & 0x0fu; remaining = 2u; minimum = 0x800u; }
                else if (ch >= 0xf0u && ch <= 0xf4u)
                { code = ch & 0x07u; remaining = 3u; minimum = 0x10000u; }
                else return false;
            }
            return remaining == 0u;
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
            if (request.disconnect_standard_input && request.merged_output_path.empty())
                return "Disconnected standard input requires explicit captured output.";
            if (request.isolation)
            {
#if !defined(_WIN32)
                return "Workspace OS isolation is unsupported on this platform; ordinary-token fallback is forbidden.";
#else
                if (!request.environment || !request.disconnect_standard_input
                    || !request.expected_executable || request.merged_output_path.empty())
                    return "Workspace isolation requires explicit environment, disconnected stdin, captured output and exact executable identity.";
                if (!valid_executable_path(request.isolation->owned_root)
                    || !request.isolation->owned_root.is_absolute()
                    || request.isolation->read_only.empty()
                    || request.isolation->writable.empty()
                    || request.isolation->read_only.size() + request.isolation->writable.size() > 16u)
                    return "Workspace isolation requires a bounded absolute owned generation and explicit read-only/writable trees.";
#endif
            }
            if (request.environment)
            {
                if (request.environment->size() > 128u)
                    return "Explicit process environment exceeds the entry bound.";
                std::vector<std::string> names{};
                std::size_t environmentBytes{1u};
                for (const auto& variable : *request.environment)
                {
                    if (variable.name.empty() || variable.name.size() > 128u
                        || !valid_text(variable.value, 32'767u, true)
                        || !valid_environment_utf8(variable.value))
                        return "Explicit process environment contains an invalid or oversized entry.";
                    std::string name{};
                    name.reserve(variable.name.size());
                    for (const unsigned char ch : variable.name)
                    {
                        if (ch <= 0x20u || ch >= 0x7fu || ch == '=')
                            return "Explicit process environment contains an invalid name.";
                        name.push_back(ch >= 'a' && ch <= 'z'
                            ? static_cast<char>(ch - 'a' + 'A') : static_cast<char>(ch));
                    }
                    if (std::ranges::find(names, name) != names.end())
                        return "Explicit process environment contains duplicate names.";
                    names.emplace_back(std::move(name));
                    environmentBytes += variable.name.size() + variable.value.size() + 2u;
                    if (environmentBytes > 64u * 1024u)
                        return "Explicit process environment exceeds the total byte bound.";
                }
            }
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
                .platform_window_id = process_window_id(slot),
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
                .verified_executable = slot.verified_executable,
                .environment_replaced = slot.started_tick_ns != 0u && slot.environment.has_value(),
                .standard_input_disconnected = slot.started_tick_ns != 0u && slot.disconnect_standard_input,
                .restricted_token_verified = slot.restricted_token_verified,
                .isolation_retired = slot.isolation_retired};
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

    std::optional<std::vector<EnvironmentVariable>> prepare_workspace_environment(
        const std::filesystem::path& workspace) noexcept
    {
        try
        {
            if (!ordinary_workspace_tree(workspace)) return std::nullopt;
            auto current = workspace.root_path();
            // Only descendants of the already admitted disposable root are
            // created. Never redirect the host's profile or mutate host env.
            const auto base = workspace / "cache" / "process";
            const auto profile = base / "profile";
            const auto temporary = base / "temp";
            const auto local = profile / "AppData" / "Local";
            const auto roaming = profile / "AppData" / "Roaming";
            const auto packages = base / "packages";
            for (const auto& directory : {temporary, local, roaming, packages})
            {
                current = workspace;
                for (const auto& component : directory.lexically_relative(workspace))
                {
                    current /= component;
                    std::error_code error{};
                    const bool exists = std::filesystem::exists(current, error);
                    if (error) return std::nullopt;
                    if (!exists && !std::filesystem::create_directory(current, error))
                        return std::nullopt;
                    if (error || !ordinary_workspace_directory(current)) return std::nullopt;
                }
            }
            const auto utf8 = [](const std::filesystem::path& path)
            {
                const auto bytes = path.u8string();
                return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            };
            std::vector<EnvironmentVariable> variables{
                {"TEMP", utf8(temporary)}, {"TMP", utf8(temporary)},
                {"TMPDIR", utf8(temporary)}, {"HOME", utf8(profile)},
                {"USERPROFILE", utf8(profile)}, {"APPDATA", utf8(roaming)},
                {"LOCALAPPDATA", utf8(local)}, {"NUGET_PACKAGES", utf8(packages)},
                {"DOTNET_CLI_HOME", utf8(profile)},
                {"DOTNET_CLI_TELEMETRY_OPTOUT", "1"},
                {"DOTNET_SKIP_FIRST_TIME_EXPERIENCE", "1"},
                {"DOTNET_NOLOGO", "1"}, {"MSBUILDDISABLENODEREUSE", "1"}};
#if defined(_WIN32)
            std::array<wchar_t, 32'768u> windowsBuffer{}, systemBuffer{};
            const UINT windowsSize = ::GetSystemWindowsDirectoryW(windowsBuffer.data(),
                static_cast<UINT>(windowsBuffer.size()));
            const UINT systemSize = ::GetSystemDirectoryW(systemBuffer.data(),
                static_cast<UINT>(systemBuffer.size()));
            if (windowsSize == 0u || windowsSize >= windowsBuffer.size()
                || systemSize == 0u || systemSize >= systemBuffer.size()) return std::nullopt;
            const std::filesystem::path windows{std::wstring{windowsBuffer.data(), windowsSize}};
            const std::filesystem::path system{std::wstring{systemBuffer.data(), systemSize}};
            variables.push_back({"SystemRoot", utf8(windows)});
            variables.push_back({"WINDIR", utf8(windows)});
            variables.push_back({"SystemDrive", utf8(windows.root_name())});
            variables.push_back({"COMSPEC", utf8(system / "cmd.exe")});
            variables.push_back({"PATH", utf8(system)});
            variables.push_back({"HOMEDRIVE", utf8(profile.root_name())});
            variables.push_back({"HOMEPATH", utf8(profile.root_directory() / profile.relative_path())});
            // These three OS install-root variables are the only inherited
            // entries. No PATH, proxy, model token, user profile or tool option
            // is copied. They allow MSBuild's installed SDK discovery.
            for (const auto& name : {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432"})
            {
                std::array<wchar_t, 32'768u> buffer{};
                const DWORD size = ::GetEnvironmentVariableW(name, buffer.data(),
                    static_cast<DWORD>(buffer.size()));
                if (size == 0u) continue;
                if (size >= buffer.size()) return std::nullopt;
                const std::filesystem::path path{std::wstring{buffer.data(), size}};
                if (!path.is_absolute() || !ordinary_workspace_directory(path)) return std::nullopt;
                variables.push_back({utf8(std::filesystem::path{name}), utf8(path)});
            }
#else
            variables.push_back({"PATH", "/usr/bin:/bin"});
            variables.push_back({"LANG", "C.UTF-8"});
            variables.push_back({"XDG_CACHE_HOME", utf8(base)});
            variables.push_back({"XDG_CONFIG_HOME", utf8(profile)});
#endif
            LaunchRequest validation{};
            validation.executable = workspace / "environment-validation";
            validation.correlation_key = "environment-validation";
            validation.environment = variables;
            if (validate_request(validation)) return std::nullopt;
            return variables;
        }
        catch (...) { return std::nullopt; }
    }

    std::optional<RuntimeEnvironment> prepare_runtime_environment(
        const std::filesystem::path& code_workspace, const std::string_view phase) noexcept
    {
        try
        {
            if (!ordinary_workspace_tree(code_workspace) || phase.empty() || phase.size() > 48u
                || !std::ranges::all_of(phase, [](const char c)
                    { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; }))
                return std::nullopt;
            const auto token = Clock::now().time_since_epoch().count();
            for (unsigned attempt = 0u; attempt < 16u; ++attempt)
            {
                const auto data = code_workspace.parent_path()
                    / (".epoch-runtime-" + std::string{phase} + "-"
                        + std::to_string(token) + "-" + std::to_string(attempt));
                std::error_code error{};
                if (!std::filesystem::create_directory(data, error))
                {
                    if (error) return std::nullopt;
                    continue; // Never reuse an existing phase directory.
                }
                auto environment = prepare_workspace_environment(data);
                if (!environment) return std::nullopt;
                return RuntimeEnvironment{data, std::move(*environment)};
            }
        }
        catch (...) {}
        return std::nullopt;
    }

    LaunchResult launch_or_focus(const LaunchRequest& request) noexcept
    {
        SupervisorStorage& value = storage();
        ProcessHandle reservedHandle{};
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
                        || slot.verified_executable != request.expected_executable
                        || slot.environment != request.environment
                        || slot.disconnect_standard_input != request.disconnect_standard_input
                        || slot.isolation != request.isolation
                        || slot.arguments != request.arguments
                        || slot.requested_working_directory != request.working_directory
                        || slot.requested_output_path != request.merged_output_path
                        || slot.append_output != request.append_output
                        || slot.window_mode != request.window_mode
                        || slot.exclusive_group != request.exclusive_group)
                    {
                        ++value.metrics.launch_failures;
                        return {.code = LaunchCode::invalid_request,
                            .message = "The correlation key is already bound to different executable path, artifact identity, arguments, working/output, environment, window, group, or isolation settings."};
                    }
                    if (slot.failed_launch)
                        return {.code = LaunchCode::active_group_busy,
                            .handle = handle_for(value, slot),
                            .message = "The matching failed launch still owns retirement or isolation cleanup."};
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
            reservedHandle = handle_for(value, *slot);
            LaunchReservation reservation{value, *slot};
            slot->correlation_key = request.correlation_key;
            slot->exclusive_group = request.exclusive_group;
            slot->display_name = request.display_name;
            slot->window_mode = request.window_mode;
            slot->environment = request.environment;
            slot->disconnect_standard_input = request.disconnect_standard_input;
            slot->isolation = request.isolation;
            slot->arguments = request.arguments;
            slot->requested_working_directory = request.working_directory;
            slot->requested_output_path = request.merged_output_path;
            slot->append_output = request.append_output;

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

            // Prepare every allocating success value before child execution.
            // After ResumeThread only no-throw ownership/state moves remain.
            auto verifiedExecutable = request.expected_executable;
            std::string startedMessage{"Process started."};
            LaunchResult startedResult{
                .code = LaunchCode::started,
                .handle = reservedHandle,
                .message = startedMessage};
            std::string spawnError{};
            if (!spawn_process(*slot, resolved, spawnError))
            {
                slot->message = std::move(spawnError);
                fail_reserved_launch(value, *slot);
                reservation.armed = false;
                return {
                    .code = LaunchCode::spawn_failed,
                    .handle = handle_for(value, *slot),
                    .message = slot->message};
            }

            slot->verified_executable = std::move(verifiedExecutable);
            slot->state = ProcessState::running;
            slot->started_tick_ns = now_ns();
            slot->message = std::move(startedMessage);
            ++value.metrics.launches;
            count_active_slot(value, *slot);
            revise(value);
            startedResult.focus = slot->focus_pending
                ? FocusCode::pending : FocusCode::unsupported;
            reservation.armed = false;
            return startedResult;
        }
        catch (const std::exception& error)
        {
            LaunchResult failed{
                .code = LaunchCode::spawn_failed,
                .handle = reservedHandle};
            try { failed.message = error.what(); }
            catch (...) {}
            return failed;
        }
        catch (...)
        {
            LaunchResult failed{
                .code = LaunchCode::spawn_failed,
                .handle = reservedHandle};
            try { failed.message = "Unknown process launch failure."; }
            catch (...) {}
            return failed;
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
            const DWORD stopped = request_job_retirement(*slot);
            slot->state = ProcessState::stop_requested;
            slot->focus_pending = false;
            slot->focus_deadline_ns = 0u;
            if (stopped != ERROR_SUCCESS)
            {
                slot->message = "Forced process stop failed; ownership is retained (Win32 "
                    + std::to_string(static_cast<unsigned long>(stopped)) + ").";
                revise(value);
                return StopCode::failed;
            }
            slot->message = slot->retirement_observation_error == ERROR_SUCCESS
                ? "Forced process stop requested."
                : "Forced process stop requested; member retirement is unverified (Win32 "
                    + std::to_string(static_cast<unsigned long>(slot->retirement_observation_error)) + ").";
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
