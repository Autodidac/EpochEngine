// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <stop_token>
import planet.clouds;
import asset.cloud_volume;
import planet.terrain;
import project.planetary_runtime;
namespace clouds=epoch::extensions::planetary_terrain::clouds;
namespace core=epoch::extensions::planetary_terrain::core;
namespace host=epochengine::project_planetary_runtime;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main()
{
    try
    {
        for(const double radius:std::array{250.0,1'737'000.0,6'371'000.0,100'000'000.0})
        {
            const core::Universe world{1337,{0u,radius}};
            const auto& body=world.body(0u);
            const auto density=clouds::density_recipe(body);
            require(density&&epochengine::asset::cloud_volume::valid(*density,body.radius),
                "bounded metric cloud recipe is missing");
            require(clouds::density_recipe(body)==density,"cloud recipe is nondeterministic");
            require(density->base_altitude_meters>body.height_scale*0.35
                &&density->base_altitude_meters+density->thickness_meters<body.atmosphere_height,
                "cloud deck does not clear ordinary terrain within its atmosphere");
            require(!clouds::density_recipe(world.body(5u))&&!clouds::density_recipe(world.body(4u)),
                "airless moon/gas body gained unsupported density");
            const auto first=clouds::build(body),second=clouds::build(body);
            require(first&&second&&first->key()==second->key(),"cloud layer is missing or nondeterministic");
            require(first->vertices().size()<clouds::limits.maximum_vertices
                &&first->indices().size()<clouds::limits.maximum_indices,"cloud memory bound exceeded");
            for(const auto& vertex:first->vertices())
            {
                const core::DVec3 point{vertex.position[0],vertex.position[1],vertex.position[2]};
                require(core::length(point)>10.0+body.height_scale/body.radius*10.0,
                    "cloud layer intersects the terrain height bound");
            }
            for(std::size_t i=0;i<first->indices().size();i+=3u)
            {
                const auto& av=first->vertices()[first->indices()[i]];
                const auto& bv=first->vertices()[first->indices()[i+1u]];
                const auto& cv=first->vertices()[first->indices()[i+2u]];
                const core::DVec3 a{av.position[0],av.position[1],av.position[2]};
                const core::DVec3 b{bv.position[0],bv.position[1],bv.position[2]};
                const core::DVec3 c{cv.position[0],cv.position[1],cv.position[2]};
                const auto crossed=core::cross(b-a,c-a);
                const core::DVec3 normal{av.normal[0]+bv.normal[0]+cv.normal[0],
                    av.normal[1]+bv.normal[1]+cv.normal[1],av.normal[2]+bv.normal[2]+cv.normal[2]};
                require(crossed.x*normal.x+crossed.y*normal.y+crossed.z*normal.z>0.0,
                    "cloud lobe triangle winding disagrees with its outward normals");
            }
            const core::Universe different{1338,{0u,radius}};
            require(clouds::density_recipe(different.body(0u))->seed!=density->seed,"cloud recipe seed did not change");
            require(clouds::build(different.body(0u))->key()!=first->key(),"cloud seed does not affect geometry");
            require(clouds::build(world.body(2u))!=nullptr,"ice atmosphere has no cloud layer");
            require(!clouds::build(world.body(5u))&&!clouds::build(world.body(4u)),
                "airless moon/gas body gained unsupported clouds");
            std::stop_source stopped;stopped.request_stop();
            require(!clouds::build(body,stopped.get_token()),"cloud cancellation ignored");
        }
        require(clouds::wind_degrees(0.0)==0.0&&clouds::wind_degrees(604800.0)==0.0
            &&clouds::wind_degrees(302400.0)==180.0,"cloud wind time is not bounded and periodic");
        require(clouds::wind_degrees(50.0)==clouds::wind_degrees(50.0+604800.0),"cloud wind cannot reproduce a time sample");
        require(clouds::wind_degrees(-1.0)==0.0&&clouds::wind_degrees(std::numeric_limits<double>::infinity())==0.0,
            "invalid cloud time is not rejected");
        std::ofstream result{"clouds-result.json"};
        result<<"{\"pass\":true,\"kind\":\"bounded geometric cumulus\",\"volumetric\":false,\"cloud_occlusion\":false,\"native_visual_acceptance\":false}\n";
        return result?0:2;
    }
    catch(const std::exception& error){std::ofstream failure{"clouds-failure.txt"};failure<<error.what();return 1;}
}
