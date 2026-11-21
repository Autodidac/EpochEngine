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
#include <utility>
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

    namespace detail
    {
        [[nodiscard]] inline std::uint32_t lattice_top(std::uint32_t index,std::uint32_t cells) noexcept
        {
            const auto side=cells+1u,grid=side*side;
            if(index<grid)return index;
            const auto edge=index-grid;
            if(edge<cells)return edge;
            if(edge<2u*cells)return (edge-cells)*side+cells;
            if(edge<3u*cells)return cells*side+3u*cells-edge;
            return (4u*cells-edge)*side;
        }
    }

    [[nodiscard]] inline std::optional<std::vector<::epochengine::project_planetary_snapshot::WaterVertex>>
    build_water_lattice_basis(const core::Body& body,core::TileKey key,std::uint32_t cells,bool skirts,
        std::stop_token stop) noexcept
    {
        namespace recipe=::epochengine::project_planetary_snapshot;
        try
        {
            if(stop.stop_requested()||!body.ocean||key.body!=body.id||key.face>5u||key.level>20u
                ||key.x>=(1u<<key.level)||key.y>=(1u<<key.level)||cells<2u||cells>64u
                ||!std::isfinite(body.radius)||body.radius<=0.0)return {};
            const auto side=cells+1u,grid=side*side,layer=grid+(skirts?4u*cells:0u);
            const auto anchor=skirts?core::tile_mesh_anchor(body,key):core::DVec3{};
            const auto skirt=skirts?core::tile_skirt_depth(body,key,static_cast<int>(cells)):0.0;
            std::vector<recipe::WaterVertex> basis;
            basis.reserve(layer);
            for(std::uint32_t i=0;i<layer;++i)
            {
                if(stop.stop_requested())return {};
                const auto top=detail::lattice_top(i,cells);
                const auto direction=core::tile_direction(key,double(top%side)/cells,double(top/side)/cells);
                double depth{};
                if(i>=grid)
                {
                    // Match the cached undeformed water layer's two float narrowings.
                    const auto local=core::FVec3{direction*body.radius-anchor};
                    const auto lowered=anchor+core::DVec3{core::lower_tile_vertex(local,anchor,skirt)};
                    depth=(std::max)(0.0,body.radius-core::length(lowered));
                    if(!std::isfinite(depth)||depth>=body.radius)return {};
                }
                basis.push_back({{direction.x,direction.y,direction.z},depth,top});
            }
            return basis;
        }
        catch(...){return {};}
    }

    [[nodiscard]] inline double lattice_spacing_meters(const core::Body& body,core::TileKey key,
        std::uint32_t cells) noexcept
    {
        const auto first=core::tile_direction(key,0.0,0.0);
        const auto next=core::tile_direction(key,1.0/cells,0.0);
        return core::length(next-first)*body.radius;
    }

    [[nodiscard]] inline std::array<float,4> breaker_edges_for_cut(const core::Body& body,core::TileKey key,
        std::uint32_t cells,std::span<const core::TileKey> cut) noexcept
    {
        const float interior=wave::spherical_breaker_scale(lattice_spacing_meters(body,key,cells));
        std::array<float,4> edges{};
        edges.fill(interior);
        if(!key.level||interior<=0.0f)return edges;
        const auto neighbors=core::neighbors(key);
        for(std::size_t side=0;side<edges.size();++side)
        {
            const auto coarse=core::parent(neighbors[side]);
            if(std::ranges::find(cut,coarse)!=cut.end())
                edges[side]=(std::min)(interior,wave::spherical_breaker_scale(lattice_spacing_meters(body,coarse,cells)));
        }
        return edges;
    }

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
                const auto top=detail::lattice_top(static_cast<std::uint32_t>(i),cells);
                const auto x=top%side,y=top/side;
                const auto direction=core::tile_direction(key,double(x)/cells,double(y)/cells);
                const auto original=source.anchor+core::DVec3{source.vertices[layer+i].position};
                const double depth=i<grid?0.0:(std::max)(0.0,body.radius-core::length(original));
                surface_indices[i]=top;
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
        void use_sample(std::shared_ptr<const wave::Tile> tile) noexcept {tile_=std::move(tile);}

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
            std::vector<std::array<float,3>>* parent_delta=nullptr,
            mesh::SphericalWaterRange* appearance=nullptr, float shore_tint_strength=1.0f,
            const std::array<float,4>* breaker_edges=nullptr) const noexcept
        {
            if(!tile_||cells<2u||cells>64u||!std::isfinite(scale)||scale<=0.0
                ||vertex_base>vertices.size()||vertices.size()-vertex_base!=source.vertices.size()
                ||index_base>indices.size()||indices.size()-index_base!=source.indices.size()
                ||source.indices.size()%3u!=0u)return false;
            if(parent_delta&&parent_delta->size()!=vertices.size())return false;
            for(const auto index:source.indices)if(index>=source.vertices.size())return false;
            if(appearance)*appearance={};
            if(!body.ocean)return true;
            const std::size_t side=cells+1u,grid=side*side;
            const auto first=std::ranges::find_if(source.vertices,[](const auto& vertex){return vertex.color[3]>0.5f;});
            if(first==source.vertices.end())return true;
            const auto layer=static_cast<std::size_t>(first-source.vertices.begin());
            if((layer!=grid&&layer!=grid+4u*cells)||source.vertices.size()!=2u*layer)return false;
            try
            {
                std::vector<std::uint32_t> surface_indices(layer);
                const double spacing_meters=lattice_spacing_meters(body,key,cells);
                const double wave_length_meters = tile_->length_meters();
                const float breaker=shore_tint_strength>0.0f?wave::spherical_breaker_scale(spacing_meters):0.0f;
                const bool blend_breaker=breaker_edges&&std::ranges::any_of(*breaker_edges,
                    [&](float weight){return weight<breaker;});
                // Every fine vertex interpolates the same immediate-parent
                // lattice. Sample each lattice point once per patch rather
                // than repeating four spherical FFT derivatives per vertex.
                // This storage is call-local: one immutable Surface can be
                // used concurrently by the existing patch workers.
                std::vector<core::DVec3> parent_points_cache(key.level>0u?grid:0u);
                std::vector<std::uint8_t> parent_points_ready(parent_points_cache.size());
                for(std::size_t i=0;i<layer;++i)
                {
                    const auto top=detail::lattice_top(static_cast<std::uint32_t>(i),cells);
                    const auto x=top%side,y=top/side;
                    const auto direction=core::tile_direction(key,double(x)/cells,double(y)/cells);
                    const auto sampled=wave::spherical_sample(*tile_,{direction.x,direction.y,direction.z},
                        body.radius,tile_->seconds());
                    if(!sampled)return false;
                    core::DVec3 p{sampled->position_meters[0],sampled->position_meters[1],sampled->position_meters[2]};
                    const auto radius=core::length(p);
                    const auto displaced_direction=p/radius;
                    surface_indices[i]=top;
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
                    const auto& bed = vertices[vertex_base+surface_indices[i]].position;
                    const double bed_radius = std::hypot(bed[0]/scale-offset.x, bed[1]/scale-offset.y,
                        bed[2]/scale-offset.z);
                    const float breaker_weight=blend_breaker?wave::spherical_water_edge_weight(*breaker_edges,
                        breaker,static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y),cells):breaker;
                    vertex.normal=wave::spherical_shoaled_normal(sampled->normal,
                        {direction.x,direction.y,direction.z},body.radius-bed_radius,radius-body.radius,
                        breaker_weight*shore_tint_strength);
                    vertex.color = wave::spherical_surface_color(body.radius - bed_radius,
                        sampled->foam, spacing_meters, wave_length_meters, false, shore_tint_strength,
                        radius - body.radius,breaker_weight);
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
                // Classify against the matching ground sample. The same water
                // edge must have one contour even with tangential displacement.
                const std::array body_center{offset.x*scale,offset.y*scale,offset.z*scale};
                const auto depth=[&](std::size_t local) {
                    // Lowered skirt vertices share wetness with their top edge.
                    const auto surface=surface_indices[local];
                    return wave::spherical_shore_depth(vertices[vertex_base+surface].position,
                        vertices[vertex_base+layer+surface].position,body_center);
                };
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
                    const auto depth_a=depth(local_a),depth_b=depth(local_b),depth_c=depth(local_c);
                    if(!depth_a||!depth_b||!depth_c)return false;
                    const auto polygon=wave::spherical_wet_polygon({*depth_a,*depth_b,*depth_c});
                    const std::array input{a,b,c};
                    std::array<std::uint32_t,4> output{};
                    for(std::size_t corner=0;corner<polygon.count;++corner)
                    {
                        const auto sample=polygon.corners[corner];
                        const auto wet=input[sample.wet],other=input[sample.other];
                        if(!sample.crossing())
                        {
                            output[corner]=wet;
                            continue;
                        }
                        const auto av=vertices[wet],bv=vertices[other];
                        const double fraction=sample.fraction;
                        mesh::Vertex boundary;
                        double normal_length{};
                        for(std::size_t axis=0;axis<3u;++axis)
                        {
                            boundary.position[axis]=static_cast<float>(std::lerp(double(av.position[axis]),double(bv.position[axis]),fraction));
                            boundary.normal[axis]=static_cast<float>(std::lerp(double(av.normal[axis]),double(bv.normal[axis]),fraction));
                            // Clipping must not introduce foam unsupported by the sampled edge.
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
                                delta[axis]=static_cast<float>(std::lerp(double(av.position[axis])+(*parent_delta)[wet][axis],
                                    double(bv.position[axis])+(*parent_delta)[other][axis],fraction)-boundary.position[axis]);
                            parent_delta->push_back(delta);
                        }
                        output[corner]=index;
                    }
                    for(std::size_t i=1u;i+1u<polygon.count;++i)
                    {
                        if(polygon.clipped)
                        {
                            // Distinct double shoreline intersections can
                            // collapse after narrowing to artifact floats. Drop
                            // only unrenderable generated faces, using the
                            // artifact's area floor; input faces still validate.
                            const auto& p=vertices[output[0]].position;
                            const auto& q=vertices[output[i]].position;
                            const auto& r=vertices[output[i+1u]].position;
                            const core::DVec3 ab{double(q[0])-p[0],double(q[1])-p[1],double(q[2])-p[2]};
                            const core::DVec3 ac{double(r[0])-p[0],double(r[1])-p[1],double(r[2])-p[2]};
                            const core::DVec3 area{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
                            if(area.x*area.x+area.y*area.y+area.z*area.z<=1.0e-24)continue;
                        }
                        indices.insert(indices.end(),{output[0],output[i],output[i+1u]});
                    }
                    if(vertices.capacity()*sizeof(mesh::Vertex)+indices.capacity()*sizeof(std::uint32_t)>maximum_bytes)return false;
                }
                if(appearance)
                {
                    const auto first_water=static_cast<std::uint32_t>(vertex_base+layer);
                    *appearance={first_water,static_cast<std::uint32_t>(vertices.size()-first_water),
                        wave::spherical_specular_scale(spacing_meters,wave_length_meters)};
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
