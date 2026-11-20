// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <stop_token>
import asset.cloud_volume;
namespace volume=epochengine::asset::cloud_volume;
int main()
{
    constexpr double radius=6371000.0;
    const volume::Recipe recipe{77u,2700.0,2160.0,12742.0,1.0f,0.0012f};
    if(!volume::valid(recipe,radius))return 1;
    const volume::Point midpoint{0.0,radius+3780.0,0.0};
    const volume::Point lower_core{0.0,radius+recipe.base_altitude_meters+0.27*recipe.thickness_meters,0.0};
    const auto sampled=volume::density(recipe,radius,lower_core,12.0);
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
        const double angle=i*0.37;const volume::Point point{lower_core[1]*std::cos(angle),0.0,lower_core[1]*std::sin(angle)};
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
        const double horizontal=std::sqrt(1.0-y*y);
        const double distance=radius+patterned.base_altitude_meters+patterned.thickness_meters*0.30;
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
    // A cloud passage must contain two dense decks around a clear gap. Aggregate
    // many surface directions so the check does not pin a seed's incidental cell.
    std::array<double,3> decks{};
    double independent{};
    for(unsigned index=0;index<256u;++index)
    {
        const double y=1.0-2.0*(index+0.5)/256.0,angle=index*2.399963229728653;
        const double horizontal=std::sqrt(1.0-y*y);
        const volume::Point up{horizontal*std::cos(angle),y,horizontal*std::sin(angle)};
        std::array<double,3> column{};
        const std::array heights{0.30,0.55,0.75};
        for(unsigned layer=0;layer<heights.size();++layer)
        {
            const auto distance=radius+recipe.base_altitude_meters+recipe.thickness_meters*heights[layer];
            const volume::Point point{up[0]*distance,up[1]*distance,up[2]*distance};
            column[layer]=volume::density(recipe,radius,point,234.0);
            for(double altitude:{16000.0,40000.0,85000.0})
                if(volume::density(recipe,radius,point,234.0,0.0,0.0,altitude)!=column[layer])return 35;
            decks[layer]+=column[layer];
        }
        independent+=std::abs(column[0]-column[2]);
    }
    if(!(decks[0]>0.0&&decks[1]<1e-9&&decks[2]>0.0&&independent>1.0))return 27;
    // Measure the entire vertical column from orbit. A sparse lower deck
    // alone cannot pass while the upper deck still blankets the same region.
    const volume::Recipe verdant{1337u,7200.0,2880.0,12742.0,0.60f,0.0012f};
    constexpr unsigned columns=512u,samples=32u;
    for(double time:{0.0,verdant.wind_period_seconds*0.37})
    {
        unsigned clear_columns{},wet_columns{};
        for(unsigned column=0;column<columns;++column)
        {
            const double y=1.0-2.0*(column+0.5)/columns,angle=column*2.399963229728653;
            const double horizontal=std::sqrt(1.0-y*y);
            const volume::Point direction{horizontal*std::cos(angle),y,horizontal*std::sin(angle)};
            double extinction_sum{};
            for(unsigned sample=0;sample<samples;++sample)
            {
                const double d=radius+verdant.base_altitude_meters+verdant.thickness_meters*(sample+0.5)/samples;
                const volume::Point position{direction[0]*d,direction[1]*d,direction[2]*d};
                const auto density=volume::density(verdant,radius,position,time,50000.0);
                extinction_sum+=volume::extinction_from_density(density,verdant.extinction_per_meter,
                    d<radius+verdant.base_altitude_meters+0.52*verdant.thickness_meters);
            }
            const double absorption=1.0-std::exp(-extinction_sum*verdant.thickness_meters/samples);
            clear_columns+=absorption<0.005;
            wet_columns+=absorption>0.35;
        }
        if(clear_columns<columns*3/10||clear_columns>columns*9/10||wet_columns<columns/20)
        {
            std::fprintf(stderr,"orbital columns at %.0f s: %u clear, %u optically thick\n",
                time,clear_columns,wet_columns);
            return 28;
        }
    }

    // Compare equal physical steps along and across the wind at two deck heights.
    // A moving sample follows the density field rather than a changing noise seed.
    std::array<double,2> along{},across{};
    constexpr double step=0.018;
    constexpr std::array heights{0.27,0.74};
    // An integer number of short formation cycles isolates the slower
    // translation. An advancing formation is tested independently below.
    const double elapsed=240.0*420.0;
    const double phase=elapsed/verdant.wind_period_seconds*2.0*std::numbers::pi;
    for(unsigned index=0;index<columns;++index)
    {
        const double latitude=std::asin(1.36*((index+0.5)/columns-0.5));
        const double longitude=index*2.399963229728653;
        for(unsigned layer=0;layer<heights.size();++layer)
        {
            const double distance=radius+verdant.base_altitude_meters+verdant.thickness_meters*heights[layer];
            const auto at=[distance](double lat,double lon) {
                return volume::Point{distance*std::cos(lat)*std::cos(lon),
                    distance*std::sin(lat),distance*std::cos(lat)*std::sin(lon)};
            };
            const auto center=volume::density(verdant,radius,at(latitude,longitude),0.0,50000.0);
            const auto windward=volume::density(verdant,radius,
                at(latitude,longitude+step/std::cos(latitude)),0.0,50000.0);
            const auto meridional=volume::density(verdant,radius,
                at(latitude+step,longitude),0.0,50000.0);
            along[layer]+=std::abs(windward-center);
            across[layer]+=std::abs(meridional-center);
            const auto advected=volume::density(verdant,radius,
                at(latitude,longitude-phase),elapsed,50000.0);
            if(std::abs(advected-center)>1e-9)return 29;
        }
    }
    // Let the upper wisps stretch farther along the wind. The lower deck
    // should form broad cloud bodies without becoming a set of zonal strips.
    if(!(across[0]>along[0]&&across[0]<along[0]*1.65&&across[1]>along[1]*1.35))
    {
        std::fprintf(stderr,"cross-wind contrast relative to along-wind: lower %.3f, upper %.3f\n",
            across[0]/along[0],across[1]/along[1]);
        return 30;
    }
    std::array<unsigned,2> separated{};
    std::array<std::array<double,2>,2> evolution{};
    for(unsigned index=0;index<columns;++index)
    {
        const double latitude=std::asin(1.8*((index+0.5)/columns-0.5));
        const double longitude=index*2.399963229728653;
        std::array<double,2> column{};
        for(unsigned layer=0;layer<heights.size();++layer)
        {
            const double distance=radius+verdant.base_altitude_meters+verdant.thickness_meters*heights[layer];
            const auto point=[&](double seconds){
                // Follow the ordinary wind so differences measure formation
                // evolution and independent shear, rather than translation.
                const auto lon=longitude-seconds/verdant.wind_period_seconds*2.0*std::numbers::pi;
                return volume::Point{distance*std::cos(latitude)*std::cos(lon),
                    distance*std::sin(latitude),distance*std::cos(latitude)*std::sin(lon)};
            };
            const auto before=volume::density(verdant,radius,point(0.0),0.0);
            column[layer]=before;
            for(unsigned interval=0;interval<2u;++interval)
            {
                const double seconds=interval==0?30.0:120.0;
                const auto moved=volume::density(verdant,radius,point(seconds),seconds);
                if(moved!=volume::density(verdant,radius,point(seconds),seconds))return 31;
                evolution[layer][interval]+=std::abs(moved-before);
            }
        }
        separated[0]+=column[0]>=0.05&&column[1]<0.005;
        separated[1]+=column[1]>=0.05&&column[0]<0.005;
    }
    for(unsigned layer=0;layer<2u;++layer)
        if(separated[layer]<columns/50u
            ||evolution[layer][0]<0.002*columns||evolution[layer][1]<0.002*columns)
        {
            std::fprintf(stderr,"deck %u has %u independent columns, 30s density change %.6f, 120s density change %.6f\n",
                layer,separated[layer],evolution[layer][0]/columns,evolution[layer][1]/columns);
            return 32;
        }
    // Fine, close-up billows must leave an optically connected interior.
    // Inspect nearby points in each strong body at both rendering footprints.
    unsigned intact_cores{};
    for(unsigned index=0;index<columns;++index)
    {
        const double y=1.0-2.0*(index+0.5)/columns,angle=index*2.399963229728653;
        const double horizontal=std::sqrt(1-y*y);
        const auto altitude=verdant.base_altitude_meters+0.27*verdant.thickness_meters;
        const auto at=[&](double meters,double longitude){
            const auto distance=radius+altitude+meters;
            return volume::Point{distance*horizontal*std::cos(longitude),distance*y,
                distance*horizontal*std::sin(longitude)};
        };
        if(volume::density(verdant,radius,at(0.0,angle),0.0,50000.0)<0.65)continue;
        ++intact_cores;
        const auto spacing=0.35*verdant.horizontal_scale_meters/(radius*horizontal);
        for(double height:{-0.04,0.0,0.04})for(double offset:{-spacing,0.0,spacing})
        {
            const auto local=volume::density(verdant,radius,at(height*verdant.thickness_meters,angle+offset),0.0);
            if(local<0.30)
            {
                std::fprintf(stderr,"optical core %u at height %.3f and angular offset %.6f has density %.4f\n",
                    index,0.27+height,offset,local);
                return 36;
            }
        }
    }
    if(intact_cores<columns/20u)return 37;

    // Replay the fixed-time native Verdant passage. Check vertical shape as
    // well as occupancy so a flat deck cannot pass on the same coverage.
    const volume::Recipe passage{2989955058u,10440.0,5760.0,12742.0,0.60f,0.00045f};
    const volume::Point up{0.5973640425584571,0.58057077605621243,0.55325742168334735};
    const volume::Point tangent{-0.7816723381753139,0.57573326028182847,0.23983237633145738};
    const auto section_direction=[&](double offset) {
        auto direction=up;
        for(unsigned axis=0;axis<3;++axis)direction[axis]+=tangent[axis]*offset/radius;
        const auto length=std::hypot(direction[0],direction[1],direction[2]);
        for(auto& coordinate:direction)coordinate/=length;
        return direction;
    };
    const auto occupied_column=[&](double offset) {
        const auto direction=section_direction(offset);
        unsigned occupied_samples{};
        double base_height=1.0;
        for(unsigned step=0;step<=260;++step)
        {
            const auto height=0.002*step;
            const auto distance=radius+passage.base_altitude_meters+height*passage.thickness_meters;
            auto point=direction;
            for(auto& coordinate:point)coordinate*=distance;
            if(volume::density(passage,radius,point,0.0)>0.15)
            {
                if(occupied_samples==0)base_height=height;
                ++occupied_samples;
            }
        }
        return std::array{double(occupied_samples)*0.002,base_height};
    };
    const auto interior=occupied_column(0.0)[0],rim=occupied_column(8000.0)[0];
    if(interior<0.35||rim<0.10||rim>=interior*0.8||occupied_column(18000.0)[0]!=0.0)return 38;
    double lowest=1.0,highest=0.0;
    for(int offset=-12000;offset<=8000;offset+=2000)
    {
        const auto base=occupied_column(offset)[1];
        if(base==1.0)return 39;
        lowest=std::fmin(lowest,base);
        highest=std::fmax(highest,base);
    }
    if(highest-lowest<0.08)
    {
        std::fprintf(stderr,"lower cloud base spans %.1f m across the passage\n",
            (highest-lowest)*passage.thickness_meters);
        return 40;
    }
    const auto column_opacity=[&](double lateral,double distance){
        const auto ray=section_direction(lateral);
        auto start=ray;
        for(auto& coordinate:start)coordinate*=radius+7940.0;
        const auto column=volume::transmittance(passage,radius,start,ray,distance,0.0);
        return column.valid?1.0-column.transmittance:-1.0;
    };
    const auto thick=column_opacity(0.0,30000.0),clear_gap=column_opacity(18000.0,30000.0);
    if(thick<0.76||clear_gap<0.0||clear_gap>0.23)
    {
        std::fprintf(stderr,"Verdant column opacity: core %.3f, neighboring gap %.3f\n",thick,clear_gap);
        return 41;
    }
    // Measure the resolved lower boundary without letting the upper deck
    // fill a legitimate clear region overhead.
    const auto lower_end=passage.base_altitude_meters+0.52*passage.thickness_meters-7940.0;
    int dense_edge=-1,clear_edge=-1;
    for(int offset=0;offset<=18000;offset+=250)
    {
        const auto opacity=column_opacity(offset,lower_end);
        if(opacity>=0.48)dense_edge=offset;
        if(dense_edge>=0&&opacity>=0.0&&opacity<=0.05)
        {clear_edge=offset;break;}
    }
    if(dense_edge<0||clear_edge-dense_edge<500||clear_edge-dense_edge>4250)
    {
        std::fprintf(stderr,"lower cloud boundary from %.2f to %.2f km\n",dense_edge/1000.0,clear_edge/1000.0);
        return 42;
    }
    return 0;
}
