// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Explicit offline producer. No Engine/renderer/model process is started.
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>
import planet.terrain;
import planet.coverage;
import asset.mesh_artifact;
import asset.texture_artifact;
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
import water.planetary;
#endif

namespace coverage = epoch::extensions::planetary_terrain::coverage;
namespace core = epoch::extensions::planetary_terrain::core;
namespace mesh = epochengine::asset::mesh;
int planet_stream_main(int argc, char** argv);
constexpr std::array<std::string_view,5> planet_mode_names{"classic","tiled","hemisphere","planet","universe"};

bool planet_write_mesh_frame(const std::filesystem::path& output,const coverage::Request& request,
    double scale,core::DVec3 origin,std::string_view key,std::size_t vertices,std::size_t indices)
{
    if(static_cast<std::size_t>(request.mode)>=planet_mode_names.size()||!core::valid_world_config(request.world)
        ||!std::isfinite(scale)||scale<=0.0||!core::finite(origin)||key.size()!=64u||vertices==0u||indices==0u)return false;
    for(const auto c:key)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    std::ostringstream frame;frame<<std::setprecision(17)<<"epoch.planet.mesh-frame.v1\nmode="<<planet_mode_names[static_cast<std::size_t>(request.mode)]
        <<"\nseed="<<request.seed<<"\nbody_index="<<request.body_index<<"\ncells_per_tile="<<request.cells_per_tile
        <<"\ntiles_per_axis="<<request.tiles_per_axis<<"\ntile_span_meters="<<request.tile_span_meters<<"\nseconds="<<request.seconds
        <<"\nsize_reference_body="<<request.world.reference_body<<"\nreference_radius_meters="<<request.world.reference_radius_meters
        <<"\nmeters_to_preview_units="<<scale<<"\norigin_meters="<<origin.x<<','<<origin.y<<','<<origin.z
        <<"\ncontent_key="<<key<<"\nvertices="<<vertices<<"\nindices="<<indices<<'\n';
    const auto bytes=frame.str();if(!frame||bytes.size()>4096u)return false;
    std::ofstream receipt{output/"terrain.frame",std::ios::binary|std::ios::noreplace};
    receipt.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));receipt.close();return static_cast<bool>(receipt);
}

mesh::Limits planet_export_limits(std::uint64_t bytes)
{
    if(bytes<1024u*1024u||bytes>64ull*1024u*1024u)return {0u,0u,0u};
    return {static_cast<std::uint32_t>((std::min)(std::uint64_t{1'048'576u},(bytes-48u)/44u)),
        static_cast<std::uint32_t>((std::min)(std::uint64_t{6'291'456u},(bytes-48u)/4u)),bytes};
}

bool planet_export_options(int& argc,char** argv,std::uint64_t& meshBytes,std::optional<double>& seconds,
    double* patch_pixels=nullptr,double* lod_hysteresis=nullptr,core::WorldConfig* world=nullptr,
    std::string* patch_identity=nullptr)
{
    bool meshSeen{},timeSeen{},patchSeen{},hysteresisSeen{},sizeSeen{},radiusSeen{};
    while(argc>=3)
    {
        const std::string_view option{argv[argc-2]},value{argv[argc-1]};
        if(option=="--patch-transport-v1")
        {
            if(!patch_identity||!patch_identity->empty()||value.size()!=136u
                ||!value.starts_with("sha256:")||value[71u]!=':')return false;
            for(std::size_t i=7u;i<value.size();++i)
                if(i!=71u&&!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
            *patch_identity=value;
        }
        else if(option=="--mesh-bytes")
        {
            if(meshSeen)return false;
            meshSeen=true;
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),meshBytes);
            if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())return false;
        }
        else if(option=="--seconds")
        {
            if(timeSeen)return false;
            timeSeen=true;double time{};
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),time);
            if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()
                ||!std::isfinite(time)||time<0.0||time>=1e12)return false;
            seconds=time;
        }
        else if(option=="--patch-pixels"||option=="--lod-hysteresis")
        {
            const bool patch=option=="--patch-pixels";
            auto* output=patch?patch_pixels:lod_hysteresis;
            auto& seen=patch?patchSeen:hysteresisSeen;
            if(!output||seen)return false;
            seen=true;double number{};
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
            if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||!std::isfinite(number)
                ||(patch?(number<32.0||number>4096.0):(number<0.0||number>0.45)))return false;
            *output=number;
        }
        else if(option=="--size-reference-body")
        {
            if(!world||sizeSeen)return false;sizeSeen=true;
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),world->reference_body);
            if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||world->reference_body>5u)return false;
        }
        else if(option=="--reference-radius-meters")
        {
            if(!world||radiusSeen)return false;radiusSeen=true;
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),world->reference_radius_meters);
            if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())return false;
        }
        else
        {
            if(option.starts_with("--"))return false;
            break;
        }
        argc-=2;
    }
    return !world||core::valid_world_config(*world);
}

