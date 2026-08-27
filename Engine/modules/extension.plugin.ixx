// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module extension.plugin;

import package.registry;

export
{
#include "../include/epoch/extension/extension.plugin_api.hpp"
}

export namespace epochengine::extensions
{
    inline constexpr std::uint32_t kExtensionAbiVersion =
        EPOCH_EXTENSION_ABI_VERSION;
    inline constexpr std::string_view kExtensionQuerySymbol =
        EPOCH_EXTENSION_QUERY_SYMBOL;

    inline constexpr std::uint64_t kMaximumPluginBytes =
        512ull * 1024ull * 1024ull;

    struct PluginHandle final
    {
        std::uint32_t index{(std::numeric_limits<std::uint32_t>::max)()};
        std::uint32_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return index != (std::numeric_limits<std::uint32_t>::max)()
                && generation != 0u;
        }

        friend constexpr bool operator==(
            const PluginHandle&,
            const PluginHandle&) noexcept = default;
    };

    enum class LoadCode : std::uint8_t
    {
        ready = 0,
        invalid_request,
        project_not_admitted,
        package_evidence_rejected,
        platform_mismatch,
        architecture_mismatch,
        invalid_expected_hash,
        path_missing,
        not_regular_file,
        file_too_large,
        file_read_failed,
        content_hash_mismatch,
        native_load_failed,
        query_symbol_missing,
        descriptor_rejected,
        capabilities_rejected,
        network_approval_required,
        duplicate_plugin,
        plugin_start_failed,
        allocation_failure
    };

    struct PluginAdmissionRequest final
    {
        package_registry::PackagePayloadRequirement requirement{};
        package_registry::PackagePayloadEvidence evidence{};
        std::filesystem::path binary_path{};
        std::string expected_content_hash{};
        std::string target_platform{};
        std::string target_architecture{};
        std::uint64_t permitted_capabilities{
            EPOCH_EXTENSION_CAPABILITY_RUNTIME};
        bool project_admitted{};
        bool network_approved{};
    };

    struct PluginInfo final
    {
        PluginHandle handle{};
        std::string package_id{};
        std::string plugin_id{};
        std::string display_name{};
        std::string plugin_version{};
        std::filesystem::path source_path{};
        std::string content_hash{};
        std::uint64_t capabilities{};
    };

    struct LoadResult final
    {
        LoadCode code{LoadCode::invalid_request};
        PluginHandle handle{};
        std::string message{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == LoadCode::ready
                && static_cast<bool>(handle);
        }
    };

    enum class ContractFailure : std::uint8_t
    {
        none = 0,
        invalid_request_not_rejected,
        project_gate_not_enforced,
        native_approval_not_enforced,
        missing_binary_not_reported,
        stale_handle_not_rejected
    };

    [[nodiscard]] std::string_view load_code_name(
        LoadCode code) noexcept;
    [[nodiscard]] std::string_view current_platform_name() noexcept;
    [[nodiscard]] std::string_view current_architecture_name() noexcept;

    class PluginLoader final
    {
    public:
        PluginLoader();
        ~PluginLoader();

        PluginLoader(const PluginLoader&) = delete;
        PluginLoader& operator=(const PluginLoader&) = delete;
        PluginLoader(PluginLoader&&) noexcept;
        PluginLoader& operator=(PluginLoader&&) noexcept;

        [[nodiscard]] LoadResult load(
            const PluginAdmissionRequest& request) noexcept;
        [[nodiscard]] bool unload(PluginHandle handle) noexcept;
        void unload_all() noexcept;
        [[nodiscard]] std::vector<PluginInfo> snapshot() const;
        [[nodiscard]] std::optional<PluginInfo> find(
            PluginHandle handle) const;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_{};
    };

    [[nodiscard]] ContractFailure run_contract() noexcept;
}