"""Actual wire line producer: equivalent bytes and one Euler evaluation per mesh."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

class MeshWireRotation(unittest.TestCase):
 def test_actual_large_wire_producer_debug_release(self):
  path=ROOT/'Engine/modules/render/render.preview_grid.ixx';source=path.read_text(encoding='utf-8')
  owned=lambda marker,end=False:definition(source,marker,end).replace('export ','',1)
  types='struct MeshPaintView;\n'+owned('    export struct Vec3',True)+owned('    export [[nodiscard]] constexpr Vec3 visual_rgb(')+'\n'.join(owned(marker,True) for marker in (
   '    export struct Vertex','    export enum class ObjectPreviewPrimitive',
   '    export struct ObjectPreviewGeometryRoute final','    export struct PreciseMeshTransform final','    export struct ObjectMarker'))
  helpers='\n'.join(owned(marker) for marker in (
   '    export [[nodiscard]] constexpr ObjectPreviewGeometryRoute object_preview_geometry_route(',
   '    export [[nodiscard]] constexpr bool object_marker_uses_solid_fill(',
   '    export [[nodiscard]] inline Vec3 add(', '    export [[nodiscard]] inline Vec3 subtract(', '    export [[nodiscard]] inline Vec3 scale('))
  euler=owned('        [[nodiscard]] inline EulerRotation euler_rotation(')
  brace=euler.index('{');euler=euler[:brace+1]+' ++euler_calls;'+euler[brace+1:]
  rotation=owned('        struct EulerRotation final',True)+euler+owned('        [[nodiscard]] inline Vec3 rotate_euler(')+owned('        [[nodiscard]] inline Vec3 mesh_vertex_position(')
  wire=owned('        enum class MarkerLinePass',True)+owned('        [[nodiscard]] inline bool mesh_wire_requested(')+owned('        struct MeshWireEdges final',True)+owned('        [[nodiscard]] inline const MeshWireEdges* find_wire_edges(')+owned('        [[nodiscard]] inline std::vector<MeshWireEdges> build_wire_edges(')
  producer=owned('    inline std::vector<Vertex> detail::build_marker_lines(')
  before=producer.replace('detail::build_marker_lines(', 'detail::build_marker_lines_before(',1)
  mesh_rotation='\n                const auto rotation = detail::euler_rotation(marker.rotationDegrees);\n'
  self.assertEqual(before.count(mesh_rotation),1)
  before=before.replace(mesh_rotation,'\n',1)
  before=before.replace('return detail::mesh_vertex_position(marker,source,rotation);','return add(center, rotate_euler_degrees({source[0]*marker.scale.x,source[1]*marker.scale.y,source[2]*marker.scale.z},marker.rotationDegrees));')
  self.assertNotEqual(before.replace('build_marker_lines_before','build_marker_lines'),producer)
  code=r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
// Immutable mesh-observation port. The tested code is the real line producer,
// Euler helpers, edge indexing and marker routing; no mesh-import claim.
namespace epochengine::asset::mesh{
struct Vertex{std::array<float,3>position;};
struct Artifact{std::vector<Vertex>v;std::vector<std::uint32_t>i;
std::span<const Vertex>vertices()const{return v;}std::span<const std::uint32_t>indices()const{return i;}
std::uint64_t key()const{return 1;}std::array<float,3>minimum()const{return{-1,-1,-1};}std::array<float,3>maximum()const{return{1,1,1};}};}
namespace epochengine::visuals{
struct Rgb{float r,g,b;};constexpr Rgb object_default(){return{.4f,.5f,.6f};}
constexpr Rgb object_selected(){return{.8f,.7f,.1f};}constexpr Rgb object_selected_outline(){return{1,.8f,.2f};}
constexpr float editor_wire_opacity_factor(){return .5f;}}
namespace render_arcade{
struct Node{bool diagnostic_overlay{},sampled_render_surface{};};
inline constexpr Node kScreenSceneNode{false,true},kCabinetBodySceneNode{false,false};}
namespace epochengine::previewgrid{
struct MeshBaseColorBinding;
TYPES
HELPERS
std::uint64_t euler_calls{};
namespace detail{ROTATION WIRE
std::vector<Vertex>build_marker_lines(std::span<const ObjectMarker>,std::span<const MeshWireEdges>,MarkerLinePass pass=MarkerLinePass::combined);
std::vector<Vertex>build_marker_lines_before(std::span<const ObjectMarker>,std::span<const MeshWireEdges>,MarkerLinePass pass=MarkerLinePass::combined);}
ROTATE_PUBLIC
PRODUCER
BEFORE
}
int main(int argc,char**argv){
 CHECK(argc==2);namespace p=epochengine::previewgrid;
 auto mesh=std::make_shared<epochengine::asset::mesh::Artifact>();constexpr unsigned cells=192,stride=cells+1;
 mesh->v.reserve(stride*stride);mesh->i.reserve(cells*cells*6);
 for(unsigned z=0;z<=cells;++z)for(unsigned x=0;x<=cells;++x)
  mesh->v.push_back({{float(x)/cells,float((x*7+z*11)%13)*.01f,float(z)/cells}});
 for(unsigned z=0;z<cells;++z)for(unsigned x=0;x<cells;++x){const auto a=z*stride+x,b=a+1,c=a+stride,d=c+1;mesh->i.insert(mesh->i.end(),{a,c,b,b,c,d});}
 const p::ObjectMarker original{.position={100,40,-700},.scale={1.25f,.75f,4},.rotationDegrees={17.3f,-48,123.5f},.primitive=p::ObjectPreviewPrimitive::IndexedMesh,.selected=true,.mesh=mesh,.wireframe=true};
 std::vector<p::ObjectMarker>markers{original,
  {.position={2,3,4},.primitive=p::ObjectPreviewPrimitive::Cube,.selected=true},
  {.primitive=p::ObjectPreviewPrimitive::Light},
  {.position={1,2,3},.scale={2,2,2},.rotationDegrees={15,45,90},.primitive=p::ObjectPreviewPrimitive::TranslationArrow}};
 auto edges=p::detail::build_wire_edges(markers,{});CHECK(edges.size()==1&&edges[0].edges->size()>100000);
 for(bool wire:{false,true})for(bool selected:{false,true})for(auto rotation:{p::Vec3{},p::Vec3{17.3f,-48,123.5f}}){
  markers[0].wireframe=wire;markers[0].selected=selected;markers[0].rotationDegrees=rotation;
  const auto explicitEdges=p::detail::build_wire_edges(std::span{markers}.first(1),{});
  CHECK(p::object_marker_uses_solid_fill(markers[0])==!wire);
  if(!wire){CHECK(explicitEdges.empty());CHECK(p::detail::build_marker_lines(std::span{markers}.first(1),{}).empty());}
  else CHECK(explicitEdges.size()==1&&!explicitEdges.front().edges->empty());
  const auto before=p::detail::build_marker_lines_before(markers,edges);
  const auto after=p::detail::build_marker_lines(markers,edges);
  CHECK(before.size()==after.size()&&std::memcmp(before.data(),after.data(),before.size()*sizeof(p::Vertex))==0);
 }
 markers[0]=original;
 p::euler_calls=0;const auto old=p::detail::build_marker_lines_before(markers,edges);const auto old_calls=p::euler_calls;
 p::euler_calls=0;const auto current=p::detail::build_marker_lines(markers,edges);const auto new_calls=p::euler_calls;
 CHECK(old_calls>=edges[0].edges->size()*2&&new_calls<=32&&old.size()==current.size());
 using Clock=std::chrono::steady_clock;
 const auto bench=[&](bool former){double total{};std::size_t count{};
  for(unsigned n=0;n<3;++n){const auto start=Clock::now();auto frame=former?p::detail::build_marker_lines_before(markers,edges):p::detail::build_marker_lines(markers,edges);
   total+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();count+=frame.size();}
  return std::pair{total/3.,count};};
 const auto first=bench(true),second=bench(false);CHECK(first.second==second.second);
 const auto empty=p::detail::build_marker_lines({},{});CHECK(empty.empty());
 std::ofstream out(argv[1]);out<<"{\"vertices\":"<<mesh->v.size()<<",\"indices\":"<<mesh->i.size()<<",\"unique_edges\":"<<edges[0].edges->size()
  <<",\"line_vertices\":"<<current.size()<<",\"before_euler_calls\":"<<old_calls<<",\"after_euler_calls\":"<<new_calls
  <<",\"before_mean_ms\":"<<first.first<<",\"after_mean_ms\":"<<second.first<<",\"bit_identical\":true}";
 CHECK(out.good());return 0;
}
'''.replace('TYPES',types).replace('HELPERS',helpers).replace('ROTATION',rotation).replace('WIRE',wire).replace('ROTATE_PUBLIC',owned('    export [[nodiscard]] inline Vec3 rotate_euler_degrees(')).replace('PRODUCER',producer).replace('BEFORE',before)
  proof=ROOT/'Artifacts/Reports/runtime-wire-rotation-oct29'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  cpp=proof/'wire.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,proof,args=lambda config:[proof/(config+'-timing.json')],windows_flags=['/fp:precise'],linux_flags=['-ffp-contract=off'],run_timeout=20)
  for item in results:item['timing']=json.loads((proof/(item['config']+'-timing.json')).read_text())
  (proof/'result.json').write_text(json.dumps({'passed':True,'cases':results,'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
   'scope':'Actual line producer, edge owner and Euler functions with immutable mesh/color observation ports. Synthetic192x192 grid, same output bytes and unchanged draw order; timings are bounded CPU microbenchmarks, not native FPS.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
