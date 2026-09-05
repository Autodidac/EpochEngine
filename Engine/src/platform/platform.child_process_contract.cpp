#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stop_token>
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
#include <winioctl.h>
#endif

import platform.child_process;

#if defined(EPOCH_PLATFORM_CHILD_PROCESS_CONTRACT_MAIN)
namespace
{
    namespace child_process = epochengine::platform::child_process;

    struct ExecutableIdentityFixture final
    {
        std::filesystem::path root{};

        ~ExecutableIdentityFixture() noexcept
        {
            if (!root.empty())
            {
                std::error_code error{};
                std::filesystem::remove_all(root, error);
            }
        }

        [[nodiscard]] bool prepare()
        {
            std::error_code error{};
            const auto temporary = std::filesystem::temp_directory_path(error);
            if (error) return false;
            const auto token = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned attempt = 0u; attempt < 16u; ++attempt)
            {
                const auto candidate = temporary / ("epoch-child-identity-"
                    + std::to_string(token) + "-" + std::to_string(attempt));
                if (std::filesystem::create_directory(candidate, error))
                {
                    root = candidate;
                    return true;
                }
                if (error) return false;
            }
            return false;
        }

        [[nodiscard]] bool write(std::string_view name, std::string_view bytes) const
        {
            std::ofstream output{root / name, std::ios::binary | std::ios::trunc};
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
            return static_cast<bool>(output);
        }
    };

    [[nodiscard]] bool inspect_identity_contract(ExecutableIdentityFixture& fixture)
    {
        const child_process::ExecutableIdentity abc{3u,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"};
        if (!abc.valid() || child_process::ExecutableIdentity{}.valid()
            || child_process::ExecutableIdentity{0u, abc.sha256}.valid()
            || child_process::ExecutableIdentity{512u * 1024u * 1024u + 1u, abc.sha256}.valid()
            || !child_process::ExecutableIdentity{512u * 1024u * 1024u, abc.sha256}.valid()
            || child_process::ExecutableIdentity{3u, std::string(64u, '0')}.valid()
            || child_process::ExecutableIdentity{3u, std::string(64u, 'A')}.valid()
            || child_process::ExecutableIdentity{3u, std::string(64u, 'g')}.valid()
            || child_process::ExecutableIdentity{3u, std::string(63u, 'a')}.valid()
            || !fixture.prepare()
            || !fixture.write("image.bin", "abc") || !fixture.write("empty.bin", {}))
            return false;
        const auto image = fixture.root / "image.bin";
        if (child_process::inspect_executable(image) != abc
            || child_process::inspect_executable({})
            || child_process::inspect_executable(fixture.root)
            || child_process::inspect_executable(fixture.root / "missing.bin")
            || child_process::inspect_executable(fixture.root / "empty.bin"))
            return false;
        child_process::LaunchRequest rejected{};
        rejected.executable = image;
        rejected.correlation_key = "contract:identity-refusal";
        rejected.window_mode = child_process::WindowMode::hidden;
        rejected.expected_executable = abc;
#if !defined(_WIN32)
        const auto unsupported = child_process::launch_or_focus(rejected);
        if (unsupported.code != child_process::LaunchCode::invalid_request
            || unsupported.message.find("unsupported on this platform") == std::string::npos)
            return false;
#endif
        rejected.expected_executable->size_bytes = 4u;
        if (child_process::launch_or_focus(rejected).code
                != child_process::LaunchCode::invalid_request)
            return false;
        rejected.expected_executable = abc;
        rejected.expected_executable->sha256.front() = '0';
        if (child_process::launch_or_focus(rejected).code
                != child_process::LaunchCode::invalid_request)
            return false;
        rejected.expected_executable = abc;
        if (!fixture.write("image.bin", "ab"))
            return false;
        const auto truncated = child_process::inspect_executable(image);
        if (!truncated || truncated->size_bytes != 2u || *truncated == abc
            || child_process::launch_or_focus(rejected).code
                != child_process::LaunchCode::invalid_request)
            return false;
        std::error_code linkError{};
        std::filesystem::create_symlink(image, fixture.root / "linked.bin", linkError);
        if (!linkError && child_process::inspect_executable(fixture.root / "linked.bin"))
            return false;
        return true;
    }
}

