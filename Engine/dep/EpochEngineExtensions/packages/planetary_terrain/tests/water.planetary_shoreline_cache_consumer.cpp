// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>
#include "../tools/planet.stream_cache.hpp"
import planet.terrain;
import water.planetary;
import asset.mesh_artifact;
namespace p=epoch::extensions::planetary_terrain::core;
namespace w=epoch::extensions::planetary_terrain::water;
namespace m=epochengine::asset::mesh;
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(int argc,char** argv){
 CHECK(argc==3);std::ofstream report{argv[1]};report<<std::setprecision(17);
 try{
 p::Universe universe{1337};const auto body=universe.body(0);constexpr double scale=10.0/6371000.0;
 w::Surface water;CHECK(water.evaluate(1337,0.0));
 std::uint64_t patches{},bad{};
 for(const auto& entry:std::filesystem::directory_iterator{argv[2]}){
  p::TileKey key;if(!planet_stream_cache::parse_filename(entry.path().filename().string(),key))continue;
  report<<"checking "<<entry.path().filename().string()<<'\n';report.flush();
  p::Mesh source;CHECK(planet_stream_cache::read_patch(entry.path(),key,16,source,true));
  std::vector<m::Vertex> vertices;std::vector<std::array<float,3>> deltas;
  for(const auto& v:source.vertices){const auto q=source.anchor+p::DVec3{v.position};
   vertices.push_back({{float(q.x*scale),float(q.y*scale),float(q.z*scale)},
    {v.normal.x,v.normal.y,v.normal.z},{},{v.color[0],v.color[1],v.color[2]}});
   deltas.push_back({float(v.parent_delta.x*scale),float(v.parent_delta.y*scale),float(v.parent_delta.z*scale)});
  }
  auto indices=source.indices;const auto original=vertices;
  CHECK(water.apply_patch(vertices,indices,0,0,source,body,key,16,scale,{},32u*1024u*1024u,&deltas));
  CHECK(deltas.size()==vertices.size());
  const auto first_water=std::ranges::find_if(source.vertices,[](const auto& v){return v.color[3]>.5f;});
  const auto terrain_count=static_cast<std::size_t>(first_water-source.vertices.begin());
  for(std::size_t i=0;i<terrain_count;++i)CHECK(vertices[i].position==original[i].position);
  std::vector<std::uint32_t> ground_before,ground_after;
  for(std::size_t t=0;t<source.indices.size();t+=3u)if(source.indices[t]<terrain_count)
   ground_before.insert(ground_before.end(),source.indices.begin()+t,source.indices.begin()+t+3u);
  const auto bad_before=bad;
  for(std::size_t t=0;t<indices.size();t+=3u){
   if(indices[t]<terrain_count)ground_after.insert(ground_after.end(),indices.begin()+t,indices.begin()+t+3u);
   const auto a=vertices[indices[t]].position,b=vertices[indices[t+1u]].position,c=vertices[indices[t+2u]].position;
   const auto x=(double(b[1])-a[1])*(double(c[2])-a[2])-(double(b[2])-a[2])*(double(c[1])-a[1]);
   const auto y=(double(b[2])-a[2])*(double(c[0])-a[0])-(double(b[0])-a[0])*(double(c[2])-a[2]);
   const auto z=(double(b[0])-a[0])*(double(c[1])-a[1])-(double(b[1])-a[1])*(double(c[0])-a[0]);
   if(x*x+y*y+z*z<=1e-24){++bad;
    report<<entry.path().filename().string()<<" triangle="<<t/3u<<" area_squared="<<x*x+y*y+z*z;
    for(unsigned i=0;i<3u;++i){const auto id=indices[t+i];const auto& q=vertices[id].position;
     report<<" vertex="<<id<<'['<<q[0]<<','<<q[1]<<','<<q[2]<<']';}report<<'\n';
   }
  }
  CHECK(ground_before==ground_after);
  if(!m::build_artifact(vertices,indices,{100000u,300000u,32u*1024u*1024u})){
   report<<"artifact refused "<<entry.path().filename().string()<<'\n';
   CHECK(bad>bad_before);
  }
  ++patches;report.flush();
 }
 report<<"patches="<<patches<<" invalid_triangles="<<bad<<'\n';
 CHECK(patches>0u);CHECK(bad==0u);return 0;
 }catch(const std::exception& e){report<<e.what()<<'\n';return 99;}
}
