// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

export module package.source_install;
import package.registry;

export namespace epochengine::package_source
{
    enum class Code : std::uint8_t
    {
        review_ready, staged, already_staged, invalid_manifest, unknown_package,
        unsafe_path, missing_file, budget_exceeded, source_changed,
        approval_required, cancelled, cache_collision, io_failure
    };

    struct SourceFile final
    {
        std::string relative_path{};
        std::uint64_t bytes{};
        std::string sha256{};
    };

    struct Inspection final
    {
        Code code{Code::invalid_manifest};
        std::string message{};
        std::filesystem::path manifest_path{};
        std::filesystem::path repository_root{};
        std::string registry_package_id{};
        std::string manifest_id{};
        std::string display_name{};
        std::string summary{};
        std::string license{};
        std::string declared_repository{};
        std::string source_path{};
        std::string content_hash{};
        std::vector<SourceFile> files{};
        std::uint64_t total_bytes{};

        [[nodiscard]] bool review_ready() const noexcept
        { return code == Code::review_ready; }
    };

    struct StageResult final
    {
        Code code{Code::io_failure};
        std::string message{};
        std::string content_hash{};
        std::filesystem::path source_root{};
        std::filesystem::path receipt_path{};

        [[nodiscard]] bool source_staged() const noexcept
        { return code == Code::staged || code == Code::already_staged; }
    };

    struct CachedSource final
    {
        Inspection inspection{};
        StageResult staged{};
        [[nodiscard]] bool review_ready() const noexcept
        { return inspection.review_ready() && staged.source_staged(); }
    };

    // Rediscover one exact, fully verified local snapshot for an existing
    // registry binding. Multiple versions require an explicit manifest choice.
    // Discovery never approves project attachment or executes source.
    [[nodiscard]] CachedSource inspect_cached_source(
        const std::filesystem::path& cache_root, std::string_view package_id,
        std::stop_token cancellation = {});

    // Explicit review of the caller's current durable Extensions source may
    // stage a new immutable snapshot. Cache-only ambiguity still needs a source
    // choice; corrupt versions and project attachments are never replaced.
    // This copies source only, without project approval or execution.
    [[nodiscard]] CachedSource inspect_available_source(
        const std::filesystem::path& cache_root,
        const std::filesystem::path& extensions_repository,
        std::string_view package_id, std::stop_token cancellation = {});

    struct ProjectAttachment final
    {
        Code code{Code::io_failure};
        std::string message{};
        std::string registry_package_id{};
        std::string manifest_id{};
        std::string content_hash{};
        std::filesystem::path relative_source_root{};
        std::filesystem::path relative_receipt_path{};
        std::filesystem::path retained_previous_source{};

        [[nodiscard]] bool source_attached() const noexcept
        { return code == Code::staged || code == Code::already_staged; }
    };

    using SourceFeature = package_registry::SourceFeature;
    struct ProjectFeatureReview final
    {
        Code code{Code::invalid_manifest};
        std::string message{};
        std::filesystem::path project_root{};
        std::string project_manifest_sha256{};
        SourceFeature feature{SourceFeature::planetary_ocean};
        bool enabled{};
        ProjectAttachment owner{}, attached_dependency{};
        CachedSource dependency{};
        [[nodiscard]] bool needs_approval() const noexcept { return code == Code::review_ready; }
        [[nodiscard]] bool sources_admitted() const noexcept { return code == Code::already_staged; }
    };
    struct ProjectFeatureAdmission final
    {
        Code code{Code::io_failure};
        std::string message{};
        ProjectAttachment owner{}, dependency{};
        [[nodiscard]] bool sources_admitted() const noexcept
        { return code == Code::staged || code == Code::already_staged; }
    };
    // Discovers the exact optional requirement through ordinary source review.
    // May stage an immutable cache snapshot; never edits project settings,
    // attaches unapproved source, downloads, compiles or activates rendering.
    [[nodiscard]] ProjectFeatureReview review_project_feature(
        const std::filesystem::path& project_root, std::string_view owner_package,
        SourceFeature feature, bool enabled, const std::filesystem::path& cache_root,
        const std::filesystem::path& extensions_repository, std::stop_token cancellation = {});
    // Binds approval to the reviewed project manifest and actual parent source.
    // Reuses the normal attachment transaction; existing versions are never
    // replaced. Feature settings/build/render activation remain caller-owned.
    [[nodiscard]] ProjectFeatureAdmission admit_project_feature(
        const ProjectFeatureReview&, std::string_view approved_dependency_hash,
        std::stop_token cancellation = {});

    [[nodiscard]] Inspection inspect_local_source(
        const std::filesystem::path& manifest,
        std::stop_token cancellation = {});
    // Windows consumers retain Epoch's existing long-path-aware host manifest.
    // Native path lengths are bounded; no OS setting or source execution changes.
    [[nodiscard]] StageResult verify_staged_source(
        const Inspection& reviewed,
        const std::filesystem::path& cache_root,
        std::stop_token cancellation = {});
    [[nodiscard]] StageResult stage_local_source(
        const Inspection& reviewed,
        std::string_view approved_content_hash,
        const std::filesystem::path& cache_root,
        std::stop_token cancellation = {});

    // Explicit project dependency approval, separate from cache staging. This
    // copies reviewed source only; compilation/publication remain project jobs.
    [[nodiscard]] ProjectAttachment attach_project_source(
        const Inspection& reviewed, const StageResult& staged,
        std::string_view approved_content_hash,
        const std::filesystem::path& project_root,
        std::stop_token cancellation = {});
    [[nodiscard]] ProjectAttachment verify_project_source(
        const Inspection& reviewed, const std::filesystem::path& project_root,
        std::stop_token cancellation = {});
    // Explicit reviewed replacement only. Old source is retained; project
    // scenes/settings are never rewritten by this transaction.
    [[nodiscard]] ProjectAttachment update_project_source(
        const Inspection& reviewed, const StageResult& staged,
        std::string_view approved_content_hash, const std::filesystem::path& project_root,
        std::string_view expected_previous_hash, std::stop_token cancellation = {});
    // Exact known registry or manifest ID only; no unbounded project scan and
    // no dependency on the original source folder or staging cache.
    [[nodiscard]] ProjectAttachment load_project_attachment(
        const std::filesystem::path& project_root, std::string_view package_id,
        std::stop_token cancellation = {});
}
