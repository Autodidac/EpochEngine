// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Optional terrain coverage; uses the reviewed planetary sampler and mesh owner.
module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stop_token>
#include <utility>
#include <vector>

export module planet.coverage;
import planet.terrain;

export namespace epoch::extensions::planetary_terrain::coverage
{
    namespace core = epoch::extensions::planetary_terrain::core;

    enum class Mode : std::uint8_t { classic, tiled, hemisphere, planet, universe };
    enum class Status : std::uint8_t { ready, invalid_request, capacity_exceeded, cancelled };

    struct Request final
    {
        Mode mode{Mode::classic};
        std::uint32_t seed{1337};
        std::size_t body_index{};
        std::uint32_t tiles_per_axis{4};
        std::uint32_t cells_per_tile{16};
        double tile_span_meters{128.0};
        double seconds{};
        bool ocean_surface{};
        std::size_t maximum_patches{128};
        std::uint64_t maximum_bytes{64u * 1024u * 1024u};
        core::WorldConfig world{};
    };

    struct Patch final
    {
        std::uint32_t body_id{};
        core::DVec3 body_center{};
        core::TileKey tile{};
        bool planar{};
        std::uint32_t grid_x{}, grid_y{};
        core::Mesh mesh{};
    };

    struct Result final
    {
        Status status{Status::invalid_request};
        std::uint64_t admitted_bytes{};
        std::vector<Patch> patches{};
        [[nodiscard]] explicit operator bool() const noexcept { return status == Status::ready; }
    };

    namespace detail
    {
        [[nodiscard]] core::DVec3 unit(core::DVec3 value) noexcept
        {
            return value / core::length(value);
        }

        // Global sample coordinates make neighboring planar grids agree exactly.
        // Sampling outside the edge for normals also preserves the shared seam.
        [[nodiscard]] core::Mesh flat_patch(const core::Body& body, const Request& request,
            std::uint32_t tile_x, std::uint32_t tile_y, std::uint32_t axis, std::stop_token stop)
        {
            const auto cells = request.cells_per_tile;
            const auto side = cells + 1u;
            const double spacing = request.tile_span_meters / cells;
            const double half_span = request.tile_span_meters * axis * 0.5;
            const auto coord = [&](std::int64_t sample) { return sample * spacing - half_span; };
            const auto surface = [&](double x, double z) {
                return core::sample_surface(body, unit({x / body.radius, 1.0, z / body.radius}));
            };
            core::Mesh mesh;
            mesh.anchor = {coord(static_cast<std::int64_t>(tile_x) * cells) + request.tile_span_meters * 0.5,
                0.0, coord(static_cast<std::int64_t>(tile_y) * cells) + request.tile_span_meters * 0.5};
            mesh.anchor.y = surface(mesh.anchor.x, mesh.anchor.z).height;
            mesh.vertices.reserve(static_cast<std::size_t>(side) * side);
            mesh.indices.reserve(static_cast<std::size_t>(cells) * cells * 6u);
            for (std::uint32_t y = 0; y <= cells; ++y)
            {
                if (stop.stop_requested()) return {};
                for (std::uint32_t x = 0; x <= cells; ++x)
                {
                    const double px = coord(static_cast<std::int64_t>(tile_x) * cells + x);
                    const double pz = coord(static_cast<std::int64_t>(tile_y) * cells + y);
                    const auto sample = surface(px, pz);
                    const core::DVec3 position{px, sample.height, pz};
                    const auto normal = unit({surface(px - spacing, pz).height - surface(px + spacing, pz).height,
                        2.0 * spacing, surface(px, pz - spacing).height - surface(px, pz + spacing).height});
                    mesh.vertices.push_back({core::FVec3{position - mesh.anchor}, core::FVec3{normal},
                        {sample.color.x, sample.color.y, sample.color.z, 0.0f}, {}});
                    mesh.bound_radius = (std::max)(mesh.bound_radius, core::length(position - mesh.anchor));
                }
            }
            for (std::uint32_t y = 0; y < cells; ++y)
                for (std::uint32_t x = 0; x < cells; ++x)
                {
                    const auto a = y * side + x, b = a + 1u, c = a + side, d = c + 1u;
                    mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
                }
            // Float local vertices can round outward from their double source.
            for (const auto& vertex : mesh.vertices)
                mesh.bound_radius = (std::max)(mesh.bound_radius, core::length(core::DVec3{vertex.position}));
            mesh.corners = {mesh.vertices[0].position, mesh.vertices[cells].position,
                mesh.vertices[static_cast<std::size_t>(side) * side - 1u].position,
                mesh.vertices[static_cast<std::size_t>(cells) * side].position};
            return mesh;
        }
    }

