"""Actual precise-position/normal owners; no model, native window or driver."""
from pathlib import Path
import hashlib
import json
import re
import unittest
from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class PreciseMeshNormals(unittest.TestCase):
    def test_shared_box_faces_match_geometric_lighting_normals(self):
        owner = ROOT / "Engine/modules/render/render.preview_grid.ixx"
        source = owner.read_text(encoding="utf-8-sig")
        def plain_module(path):
            text = path.read_text(encoding="utf-8-sig")
            text = re.sub(r"^\s*(?:export )?(?:module|import)[^;]*;\s*$", "", text, flags=re.M)
            return text.replace("export ", "")
        geometry = '\n'.join(definition(source, marker).replace('export ', '') + ';' for marker in (
            '    export struct Vec3', '    export struct Vertex'))
        geometry += '\n' + '\n'.join(definition(source, marker).replace('export ', '') for marker in (
            '    export [[nodiscard]] inline Vec3 add(', '    export [[nodiscard]] inline Vec3 scale('))
        rotation = '\n'.join(definition(source, marker) + (';' if 'struct ' in marker else '') for marker in (
            '        struct EulerRotation final', '        [[nodiscard]] inline EulerRotation euler_rotation(',
            '        [[nodiscard]] inline Vec3 rotate_euler('))
        geometry += '\nnamespace detail {\n' + rotation + '\n}\n'
        geometry += definition(source, '    export [[nodiscard]] inline Vec3 rotate_euler_degrees(').replace('export ', '')
        builder = '\n'.join(definition(source, marker) + ';' for marker in (
            '        const auto make_vertex =', '        const auto lit =', '        const auto shade_surface =',
            '        auto push_tri =', '        auto push_face =', '        auto push_box =', '        auto push_oriented_box ='))
        code = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>
