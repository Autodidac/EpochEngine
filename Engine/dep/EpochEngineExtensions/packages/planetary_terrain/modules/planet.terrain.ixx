// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch-owned module facade; upstream replacement owners retain their MIT terms.
module;
#include "epoch/planet/terrain.hpp"

export module planet.terrain;

// These declarations expose the existing upstream ABI. Algorithms and state
// remain defined once in the reviewed world/terrain source owners.
export namespace epoch::extensions::planetary_terrain::core
{
    using ::epoch::planet::DVec3;
    using ::epoch::planet::FVec3;
    using ::epoch::planet::BodyKind;
    using ::epoch::planet::Body;
    using ::epoch::planet::SurfaceSample;
    using ::epoch::planet::Universe;
    using ::epoch::planet::WorldConfig;
    using ::epoch::planet::valid_world_config;
    using ::epoch::planet::Vertex;
    using ::epoch::planet::TileKey;
    using ::epoch::planet::TileHash;
    using ::epoch::planet::Mesh;
    using ::epoch::planet::MeshConfig;
    using ::epoch::planet::LodSettings;
    using ::epoch::planet::TerrainView;
    using ::epoch::planet::TerrainPlan;
    using ::epoch::planet::hash32;
    using ::epoch::planet::noise3;
    using ::epoch::planet::fbm;
    using ::epoch::planet::sample_height;
    using ::epoch::planet::sample_surface;
    using ::epoch::planet::surface_normal;
    using ::epoch::planet::kind_name;
    using ::epoch::planet::children;
    using ::epoch::planet::parent;
    using ::epoch::planet::contains;
    using ::epoch::planet::locate_tile;
    using ::epoch::planet::neighbors;
    using ::epoch::planet::balance_cut;
    using ::epoch::planet::cube_direction;
    using ::epoch::planet::tile_direction;
    using ::epoch::planet::build_tile;
    using ::epoch::planet::plan_terrain;
    using ::epoch::planet::plan_body_terrain;
    using ::epoch::planet::finite;
    using ::epoch::planet::length;
    using ::epoch::planet::tangent;
    using ::epoch::planet::cross;
    using ::epoch::planet::normalized;
}
