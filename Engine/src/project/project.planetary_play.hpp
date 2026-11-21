// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

// Include after the existing project.planetary, project.planetary_runtime and
// core.timer imports. The host owns queues and publication; the worker owns
// typed preparation and source rechecks.
namespace epochengine::project_planetary_play
{
    struct PlanetaryWaterResult final
    {
        std::shared_ptr<const project_planetary_snapshot::Snapshot> source{};
        std::shared_ptr<const asset::mesh::Artifact> mesh{};
        std::shared_ptr<const asset::mesh::SphericalWaterAppearance> appearance{};
        std::shared_ptr<const water::wave::Tile> linked_sample{};
        std::uint32_t body{};
        std::uint64_t clock_generation{};
        double seconds{};
        double worker_seconds{};
        bool source_current{},cancelled{},reaches_live_clock{};
    };
    struct InitialPlanetaryWaterCut final
    {
        project_planetary::StreamResult stream;
        std::shared_ptr<water::wave::Provider> provider;
        std::string water_source_hash;
        std::shared_ptr<const project_planetary_snapshot::Snapshot> previous;
        std::shared_ptr<const project_planetary_snapshot::WaterLatticeProvider> lattice_provider;
    };

    [[nodiscard]] PlanetaryWaterResult sample_planetary_water(
        const std::filesystem::path& root,const project_planetary::SourceGeneration& identity,
        std::shared_ptr<const project_planetary_snapshot::Snapshot> source,
        const std::shared_ptr<water::wave::Provider>& provider,
        const project_planetary::Settings& settings,double seconds,
        std::string_view water_source_hash,std::uint64_t clock_generation,
        std::uint64_t retained_display_bytes,std::stop_token stop,
        std::shared_ptr<const water::wave::Tile>* retained_tile=nullptr,
        const project_planetary::PackagedShoreline* packaged=nullptr,
        const project_planetary::PackagedPlanetaryPose* linked_pose=nullptr);

    [[nodiscard]] project_planetary::StreamResult prepare_initial_planetary_water_cut(
        const std::filesystem::path& root,const project_planetary::Settings& settings,
        InitialPlanetaryWaterCut cut,std::uint64_t clock_generation,
        std::uint64_t retained_display_bytes,std::stop_token stop);

    [[nodiscard]] std::uint64_t planetary_cut_mesh_bytes(
        const std::shared_ptr<const asset::mesh::Artifact>& mesh,
        const std::shared_ptr<const asset::mesh::SphericalWaterAppearance>& appearance) noexcept;

    [[nodiscard]] project_planetary::StreamResult continue_planetary_wet_cut(
        const std::filesystem::path& root,const project_planetary::Settings& settings,
        project_planetary::StreamResult stream,std::uint64_t retained_display_bytes,std::stop_token stop,
        std::optional<timing::Clock::time_point> queued_at);

    project_planetary::StreamResult linked_terrain_step(
        project_planetary_runtime::TerrainProvider& provider,
        const project_planetary_runtime::TerrainView& view,std::stop_token stop);

    [[nodiscard]] bool planetary_view_changed(const project_planetary::StreamView& current,
        const project_planetary::StreamView& previous) noexcept;
}
