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
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
import project.planetary_snapshot;
import asset.mesh_artifact;
import water.fft;
import water.planetary;
import planet.terrain;
namespace p=epochengine::project_planetary_snapshot;
namespace m=epochengine::asset::mesh;
namespace fft=epochengine::water::fft;
namespace core=epoch::extensions::planetary_terrain::core;
namespace ocean=epoch::extensions::planetary_terrain::water;
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
bool same(const m::Vertex& a,const m::Vertex& b)
{return a.position==b.position&&a.normal==b.normal&&a.uv==b.uv&&a.color==b.color;}

namespace {
constexpr std::uint32_t shore_cells = 16;
constexpr core::TileKey shore_parent{4096, 131041, 0, 0, 17};
constexpr core::TileKey shore_neighbor{4095, 131041, 0, 0, 17};
constexpr double shore_seconds = 32.06666666666613;
constexpr p::Limits shore_limits{{32768, 131072, 4u*1024u*1024u}};

void split_leaf(std::vector<core::TileKey>& leaves, core::TileKey key)
{
 for(const auto neighbor:core::neighbors(key))
 {
  const auto covering=std::ranges::find_if(leaves,[neighbor](auto entry){return core::contains(entry,neighbor);});
  if(covering!=leaves.end()&&covering->level<key.level)split_leaf(leaves,*covering);
 }
 std::erase(leaves,key);
 const auto children=core::children(key);
 leaves.insert(leaves.end(),children.begin(),children.end());
}

std::vector<core::TileKey> shoreline_leaves()
{
 std::vector<core::TileKey> leaves;
 for(std::uint8_t face=0;face<6;++face)leaves.push_back({0,0,0,face,0});
 for(std::uint8_t level=0;level<=shore_parent.level;++level)
 {
  const auto gap=shore_parent.level-level;
  split_leaf(leaves,{shore_parent.x>>gap,shore_parent.y>>gap,0,0,level});
 }
 return leaves;
}

p::TileKey snapshot_key(core::TileKey key)
{return {key.x,key.y,key.body,key.face,key.level};}

std::shared_ptr<p::Snapshot> shoreline_cut(const core::Body& body,const ocean::Surface& surface,
 const core::DVec3 origin,std::span<const core::TileKey> leaves,std::uint64_t request)
{
 auto snapshot=std::make_shared<p::Snapshot>();
 auto& metadata=snapshot->metadata;
 metadata.identity={{},"sha256:"+std::string(64,'a'),std::string(64,'b'),request};
 snapshot->metric_origin_meters=std::array{origin.x,origin.y,origin.z};
 snapshot->water_sample_seconds=shore_seconds;
 const auto children=core::children(shore_parent);
 std::vector<core::TileKey> keys(leaves.begin(),leaves.end());
 for(auto key:leaves)while(key.level)
 {
  key=core::parent(key);
  if(std::ranges::find(keys,key)==keys.end())keys.push_back(key);
 }
 std::vector<m::Vertex> vertices;
 std::vector<std::uint32_t> indices;
 auto appearance=std::make_shared<m::SphericalWaterAppearance>();
 for(const auto key:keys)
 {
  p::Patch patch{.key=snapshot_key(key),.first_vertex=static_cast<std::uint32_t>(vertices.size()),
      .first_index=static_cast<std::uint32_t>(indices.size()),
      .target=std::ranges::find(leaves,key)!=leaves.end()};
  const auto neighbors=core::neighbors(key);
  for(std::size_t i=0;i<neighbors.size();++i)patch.neighbors[i]=snapshot_key(neighbors[i]);
  const bool wet=key==shore_parent||key==shore_neighbor||std::ranges::find(children,key)!=children.end();
  if(wet)
  {
   const auto source=core::build_tile(body,key,{static_cast<int>(shore_cells),false,false});
   auto local_indices=source.indices;
   std::vector<m::Vertex> local_vertices;
   std::vector<std::array<float,3>> local_deltas;
   for(const auto& v:source.vertices)
   {
    const auto q=source.anchor+core::DVec3{v.position}-origin;
    local_vertices.push_back({{float(q.x),float(q.y),float(q.z)},{v.normal.x,v.normal.y,v.normal.z},{},
        {v.color[0],v.color[1],v.color[2]}});
    local_deltas.push_back({v.parent_delta.x,v.parent_delta.y,v.parent_delta.z});
   }
   m::SphericalWaterRange range;
   const auto edges=ocean::breaker_edges_for_cut(body,key,shore_cells,leaves);
   if(!surface.apply_patch(local_vertices,local_indices,0,0,source,body,key,shore_cells,1.0,
       core::DVec3{}-origin,shore_limits.artifact.maximum_bytes,&local_deltas,&range,1.0f,&edges)
       ||!range.vertex_count||!ocean::append_recipe(metadata,patch,source,body,key,shore_cells,1.0))return {};
   range.first_vertex+=patch.first_vertex;
   appearance->ranges.push_back(range);
   vertices.insert(vertices.end(),local_vertices.begin(),local_vertices.end());
   metadata.parent_delta.insert(metadata.parent_delta.end(),local_deltas.begin(),local_deltas.end());
   for(const auto index:local_indices)indices.push_back(patch.first_vertex+index);
  }
  else
  {
   // Other branches close the cube cut. Only the shore quartet carries water.
   for(const auto uv:std::array{std::array{0.0,0.0},std::array{1.0,0.0},
                          std::array{0.0,1.0},std::array{1.0,1.0}})
   {
    const auto q=core::tile_direction(key,uv[0],uv[1])*body.radius-origin;
    vertices.push_back({{float(q.x),float(q.y),float(q.z)}});
    metadata.parent_delta.push_back({});
   }
   for(const auto index:std::array{0u,1u,3u,0u,3u,2u})indices.push_back(patch.first_vertex+index);
  }
  patch.vertex_count=static_cast<std::uint32_t>(vertices.size())-patch.first_vertex;
  patch.index_count=static_cast<std::uint32_t>(indices.size())-patch.first_index;
  metadata.patches.push_back(patch);
 }
 metadata.water_origin_meters=*snapshot->metric_origin_meters;
 snapshot->artifact=m::build_artifact(vertices,indices,shore_limits.artifact).artifact;
 if(!snapshot->artifact)return {};
 metadata.identity.artifact_key=appearance->artifact_key=snapshot->artifact->key();
 metadata.water_appearance=appearance;
 return p::valid_metadata(metadata,*snapshot->artifact,shore_limits)?snapshot:nullptr;
}

struct WetRaster {
 static constexpr std::size_t size=384;
 struct Sample {std::array<float,3> color{},normal{};bool water{};};
 std::vector<Sample> samples=std::vector<Sample>(size*size);
};

WetRaster wet_footprint(const m::Artifact& mesh,const m::SphericalWaterAppearance& appearance,
 const core::DVec3 east,const core::DVec3 north)
{
 constexpr double half_width=50.0,step=2.0*half_width/WetRaster::size;
 WetRaster result;
 const auto vertices=mesh.vertices();
 const auto indices=mesh.indices();
 struct Point {double x,y;};
 for(std::size_t i=0;i<indices.size();i+=3)
 {
  const auto first=indices[i];
  if(std::ranges::none_of(appearance.ranges,[first](const auto& range){
      return first>=range.first_vertex&&first-range.first_vertex<range.vertex_count;}))continue;
  std::array<Point,3> points{};
  for(std::size_t corner=0;corner<points.size();++corner)
  {
   const auto xyz=vertices[indices[i+corner]].position;
   const core::DVec3 q{xyz[0],xyz[1],xyz[2]};
   points[corner]={core::dot(q,east),core::dot(q,north)};
  }
  const auto [a,b,c]=points;
  const double det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
  if(std::abs(det)<1e-10)continue;
  const auto first_slot=[](double q){return (std::max)(0,int(std::ceil((q+half_width)/step-0.5)));};
  const auto last_slot=[](double q){return (std::min)(int(WetRaster::size)-1,int(std::floor((q+half_width)/step-0.5)));};
  for(int row=first_slot((std::min)({a.y,b.y,c.y}));row<=last_slot((std::max)({a.y,b.y,c.y}));++row)
   for(int col=first_slot((std::min)({a.x,b.x,c.x}));col<=last_slot((std::max)({a.x,b.x,c.x}));++col)
   {
    auto& sample=result.samples[std::size_t(row)*WetRaster::size+std::size_t(col)];
    if(sample.water)continue;
    const double x=(col+0.5)*step-half_width,y=(row+0.5)*step-half_width;
    const double wa=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det;
    const double wb=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det,wc=1.0-wa-wb;
    if((std::min)({wa,wb,wc})<-1e-10)continue;
    sample.water=true;
    for(std::size_t axis=0;axis<3;++axis)
    {
     sample.color[axis]=float(wa*vertices[indices[i]].color[axis]+wb*vertices[indices[i+1]].color[axis]+wc*vertices[indices[i+2]].color[axis]);
     sample.normal[axis]=float(wa*vertices[indices[i]].normal[axis]+wb*vertices[indices[i+1]].normal[axis]+wc*vertices[indices[i+2]].normal[axis]);
    }
   }
 }
 return result;
}

struct WetDifference {double area{},color{},normal{};std::size_t shared{};};

WetDifference wet_difference(const WetRaster& a,const WetRaster& b)
{
 constexpr double step=100.0/WetRaster::size;
 std::size_t changed{};
 WetDifference result;
 for(std::size_t i=0;i<a.samples.size();++i)
 {
  const auto& x=a.samples[i];const auto& y=b.samples[i];
  if(x.water!=y.water)++changed;
  if(!x.water||!y.water)continue;
  ++result.shared;
  for(std::size_t axis=0;axis<3;++axis)
  {
   result.color+=std::abs(double(x.color[axis])-y.color[axis]);
   result.normal+=std::abs(double(x.normal[axis])-y.normal[axis]);
  }
 }
 result.area=changed*step*step;
 const auto divisor=(std::max)(std::size_t{1},3u*result.shared);
 result.color/=divisor;result.normal/=divisor;
 return result;
}

bool matches_position(const m::Artifact& mesh,const m::SphericalWaterAppearance& appearance,
 const std::array<float,3>& expected,bool water)
{
 for(std::size_t i=0;i<mesh.vertices().size();++i)
 {
  const auto& p=mesh.vertices()[i].position;
  if(std::hypot(double(p[0])-expected[0],double(p[1])-expected[1],double(p[2])-expected[2])>2.0e-5)continue;
  const bool water_vertex=std::ranges::any_of(appearance.ranges,[i](const auto& range){
      return i>=range.first_vertex&&i-range.first_vertex<range.vertex_count;});
  if(water_vertex==water)return true;
 }
 return false;
}

bool verifies_mixed_boundary(const p::Snapshot& child,const m::Artifact& intermediate,
 const m::SphericalWaterAppearance& intermediate_appearance,const m::Artifact& final,
 const m::SphericalWaterAppearance& final_appearance,core::TileKey key,std::ostream& report)
{
 const auto patch=std::ranges::find_if(child.metadata.patches,[key](const auto& item){
     return item.key==snapshot_key(key);});
 if(patch==child.metadata.patches.end()||!patch->terrain_vertices)return false;
 for(const bool water:{false,true})
 {
  const auto layer=patch->first_vertex+(water?patch->terrain_vertices:0u);
  auto vertex=layer;
  double displacement{};
  for(std::uint32_t row=1;row<shore_cells;++row)
  {
   const auto id=layer+row*(shore_cells+1u);
   const auto& d=child.metadata.parent_delta[id];
   const auto length=std::hypot(d[0],d[1],d[2]);
   if(length>displacement){vertex=id;displacement=length;}
  }
  const auto& fine=child.artifact->vertices()[vertex].position;
  const auto& d=child.metadata.parent_delta[vertex];
  const std::array halfway{fine[0]+0.5f*d[0],fine[1]+0.5f*d[1],fine[2]+0.5f*d[2]};
  report<<(water?"mixed_water_full_displacement_m=":"mixed_ground_full_displacement_m=")<<displacement<<'\n';
  if(displacement<=1.0e-4||!matches_position(intermediate,intermediate_appearance,halfway,water)
      ||!matches_position(final,final_appearance,fine,water)
      ||matches_position(intermediate,intermediate_appearance,fine,water))return false;
 }
 return true;
}

int wet_transition(const std::filesystem::path& output)
{
 std::ofstream report{output};
 report<<std::setprecision(17);
 const auto body=core::Universe{1337}.body(0);
 const auto n=core::tile_direction(shore_parent,0.5,0.5);
 const auto origin=n*body.radius;
 const auto east=core::normalized(core::cross(core::DVec3{0.0,1.0,0.0},n));
 const auto north=core::cross(n,east);
 auto fine=shoreline_leaves();
 auto coarse=fine;
 const auto children=core::children(shore_parent);
 CHECK(std::erase_if(coarse,[&](auto key){return std::ranges::find(children,key)!=children.end();})==children.size());
 coarse.push_back(shore_parent);
 ocean::Surface surface;
 CHECK(surface.evaluate(1337,shore_seconds)&&surface.tile()->seconds()==shore_seconds);
 const auto parent=shoreline_cut(body,surface,origin,coarse,1);
 const auto child=shoreline_cut(body,surface,origin,fine,2);
 CHECK(parent&&child);

 p::Transition transition{shore_limits};
 CHECK(transition.admit(parent,{},p::InitialCut::selected_patches));
 std::shared_ptr<const m::SphericalWaterAppearance> parent_appearance;
 const auto parent_mesh=transition.materialize_wet(shore_cells,shore_seconds,{},&parent_appearance);
 CHECK(parent_mesh&&parent_appearance&&parent_appearance->artifact_key==parent_mesh->key());
 const auto parent_mask=wet_footprint(*parent_mesh,*parent_appearance,east,north);
 CHECK(transition.admit(child)&&transition.advance_wet(0.025));
 for(const auto key:children)
 {
  const auto draw=std::ranges::find_if(transition.draw(),[&](const auto& item){
      return item.snapshot->metadata.patches[item.patch].key==snapshot_key(key);});
  CHECK(draw!=transition.draw().end()&&draw->morph==1.0f);
 }
 std::shared_ptr<const m::SphericalWaterAppearance> legacy_endpoint_appearance;
 const auto legacy_endpoint=transition.materialize({},&legacy_endpoint_appearance);
 CHECK(legacy_endpoint&&legacy_endpoint_appearance);
 const auto uncorrected=wet_difference(parent_mask,
     wet_footprint(*legacy_endpoint,*legacy_endpoint_appearance,east,north));
 report<<"legacy_endpoint_xor_m2="<<uncorrected.area<<" color_mae="<<uncorrected.color<<'\n';
 CHECK(uncorrected.area>0.5&&uncorrected.color>0.02);

 std::shared_ptr<const m::SphericalWaterAppearance> endpoint_appearance;
 const auto endpoint=transition.materialize_wet(shore_cells,shore_seconds,{},&endpoint_appearance);
 CHECK(endpoint&&endpoint_appearance&&endpoint_appearance->artifact_key==endpoint->key());
 const auto difference=wet_difference(parent_mask,wet_footprint(*endpoint,*endpoint_appearance,east,north));
 report<<"wet_transition_time="<<shore_seconds<<" parent_endpoint_xor_m2="<<difference.area
   <<" color_mae="<<difference.color<<" normal_mae="<<difference.normal<<" shared="<<difference.shared<<'\n';
 CHECK(difference.shared>1024&&difference.area<=0.5&&difference.color<=0.0015&&difference.normal<=0.003);

 std::stop_source cancelled;cancelled.request_stop();
 const auto retained=endpoint_appearance;
 CHECK(!transition.materialize_wet(shore_cells,shore_seconds,cancelled.get_token(),&endpoint_appearance));
 CHECK(endpoint_appearance==retained);
 p::WetTransitionFailure refusal;
 CHECK(!transition.materialize_wet(shore_cells,shore_seconds+0.25,{},nullptr,0u,&refusal));
 CHECK(refusal.reason=="retained_clock_mismatch"&&refusal.patch);
 CHECK(!transition.materialize_wet(shore_cells,shore_seconds,{},nullptr,shore_limits.maximum_retained_bytes,&refusal));
 CHECK(refusal.reason=="retained_budget"&&!refusal.patch);
 CHECK(!transition.advance_wet(0.05,false,cancelled.get_token(),&refusal));
 CHECK(refusal.reason=="cancelled"&&!refusal.patch);
 CHECK(!transition.advance_wet(-0.05,false,{},&refusal));
 CHECK(refusal.reason=="invalid_input"&&!refusal.patch);
 CHECK(transition.advance_wet(0.05,true,{},&refusal)&&refusal.reason.empty());
 const auto paused=transition.materialize_wet(shore_cells,shore_seconds);
 CHECK(paused&&paused->key()==endpoint->key());
 const auto left_edge=children[0];
 CHECK(core::parent(core::neighbors(left_edge)[0])==shore_neighbor);
 const auto neighbor=std::ranges::find_if(transition.draw(),[](const auto& item){
     return item.snapshot->metadata.patches[item.patch].key==snapshot_key(shore_neighbor);});
 CHECK(neighbor!=transition.draw().end()&&neighbor->morph==0.0f
     &&neighbor->snapshot->metadata.patches[neighbor->patch].terrain_vertices>0u);
 CHECK(transition.advance_wet(0.1));
 const auto edge=std::ranges::find_if(transition.draw(),[left_edge](const auto& item){
     return item.snapshot->metadata.patches[item.patch].key==snapshot_key(left_edge);});
 CHECK(edge!=transition.draw().end()&&edge->morph==0.5f);
 std::shared_ptr<const m::SphericalWaterAppearance> intermediate_appearance;
 const auto intermediate=transition.materialize_wet(shore_cells,shore_seconds,{},&intermediate_appearance);
 CHECK(intermediate&&intermediate_appearance);
 for(unsigned i=0;i<12&&!transition.settled();++i)CHECK(transition.advance_wet(0.05));
 CHECK(transition.settled());

 p::Transition selected{shore_limits};
 CHECK(selected.admit(child,{},p::InitialCut::selected_patches));
 std::shared_ptr<const m::SphericalWaterAppearance> expected_appearance,settled_appearance;
 const auto expected=selected.materialize({},&expected_appearance);
 const auto settled=transition.materialize_wet(shore_cells,shore_seconds,{},&settled_appearance);
 CHECK(expected&&settled&&expected_appearance&&settled_appearance);
 const auto agreement=wet_difference(wet_footprint(*expected,*expected_appearance,east,north),
     wet_footprint(*settled,*settled_appearance,east,north));
 report<<"settled_xor_m2="<<agreement.area<<" color_mae="<<agreement.color<<" normal_mae="<<agreement.normal<<'\n';
 CHECK(agreement.shared>1024&&agreement.area<=0.05&&agreement.color<=0.0001&&agreement.normal<=0.0001);
 CHECK(verifies_mixed_boundary(*child,*intermediate,*intermediate_appearance,*settled,*settled_appearance,left_edge,report));

 const auto transport=p::encode(child->metadata,*child->artifact,shore_limits);
 auto decoded=p::DecodedSnapshot::read(transport,child->artifact,child->metadata.identity,shore_limits,
     {},child->metric_origin_meters);
 CHECK(decoded);
 const auto admitted=std::move(*decoded).materialize_selected({},shore_seconds);
 CHECK(admitted.source&&admitted.source->water_sample_seconds==shore_seconds);
 auto decoded_legacy=p::DecodedSnapshot::read(transport,child->artifact,child->metadata.identity,shore_limits,
     {},child->metric_origin_meters);
 CHECK(decoded_legacy);
 const auto legacy_transport=std::move(*decoded_legacy).materialize_selected();
 CHECK(legacy_transport.source&&!legacy_transport.source->water_sample_seconds);

 auto missing=std::make_shared<p::Snapshot>(*child);
 missing->metadata.water_appearance.reset();
 p::Transition missing_sidecar{shore_limits};
 CHECK(!missing_sidecar.admit(missing,{},p::InitialCut::selected_patches)
     ||!missing_sidecar.materialize_wet(shore_cells,shore_seconds));
 auto corrupt=std::make_shared<p::Snapshot>(*child);
 auto out_of_bounds=std::make_shared<m::SphericalWaterAppearance>(*corrupt->metadata.water_appearance);
 out_of_bounds->ranges.front().vertex_count=std::numeric_limits<std::uint32_t>::max();
 corrupt->metadata.water_appearance=out_of_bounds;
 p::Transition invalid_sidecar{shore_limits};
 CHECK(!invalid_sidecar.admit(corrupt,{},p::InitialCut::selected_patches)
     ||!invalid_sidecar.materialize_wet(shore_cells,shore_seconds));

 auto old=std::make_shared<p::Snapshot>(*child);
 old->water_sample_seconds.reset();
 p::Transition legacy{shore_limits};
 CHECK(legacy.admit(old,{},p::InitialCut::selected_patches));
 CHECK(!legacy.materialize_wet(shore_cells,shore_seconds));
 CHECK(legacy.materialize());
 report<<"wet_transition=passed\n";
 return 0;
}
}

