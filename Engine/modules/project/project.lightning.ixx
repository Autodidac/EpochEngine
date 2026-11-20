// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

export module project.lightning;

import asset.mesh_artifact;
import authoring.morphology;
import forest.preview_mesh;

export namespace epochengine::project_lightning
{
    inline constexpr std::string_view scene_type = "Lightning";
    inline constexpr double repeat_seconds = 3.0;
    inline constexpr double initial_phase_seconds = 0.09;
    inline constexpr std::uint32_t maximum_segments = 31u;
    inline constexpr asset::mesh::Limits mesh_limits{
        .maximum_vertices = 2'048u, .maximum_indices = 6'144u, .maximum_bytes = 128u * 1'024u};

    struct Projection final
    {
        std::shared_ptr<const asset::mesh::Artifact> mesh{};
        float intensity{};

        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(mesh); }
        [[nodiscard]] std::array<float, 3> color() const noexcept
        {
            return {0.67f * intensity, 0.82f * intensity, intensity};
        }
    };

    class PreviewCache final
    {
    public:
        [[nodiscard]] Projection sample(std::uint64_t seed, double presentation_seconds)
        {
            if (seed == 0u || !std::isfinite(presentation_seconds) || presentation_seconds < 0.0)
                return {};
            constexpr authoring::morphology::DischargeTiming timing{};
            const auto phase = std::fmod(std::fmod(presentation_seconds, repeat_seconds)
                + initial_phase_seconds, repeat_seconds);
            if (phase <= 0.0 || phase >= timing.propagation_seconds + timing.hold_seconds + timing.decay_seconds)
                return {};

            auto& slot = slots_[seed % slots_.size()];
            if (slot.seed != seed)
            {
                slot = {};
                slot.seed = seed;
                auto recipe = authoring::morphology::default_recipe(authoring::morphology::Domain::electrical);
                recipe.growth.seed = seed;
                recipe.growth.generations = 4u;
                recipe.growth.generation_delay_seconds = recipe.growth.segment_growth_seconds;
                recipe.duration_seconds = recipe.growth.segment_growth_seconds
                    * static_cast<float>(recipe.growth.generations + 1u);
                recipe.limits.maximum_nodes = maximum_segments + 1u;
                recipe.limits.maximum_segments = maximum_segments;
                recipe.limits.maximum_terminals = maximum_segments;
                slot.graph = authoring::morphology::build_graph(recipe);
            }
            if (slot.phase == phase) return slot.projection;

            slot.phase = phase;
            const auto discharge = authoring::morphology::sample_discharge(slot.graph, phase, timing);
            if (!discharge.valid || discharge.geometry.segments.empty())
            {
                slot.projection = {};
                return {};
            }

            // Decay changes only the tint. Reuse the same bounded geometry while
            // the pulse fades and while a paused timeline repeats its sample.
            const auto shape = (std::min)(phase, timing.propagation_seconds);
            if (slot.shape != shape || !slot.projection.mesh)
            {
                slot.shape = shape;
                slot.projection.mesh = build_mesh(slot.graph, discharge.geometry);
            }
            slot.projection.intensity = slot.projection.mesh ? discharge.intensity : 0.0f;
            return slot.projection;
        }

    private:
        using Graph = authoring::morphology::Graph;
        using Sample = authoring::morphology::Sample;
        struct Slot final
        {
            std::uint64_t seed{};
            double phase{-1.0};
            double shape{-1.0};
            Graph graph{};
            Projection projection{};
        };
        std::array<Slot, 16u> slots_{};

        [[nodiscard]] static std::shared_ptr<const asset::mesh::Artifact> build_mesh(
            const Graph& graph, const Sample& sample)
        {
            const auto mesh = forest::preview_mesh::build(graph, sample);
            if (!mesh.valid || mesh.leafCardCount != 0u || mesh.vertices.size() > mesh_limits.maximum_vertices
                || mesh.indices.size() > mesh_limits.maximum_indices)
                return {};
            std::vector<asset::mesh::Vertex> vertices;
            vertices.reserve(mesh.vertices.size());
            for (const auto& vertex : mesh.vertices)
                vertices.push_back({.position = {vertex.position.x, vertex.position.y, vertex.position.z},
                    .normal = {vertex.normal.x, vertex.normal.y, vertex.normal.z},
                    .color = {1.0f, 1.0f, 1.0f}});
            return asset::mesh::build_artifact(vertices, mesh.indices, mesh_limits).artifact;
        }
    };
}
