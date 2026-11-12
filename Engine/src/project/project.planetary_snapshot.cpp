// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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
        constexpr asset::mesh::Limits cloud_limits{24'000u,140'000u,2ull*1024u*1024u};
        std::uint64_t cloud_bytes(const Metadata& data) noexcept
        {return data.cloud&&data.cloud->artifact?48u+44u*data.cloud->artifact->vertices().size()+4u*data.cloud->artifact->indices().size():0u;}
        bool local_water(const Metadata& data) noexcept
        {return data.water_origin_meters!=std::array<double,3>{};}
        bool has_water(const Metadata& data) noexcept
        {return data.water_radius_meters!=0.0||!data.water_vertices.empty()||!data.water_triangles.empty();}
        std::uint64_t metadata_bytes(const Metadata& data) noexcept
        {return (data.cloud?(data.cloud->density?84u:56u+cloud_bytes(data)):0u)+header_bytes+patch_bytes*data.patches.size()+12u*data.parent_delta.size()+checksum_bytes
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
            // exact parents until their descendant quartet has retired.
            for(const auto& draw:cut)
                if(const auto index=find(draw.snapshot->metadata,key))return DrawPatch{draw.snapshot,*index,0.0f};
            return {};
        }
        std::uint64_t bytes(const Snapshot& snapshot) noexcept
        {
            return cloud_bytes(snapshot.metadata)+512u+snapshot.artifact->vertices().size()*sizeof(asset::mesh::Vertex)
                +snapshot.artifact->indices().size()*4u+snapshot.metadata.parent_delta.capacity()*12u
                +snapshot.metadata.patches.capacity()*sizeof(Patch)
                +snapshot.metadata.water_vertices.capacity()*sizeof(WaterVertex)
                +snapshot.metadata.water_triangles.capacity()*sizeof(WaterTriangle);
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
            const auto output=vertices*sizeof(asset::mesh::Vertex)+indices*4u;
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
        bool balanced_coarsen(std::span<const DrawPatch> cut,const Patch& value) noexcept
        {
            for(const auto neighbor:value.neighbors)
                for(const auto& draw:cut)
                    if(contains(neighbor,patch(draw).key)&&patch(draw).key.level>value.key.level+1u)return false;
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
            put(out,0x544c5050u,4);put(out,data.cloud?(data.cloud->density?5u:4u):local?3u:wet?2u:1u,4);
            if(data.cloud)put(out,local?3u:wet?2u:1u,4);
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
                }
                else
                {
                    const auto encoded=asset::mesh::encode_artifact(*cloud.artifact);if(encoded.empty())return {};
                    put(out,encoded.size(),8);out.insert(out.end(),encoded.begin(),encoded.end());
                }
            }
            const auto hash=asset::texture::deterministic_content_hash(data.cloud?(data.cloud->density?"epoch.planet.patch.v5":"epoch.planet.patch.v4"):local?"epoch.planet.patch.v3":wet?"epoch.planet.patch.v2":"epoch.planet.patch.v1",out);
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
            const auto version=get(in,offset,4);if(version<1u||version>5u)return {};
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
            if(data.identity!=expected||patches>limits.maximum_patches||patches>4096u||vertices!=artifact.vertices().size()
                ||(version==5u?legacy_size+84u!=in.size():version==4u?legacy_size+56u+48u>in.size():legacy_size!=in.size()))return {};
            auto hash_offset=in.size()-checksum_bytes;asset::mesh::ContentHash claimed;
            for(auto& word:claimed.words)word=get(in,hash_offset,8);
            if(claimed!=asset::texture::deterministic_content_hash(version==5u?"epoch.planet.patch.v5":version==4u?"epoch.planet.patch.v4":local?"epoch.planet.patch.v3":wet?"epoch.planet.patch.v2":"epoch.planet.patch.v1",in.first(in.size()-checksum_bytes)))return {};
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
                if(version==5u)
                {
                    asset::cloud_volume::Recipe recipe;recipe.seed=static_cast<std::uint32_t>(get(in,offset,4));
                    recipe.base_altitude_meters=std::bit_cast<double>(get(in,offset,8));
                    recipe.thickness_meters=std::bit_cast<double>(get(in,offset,8));
                    recipe.horizontal_scale_meters=std::bit_cast<double>(get(in,offset,8));
                    recipe.coverage=std::bit_cast<float>(static_cast<std::uint32_t>(get(in,offset,4)));
                    recipe.extinction_per_meter=std::bit_cast<float>(static_cast<std::uint32_t>(get(in,offset,4)));
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

    std::shared_ptr<const asset::mesh::Artifact> resample_water(const Snapshot& source,
        const water::wave::Tile& tile,Limits limits,std::stop_token stop) noexcept
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
            const auto maximum_output=maximum_vertices*sizeof(asset::mesh::Vertex)+maximum_indices*4u;
            if(48u+maximum_vertices*44u+maximum_indices*4u>limits.artifact.maximum_bytes
                ||2u*maximum_output+tile.payload_bytes()>limits.maximum_transient_bytes
                ||bytes(source)+2u*maximum_output+tile.payload_bytes()>limits.maximum_retained_bytes)return {};
            // Reserve the complete worst-case clipping expansion only after
            // admission. No vector growth can exceed the worker envelope.
            vertices.reserve(static_cast<std::size_t>(maximum_vertices));
            indices.reserve(static_cast<std::size_t>(maximum_indices));
            const auto admitted=[&] {
                const std::uint64_t output=vertices.capacity()*sizeof(asset::mesh::Vertex)+indices.capacity()*4u;
                return vertices.size()<=limits.artifact.maximum_vertices&&indices.size()<=limits.artifact.maximum_indices
                    &&48u+vertices.size()*44u+indices.size()*4u<=limits.artifact.maximum_bytes
                    &&2u*output+tile.payload_bytes()<=limits.maximum_transient_bytes
                    &&bytes(source)+2u*output+tile.payload_bytes()<=limits.maximum_retained_bytes;
            };
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
                if(!p.terrain_vertices)continue;
                const auto water_base=static_cast<std::uint32_t>(vertices.size());
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
                    value.normal=sampled->normal;vertices.push_back(value);
                }
                if(!admitted())return {};
                const auto pos=[&](std::uint32_t id){const auto& point=vertices[id].position;return Point{point[0],point[1],point[2]};};
                for(std::uint32_t t=0;t<p.water_triangles;++t)
                {
                    if(stop.stop_requested())return {};
                    const auto& triangle=source.metadata.water_triangles[p.first_water_triangle+t];
                    std::array<std::uint32_t,3> water{},ground{};
                    for(unsigned i=0;i<3u;++i){water[i]=water_base+triangle.water[i]-p.first_water_vertex;ground[i]=base+triangle.ground[i]-p.first_vertex;}
                    const auto origin=pos(ground[0]);const auto normal=cross(sub(pos(ground[1]),origin),sub(pos(ground[2]),origin));
                    if(dot(normal,normal)<=0.0)return {};
                    struct Clip {std::uint32_t index{};double depth{};};
                    std::array<Clip,3> input{};
                    for(unsigned i=0;i<3u;++i)
                    {
                        const auto surface=source.metadata.water_vertices[triangle.water[i]].surface_vertex;
                        input[i]={water[i],dot(normal,sub(pos(water_base+surface-p.first_water_vertex),origin))};
                    }
                    std::array<Clip,4> output{};std::size_t count{};bool clipped{};
                    for(unsigned edge=0;edge<3u;++edge)
                    {
                        const auto from=input[edge],to=input[(edge+1u)%3u];
                        const bool wet_from=from.depth>0.0,wet_to=to.depth>0.0;
                        if(wet_from)output[count++]=from;
                        if(wet_from==wet_to)continue;
                        clipped=true;const double fraction=from.depth/(from.depth-to.depth);
                        const auto av=vertices[from.index],bv=vertices[to.index];asset::mesh::Vertex boundary;
                        double length{};
                        for(unsigned axis=0;axis<3u;++axis)
                        {
                            boundary.position[axis]=static_cast<float>(std::lerp(double(av.position[axis]),double(bv.position[axis]),fraction));
                            boundary.normal[axis]=static_cast<float>(std::lerp(double(av.normal[axis]),double(bv.normal[axis]),fraction));
                            boundary.color[axis]=static_cast<float>(std::lerp(double(av.color[axis]),double(bv.color[axis]),fraction));
                            length+=double(boundary.normal[axis])*boundary.normal[axis];
                        }
                        length=std::sqrt(length);if(length<=0.0)return {};
                        for(auto& component:boundary.normal)component=static_cast<float>(component/length);
                        for(unsigned axis=0;axis<2u;++axis)boundary.uv[axis]=static_cast<float>(std::lerp(double(av.uv[axis]),double(bv.uv[axis]),fraction));
                        output[count++]={static_cast<std::uint32_t>(vertices.size()),0.0};vertices.push_back(boundary);
                    }
                    for(std::size_t i=1u;i+1u<count;++i)
                    {
                        if(clipped)
                        {
                            const auto area=cross(sub(pos(output[i].index),pos(output[0].index)),sub(pos(output[i+1u].index),pos(output[0].index)));
                            if(dot(area,area)<=1e-24)continue;
                        }
                        indices.insert(indices.end(),{output[0].index,output[i].index,output[i+1u].index});
                    }
                    if(!admitted())return {};
                }
            }
            const auto result=asset::mesh::build_artifact(vertices,indices,limits.artifact);
            return stop.stop_requested()?nullptr:result.artifact;
        }
        catch(...){return {};}
    }

    bool Transition::admit(std::shared_ptr<const Snapshot> incoming,std::stop_token stop,
        InitialCut initial_cut) noexcept
    {
        try
        {
            if(stop.stop_requested()||!incoming||!incoming->artifact||!valid_metadata(incoming->metadata,*incoming->artifact,limits_))return false;
            auto next=active_;
            if(target_)
            {
                const auto& old=target_->metadata.identity;const auto& current=incoming->metadata.identity;
                if(old.source_hash!=current.source_hash||old.recipe_key!=current.recipe_key||current.request_id<=old.request_id)return false;
                if(target_->metadata.patches.front().key.body!=incoming->metadata.patches.front().key.body)return false;
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
            target_=std::move(incoming);active_=std::move(next);return true;
        }
        catch(...){return false;}
    }
    bool Transition::advance(double elapsed,bool paused,std::stop_token stop) noexcept
    {
        if(!target_||!std::isfinite(elapsed)||elapsed<0.0||stop.stop_requested())return false;
        if(paused||elapsed==0.0)return true;
        try
        {
            auto next=active_;
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
                if(old.morph!=0.0f||key.level>=20u||!wants_split(target_->metadata,key)||!balanced_split(next,patch(old)))continue;
                std::array<std::uint32_t,4> indices{};bool complete=true;unsigned j{};
                for(const auto child:children(key))
                {const auto index=find(target_->metadata,child);if(!index){complete=false;break;}indices[j++]=*index;}
                if(!complete)continue;
                auto candidate=next;
                std::erase_if(candidate,[&](const DrawPatch& d){return patch(d).key==key;});
                for(const auto index:indices)candidate.push_back({target_,index,1.0f});
                if(budget(candidate,target_,limits_))next=std::move(candidate);
            }
            if(!budget(next,target_,limits_)||stop.stop_requested())return false;
            active_=std::move(next);return true;
        }
        catch(...){return false;}
    }
    bool Transition::settled() const noexcept
    {
        if(!target_)return false;
        for(const auto& d:active_)
        {const auto index=find(target_->metadata,patch(d).key);if(d.morph!=0.0f||!index||!target_->metadata.patches[*index].target)return false;}
        std::size_t desired{};for(const auto& p:target_->metadata.patches)desired+=p.target?1u:0u;
        return desired==active_.size();
    }
    std::shared_ptr<const asset::mesh::Artifact> Transition::materialize(std::stop_token stop) const noexcept
    {
        try
        {
            if(stop.stop_requested()||!target_||!budget(active_,target_,limits_))return {};
            std::size_t nv{},ni{};for(const auto& d:active_){nv+=patch(d).vertex_count;ni+=patch(d).index_count;}
            std::vector<asset::mesh::Vertex> vertices;std::vector<std::uint32_t> indices;vertices.reserve(nv);indices.reserve(ni);
            for(const auto& d:active_)
            {
                if(stop.stop_requested())return {};
                const auto& p=patch(d);const auto base=static_cast<std::uint32_t>(vertices.size());
                const auto sourceVertices=d.snapshot->artifact->vertices().subspan(p.first_vertex,p.vertex_count);
                const auto sourceIndices=d.snapshot->artifact->indices().subspan(p.first_index,p.index_count);
                if(d.morph==0.0f)
                    vertices.insert(vertices.end(),sourceVertices.begin(),sourceVertices.end());
                else for(std::uint32_t i=0;i<p.vertex_count;++i)
                {
                    auto vertex=sourceVertices[i];
                    const auto delta=d.snapshot->metadata.parent_delta[p.first_vertex+i];
                    for(unsigned axis=0;axis<3u;++axis)vertex.position[axis]+=delta[axis]*d.morph;
                    vertices.push_back(vertex);
                }
                for(const auto index:sourceIndices)indices.push_back(base+index-p.first_vertex);
            }
            const auto result=asset::mesh::build_artifact(vertices,indices,limits_.artifact);
            return stop.stop_requested()?nullptr:result.artifact;
        }
        catch(...){return {};}
    }
}
