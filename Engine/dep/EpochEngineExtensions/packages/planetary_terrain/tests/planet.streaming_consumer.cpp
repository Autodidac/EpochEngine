// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Real independent C++23 consumer. No renderer, scheduler or model starts.
#include <algorithm>
#include <array>
#include <map>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <stop_token>
#include <tuple>
#include <vector>
import planet.terrain;
import planet.streaming;
namespace core=epoch::extensions::planetary_terrain::core;
namespace stream=epoch::extensions::planetary_terrain::streaming;
static void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
static void drain(stream::State& state)
{
    std::size_t count{};
    while(const auto work=stream::next_work(state))
    {
        require(++count<=4096u,"unbounded work loop");
        const auto before=stream::stats(state);
        require(before.resident_bytes+before.reserved_bytes+1024u*1024u<=before.maximum_bytes,"work exceeds admitted byte budget");
        auto mesh=stream::build(state,*work);
        require(stream::complete(state,*work,std::move(mesh))==stream::Status::ready,"real mesh completion failed");
    }
    require(stream::stats(state).pending_patches==0u,"admitted plan could not finish");
}
static void verify_cut(const stream::State& state)
{
    const auto cut=stream::draw_cut(state);
    require(!cut.empty(),"complete body is missing");
    double area{};
    double face_area[6]{};
    for(const auto key:cut)
    {
        require(stream::detail::find(state,key)!=nullptr,"draw cut references an absent ancestor");
        area+=1.0/static_cast<double>(1ull<<(2u*key.level));
        face_area[key.face]+=1.0/static_cast<double>(1ull<<(2u*key.level));
        for(const auto other:cut) if(key!=other) require(!core::contains(key,other),"parent and child overlap");
        for(const auto neighbor:core::neighbors(key))
            for(const auto other:cut)
                if(core::contains(other,neighbor)) require(key.level<=other.level+1u,"resident cut is not 2:1 balanced");
    }
    require(std::abs(area-6.0)<1e-10,"resident cut contains a hole");
    for(const auto face:face_area) require(std::abs(face-1.0)<1e-10,"a cube face is incomplete");
}
static void verify_shared_surface(const stream::State& state)
{
    const auto cells=state.request.cells_per_tile, side=cells+1u;
    std::map<std::array<long long,3>,core::DVec3> shared;
    std::size_t matched{};
    for(const auto key:stream::draw_cut(state)) {
        const auto* tile=stream::detail::find(state,key);require(tile!=nullptr,"seam tile is not resident");
        for(std::uint32_t y=0;y<=cells;++y)for(std::uint32_t x=0;x<=cells;++x) {
            if(x!=0u&&x!=cells&&y!=0u&&y!=cells)continue;
            const auto d=core::tile_direction(key,double(x)/cells,double(y)/cells);
            const std::array<long long,3> quantized{std::llround(d.x*1e10),std::llround(d.y*1e10),std::llround(d.z*1e10)};
            const auto point=tile->mesh.anchor+core::DVec3{tile->mesh.vertices[y*side+x].position};
            const auto [at,inserted]=shared.emplace(quantized,point);
            if(!inserted) {
                ++matched;
                // Tile-local float coordinates have a finite metric precision
                // at an Earth-sized radius; shared geographic vertices must
                // agree within their existing float conversion allowance.
                require(core::length(point-at->second)<2.0,"shared terrain edge vertices do not meet");
            }
        }
    }
    require(matched>0u,"seam comparison did not exercise shared patch vertices");
}
struct PriorityProof final {
    unsigned nearest_level{},legacy_level{},relocated_face{},fine_nearest_level{},fine_legacy_level{},fine_settled_level{};
    std::size_t reused{},fine_needed{},fine_resident{},fine_leaves{};
};
static unsigned camera_level(const stream::State& state)
{
    const auto camera=state.request.camera-state.world.center(state.request.body_index,state.request.seconds);
    const auto target=core::locate_tile(static_cast<std::uint8_t>(state.request.body_index),camera,20);
    for(const auto key:stream::draw_cut(state))if(core::contains(key,target))return key.level;
    return 0;
}
struct OceanPriorityProof final {unsigned dry_level{},wet_level{};std::size_t leaves{},resident{};std::uint64_t bytes{};double ground_height{};};
static OceanPriorityProof verify_ocean_priority()
{
    stream::State dry{1337},wet{1337};const auto& body=wet.world.body(0);
    core::DVec3 direction{};double seabed{};bool found{};
    for(std::uint8_t face=0;face<6u&&!found;++face)for(unsigned x=0;x<8u&&!found;++x)for(unsigned y=0;y<8u;++y)
    {
        const auto point=core::cube_direction(face,(double(x)+.5)/4.0-1.0,(double(y)+.5)/4.0-1.0);
        const auto height=core::sample_height(body,point);if(height>=-100.0)continue;
        direction=point;seabed=height;found=true;break;
    }
    require(found,"ocean LOD test found no actual submerged terrain");
    stream::Request request;request.camera=wet.world.center(0,0)+direction*(body.radius+1.8);
    request.viewport_height=720;request.vertical_fov=1.05;
    request.lod={.max_level=18,.max_leaves=384,.patch_pixels=210.0,.hysteresis=0.15};
    request.maximum_resident_patches=4096;request.maximum_in_flight=4;request.maximum_bytes=128u*1024u*1024u;
    const auto original=core::plan_body_terrain(dry.world,0,request.camera,0,720,1.05,request.lod);
    const auto explicit_dry=core::plan_body_terrain(dry.world,0,request.camera,0,720,1.05,request.lod,nullptr,false);
    require(original.needed==explicit_dry.needed&&original.leaves==explicit_dry.leaves&&original.signature==explicit_dry.signature,
        "explicit dry planning changed the original plan or ordering");
    require(stream::update(dry,request)==stream::Status::ready,"near-sea dry plan refused");
    request.ocean_surface=true;
    require(stream::update(wet,request)==stream::Status::ready,"near-sea ocean plan refused");
    const auto planned_level=[&](const stream::State& state){
        const auto target=core::locate_tile(0,direction,20);
        for(const auto key:state.plan.leaves)if(core::contains(key,target))return unsigned(key.level);
        return 0u;
    };
    const auto dry_level=planned_level(dry);
    require(dry_level<18u&&planned_level(wet)==18u,"ocean camera still refines by seabed distance");
    drain(wet);verify_cut(wet);const auto stats=stream::stats(wet);
    require(camera_level(wet)==18u&&stats.pending_patches==0u,"requested ocean detail was not delivered");
    require(wet.plan.leaves.size()<=384u&&stats.resident_bytes+1024u*1024u<=request.maximum_bytes,
        "ocean detail exceeded the original leaf or memory budget");
    require(core::sample_height(body,direction)==seabed,"ocean LOD changed physical terrain height");
    const OceanPriorityProof proof{dry_level,camera_level(wet),wet.plan.leaves.size(),wet.resident.size(),stats.resident_bytes,seabed};
    request.ocean_surface=false;
    require(stream::update(wet,request)==stream::Status::ready,"ocean-off plan refused");
    require(wet.plan.needed==original.needed&&wet.plan.leaves==original.leaves&&wet.plan.signature==original.signature,
        "water-off retained ocean-only refinement or hysteresis");
    return proof;
}
// The pre-repair implementation, retained only as an executable negative
// control: level/key inventory order ignored the camera and publication cut.
static std::optional<stream::Work> legacy_work(stream::State& state)
{
    auto needed=state.plan.needed;
    std::sort(needed.begin(),needed.end(),[](auto a,auto b){return std::tie(a.level,a.body,a.face,a.y,a.x)
        <std::tie(b.level,b.body,b.face,b.y,b.x);});
    for(const auto key:needed)if(!stream::detail::find(state,key))
    {
        state.flight_reservation=stream::detail::retained_bound(state.request.cells_per_tile,state.request.ocean_surface)
            +stream::detail::scratch_bound(state.request.cells_per_tile);
        state.in_flight.push_back(stream::Work{state.generation,++state.serial,key,state.request.cells_per_tile,state.request.ocean_surface});
        return state.in_flight.back();
    }
    return {};
}
static PriorityProof verify_camera_priority()
{
    stream::State state{1337};
    stream::Request view;view.cells_per_tile=4;
    const auto center=state.world.center(0,0.0);
    const auto radius=state.world.body(0).radius;
    view.camera=center+core::DVec3{0,radius*250.0,0};
    require(stream::update(state,view)==stream::Status::ready,"priority roots refused");
    drain(state);
    view.lod.max_level=1;view.lod.max_leaves=24;
    view.camera=center+core::cube_direction(5,.13,.09)*(radius+500.0);
    require(stream::update(state,view)==stream::Status::ready,"priority near view refused");
    const auto work=stream::next_work(state);
    require(work&&work->key.face==5u&&work->key.level==1u,
        "nearest visible sibling group did not precede fixed face/key order");
    require(stream::complete(state,*work,stream::build(state,*work))==stream::Status::ready,"priority first job failed");
    verify_cut(state);
    // Both algorithms start with the same six real root meshes and exact plan.
    stream::State prioritized{1337},baseline{1337};
    view.lod.max_level=12;view.lod.max_leaves=96;
    require(stream::update(prioritized,view)==stream::Status::ready,"depth plan refused");
    for(const auto& tile:state.resident)if(tile.key.level==0)
        require(stream::restore(prioritized,tile.key,tile.mesh)==stream::Status::ready,"warm root restore failed");
    baseline=prioritized;
    for(unsigned i=0;i<32u;++i)
    {
        const auto next=stream::next_work(prioritized),old=legacy_work(baseline);
        require(next&&old,"bounded comparison did not exercise equal patch counts");
        require(stream::complete(prioritized,*next,stream::build(prioritized,*next))==stream::Status::ready,"prioritized depth job failed");
        require(stream::complete(baseline,*old,stream::build(baseline,*old))==stream::Status::ready,"baseline depth job failed");
        verify_cut(prioritized);verify_cut(baseline);
    }
    PriorityProof proof{camera_level(prioritized),camera_level(baseline)};
    require(proof.nearest_level>proof.legacy_level,"nearest region did not refine sooner than faulty baseline at equal work");
    // Moving to another face keeps every still-wanted cached mesh. New work
    // follows the new camera instead of draining the old ordering.
    const auto old_resident=prioritized.resident;
    view.camera=center+core::cube_direction(0,.13,.09)*(radius+500.0);
    require(stream::update(prioritized,view)==stream::Status::ready,"relocation plan refused");
    for(const auto& tile:old_resident)if(stream::detail::wanted(prioritized,tile.key))
    {++proof.reused;require(stream::detail::find(prioritized,tile.key)!=nullptr,"relocation rebuilt reusable mesh");}
    require(proof.reused>=6u,"relocation lost fallback coverage");verify_cut(prioritized);
    const auto relocated=stream::next_work(prioritized);
    require(relocated&&relocated->key.face==0u,"warm-cache relocation did not reprioritize work");
    proof.relocated_face=relocated->key.face;
    require(stream::complete(prioritized,*relocated,stream::build(prioritized,*relocated))==stream::Status::ready,"relocation completion failed");
    drain(prioritized);verify_cut(prioritized);
    // A camera move need not change the inventory. It still must change work
    // priority, without invalidating useful in-flight geometry or cache bytes.
    stream::State unchanged{1337};view.lod.max_level=1;view.lod.max_leaves=24;
    require(stream::update(unchanged,view)==stream::Status::ready,"unchanged-plan setup refused");
    for(const auto& tile:state.resident)if(tile.key.level==0)
        require(stream::restore(unchanged,tile.key,tile.mesh)==stream::Status::ready,"unchanged-plan root restore failed");
    const auto before_move=stream::next_work(unchanged);require(before_move&&before_move->key.face==0u,"unchanged-plan setup priority");
    const auto generation=unchanged.generation,bytes=stream::resident_bytes(unchanged);
    view.camera=center+core::cube_direction(5,.13,.09)*(radius+500.0);
    require(stream::update(unchanged,view)==stream::Status::ready&&unchanged.generation==generation
        &&stream::resident_bytes(unchanged)==bytes,"same-inventory camera move discarded valid state");
    require(!stream::next_work(unchanged),"camera reprioritization started a second in-flight job");
    require(stream::complete(unchanged,*before_move,stream::build(unchanged,*before_move))==stream::Status::ready,
        "same-inventory camera move rejected reusable job");
    const auto after_move=stream::next_work(unchanged);
    require(after_move&&after_move->key.face==5u,"same-inventory camera move kept stale priority");
    require(stream::complete(unchanged,*after_move,stream::build(unchanged,*after_move))==stream::Status::ready,"same-inventory job failed");
    drain(unchanged);verify_cut(unchanged);
    const auto settled=unchanged.generation;
    require(stream::update(unchanged,view)==stream::Status::ready&&unchanged.generation==settled
        &&!stream::next_work(unchanged),"repeated settled update duplicated work");
    // The actual original Fine profile, at the existing 500m benchmark view.
    // Cell count is deliberately cheap: this tests work order and publication,
    // not geometry generation speed or a full renderer frame-time claim.
    stream::State fine{1337};view.camera=center+core::DVec3{0,radius+500.0,0};
    view.lod={.max_level=19,.max_leaves=576,.patch_pixels=170.0,.hysteresis=.15};
    view.maximum_resident_patches=4096;view.maximum_bytes=192u*1024u*1024u;
    require(stream::update(fine,view)==stream::Status::ready,"Fine priority plan refused");
    for(const auto& tile:state.resident)if(tile.key.level==0)
        require(stream::restore(fine,tile.key,tile.mesh)==stream::Status::ready,"Fine warm root restore failed");
    auto fine_baseline=fine;
    std::ofstream trace{"priority-work.txt"};
    for(unsigned i=0;i<96u;++i)
    {
        const auto next=stream::next_work(fine),old=legacy_work(fine_baseline);
        require(next&&old,"Fine bounded comparison ran out of patches");
        require(stream::complete(fine,*next,stream::build(fine,*next))==stream::Status::ready,"Fine prioritized completion failed");
        require(stream::complete(fine_baseline,*old,stream::build(fine_baseline,*old))==stream::Status::ready,"Fine legacy completion failed");
        trace<<i<<" face "<<unsigned(next->key.face)<<" level "<<unsigned(next->key.level)<<" xy "<<next->key.x<<' '<<next->key.y
            <<" near "<<camera_level(fine)<<" legacy "<<camera_level(fine_baseline)<<'\n';
        verify_cut(fine);verify_cut(fine_baseline);
    }
    proof.fine_nearest_level=camera_level(fine);proof.fine_legacy_level=camera_level(fine_baseline);
    {std::ofstream diagnostic{"priority-comparison.json"};diagnostic<<"{\"nearest\":"<<proof.fine_nearest_level
        <<",\"legacy\":"<<proof.fine_legacy_level<<",\"draw\":"<<stream::draw_cut(fine).size()<<"}\n";}
    require(proof.fine_nearest_level>proof.fine_legacy_level,"Fine nearby detail did not improve at equal96patch work");
    drain(fine);verify_cut(fine);
    proof.fine_settled_level=camera_level(fine);proof.fine_needed=fine.plan.needed.size();
    proof.fine_resident=fine.resident.size();proof.fine_leaves=fine.plan.leaves.size();
    require(proof.fine_resident==proof.fine_needed&&stream::draw_cut(fine).size()==proof.fine_leaves,
        "Fine queue stopped before all requested patches became drawable");
    for(const auto key:stream::draw_cut(fine))require(std::ranges::find(fine.plan.leaves,key)!=fine.plan.leaves.end(),
        "Fine settled cut differs from requested leaves");
    return proof;
}
int main()
{
    try
    {
        const auto priority=verify_camera_priority();
        const auto ocean_priority=verify_ocean_priority();
        stream::State state{1337};
        const auto center=state.world.center(0,0.0);
        const auto radius=state.world.body(0).radius;
        stream::Request far;
        far.cells_per_tile=4;
        far.camera=center+core::DVec3{0,radius*250.0,0};
        require(stream::update(state,far)==stream::Status::ready,"far plan refused");
        require(stream::draw_cut(state).empty(),"cold state published a partial body");
        drain(state);verify_cut(state);
        const auto coarse=stream::stats(state);
        require(coarse.maximum_level==0u&&coarse.draw_patches==6u,"distant body was not coarse");
        auto near=far;near.lod.max_leaves=48;near.camera=center+core::DVec3{0,radius+500.0,0};
        require(stream::update(state,near)==stream::Status::ready,"near plan refused");
        const auto pending=stream::stats(state);
        require(pending.pending_patches>0u,"distance did not request additional terrain detail");
        verify_cut(state); // Old parents remain while their children stream in.
        const auto first=stream::next_work(state);
        require(first.has_value(),"near plan did not start work");
        auto first_mesh=stream::build(state,*first);
        require(stream::complete(state,*first,std::move(first_mesh))==stream::Status::ready,"incremental mesh failed");
        verify_cut(state);
        drain(state);verify_cut(state);
        const auto fine=stream::stats(state);
        require(fine.maximum_level>coarse.maximum_level&&fine.draw_patches>coarse.draw_patches,"near draw cut did not refine");
        require(fine.maximum_level>2u,"selected-body leaf budget was spent on invisible worlds; near terrain remains level2");
        const auto nearGeneration=state.generation;
        require(stream::update(state,near)==stream::Status::ready&&state.generation==nearGeneration
            && !stream::next_work(state),"identical view rebuilt or invalidated settled terrain");
        const core::DVec3 directions[]{core::cube_direction(0,0,0),core::cube_direction(0,1,0),
            core::cube_direction(0,1,1),core::cube_direction(0,.13,.09)};
        for(const auto direction:directions) {
            auto surface=near;surface.camera=center+direction*(radius+500.0);
            require(stream::update(state,surface)==stream::Status::ready,"surface-angle plan refused");
            drain(state);verify_cut(state);verify_shared_surface(state);
            require(stream::stats(state).maximum_level>2u&&state.plan.leaves.size()<=48u,
                "surface-angle detail regressed or exceeded selected-body leaf budget");
            for(const auto key:state.plan.needed)require(key.body==0,"invisible world consumed selected-body residency");
        }
        auto centerView=near;centerView.camera=center;
        require(stream::update(state,centerView)==stream::Status::ready,"finite central camera failed bounded planning");
        drain(state);verify_cut(state);
        auto rootsOnly=far;rootsOnly.lod.max_leaves=6;
        require(stream::update(state,rootsOnly)==stream::Status::ready,"six-face selected-body budget refused");
        drain(state);verify_cut(state);
        require(stream::stats(state).draw_patches==6u,"root-only selected-body budget did not preserve whole body");
        require(stream::update(state,far)==stream::Status::ready,"coarsening refused");
        require(stream::stats(state).resident_patches==6u,"unneeded detail remained resident after coarsening");
        verify_cut(state);
        require(stream::update(state,near)==stream::Status::ready,"second near plan refused");
        const auto stale=stream::next_work(state);require(stale.has_value(),"stale test missing work");
        auto stale_mesh=stream::build(state,*stale);
        require(stream::update(state,far)==stream::Status::ready,"new camera plan refused");
        require(!stream::next_work(state),"old in-flight allocation lost its reservation");
        require(stream::complete(state,*stale,std::move(stale_mesh))==stream::Status::stale,"old camera work overwrote current residency");
        require(stream::stats(state).reserved_bytes==0u,"stale job reservation not retired");
        const auto generation=state.generation;
        auto impossible=near;impossible.maximum_bytes=1024u*1024u;
        require(stream::update(state,impossible)==stream::Status::capacity_exceeded,"insufficient memory was admitted");
        require(state.generation==generation&&stream::stats(state).resident_patches==6u,"failed admission destroyed current terrain");
        auto invalid=near;invalid.camera.x=std::numeric_limits<double>::quiet_NaN();
        require(stream::update(state,invalid)==stream::Status::invalid_request,"nonfinite camera admitted");
        std::stop_source stop;stop.request_stop();
        require(stream::update(state,near,stop.get_token())==stream::Status::cancelled,"cancelled plan changed terrain");
        require(stream::update(state,near)==stream::Status::ready,"cancel test plan refused");
        const auto cancelled=stream::next_work(state);require(cancelled.has_value(),"cancel test missing work");
        require(stream::build(state,*cancelled,stop.get_token()).vertices.empty(),"cancelled job created a mesh");
        require(stream::complete(state,*cancelled,{},stop.get_token())==stream::Status::cancelled,"cancelled completion accepted");
        require(stream::stats(state).reserved_bytes==0u,"cancelled job reservation leaked");
        drain(state);verify_cut(state);
        stream::State water{1337};
        require(stream::update(water,far)==stream::Status::ready,"dry compatibility setup refused");
        drain(water);
        auto wet=far;wet.ocean_surface=true;
        require(stream::update(water,wet)==stream::Status::ready,"opaque ocean plan refused");
        require(water.resident.empty(),"old terrain-only cache survived ocean identity change");
        drain(water);verify_cut(water);
        std::size_t waterVertices{};
        core::DVec3 deepestDirection{};
        double deepestHeight{};
        const std::size_t surface=25u,layer=41u;
        for(const auto& resident:water.resident) {
            require(stream::detail::valid_mesh(resident.mesh,4,true),"water mesh failed bounded validation");
            if(resident.mesh.vertices.size()==layer*2u) {
                require(!stream::detail::valid_mesh(resident.mesh,4,false),"water cache admitted as terrain-only");
                for(std::size_t i=layer;i<layer+surface;++i) {
                    const auto& vertex=resident.mesh.vertices[i];
                    const auto point=resident.mesh.anchor+core::DVec3{vertex.position};
                    require(std::abs(core::length(point)-radius)<1.0,"streamed ocean left sea level");
                    require(vertex.color[0]==0.015f&&vertex.color[1]==0.085f&&vertex.color[2]==0.12f
                        &&vertex.color[3]==1.0f,"streamed ocean lost opaque blue color");
                    const core::DVec3 direction{vertex.normal};
                    const auto height=core::sample_height(water.world.body(0),direction);
                    if(height<deepestHeight){deepestHeight=height;deepestDirection=direction;}
                    ++waterVertices;
                }
            }
        }
        require(waterVertices>0u,"streamed ocean contains no actual water layer");
        for(std::size_t body=1;body<water.world.bodies().size();++body) {
            auto unsupported=wet;unsupported.body_index=body;
            const auto generation=water.generation;
            require(stream::update(water,unsupported)==stream::Status::invalid_request&&water.generation==generation,
                "unsupported wet body was silently admitted or replaced current state");
        }
        auto undersized=wet;
        undersized.maximum_bytes=stream::detail::overhead+water.plan.needed.size()*stream::detail::retained_bound(4)
            +stream::detail::scratch_bound(4);
        const auto waterGeneration=water.generation;
        require(stream::update(water,undersized)==stream::Status::capacity_exceeded
            &&water.generation==waterGeneration,"water layers exceeded admitted memory or replaced valid state");
        auto wetNear=near;wetNear.ocean_surface=true;
        require(stream::update(water,wetNear)==stream::Status::ready,"near ocean refused");
        const auto wetWork=stream::next_work(water);require(wetWork.has_value(),"wet stale test has no work");
        auto wetMesh=stream::build(water,*wetWork);
        require(stream::update(water,far)==stream::Status::ready,"terrain-only switch refused");
        require(stream::complete(water,*wetWork,std::move(wetMesh))==stream::Status::stale,"old ocean job replaced terrain-only state");
        require(stream::build(water,*wetWork,stop.get_token()).vertices.empty(),"cancelled ocean job allocated geometry");
        require(deepestHeight< -50.0,"high-detail ocean test has no submerged location");
        core::TileKey high=core::locate_tile(0,deepestDirection,14);
        const stream::Work highWork{1,1,high,4,true};
        const auto highMesh=stream::build(water,highWork);
        require(stream::detail::valid_mesh(highMesh,4,true)&&highMesh.vertices.size()==layer*2u,
            "high LOD ocean geometry exceeded its real shape budget or lost its sea layer");
        std::ofstream output{"streaming-result.json"};
        output<<"{\"cpu_streaming_pass\":true,\"coarse_patches\":"<<coarse.draw_patches
            <<",\"fine_patches\":"<<fine.draw_patches<<",\"fine_maximum_level\":"<<unsigned(fine.maximum_level)
            <<",\"ocean_surface_vertices\":"<<waterVertices<<",\"ocean_high_level\":14"
            <<",\"equal_work_patches\":32,\"nearest_camera_level\":"<<priority.nearest_level
            <<",\"legacy_camera_level\":"<<priority.legacy_level<<",\"relocation_reused_patches\":"<<priority.reused
            <<",\"fine_equal_work_patches\":96,\"fine_nearest_camera_level\":"<<priority.fine_nearest_level
            <<",\"fine_legacy_camera_level\":"<<priority.fine_legacy_level
            <<",\"fine_settled_camera_level\":"<<priority.fine_settled_level<<",\"fine_needed\":"<<priority.fine_needed
            <<",\"fine_resident\":"<<priority.fine_resident<<",\"fine_leaves\":"<<priority.fine_leaves
            <<",\"sea_camera_dry_level\":"<<ocean_priority.dry_level<<",\"sea_camera_delivered_wet_level\":"<<ocean_priority.wet_level
            <<",\"sea_camera_leaves\":"<<ocean_priority.leaves<<",\"sea_camera_resident\":"<<ocean_priority.resident
            <<",\"sea_camera_resident_bytes\":"<<ocean_priority.bytes<<",\"sea_camera_ground_height\":"<<ocean_priority.ground_height
            <<",\"bounded_residency\":true,\"stale_rejection\":true,\"cancellation\":true,\"native_editor_acceptance\":false}\n";
        return output?0:2;
    }
    catch(const std::exception& error){std::ofstream failure{"streaming-failure.txt"};failure<<error.what();return 1;}
}
