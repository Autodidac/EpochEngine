/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>

module project.texture_resources;

namespace epochengine::project_textures
{
    [[nodiscard]] TextureResourceContractFailure
        project_texture_resource_runtime_contract_failure()
    {
        using namespace asset::texture;
        const DocumentRevision source{{{11, 22, 33, 44}}, 7};
        CompiledTextureArtifact artifact = detail::contract_artifact(source);
        if (!validate_compiled_artifact(artifact))
            return TextureResourceContractFailure::artifact;

        project_assets::AssetRegistry registry{"epoch.canvas2d.contract"};
        const project_assets::RegistrationResult registered =
            registry.register_asset({
                "Assets/Textures/checker.rgba",
                project_assets::AssetKind::texture,
                detail::asset_revision(source)});
        if (!registered)
            return TextureResourceContractFailure::registry;

        TextureResourceService service{};
        const PublicationResult published = service.publish(
            registry, registered.handle, artifact);
        if (!published)
            return TextureResourceContractFailure::publication;
        if (published.logical.artifact_revision == source.sequence
            || published.logical.artifact_revision
                != texture_artifact::compiled_artifact_revision(artifact.identity))
        {
            return TextureResourceContractFailure::content_revision;
        }
        const PublicationResult reused = service.publish(
            registry, registered.handle, artifact);
        if (!reused || reused.code != ResourceCode::already_published)
            return TextureResourceContractFailure::publication_reuse;

        const std::array<canvas2d::LogicalTextureReference, 1> requested{
            published.logical};
        Canvas2DResourceSet resources = service.bind_canvas2d(registry, requested);
        const canvas2d::cpu::ResourceBindings bindings = resources.bindings();
        if (!resources.valid() || resources.texture_count() != 1
            || bindings.textures.size() != 1
            || bindings.textures[0].logical != published.logical
            || bindings.textures[0].pixels.size() != 4
            || bindings.textures[0].pixels[0]
                != canvas2d::cpu::Rgba8{255, 0, 0, 255})
        {
            return TextureResourceContractFailure::canvas_binding;
        }
        const std::array<canvas2d::LogicalTextureReference, 2> duplicates{
            published.logical, published.logical};
        if (service.bind_canvas2d(registry, duplicates).code()
            != ResourceCode::duplicate_binding)
        {
            return TextureResourceContractFailure::duplicate_binding;
        }

        detail::ContractDevice device{};
        {
            texture_residency::TextureResidencyCache cache{device, 1};
            texture_artifact::ResidencySelection selection{};
            selection.frame_sequence = 1;
            const ResidencyResult resident = service.acquire_resident(
                registry, published.logical, cache, selection);
            if (!resident || device.uploaded().size() != 16)
                return TextureResourceContractFailure::residency;
        }
        {
            texture_residency::TextureResidencyCache recreated{device, 2};
            texture_artifact::ResidencySelection selection{};
            selection.frame_sequence = 2;
            const ResidencyResult resident = service.acquire_resident(
                registry, published.logical, recreated, selection);
            if (!resident || !resident.residency.handle)
            {
                return TextureResourceContractFailure::cache_recreation;
            }
        }

        const project_assets::AssetRevision nextRevision{{{55, 66, 77, 88}}, 8};
        if (registry.update_revision(registered.handle, nextRevision)
            != project_assets::RegistryCode::ready
            || service.bind_canvas2d(registry, requested).code()
                != ResourceCode::missing_publication)
        {
            return TextureResourceContractFailure::stale_registry_revision;
        }
        if (service.retire(registered.handle) != ResourceCode::ready
            || service.retire(registered.handle) != ResourceCode::missing_publication)
        {
            return TextureResourceContractFailure::retirement;
        }

        const TextureResourceMetrics metrics = service.metrics();
        if (metrics.publications != 1 || metrics.publication_reuse != 1
            || metrics.binding_requests < 3 || metrics.bound_textures != 1
            || metrics.residency_successes != 2 || metrics.decoded_bytes != 0
            || metrics.rejected_operations < 2)
        {
            return TextureResourceContractFailure::metrics;
        }
        return TextureResourceContractFailure::none;
    }
}
