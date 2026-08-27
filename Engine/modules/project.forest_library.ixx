/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

export module project.forest_library;

import forest.factory;
import project.asset_registry;

export namespace epochengine::project_forests
{
    enum class ForestLibraryCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_library,
        invalid_path,
        outside_forest_assets,
        unsupported_extension,
        invalid_source,
        invalid_artifact,
        source_too_large,
        artifact_too_large,
        artifact_limit_exceeded,
        directory_failure,
        read_failure,
        write_failure,
        atomic_replace_failure,
        integrity_failure,
        identity_collision,
        ambiguous_artifact,
        not_found,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view forest_library_code_name(
        ForestLibraryCode code) noexcept
    {
        switch (code)
        {
        case ForestLibraryCode::ready: return "ready";
        case ForestLibraryCode::unchanged: return "unchanged";
        case ForestLibraryCode::invalid_library: return "invalid_library";
        case ForestLibraryCode::invalid_path: return "invalid_path";
        case ForestLibraryCode::outside_forest_assets:
            return "outside_forest_assets";
        case ForestLibraryCode::unsupported_extension:
            return "unsupported_extension";
        case ForestLibraryCode::invalid_source: return "invalid_source";
        case ForestLibraryCode::invalid_artifact: return "invalid_artifact";
        case ForestLibraryCode::source_too_large: return "source_too_large";
        case ForestLibraryCode::artifact_too_large:
            return "artifact_too_large";
        case ForestLibraryCode::artifact_limit_exceeded:
            return "artifact_limit_exceeded";
        case ForestLibraryCode::directory_failure: return "directory_failure";
        case ForestLibraryCode::read_failure: return "read_failure";
        case ForestLibraryCode::write_failure: return "write_failure";
        case ForestLibraryCode::atomic_replace_failure:
            return "atomic_replace_failure";
        case ForestLibraryCode::integrity_failure: return "integrity_failure";
        case ForestLibraryCode::identity_collision:
            return "identity_collision";
        case ForestLibraryCode::ambiguous_artifact:
            return "ambiguous_artifact";
        case ForestLibraryCode::not_found: return "not_found";
        case ForestLibraryCode::allocation_failure:
            return "allocation_failure";
        }
        return "unknown";
    }

    struct ForestLibraryLimits final
    {
        std::uint64_t maximum_source_bytes{2ull * 1024ull * 1024ull};
        std::uint64_t maximum_artifact_bytes{64ull * 1024ull * 1024ull};
        std::uint32_t maximum_artifacts_per_asset{64u};
        std::uint32_t maximum_name_bytes{512u};
        std::uint32_t maximum_project_root_bytes{4'096u};
        project_assets::RegistryLimits registry{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_source_bytes != 0u
                && maximum_source_bytes <= 2ull * 1024ull * 1024ull
                && maximum_artifact_bytes != 0u
                && maximum_artifact_bytes <= 64ull * 1024ull * 1024ull
                && maximum_artifacts_per_asset != 0u
                && maximum_name_bytes != 0u
                && maximum_name_bytes <= 4'096u
                && maximum_project_root_bytes != 0u
                && registry.valid();
        }
    };

    struct ForestLibraryMetrics final
    {
        std::uint64_t publish_requests{};
        std::uint64_t publications{};
        std::uint64_t unchanged_publications{};
        std::uint64_t source_load_requests{};
        std::uint64_t source_loads{};
        std::uint64_t artifact_load_requests{};
        std::uint64_t artifact_loads{};
        std::uint64_t bytes_written{};
        std::uint64_t bytes_read{};
        std::uint64_t rejected_operations{};
    };

    struct ForestSourceLocator final
    {
        ForestLibraryCode code{ForestLibraryCode::invalid_library};
        std::string canonical_logical_path{};
        std::filesystem::path storage_path{};
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        project_assets::AssetRevision revision{};
        std::uint64_t document_content_hash{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == ForestLibraryCode::ready
                    || code == ForestLibraryCode::unchanged)
                && !canonical_logical_path.empty()
                && !storage_path.empty()
                && project_key != 0u && asset_key != 0u
                && static_cast<bool>(revision)
                && document_content_hash != 0u
                && serialized_bytes != 0u;
        }
    };

