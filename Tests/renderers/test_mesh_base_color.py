"""Real preview/material module consumer; no window, driver, or model launch."""
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
    return r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <limits>
#include <span>
#include <vector>
#include "epoch/planet/terrain.hpp"
import render.preview_grid;
import render.lighting;
import render.texture_artifact;
import render.canvas2d;
import render.device;
import asset.texture_artifact;
import asset.mesh_artifact;
import authoring.texture;
namespace preview=epochengine::previewgrid;
namespace transfer=epochengine::texture_artifact;
namespace texture=epochengine::asset::texture;
namespace author=epochengine::authoring::texture;
namespace canvas=epochengine::canvas2d;
namespace mesh=epochengine::asset::mesh;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Mesh base-color line %d\n",line);std::exit(86);}}
#define CHECK(x) check(static_cast<bool>(x),__LINE__)
auto artifact(){
 author::CanvasDescriptor canvas{};canvas.width=8;canvas.height=8;canvas.tile_extent=4;canvas.mip_count=1;
 canvas.format=author::PixelFormat::rgba8_srgb;canvas.color_space=texture::ColorSpace::srgb;
 author::TextureDocument doc{author::DocumentHandle{91,1},author::BranchIdentity{19,1},canvas};
 CHECK(doc.create_layer(author::LayerDescriptor{.name="Base color"},0,author::TemporalPoint{{19,1},1}));
 texture::TextureCompileProfile profile{};profile.format=texture::ArtifactFormat::rgba8_srgb;profile.color_space=texture::ColorSpace::srgb;
 auto compiled=doc.compile_artifact(profile);CHECK(compiled);return compiled;
}
void check_cutout(const std::shared_ptr<preview::MeshBaseColorBinding>& binding){
 auto invalid=*binding;invalid.alpha_cutoff=std::nanf("");CHECK(!preview::valid_mesh_base_color_binding(invalid));
 invalid=*binding;invalid.alpha_cutoff=-0.1f;CHECK(!preview::valid_mesh_base_color_binding(invalid));
 invalid=*binding;invalid.alpha_cutoff=1.1f;CHECK(!preview::valid_mesh_base_color_binding(invalid));
 invalid=*binding;invalid.alpha_cutout=true;CHECK(preview::valid_mesh_base_color_binding(invalid));

}
void check_forest_shadow(const std::shared_ptr<preview::MeshBaseColorBinding>& binding){
 namespace light=epochengine::lighting;
 std::array<mesh::Vertex,9> vertices{};
 const std::array<std::array<float,3>,9> points{{
  {0,0,0},{.1f,0,0},{0,.1f,0},{3,0,0},{3.1f,0,0},{3,.1f,0},{-2,-2,1},{2,-2,1},{0,2,1}}};
 for(std::size_t i=0;i<vertices.size();++i){
  vertices[i].position=points[i];vertices[i].normal={0,0,i<3?1.0f:-1.0f};
  vertices[i].uv={.75f,.5f};vertices[i].color={1,1,1};
 }
 const std::array<std::uint32_t,9> indices{0,1,2,3,4,5,6,7,8};
 const auto mesh=mesh::build_artifact(vertices,indices);CHECK(mesh);
 preview::ObjectMarker marker;marker.primitive=preview::ObjectPreviewPrimitive::IndexedMesh;
 marker.mesh=mesh.artifact;marker.forestLighting=true;
 int context{};light::LightingFrame lighting;lighting.revision=1;
 lighting.environment.ambient={.1f,.1f,.1f};
 lighting.lights.push_back({.handle={0,1},.desc={.intensity=.7f,.direction={0,0,-1}}});
 preview::set_lighting_frame(&context,lighting);
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 const auto shadowed=preview::object_solid_geometry_for(&context);CHECK(shadowed->size()==9);
 for(std::size_t i=0;i<3;++i){
  CHECK(std::abs((*shadowed)[i].color.x-.1f)<1e-4f); // blocked by the front card
  CHECK(std::abs((*shadowed)[3+i].color.x-.8f)<1e-4f); // opposite leaf normals retain diffuse
 }
 auto cutout=std::make_shared<preview::MeshBaseColorBinding>(*binding);
 cutout->alpha_cutout=true;cutout->alpha_cutoff=1.0f;
 const auto& mip=cutout->artifact.artifact()->mips[0];
 CHECK(std::to_integer<unsigned>(mip.texels[4*mip.row_pitch_bytes+6*4+3])<255u);
 marker.baseColorTexture=cutout;
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 const auto transparent=preview::object_solid_frame_for(&context)->markers[0].textured;
 CHECK(transparent&&transparent->size()==9);
 for(std::size_t i=0;i<6;++i)CHECK(std::abs((*transparent)[i].color.x-.8f)<1e-4f);
 preview::cleanup_context(&context);
}
int main(){
 CHECK(transfer::texture_artifact_transfer_runtime_contract_failure()==transfer::TransferContractFailure::none);
 auto compiled=artifact();const auto original=compiled.mips[0].texels;
 auto sealed=transfer::seal_compiled_texture(compiled);CHECK(sealed);
 const canvas::LogicalTextureReference logical{71,transfer::compiled_artifact_revision(compiled.identity)};
 CHECK(transfer::make_artifact_key(sealed.artifact,logical).code==transfer::TransferCode::unsupported_format);
 CHECK(transfer::make_artifact_key(sealed.artifact,logical,0,transfer::TransferUsage::base_color));
 CHECK(sealed.artifact.artifact()->mips[0].texels==original);
 auto binding=std::make_shared<preview::MeshBaseColorBinding>();binding->artifact=sealed.artifact;binding->logical=logical;
 binding->sampler.min_filter=epochengine::FilterMode::linear;binding->sampler.address_u=epochengine::AddressMode::repeat;
 CHECK(preview::valid_mesh_base_color_binding(*binding));
 auto invalid=*binding;invalid.logical.artifact_revision++;CHECK(!preview::valid_mesh_base_color_binding(invalid));
 invalid=*binding;invalid.sampler.mipmapped=true;CHECK(!preview::valid_mesh_base_color_binding(invalid));
 invalid=*binding;invalid.sampler.maximum_anisotropy=2;CHECK(!preview::valid_mesh_base_color_binding(invalid));
 check_cutout(binding);
 check_forest_shadow(binding);
 std::array<mesh::Vertex,3> vertices{};
 vertices[0].position={0,0,0};vertices[1].position={1,0,0};vertices[2].position={0,1,0};
 for(auto& vertex:vertices){vertex.normal={0,0,1};vertex.color={0.25f,0.5f,0.75f};}
 vertices[0].uv={0,0};vertices[1].uv={2,0};vertices[2].uv={0,1};
 const std::array<std::uint32_t,6> indices{0,1,2,0,1,2};auto built=mesh::build_artifact(vertices,indices);CHECK(built);
 int context{};preview::ObjectMarker marker;marker.primitive=preview::ObjectPreviewPrimitive::IndexedMesh;
 marker.mesh=built.artifact;marker.position={3,4,5};marker.scale={2,3,4};
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 const auto ordinary=preview::object_solid_geometry_for(&context);CHECK(ordinary->size()==6);
 marker.baseColorTexture=binding;CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 const auto frame=preview::object_solid_frame_for(&context);CHECK(frame->geometry->empty());CHECK(frame->markers.size()==1);
 const auto textured=frame->markers[0].textured;CHECK(textured&&textured->size()==6);
 for(std::size_t i=0;i<indices.size();++i){const auto& value=(*textured)[i];const auto& source=vertices[indices[i-i%3u+(i%3u?3u-i%3u:0u)]];
  CHECK(value.uv==source.uv);CHECK(value.position.x==3+2*source.position[0]);
  CHECK(value.position.y==4+3*source.position[1]);CHECK(value.position.z==5+4*source.position[2]);
 }
 // Actual triangle order is clockwise for all preview consumers. UVs travel
 // with their source vertex, including repeated indices.
 const auto faces=[&](const auto& geometry,preview::Vec3 eye) {
  return preview::clockwise_solid_triangle_faces_camera(geometry[0].position,geometry[1].position,geometry[2].position,eye);
 };
 CHECK(faces(*ordinary,{3,4,15}));CHECK(!faces(*ordinary,{3,4,-5}));
 CHECK(faces(*textured,{3,4,15}));CHECK(!faces(*textured,{3,4,-5}));
 CHECK(built.artifact->indices()[1]==1u&&built.artifact->indices()[2]==2u);
 CHECK(preview::object_solid_frame_for(&context)==frame);
 CHECK(preview::object_solid_geometry_for(&context)->empty()); // no color-only substitute in other backends
 auto equivalent=std::make_shared<preview::MeshBaseColorBinding>(*binding);marker.baseColorTexture=equivalent;
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));CHECK(preview::object_solid_frame_for(&context)==frame);
 // Actual admission must invalidate the preview when imported color stops
 // multiplying authored fallback colors, while retaining normal lighting.
 auto unmodulated=std::make_shared<preview::MeshBaseColorBinding>(*binding);
 unmodulated->modulate_vertex_color=false;marker.baseColorTexture=unmodulated;
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 const auto unmodulatedFrame=preview::object_solid_frame_for(&context);CHECK(unmodulatedFrame!=frame);
 const auto imported=unmodulatedFrame->markers[0].textured;CHECK(imported&&imported->size()==textured->size());
 for(std::size_t i=0;i<imported->size();++i){
  const auto a=(*imported)[i].color,b=(*textured)[i].color;
  CHECK(std::abs((a.x-0.08f)*0.25f+0.08f-b.x)<1e-5f);CHECK(std::abs((a.y-0.08f)*0.5f+0.08f-b.y)<1e-5f);
  CHECK(std::abs((a.z-0.08f)*0.75f+0.08f-b.z)<1e-5f);
 }
 marker.baseColorTexture=std::make_shared<preview::MeshBaseColorBinding>(*unmodulated);
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 CHECK(preview::object_solid_frame_for(&context)==unmodulatedFrame);
 auto changed=std::make_shared<preview::MeshBaseColorBinding>(*binding);changed->sampler.address_u=epochengine::AddressMode::clamp_to_edge;
 marker.baseColorTexture=changed;CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));
 auto next=preview::object_solid_frame_for(&context);CHECK(next!=frame);CHECK(next->markers[0].marker.baseColorTexture==changed);
 marker.baseColorTexture=std::make_shared<preview::MeshBaseColorBinding>(invalid);
 CHECK(!preview::set_object_markers(&context,std::span{&marker,1u}));CHECK(preview::object_solid_frame_for(&context)==next);
 // Source and UV allocations are included in the one existing solid cache budget.
 auto large=artifact();large.mips[0].texels.reserve(preview::maximum_solid_cache_working_bytes+1);
 auto largeSeal=transfer::seal_compiled_texture(std::move(large));CHECK(largeSeal);
 auto overBudget=std::make_shared<preview::MeshBaseColorBinding>(*binding);overBudget->artifact=largeSeal.artifact;overBudget->logical.asset_key=72;
 marker.baseColorTexture=overBudget;CHECK(!preview::set_object_markers(&context,std::span{&marker,1u}));
 CHECK(preview::object_solid_frame_for(&context)==next);
 std::weak_ptr<const preview::MeshBaseColorBinding> retained=changed;
 changed.reset();marker.baseColorTexture.reset();next.reset();preview::clear_object_markers(&context);
 CHECK(retained.expired());CHECK(frame->markers[0].textured==textured); // existing reader remains valid through retirement
 CHECK(preview::set_object_markers(&context,std::span{&marker,1u}));CHECK(preview::object_solid_geometry_for(&context)->size()==6);
 preview::clear_object_markers(&context);
 // Closed wire meshes retain actual unlit triangle depth, independently from
 // lighting/camera caches; the legacy combined line consumer stays available.
 std::array<mesh::Vertex,8> cube{};
 for(unsigned i=0;i<8;++i) cube[i].position={i&1u?1.0f:-1.0f,i&2u?1.0f:-1.0f,i&4u?1.0f:-1.0f};
 const std::array<std::uint32_t,36> cubeIndices{0,1,3,0,3,2,4,7,5,4,6,7,0,4,5,0,5,1,2,3,7,2,7,6,0,2,6,0,6,4,1,5,7,1,7,3};
 const auto closed=mesh::build_artifact(cube,cubeIndices);CHECK(closed);
 preview::ObjectMarker wire;wire.mesh=closed.artifact;wire.primitive=preview::ObjectPreviewPrimitive::IndexedMesh;wire.wireframe=true;
 preview::ObjectMarker helper;helper.primitive=preview::ObjectPreviewPrimitive::Light;
 std::array markers{wire,helper};CHECK(preview::set_object_markers(&context,markers));
 const auto wf=preview::object_mesh_wire_frame_for(&context);
 CHECK(wf->depth_triangles&&wf->depth_triangles->size()==36&&!wf->helper_lines->empty());
 CHECK(preview::object_marker_geometry_for(&context)->size()==36u+wf->helper_lines->size());
 const auto depth=wf->depth_triangles;
 for(const auto& value:*depth) CHECK(value.color.x==wire.color.x&&value.color.y==wire.color.y&&value.color.z==wire.color.z);
 preview::reset_camera(&context);CHECK(preview::set_object_markers(&context,markers));CHECK(preview::object_mesh_wire_frame_for(&context)==wf);
 markers[0].selected=true;CHECK(preview::set_object_markers(&context,markers));CHECK(preview::object_mesh_wire_frame_for(&context)->depth_triangles!=depth);
 auto current=preview::object_mesh_wire_frame_for(&context);
 CHECK(!preview::wireframe_frame_admitted(markers,preview::maximum_wireframe_working_bytes));
 CHECK(!preview::set_object_markers(&context,markers,{.maximum_indices=1}));CHECK(preview::object_mesh_wire_frame_for(&context)==current);
 // Independent geometric reference: a ray to the back-face diagonal meets
 // the front triangles first. Without triangle depth the old line overlay
 // has no occluder. This is geometry evidence, not native pixel acceptance.
 const auto nearest=[&](const std::vector<preview::Vertex>& geometry) {
  float result=std::numeric_limits<float>::infinity();const preview::Vec3 eye{0,0,5},direction{0,0,-1};
  for(std::size_t i=0;i+2<geometry.size();i+=3) {
   const auto a=geometry[i].position,e1=preview::subtract(geometry[i+1].position,a),e2=preview::subtract(geometry[i+2].position,a);
   const auto p=preview::cross(direction,e2);const float det=preview::dot(e1,p);if(std::abs(det)<1e-6f)continue;
   const auto t=preview::subtract(eye,a);const float u=preview::dot(t,p)/det;if(u<0||u>1)continue;
   const auto q=preview::cross(t,e1);const float v=preview::dot(direction,q)/det;if(v<0||u+v>1)continue;
   const float distance=preview::dot(e2,q)/det;if(distance>0&&distance<result)result=distance;
  }return result;
 };
 CHECK(std::abs(nearest(*depth)-4.0f)<1e-5f);CHECK(6.0f>nearest(*depth));CHECK(!std::isfinite(nearest({})));
 markers[0].position={3,4,5};markers[0].rotationDegrees={15,27,39};markers[0].scale={2,3,4};
 CHECK(preview::set_object_markers(&context,markers));const auto moved=preview::object_mesh_wire_frame_for(&context);CHECK(moved->depth_triangles!=depth);
 markers[0].wireframe=false;CHECK(preview::set_object_markers(&context,markers));const auto solid=preview::object_solid_geometry_for(&context);CHECK(solid->size()>=36);
 for(unsigned i=0;i<36;++i) {const auto a=(*solid)[i].position,b=(*moved->depth_triangles)[i].position;CHECK(a.x==b.x&&a.y==b.y&&a.z==b.z);}
 preview::cleanup_context(&context);CHECK(!preview::object_mesh_wire_frame_for(&context)->depth_triangles);
 // Already-clockwise imports remain clockwise; reflected and precise transforms
 // preserve exterior visibility. Missing/opposing normals retain source CW order.
 preview::ObjectMarker orientation;orientation.primitive=preview::ObjectPreviewPrimitive::IndexedMesh;
 const std::array<std::uint32_t,3> cwIndices{0,2,1};
 auto cw=mesh::build_artifact(vertices,cwIndices);CHECK(cw);orientation.mesh=cw.artifact;
 CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));
 auto cwGeometry=preview::object_solid_geometry_for(&context);CHECK(faces(*cwGeometry,{0,0,5}));
 CHECK((*cwGeometry)[1].position.y==1);CHECK((*cwGeometry)[2].position.x==1);
 orientation.scale={-2,3,1};CHECK(!preview::set_object_markers(&context,std::span{&orientation,1u}));
 orientation.scale={2,3,1};orientation.rotationDegrees={0,180,30};
 CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));CHECK(faces(*preview::object_solid_geometry_for(&context),{0,0,-5}));
 orientation.preciseTransform=preview::make_precise_mesh_transform({0,0,0},{2,3,1},{0,180,30});CHECK(orientation.preciseTransform);
 for(unsigned row=0;row<3u;++row)orientation.preciseTransform->rotation[row*3u]*=-1.0;
 CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));
 CHECK(faces(*preview::object_solid_geometry_for(&context),{0,0,-5}));
 for(auto& vertex:vertices)vertex.normal={0,0,0};CHECK(!mesh::build_artifact(vertices,cwIndices));
 for(auto& vertex:vertices)vertex.normal={1,0,0};
 orientation.preciseTransform.reset();orientation.scale={1,1,1};orientation.rotationDegrees={};orientation.mesh=mesh::build_artifact(vertices,cwIndices).artifact;
 CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));CHECK(faces(*preview::object_solid_geometry_for(&context),{0,0,5}));
 vertices[0].normal={0,0,1};vertices[1].normal={0,0,1};vertices[2].normal={0,0,-2};orientation.mesh=mesh::build_artifact(vertices,cwIndices).artifact;
 CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));CHECK(faces(*preview::object_solid_geometry_for(&context),{0,0,5}));
 // Exercise the real planetary mesher on every cube face, not a reconstructed
 // winding guess. These source triangles are CCW with outward normal evidence.
 epoch::planet::Universe universe{73};auto body=universe.body(0);body.ocean=false;
 for(std::uint8_t face=0;face<6u;++face) {
  const auto patch=epoch::planet::build_tile(body,{0,0,0,face,0},{4,false,false});
  std::vector<mesh::Vertex> planetVertices;for(const auto& source:patch.vertices)
   planetVertices.push_back({{source.position.x,source.position.y,source.position.z},{source.normal.x,source.normal.y,source.normal.z}});
  orientation.mesh=mesh::build_artifact(planetVertices,patch.indices).artifact;CHECK(orientation.mesh);
  CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));
  const auto geometry=preview::object_solid_geometry_for(&context);CHECK(geometry->size()==patch.indices.size());
  for(std::size_t triangle=0;triangle<geometry->size();triangle+=3u) {
   preview::Vec3 normal{};for(unsigned corner=0;corner<3;++corner){const auto& n=patch.vertices[patch.indices[triangle+corner]].normal;normal=preview::add(normal,{n.x,n.y,n.z});}
   normal=preview::normalize(normal);const auto a=(*geometry)[triangle].position,b=(*geometry)[triangle+1].position,c=(*geometry)[triangle+2].position;
   const auto center=preview::scale(preview::add(preview::add(a,b),c),1.0f/3.0f);
   CHECK(preview::clockwise_solid_triangle_faces_camera(a,b,c,preview::add(center,preview::scale(normal,100))));
   CHECK(!preview::clockwise_solid_triangle_faces_camera(a,b,c,preview::subtract(center,preview::scale(normal,100))));
  }
  orientation.wireframe=true;CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));
  const auto wireDepth=preview::object_mesh_wire_frame_for(&context)->depth_triangles;CHECK(wireDepth->size()==geometry->size());
  for(std::size_t i=0;i<geometry->size();++i){const auto a=(*geometry)[i].position,b=(*wireDepth)[i].position;CHECK(a.x==b.x&&a.y==b.y&&a.z==b.z);}
  orientation.wireframe=false;
 }
 // LOD patches add skirts, unlike the initial coverage mesh. Their side walls
 // share radial surface normals; those normals cannot set each wall's winding.
 unsigned oldSkirtDisagreements{};
 for(std::uint8_t face=0;face<6u;++face) {
  const auto patch=epoch::planet::build_tile(body,{0,0,0,face,2},{16,false,true});
  CHECK(patch.vertices.size()==353&&patch.indices.size()==1920);
  std::vector<mesh::Vertex> source;
  for(const auto& vertex:patch.vertices)source.push_back({{vertex.position.x,vertex.position.y,vertex.position.z},
   {vertex.normal.x,vertex.normal.y,vertex.normal.z}});
  const auto built=mesh::build_artifact(source,patch.indices);CHECK(built);
  orientation.mesh=built.artifact;const auto originalKey=orientation.mesh->key();
  CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));
  const auto solid=preview::object_solid_geometry_for(&context);CHECK(solid->size()==patch.indices.size());
  for(std::size_t triangle=0;triangle<patch.indices.size();triangle+=3u) {
   for(std::size_t corner=0;corner<3u;++corner) {
    const auto& expected=source[patch.indices[triangle+(corner?3u-corner:corner)]].position;
    const auto actual=(*solid)[triangle+corner].position;
    CHECK(actual.x==expected[0]&&actual.y==expected[1]&&actual.z==expected[2]);
   }
   if(triangle>=1536u) {
    const auto& a=source[patch.indices[triangle]];const auto& b=source[patch.indices[triangle+1u]];const auto& c=source[patch.indices[triangle+2u]];
    const double abx=double(b.position[0])-a.position[0],aby=double(b.position[1])-a.position[1],abz=double(b.position[2])-a.position[2];
    const double acx=double(c.position[0])-a.position[0],acy=double(c.position[1])-a.position[1],acz=double(c.position[2])-a.position[2];
    const double nx=double(a.normal[0])+b.normal[0]+c.normal[0],ny=double(a.normal[1])+b.normal[1]+c.normal[1],nz=double(a.normal[2])+b.normal[2]+c.normal[2];
    if((aby*acz-abz*acy)*nx+(abz*acx-abx*acz)*ny+(abx*acy-aby*acx)*nz<=0)++oldSkirtDisagreements;
   }
  }
  orientation.wireframe=true;CHECK(preview::set_object_markers(&context,std::span{&orientation,1u}));
  const auto wire=preview::object_mesh_wire_frame_for(&context)->depth_triangles;CHECK(wire->size()==solid->size());
  for(std::size_t i=0;i<solid->size();++i) {
   const auto a=(*solid)[i].position,b=(*wire)[i].position;CHECK(a.x==b.x&&a.y==b.y&&a.z==b.z);
  }
  CHECK(orientation.mesh->key()==originalKey&&std::equal(orientation.mesh->indices().begin(),orientation.mesh->indices().end(),patch.indices.begin()));
  orientation.wireframe=false;
 }
 CHECK(oldSkirtDisagreements>0); // the previous per-triangle rule fails these real skirt faces
 preview::cleanup_context(&context);
 return 0;
}
'''


class MeshBaseColorTests(unittest.TestCase):
    def test_real_modules(self):
        proof = ROOT / "Artifacts/Reports/mesh-base-color-oct28" / str(time.time_ns())
        proof.mkdir(parents=True)
        (proof / "render.mesh_base_color_contract.cpp").write_text(consumer(), encoding="utf-8")
        owners = (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules")
        modules = {}
        for owner in owners:
            for path in owner.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    self.assertNotIn(match[1], modules)
                    modules[match[1]] = path
        pending, closure = ["render.preview_grid", "authoring.texture_detail"], set()
        while pending:
            name = pending.pop()
            if name in closure:
                continue
            closure.add(name)
            pending.extend(re.findall(r"^(?:export\s+)?import\s+([\w.:]+);", modules[name].read_text(encoding="utf-8-sig"), re.M))
        files = " ".join('"' + modules[name].as_posix() + '"' for name in sorted(closure))
        bases = " ".join('"' + path.as_posix() + '"' for path in owners)
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(MeshBaseColor LANGUAGES CXX)
add_executable(contract render.mesh_base_color_contract.cpp "{(ROOT / 'Engine/src/render/render.preview_camera.cpp').as_posix()}" "{(ROOT / 'Engine/src/render/render.preview_lighting.cpp').as_posix()}" "{(ROOT / 'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}" "{(ROOT / 'Engine/src/authoring/texture/authoring.texture_residency.cpp').as_posix()}")
target_sources(contract PRIVATE "{(ROOT / 'Engine/dep/EpochEngineExtensions/packages/planetary_terrain/upstream/EpochPlanet-0.2.3/src/core/terrain.cpp').as_posix()}" "{(ROOT / 'Engine/dep/EpochEngineExtensions/packages/planetary_terrain/upstream/EpochPlanet-0.2.3/src/core/world.cpp').as_posix()}")
target_include_directories(contract PRIVATE "{(ROOT / 'Engine/dep/EpochEngineExtensions/packages/planetary_terrain/upstream/EpochPlanet-0.2.3/include').as_posix()}")
target_sources(contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {bases} FILES {files})
target_include_directories(contract PRIVATE "{(ROOT / 'Engine/include').as_posix()}")
target_compile_features(contract PRIVATE cxx_std_23)
if(MSVC)
 target_compile_options(contract PRIVATE /permissive- /utf-8 /EHsc)
endif()
enable_testing()
add_test(NAME mesh_base_color COMMAND contract)
set_tests_properties(mesh_base_color PROPERTIES TIMEOUT 30)
''', encoding="utf-8")
        configure = ["cmake", "-S", str(proof), "-B", str(proof / "cmake")]
        configure += ["-G", "Visual Studio 17 2022", "-A", "x64", "-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"] if os.name == "nt" else ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
        phases = [("configure", configure)]
        for config in (("Debug", "Release") if os.name == "nt" else ("Release",)):
            phases += [(config + "-build", ["cmake", "--build", str(proof / "cmake"), "--config", config, "--parallel", "1"]),
                       (config + "-test", ["ctest", "--test-dir", str(proof / "cmake"), "-C", config, "--output-on-failure"])]
        results = []
        hashes = {modules[name].relative_to(ROOT).as_posix(): hashlib.sha256(modules[name].read_bytes()).hexdigest() for name in sorted(closure)}
        for label, command in phases:
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=240)
            (proof / (label + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
            results.append({"phase": label, "exit": run.returncode, "command": command})
            (proof / "result.json").write_text(json.dumps({"scope": "Actual modules: sealed sRGB default refusal/explicit transfer, UV/transform preservation, material cache invalidation, atomic refusal and retirement; no native draw proof", "phases": results, "modules": hashes}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(run.returncode, 0, (run.stdout + run.stderr)[-7000:])


if __name__ == "__main__":
    unittest.main()
