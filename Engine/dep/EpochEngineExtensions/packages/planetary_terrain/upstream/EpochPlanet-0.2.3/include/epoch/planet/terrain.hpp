#pragma once
#include "epoch/planet/world.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <vector>

namespace epoch::planet {
// No graphics API types are permitted in the simulation/meshing core.
struct Vertex {
    FVec3 position{},normal{};std::array<float,4> color{};
    FVec3 parent_delta{}; // Exact parent-triangle position minus fine position; used for geomorphing.
};
static_assert(sizeof(Vertex)==52);
struct TileKey {
    std::uint32_t x{},y{};
    std::uint8_t body{},face{},level{};
    friend bool operator==(const TileKey&,const TileKey&)=default;
};
struct TileHash { [[nodiscard]] std::size_t operator()(TileKey key) const noexcept; };
[[nodiscard]] std::array<TileKey,4> children(TileKey key);
[[nodiscard]] TileKey parent(TileKey key);
[[nodiscard]] bool contains(TileKey ancestor,TileKey descendant) noexcept;
[[nodiscard]] TileKey locate_tile(std::uint8_t body,DVec3 direction,std::uint8_t level);
[[nodiscard]] std::array<TileKey,4> neighbors(TileKey key);
// Coarsening-only safety pass, also used on the asynchronously resident draw cut.
[[nodiscard]] std::vector<TileKey> balance_cut(std::span<const TileKey> leaves);
[[nodiscard]] DVec3 cube_direction(std::uint8_t face,double u,double v);
[[nodiscard]] DVec3 tile_direction(TileKey key,double local_u,double local_v);
struct Mesh {
    DVec3 anchor{};
    double bound_radius{};
    std::array<FVec3,4> corners{};
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
struct MeshConfig { int cells=16;bool vegetation=false;bool skirts=true; };
[[nodiscard]] Mesh build_tile(const Body& body,TileKey key,MeshConfig config={},std::stop_token stop={});
struct LodSettings {
    std::uint8_t max_level=18;
    std::size_t max_leaves=384;
    double patch_pixels=210.0;
    double hysteresis=0.15;
};
// Optional view-directed refinement. This never removes the body's coarse
// coverage; direction and aspect only prioritize its bounded detail budget.
// An explicit camera up enables forward, 45-degree camera-down (including roll),
// and body-radial nadir rays. Only hit ancestry bypasses the view cone; all three
// share the existing detail/leaf budgets. With altitude, a smooth cubic weight
// trades these hits for the broad circumscribed viewport cone. Transition
// bounds scale with body radius (Earth: 5km to 100km above the visible surface).
// Omitting up retains the original single-ray policy exactly.
struct TerrainView { DVec3 forward{};double aspect_ratio=16.0/9.0;std::optional<DVec3> up{}; };
struct TerrainPlan { std::vector<TileKey> needed;std::vector<TileKey> leaves;std::uint64_t signature{}; };
[[nodiscard]] TerrainPlan plan_terrain(const Universe& world,DVec3 camera,double seconds,
                                     int viewport_height,double fov,LodSettings settings={},const TerrainPlan* previous=nullptr);
// Same bounded planner, restricted to one body. Its leaf budget belongs to
// that body; invisible worlds do not reserve six roots each.
[[nodiscard]] TerrainPlan plan_body_terrain(const Universe& world,std::size_t body_index,
                                          DVec3 camera,double seconds,int viewport_height,
                                          double fov,LodSettings settings={},const TerrainPlan* previous=nullptr);
// Explicit ocean consumers also measure the visible sea surface. Dry callers
// retain the original terrain-only overload and priority ordering.
[[nodiscard]] TerrainPlan plan_body_terrain(const Universe& world,std::size_t body_index,
                                          DVec3 camera,double seconds,int viewport_height,
                                          double fov,LodSettings settings,const TerrainPlan* previous,
                                          bool ocean_surface);
[[nodiscard]] TerrainPlan plan_body_terrain(const Universe& world,std::size_t body_index,
                                          DVec3 camera,double seconds,int viewport_height,
                                          double fov,LodSettings settings,const TerrainPlan* previous,
                                          bool ocean_surface,const TerrainView& view);
} // namespace epoch::planet
