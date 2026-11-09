// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stop_token>
#include <string>
#include <vector>
import project.planetary_snapshot;
import asset.mesh_artifact;
import water.fft;
import planet.terrain;
namespace p=epochengine::project_planetary_snapshot;
namespace m=epochengine::asset::mesh;
namespace fft=epochengine::water::fft;
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
bool same(const m::Vertex& a,const m::Vertex& b)
{return a.position==b.position&&a.normal==b.normal&&a.uv==b.uv&&a.color==b.color;}
int main(int argc,char** argv)
{
 if(argc==2)
 {
  namespace core=epoch::extensions::planetary_terrain::core;
  const core::Universe world{1337};const auto body=world.body(0);std::ofstream pose{argv[1]};pose<<std::setprecision(17);
  for(std::uint8_t face=0;face<6u;++face)for(unsigned x=0;x<8u;++x)for(unsigned y=0;y<8u;++y)
  {
   const auto direction=core::cube_direction(face,(double(x)+.5)/4.0-1.0,(double(y)+.5)/4.0-1.0);
   const auto height=core::sample_height(body,direction);if(height>=-100.0)continue;
   const double radius=(body.radius+1.8)*10.0/body.radius;
   pose<<direction.x*radius<<' '<<direction.y*radius<<' '<<direction.z*radius<<' '<<height<<'\n';return 0;
  }
  return 90;
 }
 CHECK(argc==4);const std::filesystem::path folder{argv[1]};std::ofstream report{argv[3]};
 const p::Limits limits{{1048576u,6291456u,32u*1024u*1024u},4096u,16u*1024u*1024u,170u*1024u*1024u,66u*1024u*1024u};
 const auto artifact=m::read_artifact_file(folder/"terrain.epoch_mesh",limits.artifact);CHECK(artifact);
 std::ifstream input{folder/"terrain.epoch_planet_patch",std::ios::binary|std::ios::ate};CHECK(input&&input.tellg()>0&&input.tellg()<=limits.maximum_metadata_bytes);
 std::vector<std::byte> encoded(static_cast<std::size_t>(input.tellg()));input.seekg(0);CHECK(input.read(reinterpret_cast<char*>(encoded.data()),static_cast<std::streamsize>(encoded.size())));
 const p::Identity identity{artifact.artifact->key(),"sha256:"+std::string(64u,'a'),std::string(64u,'b'),std::stoull(argv[2])};
 const auto metadata=p::decode(encoded,*artifact.artifact,identity,limits);CHECK(metadata&&!metadata->water_vertices.empty());
 p::Snapshot source{artifact.artifact,*metadata};CHECK(p::encode(*metadata,*artifact.artifact,limits)==encoded);
 for(const auto& patch:metadata->patches)if(patch.target&&patch.key.level==20u)
 {
  CHECK(metadata->water_meters_to_artifact==1.0&&metadata->water_origin_meters!=std::array<double,3>{});
  const auto a=artifact.artifact->vertices()[patch.first_vertex].position,b=artifact.artifact->vertices()[patch.first_vertex+1u].position;
  const auto spacing=std::hypot(double(a[0])-b[0],double(a[1])-b[1],double(a[2])-b[2]);CHECK(spacing>0&&spacing<1.0);
  report<<"fine_level=20 spacing_meters="<<spacing<<" local_metric_frame=1\n";break;
 }
 fft::Settings settings;settings.seed=1337;fft::Evaluator evaluator;
 std::shared_ptr<const m::Artifact> zero;
 for(const auto seconds:{0.0,1.25,0.0})
 {
  const auto tile=evaluator.evaluate(settings,seconds,4u*1024u*1024u);CHECK(tile);
  const auto started=std::chrono::steady_clock::now();const auto sampled=p::resample_water(source,*tile.tile,limits);CHECK(sampled);
  report<<"seconds="<<seconds<<" wall_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
   <<" vertices="<<sampled->vertices().size()<<" indices="<<sampled->indices().size()<<'\n';
  if(!zero)zero=sampled;else if(seconds==0.0)CHECK(sampled->key()==zero->key());else CHECK(sampled->key()!=zero->key());
  // Ground is copied as exact ordered vertex chunks, with no terrain remesh.
  std::size_t cursor{};
  for(const auto& patch:source.metadata.patches)if(patch.target)
  {
   const auto count=patch.terrain_vertices?patch.terrain_vertices:patch.vertex_count;
   bool found{};
   for(;cursor+count<=sampled->vertices().size();++cursor)
   {
    if(!same(sampled->vertices()[cursor],source.artifact->vertices()[patch.first_vertex]))continue;
    found=true;for(std::uint32_t i=0;i<count;++i)if(!same(sampled->vertices()[cursor+i],source.artifact->vertices()[patch.first_vertex+i])){found=false;break;}
    if(found){cursor+=count;break;}
   }
   CHECK(found);
  }
  std::stop_source stopped;stopped.request_stop();CHECK(!p::resample_water(source,*tile.tile,limits,stopped.get_token()));
  auto too_small=limits;too_small.maximum_transient_bytes=1u;CHECK(!p::resample_water(source,*tile.tile,too_small));
 }
 CHECK(p::encode(source.metadata,*source.artifact,limits)==encoded);
 auto bad=source;bad.metadata.water_vertices[0].direction[0]=std::numeric_limits<double>::quiet_NaN();CHECK(!p::valid_metadata(bad.metadata,*bad.artifact,limits));
 bad=source;bad.metadata.water_origin_meters[0]=std::numeric_limits<double>::infinity();CHECK(!p::valid_metadata(bad.metadata,*bad.artifact,limits));
 bad=source;bad.metadata.water_triangles[0].ground[0]=std::numeric_limits<std::uint32_t>::max();CHECK(!p::valid_metadata(bad.metadata,*bad.artifact,limits));
 auto legacy=source.metadata;legacy.water_radius_meters=legacy.water_meters_to_artifact=0;legacy.water_vertices.clear();legacy.water_triangles.clear();
 legacy.water_origin_meters={};
 for(auto& patch:legacy.patches){patch.terrain_vertices=patch.terrain_indices=patch.first_water_vertex=patch.first_water_triangle=patch.water_triangles=0u;}
 const auto old=p::encode(legacy,*source.artifact,limits);CHECK(!old.empty()&&old[4]==std::byte{1});CHECK(p::decode(old,*source.artifact,identity,limits));
 CHECK(!p::resample_water({source.artifact,legacy},*evaluator.current(),limits));
 report<<"water_vertices="<<metadata->water_vertices.size()<<" water_triangles="<<metadata->water_triangles.size()<<" metadata_bytes="<<encoded.size()<<" passed=1\n";
 return 0;
}
