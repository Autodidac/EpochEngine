// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <Windows.h>
#else
#  include <dlfcn.h>
#endif

module extension.plugin;

import core.logger;
import core.sha256;
import package.registry;

namespace epochengine::extensions
{
    namespace
    {
#if defined(_WIN32)
        using NativeLibrary = HMODULE;
#else
        using NativeLibrary = void*;
#endif

        constexpr std::uint32_t kInvalidIndex =
            (std::numeric_limits<std::uint32_t>::max)();

        [[nodiscard]] bool bounded_text(
            const char* value,
            std::size_t maximum) noexcept
        {
            if (!value || maximum == 0u)
                return false;
            for (std::size_t index = 0u; index < maximum; ++index)
            {
                if (value[index] == '\0')
                    return index != 0u;
            }
            return false;
        }

        [[nodiscard]] std::string lowercase(std::string_view value)
        {
            std::string result{value};
            std::transform(
                result.begin(),
                result.end(),
                result.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return result;
        }

        [[nodiscard]] bool target_matches(
            std::string_view requested,
            std::string_view current)
        {
            return !requested.empty()
                && lowercase(requested) == lowercase(current);
        }

        [[nodiscard]] std::optional<core::sha256::Digest>
        expected_digest(std::string_view value) noexcept
        {
            constexpr std::string_view prefix{"sha256:"};
            if (value.starts_with(prefix))
                value.remove_prefix(prefix.size());
            return core::sha256::from_hex(value);
        }

        [[nodiscard]] std::optional<core::sha256::Digest>
        hash_file(
            const std::filesystem::path& path,
            std::uint64_t maximumBytes) noexcept
        {
            try
            {
                std::ifstream input{path, std::ios::binary};
                if (!input)
                    return std::nullopt;

                core::sha256::Hasher hasher{};
                std::array<std::uint8_t, 64u * 1024u> buffer{};
                std::uint64_t consumed = 0u;
                while (input)
                {
                    input.read(
                        reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(buffer.size()));
                    const std::streamsize count = input.gcount();
                    if (count < 0)
                        return std::nullopt;
                    if (count == 0)
                        break;

                    consumed += static_cast<std::uint64_t>(count);
                    if (consumed > maximumBytes)
                        return std::nullopt;
                    hasher.update(std::span<const std::uint8_t>{
                        buffer.data(),
                        static_cast<std::size_t>(count)});
                }
                if (input.bad())
                    return std::nullopt;
                return hasher.finish();
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        void close_library(NativeLibrary library) noexcept
        {
            if (!library)
                return;
#if defined(_WIN32)
            (void)::FreeLibrary(library);
#else
            (void)::dlclose(library);
#endif
        }

        [[nodiscard]] NativeLibrary open_library(
            const std::filesystem::path& path) noexcept
        {
#if defined(_WIN32)
            return ::LoadLibraryExW(
                path.c_str(),
                nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
                    | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
            return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        }

        [[nodiscard]] void* find_symbol(
            NativeLibrary library,
            const char* name) noexcept
        {
            if (!library || !name)
                return nullptr;
#if defined(_WIN32)
            return reinterpret_cast<void*>(
                ::GetProcAddress(library, name));
#else
            return ::dlsym(library, name);
#endif
        }

        void extension_log(
            void*,
            unsigned int level,
            const char* message,
            unsigned long long messageSize)
        {
            if (!message || messageSize == 0u
                || messageSize > 16u * 1024u)
            {
                return;
            }

            const std::string text{
                message,
                static_cast<std::size_t>(messageSize)};
            switch (level)
            {
            case EPOCH_EXTENSION_LOG_ERROR:
                logger::error("Extensions", text);
                break;
            case EPOCH_EXTENSION_LOG_WARNING:
                logger::warn("Extensions", text);
                break;
            case EPOCH_EXTENSION_LOG_INFO:
            default:
                logger::info("Extensions", text);
                break;
            }
        }

        [[nodiscard]] epoch_extension_host_api make_host_api() noexcept
        {
            return epoch_extension_host_api{
                .abi_version = kExtensionAbiVersion,
                .struct_size = sizeof(epoch_extension_host_api),
                .user_data = nullptr,
                .log = &extension_log};
        }

        [[nodiscard]] bool descriptor_valid(
            const epoch_extension_descriptor& descriptor) noexcept
        {
            return descriptor.abi_version
                    == kExtensionAbiVersion
                && descriptor.struct_size
                    >= sizeof(epoch_extension_descriptor)
                && bounded_text(descriptor.plugin_id, 128u)
                && bounded_text(descriptor.display_name, 256u)
                && bounded_text(descriptor.plugin_version, 64u)
                && descriptor.on_load != nullptr
                && descriptor.on_unload != nullptr;
        }

        struct NativeLibraryGuard final
        {
            NativeLibrary library{};

            ~NativeLibraryGuard()
            {
                close_library(library);
            }

            NativeLibraryGuard() = default;
            explicit NativeLibraryGuard(NativeLibrary value)
                : library(value)
            {
            }
            NativeLibraryGuard(const NativeLibraryGuard&) = delete;
            NativeLibraryGuard& operator=(
                const NativeLibraryGuard&) = delete;
        };

        struct PluginStartGuard final
        {
            const epoch_extension_descriptor* descriptor{};
            epoch_extension_host_api host{};
            bool active{};

            ~PluginStartGuard()
            {
                if (!active || !descriptor || !descriptor->on_unload)
                    return;
                try
                {
                    descriptor->on_unload(&host);
                }
                catch (...)
                {
                    logger::error(
                        "Extensions",
                        "Extension rollback crossed the ABI with an exception.");
                }
            }

            PluginStartGuard(
                const epoch_extension_descriptor* value,
                epoch_extension_host_api hostApi) noexcept
                : descriptor(value),
                  host(hostApi)
            {
            }

            PluginStartGuard(const PluginStartGuard&) = delete;
            PluginStartGuard& operator=(const PluginStartGuard&) = delete;
        };
    }

    struct PluginLoader::Implementation final
    {
        struct Record final
        {
            PluginInfo info{};
            NativeLibrary library{};
            const epoch_extension_descriptor* descriptor{};
            std::uint32_t generation{};
        };

        mutable std::mutex mutex{};
        std::vector<std::unique_ptr<Record>> records{};
        std::vector<std::uint32_t> generations{};
    };

    std::string_view load_code_name(LoadCode code) noexcept
    {
        switch (code)
        {
        case LoadCode::ready: return "ready";
        case LoadCode::invalid_request: return "invalid_request";
        case LoadCode::project_not_admitted: return "project_not_admitted";
        case LoadCode::package_evidence_rejected: return "package_evidence_rejected";
        case LoadCode::platform_mismatch: return "platform_mismatch";
        case LoadCode::architecture_mismatch: return "architecture_mismatch";
        case LoadCode::invalid_expected_hash: return "invalid_expected_hash";
        case LoadCode::path_missing: return "path_missing";
        case LoadCode::not_regular_file: return "not_regular_file";
        case LoadCode::file_too_large: return "file_too_large";
        case LoadCode::file_read_failed: return "file_read_failed";
        case LoadCode::content_hash_mismatch: return "content_hash_mismatch";
        case LoadCode::native_load_failed: return "native_load_failed";
        case LoadCode::query_symbol_missing: return "query_symbol_missing";
        case LoadCode::descriptor_rejected: return "descriptor_rejected";
        case LoadCode::capabilities_rejected: return "capabilities_rejected";
        case LoadCode::network_approval_required: return "network_approval_required";
        case LoadCode::duplicate_plugin: return "duplicate_plugin";
        case LoadCode::plugin_start_failed: return "plugin_start_failed";
        case LoadCode::allocation_failure: return "allocation_failure";
        }
        return "invalid_request";
    }

    std::string_view current_platform_name() noexcept
    {
#if defined(_WIN32)
        return "windows";
#elif defined(__APPLE__)
        return "macos";
#elif defined(__linux__)
        return "linux";
#else
        return "unknown";
#endif
    }

    std::string_view current_architecture_name() noexcept
    {
#if defined(_M_X64) || defined(__x86_64__)
        return "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
        return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
        return "x86";
#else
        return "unknown";
#endif
    }

    PluginLoader::PluginLoader()
        : implementation_(std::make_unique<Implementation>())
    {
    }

    PluginLoader::~PluginLoader()
    {
        unload_all();
    }

    PluginLoader::PluginLoader(PluginLoader&& other) noexcept
        : implementation_(std::move(other.implementation_))
    {
    }

    PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept
    {
        if (this == &other)
            return *this;
        unload_all();
        implementation_ = std::move(other.implementation_);
        return *this;
    }

    LoadResult PluginLoader::load(
        const PluginAdmissionRequest& request) noexcept
    {
        try
        {
            if (!implementation_
                || request.requirement.packageId.empty()
                || request.binary_path.empty()
                || request.expected_content_hash.empty()
                || request.permitted_capabilities == 0u)
            {
                return {
                    LoadCode::invalid_request,
                    {},
                    "Native extension request is incomplete."};
            }
            if (!request.project_admitted)
            {
                return {
                    LoadCode::project_not_admitted,
                    {},
                    "The active project has not admitted this extension."};
            }

            const auto packageDecision =
                package_registry::evaluate_payload_activation(
                    request.requirement,
                    request.evidence);
            if (!packageDecision.payloadAllowed
                || !request.evidence.containsNativeCode
                || !request.evidence.nativeCodeApproved)
            {
                return {
                    LoadCode::package_evidence_rejected,
                    {},
                    "Package evidence or native-code approval was rejected."};
            }
            if (!target_matches(
                    request.target_platform,
                    current_platform_name()))
            {
                return {
                    LoadCode::platform_mismatch,
                    {},
                    "Extension platform does not match the host."};
            }
            if (!target_matches(
                    request.target_architecture,
                    current_architecture_name()))
            {
                return {
                    LoadCode::architecture_mismatch,
                    {},
                    "Extension architecture does not match the host."};
            }

            const auto expected =
                expected_digest(request.expected_content_hash);
            if (!expected
                || request.evidence.contentHash
                    != request.expected_content_hash)
            {
                return {
                    LoadCode::invalid_expected_hash,
                    {},
                    "Expected SHA-256 does not match approved package evidence."};
            }

            std::error_code error{};
            const bool exists =
                std::filesystem::exists(request.binary_path, error);
            if (error || !exists)
            {
                return {
                    LoadCode::path_missing,
                    {},
                    "Extension binary does not exist."};
            }
            if (!std::filesystem::is_regular_file(
                    request.binary_path,
                    error)
                || error)
            {
                return {
                    LoadCode::not_regular_file,
                    {},
                    "Extension binary is not a regular file."};
            }

            const std::uintmax_t bytes =
                std::filesystem::file_size(
                    request.binary_path,
                    error);
            if (error)
            {
                return {
                    LoadCode::file_read_failed,
                    {},
                    "Extension size could not be read."};
            }
            if (bytes == 0u || bytes > kMaximumPluginBytes)
            {
                return {
                    LoadCode::file_too_large,
                    {},
                    "Extension binary is empty or exceeds the size limit."};
            }

            const auto observed =
                hash_file(
                    request.binary_path,
                    kMaximumPluginBytes);
            if (!observed)
            {
                return {
                    LoadCode::file_read_failed,
                    {},
                    "Extension binary could not be hashed."};
            }
            if (*observed != *expected)
            {
                return {
                    LoadCode::content_hash_mismatch,
                    {},
                    "Extension SHA-256 changed after approval."};
            }
            NativeLibraryGuard library{
                open_library(request.binary_path)};
            if (!library.library)
            {
                return {
                    LoadCode::native_load_failed,
                    {},
                    "Native loader refused the verified extension."};
            }

            auto query = reinterpret_cast<epoch_extension_query_fn>(
                find_symbol(
                    library.library,
                    kExtensionQuerySymbol.data()));
            if (!query)
            {
                return {
                    LoadCode::query_symbol_missing,
                    {},
                    "Extension query symbol is missing."};
            }

            const epoch_extension_descriptor* descriptor{};
            try
            {
                descriptor = query(kExtensionAbiVersion);
            }
            catch (...)
            {
                return {
                    LoadCode::descriptor_rejected,
                    {},
                    "Extension query crossed the ABI with an exception."};
            }
            if (!descriptor || !descriptor_valid(*descriptor))
            {
                return {
                    LoadCode::descriptor_rejected,
                    {},
                    "Extension descriptor is invalid or ABI-incompatible."};
            }

            if ((descriptor->capabilities
                    & ~request.permitted_capabilities) != 0u)
            {
                return {
                    LoadCode::capabilities_rejected,
                    {},
                    "Extension requested capabilities not admitted by the project."};
            }
            if ((descriptor->capabilities
                    & EPOCH_EXTENSION_CAPABILITY_NETWORK) != 0u
                && !request.network_approved)
            {
                return {
                    LoadCode::network_approval_required,
                    {},
                    "Network-capable extensions require separate human approval."};
            }

            const auto duplicate_loaded = [&]()
            {
                for (const auto& loaded : implementation_->records)
                {
                    if (loaded
                        && loaded->info.plugin_id
                            == descriptor->plugin_id)
                    {
                        return true;
                    }
                }
                return false;
            };
            {
                std::unique_lock lock{implementation_->mutex};
                if (duplicate_loaded())
                {
                    return {
                        LoadCode::duplicate_plugin,
                        {},
                        "An extension with this stable ID is already loaded."};
                }
            }

            auto record = std::make_unique<Implementation::Record>();
            record->info = PluginInfo{
                .package_id =
                    std::string{request.requirement.packageId},
                .plugin_id =
                    std::string{descriptor->plugin_id},
                .display_name =
                    std::string{descriptor->display_name},
                .plugin_version =
                    std::string{descriptor->plugin_version},
                .source_path =
                    std::filesystem::weakly_canonical(
                        request.binary_path,
                        error),
                .content_hash =
                    "sha256:" + core::sha256::hex(*observed),
                .capabilities = descriptor->capabilities};
            if (error)
                record->info.source_path = request.binary_path;
            record->library = library.library;
            record->descriptor = descriptor;

            epoch_extension_host_api host = make_host_api();
            PluginStartGuard startGuard{descriptor, host};
            unsigned char started{};
            try
            {
                started = descriptor->on_load(&host);
            }
            catch (...)
            {
                return {
                    LoadCode::plugin_start_failed,
                    {},
                    "Extension startup crossed the ABI with an exception."};
            }
            if (started == 0u)
            {
                return {
                    LoadCode::plugin_start_failed,
                    {},
                    "Extension refused startup."};
            }
            startGuard.active = true;

            PluginHandle handle{};
            {
                std::unique_lock lock{implementation_->mutex};
                if (duplicate_loaded())
                {
                    return {
                        LoadCode::duplicate_plugin,
                        {},
                        "An extension with this stable ID is already loaded."};
                }

                std::uint32_t index = kInvalidIndex;
                for (std::uint32_t candidate = 0u;
                     candidate < implementation_->records.size();
                     ++candidate)
                {
                    if (!implementation_->records[candidate])
                    {
                        index = candidate;
                        break;
                    }
                }
                if (index == kInvalidIndex)
                {
                    index = static_cast<std::uint32_t>(
                        implementation_->records.size());
                    implementation_->records.push_back(nullptr);
                    implementation_->generations.push_back(0u);
                }

                std::uint32_t& generation =
                    implementation_->generations[index];
                generation = generation
                        == (std::numeric_limits<std::uint32_t>::max)()
                    ? 1u
                    : generation + 1u;
                if (generation == 0u)
                    generation = 1u;

                handle = PluginHandle{index, generation};
                record->info.handle = handle;
                record->generation = generation;
                implementation_->records[index] = std::move(record);
            }

            startGuard.active = false;
            library.library = nullptr;

            logger::info(
                "Extensions",
                "Loaded approved native extension '"
                    + std::string{descriptor->plugin_id}
                    + "' for the active project.");
            return {
                LoadCode::ready,
                handle,
                "Verified native extension loaded."};
        }
        catch (const std::bad_alloc&)
        {
            return {
                LoadCode::allocation_failure,
                {},
                "Extension loader allocation failed."};
        }
        catch (...)
        {
            return {
                LoadCode::invalid_request,
                {},
                "Extension load failed before activation."};
        }
    }

    bool PluginLoader::unload(PluginHandle handle) noexcept
    {
        if (!implementation_ || !handle)
            return false;

        std::unique_ptr<Implementation::Record> record{};
        {
            std::unique_lock lock{implementation_->mutex};
            if (handle.index >= implementation_->records.size()
                || implementation_->generations[handle.index]
                    != handle.generation
                || !implementation_->records[handle.index])
            {
                return false;
            }
            record = std::move(
                implementation_->records[handle.index]);
        }

        epoch_extension_host_api host = make_host_api();
        try
        {
            record->descriptor->on_unload(&host);
        }
        catch (...)
        {
            logger::error(
                "Extensions",
                "Extension unload crossed the ABI with an exception.");
        }
        close_library(record->library);
        record->library = nullptr;
        logger::info(
            "Extensions",
            "Unloaded native extension '"
                + record->info.plugin_id + "'.");
        return true;
    }

    void PluginLoader::unload_all() noexcept
    {
        if (!implementation_)
            return;

        for (;;)
        {
            PluginHandle handle{};
            {
                std::unique_lock lock{implementation_->mutex};
                for (const auto& record : implementation_->records)
                {
                    if (record)
                    {
                        handle = record->info.handle;
                        break;
                    }
                }
            }
            if (!handle)
                return;
            (void)unload(handle);
        }
    }

    std::vector<PluginInfo> PluginLoader::snapshot() const
    {
        std::vector<PluginInfo> result{};
        if (!implementation_)
            return result;

        std::unique_lock lock{implementation_->mutex};
        result.reserve(implementation_->records.size());
        for (const auto& record : implementation_->records)
        {
            if (record)
                result.push_back(record->info);
        }
        return result;
    }

    std::optional<PluginInfo> PluginLoader::find(
        PluginHandle handle) const
    {
        if (!implementation_ || !handle)
            return std::nullopt;

        std::unique_lock lock{implementation_->mutex};
        if (handle.index >= implementation_->records.size()
            || implementation_->generations[handle.index]
                != handle.generation)
        {
            return std::nullopt;
        }
        const auto& record =
            implementation_->records[handle.index];
        return record
            ? std::optional<PluginInfo>{record->info}
            : std::nullopt;
    }

    ContractFailure run_contract() noexcept
    {
        constexpr std::string_view packageId{
            package_registry::kEngineForestFactoryPackageId};
        constexpr std::string_view revision{
            "0123456789abcdef0123456789abcdef01234567"};
        constexpr std::string_view hash{
            "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
        constexpr std::string_view license{"LICENSE"};
        constexpr std::string_view build{"contract:pass"};

        const auto requirement =
            package_registry::payload_requirement(packageId);
        const package_registry::PackagePayloadEvidence evidence{
            .packageId = packageId,
            .sourceRepo = requirement.sourceRepo,
            .immutableRevision = revision,
            .contentHash = hash,
            .licenseEvidence = license,
            .buildTestEvidence = build,
            .approvedContentHash = hash,
            .immutableRevisionVerified = true,
            .contentHashVerified = true,
            .licenseEvidenceVerified = true,
            .buildTestEvidenceVerified = true,
            .humanApproved = true,
            .containsNativeCode = true,
            .nativeCodeApproved = true};

        PluginLoader loader{};
        PluginAdmissionRequest request{
            .requirement = requirement,
            .evidence = evidence,
            .binary_path =
                std::filesystem::path{"missing_epoch_extension"},
            .expected_content_hash = std::string{hash},
            .target_platform =
                std::string{current_platform_name()},
            .target_architecture =
                std::string{current_architecture_name()},
            .permitted_capabilities =
                EPOCH_EXTENSION_CAPABILITY_RUNTIME,
            .project_admitted = true,
            .network_approved = false};

        PluginAdmissionRequest invalid{};
        if (loader.load(invalid).code
            != LoadCode::invalid_request)
        {
            return ContractFailure::invalid_request_not_rejected;
        }

        request.project_admitted = false;
        if (loader.load(request).code
            != LoadCode::project_not_admitted)
        {
            return ContractFailure::project_gate_not_enforced;
        }
        request.project_admitted = true;

        auto unapprovedEvidence = evidence;
        unapprovedEvidence.nativeCodeApproved = false;
        request.evidence = unapprovedEvidence;
        if (loader.load(request).code
            != LoadCode::package_evidence_rejected)
        {
            return ContractFailure::native_approval_not_enforced;
        }
        request.evidence = evidence;

        if (loader.load(request).code
            != LoadCode::path_missing)
        {
            return ContractFailure::missing_binary_not_reported;
        }

        if (loader.unload(PluginHandle{0u, 1u}))
            return ContractFailure::stale_handle_not_rejected;
        return ContractFailure::none;
    }
}