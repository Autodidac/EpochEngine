// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>
import planet.terrain;
import water.planetary;
import asset.mesh_artifact;
namespace p=epoch::extensions::planetary_terrain::core;
namespace w=epoch::extensions::planetary_terrain::water;
namespace m=epochengine::asset::mesh;
using V=std::array<double,3>;
V sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
double dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V pos(const m::Vertex& v){return {v.position[0],v.position[1],v.position[2]};}
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(int argc,char** argv){
 CHECK(argc==2);std::ofstream report{argv[1]};
 p::Universe universe{1337};const auto body=universe.body(0);CHECK(body.ocean);
 w::Surface water;std::uint64_t surface_crossings{},skirt_crossings{};double maximum_error{};
 for(const double seconds:{0.0,1.25}){
 CHECK(water.evaluate(1337,seconds));
 // L4 face0 (1,14) reproduces the retained native stream's collapsed
 // shoreline fan after distinct double intersections narrow to float.
 for(const p::TileKey key:{p::TileKey{1,0,0,1,1},p::TileKey{0,0,0,4,2},p::TileKey{1,14,0,0,4}}){
 const auto source=p::build_tile(body,key,{16,false,true});const auto nv=source.vertices.size();
 CHECK(nv==706u);std::vector<m::Vertex> vertices;std::vector<std::array<float,3>> deltas;
 constexpr double scale=10.0/6371000.0;
 for(const auto& v:source.vertices){const auto q=source.anchor+p::DVec3{v.position};
 vertices.push_back({{float(q.x*scale),float(q.y*scale),float(q.z*scale)},
  {v.normal.x,v.normal.y,v.normal.z},{},{v.color[0],v.color[1],v.color[2]}});
 deltas.push_back({float(v.parent_delta.x*scale),float(v.parent_delta.y*scale),float(v.parent_delta.z*scale)});}
 const auto terrain_before=vertices;const auto deltas_before=deltas;auto indices=source.indices;
 CHECK(water.apply_patch(vertices,indices,0,0,source,body,key,16,scale,{},32u*1024u*1024u,&deltas));
 CHECK(vertices.size()>nv&&deltas.size()==vertices.size());
 for(std::size_t i=0;i<353u;++i){CHECK(vertices[i].position==terrain_before[i].position);CHECK(deltas[i]==deltas_before[i]);}
 // Every generated shoreline point must remain on its actual input water
 // edge. Surface crossings must also meet the corresponding rendered ground
 // plane. The old spherical re-projection moves coarse points kilometres off.
 for(std::size_t i=nv;i<vertices.size();++i){
  const auto q=pos(vertices[i]);bool matched=false;
  for(std::size_t tri=1920u;tri<source.indices.size()&&!matched;tri+=3u){
   const std::array ids{source.indices[tri],source.indices[tri+1u],source.indices[tri+2u]};
   const bool surface=std::ranges::all_of(ids,[](auto id){return id<642u;});
   for(unsigned edge=0;edge<3u&&!matched;++edge){
    const auto a=ids[edge],b=ids[(edge+1u)%3u];const auto av=pos(vertices[a]),bv=pos(vertices[b]);
    const auto direction=sub(bv,av);const double length2=dot(direction,direction);if(length2<=0.0)continue;
    const double t=dot(sub(q,av),direction)/length2;if(t<-.00001||t>1.00001)continue;
    V residual=sub(q,av);for(unsigned axis=0;axis<3u;++axis)residual[axis]-=t*direction[axis];
    if(dot(residual,residual)>4e-12)continue;
    if(surface){const auto ga=pos(vertices[ids[0]-353u]),gb=pos(vertices[ids[1]-353u]),gc=pos(vertices[ids[2]-353u]);
     const auto normal=cross(sub(gb,ga),sub(gc,ga));const auto error=std::abs(dot(normal,sub(q,ga)))/std::sqrt(dot(normal,normal));
     if(error>2e-6)continue;maximum_error=(std::max)(maximum_error,error);++surface_crossings;
    }else ++skirt_crossings;
    for(unsigned axis=0;axis<3u;++axis){
     const auto endpoint=std::lerp(double(vertices[a].position[axis])+deltas[a][axis],double(vertices[b].position[axis])+deltas[b][axis],t);
     CHECK(std::abs(double(vertices[i].position[axis])+deltas[i][axis]-endpoint)<3e-6);
    }
    matched=true;
   }
  }
  CHECK(matched);
 }
 CHECK(m::build_artifact(vertices,indices,{100000u,300000u,32u*1024u*1024u}));
 }}
 CHECK(surface_crossings>100u&&skirt_crossings>10u);
 report<<surface_crossings<<' '<<skirt_crossings<<' '<<maximum_error<<'\n';return 0;
}
