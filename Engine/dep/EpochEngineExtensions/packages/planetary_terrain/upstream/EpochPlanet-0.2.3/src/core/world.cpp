#include "epoch/planet/world.hpp"
#include <array>
#include <limits>
#include <stdexcept>

namespace epoch::planet {
std::uint32_t hash32(std::uint32_t x) noexcept {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
static double lattice(int x,int y,int z,std::uint32_t seed) noexcept {
    auto h=seed^hash32(static_cast<std::uint32_t>(x)*0x9e3779b9u);
    h^=hash32(static_cast<std::uint32_t>(y)*0x85ebca6bu);
    h^=hash32(static_cast<std::uint32_t>(z)*0xc2b2ae35u);
    return static_cast<double>(hash32(h)&0x00ffffffu)/8388607.5-1.0;
}
double noise3(DVec3 p,std::uint32_t seed) noexcept {
    // All procedural callers pass small, finite body-local coordinates.
    if(!finite(p)||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1e8) return 0.0;
    const int x=static_cast<int>(std::floor(p.x)),y=static_cast<int>(std::floor(p.y)),z=static_cast<int>(std::floor(p.z));
    const auto fade=[](double t) { return t*t*t*(t*(t*6.0-15.0)+10.0); };
    const double a=fade(p.x-x),b=fade(p.y-y),c=fade(p.z-z);
    return std::lerp(std::lerp(std::lerp(lattice(x,y,z,seed),lattice(x+1,y,z,seed),a),
                              std::lerp(lattice(x,y+1,z,seed),lattice(x+1,y+1,z,seed),a),b),
                     std::lerp(std::lerp(lattice(x,y,z+1,seed),lattice(x+1,y,z+1,seed),a),
                              std::lerp(lattice(x,y+1,z+1,seed),lattice(x+1,y+1,z+1,seed),a),b),c);
}
double fbm(DVec3 p,std::uint32_t seed,int octaves) noexcept {
    double sum=0,amp=0.5,norm=0;
    for(int i=0;i<std::clamp(octaves,1,12);++i) {
        sum+=noise3(p,seed+static_cast<std::uint32_t>(i)*1013u)*amp;norm+=amp;
        amp*=0.5;p=p*2.03;
    }
    return sum/norm;
}
namespace {
struct HeightField { DVec3 direction,warped;double continent,ridge,regional,height; };
HeightField height_field(const Body& b,DVec3 direction) noexcept {
    const auto d=normalized(direction);
    const DVec3 warp{noise3(d*3.0,b.seed^0x781u),noise3(d*3.0,b.seed^0x213u),noise3(d*3.0,b.seed^0x961u)};
    const DVec3 q=d+warp*0.14;
    const double continent=fbm(q*3.6,b.seed,5);
    const double ridge=1.0-std::abs(fbm(q*29.0,b.seed^0x2fa1u,5));
    const double regional=fbm(q*120.0,b.seed^0xa317u,4);
    const double fine=fbm(d*4700.0,b.seed^0x1389u,5)*9.0*b.geography_scale;
    const double height=b.height_scale*(continent*0.63-0.055+smooth(-0.08,0.4,continent)*std::pow(ridge,5)*0.32+regional*0.025)+fine;
    return {d,q,continent,ridge,regional,height};
}
}
double sample_height(const Body& b,DVec3 direction) noexcept {
    if(b.kind==BodyKind::gas) return 0.0;
    const auto field=height_field(b,direction);
    return field.height+(b.kind==BodyKind::desert?b.height_scale*0.15:0.0);
}
SurfaceSample sample_surface(const Body& b,DVec3 direction) noexcept {
    if(b.kind==BodyKind::gas) {
        const auto d=normalized(direction);
        const double bands=0.5+0.5*std::sin(d.y*80.0+noise3(d*14.0,b.seed)*3.0);
        const float variation=static_cast<float>(0.9+noise3(d*170.0,b.seed^0x44u)*0.12);
        return {0.0,0.0,0.0,mix(FVec3{0.26f,0.11f,0.065f},FVec3{0.73f,0.61f,0.38f},static_cast<float>(bands))*variation};
    }
    const auto field=height_field(b,direction);
    const auto d=field.direction,q=field.warped;
    const double continent=field.continent,ridge=field.ridge,regional=field.regional;
    double height=field.height;
    const double moisture=std::clamp(fbm(q*8.0,b.seed^0xbeefu,4)*0.8+0.55,0.0,1.0);
    const double temp=std::clamp(1.1-std::abs(d.y)*1.15-std::max(height,0.0)/(13000.0*b.geography_scale),0.0,1.0);
    FVec3 color;
    switch(b.kind) {
    case BodyKind::temperate:
        if(height<0) color={0.09f,0.15f,0.13f};
        else if(height<95*b.geography_scale) color={0.53f,0.45f,0.26f};
        else if(temp<0.17||height>5600*b.geography_scale) color={0.82f,0.87f,0.88f};
        else if(height>3300*b.geography_scale) color={0.27f,0.28f,0.26f};
        else if(moisture<0.37) color={0.43f,0.36f,0.17f};
        else color=mix(FVec3{0.07f,0.18f,0.07f},FVec3{0.24f,0.34f,0.10f},static_cast<float>(1.0-moisture));
        break;
    case BodyKind::desert:
        height+=b.height_scale*0.15;
        color=mix(FVec3{0.29f,0.11f,0.045f},FVec3{0.67f,0.39f,0.17f},static_cast<float>(std::clamp(ridge,0.0,1.0)));
        break;
    case BodyKind::ice:
        color=mix(FVec3{0.20f,0.40f,0.52f},FVec3{0.79f,0.87f,0.90f},static_cast<float>(smooth(-0.25,0.4,continent)));
        break;
    case BodyKind::volcanic:
        color=regional<-0.24?FVec3{0.75f,0.07f,0.006f}:FVec3{0.11f,0.08f,0.07f};
        break;
    case BodyKind::gas: {
        height=0;const double bands=0.5+0.5*std::sin(d.y*80.0+noise3(d*14.0,b.seed)*3.0);
        color=mix(FVec3{0.26f,0.11f,0.065f},FVec3{0.73f,0.61f,0.38f},static_cast<float>(bands));
        break;
    }
    case BodyKind::moon:
        color=mix(FVec3{0.17f,0.18f,0.20f},FVec3{0.43f,0.43f,0.42f},static_cast<float>(ridge));
        break;
    }
    const float variation=static_cast<float>(0.9+noise3(d*170.0,b.seed^0x44u)*0.12);
    return {height,temp,moisture,color*variation};
}
DVec3 surface_normal(const Body& body,DVec3 direction,double spacing) noexcept {
    const auto d=normalized(direction),t=tangent(d),u=cross(d,t);
    const double e=std::max(spacing,0.25*body.geography_scale)/body.radius;
    const auto a=normalized(d+t*e),b=normalized(d-t*e),c=normalized(d+u*e),f=normalized(d-u*e);
    const auto dx=a*(body.radius+sample_height(body,a))-b*(body.radius+sample_height(body,b));
    const auto dy=c*(body.radius+sample_height(body,c))-f*(body.radius+sample_height(body,f));
    auto n=normalized(cross(dx,dy),d); if(dot(n,d)<0) n=-n;return n;
}
std::string_view kind_name(BodyKind kind) noexcept {
    constexpr std::array names{"TEMPERATE","DESERT","ICE","VOLCANIC","GAS GIANT","MOON"};
    return names[static_cast<std::size_t>(kind)];
}
Universe::Universe(std::uint32_t seed):Universe(seed,{}) {}
Universe::Universe(std::uint32_t seed,WorldConfig config):seed_(seed),config_(config) {
    if(!valid_world_config(config))throw std::invalid_argument("Invalid physical world size");
    bodies_={
        {0,hash32(seed),"VERDANT",BodyKind::temperate,6'371'000,18'000,100'000,9.81,1.65e9,720'000,0.9,0.04,-1,{0.16f,0.37f,0.70f},true},
        {1,hash32(seed+1),"EMBER",BodyKind::desert,3'900'000,25'000,55'000,4.2,1.13e9,480'000,2.4,0.08,-1,{0.57f,0.23f,0.08f},false},
        {2,hash32(seed+2),"BOREAL",BodyKind::ice,5'300'000,16'000,65'000,7.0,2.55e9,1'080'000,4.2,0.05,-1,{0.15f,0.42f,0.56f},false},
        {3,hash32(seed+3),"CINDER",BodyKind::volcanic,2'700'000,22'000,35'000,3.3,0.74e9,260'000,5.4,0.12,-1,{0.42f,0.11f,0.04f},false},
        {4,hash32(seed+4),"AUREL",BodyKind::gas,58'000'000,0,900'000,18.0,4.0e9,1'600'000,3.1,0.025,-1,{0.51f,0.34f,0.18f},false},
        {5,hash32(seed+5),"SELENE",BodyKind::moon,1'737'000,17'000,0,1.62,42'000'000,90'000,0.1,0.12,0,{0,0,0},false}
    };
    length_scale_=config.reference_radius_meters/bodies_[config.reference_body].radius;
    if(length_scale_!=1.0)for(auto& body:bodies_) {
        body.radius*=length_scale_;body.height_scale*=length_scale_;
        body.atmosphere_height*=length_scale_;body.orbit_radius*=length_scale_;
        body.geography_scale=length_scale_;
    }
}
DVec3 Universe::center(std::size_t index,double seconds) const {
    const auto& b=body(index);
    const double angle=b.orbit_phase+std::remainder(seconds,b.orbit_period)*(2.0*pi/b.orbit_period);
    DVec3 p{std::cos(angle)*b.orbit_radius,std::sin(angle)*std::sin(b.inclination)*b.orbit_radius,
            std::sin(angle)*std::cos(b.inclination)*b.orbit_radius};
    if(b.parent>=0) p+=center(static_cast<std::size_t>(b.parent),seconds);
    return p;
}
std::size_t Universe::nearest(DVec3 p,double seconds) const {
    std::size_t result=0;double best=std::numeric_limits<double>::max();
    for(std::size_t i=0;i<bodies_.size();++i) {
        const auto dist=length(p-center(i,seconds))-bodies_[i].radius;
        if(dist<best) { best=dist;result=i; }
    }
    return result;
}
double Universe::ground_radius(std::size_t index,DVec3 direction) const {
    const auto& b=body(index);const double h=sample_height(b,direction);
    return b.radius+(b.ocean?std::max(h,0.0):h);
}
} // namespace epoch::planet
