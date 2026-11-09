// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>
export module water.planetary;
import planet.terrain;
import water.fft;
import asset.mesh_artifact;
import project.planetary_snapshot;

export namespace epoch::extensions::planetary_terrain::water
{
    namespace core=::epoch::extensions::planetary_terrain::core;
    namespace mesh=::epochengine::asset::mesh;
    namespace wave=::epochengine::water::wave;
    namespace fft=::epochengine::water::fft;

    // The package owns cube topology and ocean-layer semantics. Retain that
    // exact recipe for the Engine's existing asynchronous Water sampler; the
    // renderer never identifies ocean vertices by their display color.
    [[nodiscard]] inline bool append_recipe(::epochengine::project_planetary_snapshot::Metadata& metadata,
        ::epochengine::project_planetary_snapshot::Patch& patch,const core::Mesh& source,
        const core::Body& body,core::TileKey key,std::uint32_t cells,double scale) noexcept
    {
        namespace recipe=::epochengine::project_planetary_snapshot;
        if(!patch.target||!body.ocean||key.level<12u)return true; // Far water retains its LOD sample.
        try
        {
            if(cells<2u||cells>64u||source.indices.size()%3u)return false;
            const std::size_t side=cells+1u,grid=side*side;
            const auto first=std::ranges::find_if(source.vertices,[](const auto& vertex){return vertex.color[3]>0.5f;});
            if(first==source.vertices.end())return true;
            const auto layer=static_cast<std::size_t>(first-source.vertices.begin());
            if((layer!=grid&&layer!=grid+4u*cells)||source.vertices.size()!=2u*layer)return false;
            patch.terrain_vertices=static_cast<std::uint32_t>(layer);
            patch.first_water_vertex=static_cast<std::uint32_t>(metadata.water_vertices.size());
            patch.first_water_triangle=static_cast<std::uint32_t>(metadata.water_triangles.size());
            metadata.water_radius_meters=body.radius;metadata.water_meters_to_artifact=scale;
            std::vector<std::uint32_t> surface_indices(layer);
            for(std::size_t i=0;i<layer;++i)
            {
                std::size_t x{},y{};
                if(i<grid){x=i%side;y=i/side;}
                else
                {
                    const auto edge=i-grid;
                    if(edge<cells){x=edge;y=0;}
                    else if(edge<2u*cells){x=cells;y=edge-cells;}
                    else if(edge<3u*cells){x=3u*cells-edge;y=cells;}
                    else{x=0;y=4u*cells-edge;}
                }
                const auto direction=core::tile_direction(key,double(x)/cells,double(y)/cells);
                const auto original=source.anchor+core::DVec3{source.vertices[layer+i].position};
                const double depth=i<grid?0.0:(std::max)(0.0,body.radius-core::length(original));
                surface_indices[i]=static_cast<std::uint32_t>(y*side+x);
                metadata.water_vertices.push_back({{direction.x,direction.y,direction.z},depth,
                    patch.first_water_vertex+surface_indices[i]});
            }
            for(std::size_t t=0;t<source.indices.size();t+=3u)
            {
                const auto a=source.indices[t],b=source.indices[t+1u],c=source.indices[t+2u];
                if(a<layer&&b<layer&&c<layer){patch.terrain_indices+=3u;continue;}
                if(a<layer||b<layer||c<layer||a>=2u*layer||b>=2u*layer||c>=2u*layer)return false;
                const std::array local{static_cast<std::uint32_t>(a-layer),static_cast<std::uint32_t>(b-layer),static_cast<std::uint32_t>(c-layer)};
                std::array<std::uint32_t,3> ground{surface_indices[local[0]],surface_indices[local[1]],surface_indices[local[2]]};
                if(local[0]>=grid||local[1]>=grid||local[2]>=grid)
                {
                    const auto min_index=*std::ranges::min_element(ground),max_index=*std::ranges::max_element(ground);
                    const auto x=static_cast<std::uint32_t>(min_index%side),y=static_cast<std::uint32_t>(min_index/side);
                    const bool upper=y==cells,left=x==0u&&max_index%side==0u;
                    const auto gx=upper?x:(left?0u:(std::min)(x,cells-1u)),gy=upper?cells-1u:y;
                    const auto first=gy*static_cast<std::uint32_t>(side)+gx;
                    ground=upper||left?std::array<std::uint32_t,3>{first,first+static_cast<std::uint32_t>(side)+1u,first+static_cast<std::uint32_t>(side)}
                        :std::array<std::uint32_t,3>{first,first+1u,first+static_cast<std::uint32_t>(side)+1u};
                }
                recipe::WaterTriangle triangle;
                for(unsigned i=0;i<3u;++i){triangle.water[i]=patch.first_water_vertex+local[i];triangle.ground[i]=patch.first_vertex+ground[i];}
                metadata.water_triangles.push_back(triangle);++patch.water_triangles;
            }
            return true;
        }
        catch(...){return false;}
    }

