// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stop_token>

export module asset.cloud_volume;

export namespace epochengine::asset::cloud_volume
{
    // Immutable metric density data. The optional world provider chooses the
    // recipe; native renderers consume it without importing provider code.
    struct Recipe final
    {
        std::uint32_t seed{};
        double base_altitude_meters{}, thickness_meters{}, horizontal_scale_meters{};
        float coverage{0.55f}, extinction_per_meter{0.0015f};
        double wind_period_seconds{604800.0};
        // Zero for providers without an admitted inner solid radius.
        double view_occluder_radius_meters{};
        friend bool operator==(const Recipe&,const Recipe&)=default;
    };
    using Point=std::array<double,3>;
    inline constexpr unsigned maximum_primary_steps=64u;
    // One revolution of the existing wind remains a complete repetition.
    // Local formations evolve on a shorter cycle driven by the same timeline.
    [[nodiscard]] inline std::array<double,2> formation_phase(const Recipe& recipe,double seconds) noexcept
    {
        const double cycles=(std::max)(1.0,std::round(recipe.wind_period_seconds/240.0));
        const double period=recipe.wind_period_seconds/cycles;
        const double angle=std::fmod(std::fmod(seconds,recipe.wind_period_seconds),period)
            /period*2.0*std::numbers::pi;
        return {std::sin(angle),std::cos(angle)};
    }
    [[nodiscard]] inline bool valid(const Recipe& value,double radius) noexcept
    {
        // World size bounds apply to the reference body. Other generated
        // bodies can be smaller or larger than that reference.
        return std::isfinite(radius)&&radius>=1.0&&radius<=1e9
            &&std::isfinite(value.base_altitude_meters)&&value.base_altitude_meters>=0.0
            &&value.base_altitude_meters<=radius*0.2
            &&std::isfinite(value.thickness_meters)&&value.thickness_meters>=1.0
            &&value.thickness_meters<=radius*0.2
            &&std::isfinite(value.horizontal_scale_meters)
            &&value.horizontal_scale_meters>=(std::max)(10.0,radius/8192.0)
            &&value.horizontal_scale_meters<=radius*2.0
            &&std::isfinite(value.coverage)&&value.coverage>=0.0f&&value.coverage<=1.0f
            &&std::isfinite(value.extinction_per_meter)&&value.extinction_per_meter>=0.0f&&value.extinction_per_meter<=1.0f
            &&std::isfinite(value.wind_period_seconds)&&value.wind_period_seconds>=60.0&&value.wind_period_seconds<=1e9
            &&std::isfinite(value.view_occluder_radius_meters)
            &&value.view_occluder_radius_meters>=0.0&&value.view_occluder_radius_meters<=radius;
    }
    namespace detail
    {
        [[nodiscard]] inline std::uint32_t hash(std::uint32_t value) noexcept
        {value^=value>>16u;value*=0x7feb352du;value^=value>>15u;value*=0x846ca68bu;return value^(value>>16u);}
        [[nodiscard]] inline double lattice(std::int32_t x,std::int32_t y,std::int32_t z,std::uint32_t seed) noexcept
        {
            const auto value=hash(static_cast<std::uint32_t>(x)*0x8da6b343u
                ^static_cast<std::uint32_t>(y)*0xd8163841u^static_cast<std::uint32_t>(z)*0xcb1ab31fu^seed);
            return double(value>>8u)/16777215.0;
        }
        [[nodiscard]] inline double smooth(double value) noexcept {return value*value*(3.0-2.0*value);}
        [[nodiscard]] inline double mix(double a,double b,double value) noexcept {return a+(b-a)*value;}
        [[nodiscard]] inline double noise(Point value,std::uint32_t seed) noexcept
        {
            std::array<std::int32_t,3> cell{};Point fraction{};
            for(unsigned axis=0;axis<3;++axis)
            {const auto whole=std::floor(value[axis]);cell[axis]=static_cast<std::int32_t>(whole);fraction[axis]=smooth(value[axis]-whole);}
            const auto x=cell[0],y=cell[1],z=cell[2];
            const auto sample=[&](int dx,int dy,int dz){return lattice(x+dx,y+dy,z+dz,seed);};
            return mix(mix(mix(sample(0,0,0),sample(1,0,0),fraction[0]),mix(sample(0,1,0),sample(1,1,0),fraction[0]),fraction[1]),
                mix(mix(sample(0,0,1),sample(1,0,1),fraction[0]),mix(sample(0,1,1),sample(1,1,1),fraction[0]),fraction[1]),fraction[2]);
        }
        [[nodiscard]] inline double filtered_noise(Point value,std::uint32_t seed,double footprint) noexcept
        {
            // Unresolved value noise tends to its mean instead of becoming
            // high-contrast speckle. The native pass uses the same filter.
            const auto retained=1.0-smooth(std::clamp((footprint-0.5)/0.5,0.0,1.0));
            return retained==0.0?0.5:mix(0.5,noise(value,seed),retained);
        }
        [[nodiscard]] inline double dot(Point a,Point b) noexcept {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
        [[nodiscard]] inline bool finite(Point value) noexcept
        {return std::ranges::all_of(value,[](double v){return std::isfinite(v)&&std::abs(v)<=1e12;});}
        [[nodiscard]] inline bool time(double seconds) noexcept {return std::isfinite(seconds)&&seconds>=0.0&&seconds<1e12;}
    }
    // Exact counterpart of the native shader's uint hash and trilinear value
    // noise. Call with body-local metres after subtracting the body centre in
    // double precision. No world-position float conversion is permitted here.
    [[nodiscard]] inline double density(const Recipe& recipe,double radius,Point local,double seconds,
        double footprint_meters=0.0,double ray_footprint_meters=0.0,
        double observer_altitude_meters=0.0) noexcept
    {
        if(!valid(recipe,radius)||!detail::finite(local)||!detail::time(seconds)||recipe.coverage==0.0f
            ||!std::isfinite(footprint_meters)||footprint_meters<0.0||footprint_meters>1e12
            ||!std::isfinite(ray_footprint_meters)||ray_footprint_meters<0.0||ray_footprint_meters>1e12
            ||!std::isfinite(observer_altitude_meters))return 0.0;
        const auto distance=std::sqrt(detail::dot(local,local));
        const auto height=(distance-radius-recipe.base_altitude_meters)/recipe.thickness_meters;
        if(height<=0.0||height>=1.0)return 0.0;
        const bool upper=height>=0.58;
        if(!upper&&height>=0.52)return 0.0;
        const auto angle=-std::fmod(seconds,recipe.wind_period_seconds)/recipe.wind_period_seconds*2.0*std::numbers::pi;
        const auto cosine=std::cos(angle),sine=std::sin(angle);
        Point coordinate{(cosine*local[0]+sine*local[2])/recipe.horizontal_scale_meters,
            local[1]/recipe.horizontal_scale_meters,(-sine*local[0]+cosine*local[2])/recipe.horizontal_scale_meters};
        const auto footprint=footprint_meters/recipe.horizontal_scale_meters;
        const auto evolution=formation_phase(recipe,seconds);
        const auto phase=upper?0.6*evolution[0]+0.8*evolution[1]:evolution[0];
        const auto cross_phase=upper?0.6*evolution[1]-0.8*evolution[0]:evolution[1];
        const auto radial=recipe.horizontal_scale_meters/distance;
        const auto shear=(upper?0.55:0.15)*phase;
        coordinate={coordinate[0]+shear*coordinate[2]*radial,coordinate[1],
            coordinate[2]-shear*coordinate[0]*radial};
        // A single weather contour bounds each deck. Project the footprint
        // onto the reference sphere so the contour lines up at every height.
        const auto angular_scale=radius/distance;
        const Point surface{coordinate[0]*angular_scale,coordinate[1]*angular_scale,
            coordinate[2]*angular_scale};
        const auto deck_seed=recipe.seed^(upper?0x57454155u:0x57454154u);
        const Point synoptic{surface[0]/91.0+11.0,surface[1]/68.0,surface[2]/91.0+7.0};
        const auto front=detail::filtered_noise(synoptic,deck_seed,footprint/68.0);
        const auto bend=std::sin((surface[0]-0.60*surface[2])/49.0+3.0*front);
        const auto across=upper?14.0:22.0;
        const auto along=upper?53.0:36.0;
        Point cluster{surface[0]/along+1.4*front,surface[1]/across+0.84*bend
            +0.12*cross_phase,surface[2]/along-1.2*front};
        // Both decks occupy the same large-scale weather systems. Moisture
        // and orientation vary smoothly between clear, cellular and front regions.
        const Point provinces{surface[0]/113.0+3.2,surface[1]/113.0-7.8,surface[2]/113.0+4.6};
        const auto regime=detail::filtered_noise(provinces,recipe.seed^0x5245474du,footprint/113.0);
        const auto broken=1.0-detail::smooth(std::clamp((regime-0.16)/0.19,0.0,1.0));
        const auto wet=detail::smooth(std::clamp((regime-0.56)/0.21,0.0,1.0));
        if(wet>0.0)
        {
            const auto displacement=12.0*wet;
            cluster[0]+=displacement*std::sin((surface[2]+0.45*surface[1])/61.0+2.0*front)/along;
            cluster[1]+=1.2*displacement*std::sin((surface[0]-0.7*surface[2])/53.0-2.3*front)/across;
            cluster[2]+=displacement*std::sin((surface[1]+0.38*surface[0])/68.0+2.8*front)/along;
        }
        auto mass=detail::filtered_noise(cluster,deck_seed^0x434c5553u,footprint/across);
        if(broken>0.0)
        {
            const Point cells{surface[0]/17.0+0.75*bend,surface[1]/17.0-0.32*bend,
                surface[2]/17.0+0.7*front};
            mass=detail::mix(mass,detail::filtered_noise(cells,deck_seed^0x43554d55u,footprint/17.0),broken);
        }
        const auto dry=1.0-detail::smooth(std::clamp((regime-0.14)/0.23,0.0,1.0));
        // Give the lower front smaller coherent lobes. They form the contour
        // before extrusion, so a clear gap extends through the whole deck.
        const auto lobe_along=upper?13.0:4.5;
        const auto lobe_across=upper?10.0:3.2;
        const Point outline{(surface[0]-0.40*surface[2])/lobe_along,
            surface[1]/lobe_across+0.8*bend,(surface[2]+0.40*surface[0])/lobe_along};
        const auto lobes=detail::filtered_noise(outline,deck_seed^0x4c4f4245u,footprint/lobe_across);
        const auto regional=(0.29*front+0.56*mass)/0.85;
        const auto lobe_weight=upper?0.15:detail::mix(0.27,0.04,
            detail::smooth(std::clamp((regional-0.47)/0.11,0.0,1.0)));
        const auto coarse_structure=detail::smooth(std::clamp((footprint-0.65)/2.0,0.0,1.0));
        // Resolve a wind-sheared flank at the scale of the weather front.
        // Its relief fades in without shifting the coordinates of either field.
        double flank_weight=0.0,condensate=0.5;
        if(coarse_structure>0.0)
        {
            const auto crosswind=surface[1]+17.0*bend+39.0*(regime-0.5)+28.0*(front-0.5);
            const Point flank{(surface[0]-0.40*surface[2])/36.0,crosswind/5.5,
                (surface[2]+0.40*surface[0])/34.0};
            condensate=detail::filtered_noise(flank,deck_seed^0x464c414eu,footprint/5.5);
            flank_weight=0.64*coarse_structure*(0.68+0.32*wet);
        }
        const auto weather=detail::mix(regional,lobes,lobe_weight*(1.0-0.55*coarse_structure));
        auto margin=weather-0.12*dry-((upper?0.752:0.725)-0.41*double(recipe.coverage));
        // Stretch the coarse edges with the wind-shaped lobes and preserve dense cores.
        const auto coarse_weather=detail::smooth(std::clamp((footprint-0.36)/1.04,0.0,1.0));
        const auto taper=1.0-detail::smooth(std::clamp((margin-0.018)/0.087,0.0,1.0));
        // Wide footprints lose small lobes, so local moisture and the
        // regional front shape the coarse outline.
        const auto front_shape=1.05*(front-0.5)+0.62*(regime-0.5)-0.33*(mass-0.5);
        const auto distant_streak=0.14*bend+0.38*(lobes-0.5)+0.66*(condensate-0.5)+front_shape;
        const auto streak=detail::mix(0.48*bend+1.04*(lobes-0.5),distant_streak,coarse_structure);
        const auto extension=(std::min)(0.14*(std::max)(streak,0.0),(std::max)(0.075-margin,0.0));
        margin+=coarse_weather*(extension+0.14*taper*(std::min)(streak,0.0));
        if(coarse_structure>0.0)
        {
            const auto core_guard=1.0-0.60*detail::smooth(std::clamp((margin-0.04)/0.15,0.0,1.0));
            margin+=flank_weight*core_guard*(condensate-0.5)*0.32;
        }
        if(margin<=0.0)return 0.0;
        const auto core=detail::smooth(std::clamp((margin-0.035)/0.13,0.0,1.0));

        // Billows move in three dimensions. They sculpt the cap and thin the
        // outer contour without excavating the optically thick core.
        const auto vertical_footprint=footprint_meters/recipe.thickness_meters;
        const auto detail_footprint=(std::max)((std::max)(footprint,
            ray_footprint_meters/recipe.horizontal_scale_meters)*2.1,vertical_footprint*2.8);
        const auto billow_scale=2.1+2.8*height*radial;
        const Point billows{coordinate[0]*billow_scale+0.31*phase,coordinate[1]*billow_scale,
            coordinate[2]*billow_scale+0.24*cross_phase};
        const auto billow=detail::filtered_noise(billows,deck_seed^0x42494c4cu,detail_footprint);
        const auto erosion=(1.0-core)*0.027*(1.0-billow);
        const auto eroded_margin=margin-erosion;
        const auto contour_width=(upper?0.145:0.105)+0.025*coarse_weather;
        auto body=detail::smooth(std::clamp(eroded_margin/contour_width,0.0,1.0));
        double profile;
        if(upper)
        {
            const auto cap=0.80+0.10*core+0.08*billow;
            profile=detail::smooth(std::clamp((height-0.58)/0.10,0.0,1.0))
                *(1.0-detail::smooth(std::clamp((height-cap+0.13)/0.13,0.0,1.0)));
        }
        else
        {
            // Resolve a compact physical boundary inside the lower weather
            // contour. Coarse pixels and long ray bins use the broad profile.
            const auto cell_footprint=(std::max)(footprint,ray_footprint_meters/recipe.horizontal_scale_meters);
            const auto resolved=1.0-detail::smooth(std::clamp((cell_footprint-0.16)/0.47,0.0,1.0));
            const auto local_body=detail::smooth(std::clamp((eroded_margin-0.024)/0.060,0.0,1.0));
            body=detail::mix(body,local_body,resolved);
            // The relief field moves with the weather footprint. Its lobes
            // hang from the base while the cap retains finer billow detail.
            const Point cells{(surface[0]-0.35*surface[2])/0.46+0.18*phase,
                surface[1]/0.34,(surface[2]+0.35*surface[0])/0.46+0.16*cross_phase};
            const auto cell=detail::filtered_noise(cells,deck_seed^0x42415345u,cell_footprint/0.34);
            const auto hanging=detail::smooth(std::clamp((cell-0.28)/0.44,0.0,1.0));
            const auto center=0.26+0.04*(hanging-0.5);
            const auto depth=0.075+0.17*std::sqrt(body);
            const auto base_depth=(std::min)(depth*detail::mix(0.55,1.38,hanging),center);
            const auto cap_depth=(std::min)(depth*detail::mix(0.99,1.25,billow),0.52-center);
            const auto cross_section=height<center?(center-height)/base_depth:(height-center)/cap_depth;
            const auto edge_width=detail::mix(0.38,0.24,resolved);
            profile=1.0-detail::smooth(std::clamp(1.0+(cross_section-1.0)/edge_width,0.0,1.0));
        }
        // A coarse column covers both cloud and gaps along its shoulders.
        // Strong fronts stay optically thick as the shoulders thin out.
        const auto organized=detail::smooth(std::clamp((0.72*regional+0.28*front-0.43)/0.30,0.0,1.0));
        const auto column_relief=1.0-coarse_structure*(upper?0.79:0.78)*(1.0-organized);
        const auto flank_relief=1.0-flank_weight*(1.0-detail::smooth(
            std::clamp((condensate-0.12)/0.74,0.0,1.0)));
        return body*profile*(upper?0.48:1.0)*(0.80+0.20*billow)*column_relief*flank_relief;
    }

