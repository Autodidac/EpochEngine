// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
module project.planetary_snapshot;
import asset.texture_artifact;

namespace epochengine::project_planetary_snapshot
{
    struct WetLatticeSamples final
    {
        std::shared_ptr<const water::wave::Tile> tile;
        std::shared_ptr<const WaterLatticeProvider> provider;
        std::map<std::pair<std::uint64_t,std::uint32_t>,std::vector<asset::mesh::Vertex>> water;
        std::uint64_t retained_bytes{}, maximum_bytes{};
        std::uint32_t cells{};
    };

    std::optional<std::array<double,3>> metric_source_origin(
        std::array<double,3> camera_meters,bool bounded_origin) noexcept
    {
        // Reserve half the codec's position envelope for body geometry instead
        // of moving an ordinary planet more than a billion metres from zero.
        // The codec still validates every resulting position and triangle;
        // this does not admit bodies whose geometry itself exceeds its limits.
        constexpr double origin_limit=500'000'000.0;
        for(auto& coordinate:camera_meters)
        {
            if(!std::isfinite(coordinate)||std::abs(coordinate)>1.0e12)return {};
            if(bounded_origin)coordinate=std::clamp(coordinate,-origin_limit,origin_limit);
        }
        return camera_meters;
    }
    namespace
    {
        constexpr std::uint64_t header_bytes=191u, patch_bytes=72u, checksum_bytes=32u;
        constexpr std::uint32_t appearance_magic=0x41575045u; // EPWA
        constexpr std::string_view appearance_format="epoch.planet.water-appearance.v1";
        constexpr std::uint64_t legacy_cloud_header_bytes=56u, density_bytes=84u, solid_density_bytes=92u;
        constexpr std::array<std::string_view,6> formats{
            "epoch.planet.patch.v1","epoch.planet.patch.v2","epoch.planet.patch.v3",
            "epoch.planet.patch.v4","epoch.planet.patch.v5","epoch.planet.patch.v6"};
        constexpr asset::mesh::Limits cloud_limits{24'000u,140'000u,2ull*1024u*1024u};
        std::uint64_t cloud_bytes(const Metadata& data) noexcept
        {return data.cloud&&data.cloud->artifact?48u+44u*data.cloud->artifact->vertices().size()+4u*data.cloud->artifact->indices().size():0u;}
        bool local_water(const Metadata& data) noexcept
        {return data.water_origin_meters!=std::array<double,3>{};}
        bool has_water(const Metadata& data) noexcept
        {return data.water_radius_meters!=0.0||!data.water_vertices.empty()||!data.water_triangles.empty();}
        bool valid_metric_origin(const std::optional<std::array<double,3>>& origin) noexcept
        {
            return !origin||std::ranges::all_of(*origin,[](double coordinate){
                return std::isfinite(coordinate)&&std::abs(coordinate)<=1.0e12;
            });
        }
        std::uint64_t cloud_metadata_bytes(const CloudLayer& cloud) noexcept
        {
            if(!cloud.density)return legacy_cloud_header_bytes;
            return cloud.density->view_occluder_radius_meters>0.0?solid_density_bytes:density_bytes;
        }
        std::uint64_t metadata_bytes(const Metadata& data) noexcept
        {return (data.cloud?cloud_metadata_bytes(*data.cloud)+cloud_bytes(data):0u)+header_bytes+patch_bytes*data.patches.size()+12u*data.parent_delta.size()+checksum_bytes
            +(has_water(data)?24u+(local_water(data)?24u:0u)+20u*data.patches.size()+36u*data.water_vertices.size()+24u*data.water_triangles.size():0u);}
        bool hex(std::string_view text,std::size_t size) noexcept
        {return text.size()==size&&std::ranges::all_of(text,[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
        bool identity(const Identity& value) noexcept
        {return value.request_id&&value.source_hash.starts_with("sha256:")&&hex(std::string_view{value.source_hash}.substr(7),64u)&&hex(value.recipe_key,64u);}
        std::array<TileKey,4> children(TileKey key) noexcept
        {
            const auto level=static_cast<std::uint8_t>(key.level+1u);
            return {{{key.x*2u,key.y*2u,key.body,key.face,level},{key.x*2u+1u,key.y*2u,key.body,key.face,level},
                {key.x*2u,key.y*2u+1u,key.body,key.face,level},{key.x*2u+1u,key.y*2u+1u,key.body,key.face,level}}};
        }
        std::optional<std::uint32_t> find(const Metadata& data,TileKey key) noexcept
        {
            for(std::uint32_t i=0;i<data.patches.size();++i)if(data.patches[i].key==key)return i;
            return {};
        }
        const Patch& patch(const DrawPatch& draw) noexcept {return draw.snapshot->metadata.patches[draw.patch];}
        std::optional<DrawPatch> lookup(const std::shared_ptr<const Snapshot>& target,
            std::span<const DrawPatch> cut,TileKey key) noexcept
        {
            if(const auto index=find(target->metadata,key))return DrawPatch{target,*index,0.0f};
            // A coarsening target need not contain the intermediate ancestors
            // of the old fine cut. Its retained immutable payload owns those
            // exact parents until their descendant quartet has retired. The
            // transition budget allows only the target and one retained snapshot.
            const auto retained=std::ranges::find_if(cut,[&](const DrawPatch& draw){return draw.snapshot!=target;});
            if(retained!=cut.end())
                if(const auto index=find(retained->snapshot->metadata,key))return DrawPatch{retained->snapshot,*index,0.0f};
            return {};
        }
        std::uint64_t bytes(const Snapshot& snapshot) noexcept
        {
            return cloud_bytes(snapshot.metadata)+512u+snapshot.artifact->vertices().size()*sizeof(asset::mesh::Vertex)
                +snapshot.artifact->indices().size()*4u+snapshot.metadata.parent_delta.capacity()*12u
                +snapshot.metadata.patches.capacity()*sizeof(Patch)
                +snapshot.metadata.water_vertices.capacity()*sizeof(WaterVertex)
                +snapshot.metadata.water_triangles.capacity()*sizeof(WaterTriangle)
                +(snapshot.metadata.water_appearance?sizeof(asset::mesh::SphericalWaterAppearance)
                    +snapshot.metadata.water_appearance->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange):0u);
        }
        bool budget(std::span<const DrawPatch> active,const std::shared_ptr<const Snapshot>& target,Limits limits)
        {
            std::vector<const Snapshot*> held;
            if(target)held.push_back(target.get());
            std::uint64_t vertices{},indices{};
            for(const auto& draw:active)
            {
                if(std::ranges::find(held,draw.snapshot.get())==held.end())held.push_back(draw.snapshot.get());
                vertices+=patch(draw).vertex_count;indices+=patch(draw).index_count;
            }
            if(held.size()>2u||vertices>limits.artifact.maximum_vertices||indices>limits.artifact.maximum_indices)return false;
            std::uint64_t total{};for(const auto* snapshot:held)total+=bytes(*snapshot);
            const bool hasAppearance=std::ranges::any_of(held,[](const auto* snapshot){return bool(snapshot->metadata.water_appearance);});
            const auto output=vertices*sizeof(asset::mesh::Vertex)+indices*4u
                +(hasAppearance?sizeof(asset::mesh::SphericalWaterAppearance)
                    +active.size()*sizeof(asset::mesh::SphericalWaterRange):0u);
            // The existing scene cache retains the provider payloads and old /
            // replacement immutable cuts. Assembly and the artifact content-key
            // payload consume the existing preview transient envelope separately.
            total+=2u*output+512u;
            const auto transient=2u*output+5u*active.size()*sizeof(DrawPatch)+512u;
            return total<=limits.maximum_retained_bytes&&transient<=limits.maximum_transient_bytes;
        }
        bool wants_split(const Metadata& target,TileKey key) noexcept
        {
            for(const auto& desired:target.patches)if(desired.target&&contains(key,desired.key)&&key!=desired.key)return true;
            return false;
        }
        bool wants_coarse(const Metadata& target,TileKey key) noexcept
        {
            for(const auto& desired:target.patches)if(desired.target&&contains(desired.key,parent(key)))return true;
            return false;
        }
        bool balanced_split(std::span<const DrawPatch> cut,const Patch& value) noexcept
        {
            for(const auto neighbor:value.neighbors)
                for(const auto& draw:cut)
                    if(contains(patch(draw).key,neighbor)&&patch(draw).key.level<value.key.level)return false;
            return true;
        }
        bool touches_cube_edge(TileKey coarse,std::size_t edge,TileKey descendant) noexcept;
        bool balanced_coarsen(std::span<const DrawPatch> cut,const Patch& value) noexcept
        {
            for(std::size_t edge=0;edge<value.neighbors.size();++edge)
                for(const auto& draw:cut)
                {
                    const auto key=patch(draw).key;
                    if(key.level>value.key.level+1u&&contains(value.neighbors[edge],key)
                        &&touches_cube_edge(value.key,edge,key))return false;
                }
            return true;
        }
        void put(std::vector<std::byte>& out,std::uint64_t value,unsigned size)
        {for(unsigned i=0;i<size;++i)out.push_back(static_cast<std::byte>((value>>(8u*i))&255u));}
        std::uint64_t get(std::span<const std::byte> in,std::size_t& offset,unsigned size) noexcept
        {std::uint64_t value{};for(unsigned i=0;i<size;++i)value|=std::uint64_t{std::to_integer<unsigned>(in[offset++])}<<(8u*i);return value;}
        void put_key(std::vector<std::byte>& out,TileKey key)
        {put(out,key.x,4);put(out,key.y,4);put(out,key.body,1);put(out,key.face,1);put(out,key.level,1);}
        TileKey get_key(std::span<const std::byte> in,std::size_t& offset) noexcept
        {
            TileKey key;key.x=static_cast<std::uint32_t>(get(in,offset,4));key.y=static_cast<std::uint32_t>(get(in,offset,4));
            key.body=static_cast<std::uint8_t>(get(in,offset,1));key.face=static_cast<std::uint8_t>(get(in,offset,1));key.level=static_cast<std::uint8_t>(get(in,offset,1));return key;
        }
    }
    bool valid_key(TileKey key) noexcept
    {return key.body<6u&&key.face<6u&&key.level<=20u&&key.x<(1u<<key.level)&&key.y<(1u<<key.level);}
    TileKey parent(TileKey key) noexcept
    {if(key.level){key.x/=2u;key.y/=2u;--key.level;}return key;}
    bool contains(TileKey a,TileKey b) noexcept
    {return valid_key(a)&&valid_key(b)&&a.body==b.body&&a.face==b.face&&a.level<=b.level
        &&(b.x>>(b.level-a.level))==a.x&&(b.y>>(b.level-a.level))==a.y;}

    bool valid_metadata(const Metadata& data,const asset::mesh::Artifact& artifact,Limits limits) noexcept
    {
        const auto vertices=artifact.vertices();
        const auto indices=artifact.indices();
        if(!identity(data.identity)||data.identity.artifact_key!=artifact.key()||data.patches.size()<6u
            ||limits.maximum_patches>4096u||data.patches.size()>limits.maximum_patches
            ||vertices.size()>limits.artifact.maximum_vertices||indices.size()>limits.artifact.maximum_indices
            ||48u+vertices.size()*44u+indices.size()*4u>limits.artifact.maximum_bytes
            ||data.parent_delta.size()!=vertices.size()
            ||metadata_bytes(data)>limits.maximum_metadata_bytes)return false;
        if(data.cloud)
        {
            const auto& cloud=*data.cloud;
            if(cloud.body>5u||!std::isfinite(cloud.radius_meters)||cloud.radius_meters<=0.0
                ||!std::isfinite(cloud.wind_period_seconds)||cloud.wind_period_seconds<=0.0
                ||std::ranges::any_of(cloud.center_meters,[](double v){return !std::isfinite(v)||std::abs(v)>1e18;})
                ||std::ranges::any_of(data.patches,[&](const Patch& p){return p.key.body!=cloud.body;}))return false;
            if(cloud.density)
            {
                if(cloud.artifact||!asset::cloud_volume::valid(*cloud.density,cloud.radius_meters)
                    ||cloud.wind_period_seconds!=cloud.density->wind_period_seconds)return false;
            }
            else
            {
                if(!cloud.artifact||cloud.artifact->vertices().empty()||cloud.artifact->indices().empty()
                    ||cloud.artifact->vertices().size()>cloud_limits.maximum_vertices
                    ||cloud.artifact->indices().size()>cloud_limits.maximum_indices||cloud_bytes(data)>cloud_limits.maximum_bytes)return false;
                for(const auto& vertex:cloud.artifact->vertices())
                    for(const auto value:vertex.position)if(!std::isfinite(value)||std::abs(value)>20.0f)return false;
            }
        }
        const bool wet=has_water(data);
        if(wet)
        {
            if(!std::isfinite(data.water_radius_meters)||data.water_radius_meters<=0.0
                ||!std::isfinite(data.water_meters_to_artifact)||data.water_meters_to_artifact<=0.0
                ||data.water_triangles.empty()!=data.water_vertices.empty())return false;
            for(const auto& vertex:data.water_vertices)
            {
                double squared{};for(const auto value:vertex.direction){if(!std::isfinite(value))return false;squared+=value*value;}
                if(std::abs(squared-1.0)>1e-6||!std::isfinite(vertex.skirt_depth_meters)
                    ||vertex.skirt_depth_meters<0.0||vertex.skirt_depth_meters>=data.water_radius_meters)return false;
            }
        }
        else if(!data.water_triangles.empty()||data.water_radius_meters!=0.0||data.water_meters_to_artifact!=0.0||local_water(data))return false;
        for(const auto value:data.water_origin_meters)
            if(!std::isfinite(value)||std::abs(value)>1.0e12)return false;
        std::uint64_t vertex_end{},index_end{},water_end{},water_triangle_end{};
        std::array<std::uint64_t,6> area{};
        const auto body=data.patches.front().key.body;
        for(std::size_t i=0;i<data.patches.size();++i)
        {
            const auto& p=data.patches[i];
            if(!valid_key(p.key)||p.key.body!=body||p.first_vertex!=vertex_end||p.first_index!=index_end
                ||p.vertex_count<3u||p.index_count<3u||p.index_count%3u)return false;
            vertex_end+=p.vertex_count;index_end+=p.index_count;
            if(vertex_end>vertices.size()||index_end>indices.size())return false;
            for(const auto n:p.neighbors)if(!valid_key(n)||n.body!=body||n.level!=p.key.level||n==p.key)return false;
            if(p.key.level&&!find(data,parent(p.key)))return false;
            for(std::size_t j=0;j<i;++j)
            {
                const auto& q=data.patches[j];
                if(p.key==q.key||(p.target&&q.target&&(contains(p.key,q.key)||contains(q.key,p.key))))return false;
            }
            if(p.target)area[p.key.face]+=std::uint64_t{1}<<(2u*(20u-p.key.level));
            if(p.terrain_vertices||p.terrain_indices||p.water_triangles)
            {
                if(!wet||!p.target||p.terrain_vertices<3u||p.terrain_indices<3u||p.terrain_indices%3u
                    ||2ull*p.terrain_vertices>p.vertex_count||p.terrain_indices>p.index_count
                    ||p.first_water_vertex!=water_end||p.first_water_triangle!=water_triangle_end
                    ||std::uint64_t(p.first_water_vertex)+p.terrain_vertices>data.water_vertices.size()
                    ||std::uint64_t(p.first_water_triangle)+p.water_triangles>data.water_triangles.size())return false;
                water_end+=p.terrain_vertices;water_triangle_end+=p.water_triangles;
                for(std::uint32_t j=0;j<p.terrain_indices;++j)
                    if(indices[p.first_index+j]>=p.first_vertex+p.terrain_vertices)return false;
                for(std::uint32_t j=0;j<p.terrain_vertices;++j)
                    if(const auto id=data.water_vertices[p.first_water_vertex+j].surface_vertex;
                        id<p.first_water_vertex||id>=p.first_water_vertex+p.terrain_vertices)return false;
                for(std::uint32_t j=0;j<p.water_triangles;++j)
                {
                    const auto& triangle=data.water_triangles[p.first_water_triangle+j];
                    for(const auto id:triangle.water)
                        if(id<p.first_water_vertex||id>=p.first_water_vertex+p.terrain_vertices)return false;
                    for(const auto id:triangle.ground)
                        if(id<p.first_vertex||id>=p.first_vertex+p.terrain_vertices)return false;
                }
            }
            else if(p.first_water_vertex||p.first_water_triangle)return false;
            for(std::uint64_t j=p.first_index;j<index_end;++j)
                if(indices[j]<p.first_vertex||indices[j]>=vertex_end)return false;
            for(std::uint64_t j=p.first_vertex;j<vertex_end;++j)
                for(unsigned axis=0;axis<3u;++axis)
                    if(!std::isfinite(data.parent_delta[j][axis])||std::abs(data.parent_delta[j][axis])>1e9f
                        ||!std::isfinite(vertices[j].position[axis]+data.parent_delta[j][axis]))return false;
        }
        if(vertex_end!=vertices.size()||index_end!=indices.size()
            ||water_end!=data.water_vertices.size()||water_triangle_end!=data.water_triangles.size())return false;
        for(const auto value:area)if(value!=(std::uint64_t{1}<<40u))return false;
        for(const auto& p:data.patches)if(p.target)
            for(const auto neighbor:p.neighbors)
                for(const auto& q:data.patches)if(q.target&&contains(q.key,neighbor)&&p.key.level>q.key.level+1u)return false;
        return true;
    }
    std::vector<std::byte> encode(const Metadata& data,const asset::mesh::Artifact& artifact,Limits limits) noexcept
    {
        try
        {
            if(!valid_metadata(data,artifact,limits))return {};
            const bool wet=has_water(data);
            const bool local=wet&&local_water(data);
            std::vector<std::byte> out;out.reserve(static_cast<std::size_t>(metadata_bytes(data)));
            const unsigned base=local?3u:wet?2u:1u;
            unsigned version=base;
            if(data.cloud)
                version=!data.cloud->density?4u:cloud_metadata_bytes(*data.cloud)==solid_density_bytes?6u:5u;
            put(out,0x544c5050u,4);put(out,version,4);
            if(data.cloud)put(out,base,4);
            put(out,data.identity.request_id,8);
            for(const auto word:data.identity.artifact_key.words)put(out,word,8);
            for(const char c:data.identity.source_hash)put(out,static_cast<unsigned char>(c),1);
            for(const char c:data.identity.recipe_key)put(out,static_cast<unsigned char>(c),1);
            put(out,data.patches.size(),4);put(out,data.parent_delta.size(),4);
            if(wet)
            {
                put(out,std::bit_cast<std::uint64_t>(data.water_radius_meters),8);
                put(out,std::bit_cast<std::uint64_t>(data.water_meters_to_artifact),8);
                if(local)for(const auto value:data.water_origin_meters)put(out,std::bit_cast<std::uint64_t>(value),8);
                put(out,data.water_vertices.size(),4);put(out,data.water_triangles.size(),4);
            }
            for(const auto& p:data.patches)
            {
                put_key(out,p.key);for(const auto n:p.neighbors)put_key(out,n);
                put(out,p.first_vertex,4);put(out,p.vertex_count,4);put(out,p.first_index,4);put(out,p.index_count,4);put(out,p.target?1u:0u,1);
                if(wet){put(out,p.terrain_vertices,4);put(out,p.terrain_indices,4);put(out,p.first_water_vertex,4);put(out,p.first_water_triangle,4);put(out,p.water_triangles,4);}
            }
            for(const auto& delta:data.parent_delta)for(const auto value:delta)put(out,std::bit_cast<std::uint32_t>(value),4);
            if(wet)
            {
                for(const auto& vertex:data.water_vertices){for(const auto value:vertex.direction)put(out,std::bit_cast<std::uint64_t>(value),8);put(out,std::bit_cast<std::uint64_t>(vertex.skirt_depth_meters),8);put(out,vertex.surface_vertex,4);}
                for(const auto& triangle:data.water_triangles){for(const auto id:triangle.water)put(out,id,4);for(const auto id:triangle.ground)put(out,id,4);}
            }
            if(data.cloud)
            {
                const auto& cloud=*data.cloud;
                put(out,cloud.body,4);for(const auto value:cloud.center_meters)put(out,std::bit_cast<std::uint64_t>(value),8);
                put(out,std::bit_cast<std::uint64_t>(cloud.radius_meters),8);
                put(out,std::bit_cast<std::uint64_t>(cloud.wind_period_seconds),8);
                if(cloud.density)
                {
                    const auto& recipe=*cloud.density;put(out,recipe.seed,4);
                    put(out,std::bit_cast<std::uint64_t>(recipe.base_altitude_meters),8);
                    put(out,std::bit_cast<std::uint64_t>(recipe.thickness_meters),8);
                    put(out,std::bit_cast<std::uint64_t>(recipe.horizontal_scale_meters),8);
                    put(out,std::bit_cast<std::uint32_t>(recipe.coverage),4);
                    put(out,std::bit_cast<std::uint32_t>(recipe.extinction_per_meter),4);
                    if(version==6u)put(out,std::bit_cast<std::uint64_t>(recipe.view_occluder_radius_meters),8);
                }
                else
                {
                    const auto encoded=asset::mesh::encode_artifact(*cloud.artifact);if(encoded.empty())return {};
                    put(out,encoded.size(),8);out.insert(out.end(),encoded.begin(),encoded.end());
                }
            }
            const auto hash=asset::texture::deterministic_content_hash(formats[version-1u],out);
            for(const auto word:hash.words)put(out,word,8);
            return out;
        }
        catch(...){return {};}
    }
    std::optional<Metadata> decode(std::span<const std::byte> in,const asset::mesh::Artifact& artifact,
        const Identity& expected,Limits limits,std::stop_token stop) noexcept
    {
        try
        {
            if(stop.stop_requested()||!identity(expected)||in.size()<header_bytes+checksum_bytes||in.size()>limits.maximum_metadata_bytes)return {};
            std::size_t offset{};if(get(in,offset,4)!=0x544c5050u)return {};
            const auto version=get(in,offset,4);if(version<1u||version>formats.size())return {};
            const auto base=version>=4u?get(in,offset,4):version;
            if(base<1u||base>3u)return {};
            const bool wet=base>=2u,local=base==3u;
            Metadata data;data.identity.request_id=get(in,offset,8);
            for(auto& word:data.identity.artifact_key.words)word=get(in,offset,8);
            for(unsigned i=0;i<71u;++i)data.identity.source_hash.push_back(static_cast<char>(get(in,offset,1)));
            for(unsigned i=0;i<64u;++i)data.identity.recipe_key.push_back(static_cast<char>(get(in,offset,1)));
            const auto patches=get(in,offset,4),vertices=get(in,offset,4);
            std::uint64_t water_vertices{},water_triangles{};
            if(wet)
            {
                if(in.size()<header_bytes+24u+(local?24u:0u)+checksum_bytes)return {};
                data.water_radius_meters=std::bit_cast<double>(get(in,offset,8));
                data.water_meters_to_artifact=std::bit_cast<double>(get(in,offset,8));
                if(local)for(auto& value:data.water_origin_meters)value=std::bit_cast<double>(get(in,offset,8));
                water_vertices=get(in,offset,4);water_triangles=get(in,offset,4);
            }
            const auto legacy_size=header_bytes+(patch_bytes+(wet?20u:0u))*patches+12u*vertices+checksum_bytes
                +(wet?24u+(local?24u:0u)+36u*water_vertices+24u*water_triangles:0u);
            const auto density_size=version==6u?solid_density_bytes:version==5u?density_bytes:0u;
            const bool exact_size=version!=4u;
            const auto minimum_size=legacy_size+(exact_size?density_size:legacy_cloud_header_bytes+48u);
            if(data.identity!=expected||patches>limits.maximum_patches||patches>4096u||vertices!=artifact.vertices().size()
                ||minimum_size>in.size()||(exact_size&&minimum_size!=in.size()))return {};
            auto hash_offset=in.size()-checksum_bytes;asset::mesh::ContentHash claimed;
            for(auto& word:claimed.words)word=get(in,hash_offset,8);
            if(claimed!=asset::texture::deterministic_content_hash(formats[version-1u],in.first(in.size()-checksum_bytes)))return {};
            data.patches.resize(static_cast<std::size_t>(patches));
            for(auto& p:data.patches)
            {
                if(stop.stop_requested())return {};
                p.key=get_key(in,offset);for(auto& n:p.neighbors)n=get_key(in,offset);
                p.first_vertex=static_cast<std::uint32_t>(get(in,offset,4));p.vertex_count=static_cast<std::uint32_t>(get(in,offset,4));
                p.first_index=static_cast<std::uint32_t>(get(in,offset,4));p.index_count=static_cast<std::uint32_t>(get(in,offset,4));
                const auto target=get(in,offset,1);if(target>1u)return {};p.target=target!=0u;
                if(wet){p.terrain_vertices=static_cast<std::uint32_t>(get(in,offset,4));p.terrain_indices=static_cast<std::uint32_t>(get(in,offset,4));p.first_water_vertex=static_cast<std::uint32_t>(get(in,offset,4));p.first_water_triangle=static_cast<std::uint32_t>(get(in,offset,4));p.water_triangles=static_cast<std::uint32_t>(get(in,offset,4));}
            }
            data.parent_delta.resize(static_cast<std::size_t>(vertices));
            for(auto& delta:data.parent_delta)for(auto& value:delta)value=std::bit_cast<float>(static_cast<std::uint32_t>(get(in,offset,4)));
            data.water_vertices.resize(static_cast<std::size_t>(water_vertices));data.water_triangles.resize(static_cast<std::size_t>(water_triangles));
            for(auto& vertex:data.water_vertices){for(auto& value:vertex.direction)value=std::bit_cast<double>(get(in,offset,8));vertex.skirt_depth_meters=std::bit_cast<double>(get(in,offset,8));vertex.surface_vertex=static_cast<std::uint32_t>(get(in,offset,4));}
            for(auto& triangle:data.water_triangles){for(auto& id:triangle.water)id=static_cast<std::uint32_t>(get(in,offset,4));for(auto& id:triangle.ground)id=static_cast<std::uint32_t>(get(in,offset,4));}
            if(version>=4u)
            {
                CloudLayer cloud;cloud.body=static_cast<std::uint32_t>(get(in,offset,4));
                for(auto& value:cloud.center_meters)value=std::bit_cast<double>(get(in,offset,8));
                cloud.radius_meters=std::bit_cast<double>(get(in,offset,8));
                cloud.wind_period_seconds=std::bit_cast<double>(get(in,offset,8));
                if(version>=5u)
                {
                    asset::cloud_volume::Recipe recipe;recipe.seed=static_cast<std::uint32_t>(get(in,offset,4));
                    recipe.base_altitude_meters=std::bit_cast<double>(get(in,offset,8));
                    recipe.thickness_meters=std::bit_cast<double>(get(in,offset,8));
                    recipe.horizontal_scale_meters=std::bit_cast<double>(get(in,offset,8));
                    recipe.coverage=std::bit_cast<float>(static_cast<std::uint32_t>(get(in,offset,4)));
                    recipe.extinction_per_meter=std::bit_cast<float>(static_cast<std::uint32_t>(get(in,offset,4)));
                    if(version==6u)
                    {
                        recipe.view_occluder_radius_meters=std::bit_cast<double>(get(in,offset,8));
                        if(recipe.view_occluder_radius_meters<=0.0)return {};
                    }
                    recipe.wind_period_seconds=cloud.wind_period_seconds;cloud.density=recipe;
                }
                else
                {
                    const auto count=get(in,offset,8);
                    if(count>cloud_limits.maximum_bytes||offset>in.size()-checksum_bytes||count!=in.size()-checksum_bytes-offset)return {};
                    if(stop.stop_requested())return {};
                    const auto decoded=asset::mesh::decode_artifact(in.subspan(offset,static_cast<std::size_t>(count)),cloud_limits);
                    if(!decoded)return {};cloud.artifact=decoded.artifact;
                }
                data.cloud=std::move(cloud);
            }
            if(stop.stop_requested()||!valid_metadata(data,artifact,limits))return {};
            return data;
        }
        catch(...){return {};}
    }

    std::vector<std::byte> encode_water_appearance(
        std::span<const asset::mesh::SphericalWaterRange> ranges, const Identity& expected) noexcept
    {
        try
        {
            if(!identity(expected)||ranges.empty()||ranges.size()>4096u)return {};
            std::vector<std::byte> encoded;
            encoded.reserve(52u+ranges.size()*12u+checksum_bytes);
            put(encoded,appearance_magic,4);
            put(encoded,1u,4);
            put(encoded,expected.request_id,8);
            for(const auto word:expected.artifact_key.words)put(encoded,word,8);
            put(encoded,ranges.size(),4);
            for(const auto& range:ranges)
            {
                put(encoded,range.first_vertex,4);
                put(encoded,range.vertex_count,4);
                put(encoded,std::bit_cast<std::uint32_t>(range.specular_scale),4);
            }
            const auto hash=asset::texture::deterministic_content_hash(appearance_format,encoded);
            for(const auto word:hash.words)put(encoded,word,8);
            return encoded;
        }
        catch(...){return {};}
    }

    namespace
    {
        bool water_appearance_matches(const asset::mesh::Artifact& artifact,const Metadata& metadata,
            std::stop_token stop) noexcept
        {
            const auto& expected=metadata.identity;
            return !stop.stop_requested()&&identity(expected)&&artifact.key()==expected.artifact_key
                &&!metadata.patches.empty()&&metadata.patches.size()<=4096u;
        }

        std::shared_ptr<const asset::mesh::SphericalWaterAppearance> admit_water_appearance_ranges(
            const asset::mesh::Artifact& artifact,const Metadata& metadata,std::stop_token stop,auto read_range)
        {
            auto appearance=std::make_shared<asset::mesh::SphericalWaterAppearance>();
            appearance->artifact_key=artifact.key();
            const auto indices=artifact.indices();
            std::uint64_t vertex_end{},index_end{};
            for(std::size_t entry=0;entry<metadata.patches.size();++entry)
            {
                const auto& patch=metadata.patches[entry];
                if(stop.stop_requested()||patch.first_vertex!=vertex_end||patch.first_index!=index_end
                    ||patch.index_count%3u)return {};
                vertex_end+=patch.vertex_count;
                index_end+=patch.index_count;
                if(vertex_end>artifact.vertices().size()||index_end>indices.size())return {};
                const auto range=read_range(entry);
                // Per-vertex specular weights are produced by runtime materialization.
                if(range.vertex_specular)return {};
                const auto first=range.first_vertex;
                const auto count=range.vertex_count;
                const auto scale=range.specular_scale;
                if(!count)
                {
                    if(first||scale!=0.0f||patch.terrain_vertices)return {};
                    continue;
                }
                if(first<std::uint64_t(patch.first_vertex)+3u||count<3u
                    ||std::uint64_t(first)+count!=vertex_end||!std::isfinite(scale)||scale<0.0f||scale>1.0f
                    ||(patch.terrain_vertices&&first-patch.first_vertex!=patch.terrain_vertices))return {};
                for(std::uint64_t i=patch.first_index;i<index_end;i+=3u)
                {
                    const bool water=indices[i]>=first;
                    if((indices[i+1u]>=first)!=water||(indices[i+2u]>=first)!=water)return {};
                }
                appearance->ranges.push_back(range);
            }
            if(vertex_end!=artifact.vertices().size()||index_end!=indices.size()||stop.stop_requested())return {};
            return appearance;
        }
    }

    std::shared_ptr<const asset::mesh::SphericalWaterAppearance> admit_water_appearance(
        std::span<const asset::mesh::SphericalWaterRange> ranges,const asset::mesh::Artifact& artifact,
        const Metadata& metadata,std::stop_token stop) noexcept
    {
        try
        {
            if(!water_appearance_matches(artifact,metadata,stop)||ranges.size()!=metadata.patches.size())return {};
            return admit_water_appearance_ranges(artifact,metadata,stop,[ranges](std::size_t entry) {
                return ranges[entry];
            });
        }
        catch(...){return {};}
    }

    std::shared_ptr<const asset::mesh::SphericalWaterAppearance> decode_water_appearance(
        std::span<const std::byte> encoded,const asset::mesh::Artifact& artifact,
        const Metadata& metadata,std::stop_token stop) noexcept
    {
        try
        {
            if(!water_appearance_matches(artifact,metadata,stop)
                ||encoded.size()!=52u+metadata.patches.size()*12u+checksum_bytes)return {};
            const auto hash=asset::texture::deterministic_content_hash(appearance_format,encoded.first(encoded.size()-checksum_bytes));
            std::size_t hash_offset=encoded.size()-checksum_bytes;
            for(const auto word:hash.words)if(get(encoded,hash_offset,8)!=word)return {};
            const auto& expected=metadata.identity;
            std::size_t offset{};
            if(get(encoded,offset,4)!=appearance_magic||get(encoded,offset,4)!=1u
                ||get(encoded,offset,8)!=expected.request_id)return {};
            for(const auto word:expected.artifact_key.words)if(get(encoded,offset,8)!=word)return {};
            if(get(encoded,offset,4)!=metadata.patches.size())return {};
            return admit_water_appearance_ranges(artifact,metadata,stop,[encoded,&offset](std::size_t) {
                return asset::mesh::SphericalWaterRange{
                    static_cast<std::uint32_t>(get(encoded,offset,4)),
                    static_cast<std::uint32_t>(get(encoded,offset,4)),
                    std::bit_cast<float>(static_cast<std::uint32_t>(get(encoded,offset,4)))};
            });
        }
        catch(...){return {};}
    }

    namespace
    {
        const asset::mesh::SphericalWaterRange* water_range(const Metadata& metadata,const Patch& patch) noexcept
        {
            if(!metadata.water_appearance)return nullptr;
            const auto& ranges=metadata.water_appearance->ranges;
            const auto found=std::ranges::lower_bound(ranges,patch.first_vertex,{},
                &asset::mesh::SphericalWaterRange::first_vertex);
            return found!=ranges.end()&&found->first_vertex<std::uint64_t(patch.first_vertex)+patch.vertex_count
                ? &*found : nullptr;
        }

        double water_lattice_spacing(const Metadata& metadata,const Patch& patch) noexcept
        {
            const std::size_t first=patch.first_water_vertex;
            if(patch.terrain_vertices<2u||first>=metadata.water_vertices.size()
                ||metadata.water_vertices.size()-first<2u)
                return std::numeric_limits<double>::infinity();
            const auto& a=metadata.water_vertices[first].direction;
            const auto& b=metadata.water_vertices[first+1u].direction;
            const double x=b[0]-a[0],y=b[1]-a[1],z=b[2]-a[2];
            return std::sqrt(x*x+y*y+z*z)*metadata.water_radius_meters;
        }

        float shore_tint(const Metadata& metadata) noexcept
        {
            const auto& origin=metadata.water_origin_meters;
            return local_water(metadata)?water::wave::spherical_shore_tint_strength(
                std::hypot(origin[0],origin[1],origin[2])-metadata.water_radius_meters):1.0f;
        }

        template<class Scale>
        std::array<float,4> water_edge_scales(const Metadata& metadata,const Patch& patch,
            float interior,Scale neighbor_scale) noexcept
        {
            std::array<float,4> scales{};
            scales.fill(interior);
            if(patch.key.level==0u||interior<=0.0f)return scales;
            for(std::size_t side=0;side<scales.size();++side)
            {
                const auto adjacent=find(metadata,parent(patch.neighbors[side]));
                if(!adjacent||!metadata.patches[*adjacent].target)continue;
                scales[side]=(std::min)(interior,neighbor_scale(metadata,metadata.patches[*adjacent],interior));
            }
            return scales;
        }

        float specular_scale(const Metadata& metadata,const Patch& patch,float fallback) noexcept
        {
            const auto* range=water_range(metadata,patch);
            return range?range->specular_scale:fallback;
        }

        float breaker_scale(const Metadata& metadata,const Patch& patch,float) noexcept
        {return water::wave::spherical_breaker_scale(water_lattice_spacing(metadata,patch));}

        bool fades_at_edge(const std::array<float,4>& edges,float interior) noexcept
        {return std::ranges::any_of(edges,[&](float weight){return weight<interior;});}

        using MeshVertex=asset::mesh::Vertex;
        using WaterRange=asset::mesh::SphericalWaterRange;

        struct OceanLattice final
        {
            const Snapshot* source{};
            std::span<const MeshVertex> ground,water;
            std::span<const std::uint32_t> ground_indices;
            std::array<double,3> body_center{};
            std::array<float,4> edge_weights{};
            std::uint32_t cells{};
            float specular{};
            bool vertex_specular{}, common_origin{};
        };

        std::uint32_t perimeter_top(std::uint32_t index,std::uint32_t n) noexcept
        {
            const auto side=n+1u;
            if(index<n)return index;
            if(index<2u*n)return (index-n)*side+n;
            if(index<3u*n)return n*side+3u*n-index;
            return (4u*n-index)*side;
        }

        // Keep the old snapshot and its attested clock intact. All generated
        // positions use the new origin before narrowing to artifact floats.
        std::span<const MeshVertex> sample_lattice(WetLatticeSamples& common,const Snapshot& source,
            const Patch& p,std::span<const MeshVertex> ground,const Snapshot& target,
            bool originally_wet,std::stop_token stop)
        {
            const auto key=std::pair{source.metadata.identity.request_id,p.first_vertex};
            if(const auto found=common.water.find(key);found!=common.water.end())return found->second;
            const auto cells=common.cells,side=cells+1u,grid=side*side;
            const auto layer=static_cast<std::uint32_t>(ground.size());
            constexpr std::uint64_t allocation_overhead=256u;
            const auto resident=allocation_overhead+layer*sizeof(MeshVertex);
            const auto scratch=allocation_overhead+layer*sizeof(WaterVertex);
            if(common.retained_bytes+resident+scratch>common.maximum_bytes||stop.stop_requested())return {};
            std::optional<std::vector<WaterVertex>> generated;
            std::span<const WaterVertex> basis;
            if(p.terrain_vertices)
                basis=std::span{source.metadata.water_vertices}.subspan(p.first_water_vertex,layer);
            else if(common.provider)
            {
                generated=common.provider->build_basis(p.key,source.metadata.water_radius_meters,
                    cells,layer!=grid,stop);
                if(generated&&generated->capacity()<=layer)basis=*generated;
            }
            if(basis.size()!=layer||stop.stop_requested())return {};
            const auto radius=target.metadata.water_radius_meters;
            const auto scale=target.metadata.water_meters_to_artifact;
            const auto& origin=target.metadata.water_origin_meters;
            const std::array body_center{-origin[0]*scale,-origin[1]*scale,-origin[2]*scale};
            const auto& first=basis[0].direction;
            const auto& next=basis[1].direction;
            const auto spacing=std::hypot(next[0]-first[0],next[1]-first[1],next[2]-first[2])*radius;
            const auto tint=shore_tint(target.metadata);
            const auto breaker=tint>0.0f?water::wave::spherical_breaker_scale(spacing):0.0f;
            const auto breaker_edges=water_edge_scales(target.metadata,p,breaker,breaker_scale);
            const auto length=common.tile->length_meters();
            std::vector<MeshVertex> vertices(layer);
            for(std::uint32_t i=0;i<layer;++i)
            {
                if(stop.stop_requested())return {};
                const auto top=i<grid?i:perimeter_top(i-grid,cells);
                const auto& vertex=basis[i];
                const auto offset=p.terrain_vertices?p.first_water_vertex:0u;
                if(vertex.surface_vertex!=offset+top||!std::isfinite(vertex.skirt_depth_meters)
                    ||vertex.skirt_depth_meters<0.0||(i<grid&&vertex.skirt_depth_meters!=0.0))return {};
                auto& value=vertices[i];
                if(i>=grid)
                    value=vertices[top];
                const auto sample=water::wave::spherical_sample(*common.tile,vertex.direction,radius,common.tile->seconds());
                if(!sample)return {};
                const auto& point=sample->position_meters;
                const auto sampled_radius=std::sqrt(point[0]*point[0]+point[1]*point[1]+point[2]*point[2]);
                if(!std::isfinite(sampled_radius)||sampled_radius<=0.0)return {};
                for(unsigned axis=0;axis<3u;++axis)
                    value.position[axis]=static_cast<float>((point[axis]-point[axis]/sampled_radius
                        *vertex.skirt_depth_meters-origin[axis])*scale);
                if(i>=grid)continue;
                auto bed=ground[top].position;
                if(source.metric_origin_meters!=target.metric_origin_meters)
                    for(unsigned axis=0;axis<3u;++axis)
                        bed[axis]=static_cast<float>(double(bed[axis])+((*source.metric_origin_meters)[axis]-origin[axis]));
                const auto depth=water::wave::spherical_shore_depth(bed,value.position,body_center);
                if(!depth||(!originally_wet&&*depth>0.0))return {};
                const auto bed_radius=std::hypot(origin[0]+bed[0]/scale,origin[1]+bed[1]/scale,origin[2]+bed[2]/scale);
                const auto weight=water::wave::spherical_water_edge_weight(breaker_edges,breaker,i%side,i/side,cells);
                value.normal=water::wave::spherical_shoaled_normal(sample->normal,vertex.direction,
                    radius-bed_radius,sampled_radius-radius,weight*tint);
                value.color=water::wave::spherical_surface_color(radius-bed_radius,sample->foam,spacing,length,
                    false,tint,sampled_radius-radius,weight);
            }
            if(stop.stop_requested())return {};
            const auto inserted=common.water.emplace(key,std::move(vertices));
            common.retained_bytes+=resident;
            return inserted.first->second;
        }

        std::optional<OceanLattice> ocean_lattice(const Snapshot& source,const Patch& p,
            std::uint32_t cells,double seconds,std::span<const DrawPatch> active,
            const Snapshot& target,WetLatticeSamples* common,std::stop_token stop)
        {
            const auto* range=water_range(source.metadata,p);
            const bool regenerate=!range||source.water_sample_seconds!=seconds;
            if(regenerate&&(!common||!source.water_sample_seconds||common->tile->seconds()!=seconds
                ||common->cells!=cells||source.metadata.water_radius_meters!=target.metadata.water_radius_meters
                ||source.metadata.water_meters_to_artifact!=target.metadata.water_meters_to_artifact))return {};
            const auto side=cells+1u,grid=side*side;
            const auto vertices=source.artifact->vertices();
            const auto indices=source.artifact->indices();
            if(range&&(source.metadata.water_appearance->artifact_key!=source.artifact->key()
                ||range->first_vertex<p.first_vertex
                ||std::uint64_t(range->first_vertex)+range->vertex_count!=std::uint64_t(p.first_vertex)+p.vertex_count
                ||!std::isfinite(range->specular_scale)||range->specular_scale<0.0f||range->specular_scale>1.0f))return {};
            const auto layer=range?range->first_vertex-p.first_vertex:p.vertex_count;
            const auto topology=6u*cells*cells+(layer==grid?0u:24u*cells);
            const auto scale=source.metadata.water_meters_to_artifact;
            const auto& origin=source.metadata.water_origin_meters;
            if((layer!=grid&&layer!=grid+4u*cells)||(range&&range->vertex_count<layer)
                ||p.index_count<topology||!std::isfinite(scale)||scale<=0.0
                ||!std::isfinite(source.metadata.water_radius_meters)||source.metadata.water_radius_meters<=0.0
                ||(!source.metric_origin_meters&&origin!=std::array<double,3>{})
                ||(source.metric_origin_meters&&(*source.metric_origin_meters!=origin||scale!=1.0)))return {};
            auto ground_indices=indices.subspan(p.first_index,topology);
            if(!range&&(p.terrain_vertices||p.index_count!=topology||!source.metadata.water_appearance))return {};
            if(p.terrain_vertices&&(p.terrain_vertices!=layer||p.terrain_indices!=topology
                ||p.water_triangles!=topology/3u))return {};
            // The producer uses the same regular lattice for both surfaces.
            // Older ancestors can lack a WaterTriangle recipe. In that case
            // validate the exact ground topology before mirroring it for water.
            // Exact grid and skirt indices also establish the vertex bounds.
            std::size_t at{};
            const auto triangle=[&](std::uint32_t a,std::uint32_t b,std::uint32_t c) {
                return ground_indices[at++]==p.first_vertex+a&&ground_indices[at++]==p.first_vertex+b
                    &&ground_indices[at++]==p.first_vertex+c;
            };
            for(std::uint32_t y=0;y<cells;++y)for(std::uint32_t x=0;x<cells;++x)
            {
                const auto a=y*side+x,b=a+1u,c=a+side,d=c+1u;
                if(!triangle(a,b,d)||!triangle(a,d,c))return {};
            }
            if(layer!=grid)
                for(std::uint32_t i=0;i<4u*cells;++i)
                {
                    const auto j=(i+1u)%(4u*cells);
                    if(!triangle(perimeter_top(i,cells),grid+i,grid+j)
                        ||!triangle(perimeter_top(i,cells),grid+j,perimeter_top(j,cells)))return {};
                }
            if(p.terrain_vertices)
                for(std::uint32_t i=0;i<p.water_triangles;++i)
                    for(std::uint32_t corner=0;corner<3u;++corner)
                    {
                        const auto raw=ground_indices[i*3u+corner]-p.first_vertex;
                        const auto& typed=source.metadata.water_triangles[p.first_water_triangle+i];
                        if(typed.water[corner]!=p.first_water_vertex+raw
                            ||source.metadata.water_vertices[typed.water[corner]].surface_vertex
                                !=p.first_water_vertex+(raw<grid?raw:perimeter_top(raw-grid,cells)))return {};
                    }
            auto ground=vertices.subspan(p.first_vertex,layer);
            auto wave=regenerate?sample_lattice(*common,source,p,ground,target,range!=nullptr,stop)
                :vertices.subspan(range->first_vertex,layer);
            if(wave.empty())return {};
            const auto& center=regenerate?target.metadata.water_origin_meters:origin;
            // An admitted dry endpoint contributes signed depth but has no
            // resolved highlight. A wet parent supplies its highlight in the blend.
            const auto specular=range?range->specular_scale:0.0f;
            std::array<float,4> edges;
            edges.fill(specular);
            if(p.key.level)
                for(std::size_t side_index=0;side_index<edges.size();++side_index)
                {
                    const auto key=parent(p.neighbors[side_index]);
                    const auto neighbor=std::ranges::find_if(active,[&](const auto& draw){return patch(draw).key==key;});
                    if(neighbor==active.end())continue;
                    edges[side_index]=(std::min)(edges[side_index],
                        specular_scale(neighbor->snapshot->metadata,patch(*neighbor),edges[side_index]));
                }
            return OceanLattice{&source,ground,wave,ground_indices,
                {-center[0]*scale,-center[1]*scale,-center[2]*scale},edges,cells,specular,
                range&&range->vertex_specular&&!regenerate,regenerate};
        }

        struct LatticePoint final { double x{},y{}; };

        LatticePoint in_parent(TileKey key,std::uint32_t top,std::uint32_t n) noexcept
        {
            return {0.5*double((key.x&1u)*n+top%(n+1u)),0.5*double((key.y&1u)*n+top/(n+1u))};
        }

        struct OceanSample final
        {
            MeshVertex ground{},water{};
            double depth{};
        };

        MeshVertex sample_vertex(const MeshVertex& a,const MeshVertex& b,double weight)
        {
            MeshVertex result{};
            for(unsigned axis=0;axis<3u;++axis)
            {
                result.position[axis]=static_cast<float>(std::lerp(double(a.position[axis]),double(b.position[axis]),weight));
                result.normal[axis]=static_cast<float>(std::lerp(double(a.normal[axis]),double(b.normal[axis]),weight));
                result.color[axis]=static_cast<float>(std::lerp(double(a.color[axis]),double(b.color[axis]),weight));
            }
            for(unsigned axis=0;axis<2u;++axis)
                result.uv[axis]=static_cast<float>(std::lerp(double(a.uv[axis]),double(b.uv[axis]),weight));
            return result;
        }

        OceanSample mix_ocean(const OceanSample& a,const OceanSample& b,double weight)
        {
            return {sample_vertex(a.ground,b.ground,weight),sample_vertex(a.water,b.water,weight),
                std::lerp(a.depth,b.depth,weight)};
        }

        void rebase_vertex(MeshVertex& value,const Snapshot& from,const Snapshot& target)
        {
            if(from.metric_origin_meters!=target.metric_origin_meters)
                for(unsigned axis=0;axis<3u;++axis)
                    value.position[axis]=static_cast<float>(double(value.position[axis])
                        +((*from.metric_origin_meters)[axis]-(*target.metric_origin_meters)[axis]));
        }

        std::optional<OceanSample> read_ocean(const OceanLattice& lattice,std::uint32_t index,const Snapshot& target)
        {
            const auto n=lattice.cells,side=n+1u,grid=side*side;
            const auto top=index<grid?index:perimeter_top(index-grid,n);
            auto ground=lattice.ground[top];
            if(lattice.common_origin)rebase_vertex(ground,*lattice.source,target);
            const auto depth=water::wave::spherical_shore_depth(ground.position,lattice.water[top].position,lattice.body_center);
            if(!depth)return {};
            OceanSample result{lattice.ground[index],lattice.water[index],*depth};
            result.water.uv[0]=lattice.vertex_specular?lattice.water[index].uv[0]
                :water::wave::spherical_water_edge_weight(lattice.edge_weights,lattice.specular,top%side,top/side,n);
            rebase_vertex(result.ground,*lattice.source,target);
            if(!lattice.common_origin)rebase_vertex(result.water,*lattice.source,target);
            return result;
        }

        std::optional<OceanSample> parent_ocean(const OceanLattice& lattice,LatticePoint q,bool lower,const Snapshot& target)
        {
            const auto n=lattice.cells,side=n+1u,grid=side*side;
            if(lower&&lattice.ground.size()>grid)
            {
                std::optional<double> edge;
                if(q.y==0.0)edge=q.x;
                else if(q.x==n)edge=n+q.y;
                else if(q.y==n)edge=3.0*n-q.x;
                else if(q.x==0.0)edge=4.0*n-q.y;
                if(edge)
                {
                    const auto i=static_cast<std::uint32_t>(*edge);
                    const auto a=read_ocean(lattice,grid+i%(4u*n),target);
                    const auto b=read_ocean(lattice,grid+(i+1u)%(4u*n),target);
                    if(!a||!b)return {};
                    return mix_ocean(*a,*b,*edge-i);
                }
            }
            const auto x=(std::min)(static_cast<std::uint32_t>(q.x),n-1u);
            const auto y=(std::min)(static_cast<std::uint32_t>(q.y),n-1u);
            const auto fx=q.x-x,fy=q.y-y;
            const auto a=read_ocean(lattice,y*side+x,target);
            const auto b=read_ocean(lattice,fx>=fy?y*side+x+1u:(y+1u)*side+x,target);
            const auto d=read_ocean(lattice,(y+1u)*side+x+1u,target);
            if(!a||!b||!d)return {};
            const auto diagonal=fx>=fy?fy:fx;
            const auto other=std::abs(fx-fy);
            if(diagonal==1.0)return d;
            // Interpolate the parent's signed depth field, along with its
            // positions. Re-evaluating radial depth here changes the contour.
            return mix_ocean(mix_ocean(*a,*b,other/(1.0-diagonal)),*d,diagonal);
        }

        bool usable_triangle(std::span<const MeshVertex> vertices,std::array<std::uint32_t,3> corners) noexcept
        {
            const auto& a=vertices[corners[0]].position;
            const auto& b=vertices[corners[1]].position;
            const auto& c=vertices[corners[2]].position;
            const std::array ab{double(b[0])-a[0],double(b[1])-a[1],double(b[2])-a[2]};
            const std::array ac{double(c[0])-a[0],double(c[1])-a[1],double(c[2])-a[2]};
            const auto x=ab[1]*ac[2]-ab[2]*ac[1],y=ab[2]*ac[0]-ab[0]*ac[2],z=ab[0]*ac[1]-ab[1]*ac[0];
            return x*x+y*y+z*z>1.0e-24;
        }

        bool append_wet_triangle(std::array<std::uint32_t,3> input,std::array<double,3> depths,
            std::vector<MeshVertex>& vertices,std::vector<std::uint32_t>& indices,bool omit_collapsed=false)
        {
            const auto polygon=water::wave::spherical_wet_polygon(depths);
            std::array<std::uint32_t,4> output{};
            for(std::size_t corner=0;corner<polygon.count;++corner)
            {
                const auto sample=polygon.corners[corner];const auto wet=input[sample.wet];
                if(!sample.crossing()){output[corner]=wet;continue;}
                auto boundary=sample_vertex(vertices[wet],vertices[input[sample.other]],sample.fraction);
                const auto& n=boundary.normal;
                const auto length=std::sqrt(double(n[0])*n[0]+double(n[1])*n[1]+double(n[2])*n[2]);
                if(length<=0.0)return false;
                for(auto& component:boundary.normal)component=static_cast<float>(component/length);
                output[corner]=static_cast<std::uint32_t>(vertices.size());
                vertices.push_back(boundary);
            }
            for(std::size_t i=1u;i+1u<polygon.count;++i)
            {
                const std::array corners{output[0],output[i],output[i+1u]};
                if((polygon.clipped||omit_collapsed)&&!usable_triangle(vertices,corners))continue;
                indices.insert(indices.end(),corners.begin(),corners.end());
            }
            return true;
        }

        using CubeRay=std::array<std::int32_t,3>;
        constexpr std::int32_t face_extent=1<<20;
        struct CubeTile final {std::int32_t u{},v{},span{};};

        CubeTile cube_tile(TileKey key) noexcept
        {
            const auto span=(2*face_extent)>>key.level;
            return {static_cast<std::int32_t>(key.x)*span-face_extent,
                static_cast<std::int32_t>(key.y)*span-face_extent,span};
        }

        std::array<CubeRay,4> cube_corners(TileKey key) noexcept
        {
            const auto tile=cube_tile(key);
            std::array<CubeRay,4> result{};
            for(unsigned i=0;i<result.size();++i)
            {
                const auto u=tile.u+static_cast<std::int32_t>(i&1u)*tile.span;
                const auto v=tile.v+static_cast<std::int32_t>(i>>1u)*tile.span;
                const std::array faces{CubeRay{face_extent,v,-u},CubeRay{-face_extent,v,u},
                    CubeRay{u,face_extent,-v},CubeRay{u,-face_extent,v},
                    CubeRay{u,v,face_extent},CubeRay{-u,v,-face_extent}};
                result[i]=faces[key.face];
            }
            return result;
        }

        std::optional<std::array<std::int32_t,2>> tile_coordinates(TileKey key,CubeRay ray) noexcept
        {
            const auto [x,y,z]=ray;
            // Integer face charts keep shared vertices identical across cube
            // faces without comparing rounded mesh positions to find topology.
            const std::array faces{CubeRay{x,-z,y},CubeRay{-x,z,y},
                CubeRay{y,x,-z},CubeRay{-y,x,z},CubeRay{z,x,y},CubeRay{-z,-x,y}};
            const auto [fixed,u,v]=faces[key.face];
            const auto tile=cube_tile(key);
            if(fixed!=face_extent||u<tile.u||u>tile.u+tile.span||v<tile.v||v>tile.v+tile.span)return {};
            return std::array{u-tile.u,v-tile.v};
        }

        bool touches_cube_edge(TileKey coarse,std::size_t edge,TileKey descendant) noexcept
        {
            // The neighbor lookup names the full adjacent tile. Only an
            // interval on this edge can restrict a merge. Exact cube rays
            // also cover reversed charts and exclude point contacts.
            const auto coordinate=(edge&1u)?cube_tile(coarse).span:0;
            unsigned contacts{};
            for(const auto corner:cube_corners(descendant))
            {
                const auto point=tile_coordinates(coarse,corner);
                if(point&&(*point)[edge/2u]==coordinate&&++contacts==2u)return true;
            }
            return false;
        }

        struct CornerSample final
        {
            std::array<float,3> ground{},water{};
            double depth{};
        };

        std::optional<CornerSample> wet_corner(const DrawPatch& draw,CubeRay ray,const Snapshot& target,
            std::span<const DrawPatch> cut,WetLatticeSamples* common,std::stop_token stop)
        {
            const auto& p=patch(draw);
            const auto& source=*draw.snapshot;
            const auto* range=water_range(source.metadata,p);
            const auto uv=tile_coordinates(p.key,ray);
            if(common&&uv&&target.water_sample_seconds&&source.water_sample_seconds!=target.water_sample_seconds)
            {
                const auto n=static_cast<std::int32_t>(common->cells),span=cube_tile(p.key).span;
                const auto u=(*uv)[0]*n,v=(*uv)[1]*n;
                if(u%span||v%span)return {};
                const auto lattice=ocean_lattice(source,p,common->cells,*target.water_sample_seconds,cut,target,common,stop);
                if(!lattice)return {};
                const auto index=static_cast<std::uint32_t>((v/span)*(n+1)+u/span);
                const auto sample=read_ocean(*lattice,index,target);
                if(!sample)return {};
                return CornerSample{sample->ground.position,sample->water.position,sample->depth};
            }
            if(!range||!uv||!target.water_sample_seconds||source.water_sample_seconds!=target.water_sample_seconds
                ||source.metadata.water_meters_to_artifact!=target.metadata.water_meters_to_artifact
                ||source.metadata.water_radius_meters!=target.metadata.water_radius_meters
                ||range->first_vertex<p.first_vertex)return {};
            const auto layer=range->first_vertex-p.first_vertex;
            const auto side_without_skirts=static_cast<std::uint32_t>(std::sqrt(double(layer)));
            const auto side_with_skirts=static_cast<std::uint32_t>(std::sqrt(double(layer)+8.0));
            const auto side=side_without_skirts*side_without_skirts==layer?side_without_skirts
                :side_with_skirts>=5u&&side_with_skirts*side_with_skirts==layer+8u?side_with_skirts-2u:0u;
            if(side<3u||side>65u||range->vertex_count<layer
                ||std::uint64_t(range->first_vertex)+range->vertex_count!=std::uint64_t(p.first_vertex)+p.vertex_count)return {};
            const auto n=static_cast<std::int32_t>(side-1u),span=cube_tile(p.key).span;
            const auto u=(*uv)[0]*n,v=(*uv)[1]*n;
            if(u%span||v%span)return {};
            const auto index=static_cast<std::uint32_t>(v/span)*side+static_cast<std::uint32_t>(u/span);
            const auto vertices=source.artifact->vertices();
            auto ground=vertices[p.first_vertex+index],surface=vertices[range->first_vertex+index];
            const auto m=source.metadata.water_meters_to_artifact;
            const auto& origin=source.metadata.water_origin_meters;
            const std::array center{-origin[0]*m,-origin[1]*m,-origin[2]*m};
            const auto depth=water::wave::spherical_shore_depth(ground.position,surface.position,center);
            if(!depth)return {};
            rebase_vertex(ground,source,target);rebase_vertex(surface,source,target);
            return CornerSample{ground.position,surface.position,*depth};
        }

        bool fixed_wet_corner(const DrawPatch& coarse,CubeRay ray,const std::shared_ptr<const Snapshot>& target,
            std::span<const DrawPatch> cut,WetLatticeSamples* common,std::stop_token stop)
        {
            if(!patch(coarse).key.level)return false;
            const auto ancestor=lookup(target,cut,parent(patch(coarse).key));
            if(!ancestor)return false;
            const auto a=wet_corner(coarse,ray,*target,cut,common,stop),b=wet_corner(*ancestor,ray,*target,cut,common,stop);
            if(!a||!b||a->water!=b->water||(a->depth>0.0)!=(b->depth>0.0))return false;
            // Local anchors are narrowed independently by the producer. Keep
            // their sub-millimetre ground rounding only when depth is stable.
            constexpr double maximum_rounding_meters=0.001;
            constexpr double maximum_relative_depth_change=0.01;
            const auto units=target->metadata.water_meters_to_artifact;
            return std::isfinite(units)&&units>0.0
                &&std::hypot(double(a->ground[0])-b->ground[0],double(a->ground[1])-b->ground[1],
                    double(a->ground[2])-b->ground[2])<=maximum_rounding_meters*units
                &&std::abs(a->depth-b->depth)<=maximum_relative_depth_change*(std::min)(std::abs(a->depth),std::abs(b->depth));
        }

        void remember_shoreline_spray(asset::mesh::SphericalWaterAppearance& appearance,
            const asset::mesh::Vertex& point,const std::array<double,3>& origin,
            double meters_to_artifact,double radius,float detail) noexcept
        {
            if(detail<=0.0f)return;
            std::array<double,3> body{};
            double local_squared{};
            for(std::size_t axis=0;axis<3u;++axis)
            {
                const double local=point.position[axis]/meters_to_artifact;
                local_squared+=local*local;
                body[axis]=origin[axis]+local;
            }
            const double distance=std::hypot(body[0],body[1],body[2]);
            const double crest=distance-radius;
            if(!std::isfinite(local_squared)||local_squared>75.0*75.0||!std::isfinite(crest)
                ||crest<=0.025||crest>4.0||distance<1.0||distance>1e12)return;
            double alignment{};
            std::uint64_t cell=14695981039346656037ull;
            for(std::size_t axis=0;axis<3u;++axis)
            {
                alignment+=point.normal[axis]*body[axis]/distance;
                cell^=static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(body[axis]*0.4)));
                cell*=1099511628211ull;
            }
            const float strength=detail*static_cast<float>(std::clamp(crest/0.45,0.0,1.0)
                *std::clamp((1.0-alignment)*8.0+0.25,0.25,1.0));
            for(std::size_t index=0;index<appearance.spray_count;++index)
                if(auto& current=appearance.spray_sites[index];current.cell==cell)
                {
                    if(strength>current.strength)current={point.position,cell,strength};
                    return;
                }
            if(appearance.spray_count<appearance.spray_sites.size())
                appearance.spray_sites[appearance.spray_count++]={point.position,cell,strength};
        }

