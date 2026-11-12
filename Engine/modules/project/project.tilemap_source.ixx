/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

export module project.tilemap_source;

import authoring.tilemap;
import project.asset_registry;

export namespace epochengine::project_tilemap_sources
{
    enum class SourceCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_store,
        invalid_path,
        outside_maps,
        not_found,
        source_too_large,
        directory_failure,
        read_failure,
        write_failure,
        integrity_failure,
        malformed_source,
        atomic_replace_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view source_code_name(
        SourceCode code) noexcept
    {
        switch (code)
        {
        case SourceCode::ready: return "ready";
        case SourceCode::unchanged: return "unchanged";
        case SourceCode::invalid_store: return "invalid_store";
        case SourceCode::invalid_path: return "invalid_path";
        case SourceCode::outside_maps: return "outside_maps";
        case SourceCode::not_found: return "not_found";
        case SourceCode::source_too_large: return "source_too_large";
        case SourceCode::directory_failure: return "directory_failure";
        case SourceCode::read_failure: return "read_failure";
        case SourceCode::write_failure: return "write_failure";
        case SourceCode::integrity_failure: return "integrity_failure";
        case SourceCode::malformed_source: return "malformed_source";
        case SourceCode::atomic_replace_failure:
            return "atomic_replace_failure";
        case SourceCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct SourceLimits final
    {
        project_assets::RegistryLimits registry{};
        authoring::tilemap::DocumentLimits document{};
        std::uint64_t maximum_source_bytes{
            512ull * 1024ull * 1024ull};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return registry.valid() && document.valid()
                && maximum_source_bytes != 0u
                && maximum_source_bytes <= document.maximum_snapshot_bytes;
        }
    };

    struct SourceMetrics final
    {
        std::uint64_t save_requests{};
        std::uint64_t saves{};
        std::uint64_t unchanged_saves{};
        std::uint64_t load_requests{};
        std::uint64_t loads{};
        std::uint64_t rejected_operations{};
        std::uint64_t bytes_read{};
        std::uint64_t bytes_written{};
    };

    struct SourceLocation final
    {
        SourceCode code{SourceCode::invalid_store};
        std::string canonical_logical_path{};
        std::filesystem::path storage_path{};
        authoring::tilemap::DocumentRevision revision{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == SourceCode::ready
                    || code == SourceCode::unchanged)
                && !canonical_logical_path.empty()
                && !storage_path.empty()
                && revision.sequence != 0u
                && serialized_bytes != 0u;
        }
    };

    struct LoadedSource final
    {
        SourceCode code{SourceCode::invalid_store};
        SourceLocation location{};
        std::unique_ptr<authoring::tilemap::Document> document{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SourceCode::ready
                && static_cast<bool>(location)
                && document != nullptr && document->valid();
        }
    };

    class ProjectTileMapSourceStore final
    {
    public:
        ProjectTileMapSourceStore(
            std::string projectId,
            std::filesystem::path projectRoot,
            SourceLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] const SourceLimits& limits() const noexcept;
        [[nodiscard]] SourceMetrics metrics() const noexcept;

        [[nodiscard]] SourceLocation save(
            std::string_view logicalPath,
            const authoring::tilemap::Document& document) noexcept;
        [[nodiscard]] LoadedSource load(
            std::string_view logicalPath) noexcept;

    private:
        [[nodiscard]] SourceLocation reject_location(
            SourceCode code) noexcept;
        [[nodiscard]] LoadedSource reject_load(SourceCode code) noexcept;

        std::string project_id_{};
        std::filesystem::path project_root_{};
        SourceLimits limits_{};
        std::uint64_t project_key_{};
        SourceMetrics metrics_{};
    };

    enum class SourceContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        construction,
        invalid_path,
        outside_maps,
        first_save,
        unchanged_save,
        first_load,
        overwrite,
        overwritten_load,
        malformed_source,
        missing_source,
        metrics
    };

    [[nodiscard]] constexpr std::string_view source_contract_failure_name(
        SourceContractFailure failure) noexcept
    {
        switch (failure)
        {
        case SourceContractFailure::none: return "pass";
        case SourceContractFailure::temporary_root: return "temporary_root";
        case SourceContractFailure::construction: return "construction";
        case SourceContractFailure::invalid_path: return "invalid_path";
        case SourceContractFailure::outside_maps: return "outside_maps";
        case SourceContractFailure::first_save: return "first_save";
        case SourceContractFailure::unchanged_save: return "unchanged_save";
        case SourceContractFailure::first_load: return "first_load";
        case SourceContractFailure::overwrite: return "overwrite";
        case SourceContractFailure::overwritten_load: return "overwritten_load";
        case SourceContractFailure::malformed_source: return "malformed_source";
        case SourceContractFailure::missing_source: return "missing_source";
        case SourceContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] SourceContractFailure
        project_tilemap_source_contract_failure() noexcept;
}