// Shared exact sample encoding for single and batched procedural queries.
std::string planet_surface_packet(const core::Universe& world,std::uint32_t seed,
    const core::WorldConfig& config,std::uint32_t bodyIndex,core::DVec3 direction,std::uint64_t requestId)
{
    const auto& body=world.body(bodyIndex);
    std::ostringstream packet;packet<<std::setprecision(17)<<"epoch.planet.surface.v1\nrequest_id="<<requestId;
    if(body.kind==core::BodyKind::gas)packet<<"\nstatus=unsupported\n";
    else
    {
        direction=direction/core::length(direction);
        const auto sampled=core::sample_surface(body,direction);
        const auto point=direction*(body.radius+sampled.height);
        const auto normal=core::surface_normal(body,direction);
        if(!std::isfinite(sampled.height)||!core::finite(point)||!core::finite(normal))return {};
        packet<<"\nstatus=ready\nseed="<<seed<<"\nbody="<<body.id
            <<"\nsize_reference_body="<<config.reference_body<<"\nreference_radius_meters="<<config.reference_radius_meters
            <<"\nbody_radius_meters="<<body.radius<<"\nheight_meters="<<sampled.height
            <<"\npoint_x="<<point.x<<"\npoint_y="<<point.y<<"\npoint_z="<<point.z
            <<"\nup_x="<<direction.x<<"\nup_y="<<direction.y<<"\nup_z="<<direction.z
            <<"\nnormal_x="<<normal.x<<"\nnormal_y="<<normal.y<<"\nnormal_z="<<normal.z<<'\n';
    }
    const auto bytes=packet.str();return packet&&bytes.size()<=4096u?bytes:std::string{};
}

// One bounded admission samples one world, without mutating mesh/recipe/cache.
int planet_surface_query_main(int argc,char** argv)
{
    const bool batch=argc>1&&std::string_view{argv[1]}=="--surface-batch";
    if(batch?argc<12:argc!=11)return 2;
    const auto parse=[](const char* text,auto& value) {
        const std::string_view input{text};
        const auto parsed=std::from_chars(input.data(),input.data()+input.size(),value);
        return parsed.ec==std::errc{}&&parsed.ptr==input.data()+input.size();
    };
    std::uint32_t seed{},bodyIndex{},count{1u};std::uint64_t requestId{};core::WorldConfig config;
    if(!parse(argv[3],seed)||!parse(argv[4],bodyIndex)||!parse(argv[5],config.reference_body)
        ||!parse(argv[6],config.reference_radius_meters)||bodyIndex>5u||!core::valid_world_config(config))return 2;
    if(batch&&(!parse(argv[7],requestId)||!parse(argv[8],count)||!count||count>64u
        ||argc!=9+3*static_cast<int>(count)))return 2;
    if(!batch&&!parse(argv[10],requestId))return 2;
    if(!requestId)return 2;
    std::vector<core::DVec3> directions;directions.reserve(count);
    for(std::uint32_t index=0;index<count;++index)
    {
        core::DVec3 direction;const auto offset=batch?9u+index*3u:7u;
        if(!parse(argv[offset],direction.x)||!parse(argv[offset+1u],direction.y)||!parse(argv[offset+2u],direction.z)
            ||!core::finite(direction)||std::abs(core::length(direction)-1.0)>1e-6)return 2;
        directions.push_back(direction);
    }
    const std::filesystem::path destination{argv[2]};
    if(destination.empty()||!destination.is_absolute()||std::filesystem::exists(destination)
        ||!std::filesystem::is_directory(destination.parent_path()))return 2;
    const core::Universe world{seed,config};std::ostringstream packet;
    if(batch)packet<<"epoch.planet.surface-batch.v1\nrequest_id="<<requestId<<"\nsamples="<<count<<'\n';
    for(const auto direction:directions)
    {
        const auto sample=planet_surface_packet(world,seed,config,bodyIndex,direction,requestId);
        if(sample.empty())return 5;
        if(batch)packet<<"sample_bytes="<<sample.size()<<'\n';
        packet<<sample;
    }
    const auto bytes=packet.str();if(!packet||bytes.size()>64u*4160u+128u)return 5;
    std::ofstream receipt{destination,std::ios::binary|std::ios::noreplace};
    receipt.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));receipt.close();
    return receipt?0:7;
}