#if defined(_WIN32)
namespace
{
    constexpr std::string_view capture_input = "epoch-stdin-sentinel";
    constexpr std::string_view capture_stdout = "epoch-stdout-captured\n";
    constexpr std::string_view capture_stderr = "epoch-stderr-captured\n";

    struct ContractHandle final
    {
        HANDLE value{INVALID_HANDLE_VALUE};
        ~ContractHandle() noexcept
        {
            if (value != INVALID_HANDLE_VALUE && value != nullptr)
                (void)::CloseHandle(value);
        }
        [[nodiscard]] bool valid() const noexcept
        {
            return value != INVALID_HANDLE_VALUE && value != nullptr;
        }
    };

    [[nodiscard]] bool identity_lock_contract(ExecutableIdentityFixture& fixture)
    {
        const auto directory = fixture.root / "locked";
        const auto target = fixture.root / "target";
        const auto image = directory / "image.bin";
        const auto replacement = fixture.root / "replacement.bin";
        std::error_code error{};
        if (!std::filesystem::create_directory(directory, error) || error
            || !std::filesystem::create_directory(target, error) || error
            || !fixture.write("replacement.bin", "replacement"))
            return false;
        {
            std::ofstream output{image, std::ios::binary};
            const std::string block(64u * 1024u, 'x');
            for (unsigned index = 0u; index < 512u; ++index)
                output.write(block.data(), static_cast<std::streamsize>(block.size()));
            output.close();
            if (!output) return false;
        }

        const auto imageName = image.wstring();
        std::vector<std::byte> renameBytes(
            offsetof(FILE_RENAME_INFO, FileName) + (imageName.size() + 1u) * sizeof(wchar_t));
        auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(renameBytes.data());
        // FileRenameInfoEx flags from the documented FILE_RENAME_INFORMATION
        // contract; these names are WDK-only in some desktop SDK versions.
        constexpr DWORD replaceIfExists = 0x00000001u;
        constexpr DWORD posixSemantics = 0x00000002u;
        const DWORD renameFlags = replaceIfExists | posixSemantics;
        // Flags overlays ReplaceIfExists in the Windows 10 FileRenameInfoEx ABI.
        std::memcpy(rename, &renameFlags, sizeof(renameFlags));
        rename->FileNameLength = static_cast<DWORD>(imageName.size() * sizeof(wchar_t));
        std::memcpy(rename->FileName, imageName.c_str(), (imageName.size() + 1u) * sizeof(wchar_t));
        ContractHandle replacementHandle{::CreateFileW(replacement.c_str(), DELETE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (!replacementHandle.valid()) return false;

        struct MountPointHeader final
        {
            DWORD tag{};
            WORD data_length{};
            WORD reserved{};
            WORD substitute_offset{};
            WORD substitute_length{};
            WORD print_offset{};
            WORD print_length{};
        };
        static_assert(sizeof(MountPointHeader) == 16u);
        const auto printName = target.wstring();
        const auto substituteName = L"\\??\\" + printName;
        const std::size_t substituteBytes = (substituteName.size() + 1u) * sizeof(wchar_t);
        const std::size_t printBytes = (printName.size() + 1u) * sizeof(wchar_t);
        MountPointHeader reparseHeader{
            .tag = IO_REPARSE_TAG_MOUNT_POINT,
            .data_length = static_cast<WORD>(8u + substituteBytes + printBytes),
            .substitute_length = static_cast<WORD>(substituteBytes - sizeof(wchar_t)),
            .print_offset = static_cast<WORD>(substituteBytes),
            .print_length = static_cast<WORD>(printBytes - sizeof(wchar_t))};
        std::vector<std::byte> reparse(sizeof(reparseHeader) + substituteBytes + printBytes);
        std::memcpy(reparse.data(), &reparseHeader, sizeof(reparseHeader));
        std::memcpy(reparse.data() + sizeof(reparseHeader), substituteName.c_str(), substituteBytes);
        std::memcpy(reparse.data() + sizeof(reparseHeader) + substituteBytes, printName.c_str(), printBytes);

        std::atomic_bool finished{};
        std::optional<child_process::ExecutableIdentity> observed{};
        std::jthread inspector{[&]
        {
            observed = child_process::inspect_executable(image);
            finished.store(true, std::memory_order_release);
        }};
        bool locked{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (!finished.load(std::memory_order_acquire)
            && std::chrono::steady_clock::now() < deadline)
        {
            // A failed write-open is evidence the actual production image
            // lock is held. No test-only callback changes production behavior.
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
            ContractHandle probe{::CreateFileW(image.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
            if (!probe.valid() && ::GetLastError() == ERROR_SHARING_VIOLATION)
            {
                locked = true;
                break;
            }
        }
        if (!locked || finished.load(std::memory_order_acquire))
            return false;

        ContractHandle truncate{::CreateFileW(image.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        const DWORD truncateError = truncate.valid() ? ERROR_SUCCESS : ::GetLastError();
        const bool renamed = ::MoveFileExW(directory.c_str(),
            (fixture.root / "renamed").c_str(), 0u) != FALSE;
        const DWORD renameError = renamed ? ERROR_SUCCESS : ::GetLastError();
        ContractHandle attributes{::CreateFileW(directory.c_str(), FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
        DWORD returned{};
        const bool retagged = attributes.valid()
            && ::DeviceIoControl(attributes.value, FSCTL_SET_REPARSE_POINT,
                reparse.data(), static_cast<DWORD>(reparse.size()), nullptr, 0u,
                &returned, nullptr) != FALSE;
        const DWORD reparseError = retagged ? ERROR_SUCCESS : ::GetLastError();
        const bool replaced = ::SetFileInformationByHandle(replacementHandle.value,
            FileRenameInfoEx, rename, static_cast<DWORD>(renameBytes.size())) != FALSE;
        const DWORD replaceError = replaced ? ERROR_SUCCESS : ::GetLastError();
        ContractHandle finalProbe{::CreateFileW(image.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        const DWORD finalProbeError = finalProbe.valid() ? ERROR_SUCCESS : ::GetLastError();
        const bool lockSpannedChecks = !finalProbe.valid()
            && finalProbeError == ERROR_SHARING_VIOLATION
            && !finished.load(std::memory_order_acquire);
        inspector.join();
        return lockSpannedChecks && observed && observed->size_bytes == 32u * 1024u * 1024u
            && !truncate.valid() && truncateError == ERROR_SHARING_VIOLATION
            && !renamed && (renameError == ERROR_SHARING_VIOLATION || renameError == ERROR_ACCESS_DENIED)
            && attributes.valid() && !retagged && reparseError == ERROR_DIR_NOT_EMPTY
            && !replaced && (replaceError == ERROR_SHARING_VIOLATION || replaceError == ERROR_ACCESS_DENIED);
    }

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
    ExecutableIdentityFixture identityFixture{};
    if (!inspect_identity_contract(identityFixture))
        return 18;
    const auto selfIdentity = child_process::inspect_executable(self);
    if (!selfIdentity || !selfIdentity->valid())
        return 19;
#if defined(_WIN32)
    if (!identity_lock_contract(identityFixture))
        return 23;
#endif
    child_process::LaunchRequest first{};
    first.executable = self;
    first.working_directory = self.parent_path();
    first.arguments = {"--epoch-child-contract", "30000", "17"};
    first.correlation_key = "contract:first";
    first.exclusive_group = "contract:exclusive";
    first.display_name = "Child Process Contract";
    first.window_mode = child_process::WindowMode::hidden;
#if defined(_WIN32)
    first.expected_executable = selfIdentity;
#endif

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
        || running->platform_window_id != 0u
        || running->verified_executable != first.expected_executable)
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

    child_process::LaunchRequest wrongCorrelation = first;
    wrongCorrelation.executable = identityFixture.root / self.filename();
    std::error_code identityError{};
    if (!std::filesystem::copy_file(self, wrongCorrelation.executable,
            std::filesystem::copy_options::none, identityError) || identityError)
        return 20;
    const auto wrongPath = child_process::launch_or_focus(wrongCorrelation);
    if (wrongPath.code != child_process::LaunchCode::invalid_request
        || wrongPath.message.find("correlation key") == std::string::npos)
        return 21;
#if defined(_WIN32)
    wrongCorrelation = first;
    wrongCorrelation.expected_executable.reset();
    const auto weakenedIdentity = child_process::launch_or_focus(wrongCorrelation);
    if (weakenedIdentity.code != child_process::LaunchCode::invalid_request
        || weakenedIdentity.message.find("artifact identity") == std::string::npos)
        return 22;
#endif

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
    // Keep ordinary captured launches on their original, unverified path.
    natural.expected_executable.reset();
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