    // Dense liquid-water cores have a higher extinction per metre. Keep the
    // upper deck and the thin edge at the recipe's base coefficient.
    [[nodiscard]] inline double lower_deck_extinction(double base_extinction) noexcept
    {return (std::min)(1.0,base_extinction*2.75);}
    [[nodiscard]] inline double extinction_from_density(double sampled_density,double base_extinction,
        bool lower_deck) noexcept
    {
        const auto core=lower_deck?detail::smooth(std::clamp((sampled_density-0.08)/0.20,0.0,1.0)):0.0;
        const auto coefficient=detail::mix(base_extinction,lower_deck_extinction(base_extinction),core);
        return sampled_density*coefficient;
    }
    struct Interval final {double begin{},end{};};
    struct Intersections final {std::array<Interval,2> intervals{};unsigned count{};bool valid{};};
    // A ray can cross two disjoint parts of a shell. Opaque scene depth clips
    // both, including the far-side part, before sampling starts.
    [[nodiscard]] inline Intersections intersect(const Recipe& recipe,double radius,Point origin,
        Point direction,double opaque_distance) noexcept
    {
        Intersections result;
        if(!valid(recipe,radius)||!detail::finite(origin)||!detail::finite(direction)
            ||std::abs(detail::dot(direction,direction)-1.0)>1e-6
            ||!std::isfinite(opaque_distance)||opaque_distance<0.0||opaque_distance>1e12)return result;
        result.valid=true;
        const auto sphere=[&](double r,Interval& value){
            const auto b=detail::dot(origin,direction),c=detail::dot(origin,origin)-r*r;
            const auto discriminant=b*b-c;if(discriminant<0.0)return false;
            const auto root=std::sqrt(discriminant),q=-b-std::copysign(root,b);
            auto first=q,second=q==0.0?-b:c/q;if(first>second)std::swap(first,second);
            value={first,second};return true;};
        Interval outer{},inner{};
        if(!sphere(radius+recipe.base_altitude_meters+recipe.thickness_meters,outer))return result;
        outer.begin=(std::max)(0.0,outer.begin);outer.end=(std::min)(opaque_distance,outer.end);
        if(outer.end<=outer.begin)return result;
        if(!sphere(radius+recipe.base_altitude_meters,inner)||inner.end<=outer.begin||inner.begin>=outer.end)
        {result.intervals[result.count++]=outer;return result;}
        if(inner.begin>outer.begin)result.intervals[result.count++]={outer.begin,(std::min)(inner.begin,outer.end)};
        if(inner.end<outer.end)result.intervals[result.count++]={(std::max)(inner.end,outer.begin),outer.end};
        return result;
    }
    struct Sample final {double transmittance{1.0};unsigned samples{};bool valid{},cancelled{};};
    [[nodiscard]] inline Sample transmittance(const Recipe& recipe,double radius,Point origin,Point direction,
        double opaque_distance,double seconds,unsigned steps=maximum_primary_steps,std::stop_token stop={}) noexcept
    {
        Sample result;
        if(stop.stop_requested()){result.cancelled=true;return result;}
        if(!detail::time(seconds)||!steps||steps>maximum_primary_steps)return result;
        const auto segments=intersect(recipe,radius,origin,direction,opaque_distance);
        if(!segments.valid)return result;
        result.valid=true;
        double length{};for(unsigned i=0;i<segments.count;++i)length+=segments.intervals[i].end-segments.intervals[i].begin;
        if(length==0.0||recipe.extinction_per_meter==0.0f||recipe.coverage==0.0f)return result;
        const auto spacing=length/steps;
        const auto observer_altitude=std::sqrt(detail::dot(origin,origin))-radius;
        const auto lower_end=radius+recipe.base_altitude_meters+0.52*recipe.thickness_meters;
        for(unsigned step=0;step<steps;++step)
        {
            if(stop.stop_requested()){result.cancelled=true;result.valid=false;return result;}
            double offset=(step+0.5)*spacing,t{};
            for(unsigned i=0;i<segments.count;++i)
            {const auto segment=segments.intervals[i];if(offset<=segment.end-segment.begin){t=segment.begin+offset;break;}offset-=segment.end-segment.begin;}
            Point position{};for(unsigned axis=0;axis<3;++axis)position[axis]=origin[axis]+direction[axis]*t;
            const auto sampled_density=density(recipe,radius,position,seconds,0.0,spacing*0.5,observer_altitude);
            const auto extinction=extinction_from_density(sampled_density,recipe.extinction_per_meter,
                detail::dot(position,position)<lower_end*lower_end);
            result.transmittance*=std::exp(-extinction*spacing);
            ++result.samples;
        }
        return result;
    }
}