int main(int argc, char** argv)
{
    if(argc>1&&(std::string_view{argv[1]}=="--surface-query"||std::string_view{argv[1]}=="--surface-batch"))
        return planet_surface_query_main(argc,argv);
    if(argc>1 && (std::string_view{argv[1]}=="--stream-step"
        ||std::string_view{argv[1]}=="--stream-step-preview")) return planet_stream_main(argc,argv);
    std::uint64_t meshBytes=4ull*1024u*1024u;
    std::optional<double> seconds;
    core::WorldConfig world;
    if(!planet_export_options(argc,argv,meshBytes,seconds,nullptr,nullptr,&world))return 2;
    const auto limits=planet_export_limits(meshBytes);
    if(limits.maximum_bytes==0u)return 2;
    // mode output-directory [seed [cells tiles body tile-span-meters]].
    // Refuse existing destinations; failures never
    // replace authored files. Status codes are for the supervising build worker.
    if (argc != 3 && argc != 4 && argc != 8 && argc != 9) return 2;
    coverage::Request request;
    request.world=world;
    request.seconds=seconds.value_or(0.0);
    const auto& names=planet_mode_names;
    const auto mode = std::find(names.begin(), names.end(), std::string_view{argv[1]});
    if (mode == names.end()) return 2;
    request.mode = static_cast<coverage::Mode>(mode - names.begin());
    if (argc >= 4)
    {
        const std::string_view seed{argv[3]};
        const auto parsed = std::from_chars(seed.data(), seed.data() + seed.size(), request.seed);
        if (parsed.ec != std::errc{} || parsed.ptr != seed.data() + seed.size()) return 2;
    }
    if (argc >= 8)
    {
        const auto parse = [](const char* text, auto& value) {
            const std::string_view input{text};
            const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
            return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size();
        };
        if (!parse(argv[4], request.cells_per_tile) || !parse(argv[5], request.tiles_per_axis)
            || !parse(argv[6], request.body_index) || !parse(argv[7], request.tile_span_meters)) return 2;
        if(argc==9)
        {
            std::uint32_t ocean{};
            if(!parse(argv[8],ocean)||ocean>1u)return 2;
            request.ocean_surface=ocean!=0u;
        }
    }
    try
    {
        const auto generated = coverage::build(request);
        if (!generated) return 3;
        const auto origin = request.mode == coverage::Mode::universe ? core::DVec3{}
            : generated.patches.front().body_center;
        double extent{};
        std::size_t vertex_count{}, index_count{};
        for (const auto& patch : generated.patches)
        {
            vertex_count += patch.mesh.vertices.size();
            index_count += patch.mesh.indices.size();
            extent = (std::max)(extent,
                core::length(patch.body_center + patch.mesh.anchor - origin) + patch.mesh.bound_radius);
        }
        if (!std::isfinite(extent) || extent <= 0.0 || vertex_count > limits.maximum_vertices
            || index_count > limits.maximum_indices
            ||48u+vertex_count*44u+index_count*4u>limits.maximum_bytes) return 4;
        // This is a clearly identified, normalized overview artifact. Keep the
        // metric recipe and double patch/body origins separately for real worlds.
        // Never cast an absolute planetary position to float before rebasing.
        // A planet's saved fallback and streamed patches must use the same
        // camera/geometry mapping. The root patch's anchor+bound radius is an
        // intentionally conservative culling bound, not the body's radius.
        const double scale = request.mode == coverage::Mode::planet
            ? 10.0 / core::Universe{request.seed,request.world}.body(request.body_index).radius
            : 10.0 / extent;
        std::vector<mesh::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        struct BodyRange {std::uint32_t id{},first_vertex{},vertices{},first_index{},indices{};};
        std::vector<BodyRange> bodies;
        vertices.reserve(vertex_count); indices.reserve(index_count);
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
        epoch::extensions::planetary_terrain::water::Surface ocean;
        if(request.ocean_surface&&!ocean.evaluate(request.seed,request.seconds,limits.maximum_bytes))return 5;
        const core::Universe worldForWater{request.seed,request.world};
#else
        if(request.ocean_surface)return 5;
#endif
        for (const auto& patch : generated.patches)
        {
            const auto base = static_cast<std::uint32_t>(vertices.size());
            const auto index_base=indices.size();
            if(bodies.empty()||bodies.back().id!=patch.body_id)
                bodies.push_back({patch.body_id,base,0u,static_cast<std::uint32_t>(index_base),0u});
            const auto relative = patch.body_center + patch.mesh.anchor - origin;
            for (const auto& vertex : patch.mesh.vertices)
            {
                const auto p = (relative + core::DVec3{vertex.position}) * scale;
                vertices.push_back({{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)},
                    {vertex.normal.x, vertex.normal.y, vertex.normal.z}, {},
                    {vertex.color[0], vertex.color[1], vertex.color[2]}});
            }
            for (const auto index : patch.mesh.indices) indices.push_back(base + index);
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
            if(request.ocean_surface&&!ocean.apply_patch(vertices,indices,base,index_base,patch.mesh,
                worldForWater.body(patch.body_id),patch.tile,request.cells_per_tile,scale,
                patch.body_center-origin,limits.maximum_bytes))return 5;
#endif
            // Shoreline clipping can append vertices and replace indices.
            // Record this body's final output span, never its input grid size.
            bodies.back().vertices+=static_cast<std::uint32_t>(vertices.size()-base);
            bodies.back().indices+=static_cast<std::uint32_t>(indices.size()-index_base);
        }
        const auto artifact = mesh::build_artifact(vertices, indices, limits);
        if (!artifact) return 5;
        const auto bytes = mesh::encode_artifact(*artifact.artifact);
        if (bytes.empty()) return 5;
        const std::filesystem::path destination{argv[2]};
        // Parent admission is explicit; reject symlink parents rather than
        // redirecting a generated package artifact into another source tree.
        if (destination.empty() || destination.filename().empty()) return 6;
        const auto absolute = std::filesystem::absolute(destination).lexically_normal();
        auto ancestor = absolute.root_path();
        for (const auto& component : absolute.relative_path().parent_path())
        {
            ancestor /= component;
            const auto status = std::filesystem::symlink_status(ancestor);
            if (!std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) return 6;
        }
        if (!std::filesystem::create_directory(absolute)) return 6;
        std::ofstream output{absolute / "terrain.epoch_mesh", std::ios::binary | std::ios::noreplace};
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) return 7;
        std::ifstream verification{absolute / "terrain.epoch_mesh", std::ios::binary | std::ios::ate};
        if (!verification || verification.tellg() != static_cast<std::streamoff>(bytes.size())) return 7;
        std::vector<std::byte> written(bytes.size());
        verification.seekg(0);
        if (!verification.read(reinterpret_cast<char*>(written.data()), static_cast<std::streamsize>(written.size()))) return 7;
        const auto decoded = mesh::decode_artifact(written, limits);
        if (!decoded || decoded.artifact->key() != artifact.artifact->key()) return 7;
        std::ofstream recipe{absolute / "terrain.preview.json", std::ios::out | std::ios::noreplace};
        recipe << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "{\"schema\":\""<<(request.world==core::WorldConfig{}?"epoch.planet.mesh-preview.v1":"epoch.planet.mesh-preview.v2")<<"\",\"mode\":\"" << *mode
            << "\",\"seed\":" << request.seed << ",\"body_index\":" << request.body_index
            << ",\"cells_per_tile\":" << request.cells_per_tile << ",\"tiles_per_axis\":" << request.tiles_per_axis
            << ",\"tile_span_meters\":" << request.tile_span_meters
            << ",\"seconds\":" << request.seconds << ",\"preview_only\":true,\"meters_to_preview_units\":" << scale
            << ",\"origin_meters\":[" << origin.x << ',' << origin.y << ',' << origin.z
            << "],\"mesh_file\":\"terrain.epoch_mesh\",\"content_key\":\""
            << epochengine::asset::texture::content_hash_hex(artifact.artifact->key())
            << "\",\"vertices\":" << vertices.size() << ",\"indices\":" << indices.size()
            << ",\"patches\":[";
        bool first = true;
        for (const auto& patch : generated.patches)
        {
            if (!first) recipe << ',';
            first = false;
            recipe << "{\"body\":" << patch.body_id << ",\"body_center\":["
                << patch.body_center.x << ',' << patch.body_center.y << ',' << patch.body_center.z
                << "],\"anchor\":[" << patch.mesh.anchor.x << ',' << patch.mesh.anchor.y << ',' << patch.mesh.anchor.z
                << "],\"planar\":" << (patch.planar ? "true" : "false")
                << ",\"grid\":[" << patch.grid_x << ',' << patch.grid_y << "],\"tile\":["
                << patch.tile.x << ',' << patch.tile.y << ',' << unsigned(patch.tile.face) << ',' << unsigned(patch.tile.level) << "]}";
        }
        recipe << ']';
        if(request.ocean_surface)recipe << ",\"ocean_surface\":true";
        if(request.world!=core::WorldConfig{})recipe<<",\"size_reference_body\":"<<request.world.reference_body
            <<",\"reference_radius_meters\":"<<request.world.reference_radius_meters;
        recipe << "}\n";
        recipe.close();
        if(!recipe||!planet_write_mesh_frame(absolute,request,scale,origin,
            epochengine::asset::texture::content_hash_hex(artifact.artifact->key()),vertices.size(),indices.size()))return 7;
        // Authoritative subobjects of the one combined artifact, not authored
        // scene entities. Export ranges at the same concatenation boundary.
        const core::Universe world{request.seed,request.world};
        if(bodies.empty()||bodies.size()>16u)return 7;
        std::ifstream frameInput{absolute/"terrain.frame",std::ios::binary};
        const std::string frameBytes{std::istreambuf_iterator<char>{frameInput},{}};
        if(frameBytes.empty()||frameBytes.size()>4096u)return 7;
        std::ostringstream inventory;inventory<<std::setprecision(17)
            <<"epoch.planet.body-inventory.v1\n"<<frameBytes<<"bodies="<<bodies.size()<<'\n';
        for(const auto& range:bodies) {
            const auto& body=world.body(range.id);
            const auto center=world.center(range.id,request.seconds);
            auto minimum=vertices[range.first_vertex].position,maximum=minimum;
            for(std::uint32_t i=0;i<range.vertices;++i)for(std::size_t axis=0;axis<3u;++axis) {
                const auto value=vertices[range.first_vertex+i].position[axis];
                minimum[axis]=(std::min)(minimum[axis],value);maximum[axis]=(std::max)(maximum[axis],value);
            }
            inventory<<"body="<<body.id<<"\nparent="<<body.parent<<"\nname="<<body.name
                <<"\ncenter_meters="<<center.x<<','<<center.y<<','<<center.z
                <<"\nradius_meters="<<body.radius<<"\nfirst_vertex="<<range.first_vertex
                <<"\nvertices="<<range.vertices<<"\nfirst_index="<<range.first_index
                <<"\nindices="<<range.indices<<"\nminimum="<<minimum[0]<<','<<minimum[1]<<','<<minimum[2]
                <<"\nmaximum="<<maximum[0]<<','<<maximum[1]<<','<<maximum[2]<<'\n';
        }
        const auto inventoryBytes=inventory.str();if(inventoryBytes.size()>16384u)return 7;
        std::ofstream bodyOutput{absolute/"terrain.bodies",std::ios::binary|std::ios::noreplace};
        bodyOutput.write(inventoryBytes.data(),static_cast<std::streamsize>(inventoryBytes.size()));
        bodyOutput.close();return bodyOutput?0:7;
    }
    catch (...) { return 8; }
}
