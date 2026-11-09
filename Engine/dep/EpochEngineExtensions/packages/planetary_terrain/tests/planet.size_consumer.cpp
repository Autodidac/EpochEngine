// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Actual provider owners, offline: world scale is independent of avatar speed.
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <stop_token>
import planet.terrain;
import planet.coverage;
import planet.streaming;
import planet.exploration;
namespace core=epoch::extensions::planetary_terrain::core;
namespace coverage=epoch::extensions::planetary_terrain::coverage;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace exploration=epoch::extensions::planetary_terrain::exploration;
namespace {
std::size_t assertions{};
void require(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);}
bool near(double a,double b,double tolerance=1e-9){return std::abs(a-b)<=tolerance*(std::max)({1.0,std::abs(a),std::abs(b)});}
unsigned camera_level(const stream::State& state){
    const auto local=state.request.camera-state.world.center(0,state.request.seconds);
    const auto key=core::locate_tile(0,local,20);
    for(const auto tile:stream::draw_cut(state))if(core::contains(tile,key))return tile.level;
    return 0;
}
void build_work(stream::State& state,unsigned maximum=4096){
    unsigned built{};
    while(const auto job=stream::next_work(state)){
        require(++built<=maximum,"size streaming work unbounded");
        require(stream::complete(state,*job,stream::build(state,*job))==stream::Status::ready,"size patch completion failed");
    }
    require(stream::stats(state).pending_patches==0u,"size plan stalled");
}
}
int main(){try{
    const core::Universe original{1337},explicitDefault{1337,core::WorldConfig{}};
    require(original.config()==core::WorldConfig{}&&original.length_scale()==1.0,"default physical world changed");
    for(std::size_t index=0;index<6;++index){
        const auto& a=original.body(index);const auto& b=explicitDefault.body(index);
        require(a.radius==b.radius&&a.height_scale==b.height_scale&&a.orbit_radius==b.orbit_radius
            &&original.center(index,12345.0)==explicitDefault.center(index,12345.0),"explicit default differs");
    }
    for(const auto radius:std::array{250.0,25000.0,1737000.0,6371000.0,100000000.0})
        for(std::uint32_t reference=0;reference<6;++reference){
            const core::WorldConfig config{reference,radius};
            require(core::valid_world_config(config),"valid size refused");
            const core::Universe world{1337,config};
            const auto factor=radius/original.body(reference).radius;
            require(near(world.body(reference).radius,radius)&&world.config()==config,"requested reference radius lost");
            require(near(world.scaled_sun_radius(),original.scaled_sun_radius()*factor),"sun geometry not scaled");
            for(std::size_t index=0;index<6;++index){
                const auto& a=original.body(index);const auto& b=world.body(index);
                require(near(b.radius,a.radius*factor)&&near(b.height_scale,a.height_scale*factor)
                    &&near(b.orbit_radius,a.orbit_radius*factor)&&near(b.atmosphere_height,a.atmosphere_height*factor),"body geography scale mismatch");
                require(b.surface_gravity==a.surface_gravity&&b.orbit_period==a.orbit_period,"size changed gravity or period");
                require(core::length(world.center(index,12345.0)-original.center(index,12345.0)*factor)
                    <1e-12*(std::max)(1.0,core::length(world.center(index,12345.0))),
                    "orbital centers not scaled consistently");
                for(const auto direction:std::array{core::DVec3{1,.2,.1},core::DVec3{.2,1,.3},core::DVec3{-.4,.1,1}}){
                    const auto sample=core::sample_surface(b,direction),baseline=core::sample_surface(a,direction);
                    require(near(sample.height,baseline.height*factor),"terrain height not scaled");
                    require(sample.color==baseline.color,"size changed geographic colors");
                    require(near(world.ground_radius(index,direction),original.ground_radius(index,direction)*factor),"ground radius mismatch");
                }
            }
        }
    for(const auto config:std::array{core::WorldConfig{6,25000},core::WorldConfig{0,249.9},core::WorldConfig{0,100000001},
        core::WorldConfig{0,std::numeric_limits<double>::infinity()},core::WorldConfig{0,std::numeric_limits<double>::quiet_NaN()}}){
        require(!core::valid_world_config(config),"invalid size accepted");
        bool rejected{};try{core::Universe bad{1337,config};}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"invalid world constructor accepted");
    }
    const core::WorldConfig small{0,25000};
    exploration::Simulation simulation{1337,small};
    require(simulation.land(0)&&near(simulation.altitude(),1.8,1e-7),"small-world landing changed avatar height");
    simulation.set_time_scale(0);
    const auto start=simulation.state().local_position;
    for(unsigned tick=0;tick<60;++tick)simulation.step(1.0/60.0,{.forward=1.0});
    const auto after=simulation.state();
    const auto tangential=core::length(start/core::length(start)-after.local_position/core::length(after.local_position))*core::length(start);
    require(near(tangential,5.5,1e-4)&&near(simulation.altitude(),1.8,1e-7),"walk rate was scaled with planet");
    require(after.world==small&&exploration::valid_state(after),"size missing from simulation state");
    exploration::Simulation restored{1};restored.restore(after);
    require(restored.world().config()==small&&restored.state().local_position==after.local_position,"restore reset world size");
    auto malformed=after;malformed.world.reference_radius_meters=249;
    bool refused{};try{restored.restore(malformed);}catch(const std::invalid_argument&){refused=true;}
    require(refused&&restored.world().config()==small,"failed restore mutated size");
    exploration::Simulation tiny{1337,{4,250}};
    require(tiny.land(5)&&exploration::valid_state(tiny.state()),"valid small moon cannot be saved");
    coverage::Request request;request.mode=coverage::Mode::planet;request.cells_per_tile=4;request.world=small;
    const auto generated=coverage::build(request);
    require(static_cast<bool>(generated)&&generated.patches.size()==6,"custom coverage refused");
    for(const auto& patch:generated.patches)for(const auto& vertex:patch.mesh.vertices){
        const auto radius=core::length(patch.mesh.anchor+core::DVec3{vertex.position});
        require(radius>24900&&radius<25100,"coverage ignored custom physical radius");
    }
    request.maximum_bytes=1;
    require(coverage::build(request).status==coverage::Status::capacity_exceeded,"sizing bypassed artifact budget");
    request.maximum_bytes=64u*1024u*1024u;std::stop_source stop;stop.request_stop();
    require(coverage::build(request,stop.get_token()).status==coverage::Status::cancelled,"sizing bypassed cancellation");
    stream::State state{1337,small};stream::Request view;view.cells_per_tile=4;
    const auto center=state.world.center(0,0);
    view.camera=center+core::DVec3{0,25000*250.0,0};
    require(stream::update(state,view)==stream::Status::ready,"custom root plan refused");build_work(state);
    require(stream::draw_cut(state).size()==6,"custom far plan omitted faces");
    view.camera=center+core::cube_direction(5,.13,.09)*(25000.0+1.8);
    require(stream::update(state,view)==stream::Status::ready,"custom near plan refused");build_work(state);
    require(camera_level(state)>0&&stream::stats(state).resident_bytes<view.maximum_bytes,"custom near detail or admission broken");
    const auto generation=state.generation;
    auto low=view;low.maximum_bytes=1024u*1024u;
    require(stream::update(state,low)==stream::Status::capacity_exceeded&&state.generation==generation,"failed memory admission changed plan");
    require(stream::update(state,view,stop.get_token())==stream::Status::cancelled&&state.generation==generation,"cancelled size plan mutated state");
    std::ofstream result{"size-result.json"};result<<"{\"pass\":true,\"assertions\":"<<assertions
        <<",\"reference_radius_meters\":25000,\"walk_meters_per_second\":5.5,\"avatar_meters\":1.8,\"near_level\":"<<camera_level(state)
        <<",\"native_visual_quality_tested\":false}\n";return result?0:2;
}catch(const std::exception& error){std::ofstream failure{"size-failure.txt"};failure<<error.what();return 1;}}
