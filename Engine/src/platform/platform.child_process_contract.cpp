#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stop_token>
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
#include <winsock2.h>
#include <Windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <winioctl.h>
#elif defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

import platform.child_process;

#if defined(EPOCH_PLATFORM_CHILD_PROCESS_CONTRACT_MAIN)
namespace
{
    namespace child_process = epochengine::platform::child_process;

    [[nodiscard]] bool launch_result_ownership_contract()
    {
        using child_process::LaunchCode;
        for (const auto code : {LaunchCode::started, LaunchCode::focused_existing,
                LaunchCode::active_group_busy, LaunchCode::invalid_request,
                LaunchCode::capacity_exhausted, LaunchCode::spawn_failed})
        {
            child_process::LaunchResult result{.code = code};
            if (result.owns_new_process()) return false;
            result.handle = {.slot = 0u, .generation = 1u};
            const bool owns = code == LaunchCode::started || code == LaunchCode::spawn_failed
                || code == LaunchCode::invalid_request;
            if (result.owns_new_process() != owns
                || static_cast<bool>(result) != (code == LaunchCode::started
                    || code == LaunchCode::focused_existing)) return false;
        }
        return !child_process::LaunchResult{}.owns_new_process();
    }

    struct ExecutableIdentityFixture final
    {
        std::filesystem::path root{};
        bool preserve_evidence{};

        ~ExecutableIdentityFixture() noexcept
        {
            if (!root.empty() && !preserve_evidence)
            {
                std::error_code error{};
                std::filesystem::remove_all(root, error);
            }
        }

