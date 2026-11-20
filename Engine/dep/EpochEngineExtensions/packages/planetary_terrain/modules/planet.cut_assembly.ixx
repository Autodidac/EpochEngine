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
#include <string_view>
#include <utility>
#include <vector>

export module planet.cut_assembly;
import planet.terrain;
import planet.streaming;
import asset.mesh_artifact;
import project.planetary_snapshot;
import water.wave;
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
import water.planetary;
#endif

export namespace epoch::extensions::planetary_terrain::cut_assembly
{
    namespace core = epoch::extensions::planetary_terrain::core;
    namespace stream = epoch::extensions::planetary_terrain::streaming;
    namespace mesh = epochengine::asset::mesh;
    namespace snapshot = epochengine::project_planetary_snapshot;

    struct Frame final
    {
        core::DVec3 world_center{}, origin{};
        double scale{};
        bool metric{};
    };

    [[nodiscard]] std::optional<Frame> frame_for(const stream::State& state, bool metric) noexcept
    {
        if (!state.configured) return {};
        const auto center = state.world.center(state.request.body_index,state.request.seconds);
        if (!metric) return Frame{center,{},10.0/state.world.body(state.request.body_index).radius,false};
        const auto camera = state.request.camera-center;
        const auto origin = snapshot::metric_source_origin({camera.x,camera.y,camera.z},true);
        if (!origin) return {};
        return Frame{center,{(*origin)[0],(*origin)[1],(*origin)[2]},1.0,true};
    }

    struct Failure final
    {
        std::string_view stage{};
        std::size_t vertices{}, indices{};
        int detail{};
        // The exporter can coarsen and retry only explicit assembly limits.
        bool limit{};
        explicit operator bool() const noexcept {return !stage.empty();}
    };

    struct Geometry final
    {
        std::vector<mesh::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<std::array<float,3>> deltas;
        mesh::SphericalWaterRange water{};
    };

    struct Work final
    {
        core::TileKey tile{};
        const core::Mesh* source{};
        std::size_t maximum_vertices{}, maximum_indices{};
        std::uint64_t scratch{};
        std::array<float,4> breaker_edges{};
    };

    // State and its resident tiles stay immutable until every prepare call has
    // returned. The caller owns scheduling, cancellation, and the disk or
    // in-process publication boundary. Append is owner-only, in tile order.
    class Assembly final
    {
        const stream::State& state_;
        const Frame frame_;
        const mesh::Limits limits_;
        const bool sidecar_, water_appearance_;
        std::vector<core::TileKey> cut_, tiles_;
        std::vector<mesh::Vertex> vertices_;
        std::vector<std::uint32_t> indices_;
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
        epoch::extensions::planetary_terrain::water::Surface ocean_;
        float shore_tint_{1.0f};
#endif
    public:
        snapshot::Metadata metadata;
        std::vector<mesh::SphericalWaterRange> water_ranges;

        Assembly(const stream::State& state,Frame frame,mesh::Limits limits,
            bool sidecar,bool water_appearance)
            :state_(state),frame_(frame),limits_(limits),sidecar_(sidecar),water_appearance_(water_appearance) {}

        [[nodiscard]] Failure begin(std::shared_ptr<const epochengine::water::wave::Tile> sample={},
            std::stop_token stop={})
        {
            if (stop.stop_requested()) return {"cancelled"};
            if (sample&&!state_.request.ocean_surface) return {"water sample on dry cut"};
            cut_=stream::draw_cut(state_);
            if (cut_.empty()) return {"selected patch unavailable"};
            if (state_.request.ocean_surface)
            {
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
                const auto radius=state_.world.body(state_.request.body_index).radius;
                if (frame_.metric)
                    shore_tint_=epochengine::water::wave::spherical_shore_tint_strength(
                        std::hypot(frame_.origin.x,frame_.origin.y,frame_.origin.z)-radius);
                if (sample) ocean_.use_sample(std::move(sample));
                else if (!ocean_.evaluate(state_.world.seed(),state_.request.seconds,limits_.maximum_bytes,stop))
                    return {"ocean evaluation"};
                if (sidecar_)
                {
                    metadata.water_radius_meters=radius;
                    metadata.water_meters_to_artifact=frame_.scale;
                    metadata.water_origin_meters={frame_.origin.x,frame_.origin.y,frame_.origin.z};
                }
#else
                return {"ocean provider unavailable"};
#endif
            }
            if (sidecar_)
            {
                // Retained ancestors support transition admission. Consumers
                // without a sidecar need exactly the selected draw cut.
                for (const auto& resident : state_.resident)
                    if (std::ranges::any_of(cut_,[&](auto leaf){return core::contains(resident.key,leaf);}))
                        tiles_.push_back(resident.key);
            }
            else tiles_=cut_;
            return {};
        }

        [[nodiscard]] std::span<const core::TileKey> tiles() const noexcept {return tiles_;}
        [[nodiscard]] std::span<const core::TileKey> draw_cut() const noexcept {return cut_;}
        [[nodiscard]] std::size_t vertices() const noexcept {return vertices_.size();}
        [[nodiscard]] std::size_t indices() const noexcept {return indices_.size();}
        [[nodiscard]] std::uint64_t scratch_available() const noexcept
        {
            const auto resident=stream::detail::overhead+stream::resident_bytes(state_);
            return state_.request.maximum_bytes>resident?state_.request.maximum_bytes-resident:0u;
        }

