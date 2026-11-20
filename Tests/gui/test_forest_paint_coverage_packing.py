"""Compile the actual Forest coverage packer and mesh transform without a renderer."""
from pathlib import Path
import hashlib
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]

STANDALONE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace epochengine {
namespace asset::mesh { struct Artifact; }
namespace visuals {
struct Rgb { float r{},g{},b{}; };
constexpr Rgb object_default() noexcept { return {}; }
}
namespace previewgrid {
struct MeshBaseColorBinding;
struct MeshPaintView;
PACKER_OWNER
}
}

namespace grid = epochengine::previewgrid;
using Point = std::array<double,3>;
using Texel = std::array<float,4>;

void near(double actual, double expected, double tolerance = 1e-8) {
    assert(std::abs(actual - expected) <= tolerance);
}

double apply_covector(Texel coefficients, Point delta) {
    return coefficients[0]*delta[0]+coefficients[1]*delta[1]+coefficients[2]*delta[2];
}

grid::MeshPaintLasso make_polygon(double extent) {
    return {.center={2.5e12+16.,-3.2e12-32.,1.9e12+4.}, .direction={0.,0.,2.}, .radius=6371000.,
        .outline={{-extent,-extent,1.},{2.*extent,-extent,1.},{extent,0.,1.},
            {2.*extent,extent,1.},{-extent,extent,1.}},
        .fill={.1f,.5f,.2f,.4f}, .edge={.9f,.7f,.3f,.8f}};
}

void precise_rebased_polygon() {
    constexpr double extent = 0x1p-23;
    auto paint = std::make_shared<grid::MeshPaintView>();
    paint->origin = {2.5e12,-3.2e12,1.9e12};
    paint->meters_per_unit = 1e6;
    paint->lassos.push_back(make_polygon(extent));
    const auto& polygon = paint->lassos.front();
    const Point meterOffset{16.,-32.,4.};
    const Point worldAnchor{.015625,-.03125,.0625};
    const double radiusInMesh=polygon.radius/paint->meters_per_unit;
    const auto draw = [&](Point local) -> Point {
        // Independent world oracle: rotate +90 degrees around Z after (2,3,4) scale.
        return {worldAnchor[0]-3.*(local[1]-meterOffset[1]/paint->meters_per_unit),
            worldAnchor[1]+2.*(local[0]-meterOffset[0]/paint->meters_per_unit),
            worldAnchor[2]+4.*(local[2]-meterOffset[2]/paint->meters_per_unit-radiusInMesh)};
    };
    const grid::ObjectMarker marker{
        .position={123.f,456.f,789.f}, .scale={1.f,2.f,3.f}, .rotationDegrees={10.f,20.f,30.f},
        .preciseTransform=grid::PreciseMeshTransform{draw({0.,0.,0.}),{2.,3.,4.},
            {0.,-1.,0.,1.,0.,0.,0.,0.,1.}},
        .paintView=paint};
    const auto packed=grid::pack_mesh_paint_coverage(marker);
    assert(packed.size()==grid::kMeshPaintCoverageRowTexels);
    for (std::size_t i=0; i<3; ++i) near(packed[0][i],worldAnchor[i],1e-10);
    assert(packed[0][3]==5.f && packed[5]==polygon.fill && packed[6]==polygon.edge);
    assert(packed[1][0]==0.f && packed[1][2]==0.f && packed[2][1]==0.f && packed[2][2]==0.f);
    near(packed[1][1],1./(2.*radiusInMesh));
    near(packed[2][0],-1./(3.*radiusInMesh));
    near(packed[3][2],1./(4.*radiusInMesh));
    const float e=static_cast<float>(extent);
    assert((packed[4]==Texel{-e,-e,2.f*e,e}));
    assert((packed[7]==Texel{-e,-e,0.f,0.f} && packed[8]==Texel{2.f*e,-e,0.f,0.f}));
    assert((packed[9]==Texel{e,0.f,0.f,0.f} && packed[10]==Texel{2.f*e,e,0.f,0.f}));
    assert((packed[11]==Texel{-e,e,0.f,0.f} && packed[12]==Texel{}));

    // Replay the packed covectors as fragment arithmetic at different terrain heights.
    for (double radialScale : {.8,1.,1.2}) {
        constexpr double x=.4*extent, y=-.7*extent;
        const double magnitude=std::hypot(x,y,1.);
        const double units=radiusInMesh*radialScale/magnitude;
        const auto fragment=draw({meterOffset[0]/paint->meters_per_unit+x*units,
            meterOffset[1]/paint->meters_per_unit+y*units,meterOffset[2]/paint->meters_per_unit+units});
        const Point delta{double(float(fragment[0]))-packed[0][0],double(float(fragment[1]))-packed[0][1],
            double(float(fragment[2]))-packed[0][2]};
        const double depth=1.+apply_covector(packed[3],delta);
        near(depth,radialScale/magnitude,3e-8);
        near(apply_covector(packed[1],delta)/depth,x,1e-9);
        near(apply_covector(packed[2],delta)/depth,y,1e-9);
    }
}