    // Bounded synchronous work for an existing caller-owned job. No new scheduler,
    // worker pool, model service, renderer or permanent residency is introduced.
    [[nodiscard]] Result build(const Request& request, std::stop_token stop = {})
    {
        if (stop.stop_requested()) return {.status = Status::cancelled};
        if (static_cast<std::uint8_t>(request.mode) > static_cast<std::uint8_t>(Mode::universe)
            || request.cells_per_tile < 2u || request.cells_per_tile > 64u
            || request.tiles_per_axis < 1u || request.tiles_per_axis > 16u
            || !std::isfinite(request.tile_span_meters) || request.tile_span_meters < 1.0
            || request.tile_span_meters > 16384.0 || !std::isfinite(request.seconds)||!core::valid_world_config(request.world)) return {};
        if (request.ocean_surface && (request.mode == Mode::classic || request.mode == Mode::tiled)) return {};
        const core::Universe world{request.seed,request.world};
        if (request.body_index >= world.bodies().size()) return {};
        if (request.ocean_surface && request.mode != Mode::universe && !world.body(request.body_index).ocean) return {};
        const bool planar = request.mode == Mode::classic || request.mode == Mode::tiled;
        const std::uint32_t axis = request.mode == Mode::tiled ? request.tiles_per_axis : 1u;
        const std::size_t count = planar ? static_cast<std::size_t>(axis) * axis
            : request.mode == Mode::hemisphere ? 12u
            : request.mode == Mode::planet ? 6u : world.bodies().size() * 6u;
        // Includes retained vertex/index buffers, upstream reserve slack, temporary
        // direction/sample arrays and patch metadata; no vegetation/water layers.
        const std::uint64_t side = request.cells_per_tile + 1u;
        const std::uint64_t retained = (side * side + 512u) * sizeof(core::Vertex)
            + (static_cast<std::uint64_t>(request.cells_per_tile) * request.cells_per_tile * 6u + 1500u) * sizeof(std::uint32_t);
        const std::uint64_t scratch = side * side * (2u * sizeof(core::DVec3) + sizeof(core::SurfaceSample));
        const std::uint64_t layers = request.ocean_surface ? 2u : 1u;
        const std::uint64_t admitted = count * (layers * retained + sizeof(Patch)) + scratch + 65536u;
        if (count > request.maximum_patches || admitted > request.maximum_bytes)
            return {.status = Status::capacity_exceeded};
        Result result{.status = Status::ready, .admitted_bytes = admitted};
        result.patches.reserve(count);
        const auto append = [&](std::size_t index, core::TileKey key, std::uint32_t x, std::uint32_t y) {
            auto body = world.body(index);
            body.ocean = body.ocean && request.ocean_surface;
            if (stop.stop_requested()) return false;
            auto mesh = planar ? detail::flat_patch(body, request, x, y, axis, stop)
                : core::build_tile(body, key, {static_cast<int>(request.cells_per_tile), false, false}, stop);
            if (stop.stop_requested() || mesh.vertices.empty() || mesh.indices.empty()) return false;
            result.patches.push_back({body.id, planar ? core::DVec3{} : world.center(index, request.seconds), key, planar, x, y, std::move(mesh)});
            return true;
        };
        if (planar)
        {
            for (std::uint32_t y = 0; y < axis; ++y)
                for (std::uint32_t x = 0; x < axis; ++x)
                    if (!append(request.body_index, {}, x, y)) return {.status = Status::cancelled};
        }
        else
        {
            const auto first = request.mode == Mode::universe ? 0u : request.body_index;
            const auto end = request.mode == Mode::universe ? world.bodies().size() : first + 1u;
            for (auto index = first; index < end; ++index)
                for (std::uint8_t face = 0; face < 6u; ++face)
                {
                    const core::TileKey root{0, 0, static_cast<std::uint8_t>(world.body(index).id), face, 0};
                    if (request.mode == Mode::hemisphere)
                    {
                        if (face == 3u) continue; // Deliberate open boundary at Y=0.
                        for (const auto tile : core::children(root))
                        {
                            if (face != 2u && tile.y == 0u) continue;
                            if (!append(index, tile, 0, 0)) return {.status = Status::cancelled};
                        }
                    }
                    else if (!append(index, root, 0, 0)) return {.status = Status::cancelled};
                }
        }
        return result;
    }
}