    class Surface final
    {
    public:
        [[nodiscard]] bool evaluate(std::uint32_t seed,double seconds,
            std::uint64_t maximum_bytes=4u*1024u*1024u,std::stop_token stop={}) noexcept
        {
            fft::Settings settings;settings.seed=seed;
            const auto result=evaluator_.evaluate(settings,seconds,maximum_bytes,stop);
            if(!result)return false;
            tile_=result.tile;return true;
        }
        [[nodiscard]] std::shared_ptr<const wave::Tile> tile() const noexcept {return tile_;}

        // Only the disposable artifact is deformed. Cached terrain, its source
        // water grid and parent deltas remain unchanged across time samples.
        [[nodiscard]] bool apply_patch(std::vector<mesh::Vertex>& vertices,
            std::vector<std::uint32_t>& indices,std::uint32_t vertex_base,std::size_t index_base,
            const core::Mesh& source,const core::Body& body,core::TileKey key,std::uint32_t cells,
            double scale,core::DVec3 offset,std::uint64_t maximum_bytes,
            std::vector<std::array<float,3>>* parent_delta=nullptr) const noexcept
        {
            if(!tile_||cells<2u||cells>64u||!std::isfinite(scale)||scale<=0.0
                ||vertex_base>vertices.size()||vertices.size()-vertex_base!=source.vertices.size()
                ||index_base>indices.size()||indices.size()-index_base!=source.indices.size()
                ||source.indices.size()%3u!=0u)return false;
            if(parent_delta&&parent_delta->size()!=vertices.size())return false;
            for(const auto index:source.indices)if(index>=source.vertices.size())return false;
            if(!body.ocean)return true;
            const std::size_t side=cells+1u,grid=side*side;
            const auto first=std::ranges::find_if(source.vertices,[](const auto& vertex){return vertex.color[3]>0.5f;});
            if(first==source.vertices.end())return true;
            const auto layer=static_cast<std::size_t>(first-source.vertices.begin());
            if((layer!=grid&&layer!=grid+4u*cells)||source.vertices.size()!=2u*layer)return false;
            try
            {
                std::vector<std::uint32_t> surface_indices(layer);
                // Every fine vertex interpolates the same immediate-parent
                // lattice. Sample each lattice point once per patch rather
                // than repeating four spherical FFT derivatives per vertex.
                // This storage is call-local: one immutable Surface can be
                // used concurrently by the existing patch workers.
                std::vector<core::DVec3> parent_points_cache(key.level>0u?grid:0u);
                std::vector<std::uint8_t> parent_points_ready(parent_points_cache.size());
                for(std::size_t i=0;i<layer;++i)
                {
                    std::size_t x{},y{};
                    if(i<grid){x=i%side;y=i/side;}
                    else
                    {
                        const auto edge=i-grid;
                        if(edge<cells){x=edge;y=0;}
                        else if(edge<2u*cells){x=cells;y=edge-cells;}
                        else if(edge<3u*cells){x=3u*cells-edge;y=cells;}
                        else{x=0;y=4u*cells-edge;}
                    }
                    const auto direction=core::tile_direction(key,double(x)/cells,double(y)/cells);
                    const auto sampled=wave::spherical_sample(*tile_,{direction.x,direction.y,direction.z},
                        body.radius,tile_->seconds());
                    if(!sampled)return false;
                    core::DVec3 p{sampled->position_meters[0],sampled->position_meters[1],sampled->position_meters[2]};
                    const auto radius=core::length(p);
                    const auto displaced_direction=p/radius;
                    surface_indices[i]=static_cast<std::uint32_t>(y*side+x);
                    // Preserve the cached skirt's radial depth, not its float
                    // position or time. It continues sealing the same grid edge.
                    if(i>=grid)
                    {
                        const auto original=source.anchor+core::DVec3{source.vertices[layer+i].position};
                        const auto depth=(std::max)(0.0,body.radius-core::length(original));
                        p=p-displaced_direction*depth;
                    }
                    const auto output=(p+offset)*scale;
                    auto& vertex=vertices[vertex_base+layer+i];
                    vertex.position={static_cast<float>(output.x),static_cast<float>(output.y),static_cast<float>(output.z)};
                    vertex.normal=sampled->normal;
                    if(parent_delta)
                    {
                        std::array<float,3> delta{};
                        if(key.level>0u)
                        {
                            const double px=(double(key.x&1u)*cells+x)*0.5;
                            const double py=(double(key.y&1u)*cells+y)*0.5;
                            const auto ix=(std::min)(static_cast<std::uint32_t>(px),cells-1u);
                            const auto iy=(std::min)(static_cast<std::uint32_t>(py),cells-1u);
                            const double fx=px-ix,fy=py-iy;
                            std::array<core::DVec3,4> parent_points{};
                            for(std::size_t corner=0;corner<4u;++corner)
                            {
                                const auto px=ix+(corner&1u),py=iy+(corner>>1u);
                                const auto at=py*side+px;
                                if(!parent_points_ready[at])
                                {
                                    const auto d=core::tile_direction(core::parent(key),double(px)/cells,double(py)/cells);
                                    const auto sample=wave::spherical_sample(*tile_,{d.x,d.y,d.z},body.radius,tile_->seconds());
                                    if(!sample)return false;
                                    parent_points_cache[at]={sample->position_meters[0],sample->position_meters[1],sample->position_meters[2]};
                                    parent_points_ready[at]=1u;
                                }
                                parent_points[corner]=parent_points_cache[at];
                            }
                            const auto& a=parent_points[0];const auto& b=parent_points[1];
                            const auto& c=parent_points[2];const auto& e=parent_points[3];
                            const auto parent=fx>=fy?a+(b-a)*fx+(e-b)*fy:a+(e-c)*fx+(c-a)*fy;
                            const core::DVec3 fine{sampled->position_meters[0],sampled->position_meters[1],sampled->position_meters[2]};
                            const auto difference=(parent-fine)*scale;
                            delta={static_cast<float>(difference.x),static_cast<float>(difference.y),static_cast<float>(difference.z)};
                        }
                        (*parent_delta)[vertex_base+layer+i]=delta;
                    }
                }
                // Clip against the actual rendered terrain triangle. Analytic
                // spherical crossings leave coarse straight terrain faces far
                // below their shoreline and move water edges out of their face.
                const auto old_indices=std::vector<std::uint32_t>{indices.begin()+static_cast<std::ptrdiff_t>(index_base),indices.end()};
                indices.resize(index_base);
                for(std::size_t t=0;t<old_indices.size();t+=3u)
                {
                    const auto a=old_indices[t],b=old_indices[t+1u],c=old_indices[t+2u];
                    const auto first_water=std::uint64_t(vertex_base)+layer;
                    if(a<first_water&&b<first_water&&c<first_water)
                    {indices.insert(indices.end(),{a,b,c});continue;}
                    if(a<first_water||b<first_water||c<first_water)return false;
                    const auto local_a=static_cast<std::size_t>(a-first_water);
                    const auto local_b=static_cast<std::size_t>(b-first_water);
                    const auto local_c=static_cast<std::size_t>(c-first_water);
                    std::array<std::uint32_t,3> ground{surface_indices[local_a],surface_indices[local_b],surface_indices[local_c]};
                    if(local_a>=grid||local_b>=grid||local_c>=grid)
                    {
                        // A lowered wall keeps the wetness of its top edge. Use
                        // that edge's incident surface triangle, so clipping
                        // retains the entire sealing depth at the same crossing.
                        const auto min_index=*std::ranges::min_element(ground);
                        const auto max_index=*std::ranges::max_element(ground);
                        const auto x=static_cast<std::uint32_t>(min_index%side);
                        const auto y=static_cast<std::uint32_t>(min_index/side);
                        const bool upper=y==cells,left=x==0u&&max_index%side==0u;
                        const auto gx=upper?x:(left?0u:(std::min)(x,cells-1u));
                        const auto gy=upper?cells-1u:y;
                        const auto first=gy*static_cast<std::uint32_t>(side)+gx;
                        ground=upper||left?std::array<std::uint32_t,3>{first,first+static_cast<std::uint32_t>(side)+1u,first+static_cast<std::uint32_t>(side)}
                            :std::array<std::uint32_t,3>{first,first+1u,first+static_cast<std::uint32_t>(side)+1u};
                    }
                    const auto point=[&](std::uint32_t index){const auto& p=vertices[vertex_base+index].position;return core::DVec3{p[0],p[1],p[2]};};
                    const auto origin=point(ground[0]);
                    const auto ab=point(ground[1])-origin,ac=point(ground[2])-origin;
                    const core::DVec3 normal{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
                    if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<=0.0)return false;
                    const auto depth=[&](std::size_t local){const auto delta=
                        point(static_cast<std::uint32_t>(layer)+surface_indices[local])-origin;
                        return normal.x*delta.x+normal.y*delta.y+normal.z*delta.z;};
                    struct Clip {std::uint32_t index{};double depth{};};
                    const std::array<Clip,3> input{{{a,depth(local_a)},
                        {b,depth(local_b)},{c,depth(local_c)}}};
                    std::array<Clip,4> output{};std::size_t count{};bool clipped{};
                    for(std::size_t edge=0;edge<3u;++edge)
                    {
                        const auto from=input[edge],to=input[(edge+1u)%3u];
                        const bool wet_from=from.depth>0.0,wet_to=to.depth>0.0;
                        if(wet_from)output[count++]=from;
                        if(wet_from==wet_to)continue;
                        clipped=true;
                        const double fraction=from.depth/(from.depth-to.depth);
                        const auto av=vertices[from.index],bv=vertices[to.index];mesh::Vertex boundary;
                        double normal_length{};
                        for(std::size_t axis=0;axis<3u;++axis)
                        {
                            boundary.position[axis]=static_cast<float>(std::lerp(double(av.position[axis]),double(bv.position[axis]),fraction));
                            boundary.normal[axis]=static_cast<float>(std::lerp(double(av.normal[axis]),double(bv.normal[axis]),fraction));
                            boundary.color[axis]=static_cast<float>(std::lerp(double(av.color[axis]),double(bv.color[axis]),fraction));
                            normal_length+=double(boundary.normal[axis])*boundary.normal[axis];
                        }
                        normal_length=std::sqrt(normal_length);if(normal_length<=0.0)return false;
                        for(auto& value:boundary.normal)value=static_cast<float>(value/normal_length);
                        for(std::size_t axis=0;axis<2u;++axis)
                            boundary.uv[axis]=static_cast<float>(std::lerp(double(av.uv[axis]),double(bv.uv[axis]),fraction));
                        if((vertices.capacity()+1u)*sizeof(mesh::Vertex)+indices.capacity()*sizeof(std::uint32_t)>maximum_bytes)return false;
                        const auto index=static_cast<std::uint32_t>(vertices.size());vertices.push_back(boundary);
                        if(parent_delta)
                        {
                            std::array<float,3> delta{};
                            for(std::size_t axis=0;axis<3u;++axis)
                                delta[axis]=static_cast<float>(std::lerp(double(av.position[axis])+(*parent_delta)[from.index][axis],
                                    double(bv.position[axis])+(*parent_delta)[to.index][axis],fraction)-boundary.position[axis]);
                            parent_delta->push_back(delta);
                        }
                        output[count++]={index,0.0};
                    }
                    for(std::size_t i=1u;i+1u<count;++i)
                    {
                        if(clipped)
                        {
                            // Distinct double shoreline intersections can
                            // collapse after narrowing to artifact floats. Drop
                            // only unrenderable generated faces, using the
                            // artifact's area floor; input faces still validate.
                            const auto& p=vertices[output[0].index].position;
                            const auto& q=vertices[output[i].index].position;
                            const auto& r=vertices[output[i+1u].index].position;
                            const core::DVec3 ab{double(q[0])-p[0],double(q[1])-p[1],double(q[2])-p[2]};
                            const core::DVec3 ac{double(r[0])-p[0],double(r[1])-p[1],double(r[2])-p[2]};
                            const core::DVec3 area{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
                            if(area.x*area.x+area.y*area.y+area.z*area.z<=1.0e-24)continue;
                        }
                        indices.insert(indices.end(),{output[0].index,output[i].index,output[i+1u].index});
                    }
                    if(vertices.capacity()*sizeof(mesh::Vertex)+indices.capacity()*sizeof(std::uint32_t)>maximum_bytes)return false;
                }
                return true;
            }
            catch(...) {return false;}
        }
    private:
        fft::Evaluator evaluator_{};
        std::shared_ptr<const wave::Tile> tile_{};
    };
}
