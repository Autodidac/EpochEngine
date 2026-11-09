// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Independent module consumer; no graphics, model, scheduler or viewer starts.
#include <algorithm>
#include <cmath>
#include <fstream>
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
        for(const auto color:vertex.color)require(std::isfinite(color),"nonfinite vertex color");
        require(std::abs(length(DVec3{vertex.normal})-1.0)<1e-4,"normal is not unit length");
        maximumExtent=std::max({maximumExtent,length(DVec3{vertex.position}),length(DVec3{vertex.position}+DVec3{vertex.parent_delta})});
    }
    require(maximumExtent<=mesh.bound_radius,"bounds miss geometry or geomorph sweep");
    const auto firstDirection=tile_direction(tile,0.0,0.0);
    const auto expectedFirst=firstDirection*(body.radius+sample_height(body,firstDirection));
    require(length(mesh.anchor+DVec3{mesh.vertices.front().position}-expectedFirst)<0.02,"vertex is not metric offset from tile anchor");
    const auto center=universe.center(body.id,123.0);
    const auto camera=center+mesh.anchor+DVec3{5,7,11};
    const auto relativeTranslation=center+mesh.anchor-camera;
    require(length(relativeTranslation-DVec3{-5,-7,-11})<0.001,"double camera-relative translation");
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
    result<<"{\"cpu_consumer_pass\":true,\"vertices\":"<<mesh.vertices.size()<<",\"indices\":"<<mesh.indices.size()<<",\"vertex_stride\":"<<sizeof(Vertex)<<",\"body_local_anchor\":true,\"camera_relative_double\":true,\"cancellation\":true,\"bounds\":true,\"gpu_started\":false}\n";
    return result?0:2;
 } catch(const std::exception& error) {
    std::ofstream result{"failure.txt"};result<<error.what();return 1;
 }
}
