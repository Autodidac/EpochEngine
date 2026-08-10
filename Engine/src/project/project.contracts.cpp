/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module project.contracts;

import project.asset_registry;
import project.lifecycle;
import project.texture_admission;
import project.texture_resources;
import project.texture_pipeline;

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
        const auto pipeline =
            project_textures::project_texture_pipeline_runtime_contract_failure();
        if (pipeline != project_textures::TexturePipelineContractFailure::none)
        {
            return {
                false,
                project_textures::texture_pipeline_contract_failure_name(
                    pipeline)};
        }
        const auto admission =
            project_textures::project_texture_admission_contract_failure();
        if (admission != project_textures::TextureAdmissionContractFailure::none)
        {
            return {
                false,
                project_textures::texture_admission_contract_failure_name(
                    admission)};
        }
        const auto lifecycle =
            project_lifecycle::runtime_contract_failure();
        if (lifecycle != project_lifecycle::ContractFailure::none)
        {
            return {
                false,
                project_lifecycle::contract_failure_name(lifecycle)};
        }
        return {true, "pass"};
    }
}
