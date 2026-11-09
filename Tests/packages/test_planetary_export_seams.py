"""Measure actual converted mixed-LOD surfaces and skirts; no GPU or native runtime."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "Engine/dep/EpochEngineExtensions/packages/planetary_terrain"

CODE = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>
import planet.terrain;
import planet.streaming;
import asset.mesh_artifact;
namespace core=epoch::extensions::planetary_terrain::core;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace artifact=epochengine::asset::mesh;
using V=core::DVec3;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Converted seam contract failed line %d\n",line);std::exit(87);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V unit(V p){return p/core::length(p);}
V point(const artifact::Vertex& p){return {p.position[0],p.position[1],p.position[2]};}
int actual_conversion(const stream::State& state,std::vector<artifact::Vertex>& vertices,std::vector<std::uint32_t>& indices){
 const auto cut=stream::draw_cut(state);const double scale=10.0/state.world.body(state.request.body_index).radius;
 CONVERSION
 return 0;
}
std::uint32_t edge_vertex(unsigned edge,unsigned k,unsigned n){switch(edge){case 0:return k*(n+1);case 1:return k*(n+1)+n;case 2:return k;default:return n*(n+1)+k;}}
std::size_t lower_vertex(const core::Mesh& mesh,std::size_t top,unsigned layer,unsigned n){
 const auto grid=std::size_t(n+1)*(n+1),per=grid+4*n;const auto& vertex=mesh.vertices[top];
 for(std::size_t i=layer*per+grid;i<(layer+1)*per;++i){const auto& v=mesh.vertices[i];
  if(v.normal.x==vertex.normal.x&&v.normal.y==vertex.normal.y&&v.normal.z==vertex.normal.z
   &&v.parent_delta.x==vertex.parent_delta.x&&v.parent_delta.y==vertex.parent_delta.y&&v.parent_delta.z==vertex.parent_delta.z
   &&v.color==vertex.color)return i;
 }CHECK(false);return 0;
}
struct RayHit {bool valid{};double radius{},residual{},t{};};
RayHit ray_edge(V direction,V a,V b){
 const auto difference=b-a;const auto p=a-direction*dot(a,direction),q=difference-direction*dot(difference,direction);
 const auto denominator=dot(q,q);if(denominator<1e-25)return {};
 const auto t=-dot(p,q)/denominator;if(t< -1e-5||t>1.00001)return {};
 const auto on=a+difference*t;return {true,dot(on,direction),core::length(on-direction*dot(on,direction)),t};
}
struct Stats {std::uint64_t states{},mixedEdges{},cubeEdges{},oddSamples{},waterSamples{},exposed{},zeroSkirtExposed{},windingInward{},skirtTriangles{};double maxRadialGapMeters{},maxResidualMeters{};};
void examine(stream::State& state,Stats& stats){
 const unsigned n=state.request.cells_per_tile;const auto grid=std::size_t(n+1)*(n+1),per=grid+4*n;
 const auto cut=stream::draw_cut(state);CHECK(!cut.empty());double coverage{};
 std::vector<std::size_t> offsets,indexOffsets;std::size_t offset{},indexOffset{};for(const auto key:cut){offsets.push_back(offset);indexOffsets.push_back(indexOffset);const auto& mesh=stream::detail::find(state,key)->mesh;offset+=mesh.vertices.size();indexOffset+=mesh.indices.size();coverage+=1.0/double(std::uint64_t{1}<<(2*key.level));}
 CHECK(std::abs(coverage-6.0)<1e-10);
 std::vector<artifact::Vertex> vertices;std::vector<std::uint32_t> indices;CHECK(actual_conversion(state,vertices,indices)==0&&vertices.size()==offset&&indices.size()==indexOffset);
 const auto built=artifact::build_artifact(vertices,indices);CHECK(built);const auto bytes=artifact::encode_artifact(*built.artifact);const auto decoded=artifact::decode_artifact(bytes);CHECK(decoded);
 const auto rendered=decoded.artifact->vertices();const auto renderedIndices=decoded.artifact->indices();const double scale=10.0/state.world.body(0).radius,tolerance=4.0*scale+8.0*std::numeric_limits<float>::epsilon()*10.0;
 for(std::size_t f=0;f<cut.size();++f){const auto fine=cut[f];const auto& fm=stream::detail::find(state,fine)->mesh;const auto neighbors=core::neighbors(fine);
  for(std::size_t i=0;i<fm.indices.size();++i)CHECK(renderedIndices[indexOffsets[f]+i]==offsets[f]+fm.indices[i]);
  const unsigned layers=unsigned(fm.vertices.size()/per);CHECK(layers==1||layers==2);
  for(unsigned layer=0;layer<layers;++layer){const auto start=layer*(6*n*n+24*n)+6*n*n,end=(layer+1)*(6*n*n+24*n);CHECK(end<=fm.indices.size());
   for(std::size_t i=start;i<end;i+=3){const auto a=point(rendered[renderedIndices[indexOffsets[f]+i]]),b=point(rendered[renderedIndices[indexOffsets[f]+i+1]]),c=point(rendered[renderedIndices[indexOffsets[f]+i+2]]);
    const auto normal=cross(b-a,c-a);CHECK(core::finite(normal)&&core::length(normal)>1e-18);const auto radial=unit((a+b+c)/3.0),center=core::tile_direction(fine,.5,.5);const auto outward=radial*dot(center,radial)-center;
    ++stats.skirtTriangles;if(dot(normal,outward)< -1e-12)++stats.windingInward;
   }
  }
  for(unsigned edge=0;edge<4;++edge)for(std::size_t c=0;c<cut.size();++c){const auto coarse=cut[c];if(fine.level!=coarse.level+1||!core::contains(coarse,neighbors[edge]))continue;
   ++stats.mixedEdges;if(fine.face!=coarse.face)++stats.cubeEdges;const auto& cm=stream::detail::find(state,coarse)->mesh;const unsigned commonLayers=std::min(layers,unsigned(cm.vertices.size()/per));
   for(unsigned layer=0;layer<commonLayers;++layer)for(unsigned k=1;k<n;k+=2){
    const auto fv=layer*per+edge_vertex(edge,k,n),fl=lower_vertex(fm,fv,layer,n);const auto fineTop=point(rendered[offsets[f]+fv]),fineLow=point(rendered[offsets[f]+fl]);
    const unsigned x=edge==0?0:edge==1?n:k,y=edge==2?0:edge==3?n:k;const auto direction=core::tile_direction(fine,double(x)/n,double(y)/n);
    RayHit closest;closest.residual=1e30;std::size_t ca{},cb{};
    for(unsigned ce=0;ce<4;++ce)for(unsigned j=0;j<n;++j){const auto a=layer*per+edge_vertex(ce,j,n),b=layer*per+edge_vertex(ce,j+1,n);const auto hit=ray_edge(direction,point(rendered[offsets[c]+a]),point(rendered[offsets[c]+b]));
     if(hit.valid&&hit.residual<closest.residual){closest=hit;ca=a;cb=b;}}
    CHECK(closest.valid&&closest.residual<=tolerance);const auto lower=ray_edge(direction,point(rendered[offsets[c]+lower_vertex(cm,ca,layer,n)]),point(rendered[offsets[c]+lower_vertex(cm,cb,layer,n)]));CHECK(lower.valid&&lower.residual<=tolerance);
    const double highFine=dot(fineTop,direction),lowFine=dot(fineLow,direction),lowCoarse=lower.radius,highCoarse=closest.radius;
    CHECK(lowFine<highFine&&lowCoarse<highCoarse);++stats.oddSamples;if(layer)++stats.waterSamples;
    stats.maxRadialGapMeters=std::max(stats.maxRadialGapMeters,std::abs(highFine-highCoarse)/scale);stats.maxResidualMeters=std::max(stats.maxResidualMeters,closest.residual/scale);
    if(std::max(lowFine,lowCoarse)>std::min(highFine,highCoarse)+tolerance)++stats.exposed;
    // Negative control: eliminating both skirts must expose the noncoincident
    // fine/coarse chord samples. Zero top-edge distance is not the real contract.
    if(std::abs(highFine-highCoarse)>tolerance)++stats.zeroSkirtExposed;
   }
  }
 }++stats.states;
}
int main(int argc,char**argv){CHECK(argc==2);Stats stats;
 for(bool wet:{false,true})for(const auto direction:{core::cube_direction(0,.13,.09),core::cube_direction(0,1,0),core::cube_direction(0,1,1)}){
  stream::State state{1337};stream::Request request;request.body_index=0;request.cells_per_tile=16;request.ocean_surface=wet;request.lod.max_level=18;request.lod.max_leaves=48;request.maximum_bytes=17u*1024u*1024u;
  request.camera=state.world.center(0,0)+direction*(state.world.body(0).radius+500.0);CHECK(stream::update(state,request)==stream::Status::ready);unsigned jobs{};
  while(const auto work=stream::next_work(state)){CHECK(++jobs<1024);auto mesh=stream::build(state,*work);CHECK(stream::complete(state,*work,std::move(mesh))==stream::Status::ready);}
  CHECK(stream::stats(state).pending_patches==0);examine(state,stats);
 }
 std::ofstream result(argv[1]);result<<"{\n\"states\":"<<stats.states<<",\n\"mixedEdges\":"<<stats.mixedEdges<<",\n\"cubeEdges\":"<<stats.cubeEdges<<",\n\"oddSamples\":"<<stats.oddSamples<<",\n\"waterSamples\":"<<stats.waterSamples<<",\n\"exposedSkirtGaps\":"<<stats.exposed<<",\n\"zeroSkirtNegativeControl\":"<<stats.zeroSkirtExposed<<",\n\"skirtTriangles\":"<<stats.skirtTriangles<<",\n\"inwardWinding\":"<<stats.windingInward<<",\n\"maximumFineCoarseGapMeters\":"<<stats.maxRadialGapMeters<<",\n\"maximumRayResidualMeters\":"<<stats.maxResidualMeters<<"\n}\n";result.close();
 CHECK(stats.states==6&&stats.mixedEdges>0&&stats.cubeEdges>0&&stats.oddSamples>0&&stats.waterSamples>0&&stats.zeroSkirtExposed>0);
 CHECK(stats.exposed==0&&stats.windingInward==0); // Actual radial skirt overlap, not coincident top edges.
}
'''


class PlanetaryExportSeams(unittest.TestCase):
    def test_actual_converted_static_skirt_overlap(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC actual optional module CPU lane")
        if not (PACKAGE / "tools/planet.stream_export.cpp").is_file():
            self.skipTest("Reviewed optional planetary source not staged in this checkout")
        exporter = (PACKAGE / "tools/planet.stream_export.cpp").read_text(encoding="utf-8")
        conversion = definition(exporter[exporter.index("    int write_snapshot("):], "            for(const auto tile:cut)")
        code = CODE.replace("CONVERSION", conversion)
        proof = ROOT / "Artifacts/Reports/planet-converted-seams-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "consumer.cpp").write_text(code, encoding="utf-8")
        owners = [PACKAGE / "upstream/EpochPlanet-0.2.3/src/core/world.cpp", PACKAGE / "upstream/EpochPlanet-0.2.3/src/core/terrain.cpp", ROOT / "Engine/src/assets/texture/asset.texture_artifact.cpp"]
        modules = [PACKAGE / "modules/planet.terrain.ixx", PACKAGE / "modules/planet.streaming.ixx", ROOT / "Engine/modules/asset.mesh_artifact.ixx", ROOT / "Engine/modules/asset.texture_artifact.ixx"]
        paths = lambda values: "\n".join('"'+p.as_posix()+'"' for p in values)
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(PlanetaryConvertedSeams LANGUAGES CXX)
add_executable(converted_seams consumer.cpp {paths(owners)})
target_compile_features(converted_seams PRIVATE cxx_std_23)
target_compile_options(converted_seams PRIVATE /utf-8 /EHsc)
target_include_directories(converted_seams PRIVATE "{(PACKAGE/'upstream/EpochPlanet-0.2.3/include').as_posix()}")
target_sources(converted_seams PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(PACKAGE/'modules').as_posix()}" "{(ROOT/'Engine/modules').as_posix()}" FILES {paths(modules)})
enable_testing()
add_test(NAME actual_converted_static_seams COMMAND converted_seams "{proof.as_posix()}/measurements.json")
set_tests_properties(actual_converted_static_seams PROPERTIES TIMEOUT 90)
''', encoding="utf-8")
        identity = hashlib.sha256(code.encode()).hexdigest()[:12]
        build = ROOT / "Artifacts/Intermediate/cmake/planet-converted-seams" / identity
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"], ["cmake", "--build", str(build), "--config", "Release", "--parallel", "1"], ["ctest", "--test-dir", str(build), "-C", "Release", "--output-on-failure"]]
        runs = []
        for index, command in enumerate(commands):
            run = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=180)
            log = proof / f"lane-{index}.log"
            log.write_text(run.stdout+run.stderr, encoding="utf-8")
            runs.append({"command": command, "exitCode": run.returncode, "log": str(log)})
            self.assertEqual(run.returncode, 0, run.stdout[-8000:]+run.stderr)
        (proof / "result.json").write_text(json.dumps({"passed": True, "commands": runs, "scope": "Actual original world/terrain, actual bounded streaming and exporter conversion, actual v1 encode/decode; identity marker transform only. Static radial skirt overlap at six native-quality mixed2:1 face/corner cases; no GPU/clipping/native crack fix or geomorph timing proof.", "measurements": json.loads((proof / "measurements.json").read_text()), "sourceHashes": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in owners+modules+[PACKAGE/'tools/planet.stream_export.cpp']}}, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
