// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <utility>
#include <vector>
import planet.terrain;
import water.planetary;
import water.wave;
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
 using epochengine::water::wave::spherical_surface_color;
 using epochengine::water::wave::spherical_specular_scale;
 using epochengine::water::wave::spherical_shoaled_normal;
 constexpr std::array crest{0.6f,0.8f,0.0f};
 constexpr V up{0.0,1.0,0.0};
 // View the same known swell edge-on and from above. At the northern pole
 // the XZ field dominates. Its crest, shoulder and trough lie 4 m apart.
 std::vector<epochengine::water::wave::Sample> field(32*32);
 for(unsigned z=0;z<32;++z)for(unsigned x=0;x<32;++x)
     field[z*32+x].height=static_cast<float>(std::cos(x*0.39269908169872415));
 const auto tile=epochengine::water::wave::Tile::create(32,32.0,0.0,std::move(field),1024*1024);
 CHECK(tile);
 constexpr double radius=6371000;
 const auto elevation=[&](double x){
     const V ray{x/radius,std::sqrt(1.0-(x/radius)*(x/radius)),0.0};
     const auto s=epochengine::water::wave::spherical_sample(*tile,ray,radius,0.0);
     return s?std::hypot(s->position_meters[0],s->position_meters[1],s->position_meters[2])-radius:NAN;
 };
 CHECK(elevation(0.0)>1.19&&elevation(4.0)<-0.19&&elevation(8.0)>-0.81);
 CHECK(std::abs(elevation(0.0)-elevation(8.0)-2.0)<1e-5);
 double mean_elevation{};
 for(unsigned i=0;i<32;++i)mean_elevation+=elevation(i)/32.0;
 CHECK(std::abs(mean_elevation)<1e-5);
 const auto shoaled=spherical_shoaled_normal(crest,up,2.0,1.2,1.0f);
 const auto transition=spherical_shoaled_normal(crest,up,2.0,1.2,0.35f);
 CHECK(shoaled[0]>transition[0]&&transition[0]>crest[0]&&shoaled[1]>0.0f);
 CHECK(std::abs(std::hypot(shoaled[0],shoaled[1],shoaled[2])-1.0f)<1e-6f);
 CHECK(spherical_shoaled_normal(crest,up,14.0,1.2,1.0f)==crest);
 CHECK(spherical_shoaled_normal(crest,up,2.0,-1.2,1.0f)==crest);
 CHECK(spherical_shoaled_normal(crest,up,2.0,1.2,0.0f)==crest);
 CHECK(spherical_shoaled_normal(crest,up,2.0,NAN,1.0f)==crest);
 CHECK(spherical_specular_scale(1.0,128.0)==1.0f);
 CHECK(spherical_specular_scale(8.0,128.0)==1.0f);
 CHECK(spherical_specular_scale(20.0,128.0)==0.5f);
 CHECK(spherical_specular_scale(28.0,128.0)<spherical_specular_scale(20.0,128.0));
 CHECK(spherical_specular_scale(32.0,128.0)==0.0f);
 CHECK(spherical_specular_scale(50'000.0,128.0)==0.0f);
 CHECK(spherical_specular_scale(NAN,128.0)==0.0f);
 const auto deep=spherical_surface_color(100.0,0.0f,3.0,128.0);
 const auto shallow=spherical_surface_color(2.0,0.0f,3.0,128.0);
 using epochengine::water::wave::spherical_shore_tint_strength;
 CHECK(spherical_shore_tint_strength(2.0)==1.0f);
 CHECK(spherical_shore_tint_strength(27'500.0)==0.5f);
 CHECK(spherical_surface_color(2.0,0.0f,3.0,128.0,false,
    spherical_shore_tint_strength(126'800.0))==deep);
 CHECK(shallow[1]>2.0f*deep[1]&&shallow[2]>deep[2]);
 CHECK(spherical_surface_color(2.0,0.95f,3.0,128.0)[0]>shallow[0]);
 const auto shallow_crest=spherical_surface_color(2.0,0.95f,3.0,128.0);
 const auto deep_crest=spherical_surface_color(100.0,0.95f,3.0,128.0);
 CHECK(shallow_crest[0]-shallow[0]>deep_crest[0]-deep[0]+0.04f);
 CHECK(spherical_surface_color(2.0,0.95f,100.0,128.0)==shallow);
 const auto quiet_shore=spherical_surface_color(0.65,0.0f,1.49,128.0);
 const auto steep=epochengine::water::wave::spherical_face_incline_squared(crest,up);
 CHECK(steep>0.35&&steep<0.37);
 CHECK(spherical_surface_color(0.65,0.0f,1.49,128.0,false,1.0f,0.6,{},steep)[0]>quiet_shore[0]+0.30f);
 CHECK(spherical_surface_color(0.65,0.0f,1.49,128.0,false,1.0f,0.6,{},0.0)==quiet_shore);
 CHECK(spherical_surface_color(-1.5,0.0f,1.49,128.0,false,1.0f,0.7,{},steep)
    ==spherical_surface_color(-1.5,0.0f,1.49,128.0));
 CHECK(spherical_surface_color(0.65,0.0f,1.49,128.0,false,1.0f,-0.6)==quiet_shore);
 CHECK(spherical_surface_color(0.65,0.0f,1.49,128.0,false,1.0f,1.0)==quiet_shore);
 CHECK(spherical_surface_color(0.65,0.45f,1.49,128.0,false,1.0f,1.0)
    ==spherical_surface_color(0.65,0.45f,1.49,128.0));
 CHECK(spherical_surface_color(0.65,0.0f,2.15,128.0,false,1.0f,0.6,{},steep)[0]>quiet_shore[0]);
 CHECK(spherical_surface_color(0.65,0.0f,3.0,128.0,false,1.0f,0.6,{},steep)==quiet_shore);
 const auto quiet_beach=spherical_surface_color(-0.4,0.0f,1.49,128.0);
 const auto runup=spherical_surface_color(-0.4,0.0f,1.49,128.0,false,1.0f,0.7);
 CHECK(runup[0]>quiet_beach[0]+0.30f);
 CHECK(spherical_surface_color(-0.4,0.0f,1.49,128.0,false,1.0f,-0.5)==quiet_beach);
 CHECK(spherical_surface_color(-0.4,0.0f,1.49,128.0,false,1.0f,0.7,0.0f)==quiet_beach);
 CHECK(spherical_surface_color(0.65,0.0f,64.0,128.0,false,1.0f,0.6)==quiet_shore);
 CHECK(spherical_surface_color(100.0,0.0f,1.49,128.0,false,1.0f,1.0)==deep);
 CHECK(spherical_surface_color(0.0,0.0f,3.0,128.0,true)[0]>spherical_surface_color(0.0,0.0f,3.0,128.0)[0]);
 CHECK(spherical_surface_color(100.0,0.0f,100.0,128.0)==deep);
 CHECK(spherical_surface_color(2.0,0.0f,100.0,128.0)==shallow);
 CHECK(spherical_surface_color(0.0,1.0f,100.0,128.0,true)
    ==spherical_surface_color(0.0,0.0f,3.0,128.0));
 p::Universe universe{1337};const auto body=universe.body(0);CHECK(body.ocean);
 // The Verdant join resolves sun glints on both sides, but its coarser
 // lattice cannot represent the shoreline breaker.
 const p::TileKey verdant_fine{8193u,262082u,0u,0u,18u},verdant_coarse{4097u,131041u,0u,0u,17u};
 const std::array cut{verdant_fine,verdant_coarse};
 const auto fine_size=w::lattice_spacing_meters(body,verdant_fine,16u);
 const auto coarse_size=w::lattice_spacing_meters(body,verdant_coarse,16u);
 CHECK(spherical_specular_scale(fine_size,128.0)==1.0f&&spherical_specular_scale(coarse_size,128.0)==1.0f);
 const auto breaker_edges=w::breaker_edges_for_cut(body,verdant_fine,16u,cut);
 CHECK(breaker_edges==std::array<float,4>{1,0,1,1});
 using epochengine::water::wave::spherical_water_edge_weight;
 CHECK(spherical_water_edge_weight(breaker_edges,1.0f,16u,8u,16u)==0.0f);
 CHECK(spherical_water_edge_weight(breaker_edges,1.0f,14u,8u,16u)>
       spherical_water_edge_weight(breaker_edges,1.0f,15u,8u,16u));
 CHECK(spherical_water_edge_weight(breaker_edges,1.0f,13u,8u,16u)==1.0f);
 CHECK(spherical_surface_color(0.65,0.0f,fine_size,128.0,false,1.0f,0.6,breaker_edges[1],steep)
    ==spherical_surface_color(0.65,0.0f,coarse_size,128.0,false,1.0f,0.6,{},steep));
 w::Surface water;std::uint64_t surface_crossings{},skirt_crossings{},shared_crossings{},old_disagreements{};double maximum_error{};
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
 m::SphericalWaterRange appearance;
 CHECK(water.apply_patch(vertices,indices,0,0,source,body,key,16,scale,{},32u*1024u*1024u,&deltas,&appearance));
 CHECK(vertices.size()>nv&&deltas.size()==vertices.size());
 CHECK(appearance.first_vertex==353u&&appearance.vertex_count==vertices.size()-353u&&appearance.specular_scale==0.0f);
 for(std::size_t i=0;i<353u;++i){CHECK(vertices[i].position==terrain_before[i].position);
 CHECK(vertices[i].color==terrain_before[i].color);CHECK(deltas[i]==deltas_before[i]);}
 // Match the actual provider's emitted boundary vertices to the original
 // water edges. Both incident faces must generate the same vertex, even
 // where projecting the displaced endpoints onto each face used to disagree.
 struct EdgeCrossing {std::size_t vertex{};std::optional<double> old_fraction{};};
 std::map<std::pair<std::uint32_t,std::uint32_t>,EdgeCrossing> crossings;
 std::size_t next_boundary=nv;
 for(std::size_t tri=1920u;tri<1920u+6u*16u*16u;tri+=3u){
  const std::array ids{source.indices[tri],source.indices[tri+1u],source.indices[tri+2u]};
  const auto ground=pos(vertices[ids[0]-353u]);
  const auto normal=cross(sub(pos(vertices[ids[1]-353u]),ground),sub(pos(vertices[ids[2]-353u]),ground));
  for(unsigned edge=0;edge<3u;++edge){
   const auto a=(std::min)(ids[edge],ids[(edge+1u)%3u]),b=(std::max)(ids[edge],ids[(edge+1u)%3u]);
   const auto da=epochengine::water::wave::spherical_shore_depth(vertices[a-353u].position,vertices[a].position,{});
   const auto db=epochengine::water::wave::spherical_shore_depth(vertices[b-353u].position,vertices[b].position,{});
   CHECK(da&&db);
   if((*da>0.0)==(*db>0.0))continue;
   CHECK(next_boundary<vertices.size());
   const auto old_a=dot(normal,sub(pos(vertices[a]),ground)),old_b=dot(normal,sub(pos(vertices[b]),ground));
   const auto old_fraction=(old_a>0.0)!=(old_b>0.0)?std::optional{old_a/(old_a-old_b)}:std::nullopt;
   const auto [previous,inserted]=crossings.try_emplace(std::pair{a,b},EdgeCrossing{next_boundary,old_fraction});
   if(!inserted){
    const auto other=previous->second.vertex;
    CHECK(vertices[other].position==vertices[next_boundary].position);
    CHECK(vertices[other].color==vertices[next_boundary].color);
    CHECK(vertices[other].normal==vertices[next_boundary].normal);
    CHECK(deltas[other]==deltas[next_boundary]);
    const auto before=previous->second.old_fraction;
    if(bool(before)!=bool(old_fraction)||(before&&old_fraction&&std::abs(*before-*old_fraction)>1e-9))
     ++old_disagreements;
    ++shared_crossings;
   }
   ++next_boundary;
  }
 }
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
    if(!std::ranges::all_of(std::array{0u,1u,2u},[&](unsigned channel){
     return std::abs(double(vertices[i].color[channel])
      -std::lerp(double(vertices[a].color[channel]),double(vertices[b].color[channel]),std::clamp(t,0.0,1.0)))<0.005;
    }))continue;
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
 CHECK(surface_crossings>100u&&skirt_crossings>10u&&shared_crossings>0u&&old_disagreements>0u);
 report<<"shared_crossings="<<shared_crossings<<" old_facet_disagreements="<<old_disagreements<<'\n';

 // Saved Verdant coast, level 17 on face 0. The old clip vertex added
 // a pale tip to a nearly 5 m triangle even though both endpoints were teal.
 const p::TileKey wedgeTile{4096u,131041u,0u,0u,17u};
 const auto wedgeSource=p::build_tile(body,wedgeTile,{16,false,true});
 const auto wedgeOffset=-wedgeSource.anchor;
 std::vector<m::Vertex> wedgeVertices;
 for(const auto& vertex:wedgeSource.vertices){
  const auto q=wedgeSource.anchor+p::DVec3{vertex.position}+wedgeOffset;
  wedgeVertices.push_back({{float(q.x),float(q.y),float(q.z)},
   {vertex.normal.x,vertex.normal.y,vertex.normal.z},{},
   {vertex.color[0],vertex.color[1],vertex.color[2]}});
 }
 auto wedgeIndices=wedgeSource.indices;
 // At this phase the shaped swell cuts the first 4% of the saved edge.
 CHECK(water.evaluate(1337,14.0/60.0));
 CHECK(water.apply_patch(wedgeVertices,wedgeIndices,0,0,wedgeSource,body,wedgeTile,16,
  1.0,wedgeOffset,32u*1024u*1024u));
 constexpr std::uint32_t dry=395u,wet=396u,opposite=413u;
 const auto dryPoint=pos(wedgeVertices[dry]);
 const auto direction=sub(pos(wedgeVertices[wet]),dryPoint);
 CHECK(dot(direction,direction)>0.0);
 bool foundWedge{};
 for(std::size_t index=0;index<wedgeIndices.size();index+=3u){
  const std::array ids{wedgeIndices[index],wedgeIndices[index+1u],wedgeIndices[index+2u]};
  if(std::ranges::find(ids,wet)==ids.end()||std::ranges::find(ids,opposite)==ids.end())continue;
  const auto clip=*std::ranges::find_if(ids,[](auto id){return id!=wet&&id!=opposite;});
  if(clip<wedgeSource.vertices.size())continue;
  auto displacement=sub(pos(wedgeVertices[clip]),dryPoint);
  const double fraction=dot(displacement,direction)/dot(direction,direction);
  for(unsigned axis=0;axis<3u;++axis)displacement[axis]-=fraction*direction[axis];
  if(dot(displacement,displacement)>1e-9)continue;
  CHECK(fraction>0.02&&fraction<0.06);
  const auto span=sub(pos(wedgeVertices[clip]),pos(wedgeVertices[opposite]));
  CHECK(dot(span,span)>4.5*4.5);
  for(unsigned channel=0;channel<3u;++channel){
   CHECK(std::abs(wedgeVertices[dry].color[channel]-wedgeVertices[wet].color[channel])<1e-5f);
   const auto expected=std::lerp(double(wedgeVertices[dry].color[channel]),
    double(wedgeVertices[wet].color[channel]),fraction);
   CHECK(std::abs(double(wedgeVertices[clip].color[channel])-expected)<1e-5);
  }
  report<<"fine_shore_wedge=preserved_affine_color fraction="<<fraction
   <<" span_meters="<<std::sqrt(dot(span,span))<<'\n';
  foundWedge=true;
  break;
 }
 CHECK(foundWedge);

 // Find a real shallow coastal point at the shared corner of adjacent L12
 // and L13 tiles. This spans the old 96-m tint cutoff while leaving foam off.
 constexpr std::uint32_t tile_count=1u<<12u;
 p::TileKey coarse{},fine{};
 p::DVec3 coast{};
 double coast_depth{};
 for(std::uint8_t face=0;face<6u&&coast_depth==0.0;++face)
  for(std::uint32_t y=23u;y<tile_count&&coast_depth==0.0;y+=53u)
   for(std::uint32_t x=22u;x+1u<tile_count;x+=53u){
    const p::TileKey candidate{x,y,0,face,12};
    const auto direction=p::tile_direction(candidate,1.0,0.0);
    const double depth=-p::sample_height(body,direction);
    if(depth<4.0||depth>16.0)continue;
    coarse=candidate;fine={2u*(x+1u),2u*y,0,face,13};coast=direction;coast_depth=depth;
    break;
   }
 CHECK(coast_depth>0.0);
 const auto shared=p::tile_direction(fine,0.0,0.0);
 CHECK(p::length(coast-shared)<1e-12);
 const auto offset=-coast*body.radius;
 std::array<m::Vertex,2> edge{};
 std::array<double,2> spacing{};
 for(unsigned pass=0;pass<edge.size();++pass){
  const auto key=pass==0u?coarse:fine;
  const auto source=p::build_tile(body,key,{16,false,true});
  std::vector<m::Vertex> vertices;
  for(const auto& v:source.vertices){const auto q=source.anchor+p::DVec3{v.position}+offset;
   vertices.push_back({{float(q.x),float(q.y),float(q.z)},{v.normal.x,v.normal.y,v.normal.z},{},
    {v.color[0],v.color[1],v.color[2]}});}
  auto indices=source.indices;
  CHECK(water.apply_patch(vertices,indices,0,0,source,body,key,16,1.0,offset,32u*1024u*1024u));
  edge[pass]=vertices[353u+(pass==0u?16u:0u)];
  spacing[pass]=p::length(p::tile_direction(key,1.0/16.0,0)-p::tile_direction(key,0,0))*body.radius;
 }
 CHECK(spacing[0]>96.0&&spacing[1]<96.0);
 CHECK(dot(sub(pos(edge[0]),pos(edge[1])),sub(pos(edge[0]),pos(edge[1])))<1e-8);
 CHECK(edge[0].color[1]>deep[1]+.02f);
 for(unsigned channel=0;channel<3u;++channel)
  CHECK(std::abs(edge[0].color[channel]-edge[1].color[channel])<1e-5f);
 report<<surface_crossings<<' '<<skirt_crossings<<' '<<maximum_error<<'\n';
 report<<"coastal_depth="<<coast_depth<<" coarse_cell="<<spacing[0]<<" fine_cell="<<spacing[1]
  <<" shared_green="<<edge[0].color[1]<<'\n';return 0;
}
