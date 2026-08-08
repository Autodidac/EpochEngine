/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>

module project.asset.registry;

namespace epochengine::project_assets
{
    [[nodiscard]] RegistryContractFailure
        project_asset_registry_runtime_contract_failure()
    {
        AssetRegistry invalid{""};
        if (invalid.valid())
            return RegistryContractFailure::invalid_project;

        AssetRegistry registry{"epoch.contract.project"};
        const AssetRevision firstRevision{{{1, 2, 3, 4}}, 1};
        if (registry.register_asset({"../escape.png", AssetKind::texture, firstRevision}).code
            != RegistryCode::invalid_path)
        {
            return RegistryContractFailure::traversal_rejection;
        }

        const RegistrationResult first = registry.register_asset(
            {"Assets\\Textures//hero.png", AssetKind::texture, firstRevision});
        if (!first)
            return RegistryContractFailure::registration;
        const AssetRecord* const firstRecord = registry.resolve(first.handle);
        if (!firstRecord || firstRecord->canonical_path != "Assets/Textures/hero.png"
            || firstRecord->identity.asset_key != first.asset_key)
        {
            return RegistryContractFailure::path_normalization;
        }
        const RegistrationResult duplicate = registry.register_asset(
            {"Assets/Textures/hero.png", AssetKind::texture, firstRevision});
        if (!duplicate || duplicate.code != RegistryCode::already_registered
            || duplicate.handle != first.handle)
        {
            return RegistryContractFailure::duplicate_identity;
        }
        if (registry.register_asset(
            {"assets/textures/HERO.png", AssetKind::texture, firstRevision}).code
            != RegistryCode::path_collision)
        {
            return RegistryContractFailure::case_collision;
        }

        const AssetRevision secondRevision{{{5, 6, 7, 8}}, 2};
        if (registry.update_revision(first.handle, secondRevision)
            != RegistryCode::ready)
        {
            return RegistryContractFailure::revision_update;
        }
        if (registry.update_revision(first.handle, firstRevision)
            != RegistryCode::stale_revision)
        {
            return RegistryContractFailure::stale_revision;
        }
        const AssetRevision equivalentRevision{secondRevision.content, 3};
        if (registry.update_revision(
            first.handle, equivalentRevision)
            != RegistryCode::ready)
        {
            return RegistryContractFailure::revision_update;
        }

        const std::uint64_t stableKey = first.asset_key;
        if (registry.retire(first.handle) != RegistryCode::ready)
            return RegistryContractFailure::retirement;
        if (registry.resolve(first.handle) != nullptr
            || registry.update_revision(first.handle, secondRevision)
                != RegistryCode::stale_handle)
        {
            return RegistryContractFailure::stale_handle;
        }
        const RegistrationResult restored = registry.register_asset(
            {"Assets/Textures/hero.png", AssetKind::texture, equivalentRevision});
        if (!restored || restored.asset_key != stableKey
            || restored.handle == first.handle)
        {
            return RegistryContractFailure::stable_reregistration;
        }

        const RegistryMetrics metrics = registry.metrics();
        if (metrics.active_assets != 1 || metrics.registrations != 2
            || metrics.idempotent_registrations != 1
            || metrics.revision_updates != 2 || metrics.retirements != 1
            || metrics.rejected_operations < 4 || metrics.path_bytes == 0)
        {
            return RegistryContractFailure::metrics;
        }
        return RegistryContractFailure::none;
    }
}
