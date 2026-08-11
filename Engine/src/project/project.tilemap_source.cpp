/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module project.tilemap_source;

import platform.filesystem;

namespace epochengine::project_tilemap_sources
{
    namespace
    {
        namespace fs = std::filesystem;

        struct CanonicalSource final
        {
            std::string logical_path{};
            fs::path storage_path{};
        };

        struct ReadBytesResult final
        {
            SourceCode code{SourceCode::read_failure};
            std::vector<std::byte> bytes{};
        };

        [[nodiscard]] bool begins_with_maps_root(
            std::string_view path) noexcept
        {
            constexpr std::string_view prefix{"Assets/Maps/"};
            return path.size() > prefix.size()
                && path.starts_with(prefix);
        }

        [[nodiscard]] std::optional<CanonicalSource> canonical_source(
            const fs::path& projectRoot,
            std::string_view logicalPath,
            const SourceLimits& limits,
            SourceCode& failure)
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits.registry);
            if (!canonical)
            {
                failure = SourceCode::invalid_path;
                return std::nullopt;
            }
            if (!begins_with_maps_root(*canonical))
            {
                failure = SourceCode::outside_maps;
                return std::nullopt;
            }
            const fs::path relative{*canonical};
            if (relative.extension().generic_string() != ".epochmap")
            {
                failure = SourceCode::invalid_path;
                return std::nullopt;
            }
            failure = SourceCode::ready;
            return CanonicalSource{
                *canonical,
                (projectRoot / relative).lexically_normal()};
        }

        [[nodiscard]] ReadBytesResult read_bytes(
            const fs::path& path,
            std::uint64_t maximumBytes) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::exists(path, error) || error)
                    return {error ? SourceCode::read_failure
                                  : SourceCode::not_found};
                if (!fs::is_regular_file(path, error) || error)
                    return {SourceCode::read_failure};
                const std::uintmax_t size = fs::file_size(path, error);
                if (error)
                    return {SourceCode::read_failure};
                if (size == 0u || size > maximumBytes
                    || size > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::streamsize>::max)()))
                {
                    return {size > maximumBytes
                        ? SourceCode::source_too_large
                        : SourceCode::malformed_source};
                }

                std::vector<std::byte> bytes(
                    static_cast<std::size_t>(size));
                std::ifstream input{path, std::ios::binary};
                if (!input)
                    return {SourceCode::read_failure};
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!input || input.gcount()
                    != static_cast<std::streamsize>(bytes.size()))
                {
                    return {SourceCode::read_failure};
                }
                return {SourceCode::ready, std::move(bytes)};
            }
            catch (...)
            {
                return {SourceCode::read_failure};
            }
        }

        [[nodiscard]] SourceCode write_bytes(
            const fs::path& path,
            const std::vector<std::byte>& bytes) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return SourceCode::write_failure;
                output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                    return SourceCode::write_failure;
                output.close();
                return output ? SourceCode::ready
                              : SourceCode::write_failure;
            }
            catch (...)
            {
                return SourceCode::write_failure;
            }
        }

        [[nodiscard]] std::uint64_t next_temporary_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(
                1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        [[nodiscard]] std::string decimal_u64(std::uint64_t value)
        {
            return std::to_string(value);
        }

        struct ContractRoot final
        {
            fs::path path{};

            ContractRoot()
            {
                std::error_code error{};
                path = fs::temp_directory_path(error);
                if (error)
                {
                    path.clear();
                    return;
                }
                path /= "epoch_project_tilemap_source_contract_"
                    + decimal_u64(next_temporary_id());
                fs::remove_all(path, error);
                error.clear();
                fs::create_directories(path, error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                fs::remove_all(path, error);
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return !path.empty();
            }
        };
    }

    ProjectTileMapSourceStore::ProjectTileMapSourceStore(
        std::string projectId,
        std::filesystem::path projectRoot,
        SourceLimits limits) noexcept
        : project_id_(std::move(projectId)),
          project_root_(std::move(projectRoot)),
          limits_(limits)
    {
        if (limits_.valid() && !project_root_.empty())
        {
            project_key_ = project_assets::stable_project_identity(
                project_id_, limits_.registry);
        }
    }

    bool ProjectTileMapSourceStore::valid() const noexcept
    {
        return project_key_ != 0u && !project_root_.empty()
            && limits_.valid();
    }

    std::string_view ProjectTileMapSourceStore::project_id() const noexcept
    {
        return project_id_;
    }

    const std::filesystem::path&
        ProjectTileMapSourceStore::project_root() const noexcept
    {
        return project_root_;
    }

    const SourceLimits& ProjectTileMapSourceStore::limits() const noexcept
    {
        return limits_;
    }

    SourceMetrics ProjectTileMapSourceStore::metrics() const noexcept
    {
        return metrics_;
    }

    SourceLocation ProjectTileMapSourceStore::save(
        std::string_view logicalPath,
        const authoring::tilemap::Document& document) noexcept
    {
        ++metrics_.save_requests;
        if (!valid())
            return reject_location(SourceCode::invalid_store);
        if (!document.valid())
            return reject_location(SourceCode::malformed_source);

        try
        {
            SourceCode pathFailure{};
            const auto source = canonical_source(
                project_root_, logicalPath, limits_, pathFailure);
            if (!source)
                return reject_location(pathFailure);

            const auto serialized = document.serialize();
            if (!serialized)
                return reject_location(SourceCode::malformed_source);
            if (serialized.bytes.size() > limits_.maximum_source_bytes)
                return reject_location(SourceCode::source_too_large);

            std::error_code error{};
            fs::create_directories(
                source->storage_path.parent_path(), error);
            if (error
                || !fs::is_directory(
                    source->storage_path.parent_path(), error)
                || error)
            {
                return reject_location(SourceCode::directory_failure);
            }

            const ReadBytesResult existing = read_bytes(
                source->storage_path, limits_.maximum_source_bytes);
            if (existing.code == SourceCode::ready)
            {
                metrics_.bytes_read += existing.bytes.size();
                if (existing.bytes == serialized.bytes)
                {
                    ++metrics_.unchanged_saves;
                    return {
                        SourceCode::unchanged,
                        source->logical_path,
                        source->storage_path,
                        document.revision(),
                        serialized.bytes.size()};
                }
            }
            else if (existing.code != SourceCode::not_found)
            {
                return reject_location(existing.code);
            }

            fs::path temporary = source->storage_path;
            temporary += ".pending." + decimal_u64(next_temporary_id());
            const SourceCode written = write_bytes(
                temporary, serialized.bytes);
            if (written != SourceCode::ready)
            {
                fs::remove(temporary, error);
                return reject_location(written);
            }

            ReadBytesResult verified = read_bytes(
                temporary, limits_.maximum_source_bytes);
            metrics_.bytes_read += verified.bytes.size();
            if (verified.code != SourceCode::ready
                || verified.bytes != serialized.bytes)
            {
                fs::remove(temporary, error);
                return reject_location(SourceCode::integrity_failure);
            }
            const auto restored = authoring::tilemap::Document::deserialize(
                verified.bytes, limits_.document);
            if (!restored || restored.document->revision()
                != document.revision())
            {
                fs::remove(temporary, error);
                return reject_location(SourceCode::integrity_failure);
            }

            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, source->storage_path, error))
            {
                fs::remove(temporary, error);
                return reject_location(
                    SourceCode::atomic_replace_failure);
            }

            ++metrics_.saves;
            metrics_.bytes_written += serialized.bytes.size();
            return {
                SourceCode::ready,
                source->logical_path,
                source->storage_path,
                document.revision(),
                serialized.bytes.size()};
        }
        catch (...)
        {
            return reject_location(SourceCode::allocation_failure);
        }
    }

    LoadedSource ProjectTileMapSourceStore::load(
        std::string_view logicalPath) noexcept
    {
        ++metrics_.load_requests;
        if (!valid())
            return reject_load(SourceCode::invalid_store);
        try
        {
            SourceCode pathFailure{};
            const auto source = canonical_source(
                project_root_, logicalPath, limits_, pathFailure);
            if (!source)
                return reject_load(pathFailure);

            ReadBytesResult read = read_bytes(
                source->storage_path, limits_.maximum_source_bytes);
            if (read.code != SourceCode::ready)
                return reject_load(read.code);
            metrics_.bytes_read += read.bytes.size();
            const std::uint64_t serializedBytes = read.bytes.size();
            auto restored = authoring::tilemap::Document::deserialize(
                read.bytes, limits_.document);
            if (!restored)
                return reject_load(SourceCode::malformed_source);

            SourceLocation location{
                SourceCode::ready,
                source->logical_path,
                source->storage_path,
                restored.document->revision(),
                serializedBytes};
            ++metrics_.loads;
            return {
                SourceCode::ready,
                std::move(location),
                std::move(restored.document)};
        }
        catch (...)
        {
            return reject_load(SourceCode::allocation_failure);
        }
    }

    SourceLocation ProjectTileMapSourceStore::reject_location(
        SourceCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedSource ProjectTileMapSourceStore::reject_load(
        SourceCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    SourceContractFailure
        project_tilemap_source_contract_failure() noexcept
    {
        ContractRoot root{};
        if (!root.valid())
            return SourceContractFailure::temporary_root;

        ProjectTileMapSourceStore store{
            "epoch.tilemap-source.contract", root.path};
        if (!store.valid())
            return SourceContractFailure::construction;

        auto created = authoring::tilemap::Document::create(
            {0u, 1u},
            authoring::tilemap::MapDescriptor{
                .name = "Contract Map",
                .extent_tiles = {8u, 8u},
                .default_chunk_extent = {4u, 4u}});
        if (!created)
            return SourceContractFailure::construction;
        std::unique_ptr<authoring::tilemap::Document> document =
            std::move(created.document);
        const auto layer = document->create_layer(
            authoring::tilemap::LayerDescriptor{
                .name = "Ground",
                .extent_tiles = {8u, 8u},
                .chunk_extent = {4u, 4u}});
        if (!layer)
            return SourceContractFailure::construction;

        constexpr std::string_view logical{
            "Assets/Maps/contract.epochmap"};
        if (store.save("../contract.epochmap", *document).code
            != SourceCode::invalid_path)
        {
            return SourceContractFailure::invalid_path;
        }
        if (store.save("Assets/Textures/contract.epochmap", *document).code
            != SourceCode::outside_maps)
        {
            return SourceContractFailure::outside_maps;
        }

        const SourceLocation first = store.save(logical, *document);
        if (!first || first.code != SourceCode::ready)
            return SourceContractFailure::first_save;
        const SourceLocation unchanged = store.save(logical, *document);
        if (!unchanged || unchanged.code != SourceCode::unchanged)
            return SourceContractFailure::unchanged_save;

        LoadedSource loaded = store.load(logical);
        if (!loaded || loaded.document->snapshot().layers.size() != 1u
            || loaded.location.revision != document->revision())
        {
            return SourceContractFailure::first_load;
        }

        const auto object = document->create_object(
            authoring::tilemap::MapObjectDescriptor{
                .name = "Spawn",
                .type = "spawn",
                .position = {2.0f, 3.0f}});
        if (!object)
            return SourceContractFailure::overwrite;
        const SourceLocation overwritten = store.save(logical, *document);
        if (!overwritten || overwritten.code != SourceCode::ready
            || overwritten.revision == first.revision)
        {
            return SourceContractFailure::overwrite;
        }
        loaded = store.load(logical);
        if (!loaded || loaded.document->snapshot().objects.size() != 1u
            || loaded.location.revision != document->revision())
        {
            return SourceContractFailure::overwritten_load;
        }

        const fs::path malformed = root.path
            / "Assets" / "Maps" / "malformed.epochmap";
        std::error_code error{};
        fs::create_directories(malformed.parent_path(), error);
        const std::vector<std::byte> bad{
            std::byte{0x45}, std::byte{0x50}, std::byte{0x4f}};
        if (error || write_bytes(malformed, bad) != SourceCode::ready
            || store.load("Assets/Maps/malformed.epochmap").code
                != SourceCode::malformed_source)
        {
            return SourceContractFailure::malformed_source;
        }
        if (store.load("Assets/Maps/missing.epochmap").code
            != SourceCode::not_found)
        {
            return SourceContractFailure::missing_source;
        }

        const SourceMetrics metrics = store.metrics();
        if (metrics.save_requests != 5u || metrics.saves != 2u
            || metrics.unchanged_saves != 1u || metrics.load_requests != 4u
            || metrics.loads != 2u || metrics.rejected_operations != 4u
            || metrics.bytes_written == 0u || metrics.bytes_read == 0u)
        {
            return SourceContractFailure::metrics;
        }
        return SourceContractFailure::none;
    }
}
