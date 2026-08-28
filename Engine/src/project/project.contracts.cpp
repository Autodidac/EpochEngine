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
import project.ai_self_iteration;
import project.ai_self_iteration_session;
import project.input_controller;
import project.actor2d_runtime;
import project.sprite_animation;
import project.gameplay2d_runtime;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
import project.texture_source;
#endif
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
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
        const auto textureSources =
            project_texture_sources::
                project_texture_source_contract_failure();
        if (textureSources
            != project_texture_sources::SourceContractFailure::none)
        {
            return {
                false,
                project_texture_sources::source_contract_failure_name(
                    textureSources)};
        }
#endif
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
        }

        const auto inputProfile =
            project_input::project_input_profile_contract_failure();
        if (inputProfile != project_input::ContractFailure::none)
        {
            return {
                false,
                project_input::contract_failure_name(inputProfile)};
        }
        const auto selfIteration = project_ai_iteration::run_contract();
        if (selfIteration != project_ai_iteration::ContractFailure::none)
        {
            return {
                false,
                project_ai_iteration::contract_failure_name(selfIteration)};
        }
        const auto selfIterationSession =
            project_ai_iteration_session::run_contract();
        if (selfIterationSession
            != project_ai_iteration_session::ContractFailure::none)
        {
            return {
                false,
                project_ai_iteration_session::contract_failure_name(
                    selfIterationSession)};
        }
        const auto inputController = project_input_controller::run_contract();
        if (inputController
            != project_input_controller::ContractFailure::none)
        {
            return {
                false,
                project_input_controller::contract_failure_name(
                    inputController)};
        }
        const auto actor2d = project_actor2d::run_contract();
        if (actor2d != project_actor2d::ContractFailure::none)
        {
            return {
                false,
                project_actor2d::contract_failure_name(actor2d)};
        }
        const auto spriteAnimation =
            project_sprite_animation::
                project_sprite_animation_contract_failure();
        if (spriteAnimation
            != project_sprite_animation::ContractFailure::none)
        {
            return {
                false,
                project_sprite_animation::contract_failure_name(
                    spriteAnimation)};
        }
        const auto gameplay2d = project_gameplay2d::run_contract();
        if (gameplay2d != project_gameplay2d::ContractFailure::none)
        {
            return {
                false,
                project_gameplay2d::contract_failure_name(gameplay2d)};
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