    struct ForestArtifactLocator final
    {
        ForestLibraryCode code{ForestLibraryCode::invalid_library};
        std::string canonical_logical_path{};
        std::filesystem::path storage_path{};
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        project_assets::AssetContentHash artifact_key{};
        std::uint64_t source_revision{};
        std::uint64_t source_content_hash{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == ForestLibraryCode::ready
                    || code == ForestLibraryCode::unchanged)
                && !canonical_logical_path.empty()
                && !storage_path.empty()
                && project_key != 0u && asset_key != 0u
                && !artifact_key.empty()
                && source_revision != 0u && source_content_hash != 0u
                && serialized_bytes != 0u;
        }
    };

    struct PublishedForestAsset final
    {
        ForestLibraryCode code{ForestLibraryCode::invalid_library};
        ForestSourceLocator source{};
        ForestArtifactLocator artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == ForestLibraryCode::ready
                    || code == ForestLibraryCode::unchanged)
                && static_cast<bool>(source)
                && static_cast<bool>(artifact);
        }
    };

    struct LoadedForestSource final
    {
        ForestLibraryCode code{ForestLibraryCode::invalid_library};
        ForestSourceLocator locator{};
        forest::ForestAssetDocument document{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ForestLibraryCode::ready
                && static_cast<bool>(locator);
        }
    };

    struct LoadedForestArtifact final
    {
        ForestLibraryCode code{ForestLibraryCode::invalid_library};
        ForestArtifactLocator locator{};
        forest::CompiledForestAsset artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ForestLibraryCode::ready
                && static_cast<bool>(locator) && artifact.valid;
        }
    };

    class ForestAssetLibrary final
    {
    public:
        ForestAssetLibrary(
            std::string projectId,
            std::filesystem::path projectRoot,
            ForestLibraryLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const ForestLibraryLimits& limits() const noexcept;
        [[nodiscard]] ForestLibraryMetrics metrics() const noexcept;

        [[nodiscard]] PublishedForestAsset publish(
            std::string_view logicalPath,
            const forest::ForestAssetDocument& document) noexcept;
        [[nodiscard]] LoadedForestSource load_source(
            std::string_view logicalPath) noexcept;
        [[nodiscard]] LoadedForestArtifact load_exact(
            std::string_view logicalPath,
            const project_assets::AssetContentHash& artifactKey) noexcept;
        [[nodiscard]] LoadedForestArtifact load_latest(
            std::string_view logicalPath) noexcept;

    private:
        [[nodiscard]] PublishedForestAsset reject_publish(
            ForestLibraryCode code) noexcept;
        [[nodiscard]] LoadedForestSource reject_source(
            ForestLibraryCode code) noexcept;
        [[nodiscard]] LoadedForestArtifact reject_artifact(
            ForestLibraryCode code) noexcept;

        std::string project_id_{};
        std::filesystem::path project_root_{};
        ForestLibraryLimits limits_{};
        std::uint64_t project_key_{};
        ForestLibraryMetrics metrics_{};
    };

    enum class ForestLibraryContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        construction,
        invalid_path,
        first_publication,
        unchanged_publication,
        source_reopen,
        artifact_reopen,
        restart_reopen,
        second_revision,
        exact_old_revision,
        latest_revision,
        malformed_source,
        malformed_artifact,
        metrics
    };

    [[nodiscard]] constexpr std::string_view
        forest_library_contract_failure_name(
            ForestLibraryContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ForestLibraryContractFailure::none: return "pass";
        case ForestLibraryContractFailure::temporary_root:
            return "temporary_root";
        case ForestLibraryContractFailure::construction: return "construction";
        case ForestLibraryContractFailure::invalid_path: return "invalid_path";
        case ForestLibraryContractFailure::first_publication:
            return "first_publication";
        case ForestLibraryContractFailure::unchanged_publication:
            return "unchanged_publication";
        case ForestLibraryContractFailure::source_reopen:
            return "source_reopen";
        case ForestLibraryContractFailure::artifact_reopen:
            return "artifact_reopen";
        case ForestLibraryContractFailure::restart_reopen:
            return "restart_reopen";
        case ForestLibraryContractFailure::second_revision:
            return "second_revision";
        case ForestLibraryContractFailure::exact_old_revision:
            return "exact_old_revision";
        case ForestLibraryContractFailure::latest_revision:
            return "latest_revision";
        case ForestLibraryContractFailure::malformed_source:
            return "malformed_source";
        case ForestLibraryContractFailure::malformed_artifact:
            return "malformed_artifact";
        case ForestLibraryContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ForestLibraryContractFailure
        project_forest_library_contract_failure() noexcept;
}
