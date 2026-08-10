/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

module project.texture_pipeline;

import render.canvas2d_cpu;

namespace epochengine::project_textures
{
    namespace
    {
        [[nodiscard]] project_assets::AssetRevision asset_revision(
            const asset::texture::DocumentRevision& revision) noexcept
        {
            return {{revision.content.words}, revision.sequence};
        }

        [[nodiscard]] bool accepted(project_assets::RegistryCode code) noexcept
        {
            return code == project_assets::RegistryCode::ready
                || code == project_assets::RegistryCode::already_registered
                || code == project_assets::RegistryCode::unchanged;
        }

        [[nodiscard]] bool accepted(ResourceCode code) noexcept
        {
            return code == ResourceCode::ready
                || code == ResourceCode::already_published;
        }

        class ContractRoot final
        {
        public:
            ContractRoot() noexcept
            {
                try
                {
                    static std::atomic<std::uint64_t> next{1};
                    std::error_code error{};
                    const std::filesystem::path base =
                        std::filesystem::temp_directory_path(error);
                    if (error || base.empty())
                        return;
                    for (std::uint32_t attempt = 0; attempt < 32; ++attempt)
                    {
                        const std::uint64_t identity = next.fetch_add(
                            1, std::memory_order_relaxed);
                        std::filesystem::path candidate = base
                            / ("epoch_texture_pipeline_contract_"
                                + std::to_string(identity));
                        if (std::filesystem::create_directory(candidate, error))
                        {
                            root_ = std::move(candidate);
                            return;
                        }
                        if (error)
                            error.clear();
                    }
                }
                catch (...)
                {
                }
            }

            ~ContractRoot()
            {
                if (root_.empty())
                    return;
                std::error_code error{};
                std::filesystem::remove_all(root_, error);
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return !root_.empty();
            }

            [[nodiscard]] std::string string() const
            {
                return root_.generic_string();
            }

        private:
            std::filesystem::path root_{};
        };
    }

    ProjectTexturePipeline::ProjectTexturePipeline(
        std::string projectId,
        std::string projectRoot,
        TexturePipelineLimits limits) noexcept
        : limits_(limits),
          registry_(projectId, limits.registry),
          library_(projectId, std::move(projectRoot), limits.library),
          resources_(registry_, limits.resources)
    {
    }

    bool ProjectTexturePipeline::valid() const noexcept
    {
        return limits_.valid() && registry_.valid() && library_.valid()
            && resources_.valid()
            && registry_.project_key() == library_.project_key();
    }

    std::string_view ProjectTexturePipeline::project_id() const noexcept
    {
        return registry_.project_id();
    }

    std::string_view ProjectTexturePipeline::project_root() const noexcept
    {
        return library_.project_root();
    }

    std::uint64_t ProjectTexturePipeline::project_key() const noexcept
    {
        return valid() ? registry_.project_key() : 0;
    }

    const TexturePipelineLimits& ProjectTexturePipeline::limits() const noexcept
    {
        return limits_;
    }

    TexturePipelineMetrics ProjectTexturePipeline::metrics() const noexcept
    {
        return metrics_;
    }

    const project_assets::AssetRegistry&
        ProjectTexturePipeline::registry() const noexcept
    {
        return registry_;
    }

    const TextureArtifactLibrary& ProjectTexturePipeline::library() const noexcept
    {
        return library_;
    }

    const TextureResourceService& ProjectTexturePipeline::resources() const noexcept
    {
        return resources_;
    }

    TexturePipelineResult ProjectTexturePipeline::publish(
        std::string_view logicalPath,
        const asset::texture::CompiledTextureArtifact& artifact) noexcept
    {
        ++metrics_.publication_requests;
        if (!valid())
            return reject(PipelineCode::invalid_pipeline);
        if (logicalPath.empty())
            return reject(PipelineCode::invalid_path);
        if (!asset::texture::validate_compiled_artifact(artifact))
            return reject(PipelineCode::invalid_artifact);

        std::optional<std::string> canonical{};
        try
        {
            canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
        }
        catch (...)
        {
            return reject(PipelineCode::allocation_failure);
        }
        if (!canonical)
            return reject(PipelineCode::invalid_path);
        const project_assets::AssetRecord* const existing =
            registry_.find_by_path(*canonical);
        if (existing && existing->canonical_path != *canonical)
        {
            return reject(
                PipelineCode::registry_failure,
                LibraryCode::invalid_path,
                project_assets::RegistryCode::path_collision);
        }

        TextureArtifactLocator locator = library_.persist(*canonical, artifact);
        if (!locator)
        {
            return reject(
                PipelineCode::library_failure,
                locator.code);
        }
        const bool libraryChanged = locator.code == LibraryCode::ready;
        return activate(
            *canonical, artifact, std::move(locator), libraryChanged);
    }

