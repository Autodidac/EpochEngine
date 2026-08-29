/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.model_install;

export namespace epochengine::ai::model_install
{
    inline constexpr std::string_view receipt_schema =
        "epoch.local_ai.model.snapshot.v1";
    inline constexpr std::string_view receipt_filename =
        "installed.model.json";

    struct ArtifactSpec final
    {
        std::string file{};
        std::string source_url{};
        std::uint64_t bytes{};
        std::string sha256{};

        friend bool operator==(const ArtifactSpec&, const ArtifactSpec&) = default;
    };

    struct InstallPlan final
    {
        std::string package_id{};
        std::string display_name{};
        std::string revision{};
        std::string official_source{};
        std::string artifact_source{};
        std::vector<ArtifactSpec> artifacts{};

        friend bool operator==(const InstallPlan&, const InstallPlan&) = default;
    };

    enum class Code : std::uint8_t
    {
        published,
        already_installed,
        recovered_receipt,
        unknown_package,
        invalid_plan,
        unsafe_cache_root,
        unsafe_staging_root,
        staging_missing,
        final_conflict,
        unexpected_entry,
        missing_artifact,
        invalid_artifact_type,
        size_mismatch,
        sha256_mismatch,
        receipt_invalid,
        filesystem_error
    };

    struct FileVerification final
    {
        Code code{Code::filesystem_error};
        std::uint64_t bytes{};
        std::string sha256{};
        std::string diagnostic{};

        [[nodiscard]] bool verified() const noexcept
        {
            return code == Code::published;
        }
    };

    struct PublishRequest final
    {
        InstallPlan plan{};
        std::filesystem::path models_cache_root{};
        std::filesystem::path staging_root{};
    };

    struct PublishResult final
    {
        Code code{Code::filesystem_error};
        std::filesystem::path installed_root{};
        std::filesystem::path receipt_path{};
        std::string artifact{};
        std::string diagnostic{};
        bool published{};
        bool recovered{};

        [[nodiscard]] bool accepted() const noexcept
        {
            return code == Code::published
                || code == Code::already_installed
                || code == Code::recovered_receipt;
        }
    };

    [[nodiscard]] std::optional<InstallPlan> plan_for(
        std::string_view package_id);
    [[nodiscard]] bool valid_plan(const InstallPlan& plan);
    [[nodiscard]] std::filesystem::path version_root(
        const std::filesystem::path& models_cache_root,
        const InstallPlan& plan);
    [[nodiscard]] std::filesystem::path staging_root(
        const std::filesystem::path& models_cache_root,
        const InstallPlan& plan,
        std::string_view operation_id);
    [[nodiscard]] FileVerification verify_file(
        const std::filesystem::path& path,
        const ArtifactSpec& artifact);
    [[nodiscard]] std::string deterministic_receipt(
        const InstallPlan& plan);
    [[nodiscard]] bool receipt_matches(
        const InstallPlan& plan,
        std::string_view bytes) noexcept;
    [[nodiscard]] PublishResult publish_verified_snapshot(
        const PublishRequest& request);
    [[nodiscard]] bool run_contract();
}