void fallback_and_pole() {
    auto paint=std::make_shared<grid::MeshPaintView>();
    paint->meters_per_unit=6.;
    paint->origin={12.,0.,24.};
    paint->lassos.push_back({.center={12.,0.,24.},.direction={0.,8.,0.},.radius=12.,
        .outline={{0.,2.,-1.},{1.,2.,1.},{-1.,2.,1.}}});
    grid::ObjectMarker marker{.position={5.,7.,9.}, .scale={3.,2.,4.},
        .rotationDegrees={0.,0.,90.},.paintView=paint};
    const auto packed=grid::pack_mesh_paint_coverage(marker);
    assert(packed.size()==grid::kMeshPaintCoverageRowTexels);
    near(packed[0][0],1.); near(packed[0][1],7.); near(packed[0][2],9.);
    near(packed[1][2],.125); near(packed[2][1],1./6.); near(packed[3][0],-.25);
    assert((packed[4]==Texel{-.5f,-.5f,.5f,.5f}));
    assert((packed[7]==Texel{-.5f,0.f,0.f,0.f} && packed[8]==Texel{.5f,.5f,0.f,0.f}));
    assert((packed[9]==Texel{.5f,-.5f,0.f,0.f}));
    marker.preciseTransform=grid::make_precise_mesh_transform({5.,7.,9.},{3.,2.,4.},{0.,0.,90.});
    assert(marker.preciseTransform && grid::pack_mesh_paint_coverage(marker)==packed);
}