    TexturePipelineResult ProjectTexturePipeline::restore_latest(
        std::string_view logicalPath,
        const asset::texture::TextureCompileProfile& profile) noexcept
    {
        ++metrics_.restore_requests;
        if (!valid())
            return reject(PipelineCode::invalid_pipeline);
        if (logicalPath.empty())
            return reject(PipelineCode::invalid_path);

        std::optional<std::string> canonical{};
        try
        {
            canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
        }
        catch (...)
        {
            return reject(PipelineCode::allocation_failure);
        }
        if (!canonical)
            return reject(PipelineCode::invalid_path);
        const project_assets::AssetRecord* const existing =
            registry_.find_by_path(*canonical);
        if (existing && existing->canonical_path != *canonical)
        {
            return reject(
                PipelineCode::registry_failure,
                LibraryCode::invalid_path,
                project_assets::RegistryCode::path_collision);
        }

        LoadedTextureArtifact loaded = library_.load_latest(*canonical, profile);
        if (!loaded)
        {
            return reject(
                PipelineCode::library_failure,
                loaded.code);
        }
        TexturePipelineResult restored = activate(
            *canonical,
            loaded.artifact,
            std::move(loaded.locator),
            false);
        if (restored)
            ++metrics_.restorations;
        return restored;
    }

    TexturePipelineResult ProjectTexturePipeline::restore_exact(
        std::string_view logicalPath,
        const asset::texture::ContentHash& artifactKey) noexcept
    {
        ++metrics_.restore_requests;
        if (!valid())
            return reject(PipelineCode::invalid_pipeline);
        if (logicalPath.empty() || artifactKey.empty())
            return reject(PipelineCode::invalid_path);

        std::optional<std::string> canonical{};
        try
        {
            canonical = project_assets::canonical_logical_path(
                logicalPath,
                limits_.registry);
        }
        catch (...)
        {
            return reject(PipelineCode::allocation_failure);
        }
        if (!canonical)
            return reject(PipelineCode::invalid_path);

        LoadedTextureArtifact loaded =
            library_.load_exact(*canonical, artifactKey);
        if (!loaded)
        {
            return reject(
                PipelineCode::library_failure,
                loaded.code);
        }

        const project_assets::AssetRevision artifactRevision =
            asset_revision(loaded.artifact.identity.source_revision);
        const project_assets::AssetRecord* record =
            registry_.find_by_path(*canonical);
        project_assets::AssetHandle handle{};
        project_assets::RegistryCode registryCode{
            project_assets::RegistryCode::unchanged};
        if (!record)
        {
            const project_assets::RegistrationResult registered =
                registry_.register_asset({
                    *canonical,
                    project_assets::AssetKind::texture,
                    artifactRevision});
            registryCode = registered.code;
            handle = registered.handle;
            if (!registered)
            {
                return reject(
                    PipelineCode::registry_failure,
                    loaded.code,
                    registryCode);
            }
        }
        else
        {
            if (record->identity.kind
                != project_assets::AssetKind::texture)
            {
                return reject(
                    PipelineCode::registry_failure,
                    loaded.code,
                    project_assets::RegistryCode::path_collision);
            }
            handle = record->handle;
        }

        const PublicationResult published = resources_.publish(
            registry_,
            handle,
            loaded.artifact,
            false);
        if (!published || !accepted(published.code))
        {
            return reject(
                PipelineCode::resource_failure,
                loaded.code,
                registryCode,
                published.code);
        }

        ++metrics_.restorations;
        const bool changed =
            registryCode == project_assets::RegistryCode::ready
            || published.code == ResourceCode::ready;
        return {
            changed ? PipelineCode::ready : PipelineCode::unchanged,
            loaded.code,
            registryCode,
            published.code,
            handle,
            published.logical,
            std::move(loaded.locator)};
    }

    Canvas2DLeaseResult ProjectTexturePipeline::bind_canvas2d(
        std::span<const canvas2d::LogicalTextureReference> logicalTextures) noexcept
    {
        ++metrics_.lease_requests;
        Canvas2DLeaseResult result{};
        if (!valid())
        {
            ++metrics_.rejected_operations;
            return result;
        }

        try
        {
            auto owned = std::make_shared<Canvas2DResourceSet>(
                resources_.bind_canvas2d(registry_, logicalTextures));
            result.resource_code = owned->code();
            if (!owned->valid())
            {
                result.code = PipelineCode::resource_failure;
                ++metrics_.rejected_operations;
                return result;
            }

            result.decoded_bytes = owned->decoded_bytes();
            result.texture_count = static_cast<std::uint32_t>(
                owned->texture_count());
            result.lease.bindings = owned->bindings();
            std::shared_ptr<const Canvas2DResourceSet> retained = std::move(owned);
            result.lease.owner =
                canvas2d::scene_content::ResourceLifetime::retain(
                    std::move(retained));
            if (!result.lease.valid())
            {
                result = {};
                result.code = PipelineCode::resource_failure;
                result.resource_code = ResourceCode::allocation_failure;
                ++metrics_.rejected_operations;
                return result;
            }
            result.code = PipelineCode::ready;
            ++metrics_.leases;
            return result;
        }
        catch (...)
        {
            result.code = PipelineCode::allocation_failure;
            result.resource_code = ResourceCode::allocation_failure;
            ++metrics_.rejected_operations;
            return result;
        }
    }

