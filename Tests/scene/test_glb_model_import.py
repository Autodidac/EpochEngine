"""Real C++23 GLB reader and mesh-artifact validation; no renderer or model."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


def consumer():
    texture_source = (ROOT / "Engine/src/assets/texture/asset.texture_import.cpp").read_text(encoding="utf-8")
    png = re.search(r"constexpr std::array<unsigned char, 80> pngSource\{(.*?)\};", texture_source, re.S).group(1)
    return r'''
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
import asset.model_import;
import asset.mesh_artifact;
import asset.texture_artifact;
namespace model=epochengine::asset::model_import;
using Json=nlohmann::json;
void check(bool value,int line){if(!value){std::fprintf(stderr,"GLB contract line %d\n",line);std::exit(86);}}
#define CHECK(x) check(static_cast<bool>(x),__LINE__)
using Bytes=std::vector<std::byte>;
void append(Bytes& b,std::uint32_t v,unsigned width=4){for(unsigned i=0;i<width;++i)b.push_back(std::byte((v>>(8*i))&255));}
void set(Bytes& b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b.at(at+i)=std::byte((v>>(8*i))&255);}
struct Fixture{Json json;Bytes binary;};
Fixture fixture(unsigned indexWidth=2){
 Fixture f;for(float v:std::array{0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f})append(f.binary,std::bit_cast<std::uint32_t>(v));
 for(int n=0;n<3;++n)for(float v:std::array{0.f,0.f,1.f})append(f.binary,std::bit_cast<std::uint32_t>(v));
 for(float v:std::array{0.f,0.f,1.f,0.f,0.f,1.f})append(f.binary,std::bit_cast<std::uint32_t>(v));
 for(std::uint32_t i=0;i<3;++i)append(f.binary,i,indexWidth);
 const auto indexBytes=3*indexWidth;while(f.binary.size()%4)f.binary.push_back(std::byte{});
 const auto imageOffset=f.binary.size();constexpr std::array<unsigned char,80> png{''' + png + r'''};
 for(auto v:png)f.binary.push_back(std::byte{v});
 f.json={
  {"asset",{{"version","2.0"}}},{"scene",0},
  {"scenes",Json::array({{{"nodes",Json::array({0})}}})},
  {"nodes",Json::array({{{"mesh",0}}})},
  {"buffers",Json::array({{{"byteLength",f.binary.size()}}})},
  {"bufferViews",Json::array({{{"buffer",0},{"byteOffset",0},{"byteLength",36}},{{"buffer",0},{"byteOffset",36},{"byteLength",36}},{{"buffer",0},{"byteOffset",72},{"byteLength",24}},{{"buffer",0},{"byteOffset",96},{"byteLength",indexBytes}},{{"buffer",0},{"byteOffset",imageOffset},{"byteLength",80}}})},
  {"accessors",Json::array({{{"bufferView",0},{"componentType",5126},{"count",3},{"type","VEC3"}},{{"bufferView",1},{"componentType",5126},{"count",3},{"type","VEC3"}},{{"bufferView",2},{"componentType",5126},{"count",3},{"type","VEC2"}},{{"bufferView",3},{"componentType",indexWidth==1?5121:indexWidth==2?5123:5125},{"count",3},{"type","SCALAR"}}})},
  {"meshes",Json::array({{{"primitives",Json::array({{{"attributes",{{"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2}}},{"indices",3},{"material",0}}})}}})},
  {"images",Json::array({{{"bufferView",4},{"mimeType","image/png"}}})},
  {"textures",Json::array({{{"source",0},{"sampler",0}}})},
  {"samplers",Json::array({{{"wrapS",33071},{"wrapT",33648},{"minFilter",9987},{"magFilter",9729}}})},
  {"materials",Json::array({{{"name","Paint"},{"doubleSided",true},{"alphaMode","MASK"},{"alphaCutoff",0.25},{"emissiveFactor",Json::array({0.1,0.2,0.3})},{"pbrMetallicRoughness",{{"baseColorFactor",Json::array({0.2,0.3,0.4,0.5})},{"metallicFactor",0.1},{"roughnessFactor",0.8},{"baseColorTexture",{{"index",0}}},{"metallicRoughnessTexture",{{"index",0}}}}},{"normalTexture",{{"index",0},{"scale",0.6}}},{"occlusionTexture",{{"index",0},{"strength",0.7}}},{"emissiveTexture",{{"index",0}}}}})}
 };return f;
}
Bytes glb(const Fixture& f,std::string text={}){
 if(text.empty())text=f.json.dump();while(text.size()%4)text.push_back(' ');auto binary=f.binary;while(binary.size()%4)binary.push_back(std::byte{});
 Bytes result;append(result,0x46546c67);append(result,2);append(result,std::uint32_t(28+text.size()+binary.size()));
 append(result,std::uint32_t(text.size()));append(result,0x4e4f534a);for(unsigned char c:text)result.push_back(std::byte{c});
 append(result,std::uint32_t(binary.size()));append(result,0x004e4942);result.insert(result.end(),binary.begin(),binary.end());return result;
}
bool near(float a,float b){return std::abs(a-b)<1e-5f;}
void fails(const Fixture& f,std::optional<model::Code> expected={}){const auto result=model::read_glb(glb(f));CHECK(!result);CHECK(result.primitives.empty()&&result.images.empty());if(expected)CHECK(result.code==*expected);}
int main(int argc,char** argv){
 CHECK(argc==2);const auto base=fixture();const auto bytes=glb(base);const auto good=model::read_glb(bytes);
 CHECK(good&&good.primitives.size()==1&&good.materials.size()==1&&good.images.size()==1&&good.meters_per_unit==1);
 CHECK(good.primitives[0].geometry->vertices().size()==3&&good.primitives[0].geometry->indices()[2]==2);
 CHECK(good.primitives[0].texcoords_present&&!good.primitives[0].normals_generated&&!good.primitives[0].reflected_winding);
 CHECK(good.images[0].encoded.size()==80&&good.images[0].mime_type=="image/png"&&!good.images[0].content.empty());
 CHECK(good.materials[0].name=="Paint"&&near(good.materials[0].base_color[3],0.5f)&&near(good.materials[0].roughness,0.8f));
 CHECK(good.materials[0].base_color_texture->image==0&&good.materials[0].base_color_texture->sampler.wrap_s==33071&&good.materials[0].base_color_texture->sampler.min_filter==9987);
 CHECK(good.materials[0].normal_texture&&good.materials[0].occlusion_texture&&good.materials[0].emissive_texture&&good.materials[0].metallic_roughness_texture);
 CHECK(near(good.materials[0].normal_scale,.6f)&&near(good.materials[0].occlusion_strength,.7f)&&good.materials[0].double_sided&&good.materials[0].alpha_mode==model::AlphaMode::mask);
 CHECK(model::read_glb(bytes).source_content==good.source_content);
 for(unsigned width:{1u,4u})CHECK(model::read_glb(glb(fixture(width))));
 auto f=base;f.json["meshes"][0]["primitives"][0].erase("indices");CHECK(model::read_glb(glb(f)));
 f=base;f.json["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL");auto generated=model::read_glb(glb(f));CHECK(generated&&generated.primitives[0].normals_generated&&near(generated.primitives[0].geometry->vertices()[0].normal[2],1));
 f=base;f.json["nodes"]=Json::array({{{"translation",Json::array({10,20,30})},{"children",Json::array({1})}},{{"mesh",0},{"scale",Json::array({2,3,4})}}});
 auto transformed=model::read_glb(glb(f));CHECK(transformed&&near(transformed.primitives[0].geometry->vertices()[1].position[0],12)&&near(transformed.primitives[0].geometry->vertices()[2].position[1],23)&&near(transformed.primitives[0].geometry->vertices()[0].position[2],30));
 f=base;f.json["nodes"][0]["scale"]=Json::array({-1,2,3});auto reflected=model::read_glb(glb(f));CHECK(reflected&&reflected.primitives[0].reflected_winding&&reflected.primitives[0].geometry->indices()[1]==2&&near(reflected.primitives[0].geometry->vertices()[1].position[0],-1));
 f=base;const double angle=std::sqrt(.5);f.json["nodes"][0]["rotation"]=Json::array({0,0,angle,angle});auto rotated=model::read_glb(glb(f));CHECK(rotated&&near(rotated.primitives[0].geometry->vertices()[1].position[1],1));
 f=base;f.json["nodes"][0]["scale"]=Json::array({2,3,4});for(int n=0;n<3;++n){set(f.binary,36+n*12,std::bit_cast<std::uint32_t>(float(angle)));set(f.binary,40+n*12,std::bit_cast<std::uint32_t>(float(angle)));set(f.binary,44+n*12,0);}
 auto normals=model::read_glb(glb(f));CHECK(normals);const auto normal=normals.primitives[0].geometry->vertices()[0].normal;CHECK(near(normal[0],float(3/std::sqrt(13.)))&&near(normal[1],float(2/std::sqrt(13.))));
 f=base;f.json["nodes"][0]["matrix"]=Json::array({1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1});CHECK(near(model::read_glb(glb(f)).primitives[0].geometry->vertices()[0].position[0],4));
 f=base;f.json["bufferViews"][0]["byteStride"]=12;CHECK(model::read_glb(glb(f)));
 f=base;Bytes interleaved;for(std::size_t n=0;n<3;++n){interleaved.insert(interleaved.end(),base.binary.begin()+n*12,base.binary.begin()+n*12+12);append(interleaved,0xdeadbeef);}
 interleaved.insert(interleaved.end(),base.binary.begin()+36,base.binary.end());f.binary=std::move(interleaved);
 f.json["buffers"][0]["byteLength"]=f.binary.size();f.json["bufferViews"][0]["byteStride"]=16;f.json["bufferViews"][0]["byteLength"]=48;
 for(std::size_t n=1;n<5;++n)f.json["bufferViews"][n]["byteOffset"]=f.json["bufferViews"][n]["byteOffset"].get<unsigned>()+12;
 auto stride=model::read_glb(glb(f));CHECK(stride&&near(stride.primitives[0].geometry->vertices()[1].position[0],1));
 auto bad=bytes;set(bad,4,1);CHECK(!model::read_glb(bad));bad=bytes;set(bad,8,0);CHECK(!model::read_glb(bad));CHECK(!model::read_glb(std::span(bytes).first(18)));
 bad=bytes;set(bad,12,0xffffffffu);CHECK(!model::read_glb(bad));bad=bytes;set(bad,16,123);CHECK(model::read_glb(bad).code==model::Code::unsupported_feature);
 f=base;f.json["buffers"][0]["uri"]="../outside.bin";fails(f,model::Code::external_reference);
 f=base;f.json["images"][0]["uri"]="data:image/png;base64,AAAA";fails(f,model::Code::external_reference);
 f=base;f.json["accessors"][0]["byteOffset"]=999;fails(f);f=base;f.json["accessors"][0]["count"]=0xffffffffffffffffull;fails(f,model::Code::limit_exceeded);
 f=base;f.json["bufferViews"][0]["byteLength"]=32;fails(f);f=base;f.json["bufferViews"][0]["byteStride"]=8;fails(f);
 f=base;f.json["accessors"][0]["componentType"]=5123;fails(f);f=base;f.json["accessors"][0]["normalized"]=true;fails(f,model::Code::unsupported_feature);
 f=base;f.json["accessors"][0]["sparse"]=Json::object();fails(f,model::Code::unsupported_feature);
 f=base;set(f.binary,0,0x7fc00000);fails(f,model::Code::invalid_geometry);f=base;f.binary[100]=std::byte{3};fails(f,model::Code::invalid_geometry);
 f=base;f.json["nodes"][0]["scale"]=Json::array({0,1,1});fails(f);f=base;f.json["nodes"][0]["translation"]=Json::array({1e100,0,0});fails(f,model::Code::invalid_geometry);
 f=base;f.json["nodes"][0]["rotation"]=Json::array({0,0,0,2});fails(f);f=base;f.json["nodes"][0]["skin"]=0;fails(f,model::Code::unsupported_feature);
 f=base;f.json["animations"]=Json::array({Json::object()});fails(f,model::Code::unsupported_feature);
 f=base;f.json["meshes"][0]["primitives"][0]["targets"]=Json::array();fails(f,model::Code::unsupported_feature);
 f=base;f.json["meshes"][0]["primitives"][0]["extensions"]={{"KHR_draco_mesh_compression",Json::object()}};fails(f,model::Code::unsupported_feature);
 f=base;f.json["extensionsRequired"]=Json::array({"KHR_texture_basisu"});fails(f,model::Code::unsupported_feature);
 f=base;f.json["nodes"][0]["children"]=Json::array({0});fails(f);f=base;f.json["nodes"].push_back({{"children",Json::array({0})}});f.json["nodes"][0]["children"]=Json::array({1});fails(f);
 f=base;f.json["nodes"][0]["children"]=Json::array({1,1});f.json["nodes"].push_back(Json::object());fails(f);
 f=base;f.json["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_0");fails(f);
 f=base;f.json["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=1;fails(f,model::Code::unsupported_feature);
 f=base;f.json["materials"][0]["pbrMetallicRoughness"]["metallicFactor"]=-.1;fails(f);
 f=base;f.json["images"][0]["mimeType"]="image/webp";fails(f,model::Code::unsupported_feature);
 f=base;f.binary[104]=std::byte{};fails(f);
 auto limits=model::Limits{};limits.maximum_file_bytes=28;CHECK(model::read_glb(bytes,limits).code==model::Code::limit_exceeded);
 limits={};limits.maximum_json_values=2;CHECK(model::read_glb(bytes,limits).code==model::Code::limit_exceeded);
 limits={};limits.maximum_image_bytes=79;CHECK(model::read_glb(bytes,limits).code==model::Code::limit_exceeded);
 f=base;f.json["nodes"].push_back({{"mesh",0}});f.json["scenes"][0]["nodes"].push_back(1);CHECK(model::read_glb(glb(f)).primitives.size()==2);
 limits={};limits.maximum_primitives=1;CHECK(model::read_glb(glb(f),limits).code==model::Code::limit_exceeded);
 auto duplicate=base.json.dump();duplicate.replace(0,1,"{\"scene\":0,");CHECK(!model::read_glb(glb(base,duplicate)));
 auto deep=base;Json nesting=Json::object();for(int n=0;n<70;++n)nesting=Json{{"nested",std::move(nesting)}};deep.json["extras"]=nesting;CHECK(model::read_glb(glb(deep)).code==model::Code::limit_exceeded);
 const std::filesystem::path path=std::filesystem::path(argv[1])/"triangle.glb";{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));CHECK(out);}
 CHECK(model::read_glb_file(path).source_content==good.source_content);CHECK(!model::read_glb_file(path.parent_path()));
 // The production base-color bridge is deliberately narrower than the general
 // static reader. It must not silently flatten valid PBR metadata into color.
 CHECK(model::compile_base_color_glb(bytes).code==model::Code::unsupported_feature);
 auto opaque=base;auto& material=opaque.json["materials"][0];
 material.erase("normalTexture");material.erase("occlusionTexture");material.erase("emissiveTexture");material.erase("emissiveFactor");
 material["doubleSided"]=false;material["alphaMode"]="OPAQUE";
 auto& pbr=material["pbrMetallicRoughness"];pbr.erase("metallicRoughnessTexture");pbr["baseColorFactor"]=Json::array({1,1,1,1});pbr["metallicFactor"]=0;pbr["roughnessFactor"]=1;
 const auto compiled=model::compile_base_color_glb(glb(opaque));
 CHECK(compiled&&compiled.textures.size()==1&&compiled.primitive_textures==std::vector<std::uint32_t>{0});
 CHECK(compiled.textures[0].artifact.identity.profile.color_space==epochengine::asset::texture::ColorSpace::srgb);
 CHECK(epochengine::asset::texture::validate_compiled_artifact(compiled.textures[0].artifact));
 CHECK(compiled.model.primitives[0].geometry->vertices()[1].uv[0]==1&&compiled.model.primitives[0].geometry->vertices()[2].uv[1]==1);
 auto second=opaque;second.json["nodes"].push_back({{"mesh",0}});second.json["scenes"][0]["nodes"].push_back(1);
 const auto shared=model::compile_base_color_glb(glb(second));CHECK(shared&&shared.textures.size()==1&&shared.primitive_textures==std::vector<std::uint32_t>({0,0}));
 for(const char* feature:{"normalTexture","occlusionTexture","emissiveTexture"}){auto unsupported=opaque;unsupported.json["materials"][0][feature]={{"index",0}};CHECK(!model::compile_base_color_glb(glb(unsupported)));}
 auto tint=opaque;tint.json["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][0]=0.5;CHECK(!model::compile_base_color_glb(glb(tint)));
 auto alpha=opaque;alpha.json["materials"][0]["alphaMode"]="BLEND";CHECK(!model::compile_base_color_glb(glb(alpha)));
 auto malformed=opaque;malformed.json["bufferViews"][4]["byteLength"]=8;CHECK(!model::compile_base_color_glb(glb(malformed)));
 CHECK(!model::compile_base_color_glb(glb(opaque),{},0));CHECK(!model::compile_base_color_glb(glb(opaque),{},4096,1));
}
'''


class GlbModelImport(unittest.TestCase):
    def test_actual_module_debug_release(self):
        if os.name != "nt":
            self.skipTest("MSVC C++23 module proof lane")
        proof = ROOT / "Artifacts/Reports/model-import-glb-oct28" / str(time.time_ns())
        build = ROOT / "Artifacts/Intermediate/tests/model-import-glb" / proof.name
        proof.mkdir(parents=True)
        source = proof / "asset.model_import_contract.cpp"
        source.write_text(consumer(), encoding="utf-8")
        modules = [ROOT / "Engine/modules" / name for name in (
            "asset.texture_artifact.ixx", "asset.texture_import.ixx", "asset.mesh_artifact.ixx", "asset.model_import.ixx")]
        sources = [source, ROOT / "Engine/src/assets/texture/asset.texture_artifact.cpp",
                   ROOT / "Engine/src/assets/mesh/asset.model_import.cpp",
                   ROOT / "Engine/src/assets/texture/asset.texture_import.cpp",
                   ROOT / "Engine/src/assets/image.stb.cpp"]
        includes = ROOT / "Artifacts/Dependencies/vcpkg_installed/x64-windows/include"
        self.assertTrue((includes / "nlohmann/json.hpp").is_file())
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ModelImport LANGUAGES CXX)
add_executable(model_import {" ".join('"'+p.as_posix()+'"' for p in sources)})
target_compile_features(model_import PRIVATE cxx_std_23)
target_compile_options(model_import PRIVATE /W3 /WX /utf-8)
target_include_directories(model_import PRIVATE "{includes.as_posix()}")
target_sources(model_import PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for mode in ("Debug", "Release"):
            data = proof / mode
            data.mkdir()
            commands += [["cmake", "--build", str(build), "--config", mode, "--parallel", "1"],
                         [str(build / mode / "model_import.exe"), str(data)]]
        receipts = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            receipts.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-7000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({"passed": True, "nativeAcceptance": False,
            "commands": receipts, "sources": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in modules + sources[1:]},
            "scope": "Actual GLB reader, mesh artifact and strict base-color/PNG compiler; bounded static geometry/transforms, actual UVs, shared textures, unsupported material refusal. No renderer, project approval/import, model or network."}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
