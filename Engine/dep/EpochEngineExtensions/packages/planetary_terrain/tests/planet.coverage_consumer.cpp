// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <vector>
import planet.terrain;
import planet.coverage;
namespace coverage = epoch::extensions::planetary_terrain::coverage;
namespace core = epoch::extensions::planetary_terrain::core;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static core::DVec3 absolute(const coverage::Patch& patch, std::size_t vertex) {
    return patch.mesh.anchor + core::DVec3{patch.mesh.vertices.at(vertex).position};
}
static void verify_visible_grass(const core::Body& body, core::DVec3 direction) {
    const auto keys=coverage::grass_visible_patch_keys(body,direction);
    require(!keys.empty()&&keys.size()<=coverage::maximum_grass_visible_patches
        &&keys==coverage::grass_visible_patch_keys(body,direction*3.0),
        "grass visibility selection exceeded its budget or depended on radius from viewer");
    for(const auto& key:keys)
        require(std::ranges::count(keys,key)==1,"grass visibility selection repeated a patch");
    const auto radial=core::normalized(direction);
    const auto east=core::tangent(radial),north=core::cross(radial,east);
    for(unsigned bearing=0;bearing<48u;++bearing)
    {
        const auto angle=2.0*std::numbers::pi*bearing/48.0;
        const auto forward=east*std::cos(angle)+north*std::sin(angle);
        for(double fraction:{0.0,0.25,0.5,0.75,0.999})
        {
            const auto target=radial+forward*(coverage::grass_visibility_radius_meters*fraction/body.radius);
            const auto key=coverage::grass_patch_at(body,target);
            require(key&&std::ranges::find(keys,*key)!=keys.end(),"grass draw footprint lost a patch inside its fade radius");
        }
    }
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
    const auto& verdant = world.body(0);
    std::optional<coverage::GrassPatchKey> grassKey;
    std::optional<coverage::GrassPatchKey> seaKey;
    for (unsigned index = 0; index < 512u && (!grassKey || !seaKey); ++index) {
        const auto latitude = -0.8 + 1.6 * (index + 0.5) / 512.0;
        const auto azimuth = index * 2.399963229728653;
        const auto horizontal = std::sqrt(1.0 - latitude * latitude);
        const core::DVec3 radial{horizontal * std::cos(azimuth), latitude, horizontal * std::sin(azimuth)};
        const auto surface = core::sample_surface(verdant, radial);
        if (!grassKey && surface.height > 20.0 && surface.temperature > 0.3 && surface.moisture > 0.45)
            grassKey = coverage::grass_patch_at(verdant, radial);
        if (!seaKey && surface.height < -20.0) seaKey = coverage::grass_patch_at(verdant, radial);
    }
    require(grassKey.has_value() && seaKey.has_value(), "Verdant fixture needs fertile land and open water");
    const auto grass = coverage::build_grass_patch(verdant, *grassKey);
    const auto repeatGrass = coverage::build_grass_patch(verdant, *grassKey);
    require(grass.status == coverage::Status::ready && grass.clumps.size() > 30u
        && grass.clumps.size() <= coverage::grass_cells_per_side * coverage::grass_cells_per_side, "grass density or budget");
    require(grass.body_local_anchor == repeatGrass.body_local_anchor && grass.clumps == repeatGrass.clumps,
        "grass regenerated with a different seed or placement");
    require(coverage::grass_patch_at(verdant, grass.body_local_anchor * 3.0) == grassKey,
        "grass patch coordinates depend on viewer altitude");
    const core::DVec3 verdantCapture{3755514.6242091493,3753616.1154924748,3520829.416996601};
    const auto closeKeys=coverage::grass_patch_neighborhood(verdant,verdantCapture);
    verify_visible_grass(verdant,verdantCapture);
    require(closeKeys.size()==9u && closeKeys.front()==coverage::GrassPatchKey{0u,24883u,796173u,0u}
        &&closeKeys==coverage::grass_patch_neighborhood(verdant,verdantCapture*3.0),
        "Verdant camera does not have a stable bounded neighborhood");
    for(std::uint32_t y=796172u;y<=796174u;++y)
        for(std::uint32_t x=24882u;x<=24884u;++x)
            require(std::ranges::find(closeKeys,coverage::GrassPatchKey{0u,x,y,0u})!=closeKeys.end(),
                "Verdant grass neighborhood skipped a row or column");
    const auto inside=0.5/std::ceil(2.0*verdant.radius/coverage::grass_patch_span_meters);
    for(std::uint8_t face=0;face<6u;++face)
    {
        for(unsigned edge=0;edge<4u;++edge)
        {
            const double sign=edge%2u==0u?-1.0:1.0;
            const auto direction=[&](double edgeCoordinate) {
                return edge<2u?core::cube_direction(face,edgeCoordinate,0.37)
                    :core::cube_direction(face,0.37,edgeCoordinate);
            };
            const auto home=coverage::grass_patch_neighborhood(verdant,direction(sign*(1.0-inside)));
            const auto neighbor=coverage::grass_patch_neighborhood(verdant,direction(sign*(1.0+inside)));
            verify_visible_grass(verdant,direction(sign*(1.0-inside)));
            verify_visible_grass(verdant,direction(sign*(1.0+inside)));
            require(home.size()==9u && neighbor.size()==9u && home.front().face==face
                && neighbor.front().face!=face
                && std::ranges::count_if(home,[face](auto key){return key.face==face;})==6
                && std::ranges::find(home,neighbor.front())!=home.end()
                && std::ranges::find(neighbor,home.front())!=neighbor.end(),
                "grass neighborhood skipped or duplicated a cube-edge neighbor");
        }
        for(double u:{-1.0,1.0})
            for(double v:{-1.0,1.0})
            {
                const auto corner=core::cube_direction(face,u*(1.0-inside),v*(1.0-inside));
                const auto keys=coverage::grass_patch_neighborhood(verdant,corner);
                verify_visible_grass(verdant,corner);
                std::set<std::uint8_t> cornerFaces;
                for(auto key:keys)
                {
                    cornerFaces.insert(key.face);
                    require(std::ranges::count(keys,key)==1,"grass neighborhood repeated a cube-corner patch");
                }
                require(keys.size()==8u&&cornerFaces.size()==3u,"grass neighborhood lost a cube-corner face");
            }
    }
    bool variedHeight{}, variedTint{};
    std::set<std::uint16_t> grassCells;
    for (const auto& clump : grass.clumps) {
        const auto root = grass.body_local_anchor + core::DVec3{clump.root_offset};
        const auto direction = core::normalized(root);
        const auto terrain = core::sample_surface(verdant, direction);
        require(coverage::grass_patch_at(verdant, root) == grassKey && grassCells.insert(clump.cell).second,
            "grass crosses a patch edge or duplicates a cell");
        require(terrain.height > 0.65 && std::abs(core::length(root) - verdant.radius - terrain.height) < 2e-5,
            "grass root detached from the dry surface");
        require(core::dot(direction, core::DVec3{clump.normal}) >= 0.82 - 1e-6
            && std::abs(core::length(clump.normal) - 1.0f) < 1e-5f, "grass slope or lighting normal invalid");
        require(clump.height_meters>=0.14f&&clump.height_meters<=0.31f
            &&clump.width_meters>=0.025f&&clump.width_meters<=0.060f,
            "grass lost its narrow blade proportions");
        variedHeight |= clump.height_meters != grass.clumps.front().height_meters;
        variedTint |= clump.color != grass.clumps.front().color;
    }
    require(variedHeight && variedTint, "grass clumps lack variation");
    const auto grassMesh=coverage::mesh_grass_patch(grass);
    require(grassMesh && grassMesh->anchor==grass.body_local_anchor
        &&grassMesh->vertices.size()==grass.clumps.size()*6u
        &&grassMesh->indices.size()==grass.clumps.size()*12u, "grass draw mesh has the wrong anchor or budget");
    for(std::size_t i=0;i<grass.clumps.size();++i)
    {
        const auto root=(core::DVec3{grassMesh->vertices[i*6u].position}
            +core::DVec3{grassMesh->vertices[i*6u+1u].position})*0.5;
        const auto tip=core::DVec3{grassMesh->vertices[i*6u+2u].position};
        const auto otherTip=core::DVec3{grassMesh->vertices[i*6u+5u].position};
        const auto normal=core::DVec3{grass.clumps[i].normal};
        require(core::length(root-core::DVec3{grass.clumps[i].root_offset})<2e-6
            &&std::abs(core::dot(tip-root,normal)-grass.clumps[i].height_meters)<2e-6,
            "grass draw geometry changed root or height");
        require(core::length(tip-otherTip)>0.04
            &&std::abs(core::dot(otherTip-root,normal)-grass.clumps[i].height_meters)<2e-6,
            "crossed grass blades share a tip or changed height");
        require(grassMesh->indices[i*12u]==grassMesh->indices[i*12u+5u]
            &&grassMesh->indices[i*12u+2u]==grassMesh->indices[i*12u+3u],
            "grass blade lost its reverse face");
    }
    const coverage::GrassPatch oneClump{coverage::Status::ready,grass.body_local_anchor,{grass.clumps.front()}};
    const auto root=core::DVec3{oneClump.clumps.front().root_offset};
    const auto normal=core::DVec3{oneClump.clumps.front().normal};
    const auto radial=core::normalized(oneClump.body_local_anchor+root);
    const auto tangent=core::tangent(radial);
    const auto bladeHeight=[&](double distance,double altitude) {
        const coverage::GrassDrawView view{core::normalized(radial+tangent*(distance/verdant.radius))*verdant.radius,
            verdant.radius,altitude};
        const auto mesh=coverage::mesh_grass_patch(oneClump,{},&view);
        require(mesh&&mesh->anchor==oneClump.body_local_anchor,"faded grass changed the patch anchor");
        if(mesh->vertices.empty())return 0.0;
        const auto retainedRoot=(core::DVec3{mesh->vertices[0].position}+core::DVec3{mesh->vertices[1].position})*0.5;
        require(mesh->vertices.size()==6u&&mesh->indices.size()==12u&&core::length(retainedRoot-root)<2e-6,
            "fade relocated the grass root or altered its topology");
        return core::dot(core::DVec3{mesh->vertices[2].position}-retainedRoot,normal);
    };
    const auto fullHeight=bladeHeight(0,0);
    const auto middleHeight=bladeHeight(15.5,0);
    require(std::abs(fullHeight-oneClump.clumps.front().height_meters)<2e-6
        && std::abs(bladeHeight(9,0)-fullHeight)<2e-6
        && std::abs(middleHeight-0.5*fullHeight)<2e-6 && bladeHeight(20,0)<middleHeight
        && std::abs(bladeHeight(16.5,0)-middleHeight)<0.12*fullHeight
        && bladeHeight(22,0)==0.0 && bladeHeight(0,20)<fullHeight && bladeHeight(0,32)==0.0,
        "grass view did not fade distance and altitude continuously around its root");
    const auto seaGrass = coverage::build_grass_patch(verdant, *seaKey);
    require(seaGrass.status == coverage::Status::ready && seaGrass.clumps.empty(), "grass admitted in the sea");
    for (const core::DVec3 seam : {core::DVec3{1.0, 0.0, 1.0}, core::DVec3{1.0, 0.0, 1.000001}}) {
        const auto key = coverage::grass_patch_at(verdant, seam);
        require(key.has_value() && coverage::build_grass_patch(verdant, *key).status == coverage::Status::ready,
            "grass grid lost a cube-face seam");
    }
    require(!coverage::grass_patch_at(world.body(1), {1,0,0}), "temperate grass admitted on another biome");
    require(!coverage::grass_patch_at(verdant, {}), "zero vector admitted as a grass direction");
    require(coverage::grass_patch_neighborhood(world.body(1),{1,0,0}).empty()
        &&coverage::grass_patch_neighborhood(verdant,{}).empty(),"grass neighborhood accepted invalid coverage");
    std::stop_source grassStop;
    grassStop.request_stop();
    const auto cancelledGrass = coverage::build_grass_patch(verdant, *grassKey, grassStop.get_token());
    require(cancelledGrass.status == coverage::Status::cancelled && cancelledGrass.clumps.empty(),
        "cancelled grass patch exposed partial clumps");
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
        << ",\"terrain_bytes_unchanged\":true,\"seams\":true,\"memory_admission\":true,\"cancellation\":true"
        << ",\"grass_clumps\":" << grass.clumps.size() << ",\"verdant_neighbor_patches\":" << closeKeys.size()
        << ",\"grass_cube_edge_and_corner_coverage\":true,\"independent_narrow_blades\":true"
        << ",\"rooted_grass_view_fade\":true,\"native_renderer_tested\":false}\n";
    return result ? 0 : 2;
 } catch (const std::exception& error) {
    std::ofstream result{"coverage-failure.txt"}; result << error.what(); return 1;
 }
}