    TexturePipelineResult ProjectTexturePipeline::activate(
        std::string_view logicalPath,
        const asset::texture::CompiledTextureArtifact& artifact,
        TextureArtifactLocator locator,
        bool libraryChanged) noexcept
    {
        const project_assets::AssetRevision revision = asset_revision(
            artifact.identity.source_revision);
        const project_assets::AssetRecord* record =
            registry_.find_by_path(logicalPath);
        project_assets::AssetHandle handle{};
        project_assets::RegistryCode registryCode{
            project_assets::RegistryCode::ready};

        if (!record)
        {
            const project_assets::RegistrationResult registered =
                registry_.register_asset({
                    logicalPath,
                    project_assets::AssetKind::texture,
                    revision});
            registryCode = registered.code;
            handle = registered.handle;
            if (!registered)
            {
                return reject(
                    PipelineCode::registry_failure,
                    locator.code,
                    registryCode);
            }
        }
        else
        {
            if (record->identity.kind != project_assets::AssetKind::texture)
            {
                return reject(
                    PipelineCode::registry_failure,
                    locator.code,
                    project_assets::RegistryCode::path_collision);
            }
            handle = record->handle;
            registryCode = record->revision == revision
                ? project_assets::RegistryCode::unchanged
                : registry_.update_revision(handle, revision);
            if (!accepted(registryCode))
            {
                return reject(
                    PipelineCode::registry_failure,
                    locator.code,
                    registryCode);
            }
        }

        const PublicationResult published = resources_.publish(
            registry_, handle, artifact);
        if (!published || !accepted(published.code))
        {
            return reject(
                PipelineCode::resource_failure,
                locator.code,
                registryCode,
                published.code);
        }

        const bool changed = libraryChanged
            || registryCode == project_assets::RegistryCode::ready
            || published.code == ResourceCode::ready;
        if (changed)
            ++metrics_.publications;
        else
            ++metrics_.unchanged_publications;
        return {
            changed ? PipelineCode::ready : PipelineCode::unchanged,
            locator.code,
            registryCode,
            published.code,
            handle,
            published.logical,
            std::move(locator)};
    }

    TexturePipelineResult ProjectTexturePipeline::reject(
        PipelineCode code,
        LibraryCode libraryCode,
        project_assets::RegistryCode registryCode,
        ResourceCode resourceCode) noexcept
    {
        ++metrics_.rejected_operations;
        return {code, libraryCode, registryCode, resourceCode};
    }