        bool moving_wet_corner(const DrawPatch& draw,std::span<const DrawPatch> cut,
            const std::shared_ptr<const Snapshot>& target,WetLatticeSamples* common,std::stop_token stop)
        {
            const auto& p=patch(draw);
            if(!water_range(draw.snapshot->metadata,p))return false;
            for(const auto& other:cut)
            {
                const auto& q=patch(other);
                if(p.key.level==q.key.level||!water_range(other.snapshot->metadata,q))continue;
                const auto& coarse=p.key.level<q.key.level?draw:other;
                const auto& fine=p.key.level<q.key.level?q:p;
                if(coarse.morph==0.0f)continue;
                for(const auto ray:cube_corners(fine.key))
                    if(tile_coordinates(patch(coarse).key,ray)&&!fixed_wet_corner(coarse,ray,target,cut,common,stop))return true;
            }
            return false;
        }
    }

    std::uint64_t retained_snapshot_bytes(const Snapshot& snapshot) noexcept
    {
        return snapshot.artifact?bytes(snapshot):0u;
    }

    bool requires_water_edge_resample(const Snapshot& source) noexcept
    {
        const bool shore_visible=shore_tint(source.metadata)>0.0f;
        return std::ranges::any_of(source.metadata.patches,[&](const Patch& patch){
            if(!patch.target||!patch.terrain_vertices)return false;
            const auto* range=water_range(source.metadata,patch);
            if(!range)return false;
            const auto edges=water_edge_scales(source.metadata,patch,range->specular_scale,specular_scale);
            if(fades_at_edge(edges,range->specular_scale))return true;
            if(!shore_visible)return false;
            const float breaker=breaker_scale(source.metadata,patch,0.0f);
            return fades_at_edge(water_edge_scales(source.metadata,patch,breaker,breaker_scale),breaker);
        });
    }

