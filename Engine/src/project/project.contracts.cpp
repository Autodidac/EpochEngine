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
import project.tilemap_pipeline;
import project.tilemap_runtime;
import project.input_profile;
import project.actor2d_runtime;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import project.tilemap_source;
#endif

namespace epochengine::project_contracts
{
    ContractResult run_asset_spine_contract() noexcept
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
        const auto tilemaps =
            project_tilemaps::project_tilemap_pipeline_runtime_contract_failure();
        if (tilemaps != project_tilemaps::TileMapPipelineContractFailure::none)
        {
            return {
                false,
                project_tilemaps::tilemap_pipeline_contract_failure_name(
                    tilemaps)};
        }
        const auto tilemapRuntime =
            project_tilemap_runtime::run_contract();
        if (tilemapRuntime != project_tilemap_runtime::ContractFailure::none)
        {
            return {
                false,
                project_tilemap_runtime::contract_failure_name(
                    tilemapRuntime)};
        }        const auto inputProfile =
            project_input::project_input_profile_contract_failure();
        if (inputProfile != project_input::ContractFailure::none)
        {
            return {
                false,
                project_input::contract_failure_name(inputProfile)};
        }
        const auto actor2d = project_actor2d::run_contract();
        if (actor2d != project_actor2d::ContractFailure::none)
        {
            return {
                false,
                project_actor2d::contract_failure_name(actor2d)};
        }
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        const auto tilemapSources =
            project_tilemap_sources::project_tilemap_source_contract_failure();
        if (tilemapSources
            != project_tilemap_sources::SourceContractFailure::none)
        {
            return {
                false,
                project_tilemap_sources::source_contract_failure_name(
                    tilemapSources)};
        }
#endif

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
