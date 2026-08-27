/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module project.gui_library;

namespace epochengine::project_gui
{
    namespace
    {
        namespace fs = std::filesystem;

        inline constexpr std::string_view artifactExtension = ".epoch_gui";
        inline constexpr std::uint32_t maximumStoragePathBytes = 32'768u;

        struct ReadResult final
        {
            LibraryCode code{LibraryCode::read_failure};
            asset::gui::CompiledGuiArtifact artifact{};
            std::vector<std::byte> bytes{};
        };

        [[nodiscard]] std::string hex_u64(std::uint64_t value)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result(16u, '0');
            for (std::size_t index = 0u; index < result.size(); ++index)
            {
                const std::size_t shift = (result.size() - index - 1u) * 4u;
                result[index] = digits[(value >> shift) & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] std::string hash_hex(
            const asset::gui::ContentHash& hash)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result{};
            result.reserve(hash.bytes.size() * 2u);
            for (const auto byte : hash.bytes)
            {
                result.push_back(digits[byte >> 4u]);
                result.push_back(digits[byte & 0x0fu]);
            }
            return result;
        }

        [[nodiscard]] fs::path library_root(std::string_view projectRoot)
        {
            return fs::path{projectRoot} / "Library" / "Gui";
        }

        [[nodiscard]] fs::path asset_directory(
            std::string_view projectRoot,
            std::uint64_t assetKey)
        {
            return library_root(projectRoot) / hex_u64(assetKey);
        }

        [[nodiscard]] fs::path artifact_path(
            std::string_view projectRoot,
            std::uint64_t assetKey,
            const asset::gui::ContentHash& key)
        {
            return asset_directory(projectRoot, assetKey)
                / (hash_hex(key) + std::string{artifactExtension});
        }

        [[nodiscard]] bool artifact_file(const fs::path& path) noexcept
        {
            return path.extension().generic_string() == artifactExtension;
        }

