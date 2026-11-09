// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cmath>
#include <limits>
#include <stop_token>
import asset.cloud_volume;
namespace volume=epochengine::asset::cloud_volume;
int main()
{
    constexpr double radius=6371000.0;
    const volume::Recipe recipe{77u,2700.0,2160.0,12742.0,1.0f,0.0012f};
    if(!volume::valid(recipe,radius))return 1;
    const volume::Point midpoint{0.0,radius+3780.0,0.0};
    const auto sampled=volume::density(recipe,radius,midpoint,12.0);
    if(!(sampled>0.0&&sampled<=1.0))return 2;
    if(volume::density(recipe,radius,{0.0,radius,0.0},12.0)!=0.0
        ||volume::density(recipe,radius,{0.0,radius+7000.0,0.0},12.0)!=0.0)return 3;
    const volume::Point origin{radius+10000.0,0.0,0.0},direction{-1.0,0.0,0.0};
    const auto all=volume::intersect(recipe,radius,origin,direction,1e8);
    if(!all.valid||all.count!=2u||std::abs(all.intervals[0].begin-5140.0)>1e-5
        ||std::abs(all.intervals[0].end-7300.0)>1e-5)return 4;
    const auto clipped=volume::intersect(recipe,radius,origin,direction,6500.0);
    if(!clipped.valid||clipped.count!=1u||clipped.intervals[0].end!=6500.0)return 5;
    const auto blocked=volume::transmittance(recipe,radius,origin,direction,5000.0,10.0);
    if(!blocked.valid||blocked.transmittance!=1.0||blocked.samples)return 6;
    const auto near=volume::transmittance(recipe,radius,origin,direction,10000.0,10.0);
    if(!near.valid||near.samples!=64u||near.transmittance<=0.0||near.transmittance>=1.0)return 7;
    const auto complete=volume::transmittance(recipe,radius,origin,direction,1e8,10.0);
    if(!complete.valid||complete.transmittance>=near.transmittance)return 8;
    auto empty=recipe;empty.coverage=0;
    if(volume::transmittance(empty,radius,origin,direction,1e8,10.0).transmittance!=1.0)return 9;
    auto transparent=recipe;transparent.extinction_per_meter=0;
    if(volume::transmittance(transparent,radius,origin,direction,1e8,10.0).transmittance!=1.0)return 10;
    const auto inside=volume::intersect(recipe,radius,midpoint,{0.0,1.0,0.0},100000.0);
    if(!inside.valid||inside.count!=1u||inside.intervals[0].begin!=0.0||std::abs(inside.intervals[0].end-1080.0)>1e-4)return 11;
    if(volume::intersect(recipe,radius,origin,{0.0,1.0,0.0},1e8).count)return 12;
    if(volume::intersect(recipe,radius,origin,{0.0,2.0,0.0},1e8).valid)return 13;
    if(volume::transmittance(recipe,radius,origin,direction,1e8,0.0,65u).valid)return 14;
    std::stop_source stop;stop.request_stop();
    const auto stopped=volume::transmittance(recipe,radius,origin,direction,1e8,0.0,64u,stop.get_token());
    if(stopped.valid||!stopped.cancelled||stopped.samples)return 15;
    if(volume::density(recipe,radius,midpoint,NAN)!=0.0||volume::density(recipe,radius,midpoint,-1.0)!=0.0)return 16;
    auto bad=recipe;bad.horizontal_scale_meters=0.0;if(volume::valid(bad,radius))return 17;
    bad=recipe;bad.extinction_per_meter=NAN;if(volume::valid(bad,radius))return 18;
    bad=recipe;bad.coverage=1.1f;if(volume::valid(bad,radius))return 19;
    bad=recipe;bad.wind_period_seconds=0.0;if(volume::valid(bad,radius))return 20;
    // Seed and deterministic time are meaningful even when a ray is unchanged.
    auto patterned=recipe;patterned.coverage=0.6f;
    bool seedChanged{},timeChanged{};
    for(unsigned i=0;i<16u;++i)
    {
        const double angle=i*0.37;const volume::Point point{(radius+3780.0)*std::cos(angle),0.0,(radius+3780.0)*std::sin(angle)};
        const auto first=volume::density(patterned,radius,point,234.0);
        if(first!=volume::density(patterned,radius,point,234.0+patterned.wind_period_seconds))return 21;
        auto different=patterned;++different.seed;
        seedChanged=seedChanged||std::abs(first-volume::density(different,radius,point,234.0))>1e-4;
        timeChanged=timeChanged||std::abs(first-volume::density(patterned,radius,point,234.0+50000.0))>1e-4;
    }
    if(!seedChanged||!timeChanged)return 22;
    if(volume::density(patterned,radius,midpoint,0.0,-1.0)!=0.0
        ||volume::density(patterned,radius,midpoint,0.0,NAN)!=0.0)return 23;
    // Orbital pixels cannot resolve individual kilometre-scale billows.
    // Filtering removes their shimmer while preserving broad weather breaks.
    unsigned clear{},occupied{};
    double rawMotion{},filteredMotion{};
    for(unsigned index=0;index<1024u;++index)
    {
        const double y=1.0-2.0*(index+0.5)/1024.0,angle=index*2.399963229728653;
        const double horizontal=std::sqrt(1.0-y*y),distance=radius+3780.0;
        volume::Point point{horizontal*std::cos(angle)*distance,y*distance,horizontal*std::sin(angle)*distance};
        const auto raw=volume::density(patterned,radius,point,234.0);
        const auto filtered=volume::density(patterned,radius,point,234.0,50000.0);
        if(filtered<0.005)++clear;
        if(filtered>0.10)++occupied;
        const double nextAngle=angle+100.0/radius;
        volume::Point adjacent{horizontal*std::cos(nextAngle)*distance,y*distance,horizontal*std::sin(nextAngle)*distance};
        rawMotion+=std::abs(raw-volume::density(patterned,radius,adjacent,234.0));
        filteredMotion+=std::abs(filtered-volume::density(patterned,radius,adjacent,234.0,50000.0));
        if(!std::isfinite(filtered)||filtered<0.0||filtered>1.0)return 24;
        if(filtered!=volume::density(patterned,radius,point,234.0+patterned.wind_period_seconds,50000.0))return 25;
    }
    if(clear<100u||occupied<100u||!(filteredMotion<rawMotion*0.5))return 26;
    return 0;
}
