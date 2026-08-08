/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module project.contracts;

import project.asset.registry;
import project.texture.resources;

namespace epochengine::project_contracts
{
    ContractResult run_texture_spine_contract() noexcept
    {
        const auto registry =
            project_assets::project_asset_registry_runtime_contract_failure();
        if (registry != project_assets::RegistryContractFailure::none)
            return {false, project_assets::registry_contract_failure_name(registry)};

        const auto textures =
            project_textures::project_texture_resource_runtime_contract_failure();
        if (textures != project_textures::TextureResourceContractFailure::none)
        {
            return {
                false,
                project_textures::texture_resource_contract_failure_name(textures)};
        }
        return {true, "pass"};
    }
}
