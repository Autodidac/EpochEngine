// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Optional terrain coverage; uses the reviewed planetary sampler and mesh owner.
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
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

    struct GrassPatchKey final
    {
        std::uint32_t body{}, x{}, y{};
        std::uint8_t face{};
        friend bool operator==(GrassPatchKey, GrassPatchKey) noexcept = default;
    };

    struct GrassClump final
    {
        std::uint16_t cell{};
        core::FVec3 root_offset{}, normal{}, color{};
        float height_meters{}, width_meters{}, rotation_radians{};
        friend bool operator==(const GrassClump&, const GrassClump&) noexcept = default;
    };

    struct GrassPatch final
    {
        Status status{Status::invalid_request};
        core::DVec3 body_local_anchor{};
        std::vector<GrassClump> clumps{};
    };

    inline constexpr double grass_patch_span_meters = 16.0;
    inline constexpr std::uint32_t grass_cells_per_side = 24u;
    inline constexpr double grass_visibility_radius_meters = 22.0;
    inline constexpr std::size_t maximum_grass_visible_patches = 64u;

    struct GrassDrawView final
    {
        core::DVec3 surface_center_meters{};
        double body_radius_meters{}, altitude_meters{};
    };

    namespace detail
    {
        [[nodiscard]] std::uint32_t grass_face_span(const core::Body& body) noexcept
        {
            if (body.kind != core::BodyKind::temperate || body.id > 255u
                || !std::isfinite(body.radius) || body.radius < 1.0 || body.radius > 1.0e9
                || !std::isfinite(body.height_scale) || body.height_scale < 0.0
                || !std::isfinite(body.geography_scale) || body.geography_scale <= 0.0)
                return 0u;
            return static_cast<std::uint32_t>(std::ceil(2.0 * body.radius / grass_patch_span_meters));
        }

        [[nodiscard]] double grass_unit(std::uint32_t seed) noexcept
        {
            return static_cast<double>(core::hash32(seed) >> 8u) / 16777216.0;
        }

        [[nodiscard]] core::DVec3 grass_direction(GrassPatchKey key, std::uint32_t face_span, double u, double v)
        {
            const auto scale = 2.0 / face_span;
            return core::cube_direction(key.face, (key.x + u) * scale - 1.0, (key.y + v) * scale - 1.0);
        }

        [[nodiscard]] std::uint32_t grass_grid_index(double coordinate, std::uint32_t face_span) noexcept
        {
            const auto grid = std::clamp(coordinate * 0.5 + 0.5, 0.0, 1.0) * face_span;
            return (std::min)(static_cast<std::uint32_t>(grid), face_span - 1u);
        }

        [[nodiscard]] std::array<core::DVec3,3> grass_face_axes(std::uint8_t face)
        {
            const auto depth = core::cube_direction(face, 0.0, 0.0);
            const auto across = core::cube_direction(face, 1.0, 0.0);
            const auto up = core::cube_direction(face, 0.0, 1.0);
            return {depth,
                core::normalized(across - depth * core::dot(across, depth)),
                core::normalized(up - depth * core::dot(up, depth))};
        }

        [[nodiscard]] double grass_fade(double distance, double full, double cutoff) noexcept
        {
            const auto t = std::clamp((cutoff - distance) / (cutoff - full), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }

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

    // This grid is fixed in physical body coordinates. Selecting a terrain LOD
    // or moving the camera across a patch never reseeds its clumps.
    [[nodiscard]] std::optional<GrassPatchKey> grass_patch_at(const core::Body& body, core::DVec3 direction)
    {
        const auto face_span = detail::grass_face_span(body);
        const double length = core::length(direction);
        if (face_span == 0u || !core::finite(direction) || !std::isfinite(length) || length < 1e-12) return {};
        const auto face = core::locate_tile(static_cast<std::uint8_t>(body.id), direction, 0).face;
        const auto [axis, u_axis, v_axis] = detail::grass_face_axes(face);
        const auto depth = core::dot(direction, axis);
        return GrassPatchKey{body.id, detail::grass_grid_index(core::dot(direction, u_axis) / depth, face_span),
            detail::grass_grid_index(core::dot(direction, v_axis) / depth, face_span), face};
    }

    [[nodiscard]] std::vector<GrassPatchKey> grass_patch_neighborhood(const core::Body& body, core::DVec3 direction)
    {
        const auto home = grass_patch_at(body, direction);
        if (!home) return {};

        const auto span = detail::grass_face_span(body);
        // Project just beyond a face edge, as the terrain grid does for its
        // neighbors. A full cell beyond the edge can skew the adjacent row.
        const auto edge_probe = [span](std::int64_t index) {
            constexpr double epsilon = 1e-5;
            return std::clamp(static_cast<double>(index) + 0.5, -epsilon, span + epsilon);
        };
        std::vector<GrassPatchKey> keys;
        keys.reserve(9u);
        keys.push_back(*home);
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
            {
                if (x == 0 && y == 0) continue;
                const auto column = static_cast<std::int64_t>(home->x) + x;
                const auto row = static_cast<std::int64_t>(home->y) + y;
                if (column >= 0 && column < span && row >= 0 && row < span)
                {
                    keys.push_back({home->body, static_cast<std::uint32_t>(column),
                        static_cast<std::uint32_t>(row), home->face});
                    continue;
                }
                const auto projected = detail::grass_direction(*home, span,
                    edge_probe(column) - home->x, edge_probe(row) - home->y);
                const auto neighbor = grass_patch_at(body, projected);
                if (neighbor && std::ranges::find(keys, *neighbor) == keys.end())
                    keys.push_back(*neighbor);
            }
        return keys;
    }

    [[nodiscard]] std::vector<GrassPatchKey> grass_visible_patch_keys(const core::Body& body, core::DVec3 direction)
    {
        const auto span = detail::grass_face_span(body);
        const auto length = core::length(direction);
        if (span == 0u || !std::isfinite(length) || length < 1e-12) return {};

        const auto eye = direction / length;
        const auto chord = grass_visibility_radius_meters / body.radius + 1e-12;
        const auto half_cell = 1.0 / span;
        const auto cell_diagonal = std::sqrt(2.0) * half_cell;
        constexpr double minimum_face_depth = 0.5773502691896257; // 1/sqrt(3)
        std::vector<std::pair<double,GrassPatchKey>> candidates;
        candidates.reserve(maximum_grass_visible_patches);
        for (std::uint8_t face = 0; face < 6u; ++face)
        {
            const auto [axis, u_axis, v_axis] = detail::grass_face_axes(face);
            const auto depth = core::dot(eye, axis);
            const auto near = (std::max)(minimum_face_depth, depth - chord);
            const auto far = (std::min)(1.0, depth + chord);
            if (near > far) continue;

            // On the face, the projected coordinate is the tangent component
            // divided by depth. Bound both across the angular footprint.
            const auto limits = [&](double component) -> std::optional<std::pair<std::uint32_t,std::uint32_t>> {
                const auto low = component - chord, high = component + chord;
                const auto lower = low / (low < 0.0 ? near : far);
                const auto upper = high / (high > 0.0 ? near : far);
                if (lower > 1.0 || upper < -1.0) return {};
                return {{detail::grass_grid_index(lower, span), detail::grass_grid_index(upper, span)}};
            };
            const auto columns = limits(core::dot(eye, u_axis));
            const auto rows = limits(core::dot(eye, v_axis));
            if (!columns || !rows) continue;
            for (auto y = rows->first; y <= rows->second; ++y)
                for (auto x = columns->first; x <= columns->second; ++x)
                {
                    const auto u = (2.0 * x + 1.0) * half_cell - 1.0;
                    const auto v = (2.0 * y + 1.0) * half_cell - 1.0;
                    const auto distance = core::length(core::cube_direction(face, u, v) - eye);
                    // Differentiating normalization bounds angular change by
                    // the inverse shortest unnormalized vector in this cell.
                    const auto patch_radius = cell_diagonal / std::hypot(1.0,
                        (std::max)(0.0, std::abs(u) - half_cell), (std::max)(0.0, std::abs(v) - half_cell));
                    if (distance <= chord + patch_radius)
                        candidates.push_back({distance, {body.id, x, y, face}});
                }
        }
        std::ranges::stable_sort(candidates, {}, &std::pair<double,GrassPatchKey>::first);
        std::vector<GrassPatchKey> keys;
        keys.reserve((std::min)(candidates.size(), maximum_grass_visible_patches));
        for (const auto& candidate : candidates)
        {
            if (keys.size() == maximum_grass_visible_patches) break;
            keys.push_back(candidate.second);
        }
        return keys;
    }

    // The caller owns scheduling and residency. Keep render blades separate
    // from the terrain mesh so they do not change collision or water clipping.
    [[nodiscard]] GrassPatch build_grass_patch(const core::Body& body, GrassPatchKey key, std::stop_token stop = {})
    {
        if (stop.stop_requested()) return {.status = Status::cancelled};
        const auto face_span = detail::grass_face_span(body);
        if (key.body != body.id || key.face > 5u || key.x >= face_span || key.y >= face_span) return {};
        const auto center = detail::grass_direction(key, face_span, 0.5, 0.5);
        GrassPatch patch{.status = Status::ready,
            .body_local_anchor = center * (body.radius + core::sample_height(body, center))};
        const auto patch_seed = core::hash32(body.seed ^ core::hash32(key.x * 1013904223u)
            ^ core::hash32(key.y * 1664525u) ^ core::hash32(key.face * 48271u + key.body));
        constexpr auto count = grass_cells_per_side * grass_cells_per_side;
        patch.clumps.reserve(count);
        for (std::uint32_t cell = 0; cell < count; ++cell)
        {
            if (stop.stop_requested()) return {.status = Status::cancelled};
            const auto seed = core::hash32(patch_seed + cell * 1013u);
            const auto u = (cell % grass_cells_per_side + 0.10 + 0.80 * detail::grass_unit(seed)) / grass_cells_per_side;
            const auto v = (cell / grass_cells_per_side + 0.10 + 0.80 * detail::grass_unit(seed ^ 0x27d4eb2du)) / grass_cells_per_side;
            const auto direction = detail::grass_direction(key, face_span, u, v);
            const auto surface = core::sample_surface(body, direction);
            if ((body.ocean && surface.height <= 0.65) || surface.temperature < 0.23 || surface.moisture < 0.34) continue;
            const auto root = direction * (body.radius + surface.height);
            const auto density = std::clamp(0.67 + 0.30 * core::noise3(root / 4.5, body.seed ^ 0x9e3779b9u)
                + 0.45 * (surface.moisture - 0.5), 0.0, 1.0);
            if (detail::grass_unit(seed ^ 0x85ebca6bu) >= density) continue;
            const auto normal = core::surface_normal(body, direction, 0.55);
            if (core::dot(normal, direction) < 0.82) continue;
            const auto tint = static_cast<float>(detail::grass_unit(seed ^ 0x165667b1u));
            patch.clumps.push_back({static_cast<std::uint16_t>(cell), core::FVec3{root - patch.body_local_anchor},
                core::FVec3{normal}, {0.12f + 0.10f * tint, 0.21f + 0.14f * tint, 0.045f + 0.035f * tint},
                0.14f + 0.17f * static_cast<float>(detail::grass_unit(seed ^ 0xc2b2ae35u)),
                0.025f + 0.035f * static_cast<float>(detail::grass_unit(seed ^ 0x94d049bbu)),
                static_cast<float>(2.0 * std::numbers::pi * detail::grass_unit(seed ^ 0x7feb352du))});
        }
        if (stop.stop_requested()) return {.status = Status::cancelled};
        return patch;
    }

    // Bounded synchronous work for an existing caller-owned job. No new scheduler,
    // worker pool, model service, renderer or permanent residency is introduced.
    // Ground cover is its own draw mesh. Keep it outside collision, terrain
    // skirts, and the water lattice so terrain LOD cannot erase its roots.
    [[nodiscard]] inline std::optional<core::Mesh> mesh_grass_patch(
        const GrassPatch& patch, std::stop_token stop = {}, const GrassDrawView* view = nullptr)
    {
        if(stop.stop_requested()||patch.status!=Status::ready||!core::finite(patch.body_local_anchor)
            ||patch.clumps.size()>grass_cells_per_side*grass_cells_per_side)return {};
        if(view&&(!core::finite(view->surface_center_meters)||!std::isfinite(view->body_radius_meters)
            ||view->body_radius_meters<=0.0||!std::isfinite(view->altitude_meters)))return {};
        core::Mesh mesh;
        mesh.anchor=patch.body_local_anchor;
        const auto altitude_fade=view?detail::grass_fade(view->altitude_meters,9.0,32.0):1.0;
        if(altitude_fade<=0.0)return mesh;
        mesh.vertices.reserve(patch.clumps.size()*6u);
        mesh.indices.reserve(patch.clumps.size()*12u);
        for(const auto& clump:patch.clumps)
        {
            if(stop.stop_requested())return {};
            const core::DVec3 root{clump.root_offset},normal{clump.normal};
            if(!core::finite(root)||!core::finite(normal)||!core::finite(clump.color)
                ||std::abs(core::dot(normal,normal)-1.0)>1e-4
                ||!std::isfinite(clump.rotation_radians)
                ||!std::isfinite(clump.height_meters)||clump.height_meters<=0.0f
                ||!std::isfinite(clump.width_meters)||clump.width_meters<=0.0f)return {};
            auto gain=altitude_fade;
            if(view)
            {
                const auto surface=core::normalized(patch.body_local_anchor+root)*view->body_radius_meters;
                const auto distance=core::length(surface-view->surface_center_meters);
                gain*=detail::grass_fade(distance,9.0,grass_visibility_radius_meters);
                // At this size blades are sub-pixel, and float vertices
                // otherwise risk collapsing to a degenerate triangle.
                if(gain<0.015)continue;
            }
            const auto tangent=core::tangent(normal),bitangent=core::cross(normal,tangent);
            const auto sine=std::sin(clump.rotation_radians),cosine=std::cos(clump.rotation_radians);
            const auto side=tangent*cosine+bitangent*sine;
            const auto facing=core::cross(normal,side);
            const auto append=[&](core::DVec3 point,float shade){
                mesh.vertices.push_back({core::FVec3{point},clump.normal,
                    {clump.color.x*shade,clump.color.y*shade,clump.color.z*shade,1.0f},{}});
                mesh.bound_radius=(std::max)(mesh.bound_radius,core::length(point));
            };
            for(const auto axis:{side,facing})
            {
                const auto first=static_cast<std::uint32_t>(mesh.vertices.size());
                const auto width=axis*(clump.width_meters*0.5*gain);
                const auto lean=axis*0.22+core::cross(normal,axis)*0.12;
                const auto tip=root+(normal+lean)*(clump.height_meters*gain);
                append(root-width,0.68f);
                append(root+width,0.68f);
                append(tip,1.0f);
                mesh.indices.insert(mesh.indices.end(),{first,first+1u,first+2u,first+2u,first+1u,first});
            }
        }
        if(stop.stop_requested())return {};
        return mesh;
    }

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
