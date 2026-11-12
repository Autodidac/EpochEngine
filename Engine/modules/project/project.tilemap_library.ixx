/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>
#include <string_view>

export module project.tilemap_library;

import asset.tilemap_artifact;
import project.asset_registry;

export namespace epochengine::project_tilemaps
{
    enum class LibraryCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_library,
        invalid_path,
        invalid_artifact,
        serialized_budget_exceeded,
        artifact_limit_exceeded,
        directory_failure,
        write_failure,
        read_failure,
        integrity_failure,
        identity_collision,
        not_found,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view library_code_name(
        LibraryCode code) noexcept
    {
        switch (code)
        {
        case LibraryCode::ready: return "ready";
        case LibraryCode::unchanged: return "unchanged";
        case LibraryCode::invalid_library: return "invalid_library";
        case LibraryCode::invalid_path: return "invalid_path";
        case LibraryCode::invalid_artifact: return "invalid_artifact";
        case LibraryCode::serialized_budget_exceeded:
            return "serialized_budget_exceeded";
        case LibraryCode::artifact_limit_exceeded:
            return "artifact_limit_exceeded";
        case LibraryCode::directory_failure: return "directory_failure";
        case LibraryCode::write_failure: return "write_failure";
        case LibraryCode::read_failure: return "read_failure";
        case LibraryCode::integrity_failure: return "integrity_failure";
        case LibraryCode::identity_collision: return "identity_collision";
        case LibraryCode::not_found: return "not_found";
        case LibraryCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct TileMapLibraryLimits final
    {
        std::uint32_t maximum_artifacts_per_asset{64u};
        std::uint64_t maximum_serialized_bytes{2ull * 1024ull * 1024ull * 1024ull};
        std::uint32_t maximum_project_root_bytes{4'096u};
        project_assets::RegistryLimits registry{};
        asset::tilemap::ArtifactLimits artifact{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_artifacts_per_asset != 0u
                && maximum_serialized_bytes != 0u
                && maximum_project_root_bytes != 0u
                && registry.valid() && artifact.valid()
                && maximum_serialized_bytes <= artifact.maximum_serialized_bytes;
        }
    };

    struct TileMapLibraryMetrics final
    {
        std::uint64_t identify_requests{};
        std::uint64_t persist_requests{};
        std::uint64_t writes{};
        std::uint64_t unchanged_writes{};
        std::uint64_t load_requests{};
        std::uint64_t loads{};
        std::uint64_t bytes_written{};
        std::uint64_t bytes_read{};
        std::uint64_t rejected_operations{};
    };

    struct TileMapArtifactLocator final
    {
        LibraryCode code{LibraryCode::invalid_library};
        std::string canonical_logical_path{};
        std::string storage_path{};
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        asset::tilemap::ContentHash artifact_key{};
        asset::tilemap::DocumentRevision source_revision{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == LibraryCode::ready || code == LibraryCode::unchanged)
                && !canonical_logical_path.empty() && !storage_path.empty()
                && project_key != 0u && asset_key != 0u
                && !artifact_key.empty() && static_cast<bool>(source_revision)
                && serialized_bytes != 0u;
        }
    };

    struct LoadedTileMapArtifact final
    {
        LibraryCode code{LibraryCode::invalid_library};
        TileMapArtifactLocator locator{};
        asset::tilemap::CompiledTileMapArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == LibraryCode::ready
                && static_cast<bool>(locator)
                && static_cast<bool>(artifact.identity);
        }
    };

    class TileMapArtifactLibrary final
    {
    public:
        TileMapArtifactLibrary(
            std::string projectId,
            std::string projectRoot,
            TileMapLibraryLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::string_view project_root() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const TileMapLibraryLimits& limits() const noexcept;
        [[nodiscard]] TileMapLibraryMetrics metrics() const noexcept;

        [[nodiscard]] TileMapArtifactLocator identify(
            std::string_view logicalPath,
            const asset::tilemap::CompiledTileMapArtifact& artifact) noexcept;
        [[nodiscard]] TileMapArtifactLocator persist(
            std::string_view logicalPath,
            const asset::tilemap::CompiledTileMapArtifact& artifact) noexcept;
        [[nodiscard]] LoadedTileMapArtifact load_exact(
            std::string_view logicalPath,
            const asset::tilemap::ContentHash& artifactKey) noexcept;
        [[nodiscard]] LoadedTileMapArtifact load_latest(
            std::string_view logicalPath) noexcept;

    private:
        [[nodiscard]] TileMapArtifactLocator reject_locator(
            LibraryCode code) noexcept;
        [[nodiscard]] LoadedTileMapArtifact reject_load(
            LibraryCode code) noexcept;

        std::string project_id_{};
        std::string project_root_{};
        TileMapLibraryLimits limits_{};
        std::uint64_t project_key_{};
        TileMapLibraryMetrics metrics_{};
    };
}
