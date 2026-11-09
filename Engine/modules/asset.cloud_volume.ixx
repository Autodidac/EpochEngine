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
        friend bool operator==(const Recipe&,const Recipe&)=default;
    };
    using Point=std::array<double,3>;
    inline constexpr unsigned maximum_primary_steps=64u;
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
            &&std::isfinite(value.wind_period_seconds)&&value.wind_period_seconds>=60.0&&value.wind_period_seconds<=1e9;
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
        double footprint_meters=0.0) noexcept
    {
        if(!valid(recipe,radius)||!detail::finite(local)||!detail::time(seconds)||recipe.coverage==0.0f
            ||!std::isfinite(footprint_meters)||footprint_meters<0.0||footprint_meters>1e12)return 0.0;
        const auto distance=std::sqrt(detail::dot(local,local));
        const auto height=(distance-radius-recipe.base_altitude_meters)/recipe.thickness_meters;
        if(height<=0.0||height>=1.0)return 0.0;
        const auto angle=-std::fmod(seconds,recipe.wind_period_seconds)/recipe.wind_period_seconds*2.0*std::numbers::pi;
        const auto cosine=std::cos(angle),sine=std::sin(angle);
        Point coordinate{(cosine*local[0]+sine*local[2])/recipe.horizontal_scale_meters,
            local[1]/recipe.horizontal_scale_meters,(-sine*local[0]+cosine*local[2])/recipe.horizontal_scale_meters};
        const auto footprint=footprint_meters/recipe.horizontal_scale_meters;
        Point regional{},regionalDetail{},bladeDetail{};
        for(unsigned axis=0;axis<3u;++axis)
        {
            regional[axis]=coordinate[axis]/32.0;
            regionalDetail[axis]=coordinate[axis]/16.0+9.0;
            bladeDetail[axis]=coordinate[axis]*2.0+17.0;
        }
        // Two regional samples form weather systems; two local samples retain
        // the cumulus detail. The four-noise-evaluation budget is unchanged.
        const auto weather=0.7*detail::filtered_noise(regional,recipe.seed^0x57454154u,footprint/32.0)
            +0.3*detail::filtered_noise(regionalDetail,recipe.seed^0x48455231u,footprint/16.0);
        const auto cover=detail::smooth(std::clamp((weather-(1.0-double(recipe.coverage)))/0.20,0.0,1.0));
        if(cover==0.0)return 0.0;
        const auto shape=(2.0*detail::filtered_noise(coordinate,recipe.seed,footprint)
            +detail::filtered_noise(bladeDetail,recipe.seed+1013u,footprint*2.0))/3.0;
        const auto detail_mask=std::clamp((shape-0.25)/0.50,0.0,1.0);
        const auto lower=detail::smooth(std::clamp(height/0.20,0.0,1.0));
        const auto upper=1.0-detail::smooth(std::clamp((height-0.55)/0.45,0.0,1.0));
        return cover*detail_mask*lower*upper;
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
        for(unsigned step=0;step<steps;++step)
        {
            if(stop.stop_requested()){result.cancelled=true;result.valid=false;return result;}
            double offset=(step+0.5)*spacing,t{};
            for(unsigned i=0;i<segments.count;++i)
            {const auto segment=segments.intervals[i];if(offset<=segment.end-segment.begin){t=segment.begin+offset;break;}offset-=segment.end-segment.begin;}
            Point position{};for(unsigned axis=0;axis<3;++axis)position[axis]=origin[axis]+direction[axis]*t;
            result.transmittance*=std::exp(-density(recipe,radius,position,seconds)*recipe.extinction_per_meter*spacing);
            ++result.samples;
        }
        return result;
    }
}