MATH
LIGHTING
GEOMETRY
std::vector<Vertex> actual_box(Vec3 center,Vec3 half,Vec3 rotation,Vec3 color,
 const epochengine::lighting::LightingFrame&lightingFrame,Vec3 eye,bool oriented){
 std::vector<Vertex>out;const struct{Vec3 eye;}camera{eye};
 BUILDERS
 if(oriented)push_oriented_box(center,half,rotation,color);else push_box(center,half,color);
 return out;
}
using F3=epochengine::render_math::Float3;
F3 point(Vec3 v){return {v.x,v.y,v.z};}
namespace math=epochengine::render_math;
namespace lighting=epochengine::lighting;
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
int main(){
 const Vec3 center{3,-5,7},albedo{.24f,.37f,.52f},eye{17,19,23};
 const std::array<Vec3,6>axes{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
 for(const bool oriented:{false,true})
 for(const auto rotation:std::array{Vec3{},Vec3{15,27,39},Vec3{-90,30,175}})
 for(const auto half:std::array{Vec3{.25f,.5f,1},Vec3{2,3,.5f},Vec3{.1f,20,7}})
 for(unsigned placement=0;placement<7;++placement){
  const float distance=std::hypot(half.x,half.y,half.z)*8+4;
  lighting::LightingFrame frame;frame.revision=1;frame.environment.ambient={.12f,.12f,.12f};
  if(placement<6){const auto direction=axes[placement];frame.lights.push_back({.handle={0,1},
   .desc={.kind=lighting::LightKind::Point,.intensity=distance*distance*.35f,
    .position={center.x+direction.x*distance,center.y+direction.y*distance,center.z+direction.z*distance},.range=distance*4}});}
  const auto vertices=actual_box(center,half,rotation,albedo,frame,eye,oriented);CHECK(vertices.size()==36u);
  std::array<F3,6>outward{};
  for(unsigned face=0;face<6;++face){
   const auto*v=vertices.data()+face*6;
   const auto a=point(v[0].position),b=point(v[1].position),c=point(v[2].position),d=point(v[5].position);
   const auto inward=math::cross(math::subtract(b,a),math::subtract(c,a));
   CHECK(math::length(inward)>0);
   const auto normal=math::scale(math::normalize(inward),-1);outward[face]=normal;
   const F3 midpoint{(a.x+b.x+c.x+d.x)*.25f,(a.y+b.y+c.y+d.y)*.25f,(a.z+b.z+c.z+d.z)*.25f};
   CHECK(math::dot(normal,math::subtract(midpoint,point(center)))>0);
   const auto second=math::scale(math::normalize(math::cross(math::subtract(c,a),math::subtract(d,a))),-1);
   CHECK(math::dot(normal,second)>.99999f); // Both triangles retain the same CW outward face.
   const auto expected=lighting::evaluate_reference_raster_lighting(frame,lighting::SurfaceSample{
    .position=midpoint,.normal=normal,.viewDirection={eye.x-midpoint.x,eye.y-midpoint.y,eye.z-midpoint.z},
    .albedo={albedo.x,albedo.y,albedo.z},.specularColor={.18f,.18f,.18f},.shininess=24});
   for(unsigned corner=0;corner<6;++corner){const auto color=v[corner].color;
    CHECK(std::abs(color.x-std::clamp(expected.combined.r,0.f,1.f))<.00003f);
    CHECK(std::abs(color.y-std::clamp(expected.combined.g,0.f,1.f))<.00003f);
    CHECK(std::abs(color.z-std::clamp(expected.combined.b,0.f,1.f))<.00003f);}
  }
  // The independent triangle cross-products cover each signed axis once,
  // including rotated/nonuniform boxes; there are no duplicate or inside faces.
  for(const auto axis:axes){const auto target=oriented?rotate_euler_degrees(axis,rotation):axis;
   CHECK(std::count_if(outward.begin(),outward.end(),[&](F3 n){return math::dot(n,point(target))>.99999f;})==1);}
 }
}
'''.replace('MATH', plain_module(ROOT/'Engine/modules/render/render.math.ixx'))
        code = code.replace('LIGHTING', plain_module(ROOT/'Engine/modules/render/render.lighting.ixx'))
        code = code.replace('GEOMETRY', geometry).replace('BUILDERS', builder)
        report = ROOT/'Artifacts/Reports/shared-box-normals-nov06'
        report.mkdir(parents=True, exist_ok=True)
        unit = report/'contract.cpp'; unit.write_text(code, encoding='utf-8')
        records = compile_and_run(self, unit, report, windows_flags=('/W4','/WX'))
        (report/'result.json').write_text(json.dumps({'passed':True,'source_sha256':hashlib.sha256(owner.read_bytes()).hexdigest(),
            'scope':'Actual shared box builders and shading/rotation helpers + real reference lighting; all12triangle normals, rotation, nonuniform scale, six axis point lights and one ambient-only control; no native renderer or GUI.',
            'box_cases':126,'faces':756,'triangles':1512,'records':records},indent=2)+'\n',encoding='utf-8')

    def test_composed_rotation_and_inverse_transpose_scale(self):
        path = ROOT / "Engine/modules/render/render.preview_grid.ixx"
        source = path.read_text(encoding="utf-8-sig")
        implementation = ROOT / "Engine/src/render/render.preview_camera.cpp"
        camera_source = implementation.read_text(encoding="utf-8")
        pieces = []
        for marker, semicolon in (
            ("    export struct PreciseMeshTransform final", True),
            ("    [[nodiscard]] bool valid_precise_mesh_transform(", False),
            ("    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(", False),
            ("        struct EulerRotation final", True),
            ("        [[nodiscard]] inline EulerRotation euler_rotation(", False),
            ("        [[nodiscard]] inline Vec3 rotate_euler(", False),
            ("        [[nodiscard]] inline Vec3 mesh_vertex_normal(", False),
        ):
            owner = camera_source if ("valid_precise_mesh_transform" in marker or "make_precise_mesh_transform" in marker) else source
            text = definition(owner, marker).replace("export ", "", 1)
            if semicolon:
                text += ";"
            if "mesh_vertex_normal" in marker:
                pieces.append("struct ObjectMarker {Vec3 scale{1,1,1};std::optional<PreciseMeshTransform> preciseTransform;};")
            pieces.append(text)
        self.assertIn("normalize(detail::mesh_vertex_normal(marker,source.normal,rotation))", source)
        code = r'''
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
struct Vec3 {float x{},y{},z{};};
ACTUAL
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 for(const auto angles:std::array{Vec3{0,90,0},Vec3{15,27,39},Vec3{-90,30,175}})
 for(const auto scales:std::array{std::array{2.,3.,.5},std::array{.0001,1000.,7.},std::array{-2.,3.,.5}}){
  ObjectMarker marker;marker.scale={999,999,999};
  marker.preciseTransform=make_precise_mesh_transform({0,0,0},scales,angles);CHECK(marker.preciseTransform);
  const auto normal=mesh_vertex_normal(marker,{1,2,3},euler_rotation({}));
  CHECK(std::isfinite(normal.x)&&std::isfinite(normal.y)&&std::isfinite(normal.z));
  CHECK(std::abs(std::hypot(normal.x,normal.y,normal.z)-1.0)<1e-6);
  for(const auto tangent:std::array{std::array{2.,-1.,0.},std::array{3.,6.,-5.}}){
   const auto moved=marker.preciseTransform->apply(tangent);
   const auto agreement=normal.x*moved[0]+normal.y*moved[1]+normal.z*moved[2];
   CHECK(std::abs(agreement)/std::hypot(moved[0],moved[1],moved[2])<1e-6);
  }
 }
 ObjectMarker composed;composed.preciseTransform=make_precise_mesh_transform({0,0,0},{1,1,1},{0,90,0});
 const auto turned=mesh_vertex_normal(composed,{1,0,0},euler_rotation({}));
 CHECK(std::abs(turned.x)<1e-6&&std::abs(turned.z+1)<1e-6);
 // The original Euler path must retain its exact float operation order.
 for(const auto angles:std::array{Vec3{},Vec3{15,90,30},Vec3{-12,178,-34}}){
  ObjectMarker marker;marker.scale={2,3,4};const auto rotation=euler_rotation(angles);
  const auto actual=mesh_vertex_normal(marker,{.3f,.4f,.7f},rotation);
  const auto legacy=rotate_euler({.3f/2,.4f/3,.7f/4},rotation);
  CHECK(std::bit_cast<std::uint32_t>(actual.x)==std::bit_cast<std::uint32_t>(legacy.x));
  CHECK(std::bit_cast<std::uint32_t>(actual.y)==std::bit_cast<std::uint32_t>(legacy.y));
  CHECK(std::bit_cast<std::uint32_t>(actual.z)==std::bit_cast<std::uint32_t>(legacy.z));
 }
 composed.preciseTransform->scale={1e-30,1e30,1};
 const auto bounded=mesh_vertex_normal(composed,{1,0,0},euler_rotation({}));
 CHECK(std::isfinite(bounded.x)&&std::isfinite(bounded.z)&&std::abs(bounded.z+1)<1e-6);
}
'''.replace("ACTUAL", "\n".join(pieces))
        report = ROOT / "Artifacts/Reports/planet-clouds-nov02/precise-normals"
        report.mkdir(parents=True, exist_ok=True)
        cpp = report / "consumer.cpp"
        cpp.write_text(code, encoding="utf-8")
        runs = compile_and_run(self, cpp, report, windows_flags=("/W4", "/WX"))
        (report / "result.json").write_text(json.dumps({
            "passed": True, "scope": "Actual R/S normal transform vs transformed tangent orthogonality; composed matrix, nonuniform and reflected scales; bit-exact legacy Euler; CPU only",
            "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "implementation_sha256": hashlib.sha256(implementation.read_bytes()).hexdigest(), "runs": runs}, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