        [[nodiscard]] ReadResult read_artifact(
            const fs::path& path,
            const LibraryLimits& limits) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::is_regular_file(path, error) || error)
                    return {LibraryCode::not_found};
                const std::uintmax_t fileBytes = fs::file_size(path, error);
                if (error || fileBytes == 0u)
                    return {LibraryCode::read_failure};
                if (fileBytes > limits.maximum_serialized_bytes
                    || fileBytes > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::size_t>::max)()))
                {
                    return {LibraryCode::serialized_budget_exceeded};
                }

                std::vector<std::byte> bytes(
                    static_cast<std::size_t>(fileBytes));
                std::ifstream input{path, std::ios::binary};
                if (!input)
                    return {LibraryCode::read_failure};
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!input || input.gcount()
                    != static_cast<std::streamsize>(bytes.size()))
                {
                    return {LibraryCode::read_failure};
                }

                auto decoded = asset::gui::deserialize(
                    bytes, limits.artifact);
                if (!decoded)
                    return {LibraryCode::integrity_failure};
                return {
                    LibraryCode::ready,
                    std::move(decoded.artifact),
                    std::move(bytes)};
            }
            catch (...)
            {
                return {LibraryCode::allocation_failure};
            }
        }

        [[nodiscard]] LibraryCode write_bytes(
            const fs::path& path,
            const std::vector<std::byte>& bytes) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return LibraryCode::write_failure;
                output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                    return LibraryCode::write_failure;
                output.close();
                return output ? LibraryCode::ready
                              : LibraryCode::write_failure;
            }
            catch (...)
            {
                return LibraryCode::write_failure;
            }
        }

        [[nodiscard]] std::uint64_t next_temporary_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        [[nodiscard]] ArtifactLocator make_locator(
            LibraryCode code,
            std::string canonicalPath,
            const fs::path& storagePath,
            std::uint64_t projectKey,
            std::uint64_t assetKey,
            const asset::gui::CompiledGuiArtifact& artifact,
            std::uint64_t serializedBytes)
        {
            return {
                code,
                std::move(canonicalPath),
                storagePath.generic_string(),
                projectKey,
                assetKey,
                artifact.identity.key,
                artifact.identity.source_revision,
                serializedBytes};
        }

        [[nodiscard]] bool newer(
            const asset::gui::CompiledGuiArtifact& candidate,
            const asset::gui::CompiledGuiArtifact& current) noexcept
        {
            if (candidate.identity.source_revision.sequence
                != current.identity.source_revision.sequence)
            {
                return candidate.identity.source_revision.sequence
                    > current.identity.source_revision.sequence;
            }
            return hash_hex(candidate.identity.key)
                > hash_hex(current.identity.key);
        }
    }

    ArtifactLibrary::ArtifactLibrary(
        std::string projectId,
        std::string projectRoot,
        LibraryLimits limits) noexcept
        : project_id_(std::move(projectId)), limits_(limits)
    {
        if (!limits_.valid() || projectRoot.empty()
            || projectRoot.size() > limits_.maximum_project_root_bytes)
        {
            return;
        }
        project_key_ = project_assets::stable_project_identity(
            project_id_, limits_.registry);
        if (project_key_ == 0u)
            return;
        try
        {
            std::error_code error{};
            fs::path root{std::move(projectRoot)};
            if (root.is_relative())
                root = fs::absolute(root, error);
            if (error || root.empty())
            {
                project_key_ = 0u;
                return;
            }
            project_root_ = root.lexically_normal().generic_string();
            if (project_root_.empty()
                || project_root_.size() > limits_.maximum_project_root_bytes)
            {
                project_root_.clear();
                project_key_ = 0u;
            }
        }
        catch (...)
        {
            project_root_.clear();
            project_key_ = 0u;
        }
    }

    bool ArtifactLibrary::valid() const noexcept
    {
        return limits_.valid() && project_key_ != 0u && !project_root_.empty();
    }

    std::string_view ArtifactLibrary::project_id() const noexcept
    {
        return project_id_;
    }

    std::string_view ArtifactLibrary::project_root() const noexcept
    {
        return project_root_;
    }

    std::uint64_t ArtifactLibrary::project_key() const noexcept
    {
        return project_key_;
    }

    const LibraryLimits& ArtifactLibrary::limits() const noexcept
    {
        return limits_;
    }

    LibraryMetrics ArtifactLibrary::metrics() const noexcept
    {
        return metrics_;
    }

    ArtifactLocator ArtifactLibrary::identify(
        std::string_view logicalPath,
        const asset::gui::CompiledGuiArtifact& artifact) noexcept
    {
        ++metrics_.identify_requests;
        if (!valid())
            return reject_locator(LibraryCode::invalid_library);
        if (!asset::gui::validate(artifact, limits_.artifact))
            return reject_locator(LibraryCode::invalid_artifact);
        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_locator(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::gui_document,
                *canonical);
            const auto serialized = asset::gui::serialize(
                artifact, limits_.artifact);
            if (assetKey == 0u || !serialized)
                return reject_locator(LibraryCode::invalid_artifact);
            if (serialized.bytes.size() > limits_.maximum_serialized_bytes)
                return reject_locator(
                    LibraryCode::serialized_budget_exceeded);
            const fs::path path = artifact_path(
                project_root_, assetKey, artifact.identity.key);
            if (path.generic_string().size() > maximumStoragePathBytes)
                return reject_locator(LibraryCode::invalid_path);
            return make_locator(
                LibraryCode::ready, *canonical, path, project_key_, assetKey,
                artifact, serialized.bytes.size());
        }
        catch (...)
        {
            return reject_locator(LibraryCode::allocation_failure);
        }
    }

    ArtifactLocator ArtifactLibrary::persist(
        std::string_view logicalPath,
        const asset::gui::CompiledGuiArtifact& artifact) noexcept
    {
        ++metrics_.persist_requests;
        ArtifactLocator locator = identify(logicalPath, artifact);
        if (!locator)
            return locator;
        try
        {
            auto serialized = asset::gui::serialize(
                artifact, limits_.artifact);
            if (!serialized)
                return reject_locator(LibraryCode::invalid_artifact);
            const fs::path destination{locator.storage_path};
            const fs::path directory = destination.parent_path();
            std::error_code error{};
            fs::create_directories(directory, error);
            if (error || !fs::is_directory(directory, error) || error)
                return reject_locator(LibraryCode::directory_failure);

            std::uint32_t artifactCount{};
            for (fs::directory_iterator iterator{directory, error}, end;
                 !error && iterator != end; iterator.increment(error))
            {
                if (iterator->is_regular_file(error) && !error
                    && artifact_file(iterator->path()))
                {
                    ++artifactCount;
                }
            }
            if (error)
                return reject_locator(LibraryCode::directory_failure);

            if (fs::exists(destination, error) && !error)
            {
                ReadResult existing = read_artifact(destination, limits_);
                metrics_.bytes_read += existing.bytes.size();
                if (existing.code != LibraryCode::ready)
                    return reject_locator(existing.code);
                if (existing.bytes == serialized.bytes
                    && existing.artifact.identity == artifact.identity)
                {
                    ++metrics_.unchanged_writes;
                    locator.code = LibraryCode::unchanged;
                    return locator;
                }
                return reject_locator(LibraryCode::identity_collision);
            }
            if (error)
                return reject_locator(LibraryCode::read_failure);
            if (artifactCount >= limits_.maximum_artifacts_per_asset)
                return reject_locator(
                    LibraryCode::artifact_limit_exceeded);

            fs::path temporary = destination;
            temporary += ".pending." + hex_u64(next_temporary_id());
            const LibraryCode written = write_bytes(temporary, serialized.bytes);
            if (written != LibraryCode::ready)
            {
                fs::remove(temporary, error);
                return reject_locator(written);
            }
            ReadResult verified = read_artifact(temporary, limits_);
            metrics_.bytes_read += verified.bytes.size();
            if (verified.code != LibraryCode::ready
                || verified.bytes != serialized.bytes
                || verified.artifact.identity != artifact.identity)
            {
                fs::remove(temporary, error);
                return reject_locator(LibraryCode::integrity_failure);
            }

            fs::rename(temporary, destination, error);
            if (error)
            {
                fs::remove(temporary, error);
                return reject_locator(LibraryCode::write_failure);
            }
            ++metrics_.writes;
            metrics_.bytes_written += serialized.bytes.size();
            return locator;
        }
        catch (...)
        {
            return reject_locator(LibraryCode::allocation_failure);
        }
    }

    LoadedArtifact ArtifactLibrary::load_exact(
        std::string_view logicalPath,
        const asset::gui::ContentHash& artifactKey) noexcept
    {
        ++metrics_.load_requests;
        if (!valid())
            return reject_load(LibraryCode::invalid_library);
        if (artifactKey.empty())
            return reject_load(LibraryCode::invalid_artifact);
        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_load(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::gui_document,
                *canonical);
            if (assetKey == 0u)
                return reject_load(LibraryCode::invalid_path);
            const fs::path path = artifact_path(
                project_root_, assetKey, artifactKey);
            ReadResult read = read_artifact(path, limits_);
            metrics_.bytes_read += read.bytes.size();
            if (read.code != LibraryCode::ready)
                return reject_load(read.code);
            if (read.artifact.identity.key != artifactKey)
                return reject_load(LibraryCode::integrity_failure);
            ArtifactLocator locator = make_locator(
                LibraryCode::ready, *canonical, path, project_key_, assetKey,
                read.artifact, read.bytes.size());
            ++metrics_.loads;
            return {
                LibraryCode::ready,
                std::move(locator),
                std::move(read.artifact)};
        }
        catch (...)
        {
            return reject_load(LibraryCode::allocation_failure);
        }
    }

    LoadedArtifact ArtifactLibrary::load_latest(
        std::string_view logicalPath) noexcept
    {
        ++metrics_.load_requests;
        if (!valid())
            return reject_load(LibraryCode::invalid_library);
        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_load(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::gui_document,
                *canonical);
            if (assetKey == 0u)
                return reject_load(LibraryCode::invalid_path);
            const fs::path directory = asset_directory(project_root_, assetKey);
            std::error_code error{};
            if (!fs::is_directory(directory, error) || error)
                return reject_load(LibraryCode::not_found);

            bool found{};
            ReadResult selected{};
            fs::path selectedPath{};
            for (fs::directory_iterator iterator{directory, error}, end;
                 !error && iterator != end; iterator.increment(error))
            {
                if (!iterator->is_regular_file(error) || error
                    || !artifact_file(iterator->path()))
                {
                    continue;
                }
                ReadResult candidate = read_artifact(
                    iterator->path(), limits_);
                metrics_.bytes_read += candidate.bytes.size();
                if (candidate.code != LibraryCode::ready)
                    continue;
                if (!found || newer(candidate.artifact, selected.artifact))
                {
                    selected = std::move(candidate);
                    selectedPath = iterator->path();
                    found = true;
                }
            }
            if (error)
                return reject_load(LibraryCode::directory_failure);
            if (!found)
                return reject_load(LibraryCode::not_found);

            ArtifactLocator locator = make_locator(
                LibraryCode::ready, *canonical, selectedPath, project_key_,
                assetKey, selected.artifact, selected.bytes.size());
            ++metrics_.loads;
            return {
                LibraryCode::ready,
                std::move(locator),
                std::move(selected.artifact)};
        }
        catch (...)
        {
            return reject_load(LibraryCode::allocation_failure);
        }
    }

    ArtifactLocator ArtifactLibrary::reject_locator(
        LibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }

    LoadedArtifact ArtifactLibrary::reject_load(LibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }
}