int main(int argc,char** argv)
{
 if(argc==3&&std::string_view{argv[1]}=="--wet-transition")return wet_transition(argv[2]);
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
 // A two-patch coastal sidecar with equal glint weights still requests
 // edge resampling when its admitted water lattices resolve foam differently.
 {
  namespace core=epoch::extensions::planetary_terrain::core;
  const auto body=core::Universe{1337}.body(0);
  const core::TileKey fine{8193u,262082u,0u,0u,18u},coarse{4097u,131041u,0u,0u,17u};
  const auto key=[](core::TileKey tile){return p::TileKey{tile.x,tile.y,tile.body,tile.face,tile.level};};
  p::Snapshot seam;
  seam.metadata.water_radius_meters=body.radius;
  for(const auto tile:{coarse,fine})
  {
   const auto first=static_cast<std::uint32_t>(seam.metadata.water_vertices.size());
   p::Patch patch{.key=key(tile),.first_vertex=first*8u,.vertex_count=16u,
       .target=true,.terrain_vertices=4u,.first_water_vertex=first};
   const auto neighbors=core::neighbors(tile);
   for(unsigned side=0;side<4;++side)patch.neighbors[side]=key(neighbors[side]);
   seam.metadata.patches.push_back(patch);
   for(const double u:{0.0,1.0/16.0})
   {
    const auto direction=core::tile_direction(tile,u,0.0);
    seam.metadata.water_vertices.push_back({{direction.x,direction.y,direction.z}});
   }
  }
  auto ranges=std::make_shared<m::SphericalWaterAppearance>();
  ranges->ranges={{4u,12u,1.0f},{20u,12u,1.0f}};
  seam.metadata.water_appearance=ranges;
  CHECK(p::requires_water_edge_resample(seam));
  seam.metadata.patches.front().target=false;
  CHECK(!p::requires_water_edge_resample(seam));
 }
 const p::Limits limits{{1048576u,6291456u,32u*1024u*1024u},4096u,16u*1024u*1024u,170u*1024u*1024u,66u*1024u*1024u};
 const auto artifact=m::read_artifact_file(folder/"terrain.epoch_mesh",limits.artifact);CHECK(artifact);
 std::ifstream input{folder/"terrain.epoch_planet_patch",std::ios::binary|std::ios::ate};CHECK(input&&input.tellg()>0&&input.tellg()<=limits.maximum_metadata_bytes);
 std::vector<std::byte> encoded(static_cast<std::size_t>(input.tellg()));input.seekg(0);CHECK(input.read(reinterpret_cast<char*>(encoded.data()),static_cast<std::streamsize>(encoded.size())));
 const p::Identity identity{artifact.artifact->key(),"sha256:"+std::string(64u,'a'),std::string(64u,'b'),std::stoull(argv[2])};
 const auto metadata=p::decode(encoded,*artifact.artifact,identity,limits);CHECK(metadata&&!metadata->water_vertices.empty());
 p::Snapshot source{artifact.artifact,*metadata};CHECK(p::encode(*metadata,*artifact.artifact,limits)==encoded);
 const auto appearanceFile=folder/"terrain.water_appearance";
 if(std::filesystem::exists(appearanceFile))
 {
  std::ifstream visual{appearanceFile,std::ios::binary|std::ios::ate};
  CHECK(visual&&visual.tellg()>0&&visual.tellg()<=p::maximum_water_appearance_bytes);
  std::vector<std::byte> bytes(static_cast<std::size_t>(visual.tellg()));visual.seekg(0);
  CHECK(visual.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())));
  source.metadata.water_appearance=p::decode_water_appearance(bytes,*artifact.artifact,source.metadata);
  CHECK(source.metadata.water_appearance&&!source.metadata.water_appearance->ranges.empty());
 }
 for(const auto& patch:metadata->patches)if(patch.target&&patch.key.level==20u)
 {
  CHECK(metadata->water_meters_to_artifact==1.0&&metadata->water_origin_meters!=std::array<double,3>{});
  const auto a=artifact.artifact->vertices()[patch.first_vertex].position,b=artifact.artifact->vertices()[patch.first_vertex+1u].position;
  const auto spacing=std::hypot(double(a[0])-b[0],double(a[1])-b[1],double(a[2])-b[2]);CHECK(spacing>0&&spacing<1.0);
  report<<"fine_level=20 spacing_meters="<<spacing<<" local_metric_frame=1\n";break;
 }
 auto settings=ocean::swell_settings(1337);fft::Evaluator evaluator;
 const bool needs_edge_blend=p::requires_water_edge_resample(source);
 std::shared_ptr<const m::Artifact> zero;
 for(const auto seconds:{0.0,1.25,0.0})
 {
  const auto tile=evaluator.evaluate(settings,seconds,4u*1024u*1024u);CHECK(tile);
  std::shared_ptr<const m::SphericalWaterAppearance> appearance;
  const auto started=std::chrono::steady_clock::now();const auto sampled=p::resample_water(source,*tile.tile,limits,{},&appearance);CHECK(sampled);
  CHECK(bool(appearance)==bool(source.metadata.water_appearance));
  if(appearance)
  {
   CHECK(appearance->artifact_key==sampled->key()&&!appearance->ranges.empty());
   CHECK(std::ranges::any_of(appearance->ranges,[](const auto& range){return range.specular_scale==1.0f;}));
   bool blended{};
   for(const auto& range:appearance->ranges)
   {
    CHECK(range.vertex_count>=3u&&std::size_t(range.first_vertex)+range.vertex_count<=sampled->vertices().size());
    if(!range.vertex_specular)continue;
    blended=true;
    bool reduced{},interior{};
    for(const auto& vertex:sampled->vertices().subspan(range.first_vertex,range.vertex_count))
    {
     CHECK(std::isfinite(vertex.uv[0])&&vertex.uv[0]>=0.0f&&vertex.uv[0]<=range.specular_scale);
     reduced=reduced||vertex.uv[0]<range.specular_scale;
     interior=interior||vertex.uv[0]==range.specular_scale;
    }
    CHECK(reduced&&interior);
   }
   CHECK(!blended||needs_edge_blend);
   report<<"water_edge_blend="<<blended<<'\n';
  }
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