        [[nodiscard]] bool prepare(std::string_view prefix = "epoch-child-identity-")
        {
            std::error_code error{};
            const auto temporary = std::filesystem::temp_directory_path(error);
            if (error) return false;
            const auto token = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned attempt = 0u; attempt < 16u; ++attempt)
            {
                const auto candidate = temporary / (std::string{prefix}
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

    [[nodiscard]] bool environment_validation_contract(const std::filesystem::path& self)
    {
        using child_process::EnvironmentVariable;
        child_process::LaunchRequest request{};
        request.executable = self;
        request.correlation_key = "contract:invalid-environment";
        request.window_mode = child_process::WindowMode::hidden;
        const auto rejected = [&](std::vector<EnvironmentVariable> entries)
        {
            request.environment = std::move(entries);
            return child_process::launch_or_focus(request).code
                == child_process::LaunchCode::invalid_request;
        };
        for (const auto& name : std::vector<std::string>{
                {}, std::string(129u, 'a'), "has=name", "has space", "has\tspace",
                "has\nline", std::string{"bad\0name", 8u}, std::string(1u, '\x7f'),
                "nonascii_\xc3\xa9"})
        {
            if (!rejected({{name, "value"}})) return false;
        }
        if (!rejected({{"Duplicate", "one"}, {"duplicate", "two"}})
            || !rejected({{"same", "one"}, {"same", "two"}})
            || !rejected({{"VALUE", std::string{"safe\0hidden", 11u}}})
            || !rejected({{"VALUE", std::string(32768u, 'v')}})
            || !rejected({{"VALUE", "invalid_\xc0\xaf"}})
            || !rejected({{"A", std::string(32767u, 'a')},
                          {"B", std::string(32767u, 'b')},
                          {"C", std::string(32767u, 'c')}}))
            return false;
        std::vector<EnvironmentVariable> excessive{};
        for (unsigned index = 0u; index < 129u; ++index)
            excessive.push_back({"ENTRY_" + std::to_string(index), "v"});
        if (!rejected(std::move(excessive))) return false;
        request.environment.reset();
        request.disconnect_standard_input = true;
        return child_process::launch_or_focus(request).code
            == child_process::LaunchCode::invalid_request;
    }

    [[nodiscard]] bool failed_launch_ownership_contract(const ExecutableIdentityFixture& fixture)
    {
        // The identity fixture contains ordinary data, never executable code.
        // Native spawn/exec refusal still returns an owned reservation that the
        // caller must settle and release, independently of launch success.
        child_process::LaunchRequest request{};
        request.executable = fixture.root / "image.bin";
        request.working_directory = fixture.root;
        request.correlation_key = "contract:failed-launch-ownership";
        request.window_mode = child_process::WindowMode::hidden;
        const auto rejected = child_process::launch_or_focus(request);
        if (!rejected.handle.valid()) return false;
        const auto settled = child_process::wait(rejected.handle, {}, 5'000'000'000ull);
        const bool released = child_process::release(rejected.handle);
        return rejected.code == child_process::LaunchCode::spawn_failed
            && rejected.owns_new_process() && !static_cast<bool>(rejected)
            && settled.code == child_process::WaitCode::failed && settled.process
            && settled.process->state == child_process::ProcessState::failed
            && !settled.process->active() && released
            && !child_process::snapshot(rejected.handle)
            && child_process::metrics().active_processes == 0u;
    }

    [[nodiscard]] bool isolation_request_contract(const std::filesystem::path& self)
    {
        child_process::LaunchRequest request{};
        request.executable = self;
        request.correlation_key = "contract:invalid-isolation";
        request.isolation.emplace();
        const auto before = child_process::metrics().launches;
        const auto rejected = child_process::launch_or_focus(request);
        if (rejected.code != child_process::LaunchCode::invalid_request
            || child_process::metrics().launches != before) return false;
#if !defined(_WIN32)
        return rejected.message.find("unsupported on this platform") != std::string::npos;
#else
        // No profile or permission changes: each malformed policy is refused
        // by the pure request validator before native process preparation.
        request.environment.emplace();
        request.disconnect_standard_input = true;
        request.merged_output_path = self.parent_path() / "never-created.log";
        request.expected_executable = child_process::inspect_executable(self);
        if (!request.expected_executable) return false;
        for (unsigned variation = 0u; variation < 4u; ++variation)
        {
            request.isolation = child_process::WorkspaceIsolation{self.parent_path(),
                {self.parent_path() / "code"}, {self.parent_path() / "scratch"}};
            if (variation == 0u) request.isolation->owned_root = "relative";
            if (variation == 1u) request.isolation->read_only.clear();
            if (variation == 2u) request.isolation->writable.clear();
            if (variation == 3u) request.isolation->writable.resize(17u, self.parent_path() / "scratch");
            if (child_process::launch_or_focus(request).code != child_process::LaunchCode::invalid_request)
                return false;
        }
        return child_process::metrics().launches == before;
#endif
    }

    [[nodiscard]] bool create_fixture_directory_link(
        const std::filesystem::path& link, const std::filesystem::path& target)
    {
#if defined(_WIN32)
        std::error_code error{};
        if (!std::filesystem::create_directory(link, error) || error) return false;
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
        const auto substituteBytes = (substituteName.size() + 1u) * sizeof(wchar_t);
        const auto printBytes = (printName.size() + 1u) * sizeof(wchar_t);
        if (substituteBytes + printBytes + 8u > 16u * 1024u) return false;
        MountPointHeader header{
            .tag = IO_REPARSE_TAG_MOUNT_POINT,
            .data_length = static_cast<WORD>(8u + substituteBytes + printBytes),
            .substitute_length = static_cast<WORD>(substituteBytes - sizeof(wchar_t)),
            .print_offset = static_cast<WORD>(substituteBytes),
            .print_length = static_cast<WORD>(printBytes - sizeof(wchar_t))};
        std::vector<std::byte> data(sizeof(header) + substituteBytes + printBytes);
        std::memcpy(data.data(), &header, sizeof(header));
        std::memcpy(data.data() + sizeof(header), substituteName.c_str(), substituteBytes);
        std::memcpy(data.data() + sizeof(header) + substituteBytes,
            printName.c_str(), printBytes);
        const HANDLE handle = ::CreateFileW(link.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;
        DWORD returned{};
        const bool created = ::DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT,
            data.data(), static_cast<DWORD>(data.size()), nullptr, 0u,
            &returned, nullptr) != FALSE;
        (void)::CloseHandle(handle);
        return created;
#else
        std::error_code error{};
        std::filesystem::create_directory_symlink(target, link, error);
        return !error;
#endif
    }

    [[nodiscard]] bool workspace_environment_contract(const ExecutableIdentityFixture& fixture)
    {
        std::error_code error{};
        const auto first = fixture.root / "environment-first";
        const auto second = fixture.root / "environment-second";
        const auto outside = fixture.root / "environment-outside";
        const auto poisoned = fixture.root / "environment-poisoned";
        for (const auto& root : {first, second, outside, poisoned})
            if (!std::filesystem::create_directory(root, error) || error) return false;
        if (child_process::prepare_workspace_environment({})
            || child_process::prepare_workspace_environment("relative")
            || child_process::prepare_workspace_environment(fixture.root.root_path())
            || child_process::prepare_workspace_environment(fixture.root / "missing-workspace")
            || child_process::prepare_workspace_environment(fixture.root / "image.bin"))
            return false;
        const auto firstEnvironment = child_process::prepare_workspace_environment(first);
        const auto secondEnvironment = child_process::prepare_workspace_environment(second);
        if (!firstEnvironment || !secondEnvironment
            || *firstEnvironment == *secondEnvironment) return false;
        const auto utf8 = [](const std::filesystem::path& path)
        {
            const auto bytes = path.u8string();
            return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        };
        const auto verify = [&](const auto& environment, const std::filesystem::path& root)
        {
            const auto lookup = [&](std::string_view name) -> std::optional<std::string>
            {
                for (const auto& entry : environment)
                    if (entry.name == name) return entry.value;
                return std::nullopt;
            };
            if (lookup("EPOCH_CHILD_PRIVATE_SENTINEL")) return false;
            const auto process = root / "cache" / "process";
            const std::vector<std::pair<std::string, std::filesystem::path>> expected{
                {"TEMP", process / "temp"}, {"TMP", process / "temp"},
                {"TMPDIR", process / "temp"}, {"HOME", process / "profile"},
                {"USERPROFILE", process / "profile"},
                {"APPDATA", process / "profile" / "AppData" / "Roaming"},
                {"LOCALAPPDATA", process / "profile" / "AppData" / "Local"},
                {"DOTNET_CLI_HOME", process / "profile"},
                {"NUGET_PACKAGES", process / "packages"}};
            for (const auto& [name, path] : expected)
                if (lookup(name) != utf8(path) || !std::filesystem::is_directory(path, error)
                    || error) return false;
            return true;
        };
        if (!verify(*firstEnvironment, first) || !verify(*secondEnvironment, second)
            || child_process::prepare_workspace_environment(first) != firstEnvironment)
            return false;

        // Links point only into this owned fixture. The factory must refuse
        // before creating a process cache through either redirected root.
        const auto linkedRoot = fixture.root / "environment-root-link";
        const auto linkedCache = poisoned / "cache";
        if (!create_fixture_directory_link(linkedRoot, outside)
            || !create_fixture_directory_link(linkedCache, outside)) return false;
        const bool rejected = !child_process::prepare_workspace_environment(linkedRoot)
            && !child_process::prepare_workspace_environment(poisoned)
            && !std::filesystem::exists(outside / "cache", error) && !error
            && !std::filesystem::exists(outside / "process", error) && !error;
        if (!std::filesystem::remove(linkedRoot, error) || error
            || !std::filesystem::remove(linkedCache, error) || error)
            return false;
        return rejected;
    }
}

#if defined(__linux__)
namespace
{
    constexpr std::string_view descriptor_stdout = "epoch-linux-stdout-captured\n";
    constexpr std::string_view descriptor_stderr = "epoch-linux-stderr-captured\n";

    struct InheritableDescriptorFixture final
    {
        int channel[2]{-1, -1};
        int high{-1};
        struct stat identity{};

        ~InheritableDescriptorFixture() noexcept
        {
            if (high >= 0) (void)::close(high);
            for (const auto descriptor : channel)
                if (descriptor >= 0) (void)::close(descriptor);
        }

        [[nodiscard]] bool prepare()
        {
            if (::pipe(channel) != 0) return false;
            high = ::fcntl(channel[0], F_DUPFD, 128);
            if (high < 0 || ::fstat(channel[0], &identity) != 0) return false;
            // F_DUPFD deliberately does not set CLOEXEC. The low pipe ends and
            // sparse high duplicate straddle the launcher's setup channel.
            for (const int descriptor : {channel[0], channel[1], high})
                if (descriptor <= STDERR_FILENO || ::fcntl(descriptor, F_GETFD) != 0)
                    return false;
            return true;
        }
    };

    struct DescriptorLimitFixture final
    {
        struct rlimit original{};
        bool changed{};

        ~DescriptorLimitFixture() noexcept
        {
            if (changed) (void)::setrlimit(RLIMIT_NOFILE, &original);
        }

        [[nodiscard]] bool lower_below(int descriptor)
        {
            if (::getrlimit(RLIMIT_NOFILE, &original) != 0
                || descriptor < 128 || original.rlim_cur <= 128u)
                return false;
            auto lowered = original;
            lowered.rlim_cur = 128u;
            changed = ::setrlimit(RLIMIT_NOFILE, &lowered) == 0;
            return changed;
        }

        [[nodiscard]] bool restore() noexcept
        {
            if (changed && ::setrlimit(RLIMIT_NOFILE, &original) != 0) return false;
            changed = false;
            return true;
        }
    };

    [[nodiscard]] int run_descriptor_inheritance_child(char** arguments)
    {
        const auto device = std::stoull(arguments[5]);
        const auto inode = std::stoull(arguments[6]);
        for (int index = 2; index != 5; ++index)
        {
            struct stat observed{};
            if (::fstat(std::stoi(arguments[index]), &observed) == 0)
            {
                // A loader may reuse a now-free number. Only an inherited
                // reference to the exact parent pipe is the leak under test.
                if (static_cast<std::uint64_t>(observed.st_dev) == device
                    && static_cast<std::uint64_t>(observed.st_ino) == inode)
                    return 71;
            }
            else if (errno != EBADF) return 72;
        }
        char input{};
        if (::read(STDIN_FILENO, &input, 1u) != 0) return 73;
        const auto write_all = [](int descriptor, std::string_view bytes)
        {
            while (!bytes.empty())
            {
                const auto written = ::write(descriptor, bytes.data(), bytes.size());
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) return false;
                bytes.remove_prefix(static_cast<std::size_t>(written));
            }
            return true;
        };
        return write_all(STDOUT_FILENO, descriptor_stdout)
            && write_all(STDERR_FILENO, descriptor_stderr) ? 17 : 74;
    }

    [[nodiscard]] bool descriptor_inheritance_contract(
        const std::filesystem::path& self, const ExecutableIdentityFixture& fixture)
    {
        InheritableDescriptorFixture inherited{};
        if (!inherited.prepare()) return false;
        for (const bool explicitEnvironment : {false, true})
        {
            child_process::LaunchRequest request{};
            request.executable = self;
            request.working_directory = self.parent_path();
            request.correlation_key = explicitEnvironment
                ? "contract:linux-fds-execve" : "contract:linux-fds-execv";
            request.window_mode = child_process::WindowMode::hidden;
            request.merged_output_path = fixture.root / (explicitEnvironment
                ? "linux-fds-execve.log" : "linux-fds-execv.log");
            request.arguments = {"--epoch-descriptor-inheritance-contract",
                std::to_string(inherited.channel[0]), std::to_string(inherited.channel[1]),
                std::to_string(inherited.high),
                std::to_string(static_cast<std::uint64_t>(inherited.identity.st_dev)),
                std::to_string(static_cast<std::uint64_t>(inherited.identity.st_ino))};
            if (explicitEnvironment) request.environment.emplace();
            request.disconnect_standard_input = explicitEnvironment;
            DescriptorLimitFixture limit{};
            // The second child inherits a limit below an already-open FD.
            // Iterating only to sysconf(_SC_OPEN_MAX) would miss that FD.
            if (explicitEnvironment && !limit.lower_below(inherited.high)) return false;
            const auto launched = child_process::launch_or_focus(request);
            const bool restored = limit.restore();
            if (launched.code != child_process::LaunchCode::started) return false;
            const auto completed = child_process::wait(launched.handle, {}, 5'000'000'000ull);
            if (!completed || !completed.process || completed.process->active())
            {
                (void)child_process::stop(launched.handle, child_process::StopMode::force);
                (void)child_process::wait(launched.handle, {}, 5'000'000'000ull);
                (void)child_process::release(launched.handle);
                return false;
            }
            const bool released = child_process::release(launched.handle);
            if (!restored || !released || !completed.process->exit_code_valid
                || completed.process->exit_code != 17
                || completed.process->environment_replaced != explicitEnvironment
                || completed.process->standard_input_disconnected != explicitEnvironment)
                return false;
            std::ifstream output{request.merged_output_path, std::ios::binary};
            const std::string bytes{std::istreambuf_iterator<char>{output},
                std::istreambuf_iterator<char>{}};
            if (bytes != std::string{descriptor_stdout} + std::string{descriptor_stderr})
                return false;
            for (const int descriptor : {inherited.channel[0], inherited.channel[1], inherited.high})
                if (::fcntl(descriptor, F_GETFD) != 0) return false;
        }

        // A failed exec after descriptor cleanup must still reach the parent
        // through the retained CLOEXEC writer, with its original errno.
        const auto invalidImage = fixture.root / "linux-invalid-executable.bin";
        if (!fixture.write("linux-invalid-executable.bin", "not an executable\n")
            || ::chmod(invalidImage.c_str(), 0700) != 0) return false;
        child_process::LaunchRequest invalid{};
        invalid.executable = invalidImage;
        invalid.working_directory = fixture.root;
        invalid.correlation_key = "contract:linux-exec-errno";
        invalid.window_mode = child_process::WindowMode::hidden;
        const auto rejected = child_process::launch_or_focus(invalid);
        const auto failed = child_process::snapshot(rejected.handle);
        const bool released = child_process::release(rejected.handle);
        return rejected.code == child_process::LaunchCode::spawn_failed
            && rejected.message == "exec failed with errno " + std::to_string(ENOEXEC) + "."
            && failed && failed->state == child_process::ProcessState::failed
            && !failed->active() && failed->platform_process_id == 0u && released
            && child_process::metrics().active_processes == 0u;
    }
}
#endif

#if defined(_WIN32)
namespace
{
    constexpr std::string_view capture_input = "epoch-stdin-sentinel";
    constexpr std::string_view capture_stdout = "epoch-stdout-captured\n";
    constexpr std::string_view capture_stderr = "epoch-stderr-captured\n";
    constexpr std::wstring_view private_environment_name = L"EPOCH_CHILD_PRIVATE_SENTINEL";
    constexpr std::wstring_view private_environment_value = L"synthetic-host-only-value";
    constexpr std::wstring_view explicit_environment_name = L"EPOCH_CHILD_EXPLICIT_UTF8";
    constexpr std::wstring_view empty_environment_name = L"EPOCH_CHILD_EXPLICIT_EMPTY";

    [[nodiscard]] bool lookup_environment(
        std::wstring_view name, std::optional<std::wstring>& value)
    {
        value.reset();
        const auto block = ::GetEnvironmentStringsW();
        if (!block) return false;
        for (const wchar_t* cursor = block; *cursor != L'\0';)
        {
            const std::wstring_view entry{cursor};
            const auto separator = entry.find(L'=');
            if (separator != std::wstring_view::npos
                && entry.substr(0u, separator) == name)
            {
                value = entry.substr(separator + 1u);
                break;
            }
            cursor += entry.size() + 1u;
        }
        (void)::FreeEnvironmentStringsW(block);
        return true;
    }

    struct PrivateEnvironmentFixture final
    {
        std::optional<std::wstring> previous{};
        bool changed{};

        [[nodiscard]] bool prepare()
        {
            if (!lookup_environment(private_environment_name, previous)) return false;
            changed = ::SetEnvironmentVariableW(private_environment_name.data(),
                private_environment_value.data()) != FALSE;
            return changed;
        }

        ~PrivateEnvironmentFixture() noexcept
        {
            if (changed)
                (void)::SetEnvironmentVariableW(private_environment_name.data(),
                    previous ? previous->c_str() : nullptr);
        }
    };

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

    // Console-only descendant fixture. A real inherited Job Object must own
    // the grandchild even when this immediate child exits normally first.
    [[nodiscard]] int run_retirement_parent(const std::filesystem::path& root, bool churn)
    {
        if (!root.is_absolute()
            || !root.filename().string().starts_with("epoch-child-retirement-"))
            return 150;
        std::wstring self(32'768u, L'\0');
        const DWORD length = ::GetModuleFileNameW(nullptr, self.data(),
            static_cast<DWORD>(self.size()));
        if (length == 0u || length >= self.size()) return 151;
        self.resize(length);
        std::wstring command = L"\"" + self
            + L"\" --epoch-child-contract 30000 0";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!::CreateProcessW(self.c_str(), command.data(), nullptr, nullptr,
                FALSE, CREATE_NO_WINDOW, nullptr, root.c_str(), &startup, &process))
            return 152;
        ContractHandle descendant{process.hProcess};
        ContractHandle thread{process.hThread};
        BOOL inJob{};
        if (!::IsProcessInJob(descendant.value, nullptr, &inJob) || !inJob)
        {
            (void)::TerminateProcess(descendant.value, 153u);
            (void)::WaitForSingleObject(descendant.value, 5'000u);
            return 153;
        }
        {
            std::ofstream record{root / "descendant.pid", std::ios::binary};
            record << process.dwProcessId << '\n';
            record.close();
            if (!record) return 154;
        }
        if (churn)
        {
            // Exercise real membership snapshots while short-lived sibling
            // processes finish. A vanished PID must not pin an empty group.
            for (unsigned index = 0u; index < 32u; ++index)
            {
                std::wstring transientCommand = L"\"" + self
                    + L"\" --epoch-child-contract " + std::to_wstring(index % 3u) + L" 0";
                PROCESS_INFORMATION transient{};
                if (!::CreateProcessW(self.c_str(), transientCommand.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, root.c_str(), &startup, &transient))
                    return 156;
                ContractHandle transientProcess{transient.hProcess};
                ContractHandle transientThread{transient.hThread};
            }
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::error_code error{};
            if (std::filesystem::exists(root / "parent.exit", error) && !error)
                return 37;
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return 155;
    }

    struct OwnedContractProcess final
    {
        child_process::ProcessHandle handle{};
        ~OwnedContractProcess() noexcept
        {
            if (!handle.valid()) return;
            std::stop_source stop{};
            (void)stop.request_stop();
            (void)child_process::wait(handle, stop.get_token(), 5'000'000'000ull);
            (void)child_process::release(handle);
        }
    };

    [[nodiscard]] bool exit_code_259_contract(const std::filesystem::path& self)
    {
        child_process::LaunchRequest request{};
        request.executable = self;
        request.working_directory = self.parent_path();
        request.arguments = {"--epoch-child-contract", "20", "259"};
        request.correlation_key = "contract:actual-exit-259";
        request.window_mode = child_process::WindowMode::hidden;
        const auto launch = child_process::launch_or_focus(request);
        if (launch.code != child_process::LaunchCode::started) return false;
        OwnedContractProcess owner{launch.handle};
        const auto result = child_process::wait(launch.handle, {}, 2'000'000'000ull);
        return result.code == child_process::WaitCode::exited && result.process
            && !result.process->active() && result.process->exit_code_valid
            && result.process->exit_code == 259;
    }

    [[nodiscard]] bool descendant_retirement_contract(
        const std::filesystem::path& self, std::string_view mode)
    {
        ExecutableIdentityFixture fixture{};
        if (!fixture.prepare("epoch-child-retirement-")) return false;
        fixture.preserve_evidence = true;
        const auto fail = [&](std::string_view detail)
        {
            (void)fixture.write("retirement.evidence.txt",
                "mode=" + std::string{mode} + "\nresult=FAIL\ndetail=" + std::string{detail} + "\n");
            return false;
        };
        child_process::LaunchRequest request{};
        request.executable = self;
        request.working_directory = fixture.root;
        request.arguments = {"--epoch-retirement-parent", fixture.root.string()};
        if (mode == "natural_churn") request.arguments.emplace_back("churn");
        request.correlation_key = "contract:descendant-retirement-" + std::string{mode};
        request.exclusive_group = "contract:descendant-retirement";
        request.window_mode = child_process::WindowMode::hidden;
        const auto launch = child_process::launch_or_focus(request);
        if (launch.code != child_process::LaunchCode::started) return fail(launch.message);
        OwnedContractProcess owner{launch.handle};
        const auto running = child_process::snapshot(launch.handle);
        if (!running || !running->active()) return fail("Parent is not active after launch.");
        ContractHandle parent{::OpenProcess(SYNCHRONIZE, FALSE,
            static_cast<DWORD>(running->platform_process_id))};
        if (!parent.valid()) return fail("Parent observation handle could not be opened.");

        DWORD descendantId{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::ifstream record{fixture.root / "descendant.pid", std::ios::binary};
            if (record >> descendantId && descendantId != 0u) break;
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        if (descendantId == 0u) return fail("Descendant identity was not reported.");
        ContractHandle descendant{::OpenProcess(SYNCHRONIZE, FALSE, descendantId)};
        if (!descendant.valid()
            || ::WaitForSingleObject(descendant.value, 0u) != WAIT_TIMEOUT)
            return fail("Descendant observation handle is not live.");

        std::stop_source cancellation{};
        const bool natural = mode == "natural" || mode == "natural_churn";
        if (natural)
        {
            if (!fixture.write("parent.exit", "exit")
                || ::WaitForSingleObject(parent.value, 3'000u) != WAIT_OBJECT_0
                || ::WaitForSingleObject(descendant.value, 0u) != WAIT_TIMEOUT)
                return fail("Parent exit/descendant liveness preconditions did not hold.");
            // The immediate process is dead but its descendant is not. The
            // first poll must retain ownership, not release the job and claim
            // completion from the parent's exit code alone.
            const auto retiring = child_process::snapshot(launch.handle);
            if (!retiring || !retiring->active() || retiring->exit_code_valid)
                return fail("First parent-exit poll lost retirement ownership: "
                    + (retiring ? retiring->message : std::string{"no snapshot"}));
        }
        else if (mode == "cancel") (void)cancellation.request_stop();
        else if (mode != "timeout") return fail("Unknown retirement fixture mode.");

        const auto result = child_process::wait(launch.handle, cancellation.get_token(),
            mode == "timeout" ? 20'000'000ull : 5'000'000'000ull);
        const auto expected = natural ? child_process::WaitCode::exited
            : (mode == "cancel" ? child_process::WaitCode::cancelled
                : child_process::WaitCode::timed_out);
        const DWORD parentWait = ::WaitForSingleObject(parent.value, 0u);
        const DWORD descendantWait = ::WaitForSingleObject(descendant.value, 0u);
        const bool groupActive = child_process::active_in_group(request.exclusive_group).has_value();
        const bool success = result.code == expected && result.process && !result.process->active()
            && result.process->exit_code_valid
            && (!natural || result.process->exit_code == 37)
            && parentWait == WAIT_OBJECT_0 && descendantWait == WAIT_OBJECT_0 && !groupActive;
        if (!success) return fail("Terminal retirement check failed: " + result.message
            + "; process=" + (result.process ? result.process->message : "missing")
            + "; wait=" + std::string{child_process::wait_code_name(result.code)}
            + "; exit=" + (result.process ? std::to_string(result.process->exit_code) : "missing")
            + "; parent_wait=" + std::to_string(parentWait)
            + "; descendant_wait=" + std::to_string(descendantWait)
            + "; group_active=" + std::to_string(groupActive));
        fixture.preserve_evidence = false;
        return true;
    }

    [[nodiscard]] int run_retirement_contracts(const std::filesystem::path& self)
    {
        if (!exit_code_259_contract(self)) return 60;
        if (!descendant_retirement_contract(self, "natural")) return 61;
        if (!descendant_retirement_contract(self, "cancel")) return 62;
        if (!descendant_retirement_contract(self, "timeout")) return 63;
        if (!descendant_retirement_contract(self, "natural_churn")) return 64;
        return 0;
    }

    // This opt-in probe launches only a copy of this console component, with
    // literal synthetic fixtures. It never launches an editor, GPU context,
    // model request, server/listener, or generated project.
    [[nodiscard]] int run_isolation_child(char** argv)
    {
        const auto code = std::filesystem::path{argv[2]};
        const auto scratch = std::filesystem::path{argv[3]};
        const auto outside = std::filesystem::path{argv[4]};
        const auto readable = [&](const std::filesystem::path& path)
        {
            ContractHandle file{::CreateFileW(path.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
            if (!file.valid()) return false;
            char bytes[64]{};
            DWORD count{};
            return ::ReadFile(file.value, bytes, sizeof(bytes), &count, nullptr) != FALSE
                && std::string_view{bytes, count} == "immutable-input";
        };
        const auto denied = [&](const std::filesystem::path& path, DWORD access)
        {
            ContractHandle file{::CreateFileW(path.c_str(), access,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
            return !file.valid() && ::GetLastError() == ERROR_ACCESS_DENIED;
        };
        if (!readable(code / "input.txt")) return 151;
        if (!denied(code / "input.txt", GENERIC_WRITE)
            || !denied(code / "input.txt", WRITE_DAC | WRITE_OWNER)) return 152;
        if (!denied(outside, GENERIC_READ) || !denied(outside, GENERIC_WRITE)
            || !denied(outside, WRITE_DAC | WRITE_OWNER)) return 153;
        {
            ContractHandle file{::CreateFileW((scratch / "result.txt").c_str(),
                GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
            DWORD written{};
            constexpr std::string_view marker = "owned-scratch-only";
            if (!file.valid() || ::WriteFile(file.value, marker.data(),
                    static_cast<DWORD>(marker.size()), &written, nullptr) == FALSE
                || written != marker.size()) return 154;
        }
        if (::CreateHardLinkW((scratch / "result-alias.txt").c_str(),
                (scratch / "result.txt").c_str(), nullptr) == FALSE) return 158;
        // Connect only to the already operator-approved local model endpoint;
        // send no bytes. A refused/absent server is NOT network-policy proof.
        WSADATA sockets{};
        const int socketStartup = ::WSAStartup(MAKEWORD(2, 2), &sockets);
        if (socketStartup != 0)
        {
            const std::string diagnostic = "file-access-checks=passed\nwinsock-startup-error="
                + std::to_string(socketStartup) + "\nconnection-denial-check=not-exercised\n";
            DWORD written{};
            (void)::WriteFile(::GetStdHandle(STD_OUTPUT_HANDLE), diagnostic.data(),
                static_cast<DWORD>(diagnostic.size()), &written, nullptr);
            return 155;
        }
        const SOCKET connection = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connection == INVALID_SOCKET)
        {
            const int failure = ::WSAGetLastError();
            (void)::WSACleanup();
            return failure == WSAEACCES ? 0 : 156;
        }
        u_long nonblocking = 1u;
        const bool modeReady = ::ioctlsocket(connection, FIONBIO, &nonblocking) == 0;
        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = ::htons(1234u);
        endpoint.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        const int connected = modeReady
            ? ::connect(connection, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) : 0;
        int failure = connected == SOCKET_ERROR ? ::WSAGetLastError() : 0;
        if (failure == WSAEWOULDBLOCK)
        {
            fd_set writable{}, failed{};
            FD_ZERO(&writable); FD_SET(connection, &writable);
            FD_ZERO(&failed); FD_SET(connection, &failed);
            timeval deadline{3, 0};
            if (::select(0, nullptr, &writable, &failed, &deadline) > 0)
            {
                int size = sizeof(failure);
                if (::getsockopt(connection, SOL_SOCKET, SO_ERROR,
                        reinterpret_cast<char*>(&failure), &size) != 0)
                    failure = ::WSAGetLastError();
            }
        }
        (void)::closesocket(connection);
        (void)::WSACleanup();
        return failure == WSAEACCES ? 0 : 157;
    }

    [[nodiscard]] std::optional<std::wstring> file_dacl(const std::filesystem::path& path)
    {
        PSECURITY_DESCRIPTOR descriptor{};
        PACL acl{};
        if (::GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &descriptor) != ERROR_SUCCESS)
            return std::nullopt;
        LPWSTR encoded{};
        const bool converted = ::ConvertSecurityDescriptorToStringSecurityDescriptorW(
            descriptor, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &encoded, nullptr) != FALSE;
        std::optional<std::wstring> result{};
        if (converted) result = encoded;
        if (encoded) (void)::LocalFree(encoded);
        if (descriptor) (void)::LocalFree(descriptor);
        return result;
    }

    [[nodiscard]] int run_isolation_contract(const std::filesystem::path& self)
    {
        ExecutableIdentityFixture fixture{};
        fixture.preserve_evidence = true;
        if (!fixture.prepare("epoch-workspace-isolation-")) return 140;
        const auto fail = [&](int code, const std::string& message)
        {
            (void)fixture.write("isolation.evidence.txt", message);
            return code;
        };
        const auto root = fixture.root / "generation";
        const auto code = root / "code";
        const auto scratch = root / "scratch";
        std::error_code error{};
        std::filesystem::create_directories(code, error);
        if (error) return fail(141, "Cannot create code fixture.");
        std::filesystem::create_directory(scratch, error);
        if (error) return fail(141, "Cannot create scratch fixture.");
        const auto executable = code / "probe.exe";
        std::filesystem::copy_file(self, executable, std::filesystem::copy_options::none, error);
        if (error || !fixture.write("generation/code/input.txt", "immutable-input")
            || !fixture.write("outside.txt", "host-private-fixture"))
            return fail(141, "Cannot create synthetic files.");
        const auto beforeCode = file_dacl(code);
        const auto beforeInput = file_dacl(code / "input.txt");
        const auto beforeScratch = file_dacl(scratch);
        if (!beforeCode || !beforeInput || !beforeScratch) return fail(142, "Cannot inspect fixture ACLs.");
        child_process::LaunchRequest request{};
        request.executable = executable;
        request.working_directory = scratch;
        request.merged_output_path = fixture.root / "child.log";
        request.arguments = {"--epoch-isolation-child", code.string(), scratch.string(),
            (fixture.root / "outside.txt").string()};
        request.window_mode = child_process::WindowMode::hidden;
        request.correlation_key = "contract:workspace-isolation";
        request.exclusive_group = request.correlation_key;
        request.expected_executable = child_process::inspect_executable(executable);
        request.environment = child_process::prepare_workspace_environment(scratch);
        request.disconnect_standard_input = true;
        request.isolation = child_process::WorkspaceIsolation{root, {code}, {scratch}};
        if (!request.environment || !request.expected_executable) return fail(142, "Cannot prepare request.");
        const auto refusedBeforeExecution = [&](std::string_view diagnostic)
        {
            const auto refused = child_process::launch_or_focus(request);
            OwnedContractProcess owner{refused.handle};
            const auto state = child_process::snapshot(refused.handle);
            const bool released = child_process::release(refused.handle);
            if (released) owner.handle = {};
            return refused.code == child_process::LaunchCode::spawn_failed
                && refused.message.find(diagnostic) != std::string::npos
                && state && !state->active() && state->platform_process_id == 0u
                && released && !child_process::active_in_group(request.exclusive_group)
                && file_dacl(code) == beforeCode && file_dacl(code / "input.txt") == beforeInput
                && file_dacl(scratch) == beforeScratch;
        };
        // Admission must discover a null descendant DACL before the first
        // inheritable root grant can rewrite it. This canary is fixture-only.
        const auto nullDaclPath = code / "null-dacl-canary.txt";
        auto nullDaclName = nullDaclPath.wstring();
        if (!fixture.write("generation/code/null-dacl-canary.txt", "synthetic-null-dacl")
            || ::SetNamedSecurityInfoW(nullDaclName.data(), SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            return fail(148, "Cannot prepare null-DACL admission canary.");
        const auto nullDacl = file_dacl(nullDaclPath);
        if (!nullDacl || !refusedBeforeExecution("absent or invalid DACL")
            || file_dacl(nullDaclPath) != nullDacl)
            return fail(148, "Null-DACL refusal changed permissions or reached execution.");
        if (!std::filesystem::remove(nullDaclPath, error) || error)
            return fail(148, "Cannot retire null-DACL canary.");

        const auto outsideAlias = fixture.root / "outside-alias.txt";
        if (::CreateHardLinkW(outsideAlias.c_str(), (code / "input.txt").c_str(), nullptr) == FALSE
            || !refusedBeforeExecution("hard-linked"))
            return fail(149, "Hardlinked admission was not refused before execution.");
        if (!std::filesystem::remove(outsideAlias, error) || error)
            return fail(149, "Cannot retire hardlink canary.");

        const auto detour = fixture.root / "detour";
        const auto junction = scratch / "redirected";
        if (!std::filesystem::create_directory(detour, error) || error
            || !create_fixture_directory_link(junction, detour)
            || !refusedBeforeExecution("reparse"))
            return fail(150, "Reparse admission was not refused before execution.");
        if (!std::filesystem::remove(junction, error) || error)
            return fail(150, "Cannot retire junction canary.");
        const auto launch = child_process::launch_or_focus(request);
        OwnedContractProcess owner{launch.handle};
        if (!launch)
        {
            if (launch.handle.valid())
            {
                (void)child_process::wait(launch.handle, {}, 10'000'000'000ull);
                (void)child_process::release(launch.handle);
            }
            return fail(143, "Restricted console launch failed: " + launch.message);
        }
        const auto result = child_process::wait(launch.handle, {}, 15'000'000'000ull);
        const bool released = child_process::release(launch.handle);
        if (released) owner.handle = {};
        const auto read = [](const std::filesystem::path& path)
        {
            std::ifstream input{path, std::ios::binary};
            return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        };
        // Record host-side preservation and retirement even when a later
        // child subcheck fails. A Winsock initialization failure must not hide
        // these independent results or be mistaken for connection denial.
        const bool dataPreserved = read(scratch / "result.txt") == "owned-scratch-only"
            && read(scratch / "result-alias.txt") == "owned-scratch-only"
            && read(code / "input.txt") == "immutable-input"
            && read(fixture.root / "outside.txt") == "host-private-fixture";
        const bool daclRestored = file_dacl(code) == beforeCode
            && file_dacl(code / "input.txt") == beforeInput
            && file_dacl(scratch) == beforeScratch;
        const bool groupRetired = !child_process::active_in_group(request.exclusive_group);
        const bool tokenVerified = result.process && result.process->restricted_token_verified;
        const bool isolationRetired = result.process && result.process->isolation_retired;
        const bool childPassed = result && result.process && result.process->exit_code_valid
            && result.process->exit_code == 0;
        const std::string evidence = "process=" + result.message
            + "\nnull_dacl_admission_refused=1\nhardlink_admission_refused=1\nreparse_admission_refused=1"
            + "\nchild_exit=" + (result.process && result.process->exit_code_valid
                ? std::to_string(result.process->exit_code) : "missing")
            + "\nrestricted_token_verified=" + std::to_string(tokenVerified)
            + "\nisolation_retired=" + std::to_string(isolationRetired)
            + "\nhandle_released=" + std::to_string(released)
            + "\ndata_preserved=" + std::to_string(dataPreserved)
            + "\nprior_dacl_restored=" + std::to_string(daclRestored)
            + "\nprocess_group_retired=" + std::to_string(groupRetired) + "\n";
        if (!dataPreserved) return fail(145, evidence);
        if (!daclRestored) return fail(146, evidence);
        if (!groupRetired) return fail(147, evidence);
        if (!childPassed || !tokenVerified || !isolationRetired || !released)
            return fail(144, evidence);
        fixture.preserve_evidence = false;
        return 0;
    }

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
        std::optional<std::wstring> inherited{};
        if (!lookup_environment(private_environment_name, inherited)
            || inherited != private_environment_value)
            return 90;
        const HANDLE forbidden = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(std::stoull(sentinelText)));
        // Handle values are process-relative and may be reused by child
        // startup. The parent checks its actual event object after exit;
        // signalling a different child-local event is not an inherited leak.
        (void)::SetEvent(forbidden);

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

    [[nodiscard]] int run_execution_input_child(
        std::string_view mode, const char* sentinelText)
    {
        std::optional<std::wstring> value{};
        if (!lookup_environment(private_environment_name, value) || value)
            return 101;
        if (mode == "replaced")
        {
            if (!lookup_environment(explicit_environment_name, value)
                || value != L"Gr\u00fc\u00dfe \u96ea")
                return 102;
            if (!lookup_environment(empty_environment_name, value)
                || !value || !value->empty())
                return 103;
        }
        else if (mode == "empty")
        {
            const auto block = ::GetEnvironmentStringsW();
            if (!block) return 104;
            const bool empty = *block == L'\0';
            (void)::FreeEnvironmentStringsW(block);
            if (!empty) return 105;
        }
        else return 106;

        const HANDLE forbidden = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(std::stoull(sentinelText)));
        // Only the parent's sentinel state proves object inheritance. A
        // successful call on a recycled child-local handle is not that proof.
        (void)::SetEvent(forbidden);
        const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
        char byte{};
        DWORD received{};
        if (::GetFileType(input) != FILE_TYPE_CHAR
            || ::ReadFile(input, &byte, 1u, &received, nullptr) == FALSE
            || received != 0u)
            return 108;
        constexpr std::string_view marker = "epoch-private-input-contract-passed\n";
        DWORD written{};
        if (::WriteFile(::GetStdHandle(STD_OUTPUT_HANDLE), marker.data(),
                static_cast<DWORD>(marker.size()), &written, nullptr) == FALSE
            || written != marker.size())
            return 109;
        return 27;
    }

    [[nodiscard]] bool execution_input_contract(
        const std::filesystem::path& self, ExecutableIdentityFixture& fixture)
    {
        const auto fail = [&](std::string detail)
        {
            fixture.preserve_evidence = true;
            detail = "execution-input: " + detail + "\n";
            (void)fixture.write("execution-input.evidence.txt", detail);
            DWORD written{};
            (void)::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), detail.data(),
                static_cast<DWORD>(detail.size()), &written, nullptr);
            return false;
        };
        child_process::LaunchRequest held{};
        held.executable = self;
        held.working_directory = fixture.root;
        held.merged_output_path = fixture.root / "input-binding.log";
        held.arguments = {"--epoch-child-contract", "30000", "0"};
        held.correlation_key = "contract:explicit-input-binding";
        held.window_mode = child_process::WindowMode::hidden;
        held.disconnect_standard_input = true;
        held.environment = std::vector<child_process::EnvironmentVariable>{
            {"FIRST", "one"}, {"SECOND", "two"}};
        const auto live = child_process::launch_or_focus(held);
        if (live.code != child_process::LaunchCode::started) return fail("binding launch: " + live.message);
        const auto duplicate = child_process::launch_or_focus(held);
        bool bindingsValid = duplicate.code == child_process::LaunchCode::focused_existing
            && duplicate.handle == live.handle;
        for (unsigned change = 0u; change < 11u; ++change)
        {
            auto different = held;
            if (change == 0u) different.environment->front().value = "changed";
            else if (change == 1u) different.environment->front().name = "OTHER";
            else if (change == 2u)
                std::swap(different.environment->front(), different.environment->back());
            else if (change == 3u) different.environment.reset();
            else if (change == 4u) different.disconnect_standard_input = false;
            else if (change == 5u) different.arguments.back() = "1";
            else if (change == 6u) different.working_directory = self.parent_path();
            else if (change == 7u) different.merged_output_path = fixture.root / "different.log";
            else if (change == 8u) different.append_output = true;
            else if (change == 9u) different.window_mode = child_process::WindowMode::normal;
            else different.exclusive_group = "contract:other-group";
            const auto refused = child_process::launch_or_focus(different);
            bindingsValid = bindingsValid
                && refused.code == child_process::LaunchCode::invalid_request
                && refused.message.find("correlation") != std::string::npos;
        }
        std::stop_source cancellation{};
        (void)cancellation.request_stop();
        const auto stopped = child_process::wait(
            live.handle, cancellation.get_token(), 5'000'000'000ull);
        if (!child_process::release(live.handle) || !bindingsValid
            || stopped.code != child_process::WaitCode::cancelled)
            return fail("binding/cancel: bindings=" + std::to_string(bindingsValid)
                + " wait=" + std::string{child_process::wait_code_name(stopped.code)} + " " + stopped.message);

        for (const std::string mode : {"replaced", "empty"})
        {
            CaptureInheritanceFixture capture{};
            if (!capture.prepare()) return fail(mode + ": capture preparation");
            child_process::LaunchRequest request{};
            request.executable = self;
            request.working_directory = fixture.root;
            request.merged_output_path = fixture.root / (mode + ".log");
            request.arguments = {"--epoch-execution-input-contract", mode,
                std::to_string(reinterpret_cast<std::uintptr_t>(capture.sentinel))};
            request.correlation_key = "contract:execution-input-" + mode;
            request.window_mode = child_process::WindowMode::hidden;
            request.disconnect_standard_input = true;
            request.environment.emplace();
            if (mode == "replaced")
            {
                request.environment->push_back({"EPOCH_CHILD_EXPLICIT_UTF8",
                    "Gr\xc3\xbc\xc3\x9f" "e \xe9\x9b\xaa"});
                request.environment->push_back({"EPOCH_CHILD_EXPLICIT_EMPTY", {}});
            }
            const auto launched = child_process::launch_or_focus(request);
            capture.restore_input();
            if (launched.code != child_process::LaunchCode::started) return fail(mode + ": " + launched.message);
            const auto result = child_process::wait(launched.handle, {}, 5'000'000'000ull);
            const bool success = result && result.process
                && result.process->exit_code_valid && result.process->exit_code == 27
                && result.process->environment_replaced
                && result.process->standard_input_disconnected;
            if (!success)
                (void)child_process::stop(launched.handle, child_process::StopMode::force);
            if (!child_process::release(launched.handle) || !success)
                return fail(mode + ": completion=" + result.message + " exit="
                    + (result.process && result.process->exit_code_valid
                        ? std::to_string(result.process->exit_code) : "unknown")
                    + " host_sentinel_wait=" + std::to_string(::WaitForSingleObject(capture.sentinel, 0u)));
            DWORD available{};
            if (::PeekNamedPipe(capture.input_read, nullptr, 0u, nullptr,
                    &available, nullptr) == FALSE
                || available != capture_input.size()
                || ::WaitForSingleObject(capture.sentinel, 0u) != WAIT_TIMEOUT)
                return fail(mode + ": host sentinel/input changed; available=" + std::to_string(available));
            std::ifstream output{request.merged_output_path, std::ios::binary};
            const std::string bytes{std::istreambuf_iterator<char>{output},
                std::istreambuf_iterator<char>{}};
            if (bytes != "epoch-private-input-contract-passed\n")
                return fail(mode + ": captured marker differs; bytes=" + std::to_string(bytes.size()));
        }
        return true;
    }

    // Explicitly invoked compatibility proof, never part of the default CTest
    // path. It compiles only these fresh fixture literals, not Engine source.
    [[nodiscard]] int run_msbuild_environment_contract(
        const std::filesystem::path& msbuild) noexcept
    {
        ExecutableIdentityFixture fixture{};
        fixture.preserve_evidence = true;
        try
        {
            if (!msbuild.is_absolute()
                || ::CompareStringOrdinal(msbuild.filename().c_str(), -1,
                    L"MSBuild.exe", -1, TRUE) != CSTR_EQUAL)
                return 80;
            if (!fixture.prepare("epoch-msbuild-environment-")) return 81;
            const auto utf8 = [](const std::filesystem::path& path)
            {
                const auto bytes = path.u8string();
                return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            };
            const auto record = [&](std::string_view text)
            {
                std::ofstream output{fixture.root / "contract.evidence.txt",
                    std::ios::binary | std::ios::app};
                output.write(text.data(), static_cast<std::streamsize>(text.size()));
                output.put('\n');
                output.close();
                return static_cast<bool>(output);
            };
            const auto fail = [&](int code, std::string_view detail)
            {
                (void)record("result=FAIL\ncode=" + std::to_string(code)
                    + "\ndetail=" + std::string{detail});
                return code;
            };
            if (!record("schema=epoch-msbuild-environment-contract/v1\nscope=tiny-v143-x64-sdk10-console-only\nfixture="
                    + utf8(fixture.root) + "\nmsbuild=" + utf8(msbuild))) return 82;
            const auto compilerIdentity = child_process::inspect_executable(msbuild);
            if (!compilerIdentity) return fail(83, "Exact MSBuild image identity could not be inspected.");

            constexpr std::string_view project = R"epoch(<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{71668213-B69B-49A8-B431-55AA9B549308}</ProjectGuid>
    <Keyword>Win32Proj</Keyword><RootNamespace>EpochEnvironmentContract</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration">
    <ConfigurationType>Application</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset><WholeProgramOptimization>false</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>$(MSBuildThisFileDirectory)bin\</OutDir><IntDir>$(MSBuildThisFileDirectory)obj\</IntDir>
    <TargetName>epoch_env_probe</TargetName><VcpkgEnabled>false</VcpkgEnabled>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile><WarningLevel>Level4</WarningLevel><Optimization>Disabled</Optimization>
      <PrecompiledHeader>NotUsing</PrecompiledHeader><RuntimeLibrary>MultiThreaded</RuntimeLibrary>
      <LanguageStandard>stdcpp20</LanguageStandard><MultiProcessorCompilation>false</MultiProcessorCompilation>
    </ClCompile>
    <Link><SubSystem>Console</SubSystem><GenerateDebugInformation>false</GenerateDebugInformation></Link>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="epoch.env_probe.cpp" /></ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <Target Name="ReportEpochEnvironmentToolchain" BeforeTargets="ClCompile">
    <Message Text="Epoch environment contract: toolset=$(PlatformToolset), platform=$(Platform), SDK=$(WindowsTargetPlatformVersion)" Importance="High" />
  </Target>
</Project>
)epoch";
            constexpr std::string_view source = R"epoch(#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cwchar>

static bool matches_environment(const wchar_t* name, const wchar_t* expected)
{
    wchar_t value[32768]{};
    const DWORD length = ::GetEnvironmentVariableW(name, value, 32768);
    return length > 0 && length < 32768 && std::wcscmp(value, expected) == 0;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3 || !matches_environment(L"USERPROFILE", argv[1])
        || !matches_environment(L"TEMP", argv[2])) return 101;
    wchar_t private_value[64]{};
    ::SetLastError(ERROR_SUCCESS);
    if (::GetEnvironmentVariableW(L"EPOCH_CHILD_PRIVATE_SENTINEL", private_value, 64) != 0
        || ::GetLastError() != ERROR_ENVVAR_NOT_FOUND) return 102;
    const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
    char byte{};
    DWORD received{};
    if (::GetFileType(input) != FILE_TYPE_CHAR
        || !::ReadFile(input, &byte, 1, &received, nullptr) || received != 0) return 103;
    return 42;
}
)epoch";
            if (!fixture.write("epoch.env_probe.vcxproj", project)
                || !fixture.write("epoch.env_probe.cpp", source))
                return fail(84, "Fresh compiler fixture source could not be written.");
            PrivateEnvironmentFixture privateEnvironment{};
            if (!privateEnvironment.prepare()) return fail(85, "Private synthetic environment sentinel was unavailable.");
            const auto environment = child_process::prepare_workspace_environment(fixture.root);
            if (!environment) return fail(86, "Workspace-local execution environment could not be prepared.");

            struct OwnedContractChild final
            {
                child_process::ProcessHandle handle{};
                ~OwnedContractChild() noexcept
                {
                    if (!handle.valid()) return;
                    std::stop_source cancellation{};
                    (void)cancellation.request_stop();
                    (void)child_process::wait(handle, cancellation.get_token(), 5'000'000'000ull);
                    (void)child_process::release(handle);
                }
            };
            const auto run = [&](const child_process::LaunchRequest& request,
                std::uint64_t timeout, std::int32_t expectedExit, std::string_view stage)
            {
                const auto launched = child_process::launch_or_focus(request);
                OwnedContractChild owned{launched.handle};
                if (!record(std::string{stage} + ".launch="
                        + std::string{child_process::launch_code_name(launched.code)})) return false;
                if (launched.code != child_process::LaunchCode::started) return false;
                const auto waited = child_process::wait(launched.handle, {}, timeout);
                const auto& process = waited.process;
                const bool actualExit = waited && process && !process->active()
                    && process->exit_code_valid && process->exit_code == expectedExit
                    && process->verified_executable == request.expected_executable
                    && process->environment_replaced && process->standard_input_disconnected;
                if (!record(std::string{stage} + ".wait="
                        + std::string{child_process::wait_code_name(waited.code)}
                        + "\n" + std::string{stage} + ".exit="
                        + (process && process->exit_code_valid ? std::to_string(process->exit_code) : "unavailable")
                        + "\n" + std::string{stage} + ".verified=" + (actualExit ? "true" : "false"))) return false;
                if (!actualExit) return false;
                if (!child_process::release(launched.handle)) return false;
                owned.handle = {};
                return true;
            };
            child_process::LaunchRequest compile{};
            compile.executable = msbuild;
            compile.working_directory = fixture.root;
            compile.merged_output_path = fixture.root / "msbuild.log";
            compile.arguments = {"epoch.env_probe.vcxproj", "/t:Build", "/noautorsp", "/nr:false", "/m:1",
                "/nologo", "/v:minimal", "/p:Configuration=Release", "/p:Platform=x64",
                "/p:PlatformToolset=v143", "/p:VcpkgEnabled=false",
                "/p:VcpkgEnableManifest=false", "/p:ImportDirectoryBuildProps=false", "/p:ImportDirectoryBuildTargets=false"};
            compile.correlation_key = "contract:msbuild-environment:compile";
            compile.exclusive_group = "contract:msbuild-environment";
            compile.window_mode = child_process::WindowMode::hidden;
            compile.expected_executable = compilerIdentity;
            compile.environment = environment;
            compile.disconnect_standard_input = true;
            if (!record("compiler.bytes=" + std::to_string(compilerIdentity->size_bytes)
                    + "\ncompiler.sha256=" + compilerIdentity->sha256 + "\ncompiler.timeout_seconds=180"))
                return fail(87, "Compiler identity evidence could not be written.");
            if (!run(compile, 180'000'000'000ull, 0, "compiler"))
                return fail(88, "Real MSBuild compile did not complete successfully; inspect msbuild.log.");
            const auto executable = fixture.root / "bin" / "epoch_env_probe.exe";
            const auto builtIdentity = child_process::inspect_executable(executable);
            if (!builtIdentity) return fail(89, "Fresh compiler output is missing or cannot be identity-bound.");
            if (!record("probe.bytes=" + std::to_string(builtIdentity->size_bytes)
                    + "\nprobe.sha256=" + builtIdentity->sha256))
                return fail(90, "Compiled helper identity evidence could not be written.");
            child_process::LaunchRequest probe{};
            probe.executable = executable;
            probe.working_directory = fixture.root;
            probe.merged_output_path = fixture.root / "probe.log";
            probe.arguments = {utf8(fixture.root / "cache" / "process" / "profile"),
                utf8(fixture.root / "cache" / "process" / "temp")};
            probe.correlation_key = "contract:msbuild-environment:probe";
            probe.exclusive_group = compile.exclusive_group;
            probe.window_mode = child_process::WindowMode::hidden;
            probe.expected_executable = builtIdentity;
            probe.environment = environment;
            probe.disconnect_standard_input = true;
            if (!run(probe, 10'000'000'000ull, 42, "probe"))
                return fail(91, "Compiled console helper failed its isolated-input/environment check.");
            if (child_process::metrics().active_processes != 0u)
                return fail(92, "Test-owned child retirement is incomplete.");
            return record("result=PASS\nretired=true\nlimitations=not-full-engine-build-or-OS-confinement-proof") ? 0 : 93;
        }
        catch (...)
        {
            try
            {
                if (!fixture.root.empty())
                    (void)fixture.write("contract.exception.txt", "Unexpected compatibility-contract failure; all existing fixture evidence is retained.\n");
            }
            catch (...) {}
            return 94;
        }
    }
}
#endif

static int run_contract_entry(int argc, char** argv)
{
    namespace child_process = epochengine::platform::child_process;

#if defined(__linux__)
    if (argc == 7
        && std::string_view{argv[1]} == "--epoch-descriptor-inheritance-contract")
        return run_descriptor_inheritance_child(argv);
#endif

    if (argc >= 2 && std::string_view{argv[1]} == "--epoch-msbuild-environment-contract")
    {
#if defined(_WIN32)
        return argc == 3 ? run_msbuild_environment_contract(std::filesystem::path{argv[2]}) : 80;
#else
        return 80;
#endif
    }

#if defined(_WIN32)
    if (argc == 2 && std::string_view{argv[1]} == "--epoch-input-contract-only")
    {
        ExecutableIdentityFixture fixture{};
        PrivateEnvironmentFixture environment{};
        if (!fixture.prepare("epoch-execution-input-") || !environment.prepare()) return 26;
        return execution_input_contract(std::filesystem::absolute(argv[0]), fixture) ? 0 : 27;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--epoch-isolation-contract-only")
        return run_isolation_contract(std::filesystem::absolute(argv[0]));
    if (argc == 5 && std::string_view{argv[1]} == "--epoch-isolation-child")
        return run_isolation_child(argv);
    if (argc == 2 && std::string_view{argv[1]} == "--epoch-retirement-contract-only")
        return run_retirement_contracts(std::filesystem::absolute(argv[0]));
    if ((argc == 3 || (argc == 4 && std::string_view{argv[3]} == "churn"))
        && std::string_view{argv[1]} == "--epoch-retirement-parent")
        return run_retirement_parent(std::filesystem::path{argv[2]}, argc == 4);
    if (argc == 4
        && std::string_view{argv[1]} == "--epoch-execution-input-contract")
    {
        return run_execution_input_child(argv[2], argv[3]);
    }
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

    if (!launch_result_ownership_contract()) return 30;
    const auto unreserved = child_process::launch_or_focus({});
    if (unreserved.code != child_process::LaunchCode::invalid_request
        || unreserved.handle.valid() || unreserved.owns_new_process()) return 31;
    child_process::LaunchRequest invalid{};
    invalid.executable = "missing-child-process-contract-executable";
    invalid.correlation_key = "invalid";
    const auto reservedInvalid = child_process::launch_or_focus(invalid);
    const auto invalidSnapshot = child_process::snapshot(reservedInvalid.handle);
    const bool invalidReleased = child_process::release(reservedInvalid.handle);
    if (reservedInvalid.code != child_process::LaunchCode::invalid_request
        || !reservedInvalid.owns_new_process() || !invalidSnapshot
        || invalidSnapshot->active() || invalidSnapshot->state != child_process::ProcessState::failed
        || !invalidReleased || child_process::snapshot(reservedInvalid.handle))
    {
        return 2;
    }

    const std::filesystem::path self = std::filesystem::absolute(argv[0]);
    if (!environment_validation_contract(self))
        return 24;
    if (!isolation_request_contract(self))
        return 25;
    ExecutableIdentityFixture identityFixture{};
    if (!inspect_identity_contract(identityFixture))
        return 18;
    if (!failed_launch_ownership_contract(identityFixture)) return 32;
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
        || !launched.owns_new_process())
    {
        return 3;
    }
    const auto running = child_process::snapshot(launched.handle);
    if (!running || !running->active()
        || running->correlation_key != first.correlation_key
        || running->platform_process_id == 0u
        || running->platform_window_id != 0u
        || running->verified_executable != first.expected_executable
        || running->environment_replaced || running->standard_input_disconnected)
    {
        return 4;
    }

    const child_process::LaunchResult duplicate =
        child_process::launch_or_focus(first);
    if (duplicate.code != child_process::LaunchCode::focused_existing
        || duplicate.handle != launched.handle || duplicate.owns_new_process())
    {
        return 5;
    }

    for (const bool disconnect : {false, true})
    {
        auto changedInputs = first;
        if (disconnect)
        {
            changedInputs.disconnect_standard_input = true;
            changedInputs.merged_output_path = identityFixture.root / "not-launched.log";
        }
        else changedInputs.environment.emplace();
        const auto refused = child_process::launch_or_focus(changedInputs);
        if (refused.code != child_process::LaunchCode::invalid_request
            || refused.owns_new_process() || refused.handle.valid()
            || refused.message.find("correlation") == std::string::npos)
            return 25;
    }

    child_process::LaunchRequest wrongCorrelation = first;
    wrongCorrelation.executable = identityFixture.root / self.filename();
    std::error_code identityError{};
    if (!std::filesystem::copy_file(self, wrongCorrelation.executable,
            std::filesystem::copy_options::none, identityError) || identityError)
        return 20;
    const auto wrongPath = child_process::launch_or_focus(wrongCorrelation);
    if (wrongPath.code != child_process::LaunchCode::invalid_request
        || wrongPath.owns_new_process() || wrongPath.handle.valid()
        || wrongPath.message.find("correlation key") == std::string::npos)
        return 21;
#if defined(_WIN32)
    wrongCorrelation = first;
    wrongCorrelation.expected_executable.reset();
    const auto weakenedIdentity = child_process::launch_or_focus(wrongCorrelation);
    if (weakenedIdentity.code != child_process::LaunchCode::invalid_request
        || weakenedIdentity.owns_new_process() || weakenedIdentity.handle.valid()
        || weakenedIdentity.message.find("artifact identity") == std::string::npos)
        return 22;
#endif

    child_process::LaunchRequest competing = first;
    competing.correlation_key = "contract:competing";
    const child_process::LaunchResult busy =
        child_process::launch_or_focus(competing);
    if (busy.code != child_process::LaunchCode::active_group_busy
        || busy.handle != launched.handle || busy.owns_new_process())
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
    PrivateEnvironmentFixture environment{};
    if (!environment.prepare()) return 26;
    CaptureInheritanceFixture capture{};
    if (!capture.prepare())
        return 15;
    natural.arguments = {"--epoch-capture-inheritance-contract",
        std::to_string(reinterpret_cast<std::uintptr_t>(capture.sentinel))};
#endif
    if (!workspace_environment_contract(identityFixture)) return 28;
    const child_process::LaunchResult second =
        child_process::launch_or_focus(natural);
#if defined(_WIN32)
    capture.restore_input();
#endif
    if (!second || !second.owns_new_process()
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
        || completed.process->exit_code != 17
        || completed.process->environment_replaced
        || completed.process->standard_input_disconnected)
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
    if (!child_process::release(second.handle)) return 14;
#if defined(__linux__)
    if (!descriptor_inheritance_contract(self, identityFixture)) return 29;
#endif
#if defined(_WIN32)
    if (!execution_input_contract(self, identityFixture)) return 27;
    if (const int retirement = run_retirement_contracts(self); retirement != 0)
        return retirement;
#endif
    return 0;
}

int main(int argc, char** argv)
{
    const int result = run_contract_entry(argc, argv);
    // Preserve the exact failing checkpoint in CTest output. Child fixtures
    // deliberately return nonzero sentinels and must keep their capture bytes.
    if (result != 0 && (argc == 1 || (argc == 2
            && (std::string_view{argv[1]} == "--epoch-input-contract-only"
                || std::string_view{argv[1]} == "--epoch-retirement-contract-only"))))
    {
        namespace child_process = epochengine::platform::child_process;
        std::string diagnostic = "platform.child_process contract exit=" + std::to_string(result) + "\n";
        for (const auto& process : child_process::snapshots())
            diagnostic += process.correlation_key + " | "
                + std::string{child_process::process_state_name(process.state)}
                + " | exit=" + (process.exit_code_valid ? std::to_string(process.exit_code) : "unknown")
                + " | " + process.message + "\n";
#if defined(_WIN32)
        DWORD written{};
        (void)::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), diagnostic.data(),
            static_cast<DWORD>(diagnostic.size()), &written, nullptr);
#elif defined(__linux__)
        (void)::write(STDERR_FILENO, diagnostic.data(), diagnostic.size());
#endif
    }
    return result;
}
#endif
