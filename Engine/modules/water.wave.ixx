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

export module water.wave;

export namespace epochengine::water::wave
{
    // Renderer-neutral, metric wave data. An optional implementation supplies
    // this immutable result; Water owns the sampling used by every consumer.
    struct Sample final
    {
        float height{}, displacement_x{}, displacement_z{};
        std::array<float,3> normal{0.0f,1.0f,0.0f};
        float foam{};
    };
    struct DisplacementSample final
    {
        float height{}, displacement_x{}, displacement_z{}, foam{};
    };

    class Tile final
    {
        struct Coordinate final {std::uint32_t index{};float fraction{};};
        struct Blend final
        {
            const Sample& a;const Sample& b;const Sample& c;const Sample& d;
            float tx{},tz{};
            [[nodiscard]] float interpolate(float av,float bv,float cv,float dv) const noexcept
            {return std::lerp(std::lerp(av,bv,tx),std::lerp(cv,dv,tx),tz);}
            [[nodiscard]] DisplacementSample displacement() const noexcept
            {
                return {interpolate(a.height,b.height,c.height,d.height),
                    interpolate(a.displacement_x,b.displacement_x,c.displacement_x,d.displacement_x),
                    interpolate(a.displacement_z,b.displacement_z,c.displacement_z,d.displacement_z),
                    interpolate(a.foam,b.foam,c.foam,d.foam)};
            }
        };
        [[nodiscard]] Coordinate coordinate(double value) const noexcept
        {
            const double wrapped=std::fmod(value,length_)/length_+0.5;
            const double scaled=(wrapped-std::floor(wrapped))*resolution_;
            return {static_cast<std::uint32_t>(scaled),static_cast<float>(scaled-std::floor(scaled))};
        }
        [[nodiscard]] Blend blend(Coordinate x,Coordinate z) const noexcept
        {
            const auto at=[&](std::uint32_t px,std::uint32_t pz)->const Sample& {
                return samples_[std::size_t(pz&(resolution_-1u))*resolution_+(px&(resolution_-1u))];
            };
            return {at(x.index,z.index),at(x.index+1u,z.index),at(x.index,z.index+1u),
                at(x.index+1u,z.index+1u),x.fraction,z.fraction};
        }
    public:
        [[nodiscard]] static std::shared_ptr<const Tile> create(
            std::uint32_t resolution, double length_meters, double seconds,
            std::vector<Sample> samples, std::uint64_t maximum_bytes) noexcept
        {
            if(resolution<8u||resolution>512u||(resolution&(resolution-1u))!=0u
                ||!std::isfinite(length_meters)||length_meters<=0.0
                ||!std::isfinite(seconds)||std::abs(seconds)>=1e12
                ||samples.size()!=std::size_t(resolution)*resolution
                ||samples.capacity()>maximum_bytes/sizeof(Sample))return {};
            for(const auto& sample:samples)
            {
                for(const auto value:{sample.height,sample.displacement_x,sample.displacement_z,
                    sample.normal[0],sample.normal[1],sample.normal[2],sample.foam})
                    if(!std::isfinite(value))return {};
                const double norm=double(sample.normal[0])*sample.normal[0]
                    +double(sample.normal[1])*sample.normal[1]+double(sample.normal[2])*sample.normal[2];
                if(std::abs(norm-1.0)>1e-4||sample.normal[1]<=0.0f
                    ||sample.foam<0.0f||sample.foam>1.0f)return {};
            }
            try { return std::shared_ptr<const Tile>{new Tile{resolution,length_meters,seconds,std::move(samples)}}; }
            catch(...) { return {}; }
        }

        [[nodiscard]] std::uint32_t resolution() const noexcept { return resolution_; }
        [[nodiscard]] double length_meters() const noexcept { return length_; }
        [[nodiscard]] double seconds() const noexcept { return seconds_; }
        [[nodiscard]] std::uint64_t payload_bytes() const noexcept
        { return samples_.capacity()*sizeof(Sample); }
        [[nodiscard]] std::span<const Sample> samples() const noexcept { return samples_; }

        [[nodiscard]] std::optional<Sample> sample(double x_meters,double z_meters,double seconds) const noexcept
        {
            // A retained previous frame is valid for its own time only. The
            // host may display it while a new tile runs, but cannot relabel it.
            if(seconds!=seconds_||!std::isfinite(x_meters)||!std::isfinite(z_meters))return {};
            const auto interpolation=blend(coordinate(x_meters),coordinate(z_meters));
            const auto& a=interpolation.a;const auto& b=interpolation.b;
            const auto& c=interpolation.c;const auto& d=interpolation.d;
            const auto interpolate=[&](float av,float bv,float cv,float dv) {
                return interpolation.interpolate(av,bv,cv,dv);
            };
            Sample result{interpolate(a.height,b.height,c.height,d.height),
                interpolate(a.displacement_x,b.displacement_x,c.displacement_x,d.displacement_x),
                interpolate(a.displacement_z,b.displacement_z,c.displacement_z,d.displacement_z)};
            for(std::size_t axis=0;axis<3;++axis)
                result.normal[axis]=interpolate(a.normal[axis],b.normal[axis],c.normal[axis],d.normal[axis]);
            const double norm=std::hypot(double(result.normal[0]),double(result.normal[1]),double(result.normal[2]));
            for(auto& value:result.normal)value=static_cast<float>(value/norm);
            result.foam=interpolate(a.foam,b.foam,c.foam,d.foam);
            return result;
        }
        // Spherical displacement needs the YZ, XZ and XY fields, but computes
        // its own geometric normal. Share coordinate wrapping and avoid three
        // unused interpolated/normalised planar normals at every sample point.
        [[nodiscard]] std::optional<std::array<DisplacementSample,3>> displacement_projections(
            std::array<double,3> meters,double seconds) const noexcept
        {
            if(seconds!=seconds_)return {};
            for(const auto value:meters)if(!std::isfinite(value))return {};
            const auto x=coordinate(meters[0]),y=coordinate(meters[1]),z=coordinate(meters[2]);
            return std::array{blend(y,z).displacement(),blend(x,z).displacement(),blend(x,y).displacement()};
        }

