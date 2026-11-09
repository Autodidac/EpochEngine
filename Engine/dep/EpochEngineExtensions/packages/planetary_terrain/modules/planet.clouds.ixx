// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <stop_token>
#include <vector>
export module planet.clouds;
import planet.terrain;
import asset.mesh_artifact;
import asset.cloud_volume;

export namespace epoch::extensions::planetary_terrain::clouds
{
    namespace core = epoch::extensions::planetary_terrain::core;
    namespace mesh = epochengine::asset::mesh;
    // Initial geometric cumulus layer, not a volumetric renderer or an
    // occlusion certificate. Opaque closed lobes can be seen from either side.
    // One immutable artifact is reused; time changes only its wind transform.
    inline constexpr mesh::Limits limits{24'000u, 140'000u, 2ull*1024u*1024u};
    [[nodiscard]] inline double wind_degrees(double seconds) noexcept
    {
        if(!std::isfinite(seconds)||seconds<0.0||seconds>=1e12)return 0.0;
        return std::fmod(seconds,604800.0)/604800.0*360.0;
    }
    [[nodiscard]] inline bool supports(const core::Body& body) noexcept
    {
        return body.atmosphere_height>0.0&&std::isfinite(body.radius)&&body.radius>0.0
            &&std::isfinite(body.height_scale)&&body.height_scale>=0.0
            &&(body.kind==core::BodyKind::temperate||body.kind==core::BodyKind::ice);
    }
    [[nodiscard]] inline std::optional<epochengine::asset::cloud_volume::Recipe> density_recipe(
        const core::Body& body) noexcept
    {
        if(!supports(body))return {};
        epochengine::asset::cloud_volume::Recipe value{
            .seed=body.seed^0xc10d5u,
            // Keep ordinary hills below the deck rather than immersing most
            // landings in cloud. Tall authored peaks may still enter a cloud.
            .base_altitude_meters=(std::max)(1.0,body.height_scale*0.40),
            .thickness_meters=(std::max)(1.0,body.height_scale*0.16),
            .horizontal_scale_meters=(std::max)(10.0,body.radius*0.002),
            .coverage=0.60f,.extinction_per_meter=0.0012f};
        // Preserve optical depth when a user authors a much smaller world.
        value.extinction_per_meter=static_cast<float>((std::min)(1.0,2.592/value.thickness_meters));
        return epochengine::asset::cloud_volume::valid(value,body.radius)?std::optional{value}:std::nullopt;
    }
    [[nodiscard]] inline std::shared_ptr<const mesh::Artifact> build(
        const core::Body& body,std::stop_token stop = {}) noexcept
    {
        if(stop.stop_requested()||!supports(body))return {};
        try
        {
            constexpr unsigned candidates=128u,segments=12u,rings=4u;
            constexpr double pi=std::numbers::pi_v<double>;
            std::vector<mesh::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            vertices.reserve(candidates*3u*(2u+segments*rings));
            indices.reserve(candidates*3u*rings*segments*6u);
            const double base=(std::max)(body.height_scale*1.25,body.radius*0.0015);
            const double thickness=(std::max)(body.radius*0.00015,body.height_scale*0.045);
            for(unsigned candidate=0;candidate<candidates;++candidate)
            {
                if(stop.stop_requested())return {};
                // Equal-area directions avoid latitude bands and seam duplicates.
                const double y=1.0-2.0*(candidate+0.5)/candidates;
                const double phi=candidate*2.399963229728653;
                const double horizontal=std::sqrt((std::max)(0.0,1.0-y*y));
                const core::DVec3 direction{horizontal*std::cos(phi),y,horizontal*std::sin(phi)};
                if(core::fbm(direction*3.1,body.seed^0xc10d5u,3)<-0.10)continue;
                const auto tangent=core::tangent(direction);
                const auto bitangent=core::cross(direction,tangent);
                const auto random=core::hash32(body.seed^(candidate*1013u));
                for(unsigned lobe=0;lobe<3u;++lobe)
                {
                    const double spread=(double(lobe)-1.0)*0.055;
                    const auto center=core::normalized(direction+tangent*spread
                        +bitangent*(double((random>>(lobe*3u))&7u)-3.5)*0.005);
                    const auto t=core::tangent(center);
                    const auto b=core::cross(center,t);
                    const double width=0.06+double((random>>(lobe*4u))&15u)*0.004;
                    const double depth=width*(0.60+double((random>>19u)&7u)*0.035);
                    const auto first=static_cast<std::uint32_t>(vertices.size());
                    const auto vertex=[&](double polar,double angle)
                    {
                        const double radial=std::cos(polar),rim=std::sin(polar);
                        const double x=rim*std::cos(angle),z=rim*std::sin(angle);
                        const auto up=core::normalized(center+t*(x*width)+b*(z*depth));
                        const auto position=up*((body.radius+base+thickness*radial)*10.0/body.radius);
                        // Ellipsoid gradient in the curved tangent frame.
                        const auto normal=core::normalized(t*(x/(body.radius*width))
                            +b*(z/(body.radius*depth))+up*(radial/thickness));
                        const float shade=static_cast<float>(0.84+0.13*(radial*0.5+0.5));
                        vertices.push_back({{float(position.x),float(position.y),float(position.z)},
                            {float(normal.x),float(normal.y),float(normal.z)},{},{shade,shade,shade}});
                    };
                    vertex(0.0,0.0);
                    for(unsigned ring=1u;ring<=rings;++ring)
                        for(unsigned segment=0;segment<segments;++segment)
                            vertex(pi*ring/(rings+1u),2.0*pi*segment/segments);
                    vertex(pi,0.0);
                    for(unsigned segment=0;segment<segments;++segment)
                    {
                        const auto next=(segment+1u)%segments;
                        indices.insert(indices.end(),{first,first+1u+segment,first+1u+next});
                        for(unsigned ring=0;ring+1u<rings;++ring)
                        {
                            const auto a=first+1u+ring*segments+segment,c=a+segments;
                            const auto b0=first+1u+ring*segments+next,d=b0+segments;
                            indices.insert(indices.end(),{a,c,b0,b0,c,d});
                        }
                        const auto a=first+1u+(rings-1u)*segments+segment;
                        const auto b0=first+1u+(rings-1u)*segments+next;
                        indices.insert(indices.end(),{a,first+1u+rings*segments,b0});
                    }
                }
            }
            if(stop.stop_requested()||vertices.empty())return {};
            return mesh::build_artifact(vertices,indices,limits).artifact;
        }
        catch(...){return {};}
    }
}