    TexturePipelineContractFailure
        project_texture_pipeline_runtime_contract_failure() noexcept
    {
        using namespace asset::texture;
        ContractRoot root{};
        if (!root.valid())
            return TexturePipelineContractFailure::temporary_root;

        const std::string projectRoot = root.string();
        ProjectTexturePipeline pipeline{
            "epoch.project-texture-pipeline.contract", projectRoot};
        if (!pipeline.valid())
            return TexturePipelineContractFailure::construction;

        const DocumentRevision firstRevision{{{11, 22, 33, 44}}, 7};
        const CompiledTextureArtifact first = detail::contract_artifact(
            firstRevision);
        CompiledTextureArtifact invalid = first;
        invalid.payload_content.words[0] ^= 1;
        if (pipeline.publish("Assets/Textures/checker.rgba", invalid).code
            != PipelineCode::invalid_artifact)
        {
            return TexturePipelineContractFailure::invalid_rejection;
        }

        const TexturePipelineResult published = pipeline.publish(
            "Assets\\Textures//checker.rgba", first);
        if (!published || published.code != PipelineCode::ready
            || published.locator.canonical_logical_path
                != "Assets/Textures/checker.rgba")
        {
            return TexturePipelineContractFailure::first_publication;
        }
        const TexturePipelineResult repeated = pipeline.publish(
            "Assets/Textures/checker.rgba", first);
        if (!repeated || repeated.code != PipelineCode::unchanged
            || repeated.logical != published.logical
            || repeated.locator.storage_path != published.locator.storage_path)
        {
            return TexturePipelineContractFailure::idempotent_publication;
        }

        TextureArtifactLibrary verifier{
            "epoch.project-texture-pipeline.contract", projectRoot};
        if (pipeline.publish(
                "assets/textures/checker.rgba", first).code
            != PipelineCode::registry_failure)
        {
            return TexturePipelineContractFailure::portable_path_collision;
        }

        const LoadedTextureArtifact exact = verifier.load_exact(
            "Assets/Textures/checker.rgba", first.identity.key);
        if (!exact
            || exact.artifact.payload_content.words
                != first.payload_content.words)
            return TexturePipelineContractFailure::exact_load;

        ProjectTexturePipeline restored{
            "epoch.project-texture-pipeline.contract", projectRoot};
        const TexturePipelineResult restoration = restored.restore_latest(
            "Assets/Textures/checker.rgba", first.identity.profile);
        if (!restoration || restoration.logical != published.logical)
            return TexturePipelineContractFailure::restoration;

        ProjectTexturePipeline foreign{
            "epoch.project-texture-pipeline.foreign", projectRoot};
        if (foreign.restore_latest(
                "Assets/Textures/checker.rgba", first.identity.profile).code
            != PipelineCode::library_failure)
        {
            return TexturePipelineContractFailure::project_isolation;
        }

        const std::array logical{restoration.logical};
        Canvas2DLeaseResult leased = restored.bind_canvas2d(logical);
        if (!leased || leased.texture_count != 1 || leased.decoded_bytes != 16)
            return TexturePipelineContractFailure::lease;

        canvas2d::scene_content::SceneContent content{};
        content.project.logical_canvas = {8, 8};
        content.project.pixels_per_world_unit = 1.0f;
        content.camera.pixels_per_world_unit = 1.0f;
        content.source_revision = firstRevision.sequence;
        canvas2d::SpriteSubmission sprite{};
        sprite.sprite = {1, 1};
        sprite.material.stable_key = static_cast<std::uint32_t>(
            restoration.logical.artifact_revision
                ^ (restoration.logical.artifact_revision >> 32u));
        if (sprite.material.stable_key == 0)
            sprite.material.stable_key = 1;
        sprite.material.source = canvas2d::SpriteSourceKind::texture;
        sprite.material.logical_texture = restoration.logical;
        sprite.material.alpha = canvas2d::SpriteAlphaMode::opaque;
        sprite.material.color_space = canvas2d::SpriteColorSpace::linear;
        sprite.transform.size = {2.0f, 2.0f};
        sprite.stable_sequence = 1;
        content.sprites.push_back(sprite);
        content.resources = std::move(leased.lease);
        if (!content.valid()
            || canvas2d::scene_content::validate_resource_closure(
                content.sprites, content.resources)
                != canvas2d::scene_content::ResourceClosureCode::ready)
        {
            return TexturePipelineContractFailure::resource_closure;
        }

        int owner{};
        const auto publication = canvas2d::scene_content::publish(
            &owner, std::move(content));
        const auto acquired = canvas2d::scene_content::acquire(&owner);
        if (!publication || !acquired)
        {
            (void)canvas2d::scene_content::retire(&owner);
            return TexturePipelineContractFailure::scene_publication;
        }
        const auto frame = canvas2d::scene_content::compile(acquired, {8, 8});
        const auto raster = canvas2d::cpu::rasterize(
            frame, acquired.content->resources.bindings);
        (void)canvas2d::scene_content::retire(&owner);
        if (!frame || !raster)
            return TexturePipelineContractFailure::cpu_raster;

        const DocumentRevision nextRevision{{{55, 66, 77, 88}}, 8};
        const CompiledTextureArtifact next = detail::contract_artifact(
            nextRevision);
        const TexturePipelineResult advanced = restored.publish(
            "Assets/Textures/checker.rgba", next);
        if (!advanced || advanced.code != PipelineCode::ready
            || advanced.logical == restoration.logical)
        {
            return TexturePipelineContractFailure::revision_advance;
        }
        if (restored.publish("Assets/Textures/checker.rgba", first).code
            != PipelineCode::registry_failure)
        {
            return TexturePipelineContractFailure::stale_revision;
        }

        const TexturePipelineMetrics metrics = restored.metrics();
        if (metrics.restore_requests != 1 || metrics.restorations != 1
            || metrics.publication_requests != 2 || metrics.publications != 2
            || metrics.lease_requests != 1 || metrics.leases != 1
            || metrics.rejected_operations == 0)
        {
            return TexturePipelineContractFailure::metrics;
        }
        return TexturePipelineContractFailure::none;
    }
}
