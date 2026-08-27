/*
 * EPOCH - authenticated private source access
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <filesystem>
#include <string>

export module updater.source_access;

export namespace epochengine::updater
{
    enum class SourceAccessPhase
    {
        Idle,
        Starting,
        AwaitingApproval,
        ExchangingToken,
        FetchingManifest,
        Downloading,
        Decrypting,
        Ready,
        Cancelled,
        Failed
    };

    struct SourceAccessStatus
    {
        SourceAccessPhase phase{ SourceAccessPhase::Idle };
        std::string message{};
        std::string user_code{};
        std::string verification_uri{};
        float progress{};
    };

    struct PreparedSourceArchive
    {
        bool ok{};
        std::filesystem::path archive_path{};
        std::filesystem::path cleanup_root{};
        std::string archive_format{};
        std::string source_version{};
        std::string commit{};
        std::string message{};
    };

    SourceAccessStatus private_source_access_status();
    void cancel_private_source_access() noexcept;
    bool private_source_access_enabled() noexcept;
    bool private_source_device_registered() noexcept;
    void reset_private_source_device_registration() noexcept;
    PreparedSourceArchive acquire_private_source_archive(
        const std::filesystem::path& protected_cache_root);
    bool private_source_access_contract_self_test();
}