    private:
        Tile(std::uint32_t resolution,double length,double seconds,std::vector<Sample> samples) noexcept
            :resolution_(resolution),length_(length),seconds_(seconds),samples_(std::move(samples)){}
        std::uint32_t resolution_{};
        double length_{},seconds_{};
        std::vector<Sample> samples_{};
    };

    enum class Code : std::uint8_t {ready,cancelled,invalid,limit_exceeded,evaluation_failed};
    struct Evaluation final
    {
        Code code{Code::invalid};
        std::shared_ptr<const Tile> tile{};
        std::uint64_t peak_payload_bytes{};
        bool reused{};
        [[nodiscard]] explicit operator bool() const noexcept {return code==Code::ready;}
    };
    class Provider
    {
    public:
        virtual ~Provider()=default;
        // One caller-owned worker at a time. The host owns scheduling, queued
        // time requests, source admission and the aggregate retained budget.
        [[nodiscard]] virtual Evaluation evaluate(double seconds,std::uint64_t maximum_bytes,
            std::stop_token={}) noexcept=0;
    };

    struct SurfacePoint final
    {
        std::array<double,3> position_meters{};
        std::array<float,3> normal{0.0f,1.0f,0.0f};
        float foam{};
    };

    // Three smoothly weighted metric projections cover the entire sphere.
    // Coordinates depend on the body, never the patch, camera, cube face or
    // LOD, so adjacent patches evaluate exactly the same surface at a seam.
    [[nodiscard]] inline std::optional<SurfacePoint> spherical_sample(const Tile& tile,
        std::array<double,3> direction,double radius_meters,double seconds) noexcept
    {
        const auto norm=[](const auto& p) {return std::hypot(double(p[0]),double(p[1]),double(p[2]));};
        for(const auto value:direction)if(!std::isfinite(value))return {};
        if(!std::isfinite(radius_meters)||radius_meters<=0.0||std::abs(norm(direction)-1.0)>1e-6
            ||seconds!=tile.seconds())return {};
        const auto point=[&](const std::array<double,3>& n)->std::optional<SurfacePoint> {
            std::array<double,3> weight{};double sum{};
            for(std::size_t i=0;i<3;++i){weight[i]=n[i]*n[i]*n[i]*n[i];sum+=weight[i];}
            for(auto& value:weight)value/=sum;
            const auto projections=tile.displacement_projections(
                {n[0]*radius_meters,n[1]*radius_meters,n[2]*radius_meters},seconds);
            if(!projections)return {};
            const auto* yz=&(*projections)[0];const auto* xz=&(*projections)[1];const auto* xy=&(*projections)[2];
            const double height=weight[0]*yz->height+weight[1]*xz->height+weight[2]*xy->height;
            std::array<double,3> displacement{
                weight[1]*xz->displacement_x+weight[2]*xy->displacement_x,
                weight[0]*yz->displacement_x+weight[2]*xy->displacement_z,
                weight[0]*yz->displacement_z+weight[1]*xz->displacement_z};
            double radial{};for(std::size_t i=0;i<3;++i)radial+=displacement[i]*n[i];
            SurfacePoint result;
            for(std::size_t i=0;i<3;++i)
                result.position_meters[i]=n[i]*(radius_meters+height)+displacement[i]-radial*n[i];
            result.foam=static_cast<float>(weight[0]*yz->foam+weight[1]*xz->foam+weight[2]*xy->foam);
            return result;
        };
        const auto cross=[](const auto& a,const auto& b) {
            return std::array{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
        };
        const std::array<double,3> reference=std::abs(direction[1])<0.9
            ?std::array{0.0,1.0,0.0}:std::array{1.0,0.0,0.0};
        auto u=cross(reference,direction);const auto ul=norm(u);for(auto& value:u)value/=ul;
        const auto v=cross(direction,u);
        const double step=(std::min)(1e-3,tile.length_meters()/tile.resolution()*0.125/radius_meters);
        if(step<=1e-15)return {};
        std::array<std::array<double,3>,2> derivative{};
        for(std::size_t axis=0;axis<2;++axis)
        {
            const auto& tangent=axis==0?u:v;
            auto left=direction,right=direction;
            for(std::size_t i=0;i<3;++i){left[i]-=tangent[i]*step;right[i]+=tangent[i]*step;}
            const auto ll=norm(left),rl=norm(right);
            for(std::size_t i=0;i<3;++i){left[i]/=ll;right[i]/=rl;}
            const auto a=point(left),b=point(right);if(!a||!b)return {};
            for(std::size_t i=0;i<3;++i)derivative[axis][i]=b->position_meters[i]-a->position_meters[i];
        }
        auto result=point(direction);if(!result)return {};
        const auto normal=cross(derivative[0],derivative[1]);const auto length=norm(normal);
        if(!std::isfinite(length)||length<=0.0)return {};
        for(std::size_t i=0;i<3;++i)
        {
            if(!std::isfinite(result->position_meters[i]))return {};
            result->normal[i]=static_cast<float>(normal[i]/length);
        }
        return result;
    }
}
