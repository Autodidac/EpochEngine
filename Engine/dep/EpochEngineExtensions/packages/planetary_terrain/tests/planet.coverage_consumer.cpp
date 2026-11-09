// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <stop_token>
import planet.terrain;
import planet.coverage;
namespace coverage = epoch::extensions::planetary_terrain::coverage;
namespace core = epoch::extensions::planetary_terrain::core;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static core::DVec3 absolute(const coverage::Patch& patch, std::size_t vertex) {
    return patch.mesh.anchor + core::DVec3{patch.mesh.vertices.at(vertex).position};
}
int main() {
 try {
    coverage::Request request;
    request.cells_per_tile = 4;
    request.tiles_per_axis = 4;
    request.tile_span_meters = 64;
    const auto classic = coverage::build(request);
    require(static_cast<bool>(classic) && classic.patches.size() == 1, "classic must generate one real grid");
    require(classic.patches[0].planar && classic.patches[0].mesh.vertices.size() == 25, "classic planar grid");
    request.mode = coverage::Mode::tiled;
    const auto tiled = coverage::build(request);
    require(static_cast<bool>(tiled) && tiled.patches.size() == 16, "tiled 4x4 coverage");
    std::uint64_t bytes{};
    for (const auto& patch : tiled.patches) {
        bytes += patch.mesh.vertices.capacity() * sizeof(core::Vertex) + patch.mesh.indices.capacity() * sizeof(std::uint32_t);
        for (const auto index : patch.mesh.indices) require(index < patch.mesh.vertices.size(), "invalid grid index");
        for (const auto& vertex : patch.mesh.vertices) {
            require(core::finite(vertex.position) && core::finite(vertex.normal), "nonfinite grid vertex");
            require(std::abs(core::length(core::DVec3{vertex.normal}) - 1.0) < 1e-5, "grid normal length");
            require(core::length(core::DVec3{vertex.position}) <= patch.mesh.bound_radius, "grid bounds");
        }
    }
    require(bytes < tiled.admitted_bytes && tiled.admitted_bytes <= request.maximum_bytes, "memory admission misses retained buffers");
    const auto& left = tiled.patches[0];
    const auto& right = tiled.patches[1];
    for (std::size_t row = 0; row < 5; ++row) {
        require(core::length(absolute(left, row * 5 + 4) - absolute(right, row * 5)) < 1e-4, "grid position seam");
        require(left.mesh.vertices[row * 5 + 4].normal == right.mesh.vertices[row * 5].normal, "grid lighting seam");
    }
    const auto a = absolute(left, 0), b = absolute(left, 1), c = absolute(left, 5);
    require(std::abs(b.x - a.x - 16.0) < 1e-5 && std::abs(c.z - a.z - 16.0) < 1e-5, "classic/tiled spacing must be planar meters");
    request.mode = coverage::Mode::hemisphere;
    const auto hemisphere = coverage::build(request);
    require(static_cast<bool>(hemisphere) && hemisphere.patches.size() == 12, "hemisphere must cover exact northern half");
    bool equator{};
    for (const auto& patch : hemisphere.patches) {
        require(!patch.planar, "hemisphere incorrectly flattened");
        for (std::size_t i = 0; i < patch.mesh.vertices.size(); ++i) {
            const auto p = absolute(patch, i);
            require(p.y > -1.0, "hemisphere crossed open equator");
            equator |= std::abs(p.y) < 1.0;
        }
    }
    require(equator, "hemisphere has no intentional edge");
    request.mode = coverage::Mode::planet;
    const auto planet = coverage::build(request);
    require(static_cast<bool>(planet) && planet.patches.size() == 6, "planet must cover all six faces");
    std::set<std::uint8_t> faces;
    for (const auto& patch : planet.patches) faces.insert(patch.tile.face);
    require(faces.size() == 6, "planet duplicated or omitted a cube face");
    request.ocean_surface = true;
    const auto wet = coverage::build(request);
    require(static_cast<bool>(wet) && wet.patches.size() == planet.patches.size(), "opaque ocean coverage refused");
    std::size_t waterVertices{};
    const core::Universe oceanWorld{request.seed};
    for (std::size_t face = 0; face < wet.patches.size(); ++face) {
        const auto& dry = planet.patches[face].mesh;
        const auto& patch = wet.patches[face];
        require(patch.mesh.vertices.size() >= dry.vertices.size(), "ocean replaced terrain");
        for(std::size_t i=0;i<dry.vertices.size();++i) {
            const auto& a=dry.vertices[i];const auto& b=patch.mesh.vertices[i];
            require(a.position==b.position && a.normal==b.normal && a.color==b.color
                && a.parent_delta==b.parent_delta, "terrain-only vertex bytes changed");
        }
        require(std::equal(dry.indices.begin(),dry.indices.end(),patch.mesh.indices.begin()), "terrain indices changed");
        for(std::size_t i=dry.vertices.size();i<patch.mesh.vertices.size();++i) {
            const auto& vertex=patch.mesh.vertices[i];
            require(vertex.color[3]==1.0f && vertex.color[0]==0.015f
                && vertex.color[1]==0.085f && vertex.color[2]==0.12f, "ocean has no real opaque blue color");
            require(std::abs(core::length(absolute(patch,i))-oceanWorld.body(0).radius)<1.0, "ocean left sea-level sphere");
            require(std::abs(core::length(core::DVec3{vertex.normal})-1.0)<1e-5, "ocean normal invalid");
            ++waterVertices;
        }
        for(std::size_t i=0;i<patch.mesh.indices.size();i+=3u) {
            const auto a=absolute(patch,patch.mesh.indices[i]);
            const auto u=absolute(patch,patch.mesh.indices[i+1u])-a,v=absolute(patch,patch.mesh.indices[i+2u])-a;
            require(core::length(core::DVec3{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x})>1e-12, "degenerate ocean triangle");
        }
    }
    require(waterVertices>0u, "ocean flag generated no actual sea geometry");
    request.maximum_bytes=planet.admitted_bytes;
    require(coverage::build(request).status==coverage::Status::capacity_exceeded, "ocean ignored double-layer memory admission");
    request.maximum_bytes=64u*1024u*1024u;
    for(std::size_t body=1;body<oceanWorld.bodies().size();++body) {
        request.body_index=body;
        require(coverage::build(request).status==coverage::Status::invalid_request,"unsupported selected-body ocean was silently ignored");
    }
    request.body_index=0;
    request.mode=coverage::Mode::classic;
    require(coverage::build(request).status==coverage::Status::invalid_request, "unimplemented flat ocean silently ignored");
    request.mode=coverage::Mode::hemisphere;
    require(static_cast<bool>(coverage::build(request)), "curved hemisphere ocean refused");
    request.mode=coverage::Mode::universe;
    require(static_cast<bool>(coverage::build(request)), "finite universe ocean refused");
    request.ocean_surface=false;
    request.mode = coverage::Mode::universe;
    const auto universe = coverage::build(request);
    require(static_cast<bool>(universe), "bounded multi-body generation failed");
    const core::Universe world{request.seed};
    require(universe.patches.size() == world.bodies().size() * 6, "multi-body coverage count");
    std::set<std::uint32_t> bodies;
    for (const auto& patch : universe.patches) {
        bodies.insert(patch.body_id);
        require(core::finite(patch.body_center), "invalid body origin");
    }
    require(bodies.size() == world.bodies().size(), "multi-body identities collapsed");
    request.maximum_patches = 1;
    auto denied = coverage::build(request);
    require(denied.status == coverage::Status::capacity_exceeded && denied.patches.empty(), "patch budget must fail before generation");
    request.maximum_patches = 128;
    request.maximum_bytes = 1;
    denied = coverage::build(request);
    require(denied.status == coverage::Status::capacity_exceeded && denied.patches.empty(), "byte budget must fail before generation");
    request.maximum_bytes = 64 * 1024 * 1024;
    std::stop_source stop; stop.request_stop();
    const auto cancelled = coverage::build(request, stop.get_token());
    require(cancelled.status == coverage::Status::cancelled && cancelled.patches.empty(), "cancelled work exposed partial geometry");
    request.tile_span_meters = std::numeric_limits<double>::quiet_NaN();
    require(coverage::build(request).status == coverage::Status::invalid_request, "nonfinite span admitted");
    request.tile_span_meters = 64;
    request.body_index = 9999;
    require(coverage::build(request).status == coverage::Status::invalid_request, "unknown body admitted");
    std::ofstream result{"coverage-result.json"};
    result << "{\"cpu_coverage_pass\":true,\"classic\":1,\"tiled\":16,\"hemisphere\":12,\"planet\":6,\"universe_patches\":"
        << universe.patches.size() << ",\"ocean_surface_vertices\":" << waterVertices
        << ",\"terrain_bytes_unchanged\":true,\"seams\":true,\"memory_admission\":true,\"cancellation\":true,\"native_renderer_tested\":false}\n";
    return result ? 0 : 2;
 } catch (const std::exception& error) {
    std::ofstream result{"coverage-failure.txt"}; result << error.what(); return 1;
 }
}
