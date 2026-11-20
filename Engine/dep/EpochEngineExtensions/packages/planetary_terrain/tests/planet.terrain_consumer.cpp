// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Independent module consumer; no graphics, model, scheduler or viewer starts.
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <stop_token>
import planet.terrain;
using namespace epoch::extensions::planetary_terrain::core;
static void require(bool condition,const char* diagnostic) {
    if(!condition) throw std::runtime_error(diagnostic);
}
int main() {
 try {
    Universe universe{73};
    auto body=universe.body(0);
    require(body.radius==6'371'000.0,"metric Earth radius");
    body.ocean=false;
    const TileKey tile{400,512,static_cast<std::uint8_t>(body.id),0,10};
    const MeshConfig config{8,false,false};
    const auto mesh=build_tile(body,tile,config);
    require(!mesh.vertices.empty()&&!mesh.indices.empty(),"actual indexed mesh is empty");
    require(mesh.indices.size()%3u==0u,"triangle index count");
    require(finite(mesh.anchor)&&std::isfinite(mesh.bound_radius)&&mesh.bound_radius>0,"finite metric bounds");
    const auto direction=tile_direction(tile,0.5,0.5);
    const auto expectedAnchor=direction*(body.radius+sample_height(body,direction));
    require(length(mesh.anchor-expectedAnchor)<1e-7,"body-local double tile anchor");
    for(const auto index:mesh.indices)require(index<mesh.vertices.size(),"index outside vertex storage");
    double maximumExtent{};
    for(const auto& vertex:mesh.vertices) {
        require(finite(vertex.position)&&finite(vertex.normal)&&finite(vertex.parent_delta),"nonfinite CPU vertex");
        for(const auto color:vertex.color)require(std::isfinite(color)&&color>=0.0f&&color<=1.0f,"vertex color outside normalized range");
        require(std::abs(length(DVec3{vertex.normal})-1.0)<1e-4,"normal is not unit length");
        maximumExtent=std::max({maximumExtent,length(DVec3{vertex.position}),length(DVec3{vertex.position}+DVec3{vertex.parent_delta})});
    }
    require(maximumExtent<=mesh.bound_radius,"bounds miss geometry or geomorph sweep");
    const auto firstDirection=tile_direction(tile,0.0,0.0);
    const auto expectedFirst=firstDirection*(body.radius+sample_height(body,firstDirection));
    require(length(mesh.anchor+DVec3{mesh.vertices.front().position}-expectedFirst)<0.02,"vertex is not metric offset from tile anchor");
    const auto child=build_tile(body,children(tile).front(),config);
    const auto side=static_cast<std::size_t>(config.cells+1);
    for(std::size_t y=0;y<side;y+=2)
        for(std::size_t x=0;x<side;x+=2)
            require(child.vertices[y*side+x].color==mesh.vertices[(y/2)*side+x/2].color,
                "shared parent and child terrain samples disagree on color");
    const auto center=universe.center(body.id,123.0);
    const auto camera=center+mesh.anchor+DVec3{5,7,11};
    const auto relativeTranslation=center+mesh.anchor-camera;
    require(length(relativeTranslation-DVec3{-5,-7,-11})<0.001,"double camera-relative translation");

    const Universe verdant{1337};
    auto plateauBody=verdant.body(0);
    plateauBody.ocean=false;
    const TileKey plateauKey{289,918418,0,0,20};
    const MeshConfig closeConfig{16,false,false};
    const auto plateau=build_tile(plateauBody,plateauKey,closeConfig);
    const auto repeated=build_tile(plateauBody,plateauKey,closeConfig);
    const auto parentKey=parent(plateauKey);
    const auto coarse=build_tile(plateauBody,parentKey,closeConfig);
    constexpr unsigned closeSide=17;
    require(plateau.vertices.size()==closeSide*closeSide&&plateau.indices.size()==16u*16u*6u
        &&std::equal(plateau.indices.begin(),plateau.indices.end(),repeated.indices.begin(),repeated.indices.end())
        &&repeated.vertices.size()==plateau.vertices.size(),
        "plateau grid topology or repeatability changed");
    const auto luminance=[](double r,double g,double b){return 0.2126*r+0.7152*g+0.0722*b;};
    double beforeMin=1.0,beforeMax=0.0,afterMin=1.0,afterMax=0.0,maximumSpacing{};
    for(unsigned y=0;y<closeSide;++y)for(unsigned x=0;x<closeSide;++x)
    {
        const auto index=y*closeSide+x;
        const auto& vertex=plateau.vertices[index];
        const auto& again=repeated.vertices[index];
        const auto direction=tile_direction(plateauKey,x/16.0,y/16.0);
        const auto sample=sample_surface(plateauBody,direction);
        const auto normal=surface_normal(plateauBody,direction,3.0);
        const double facing=normal.x*direction.x+normal.y*direction.y+normal.z*direction.z;
        require(sample.height>5000.0&&1.0-facing<0.025,"plateau fixture lost its nearly flat snow");
        require(length(plateau.anchor+DVec3{vertex.position}-direction*(plateauBody.radius+sample.height))<1e-5
            &&length(DVec3{vertex.normal}-normal)<1e-7&&vertex.color[3]==0.0f,
            "terrain albedo changed geometry, normals or material id");
        require(vertex.position==again.position&&vertex.normal==again.normal
            &&vertex.parent_delta==again.parent_delta&&vertex.color==again.color,
            "metric albedo or geometry is not deterministic");
        if(x%2u==0&&y%2u==0)
        {
            const unsigned parentX=(plateauKey.x&1u)*8u+x/2u;
            const unsigned parentY=(plateauKey.y&1u)*8u+y/2u;
            require(vertex.color==coarse.vertices[parentY*closeSide+parentX].color,
                "shared snowy detail changed across parent and child");
        }
        const double before=luminance(sample.color.x,sample.color.y,sample.color.z);
        const double after=luminance(vertex.color[0],vertex.color[1],vertex.color[2]);
        require(std::abs(before-after)<0.081,"terrain detail overwhelmed base snow color");
        beforeMin=std::min(beforeMin,before);beforeMax=std::max(beforeMax,before);
        afterMin=std::min(afterMin,after);afterMax=std::max(afterMax,after);
        if(x)maximumSpacing=std::max(maximumSpacing,length(DVec3{vertex.position-plateau.vertices[index-1u].position}));
    }
    require(maximumSpacing<0.5&&afterMax-afterMin>(beforeMax-beforeMin)+0.02,
        "fine albedo contrast is not resolved by the plateau grid");

    const TileKey faceEdge{0,plateauKey.y,0,0,plateauKey.level};
    const auto adjacent=neighbors(faceEdge).front();
    require(adjacent.face!=faceEdge.face,"cube seam fixture has no adjacent face");
    const auto firstFace=build_tile(plateauBody,faceEdge,closeConfig);
    const auto secondFace=build_tile(plateauBody,adjacent,closeConfig);
    for(unsigned y=0;y<closeSide;++y)
    {
        const auto point=tile_direction(faceEdge,0.0,y/16.0);
        bool found{};
        for(unsigned v=0;v<closeSide&&!found;++v)for(unsigned u=0;u<closeSide&&!found;++u)
        {
            if(length(tile_direction(adjacent,u/16.0,v/16.0)-point)>=1e-12)continue;
            found=true;
            require(firstFace.vertices[y*closeSide].color==secondFace.vertices[v*closeSide+u].color,
                "shared material sample changed at a cube-face seam");
        }
        require(found,"cube seam fixture missed a shared edge vertex");
    }

    std::stop_source cancel;cancel.request_stop();
    const auto cancelled=build_tile(body,tile,config,cancel.get_token());
    require(cancelled.vertices.empty()&&cancelled.indices.empty(),"cancelled mesh published geometry");
    bool invalidCells{};
    try{(void)build_tile(body,tile,MeshConfig{1,false,false});}catch(const std::invalid_argument&){invalidCells=true;}
    require(invalidCells,"invalid cell bound admitted");
    auto invalidBody=body;invalidBody.radius=std::numeric_limits<double>::quiet_NaN();
    bool invalidRadius{};
    try{(void)build_tile(invalidBody,tile,config);}catch(const std::invalid_argument&){invalidRadius=true;}
    require(invalidRadius,"nonfinite body radius admitted");
    std::ofstream result{"result.json"};
    result<<std::setprecision(12)<<"{\"cpu_consumer_pass\":true,\"vertices\":"<<mesh.vertices.size()<<",\"indices\":"<<mesh.indices.size()
        <<",\"vertex_stride\":"<<sizeof(Vertex)<<",\"body_local_anchor\":true,\"camera_relative_double\":true,\"cancellation\":true,\"bounds\":true,\"gpu_started\":false"
        <<",\"verdant_detail_grid_max_spacing_m\":"<<maximumSpacing
        <<",\"verdant_detail_before_range\":"<<beforeMax-beforeMin
        <<",\"verdant_detail_after_range\":"<<afterMax-afterMin<<"}\n";
    return result?0:2;
 } catch(const std::exception& error) {
    std::ofstream result{"failure.txt"};result<<error.what();return 1;
 }
}
