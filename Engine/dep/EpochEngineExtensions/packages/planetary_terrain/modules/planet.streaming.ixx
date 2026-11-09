// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Caller-owned LOD residency. Scheduling, rendering and disk I/O stay outside.
module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

export module planet.streaming;
import planet.terrain;

export namespace epoch::extensions::planetary_terrain::streaming
{
    namespace core = epoch::extensions::planetary_terrain::core;
    enum class Status : std::uint8_t { ready, pending, invalid_request, capacity_exceeded, cancelled, stale, invalid_mesh };
    struct Request final
    {
        core::DVec3 camera{}; // Metric world coordinates, never preview-space floats.
        double seconds{};
        int viewport_height{1080};
        double vertical_fov{1.0471975511965976};
        std::size_t body_index{};
        core::LodSettings lod{.max_level=12, .max_leaves=96, .patch_pixels=210.0, .hysteresis=0.15};
        std::uint32_t cells_per_tile{16};
        bool ocean_surface{};
        std::size_t maximum_resident_patches{256};
        std::size_t maximum_in_flight{1}; // Explicit opt-in; legacy callers claim one patch.
        std::uint64_t maximum_bytes{64u * 1024u * 1024u};
        std::optional<core::TerrainView> view{};
    };
    struct Work final
    {
        std::uint64_t generation{}, serial{};
        core::TileKey key{};
        std::uint32_t cells_per_tile{};
        bool ocean_surface{};
        [[nodiscard]] friend bool operator==(const Work&,const Work&) = default;
    };
    struct Resident final { core::TileKey key{}; core::Mesh mesh{}; };
    struct Stats final
    {
        std::uint64_t generation{}, resident_bytes{}, reserved_bytes{}, maximum_bytes{};
        std::size_t resident_patches{}, pending_patches{}, draw_patches{}, claimed_patches{};
        std::uint8_t minimum_level{}, maximum_level{};
        bool complete_coverage{};
    };
    struct State final
    {
        explicit State(std::uint32_t seed=1337) : world(seed) {}
        State(std::uint32_t seed,core::WorldConfig config) : world(seed,config) {}
        core::Universe world;
        Request request{};
        core::TerrainPlan plan{};
        std::vector<Resident> resident{};
        std::vector<Work> in_flight{};
        std::uint64_t generation{}, serial{}, flight_reservation{};
        bool configured{};
    };
    namespace detail
    {
        // Covers the bounded planner/metadata arrays in addition to mesh buffers.
        constexpr std::uint64_t overhead = 1024u * 1024u;
        [[nodiscard]] std::uint64_t mesh_bytes(const core::Mesh& mesh) noexcept
        { return mesh.vertices.capacity()*sizeof(core::Vertex)+mesh.indices.capacity()*sizeof(std::uint32_t)+sizeof(Resident); }
        [[nodiscard]] std::uint64_t retained_bound(std::uint32_t cells, bool ocean_surface=false) noexcept
        {
            const std::uint64_t side=cells+1u;
            // build_tile reserves grid+512 vertices and grid+1500 indices;
            // 64-cell skirts may grow the latter. Include vector growth slack.
            const std::uint64_t layers=ocean_surface?2u:1u;
            return layers*(2u*(side*side+512u)*sizeof(core::Vertex)
                +2u*(6u*cells*cells+1536u)*sizeof(std::uint32_t))+sizeof(Resident);
        }
        [[nodiscard]] std::uint64_t scratch_bound(std::uint32_t cells) noexcept
        {
            const std::uint64_t side=cells+1u;
            return side*side*(2u*sizeof(core::DVec3)+sizeof(core::SurfaceSample))+4u*cells*sizeof(std::uint32_t);
        }
        [[nodiscard]] bool wanted(const State& state, core::TileKey key)
        { return std::ranges::find(state.plan.needed,key)!=state.plan.needed.end(); }
        [[nodiscard]] const Resident* find(const State& state, core::TileKey key)
        {
            const auto at=std::ranges::find(state.resident,key,&Resident::key);
            return at==state.resident.end()?nullptr:&*at;
        }
        [[nodiscard]] bool valid_mesh(const core::Mesh& mesh, std::uint32_t cells, bool ocean_surface=false)
        {
            const std::size_t side=cells+1u;
            const std::size_t vertices=side*side+4u*cells,indices=6u*cells*cells+24u*cells;
            const bool water=mesh.vertices.size()==vertices*2u;
            if((mesh.vertices.size()!=vertices && !(ocean_surface&&water))
                || mesh.indices.size()!=indices*(water?2u:1u)
                || mesh_bytes(mesh)>retained_bound(cells,ocean_surface) || !core::finite(mesh.anchor)
                || !std::isfinite(mesh.bound_radius) || mesh.bound_radius<=0.0) return false;
            for(std::size_t i=0;i<mesh.vertices.size();++i)
            {
                const auto& vertex=mesh.vertices[i];
                if(!core::finite(vertex.position)||!core::finite(vertex.normal)||!core::finite(vertex.parent_delta)) return false;
                for(const auto value:vertex.color) if(!std::isfinite(value)) return false;
                if(vertex.color[3]!=(i>=vertices?1.0f:0.0f)) return false;
            }
            return std::ranges::all_of(mesh.indices,[&](auto index){return index<mesh.vertices.size();});
        }
    }
    [[nodiscard]] std::uint64_t resident_bytes(const State& state) noexcept
    {
        std::uint64_t result{};
        for(const auto& tile:state.resident) result+=detail::mesh_bytes(tile.mesh);
        return result;
    }
    // The caller serializes access to State. Jobs receive immutable Work copies;
    // only completion mutates residency, so old camera jobs cannot publish later.
    [[nodiscard]] Status update(State& state, const Request& request, std::stop_token stop={})
    {
        if(stop.stop_requested()) return Status::cancelled;
        if(request.body_index>=state.world.bodies().size() || request.cells_per_tile<2u || request.cells_per_tile>64u
            || request.maximum_in_flight<1u || request.maximum_in_flight>4u
            || request.maximum_resident_patches<6u || request.maximum_resident_patches>4096u
            || request.lod.max_leaves<6u || request.lod.max_leaves>2048u
            || request.maximum_bytes<detail::overhead || request.maximum_bytes>1024ull*1024ull*1024ull
            || state.generation==std::numeric_limits<std::uint64_t>::max()) return Status::invalid_request;
        if(request.ocean_surface&&!state.world.body(request.body_index).ocean) return Status::invalid_request;
        core::TerrainPlan plan;
        try {
            const auto* previous=state.configured&&request.ocean_surface==state.request.ocean_surface?&state.plan:nullptr;
            if(request.view)plan=core::plan_body_terrain(state.world,request.body_index,request.camera,request.seconds,
                request.viewport_height,request.vertical_fov,request.lod,previous,request.ocean_surface,*request.view);
            else plan=core::plan_body_terrain(state.world,request.body_index,request.camera,request.seconds,request.viewport_height,
                request.vertical_fov,request.lod,previous,request.ocean_surface);
        }
        catch(...) { return Status::invalid_request; }
        const auto body=state.world.body(request.body_index).id;
        std::erase_if(plan.needed,[&](auto key){return key.body!=body;});
        std::erase_if(plan.leaves,[&](auto key){return key.body!=body;});
        const auto required=detail::overhead+plan.needed.size()*detail::retained_bound(request.cells_per_tile,request.ocean_surface)
            +request.maximum_in_flight*detail::scratch_bound(request.cells_per_tile);
        if(plan.needed.size()>request.maximum_resident_patches || required>request.maximum_bytes
            || state.flight_reservation+resident_bytes(state)+detail::overhead>request.maximum_bytes) return Status::capacity_exceeded;
        if(stop.stop_requested()) return Status::cancelled;
        const bool same=state.configured && request.cells_per_tile==state.request.cells_per_tile
            && request.ocean_surface==state.request.ocean_surface
            && request.body_index==state.request.body_index && plan.leaves==state.plan.leaves;
        if(!same)
        {
            ++state.generation;
            if(!state.configured || request.cells_per_tile!=state.request.cells_per_tile
                || request.ocean_surface!=state.request.ocean_surface) state.resident.clear();
        }
        // Same inventory can have a new camera-priority order. Keep reusable
        // geometry and in-flight identity, but always replace that work order.
        state.plan=std::move(plan);
        if(!same) std::erase_if(state.resident,[&](const auto& tile){return !detail::wanted(state,tile.key);});
        state.request=request;
        state.configured=true;
        return Status::ready;
    }
    // Persisted patches pass the same structural and memory checks as job output.
    [[nodiscard]] Status restore(State& state, core::TileKey key, core::Mesh mesh)
    {
        if(!state.configured || !detail::wanted(state,key)) return Status::stale;
        if(detail::find(state,key)) return Status::ready;
        if(!detail::valid_mesh(mesh,state.request.cells_per_tile,
            state.request.ocean_surface&&state.world.body(key.body).ocean)) return Status::invalid_mesh;
        if(state.resident.size()>=state.request.maximum_resident_patches
            ||detail::overhead+resident_bytes(state)+state.flight_reservation+detail::mesh_bytes(mesh)>state.request.maximum_bytes)
            return Status::capacity_exceeded;
        state.resident.push_back({key,std::move(mesh)});
        return Status::ready;
    }
    [[nodiscard]] std::optional<Work> next_work(State& state)
    {
        if(!state.configured || state.in_flight.size()>=state.request.maximum_in_flight
            || state.serial==std::numeric_limits<std::uint64_t>::max()) return {};
        for(const auto key:state.plan.needed)
        {
            if(detail::find(state,key) || std::ranges::any_of(state.in_flight,
                [&](const auto& work){return work.key==key;})) continue;
            const auto reserve=detail::retained_bound(state.request.cells_per_tile,state.request.ocean_surface)
                +detail::scratch_bound(state.request.cells_per_tile);
            if(detail::overhead+resident_bytes(state)+state.flight_reservation+reserve>state.request.maximum_bytes) return {};
            const Work work{state.generation,state.serial+1u,key,state.request.cells_per_tile,state.request.ocean_surface};
            state.in_flight.push_back(work); // Allocation failure must not consume identity/reservation.
            ++state.serial;
            state.flight_reservation+=reserve;
            return work;
        }
        return {};
    }
    // Independent jobs capture this body value and Work before leaving the owner.
    // No State, request, plan, resident vector or Universe is read by the worker.
    [[nodiscard]] core::Mesh build(core::Body body, Work work, std::stop_token stop={})
    {
        if(stop.stop_requested() || body.id!=work.key.body || work.cells_per_tile<2u || work.cells_per_tile>64u) return {};
        body.ocean=body.ocean&&work.ocean_surface;
        return core::build_tile(body,work.key,{static_cast<int>(work.cells_per_tile),false,true},stop);
    }
    [[nodiscard]] core::Mesh build(const State& state, Work work, std::stop_token stop={})
    {
        if(stop.stop_requested() || work.key.body>=state.world.bodies().size()) return {};
        return build(state.world.body(work.key.body),work,stop);
    }
    // Release exactly one admitted identity, including failed/cancelled jobs.
    // The caller must first retire its execution and destroy any result buffers.
    [[nodiscard]] Status abandon(State& state, Work work)
    {
        const auto at=std::ranges::find(state.in_flight,work);
        if(at==state.in_flight.end()) return Status::stale;
        state.flight_reservation-=detail::retained_bound(work.cells_per_tile,work.ocean_surface)
            +detail::scratch_bound(work.cells_per_tile);
        state.in_flight.erase(at);
        return Status::cancelled;
    }
    [[nodiscard]] Status complete(State& state, Work work, core::Mesh mesh, std::stop_token stop={})
    {
        if(abandon(state,work)==Status::stale) return Status::stale;
        if(stop.stop_requested()) return Status::cancelled;
        if(work.generation!=state.generation || !detail::wanted(state,work.key)) return Status::stale;
        return restore(state,work.key,std::move(mesh));
    }
    // A parent remains until all four child regions are covered. Coarsening the
    // resident cut uses the upstream 2:1 neighbor rule and its retained ancestors.
    [[nodiscard]] std::vector<core::TileKey> draw_cut(const State& state)
    {
        if(!state.configured) return {};
        std::vector<core::TileKey> cut;
        const auto visit=[&](auto&& self,core::TileKey key)->void {
            if(!detail::find(state,key)) return;
            if(key.level<20u)
            {
                const auto children=core::children(key);
                if(std::ranges::all_of(children,[&](auto child){return detail::wanted(state,child)&&detail::find(state,child);}))
                { for(const auto child:children) self(self,child); return; }
            }
            cut.push_back(key);
        };
        const auto body=static_cast<std::uint8_t>(state.world.body(state.request.body_index).id);
        for(std::uint8_t face=0;face<6u;++face)
        {
            const core::TileKey root{0,0,body,face,0};
            if(!detail::find(state,root)) return {}; // Never publish an incomplete body.
            visit(visit,root);
        }
        return core::balance_cut(cut);
    }
    [[nodiscard]] Stats stats(const State& state)
    {
        const auto cut=draw_cut(state);
        Stats result{.generation=state.generation,.resident_bytes=resident_bytes(state),
            .reserved_bytes=state.flight_reservation,.maximum_bytes=state.request.maximum_bytes,
            .resident_patches=state.resident.size(),.draw_patches=cut.size(),.claimed_patches=state.in_flight.size(),.complete_coverage=!cut.empty()};
        for(const auto key:state.plan.needed) if(!detail::find(state,key)) ++result.pending_patches;
        if(!cut.empty())
        {
            result.minimum_level=20u;
            for(const auto key:cut) {result.minimum_level=(std::min)(result.minimum_level,key.level);result.maximum_level=(std::max)(result.maximum_level,key.level);}
        }
        return result;
    }
}