    std::shared_ptr<const asset::mesh::Artifact> resample_water(const Snapshot& source,
        const water::wave::Tile& tile,Limits limits,std::stop_token stop,
        std::shared_ptr<const asset::mesh::SphericalWaterAppearance>* appearance) noexcept
    {
        try
        {
            if(stop.stop_requested()||!source.artifact||source.metadata.water_vertices.empty()
                ||!valid_metadata(source.metadata,*source.artifact,limits))return {};
            using Point=std::array<double,3>;
            const auto sub=[](Point a,Point b){return Point{a[0]-b[0],a[1]-b[1],a[2]-b[2]};};
            const auto cross=[](Point a,Point b){return Point{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
            const auto dot=[](Point a,Point b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
            const auto original=source.artifact->vertices();
            const auto original_indices=source.artifact->indices();
            std::vector<asset::mesh::Vertex> vertices;std::vector<std::uint32_t> indices;
            std::uint64_t maximum_vertices{},maximum_indices{};
            for(const auto& patch:source.metadata.patches)if(patch.target)
            {
                maximum_vertices+=patch.terrain_vertices?2ull*patch.terrain_vertices+2ull*patch.water_triangles:patch.vertex_count;
                maximum_indices+=patch.terrain_vertices?std::uint64_t(patch.terrain_indices)+6ull*patch.water_triangles:patch.index_count;
                if(maximum_vertices>limits.artifact.maximum_vertices||maximum_indices>limits.artifact.maximum_indices)return {};
            }
            auto rendered=appearance&&source.metadata.water_appearance
                ? std::make_shared<asset::mesh::SphericalWaterAppearance>() : nullptr;
            if(rendered)rendered->ranges.reserve(source.metadata.water_appearance->ranges.size());
            const auto appearance_bytes=rendered?sizeof(*rendered)+rendered->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange):0u;
            const auto maximum_output=maximum_vertices*sizeof(asset::mesh::Vertex)+maximum_indices*4u+appearance_bytes;
            if(48u+maximum_vertices*44u+maximum_indices*4u>limits.artifact.maximum_bytes
                ||2u*maximum_output+tile.payload_bytes()>limits.maximum_transient_bytes
                ||bytes(source)+2u*maximum_output+tile.payload_bytes()>limits.maximum_retained_bytes)return {};
            // Reserve the complete worst-case clipping expansion only after
            // admission. No vector growth can exceed the worker envelope.
            vertices.reserve(static_cast<std::size_t>(maximum_vertices));
            indices.reserve(static_cast<std::size_t>(maximum_indices));
            const auto admitted=[&] {
                const std::uint64_t output=vertices.capacity()*sizeof(asset::mesh::Vertex)+indices.capacity()*4u+appearance_bytes;
                return vertices.size()<=limits.artifact.maximum_vertices&&indices.size()<=limits.artifact.maximum_indices
                    &&48u+vertices.size()*44u+indices.size()*4u<=limits.artifact.maximum_bytes
                    &&2u*output+tile.payload_bytes()<=limits.maximum_transient_bytes
                    &&bytes(source)+2u*output+tile.payload_bytes()<=limits.maximum_retained_bytes;
            };
            const float shore_tint_strength=shore_tint(source.metadata);
            for(const auto& p:source.metadata.patches)
            {
                if(!p.target)continue;
                if(stop.stop_requested())return {};
                const auto base=static_cast<std::uint32_t>(vertices.size());
                const auto ground_count=p.terrain_vertices?p.terrain_vertices:p.vertex_count;
                const auto ground_indices=p.terrain_vertices?p.terrain_indices:p.index_count;
                vertices.insert(vertices.end(),original.begin()+p.first_vertex,original.begin()+p.first_vertex+ground_count);
                for(std::uint32_t i=0;i<ground_indices;++i)
                    indices.push_back(base+original_indices[p.first_index+i]-p.first_vertex);
                if(!admitted())return {};
                const auto* range=rendered?water_range(source.metadata,p):nullptr;
                if(!p.terrain_vertices)
                {
                    if(range)rendered->ranges.push_back({base+range->first_vertex-p.first_vertex,range->vertex_count,range->specular_scale});
                    continue;
                }
                const auto water_base=static_cast<std::uint32_t>(vertices.size());
                const double ocean_radius = source.metadata.water_radius_meters;
                const double scale = source.metadata.water_meters_to_artifact;
                const auto& ocean_origin = source.metadata.water_origin_meters;
                const double spacing_meters=water_lattice_spacing(source.metadata,p);
                const double wave_length_meters = tile.length_meters();
                std::uint32_t grid_vertices{};
                while(grid_vertices<p.terrain_vertices
                    &&source.metadata.water_vertices[p.first_water_vertex+grid_vertices].surface_vertex
                        ==p.first_water_vertex+grid_vertices)++grid_vertices;
                const auto grid_side=static_cast<std::uint32_t>(std::sqrt(double(grid_vertices)));
                const bool grid_layout=grid_side>=3u&&grid_side*grid_side==grid_vertices
                    &&(p.terrain_vertices==grid_vertices||p.terrain_vertices==grid_vertices+4u*(grid_side-1u));
                const auto edge_scale=range&&grid_layout
                    ?water_edge_scales(source.metadata,p,range->specular_scale,specular_scale):std::array<float,4>{};
                const bool blend_edges=range&&grid_layout&&fades_at_edge(edge_scale,range->specular_scale);
                const float breaker=shore_tint_strength>0.0f?water::wave::spherical_breaker_scale(spacing_meters):0.0f;
                const auto breaker_edges=grid_layout
                    ?water_edge_scales(source.metadata,p,breaker,breaker_scale):std::array<float,4>{};
                const bool blend_breaker=grid_layout&&fades_at_edge(breaker_edges,breaker);
                for(std::uint32_t i=0;i<p.terrain_vertices;++i)
                {
                    if(stop.stop_requested())return {};
                    const auto& basis=source.metadata.water_vertices[p.first_water_vertex+i];
                    const auto sampled=water::wave::spherical_sample(tile,basis.direction,
                        source.metadata.water_radius_meters,tile.seconds());
                    if(!sampled)return {};
                    auto value=original[p.first_vertex+p.terrain_vertices+i];
                    auto point=sampled->position_meters;const auto radius=std::sqrt(dot(point,point));
                    if(!std::isfinite(radius)||radius<=0.0)return {};
                    for(unsigned axis=0;axis<3u;++axis)
                        value.position[axis]=static_cast<float>((point[axis]-point[axis]/radius*basis.skirt_depth_meters-source.metadata.water_origin_meters[axis])
                            *source.metadata.water_meters_to_artifact);
                    const auto& ground = vertices[base+basis.surface_vertex-p.first_water_vertex].position;
                    const double bed_radius = std::hypot(ocean_origin[0]+ground[0]/scale,
                        ocean_origin[1]+ground[1]/scale, ocean_origin[2]+ground[2]/scale);
                    float breaker_weight=breaker;
                    if(blend_edges||blend_breaker)
                    {
                        const auto index=basis.surface_vertex-p.first_water_vertex;
                        if(index>=grid_vertices)return {};
                        const auto x=index%grid_side,y=index/grid_side,last=grid_side-1u;
                        if(blend_edges)value.uv[0]=water::wave::spherical_water_edge_weight(edge_scale,range->specular_scale,x,y,last);
                        if(blend_breaker)breaker_weight=water::wave::spherical_water_edge_weight(breaker_edges,breaker,x,y,last);
                    }
                    value.color=water::wave::spherical_surface_color(ocean_radius-bed_radius,
                        sampled->foam,spacing_meters,wave_length_meters,false,shore_tint_strength,
                        radius-ocean_radius,breaker_weight);
                    value.normal=water::wave::spherical_shoaled_normal(sampled->normal,basis.direction,
                        ocean_radius-bed_radius,radius-ocean_radius,breaker_weight*shore_tint_strength);
                    vertices.push_back(value);
                }
                if(!admitted())return {};
                const auto pos=[&](std::uint32_t id){const auto& point=vertices[id].position;return Point{point[0],point[1],point[2]};};
                const std::array body_center{-ocean_origin[0]*scale,-ocean_origin[1]*scale,-ocean_origin[2]*scale};
                for(std::uint32_t t=0;t<p.water_triangles;++t)
                {
                    if(stop.stop_requested())return {};
                    const auto& triangle=source.metadata.water_triangles[p.first_water_triangle+t];
                    std::array<std::uint32_t,3> input{};
                    std::array<double,3> depths{};
                    for(unsigned i=0;i<3u;++i)
                    {
                        const auto surface=source.metadata.water_vertices[triangle.water[i]].surface_vertex-p.first_water_vertex;
                        const auto depth=water::wave::spherical_shore_depth(vertices[base+surface].position,
                            vertices[water_base+surface].position,body_center);
                        if(!depth)return {};
                        input[i]=water_base+triangle.water[i]-p.first_water_vertex;
                        depths[i]=*depth;
                    }
                    const auto polygon=water::wave::spherical_wet_polygon(depths);
                    const bool surface=rendered&&range&&breaker>0.0f&&polygon.clipped
                        &&std::ranges::all_of(triangle.water,[&](auto index){
                            return source.metadata.water_vertices[index].surface_vertex==index;});
                    std::array<std::uint32_t,4> output{};
                    for(std::size_t corner=0;corner<polygon.count;++corner)
                    {
                        const auto sample=polygon.corners[corner];
                        const auto wet=input[sample.wet];
                        if(!sample.crossing())
                        {
                            output[corner]=wet;
                            continue;
                        }
                        const auto av=vertices[wet],bv=vertices[input[sample.other]];
                        const double fraction=sample.fraction;
                        asset::mesh::Vertex boundary;
                        double length{};
                        for(unsigned axis=0;axis<3u;++axis)
                        {
                            boundary.position[axis]=static_cast<float>(std::lerp(double(av.position[axis]),double(bv.position[axis]),fraction));
                            boundary.normal[axis]=static_cast<float>(std::lerp(double(av.normal[axis]),double(bv.normal[axis]),fraction));
                            // Match provider clipping so animation does not repaint the shoreline.
                            boundary.color[axis]=static_cast<float>(std::lerp(double(av.color[axis]),double(bv.color[axis]),fraction));
                            length+=double(boundary.normal[axis])*boundary.normal[axis];
                        }
                        length=std::sqrt(length);if(length<=0.0)return {};
                        for(auto& component:boundary.normal)component=static_cast<float>(component/length);
                        for(unsigned axis=0;axis<2u;++axis)boundary.uv[axis]=static_cast<float>(std::lerp(double(av.uv[axis]),double(bv.uv[axis]),fraction));
                        output[corner]=static_cast<std::uint32_t>(vertices.size());
                        vertices.push_back(boundary);
                        if(surface)remember_shoreline_spray(*rendered,boundary,ocean_origin,scale,ocean_radius,breaker);
                    }
                    for(std::size_t i=1u;i+1u<polygon.count;++i)
                    {
                        if(polygon.clipped)
                        {
                            const auto area=cross(sub(pos(output[i]),pos(output[0])),sub(pos(output[i+1u]),pos(output[0])));
                            if(dot(area,area)<=1e-24)continue;
                        }
                        indices.insert(indices.end(),{output[0],output[i],output[i+1u]});
                    }
                    if(!admitted())return {};
                }
                if(range)rendered->ranges.push_back({water_base,static_cast<std::uint32_t>(vertices.size()-water_base),range->specular_scale,blend_edges});
            }
            const auto result=asset::mesh::build_artifact(vertices,indices,limits.artifact);
            if(stop.stop_requested()||!result)return {};
            if(rendered)rendered->artifact_key=result.artifact->key();
            if(appearance)*appearance=std::move(rendered);
            return result.artifact;
        }
        catch(...){return {};}
    }

    bool Transition::admit(std::shared_ptr<const Snapshot> incoming,std::stop_token stop,
        InitialCut initial_cut) noexcept
    {
        if(stop.stop_requested()||!incoming||!incoming->artifact||!valid_metric_origin(incoming->metric_origin_meters)
            ||!valid_metadata(incoming->metadata,*incoming->artifact,limits_))return false;
        return admit_validated(std::move(incoming),stop,initial_cut);
    }

    DecodedSnapshot::DecodedSnapshot(Snapshot snapshot, Limits limits) noexcept
        : snapshot_(std::move(snapshot)), limits_(limits) {}

    std::optional<DecodedSnapshot> DecodedSnapshot::read(std::span<const std::byte> bytes,
        std::shared_ptr<const asset::mesh::Artifact> artifact, const Identity& expected, Limits limits,
        std::stop_token stop, std::optional<std::array<double,3>> metric_origin) noexcept
    {
        if(stop.stop_requested()||!artifact||!valid_metric_origin(metric_origin))return {};
        auto metadata=decode(bytes,*artifact,expected,limits,stop);
        if(!metadata)return {};
        return DecodedSnapshot{Snapshot{std::move(artifact),std::move(*metadata),metric_origin},limits};
    }

    bool DecodedSnapshot::read_water_appearance(std::span<const std::byte> bytes,std::stop_token stop) noexcept
    {
        if(!snapshot_.artifact)return false;
        auto appearance=decode_water_appearance(bytes,*snapshot_.artifact,snapshot_.metadata,stop);
        if(!appearance)return false;
        snapshot_.metadata.water_appearance=std::move(appearance);
        return true;
    }

    bool DecodedSnapshot::normalize_cloud_center(const std::array<double,3>& world_source_origin) noexcept
    {
        if(!snapshot_.artifact||!snapshot_.metadata.cloud||!snapshot_.metric_origin_meters)return false;
        auto& center=snapshot_.metadata.cloud->center_meters;
        for(std::size_t axis=0;axis<3u;++axis)
            if(center[axis]+(*snapshot_.metric_origin_meters)[axis]!=world_source_origin[axis])return false;
        center={};
        return true;
    }

    SelectedCut DecodedSnapshot::materialize_selected(std::stop_token stop,
        std::optional<double> verified_water_seconds) && noexcept
    {
        try
        {
            if(stop.stop_requested()||!snapshot_.artifact)return {};
            if(verified_water_seconds&&!std::isfinite(*verified_water_seconds))return {};
            snapshot_.water_sample_seconds=verified_water_seconds;
            auto source=std::make_shared<const Snapshot>(std::move(snapshot_));
            Transition initial{limits_};
            if(!initial.admit_validated(source,stop,InitialCut::selected_patches))return {};
            std::shared_ptr<const asset::mesh::SphericalWaterAppearance> appearance;
            auto artifact=initial.materialize(stop,&appearance);
            if(!artifact)return {};
            return {std::move(source),std::move(artifact),std::move(appearance)};
        }
        catch(...){return {};}
    }

    bool Transition::admit_validated(std::shared_ptr<const Snapshot> incoming,std::stop_token stop,
        InitialCut initial_cut) noexcept
    {
        try
        {
            if(stop.stop_requested())return false;
            auto next=active_;
            if(target_)
            {
                const auto& old=target_->metadata.identity;const auto& current=incoming->metadata.identity;
                if(old.source_hash!=current.source_hash||old.recipe_key!=current.recipe_key||current.request_id<=old.request_id)return false;
                if(target_->metadata.patches.front().key.body!=incoming->metadata.patches.front().key.body)return false;
                if(target_->metric_origin_meters.has_value()!=incoming->metric_origin_meters.has_value())return false;
                // Rebind only by exact hierarchical identity. Never match flat
                // vertex offsets between two different cuts.
                for(auto& draw:next)if(const auto index=find(incoming->metadata,patch(draw).key))
                {draw.snapshot=incoming;draw.patch=*index;}
            }
            else
                for(std::uint32_t i=0;i<incoming->metadata.patches.size();++i)
                    if(initial_cut==InitialCut::selected_patches ? incoming->metadata.patches[i].target
                        : incoming->metadata.patches[i].key.level==0u)next.push_back({incoming,i,0.0f});
            if(!budget(next,incoming,limits_)||stop.stop_requested())return false;
            wet_samples_.reset();target_=std::move(incoming);active_=std::move(next);return true;
        }
        catch(...){return false;}
    }
    bool Transition::advance(double elapsed,bool paused,std::stop_token stop) noexcept
    {
        return advance_cut(elapsed,paused,stop,false,nullptr);
    }
    bool Transition::advance_wet(double elapsed,bool paused,std::stop_token stop,WetTransitionFailure* failure) noexcept
    {
        return advance_cut(elapsed,paused,stop,true,failure);
    }

    bool Transition::needs_retained_water_sample() const noexcept
    {
        if(!target_||!target_->water_sample_seconds)return false;
        return std::ranges::any_of(active_,[&](const DrawPatch& draw){
            return draw.snapshot!=target_&&draw.snapshot->water_sample_seconds
                &&draw.snapshot->water_sample_seconds!=target_->water_sample_seconds;
        });
    }

    bool Transition::prepare_retained_water(std::uint32_t cells,std::shared_ptr<const water::wave::Tile> tile,
        std::shared_ptr<const WaterLatticeProvider> provider,std::uint64_t retained_display_bytes,
        std::stop_token stop) noexcept
    {
        try
        {
            if(stop.stop_requested()||cells<2u||cells>64u||!tile||!target_||tile->seconds()!=target_->water_sample_seconds
                ||!needs_retained_water_sample()||!budget(active_,target_,limits_)
                ||retained_display_bytes>=limits_.maximum_retained_bytes)return false;
            auto room=limits_.maximum_retained_bytes-retained_display_bytes;
            const auto charge=[&](std::uint64_t cost){if(cost>room)return false;room-=cost;return true;};
            if(!charge(bytes(*target_)))return false;
            const auto prior=std::ranges::find_if(active_,[&](const auto& draw){return draw.snapshot!=target_;});
            if(prior==active_.end()||!charge(bytes(*prior->snapshot)))return false;
            room=(std::min)(room,limits_.maximum_transient_bytes);
            const auto cost=tile->payload_bytes()+sizeof(WetLatticeSamples)+256u;
            if(cost>room)return false;
            wet_samples_=std::make_shared<WetLatticeSamples>(WetLatticeSamples{
                std::move(tile),std::move(provider),{},cost,room,cells});
            return true;
        }
        catch(...){return false;}
    }

    bool Transition::advance_cut(double elapsed,bool paused,std::stop_token stop,bool wet,WetTransitionFailure* failure) noexcept
    {
        if(failure)*failure={};
        const auto reject=[&](std::string_view reason,std::optional<TileKey> key={}){
            if(failure)*failure={reason,key};
            return false;
        };
        if(stop.stop_requested())return reject("cancelled");
        if(!target_||!std::isfinite(elapsed)||elapsed<0.0)return reject("invalid_input");
        if(paused||elapsed==0.0)return true;
        try
        {
            auto next=active_;
            WetTransitionFailure blocked;
            const auto amount=static_cast<float>((std::min)(elapsed,0.1)/0.20);
            // Reverse direction from the existing phase on retarget. Coarsening
            // cannot start until all four immediate siblings are leaves.
            for(auto& draw:next)
            {
                const auto key=patch(draw).key;
                bool coarsen=key.level&&wants_coarse(target_->metadata,key);
                if(coarsen)
                {
                    const auto siblings=children(parent(key));
                    for(const auto sibling:siblings)
                        if(!std::ranges::any_of(next,[&](const DrawPatch& other){return patch(other).key==sibling;}))coarsen=false;
                    const auto p=lookup(target_,next,parent(key));
                    if(!p||!balanced_coarsen(next,patch(*p)))coarsen=false;
                    if(wet&&coarsen&&std::ranges::any_of(active_,[&](auto sibling){
                        if(std::ranges::find(siblings,patch(sibling).key)==siblings.end())return false;
                        sibling.morph=1.0f;
                        return moving_wet_corner(sibling,active_,target_,wet_samples_.get(),stop);
                    }))
                    {
                        coarsen=false;
                        blocked={"moving_wet_corner",key};
                    }
                }
                draw.morph=std::clamp(draw.morph+(coarsen?amount:-amount),0.0f,1.0f);
            }
            // Remove a quartet only after every vertex reaches its own parent
            // triangle. An interrupted quartet remains complete and reversible.
            for(std::size_t i=0;i<next.size();)
            {
                const auto key=patch(next[i]).key;
                if(!key.level||next[i].morph!=1.0f||!wants_coarse(target_->metadata,key)){++i;continue;}
                const auto p=parent(key);const auto siblings=children(p);const auto replacement=lookup(target_,next,p);
                bool complete=bool(replacement);
                for(const auto sibling:siblings)complete=complete&&std::ranges::any_of(next,[&](const DrawPatch& d){return patch(d).key==sibling&&d.morph==1.0f;});
                if(!complete){++i;continue;}
                std::erase_if(next,[&](const DrawPatch& d){return contains(p,patch(d).key);});
                next.push_back(*replacement);i=0u;
            }
            // New child quartets enter at the parent endpoint and do not also
            // consume this frame's advance. Deep refinement is sequential.
            const auto before=next;
            for(const auto& old:before)
            {
                const auto key=patch(old).key;
                if(old.morph!=0.0f||key.level>=20u||!wants_split(target_->metadata,key))continue;
                const auto frontier=wet?std::span<const DrawPatch>{before}:std::span<const DrawPatch>{next};
                if(!balanced_split(frontier,patch(old)))continue;
                if(wet&&std::ranges::any_of(patch(old).neighbors,[&](auto edge){
                    return std::ranges::any_of(before,[&](const auto& neighbor){
                        return neighbor.morph>0.0f&&contains(patch(neighbor).key,edge);
                    });
                }))continue;
                std::array<std::uint32_t,4> indices{};bool complete=true;unsigned j{};
                for(const auto child:children(key))
                {const auto index=find(target_->metadata,child);if(!index){complete=false;break;}indices[j++]=*index;}
                if(!complete)continue;
                auto candidate=next;
                std::erase_if(candidate,[&](const DrawPatch& d){return patch(d).key==key;});
                for(const auto index:indices)candidate.push_back({target_,index,1.0f});
                if(wet&&std::ranges::any_of(std::span<const DrawPatch>{candidate}.last(indices.size()),[&](const auto& added){
                    return moving_wet_corner(added,candidate,target_,wet_samples_.get(),stop);
                }))
                {
                    blocked={"moving_wet_corner",key};
                    continue;
                }
                if(budget(candidate,target_,limits_))next=std::move(candidate);
                else if(blocked.reason.empty())blocked={"advance_budget",key};
            }
            if(stop.stop_requested())return reject("cancelled");
            if(!budget(next,target_,limits_))return reject("advance_budget");
            if(wet&&!settled()&&std::ranges::equal(active_,next,[](const auto& a,const auto& b){
                return a.snapshot==b.snapshot&&a.patch==b.patch&&a.morph==b.morph;
            }))return reject(blocked.reason.empty()?"wet_advance_no_progress":blocked.reason,blocked.patch);
            active_=std::move(next);return true;
        }
        catch(...){return reject("advance_exception");}
    }
    bool Transition::settled() const noexcept
    {
        if(!target_)return false;
        for(const auto& d:active_)
        {const auto index=find(target_->metadata,patch(d).key);if(d.morph!=0.0f||!index||!target_->metadata.patches[*index].target)return false;}
        std::size_t desired{};for(const auto& p:target_->metadata.patches)desired+=p.target?1u:0u;
        return desired==active_.size();
    }
    std::shared_ptr<const asset::mesh::Artifact> Transition::materialize(std::stop_token stop,
        std::shared_ptr<const asset::mesh::SphericalWaterAppearance>* appearance) const noexcept
    {
        try
        {
            if(stop.stop_requested()||!target_||!budget(active_,target_,limits_))return {};
            std::size_t nv{},ni{};for(const auto& d:active_){nv+=patch(d).vertex_count;ni+=patch(d).index_count;}
            std::vector<asset::mesh::Vertex> vertices;std::vector<std::uint32_t> indices;vertices.reserve(nv);indices.reserve(ni);
            auto rendered=appearance&&std::ranges::any_of(active_,[](const auto& draw){return bool(draw.snapshot->metadata.water_appearance);})
                ? std::make_shared<asset::mesh::SphericalWaterAppearance>() : nullptr;
            if(rendered)rendered->ranges.reserve(active_.size());
            for(const auto& d:active_)
            {
                if(stop.stop_requested())return {};
                const auto& p=patch(d);const auto base=static_cast<std::uint32_t>(vertices.size());
                const auto sourceVertices=d.snapshot->artifact->vertices().subspan(p.first_vertex,p.vertex_count);
                const auto sourceIndices=d.snapshot->artifact->indices().subspan(p.first_index,p.index_count);
                if(d.snapshot->metric_origin_meters!=target_->metric_origin_meters)
                {
                    const auto& from=*d.snapshot->metric_origin_meters;
                    const auto& to=*target_->metric_origin_meters;
                    const std::array offset{from[0]-to[0],from[1]-to[1],from[2]-to[2]};
                    for(std::uint32_t i=0;i<p.vertex_count;++i)
                    {
                        auto vertex=sourceVertices[i];
                        const auto& delta=d.snapshot->metadata.parent_delta[p.first_vertex+i];
                        for(unsigned axis=0;axis<3u;++axis)
                            vertex.position[axis]=static_cast<float>(double(vertex.position[axis])+offset[axis]
                                +double(delta[axis])*d.morph);
                        vertices.push_back(vertex);
                    }
                }
                else if(d.morph==0.0f)
                    vertices.insert(vertices.end(),sourceVertices.begin(),sourceVertices.end());
                else for(std::uint32_t i=0;i<p.vertex_count;++i)
                {
                    auto vertex=sourceVertices[i];
                    const auto delta=d.snapshot->metadata.parent_delta[p.first_vertex+i];
                    for(unsigned axis=0;axis<3u;++axis)vertex.position[axis]+=delta[axis]*d.morph;
                    vertices.push_back(vertex);
                }
                for(const auto index:sourceIndices)indices.push_back(base+index-p.first_vertex);
                if(rendered)if(const auto* range=water_range(d.snapshot->metadata,p))
                    rendered->ranges.push_back({base+range->first_vertex-p.first_vertex,range->vertex_count,range->specular_scale});
            }
            const auto result=asset::mesh::build_artifact(vertices,indices,limits_.artifact);
            if(stop.stop_requested()||!result)return {};
            if(rendered)rendered->artifact_key=result.artifact->key();
            if(appearance)*appearance=std::move(rendered);
            return result.artifact;
        }
        catch(...){return {};}
    }

    std::shared_ptr<const asset::mesh::Artifact> Transition::materialize_wet(std::uint32_t cells,
        double sampled_seconds,std::stop_token stop,std::shared_ptr<const asset::mesh::SphericalWaterAppearance>* appearance,
        std::uint64_t retained_display_bytes,WetTransitionFailure* failure) const noexcept
    {
        if(failure)*failure={};
        const auto reject=[&](std::string_view reason,std::optional<TileKey> key={})
            -> std::shared_ptr<const asset::mesh::Artifact> {
                if(failure)*failure={reason,key};
                return {};
            };
        try
        {
            if(stop.stop_requested())return reject("cancelled");
            if(!target_||cells<2u||cells>64u||!std::isfinite(sampled_seconds))return reject("invalid_input");
            if(retained_display_bytes>=limits_.maximum_retained_bytes||!budget(active_,target_,limits_))
                return reject("retained_budget");
            struct Plan final
            {
                const DrawPatch* draw{};
                std::optional<OceanLattice> fine,parent;
                std::array<float,4> edge_phase{};
            };
            const auto side=cells+1u,grid=side*side;
            const std::uint64_t scratch=active_.size()*sizeof(Plan)+grid*sizeof(double);
            if(scratch>=limits_.maximum_transient_bytes)return reject("scratch_budget");
            std::vector<Plan> plans;
            plans.reserve(active_.size());
            std::uint64_t vertex_count{},index_count{};
            std::array<const Snapshot*,2> retained{target_.get(),nullptr};
            auto retained_bytes=bytes(*target_);
            for(const auto& draw:active_)
            {
                if(stop.stop_requested())return reject("cancelled");
                const auto& source=*draw.snapshot;
                if(&source!=retained[0] && &source!=retained[1])
                {
                    if(retained[1])return reject("too_many_snapshots");
                    retained[1]=&source;
                    retained_bytes+=bytes(source);
                }
                const auto& p=patch(draw);
                Plan plan{};
                plan.draw=&draw;
                const bool wet=water_range(source.metadata,p)!=nullptr;
                bool needs_parent=draw.morph>0.0f;
                if(wet||wet_samples_)
                {
                    for(std::size_t edge=0;edge<plan.edge_phase.size();++edge)
                    {
                        const auto key=p.neighbors[edge];
                        const auto neighbor=std::ranges::find_if(active_,[&](const auto& candidate){
                            return contains(patch(candidate).key,key);
                        });
                        if(neighbor==active_.end()||!water_range(neighbor->snapshot->metadata,patch(*neighbor)))continue;
                        const auto other=patch(*neighbor).key;
                        if(other.level==key.level)plan.edge_phase[edge]=neighbor->morph;
                        else
                        {
                            // A static coarse edge meets the fine patch's
                            // parent endpoint. The fine edge and skirt then
                            // relax together onto their ordinary settled cut.
                            // Two moving levels require a common grandparent.
                            if(wet&&other.level+1u!=p.key.level)return reject("neighbor_level_step",p.key);
                            if(wet&&neighbor->morph!=0.0f)return reject("moving_coarse_neighbor",p.key);
                        }
                        needs_parent=needs_parent||plan.edge_phase[edge]>0.0f;
                    }
                }
                const auto outer=needs_parent&&p.key.level?lookup(target_,active_,parent(p.key)):std::nullopt;
                const bool wet_parent=outer&&water_range(outer->snapshot->metadata,patch(*outer));
                if(wet||(wet_samples_&&wet_parent))
                {
                    if(source.water_sample_seconds!=sampled_seconds&&!wet_samples_)
                        return reject("retained_clock_mismatch",p.key);
                    plan.fine=ocean_lattice(source,p,cells,sampled_seconds,active_,*target_,wet_samples_.get(),stop);
                    if(!plan.fine)return reject("fine_lattice_invalid",p.key);
                    if(needs_parent)
                    {
                        if(!p.key.level)return reject("parent_of_root",p.key);
                        if(!outer)return reject("missing_parent",p.key);
                        if(outer->snapshot->water_sample_seconds!=sampled_seconds&&!wet_samples_)
                            return reject("parent_clock_mismatch",p.key);
                        plan.parent=ocean_lattice(*outer->snapshot,patch(*outer),cells,sampled_seconds,active_,
                            *target_,wet_samples_.get(),stop);
                        if(!plan.parent||plan.parent->ground.size()!=plan.fine->ground.size()
                            ||plan.parent->source->metadata.water_radius_meters!=source.metadata.water_radius_meters
                            ||plan.parent->source->metadata.water_meters_to_artifact!=source.metadata.water_meters_to_artifact)
                            return reject("parent_lattice_invalid",p.key);
                    }
                    const auto topology=plan.fine->ground_indices.size();
                    vertex_count+=2u*plan.fine->ground.size()+2u*(topology/3u);
                    index_count+=3u*topology;
                }
                else
                {
                    if(p.terrain_vertices||(!source.metadata.water_appearance&&has_water(source.metadata)))
                        return reject("missing_water_appearance",p.key);
                    vertex_count+=p.vertex_count;
                    index_count+=p.index_count;
                }
                plans.push_back(std::move(plan));
            }
            const auto appearance_bytes=appearance
                ?sizeof(asset::mesh::SphericalWaterAppearance)+plans.size()*sizeof(WaterRange):0u;
            const auto output=vertex_count*sizeof(MeshVertex)+index_count*sizeof(std::uint32_t)+appearance_bytes;
            if(vertex_count>limits_.artifact.maximum_vertices||index_count>limits_.artifact.maximum_indices
                ||48u+vertex_count*44u+index_count*4u>limits_.artifact.maximum_bytes)
                return reject("artifact_budget");
            const auto sampled_bytes=wet_samples_?wet_samples_->retained_bytes:0u;
            if(2u*output+scratch+sampled_bytes>limits_.maximum_transient_bytes)return reject("output_scratch_budget");
            if(retained_bytes+2u*output+scratch+sampled_bytes>limits_.maximum_retained_bytes-retained_display_bytes)
                return reject("output_retained_budget");

            std::vector<MeshVertex> vertices;
            std::vector<std::uint32_t> indices;
            vertices.reserve(static_cast<std::size_t>(vertex_count));
            indices.reserve(static_cast<std::size_t>(index_count));
            std::vector<double> depth(grid);
            auto rendered=appearance?std::make_shared<asset::mesh::SphericalWaterAppearance>():nullptr;
            if(rendered)rendered->ranges.reserve(plans.size());
            for(const auto& plan:plans)
            {
                if(stop.stop_requested())return reject("cancelled");
                const auto& draw=*plan.draw;
                const auto& source=*draw.snapshot;
                const auto& p=patch(draw);
                const auto base=static_cast<std::uint32_t>(vertices.size());
                if(!plan.fine)
                {
                    const auto original=source.artifact->vertices().subspan(p.first_vertex,p.vertex_count);
                    for(std::uint32_t i=0;i<p.vertex_count;++i)
                    {
                        auto value=original[i];
                        const auto delta=source.metadata.parent_delta[p.first_vertex+i];
                        if(source.metric_origin_meters!=target_->metric_origin_meters)
                            for(unsigned axis=0;axis<3u;++axis)
                                value.position[axis]=static_cast<float>(double(value.position[axis])
                                    +((*source.metric_origin_meters)[axis]-(*target_->metric_origin_meters)[axis])
                                    +double(delta[axis])*draw.morph);
                        else for(unsigned axis=0;axis<3u;++axis)value.position[axis]+=delta[axis]*draw.morph;
                        vertices.push_back(value);
                    }
                    for(const auto index:source.artifact->indices().subspan(p.first_index,p.index_count))
                        indices.push_back(base+index-p.first_vertex);
                    continue;
                }

                const auto layer=static_cast<std::uint32_t>(plan.fine->ground.size());
                vertices.resize(vertices.size()+2u*layer);
                const auto surface_index=[&](std::uint32_t i){return i<grid?i:perimeter_top(i-grid,cells);};
                for(std::uint32_t i=0;i<layer;++i)
                {
                    if(stop.stop_requested())return reject("cancelled");
                    const auto top=surface_index(i),x=top%side,y=top/side;
                    auto phase=draw.morph;
                    if(x==0u)phase=(std::max)(phase,plan.edge_phase[0]);
                    if(x==cells)phase=(std::max)(phase,plan.edge_phase[1]);
                    if(y==0u)phase=(std::max)(phase,plan.edge_phase[2]);
                    if(y==cells)phase=(std::max)(phase,plan.edge_phase[3]);
                    auto value=read_ocean(*plan.fine,i,*target_);
                    if(!value)return reject("fine_sample_invalid",p.key);
                    if(phase>0.0f)
                    {
                        const auto parent_sample=parent_ocean(*plan.parent,in_parent(p.key,top,cells),i>=grid,*target_);
                        if(!parent_sample)return reject("parent_sample_invalid",p.key);
                        value=mix_ocean(*value,*parent_sample,phase);
                    }
                    if(i<grid)depth[i]=value->depth;
                    vertices[base+i]=value->ground;
                    vertices[base+layer+i]=value->water;
                }
                const auto water_base=base+layer;
                const auto water_indices_begin=indices.size();
                const auto ground=plan.fine->ground_indices;
                const auto is_hidden_skirt=[&](std::size_t index){
                    if(draw.morph!=1.0f||index<6u*cells*cells)return false;
                    const auto edge=(index-6u*cells*cells)/(6u*cells);
                    const auto quadrant=edge%2u?p.key.x&1u:p.key.y&1u;
                    return quadrant!=(edge==1u||edge==2u);
                };
                // Preserve the contiguous ground and water triangle runs used
                // by the renderer's bounded water detail range assembly.
                for(std::size_t i=0;i<ground.size();i+=3u)
                {
                    if(stop.stop_requested())return reject("cancelled");
                    if(is_hidden_skirt(i))continue;
                    const std::array corners{base+ground[i]-p.first_vertex,base+ground[i+1]-p.first_vertex,
                        base+ground[i+2]-p.first_vertex};
                    if(i<6u*cells*cells||usable_triangle(vertices,corners))
                        indices.insert(indices.end(),corners.begin(),corners.end());
                }
                for(std::size_t i=0;i<ground.size();i+=3u)
                {
                    if(stop.stop_requested())return reject("cancelled");
                    if(is_hidden_skirt(i))continue;
                    const std::array local{ground[i]-p.first_vertex,ground[i+1]-p.first_vertex,ground[i+2]-p.first_vertex};
                    const bool skirt=i>=6u*cells*cells;
                    const std::array water_corners{water_base+local[0],water_base+local[1],water_base+local[2]};
                    const std::array wet_depth{depth[surface_index(local[0])],depth[surface_index(local[1])],depth[surface_index(local[2])]};
                    const auto first_boundary=vertices.size();
                    const auto first_index=indices.size();
                    if(!append_wet_triangle(water_corners,wet_depth,vertices,indices,skirt))
                        return reject("invalid_wet_triangle",p.key);
                    if(rendered&&!skirt&&indices.size()>first_index&&vertices.size()>first_boundary)
                    {
                        const auto& metric=target_->metadata;
                        const float detail=breaker_scale(source.metadata,p,0.0f);
                        for(auto index=first_boundary;index<vertices.size();++index)
                            remember_shoreline_spray(*rendered,vertices[index],metric.water_origin_meters,
                                metric.water_meters_to_artifact,metric.water_radius_meters,detail);
                    }
                }
                if(indices.size()==water_indices_begin&&!water_range(source.metadata,p))
                {
                    vertices.resize(water_base);
                }
                else if(rendered)
                {
                    auto scale=plan.fine->specular;
                    if(plan.parent)scale=(std::max)(scale,plan.parent->specular);
                    const bool varying=plan.parent||plan.fine->vertex_specular
                        ||fades_at_edge(plan.fine->edge_weights,scale);
                    rendered->ranges.push_back({water_base,static_cast<std::uint32_t>(vertices.size()-water_base),scale,varying});
                }
            }
            const auto result=asset::mesh::build_artifact(vertices,indices,limits_.artifact);
            if(stop.stop_requested())return reject("cancelled");
            if(!result)return reject("invalid_output_artifact");
            if(rendered)rendered->artifact_key=result.artifact->key();
            if(appearance)*appearance=std::move(rendered);
            return result.artifact;
        }
        catch(...){return reject("worker_exception");}
    }
}