void invalid_and_limits() {
    constexpr auto nan=std::numeric_limits<double>::quiet_NaN();
    constexpr auto infinity=std::numeric_limits<double>::infinity();
    constexpr auto width=grid::kMeshPaintCoverageRowTexels;
    constexpr auto rowLimit=grid::kMaximumMeshPaintCoverageRows;
    assert(width==135u && rowLimit==65u);
    assert(grid::pack_mesh_paint_coverage({}).empty());
    auto paint=std::make_shared<grid::MeshPaintView>();
    grid::ObjectMarker marker{.paintView=paint};
    assert(grid::pack_mesh_paint_coverage(marker).empty());
    const auto usable=make_polygon(.1);
    paint->lassos={usable};
    for (auto scale : {0.,-1.,nan,infinity}) {
        paint->meters_per_unit=scale;
        assert(grid::pack_mesh_paint_coverage(marker).empty());
    }
    paint->meters_per_unit=1.;
    assert(!grid::pack_mesh_paint_coverage(marker).empty());
    marker.scale.x=0.f;
    assert(grid::pack_mesh_paint_coverage(marker).empty());
    marker.scale.x=1.f;
    marker.preciseTransform=grid::PreciseMeshTransform{};
    marker.preciseTransform->rotation[0]=.25;
    assert(grid::pack_mesh_paint_coverage(marker).empty());
    marker.preciseTransform.reset();

    for (auto radius : {0.,-1.,nan,infinity}) {
        paint->lassos[0].radius=radius;
        assert(grid::pack_mesh_paint_coverage(marker).empty());
    }
    const auto reject = [&](auto mutate) {
        paint->lassos={usable,usable};
        mutate(paint->lassos[1]);
        assert(grid::pack_mesh_paint_coverage(marker).empty());
    };
    reject([](auto& polygon){polygon.outline.resize(2u);});
    reject([](auto& polygon){polygon.outline.resize(129u,{0,0,1});});
    reject([](auto& polygon){polygon.outline[1]={0,0,0};});
    reject([](auto& polygon){polygon.outline[1]={0,1,-1};});
    reject([](auto& polygon){polygon.outline[1]={1,0,.5};});
    reject([&](auto& polygon){polygon.outline[1]={1,0,nan};});
    reject([](auto& polygon){polygon.direction={0,0,0};});
    reject([&](auto& polygon){polygon.center[2]=infinity;});
    reject([](auto& polygon){polygon.center[2]=1e100;});
    reject([](auto& polygon){polygon.fill[0]=-.01f;});
    reject([](auto& polygon){polygon.edge[3]=1.01f;});
    reject([&](auto& polygon){polygon.edge[2]=static_cast<float>(nan);});

    auto bounded=usable;
    bounded.outline.clear();
    for(std::size_t vertex=0;vertex<128u;++vertex) {
        const auto angle=double(vertex)*6.28318530717958647692/128.;
        bounded.outline.push_back({.1*std::cos(angle),.1*std::sin(angle),1.});
    }
    paint->lassos.assign(rowLimit,bounded);
    paint->lassos.back().fill[3]=.9f;
    const auto table=grid::pack_mesh_paint_coverage(marker);
    assert(table.size()==width*rowLimit && table[3u*width][3]==128.f);
    assert(table[(rowLimit-1u)*width+5u][3]==.9f);
    paint->lassos.push_back(bounded);
    assert(grid::pack_mesh_paint_coverage(marker).empty());
}

int main() {
    precise_rebased_polygon();
    fallback_and_pole();
    invalid_and_limits();
}
'''


class ForestPaintCoveragePacking(unittest.TestCase):
    def test_actual_packer_uses_rebased_body_chart_and_rejects_invalid_rows(self):
        module = ROOT / "Engine/modules/render/render.preview_grid.ixx"
        transform = ROOT / "Engine/src/render/render.preview_camera.cpp"
        frozen = {owner: owner.read_bytes() for owner in (module, transform)}
        module_text, transform_text = (value.decode("utf-8").replace("\r\n", "\n") for value in frozen.values())
        fields = "\n".join(definition(module_text, f"    export {marker}", True) for marker in (
            "struct Vec3\n", "enum class ObjectPreviewPrimitive", "struct PreciseMeshTransform final",
            "struct ObjectMarker\n", "struct MeshPaintRegion final", "struct MeshPaintLasso final", "struct MeshPaintView final",
        ))
        default_color = definition(module_text, "    export [[nodiscard]] constexpr Vec3 visual_rgb(")
        fields = fields.replace("    export struct ObjectMarker\n", default_color + "\n    export struct ObjectMarker\n")
        transform_functions = "\n".join(definition(transform_text, f"    [[nodiscard]] {marker}") for marker in (
            "bool valid_precise_mesh_transform(", "std::optional<PreciseMeshTransform> make_precise_mesh_transform(",
        ))
        constants = "\n".join(line for line in module_text.splitlines()
                              if "export inline constexpr std::size_t k" in line and "MeshPaintCoverage" in line)
        packing = definition(module_text, "    export [[nodiscard]] inline std::vector<std::array<float,4>> pack_mesh_paint_coverage(")
        actual = "\n".join((fields, transform_functions, constants, packing)).replace("export ", "")
        report = ROOT / "Artifacts/Reports/forest-paint-coverage-packing" / str(time.time_ns())
        report.mkdir(parents=True)
        fixture = report / "contract.cpp"
        fixture.write_text(STANDALONE.replace("PACKER_OWNER", actual), encoding="utf-8")
        records = compile_and_run(self, fixture, report)
        (report / "records.json").write_text(json.dumps({"runs": records, "source_sha256": {
            str(owner.relative_to(ROOT)): hashlib.sha256(value).hexdigest() for owner, value in frozen.items()
        }}, indent=2), encoding="utf-8")