        [[nodiscard]] std::optional<Work> work_for(core::TileKey tile) const
        {
            const auto* resident=stream::detail::find(state_,tile);
            if (!resident) return {};
            const auto& source=resident->mesh;
            Work work{tile,&source,source.vertices.size()+2u*source.indices.size()/3u,2u*source.indices.size()};
            const std::uint64_t side=state_.request.cells_per_tile+1u;
            const std::uint64_t water_scratch=state_.request.ocean_surface
                ?source.indices.size()*sizeof(std::uint32_t)+source.vertices.size()*sizeof(std::uint32_t)
                    +side*side*(sizeof(core::DVec3)+sizeof(std::uint8_t)):0u;
            work.scratch=work.maximum_vertices*(sizeof(mesh::Vertex)+(sidecar_?sizeof(std::array<float,3>):0u))
                +work.maximum_indices*sizeof(std::uint32_t)+water_scratch+sizeof(mesh::Vertex);
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
            if (state_.request.ocean_surface&&shore_tint_>0.0f)
                work.breaker_edges=epoch::extensions::planetary_terrain::water::breaker_edges_for_cut(
                    state_.world.body(state_.request.body_index),tile,state_.request.cells_per_tile,cut_);
#endif
            return work;
        }

        [[nodiscard]] std::optional<Geometry> prepare(const Work& work,std::stop_token stop) const
        {
            if (stop.stop_requested()||!work.source) return {};
            Geometry result;
            result.vertices.reserve(work.maximum_vertices);
            result.indices.reserve(work.maximum_indices);
            if (sidecar_) result.deltas.reserve(work.maximum_vertices);
            const auto& source=*work.source;
            const double scale=frame_.scale;
            for (const auto& vertex : source.vertices)
            {
                const auto relative=(source.anchor+core::DVec3{vertex.position}-frame_.origin)*scale;
                result.vertices.push_back({{static_cast<float>(relative.x),static_cast<float>(relative.y),static_cast<float>(relative.z)},
                    {vertex.normal.x,vertex.normal.y,vertex.normal.z},{},{vertex.color[0],vertex.color[1],vertex.color[2]}});
                if (sidecar_) result.deltas.push_back({static_cast<float>(vertex.parent_delta.x*scale),
                    static_cast<float>(vertex.parent_delta.y*scale),static_cast<float>(vertex.parent_delta.z*scale)});
            }
            result.indices.insert(result.indices.end(),source.indices.begin(),source.indices.end());
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
            if (state_.request.ocean_surface&&!ocean_.apply_patch(result.vertices,result.indices,0u,0u,source,
                state_.world.body(state_.request.body_index),work.tile,state_.request.cells_per_tile,scale,
                core::DVec3{}-frame_.origin,work.scratch,sidecar_?&result.deltas:nullptr,&result.water,
                shore_tint_,&work.breaker_edges)) return {};
#endif
            if (stop.stop_requested()||result.vertices.size()>work.maximum_vertices||result.indices.size()>work.maximum_indices
                ||result.vertices.capacity()*sizeof(mesh::Vertex)+result.indices.capacity()*sizeof(std::uint32_t)
                    +result.deltas.capacity()*sizeof(std::array<float,3>)>work.scratch) return {};
            return result;
        }

        [[nodiscard]] Failure append(const Work& work,const Geometry& chunk)
        {
            const auto nv=vertices_.size()+chunk.vertices.size(), ni=indices_.size()+chunk.indices.size();
            if (nv>limits_.maximum_vertices||ni>limits_.maximum_indices||48u+nv*44u+ni*4u>limits_.maximum_bytes)
                return {"terrain assembly limit",nv,ni,0,true};
            const auto base=static_cast<std::uint32_t>(vertices_.size());
            if (sidecar_)
            {
                snapshot::Patch patch;
                const auto convert=[](core::TileKey key){return snapshot::TileKey{key.x,key.y,key.body,key.face,key.level};};
                patch.key=convert(work.tile);
                const auto neighbors=core::neighbors(work.tile);
                for (unsigned i=0;i<4u;++i) patch.neighbors[i]=convert(neighbors[i]);
                patch.first_vertex=base;
                patch.vertex_count=static_cast<std::uint32_t>(chunk.vertices.size());
                patch.first_index=static_cast<std::uint32_t>(indices_.size());
                patch.index_count=static_cast<std::uint32_t>(chunk.indices.size());
                patch.target=std::ranges::find(cut_,work.tile)!=cut_.end();
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
                if (state_.request.ocean_surface&&!epoch::extensions::planetary_terrain::water::append_recipe(
                    metadata,patch,*work.source,state_.world.body(state_.request.body_index),work.tile,
                    state_.request.cells_per_tile,frame_.scale)) return {"ocean recipe",vertices_.size(),indices_.size()};
#endif
                metadata.patches.push_back(patch);
                if (water_appearance_)
                {
                    auto range=chunk.water;
                    if (range.vertex_count) range.first_vertex+=base;
                    water_ranges.push_back(range);
                }
                metadata.parent_delta.insert(metadata.parent_delta.end(),chunk.deltas.begin(),chunk.deltas.end());
            }
            vertices_.insert(vertices_.end(),chunk.vertices.begin(),chunk.vertices.end());
            for (const auto index : chunk.indices) indices_.push_back(base+index);
            return {};
        }

        [[nodiscard]] mesh::Result build() const noexcept
        {return mesh::build_artifact(vertices_,indices_,limits_);}
    };
}
