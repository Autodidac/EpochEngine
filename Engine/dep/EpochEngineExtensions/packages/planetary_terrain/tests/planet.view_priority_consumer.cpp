// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Real CPU planner consumer: view priority is not a renderer occlusion claim.
#include "epoch/planet/terrain.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
namespace p=epoch::planet;
static void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
static p::TileKey nearest(const p::TerrainPlan& plan,p::DVec3 direction) {
    const auto target=p::locate_tile(0,direction,20);unsigned count{};p::TileKey found{};
    for(const auto key:plan.leaves)if(p::contains(key,target)){++count;found=key;}
    require(count==1u,"camera needs exactly one covering leaf");return found;
}
static void coverage(const p::TerrainPlan& plan) {
    require(plan.leaves.size()<=384u,"view priority enlarged the leaf budget");
    std::array<double,6> area{};
    for(const auto key:plan.leaves) {
        require(key.level<=20u,"view priority exceeded the authored ceiling");
        area[key.face]+=1.0/double(1ull<<(2u*key.level));
        for(const auto edge:p::neighbors(key))for(const auto other:plan.leaves)
            if(p::contains(other,edge))require(key.level<=other.level+1u,"cut lost 2:1 balance");
    }
    for(const auto face:area)require(std::abs(face-1.0)<1e-10,"coarse body coverage was removed");
}
static double camera_cell(const p::Universe& world,p::TileKey key,p::DVec3 direction) {
    const auto mesh=p::build_tile(world.body(0),key,{16,false,true});
    const auto ground=direction*world.ground_radius(0,direction);
    std::size_t nearest{};double distance=std::numeric_limits<double>::max();
    for(std::size_t i=0;i<17u*17u;++i) {
        const auto d=p::length(mesh.anchor+p::DVec3{mesh.vertices[i].position}-ground);
        if(d<distance){distance=d;nearest=i;}
    }
    const auto next=nearest%17u==16u?nearest-1u:nearest+1u;
    return p::length(p::DVec3{mesh.vertices[next].position}-p::DVec3{mesh.vertices[nearest].position});
}
// Independent local ray oracle: bracket the first surface crossing, then bisect.
// These short downward test rays stay outside the limb and need no ray marcher.
static p::DVec3 local_hit(const p::Body& body,p::DVec3 eye,p::DVec3 ray,double range) {
    const auto gap=[&](double t) {
        const auto point=eye+ray*t;
        return p::length(point)-body.radius-std::max(0.,p::sample_height(body,p::normalized(point)));
    };
    double low{};
    for(unsigned i=1;i<=256u;++i) {
        double high=range*i/256.;
        if(gap(high)<=0.) {
            for(unsigned j=0;j<48u;++j) {
                const double middle=(low+high)*.5;
                if(gap(middle)>0.)low=middle;else high=middle;
            }
            return p::normalized(eye+ray*((low+high)*.5));
        }
        low=high;
    }
    throw std::runtime_error("test ray missed its local surface");
}
static int run(int argc,char**argv) {
    require(argc==2,"expected measurement path");p::Universe world{1337};const auto& body=world.body(0);
    std::ofstream out(argv[1]);out<<std::setprecision(17)<<"{\"cases\":[";bool first=true;
    unsigned improved{},equivalent{};
    for(unsigned face=0;face<6u;++face)for(const double altitude:{1.8,100.,1000.,10000.}) {
        const auto direction=p::cube_direction(static_cast<std::uint8_t>(face),.13,.09);
        const auto eye=world.center(0,0)+direction*(body.radius+std::max(0.,p::sample_height(body,direction))+altitude);
        const p::LodSettings settings{20u,384u,210.,.15};constexpr double fov=1.0471975511965976;
        const auto baseline=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true);
        const p::TerrainView view{direction*-1.,16./9.};
        const auto directed=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,view);
        coverage(baseline);coverage(directed);
        const auto old=nearest(baseline,direction),current=nearest(directed,direction);
        require(current.level>=old.level,"in-view detail regressed");
        if(altitude>=100.){if(current.level>old.level)++improved;}
        else {require(current.level==20u,"close camera did not reach the authored ceiling");++equivalent;}
        const auto normalized=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{view.forward*7.,view.aspect_ratio});
        require(normalized.leaves==directed.leaves,"forward magnitude changed detail");
        const auto equalFocal=p::plan_body_terrain(world,0,eye,0,2160,2*std::atan(2*std::tan(fov/2)),settings,nullptr,true);
        require(equalFocal.leaves==baseline.leaves,"equivalent viewport focal length changed legacy behavior");
        const auto dry=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr);
        const auto explicitDry=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,false);
        require(dry.leaves==explicitDry.leaves&&dry.needed==explicitDry.needed,"legacy overloads diverged");
        const auto away=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{direction,16./9.});
        coverage(away);
        for(const auto uv:std::array{p::DVec3{.999999,.999999,0},p::DVec3{-.999999,.999999,0}}) {
            const auto edgeDirection=p::cube_direction(static_cast<std::uint8_t>(face),uv.x,uv.y);
            const auto edgeEye=world.center(0,0)+edgeDirection*(body.radius+std::max(0.,p::sample_height(body,edgeDirection))+altitude);
            const auto edgePlan=p::plan_body_terrain(world,0,edgeEye,0,1080,fov,settings,nullptr,true,{edgeDirection*-1.,16./9.});
            coverage(edgePlan);(void)nearest(edgePlan,edgeDirection);
        }
        if(!first)out<<',';first=false;
        out<<"{\"face\":"<<face<<",\"altitude_meters\":"<<altitude<<",\"legacy_camera_level\":"<<unsigned(old.level)
           <<",\"view_camera_level\":"<<unsigned(current.level)<<",\"leaves\":"<<directed.leaves.size()
           <<",\"legacy_camera_cell_meters\":"<<camera_cell(world,old,direction)
           <<",\"view_camera_cell_meters\":"<<camera_cell(world,current,direction)<<'}';
    }
    out<<"],\"horizon_cases\":[";first=true;
    for(unsigned face=0;face<6u;++face)for(const double altitude:{1000.,10000.}) {
        const auto direction=p::cube_direction(static_cast<std::uint8_t>(face),.99,.3);
        const auto tangent=p::normalized(p::cross(direction,p::DVec3{0,1,0}));
        const double radius=body.radius+std::max(0.,p::sample_height(body,direction))+altitude;
        const double angle=.65*std::acos(body.radius/radius);
        const auto target=direction*std::cos(angle)+tangent*std::sin(angle);
        const auto localEye=direction*radius;
        const auto forward=p::normalized(target*world.ground_radius(0,target)-localEye);
        const auto eye=world.center(0,0)+localEye;
        const p::LodSettings settings{20u,384u,210.,.15};
        const auto legacy=p::plan_body_terrain(world,0,eye,0,1080,1.0471975511965976,settings,nullptr,true);
        const auto current=p::plan_body_terrain(world,0,eye,0,1080,1.0471975511965976,settings,nullptr,true,{forward,16./9.});
        coverage(current);
        const auto old=nearest(legacy,target),now=nearest(current,target);
        require(now.level>=old.level,"view priority lost a visible horizon target");
        if(!first)out<<',';first=false;
        out<<"{\"face\":"<<face<<",\"altitude_meters\":"<<altitude<<",\"legacy_target_level\":"<<unsigned(old.level)
           <<",\"view_target_level\":"<<unsigned(now.level)<<",\"leaves\":"<<current.leaves.size()<<'}';
    }
    out<<"],\"ray_target_cases\":[";first=true;
    for(unsigned face=0;face<6u;++face)for(const double altitude:{100.,1000.}) {
        const auto beneath=p::cube_direction(static_cast<std::uint8_t>(face),.13,.09);
        const auto tangent=p::normalized(p::cross(beneath,p::DVec3{0,1,0}));
        const auto target=p::normalized(beneath+tangent*(altitude/body.radius));
        const auto eye=beneath*(world.ground_radius(0,beneath)+altitude);
        const auto forward=p::normalized(target*world.ground_radius(0,target)-eye);
        const p::LodSettings settings{20u,384u,210.,.15};
        const auto aimed=p::plan_body_terrain(world,0,world.center(0,0)+eye,0,1080,1.0471975511965976,
            settings,nullptr,true,{forward,16./9.});
        const auto down=p::plan_body_terrain(world,0,world.center(0,0)+eye,0,1080,1.0471975511965976,
            settings,nullptr,true,{beneath*-1.,16./9.});
        coverage(aimed);coverage(down);
        const auto looked=nearest(aimed,target),under=nearest(aimed,beneath),old=nearest(down,target);
        require(looked.level>=under.level,"highest detail still follows the ground beneath the camera");
        require(looked.level>old.level,"turning toward a terrain target did not improve its detail");
        require(aimed.leaves!=down.leaves,"camera direction did not retarget the detail cut");
        if(!first)out<<',';first=false;
        out<<"{\"face\":"<<face<<",\"altitude_meters\":"<<altitude<<",\"ray_target_level\":"<<unsigned(looked.level)
            <<",\"beneath_level\":"<<unsigned(under.level)<<",\"down_view_target_level\":"<<unsigned(old.level)<<'}';
    }
    out<<"],\"grazing_ray_cases\":[";first=true;
    for(const double altitude:{1.8,100.})for(const double horizontal:{10.,100.}) {
        const auto beneath=p::cube_direction(0,.13,.09);
        const auto tangent=p::tangent(beneath);
        const auto target=p::normalized(beneath+tangent*(altitude*horizontal/body.radius));
        const auto localEye=beneath*(world.ground_radius(0,beneath)+altitude);
        const auto forward=p::normalized(target*world.ground_radius(0,target)-localEye);
        const p::LodSettings settings{20u,384u,210.,.15};
        const auto aimed=p::plan_body_terrain(world,0,world.center(0,0)+localEye,0,1080,1.0471975511965976,
            settings,nullptr,true,{forward,16./9.});
        const auto down=p::plan_body_terrain(world,0,world.center(0,0)+localEye,0,1080,1.0471975511965976,
            settings,nullptr,true,{-beneath,16./9.});
        coverage(aimed);coverage(down);
        const auto looked=nearest(aimed,target),under=nearest(aimed,beneath),old=nearest(down,target);
        require(looked.level>=under.level,"grazing ray refinement returned beneath the camera");
        require(looked.level>=old.level,"grazing ray target lost detail on camera turn");
        if(horizontal==100.)require(looked.level>old.level,"grazing ray exhausted its refinement work budget");
        if(!first)out<<',';first=false;
        out<<"{\"altitude_meters\":"<<altitude<<",\"horizontal_altitude_ratio\":"<<horizontal
           <<",\"ray_target_level\":"<<unsigned(looked.level)<<",\"beneath_level\":"<<unsigned(under.level)
           <<",\"down_view_target_level\":"<<unsigned(old.level)<<'}';
    }
    out<<"],\"footprint_cases\":[";first=true;unsigned lowerImproved{},nadirImproved{};
    for(unsigned face=0;face<6u;++face)for(const double altitude:{100.,1000.})for(const double roll:{0.,p::pi*.5}) {
        const auto beneath=p::cube_direction(static_cast<std::uint8_t>(face),.13,.09);
        const auto tangent=p::tangent(beneath);
        const auto target=p::normalized(beneath+tangent*(altitude/body.radius));
        const auto localEye=beneath*(body.radius+std::max(0.,p::sample_height(body,beneath))+altitude);
        const auto forward=p::normalized(target*(body.radius+std::max(0.,p::sample_height(body,target)))-localEye);
        const auto upright=p::normalized(beneath-forward*p::dot(beneath,forward));
        const auto up=p::rotate(upright,forward,roll);
        constexpr double fov=1.0471975511965976;
        const auto lower=p::normalized(forward-up);
        require(std::abs(p::dot(forward,lower)-std::sqrt(.5))<1e-12,"down ray is not 45 degrees from view");
        const auto centerHit=local_hit(body,localEye,forward,altitude*4.);
        const auto lowerHit=local_hit(body,localEye,lower,altitude*4.);
        const auto nadirHit=local_hit(body,localEye,-beneath,altitude*4.);
        const p::LodSettings settings{20u,384u,210.,.15};
        const auto eye=world.center(0,0)+localEye;
        const auto legacy=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{forward,16./9.});
        const auto started=std::chrono::steady_clock::now();
        const auto footprint=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{forward,16./9.,up});
        const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        coverage(footprint);
        const auto oldCenter=nearest(legacy,centerHit),oldLower=nearest(legacy,lowerHit),oldNadir=nearest(legacy,nadirHit);
        const auto newCenter=nearest(footprint,centerHit),newLower=nearest(footprint,lowerHit),newNadir=nearest(footprint,nadirHit);
        require(newLower.level>=oldLower.level,"footprint lost 45-degree detail");
        require(newNadir.level>=oldNadir.level,"footprint lost nadir detail");
        require(newCenter.level+2u>=oldCenter.level,"three-ray footprint lost more than two center levels");
        require(newCenter.level>=16u&&newLower.level>=16u&&newNadir.level>=16u,"footprint did not refine all three hits");
        if(newLower.level>oldLower.level)++lowerImproved;
        if(newNadir.level>oldNadir.level)++nadirImproved;
        const auto normalized=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,
            {forward*3.,16./9.,up*4.+forward*.1});
        require(normalized.leaves==footprint.leaves,"equivalent camera basis changed footprint");
        const auto away=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{-forward,16./9.,up});
        coverage(away);
        require(away.leaves!=footprint.leaves,"away footprint retained ground focus");
        if(!first)out<<',';first=false;
        out<<"{\"face\":"<<face<<",\"altitude_meters\":"<<altitude<<",\"roll_radians\":"<<roll
            <<",\"legacy_center_level\":"<<unsigned(oldCenter.level)<<",\"legacy_lower_level\":"<<unsigned(oldLower.level)
            <<",\"legacy_nadir_level\":"<<unsigned(oldNadir.level)
            <<",\"center_level\":"<<unsigned(newCenter.level)<<",\"lower_level\":"<<unsigned(newLower.level)
            <<",\"nadir_level\":"<<unsigned(newNadir.level)
            <<",\"planning_ms\":"<<milliseconds<<",\"leaves\":"<<footprint.leaves.size()<<'}';
    }
    require(lowerImproved>=12u,"footprint did not improve 45-degree detail across faces/roll");
    out<<"],\"center_miss_lower_hit_cases\":[";first=true;
    for(unsigned face=0;face<6u;++face) {
        const auto radial=p::cube_direction(static_cast<std::uint8_t>(face),.13,.09);
        // Above the full terrain envelope and pointing outwards: center cannot
        // hit the body. Camera-down and nadir must still refine outside its cone.
        const auto localEye=radial*(body.radius+25000.);
        const auto forward=p::normalized(p::tangent(radial)+radial*.04);
        const auto up=p::normalized(radial-forward*p::dot(radial,forward));
        constexpr double fov=1.0471975511965976;
        const auto lower=p::normalized(forward-up);
        const auto hit=local_hit(body,localEye,lower,300000.);
        const auto eye=world.center(0,0)+localEye;
        const p::LodSettings settings{20u,384u,210.,.15};
        const auto legacy=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{forward,16./9.});
        const auto footprint=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{forward,16./9.,up});
        coverage(footprint);
        const auto old=nearest(legacy,hit),now=nearest(footprint,hit);
        require(now.level>old.level,"lower focus was lost when the center ray missed");
        const auto oldNadir=nearest(legacy,radial),newNadir=nearest(footprint,radial);
        require(newNadir.level>oldNadir.level,"outside-cone nadir focus was discarded");
        const auto limited=p::plan_body_terrain(world,0,eye,0,1080,fov,{10u,384u,210.,.15},nullptr,true,{forward,16./9.,up});
        coverage(limited);
        for(const auto key:limited.leaves)require(key.level<=10u,"footprint exceeded a lower authored ceiling");
        if(!first)out<<',';first=false;
        out<<"{\"face\":"<<face<<",\"legacy_lower_level\":"<<unsigned(old.level)
            <<",\"lower_level\":"<<unsigned(now.level)<<",\"legacy_nadir_level\":"<<unsigned(oldNadir.level)
            <<",\"nadir_level\":"<<unsigned(newNadir.level)<<'}';
    }
    out<<"],\"orbital_broad_cases\":[";first=true;
    for(unsigned face=0;face<6u;++face)for(const double altitude:{100000.,1000000.}) {
        const auto radial=p::cube_direction(static_cast<std::uint8_t>(face),.13,.09);
        const auto localEye=radial*(world.ground_radius(0,radial)+altitude);
        const auto eye=world.center(0,0)+localEye;
        const auto forward=-radial,up=p::tangent(radial),right=p::normalized(p::cross(forward,up));
        constexpr double fov=1.0471975511965976;
        const p::LodSettings settings{20u,384u,210.,.15};
        const auto broad=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{forward,16./9.,up});
        coverage(broad);
        const auto rolled=p::plan_body_terrain(world,0,eye,0,1080,fov,settings,nullptr,true,{forward,16./9.,right});
        require(rolled.leaves==broad.leaves,"orbital cut still depends on the camera-down ray");
        unsigned minimum=20u,maximum{};std::vector<p::TileKey> sampled;
        for(const double x:{-.75,0.,.75})for(const double y:{-.45,0.,.45}) {
            const auto ray=p::normalized(forward+right*std::tan(fov*x)+up*std::tan(fov*y));
            const auto hit=local_hit(body,localEye,ray,altitude*2.);
            const auto key=nearest(broad,hit);
            minimum=std::min(minimum,unsigned(key.level));maximum=std::max(maximum,unsigned(key.level));
            if(std::find(sampled.begin(),sampled.end(),key)==sampled.end())sampled.push_back(key);
        }
        require(maximum<=minimum+2u,"orbital visible region collapsed to a single detail point");
        require(sampled.size()>=4u,"orbital central region did not span multiple refined tiles");
        if(!first)out<<',';first=false;
        out<<"{\"face\":"<<face<<",\"altitude_meters\":"<<altitude<<",\"minimum_region_level\":"<<minimum
           <<",\"maximum_region_level\":"<<maximum<<",\"distinct_region_tiles\":"<<sampled.size()<<'}';
    }
    // Cubic altitude weights have zero endpoint slope; centimetre changes on
    // either side of each default-Earth transition endpoint retain the cut.
    const auto transitionRadial=p::cube_direction(0,.13,.09);
    const auto transitionUp=p::tangent(transitionRadial);
    for(const double altitude:{5000.,100000.}) {
        const auto planAt=[&](double offset) {
            const auto eye=world.center(0,0)+transitionRadial*(world.ground_radius(0,transitionRadial)+altitude+offset);
            return p::plan_body_terrain(world,0,eye,0,1080,1.0471975511965976,
                {20u,384u,210.,.15},nullptr,true,{-transitionRadial,16./9.,transitionUp});
        };
        const auto before=planAt(-.01),after=planAt(.01);
        coverage(before);coverage(after);
        require(before.leaves==after.leaves,"altitude endpoint introduced an abrupt detail cut");
    }
    // Misses and interior cameras retain the complete bounded fallback cut.
    const auto fallbackUp=p::cube_direction(0,.13,.09);
    for(const auto localEye:std::array{fallbackUp*(body.radius+50000.),fallbackUp*(body.radius-50000.),p::DVec3{}})
        for(const auto forward:std::array{fallbackUp,p::tangent(fallbackUp),p::normalized(fallbackUp+p::tangent(fallbackUp))}) {
            coverage(p::plan_body_terrain(world,0,world.center(0,0)+localEye,0,1080,1.0471975511965976,
                {20u,384u,210.,.15},nullptr,true,{forward,16./9.}));
            coverage(p::plan_body_terrain(world,0,world.center(0,0)+localEye,0,1080,1.0471975511965976,
                {20u,384u,210.,.15},nullptr,true,{forward,16./9.,p::tangent(forward)}));
        }
    const auto camera=world.center(0,0)+p::DVec3{body.radius+1000.,0,0};
    for(const auto view:std::array{p::TerrainView{{0,0,0},1.},p::TerrainView{{1,0,0},0.},
        p::TerrainView{{1,0,0},11.},p::TerrainView{{std::numeric_limits<double>::quiet_NaN(),0,0},1.},
        p::TerrainView{{1,0,0},1.,p::DVec3{}},p::TerrainView{{1,0,0},1.,p::DVec3{1,0,0}},
        p::TerrainView{{1,0,0},1.,p::DVec3{0,std::numeric_limits<double>::quiet_NaN(),0}}}) {
        bool refused{};try{(void)p::plan_body_terrain(world,0,camera,0,1080,1.,{},nullptr,true,view);}
        catch(const std::invalid_argument&){refused=true;}require(refused,"invalid view accepted");
    }
    require(improved>=12u,"view priority did not improve the approach sample set");
    out<<"],\"altitude_blend_near_meters\":5000,\"altitude_blend_far_meters\":100000,\"approach_improved\":"<<improved<<",\"close_ceiling_retained\":"<<equivalent
       <<",\"lower_view_improved\":"<<lowerImproved
       <<",\"nadir_improved\":"<<nadirImproved<<",\"maximum_ray_steps_each\":16384,\"maximum_ray_steps_total\":49152"
       <<",\"complete_balanced_sphere\":true,\"leaf_budget\":384,\"gpu_or_native_acceptance\":false}\n";
    require(bool(out),"measurement write failed");
    return 0;
}
int main(int argc,char**argv) {
    try{return run(argc,argv);}catch(const std::exception& error) {
        if(argc==2)std::ofstream(std::string(argv[1])+".failure")<<error.what();return 86;
    }
}
