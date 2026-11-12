"""Actual persistent double instance pivot; CPU modules, no renderer/model."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest
from Tests.scene.test_mesh_coordinates import CODE as COORDINATE_CODE
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
PLAY_SCENE = (ROOT / 'Engine/src/epoch/implementation/epoch.project_play_scene.inl').read_text(encoding='utf-8')
SPAWN_FOCUS = definition(PLAY_SCENE, '            static void focus_project_spawn(')
CODE = COORDINATE_CODE[:COORDINATE_CODE.index("int main(")] + r'''
import scene.interaction;
import forest.preview_mesh;
namespace interaction=epochengine::scene_interaction;
namespace previewgrid {
struct Vec3 { float x{}, y{}, z{}; };
struct SpawnCamera {
 std::array<double, 3> eye{};
 bool precise{};
};
bool focus_camera(const void* context, Vec3 eye, float radius) {
 CHECK(radius == 0.f);
 auto& camera = *const_cast<SpawnCamera*>(static_cast<const SpawnCamera*>(context));
 camera = {{eye.x, eye.y, eye.z}, false};
 return true;
}
bool focus_camera_precise(const void* context, const std::array<double, 3>& eye, float radius) {
 CHECK(radius == 0.f);
 auto& camera = *const_cast<SpawnCamera*>(static_cast<const SpawnCamera*>(context));
 camera = {eye, true};
 return true;
}
}
ACTUAL_SPAWN_FOCUS
void project_spawn_contract(const fs::path& root) {
 auto source = legacy_source();
 source.objects.push_back({.id=4, .name="Primary Landing", .type="Spawn",
                           .position={0.03125f, 0.125f, -0.25f},
                           .position_anchor=std::array<double, 3>{6371000.125, 12.375, -0.25}});
 source.primary_spawn = 4;
 const auto authored = document::SceneDocument::from_snapshot(source);
 CHECK(authored);
 const auto projected = authored.value.project_snapshot();
 CHECK(projected && projected.snapshot.primary_spawn == 4);
 const auto path = root / "primary-spawn.epoch";
 CHECK(scene::persistence::save_scene_snapshot_atomic(path, projected.snapshot));
 const auto reopened = scene::persistence::load_scene_snapshot(path);
 CHECK(reopened.result && reopened.snapshot.primary_spawn == 4);
 CHECK(scene::persistence::scene_snapshot_semantically_equal(projected.snapshot, reopened.snapshot));
 runtime::SceneRuntime compiled;
 CHECK(compiled.replace(reopened.snapshot).committed());
 previewgrid::SpawnCamera camera;
 focus_project_spawn(&camera, *compiled.projection());
 CHECK(camera.precise);
 CHECK(camera.eye == std::array<double, 3>{6371000.15625, 12.5 + double(1.65f), -0.5});

 source.objects.back().position_anchor.reset();
 CHECK(compiled.replace(source).committed());
 focus_project_spawn(&camera, *compiled.projection());
 CHECK(!camera.precise);
 CHECK(camera.eye == std::array<double, 3>{0.03125, double(0.125f + 1.65f), -0.25});
}
void surface_rotation_contract() {
 namespace forest=epochengine::forest::preview_mesh;
 const auto matches=[](std::array<double,3> up,std::array<double,3> heading) {
  const auto rotation=forest::surface_rotation(up,heading);CHECK(rotation);
  // Renderer basis is Rx*Ry*Rz (apply Z, then Y, then X).
  // Independent analytic columns check the helper and selection against that contract.
  constexpr double radians=.01745329251994329576923690768489;
  const double x=(*rotation)[0]*radians,y=(*rotation)[1]*radians,z=(*rotation)[2]*radians;
  const double sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
  const std::array<double,3> localY{-cy*sz,cx*cz-sx*sy*sz,sx*cz+cx*sy*sz};
  const std::array<double,3> localZ{sy,-sx*cy,cx*cy};
  double norm{};for(std::size_t i=0;i<3;++i)norm+=up[i]*up[i];
  norm=std::sqrt(norm);for(auto& value:up)value/=norm;
  for(std::size_t i=0;i<3;++i)CHECK(std::abs(localY[i]-up[i])<1e-5);
  CHECK(std::abs(localZ[0]*up[0]+localZ[1]*up[1]+localZ[2]*up[2])<1e-5);
  double projection{};for(std::size_t i=0;i<3;++i)projection+=heading[i]*up[i];
  std::array<double,3> projected{};double square{};
  for(std::size_t i=0;i<3;++i){projected[i]=heading[i]-projection*up[i];square+=projected[i]*projected[i];}
  if(square>1e-15){const auto length=std::sqrt(square);
   CHECK((localZ[0]*projected[0]+localZ[1]*projected[1]+localZ[2]*projected[2])/length>.99999);}
  const auto indexedY=interaction::detail::rotate_xyz_degrees({0,1,0},*rotation);
  const auto indexedZ=interaction::detail::rotate_xyz_degrees({0,0,1},*rotation);
  CHECK(std::abs(double(indexedY.x)-localY[0])<1e-5&&std::abs(double(indexedY.y)-localY[1])<1e-5
   &&std::abs(double(indexedY.z)-localY[2])<1e-5);
  CHECK(std::abs(double(indexedZ.x)-localZ[0])<1e-5&&std::abs(double(indexedZ.y)-localZ[1])<1e-5
   &&std::abs(double(indexedZ.z)-localZ[2])<1e-5);
 };
 const std::array<std::array<double,3>,8> ups{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},
  {0,0,1},{0,0,-1},{1,2,3},{-3,2,-1}}};
 for(const auto& up:ups){matches(up,{.3,.4,.5});matches(up,up);}
 CHECK(!forest::surface_rotation({0,0,0},{0,0,1}));
 CHECK(!forest::surface_rotation({0,1,0},{0,0,0}));
 CHECK(!forest::surface_rotation({0,std::numeric_limits<double>::quiet_NaN(),0},{0,0,1}));
 CHECK(!forest::surface_rotation({0,1,0},{0,std::numeric_limits<double>::infinity(),1}));
}
void forest_parent_contract() {
 auto source=legacy_source();auto& parent=source.objects.back();
 parent.position={10,20,30};parent.rotation={};parent.scale={1,1,1};
 source.objects.push_back({.id=4,.name="AttachedTree",.type="StaticMesh",.category="Forest",
   .position={1,0,0},.position_anchor=std::array<double,3>{10,120,30}});
 auto& tree=source.objects.back();
 tree.forest_region=scene::SceneForestRegionInstance{.region_id=1,.terrain_object_id=3,.plant_genome=1,
  .plant_revision=1,.plant_hash=1,.candidate_count=1,.radius_meters=1,.density_per_square_meter=.1,
  .body_radius_meters=100,.center_direction={0,1,0},.body_local_root_meters={0,100,0},
  .terrain_artifact_key={1,2,3,4},.provider_content_hash=std::string(64,'a'),.recipe="planet",.plant_source="Assets/tree.json"};
 CHECK(tree.forest_region->valid());
 auto built=document::SceneDocument::from_snapshot(source);CHECK(built);auto doc=built.value;
 const auto handle=doc.find({3});CHECK(handle);
 const auto initial=doc.revision().content;
 const auto change=[&](document::TransformComponent transform){
  const std::array<document::SceneOperation,1> ops{document::ObjectTransformChangedOperation{*handle,transform}};
  return doc.apply_transaction(ops,"Parent move");};
 auto transform=doc.object(*handle)->descriptor.transform;
 transform.position.x=20;CHECK(change(transform));
 transform.rotation_degrees.z=90;CHECK(change(transform));
 transform.scale={2,2,2};CHECK(change(transform));
 auto snapshot=doc.project_snapshot();CHECK(snapshot);
 const auto* moved=scene::find_object_by_id(snapshot.snapshot,4);CHECK(moved);
 const auto world=scene::world_position(*moved);CHECK(world);
 CHECK(std::abs((*world)[0]+180)<1e-5 && std::abs((*world)[1]-22)<1e-5 && std::abs((*world)[2]-30)<1e-5);
 CHECK(std::abs(moved->rotation[2]-90)<1e-4 && moved->scale==scene::Vec3({2,2,2}));
 const auto finalHash=doc.revision().content;
 CHECK(doc.undo());CHECK(doc.undo());CHECK(doc.undo());CHECK(doc.revision().content==initial);
 CHECK(doc.redo());CHECK(doc.redo());CHECK(doc.redo());CHECK(doc.revision().content==finalHash);
 const auto encoded=scene::serialize_snapshot_text(snapshot.snapshot);
 const auto decoded=scene::parse_snapshot_text(encoded);CHECK(decoded);
 const auto reopened=document::SceneDocument::from_snapshot(decoded.snapshot);CHECK(reopened);
 const auto restored=reopened.value.project_snapshot();CHECK(restored);
 const auto* restoredTree=scene::find_object_by_id(restored.snapshot,4);CHECK(restoredTree);
 CHECK(restoredTree->position_anchor==moved->position_anchor && restoredTree->rotation==moved->rotation
  && restoredTree->scale==moved->scale && restoredTree->forest_region==moved->forest_region);
 transform.scale={2,3,2};CHECK(!change(transform));CHECK(doc.revision().content==finalHash);
 // Nonuniform scale remains valid for objects with no attached forest children.
 auto plain=document::SceneDocument::from_snapshot(legacy_source());CHECK(plain);
 const auto plainHandle=plain.value.find({3});CHECK(plainHandle);
 const std::array<document::SceneOperation,1> plainOps{document::ObjectTransformChangedOperation{*plainHandle,transform}};
 CHECK(plain.value.apply_transaction(plainOps,"Independent scale"));
}
void interaction_contract() {
 CHECK(interaction::run_scene_interaction_contract_checks());
 const std::array<double,3> anchor{6371000.123456789,-4567890.234567891,1234567.345678912};
 scene::SceneSnapshot source;source.scene_id="anchored-tree-picking";
 source.objects.push_back({.id=3,.name="Tree",.type="StaticMesh",.category="Forest",
  .position={.125f,.25f,-.5f},.rotation={0,0,90},.scale={2,3,4},.position_anchor=anchor});
 const std::array<interaction::SceneObjectInteractionOverride,1> overrides{{
  {.object={3,1},.localBounds={{-.5f,0,-.25f},{.5f,4,.25f}},.useLocalBounds=true}}};
 interaction::SceneInteractionBuildOptions options;options.coordinateOrigin=anchor;
 interaction::SceneInteractionQuery query;CHECK(query.rebuild(source,options,overrides).committed());
 const auto* record=query.resolve({3,1});CHECK(record&&record->queryable);
 const auto near=[](float a,float b){return std::abs(a-b)<1e-5f;};
 CHECK(near(record->worldBounds.minimum.x,-11.875f)&&near(record->worldBounds.minimum.y,-.75f)
  &&near(record->worldBounds.minimum.z,-1.5f));
 CHECK(near(record->worldBounds.maximum.x,.125f)&&near(record->worldBounds.maximum.y,1.25f)
  &&near(record->worldBounds.maximum.z,.5f));
 const auto focus=query.focus_target({3,1});CHECK(focus);
 CHECK(near(focus->target.x,-5.875f)&&near(focus->target.y,.25f)&&near(focus->target.z,-.5f));
 CHECK(focus->radius>6&&std::isfinite(focus->suggestedDistance)&&focus->objectCount==1);
 epochengine::ray::RayDesc ray;ray.origin={-5.875f,.25f,-10};ray.direction={0,0,1};
 const auto hit=query.trace(ray);CHECK(hit.has_hit()&&hit.hit.object==interaction::SceneObjectId{3,1});
 CHECK(near(hit.hit.distance,8.5f)&&near(hit.hit.position.z,-1.5f));
 // A sub-float-at-Earth-scale local edit must still move the exact local bounds.
 source.objects[0].position[0]+=.03125f;
 CHECK(query.rebuild(source,options,overrides).committed());
 CHECK(near(query.resolve({3,1})->worldBounds.maximum.x,.15625f));
 const auto movedFocus=query.focus_target({3,1});CHECK(movedFocus&&near(movedFocus->target.x,-5.84375f));
 // The default query frame remains exactly the previous zero-origin frame.
 auto legacy=source;legacy.objects[0].position_anchor.reset();
 interaction::SceneInteractionQuery implicitOrigin,explicitOrigin;
 CHECK(implicitOrigin.rebuild(legacy,{},overrides).committed());
 options.coordinateOrigin={};CHECK(explicitOrigin.rebuild(legacy,options,overrides).committed());
 const auto* implicit=implicitOrigin.resolve({3,1});const auto* explicitZero=explicitOrigin.resolve({3,1});
 CHECK(implicit&&explicitZero);
 CHECK(implicit->worldBounds.minimum.x==explicitZero->worldBounds.minimum.x
  &&implicit->worldBounds.minimum.y==explicitZero->worldBounds.minimum.y
  &&implicit->worldBounds.minimum.z==explicitZero->worldBounds.minimum.z
  &&implicit->worldBounds.maximum.x==explicitZero->worldBounds.maximum.x
  &&implicit->worldBounds.maximum.y==explicitZero->worldBounds.maximum.y
  &&implicit->worldBounds.maximum.z==explicitZero->worldBounds.maximum.z);
 // Invalid frame origins reject atomically and preserve the previously indexed tree.
 const auto oldRevision=query.interaction_revision();options.coordinateOrigin=anchor;
 options.coordinateOrigin[0]=std::numeric_limits<double>::quiet_NaN();
 CHECK(!query.rebuild(source,options,overrides).committed()&&query.interaction_revision()==oldRevision);
 CHECK(near(query.resolve({3,1})->worldBounds.maximum.x,.15625f));
}
int main(int argc,char** argv) {
 CHECK(argc==2);geometry_change_contract();legacy_contract();scale_contract();interaction_contract();surface_rotation_contract();forest_parent_contract();
 CHECK(runtime::run_scene_runtime_contract_checks());
 const fs::path root{argv[1]};fs::create_directories(root);
 project_spawn_contract(root);
 auto seed=legacy_source();const auto canonical=document::SceneDocument::from_snapshot(seed);CHECK(canonical);
 auto canonicalSnapshot=canonical.value.project_snapshot();CHECK(canonicalSnapshot);
 auto source=canonicalSnapshot.snapshot;auto& tree=source.objects.back();
 tree.position={0.125f,0.25f,-0.5f};tree.rotation={0,0,0};tree.scale={1,1,1};
 const std::array<double,3> anchor{6371000.123456789,-4567890.234567891,1234567.345678912};
 tree.position_anchor=anchor;
 const auto original=scene::world_position(tree);CHECK(original);
 for(std::size_t i=0;i<3;++i)CHECK(std::abs((*original)[i]-anchor[i]-double(tree.position[i]))<1e-9);
 CHECK(scene::validate_scene_document(source,scene::SceneDocumentRequirement::runnable));
 const auto built=document::SceneDocument::from_snapshot(source);CHECK(built);
 auto doc=built.value;const auto initialHash=doc.revision().content;
 auto changedAnchor=source;(*changedAnchor.objects.back().position_anchor)[0]+=.0625;
 const auto changedDoc=document::SceneDocument::from_snapshot(changedAnchor);CHECK(changedDoc);
 CHECK(changedDoc.value.revision().content!=initialHash);
 CHECK(!scene::persistence::scene_snapshot_semantically_equal(source,changedAnchor));
 const auto handle=doc.find({3});CHECK(handle);
 auto transformed=doc.object(*handle)->descriptor.transform;
 transformed.position.x+=.03125f;transformed.rotation_degrees={47,-83,121};transformed.scale={.5f,2,3};
 const std::array<document::SceneOperation,1> move{document::ObjectTransformChangedOperation{*handle,transformed}};
 CHECK(doc.apply_transaction(move,"Move/rotate/scale anchored tree"));
 const auto changed=doc.project_snapshot();CHECK(changed);
 const auto* moved=scene::find_object_by_id(changed.snapshot,3);CHECK(moved&&moved->position_anchor==anchor);
 const auto movedPivot=scene::world_position(*moved);CHECK(movedPivot);
 CHECK(std::abs((*movedPivot)[0]-(*original)[0]-.03125)<1e-9);
 CHECK((*movedPivot)[1]==(*original)[1]&&(*movedPivot)[2]==(*original)[2]);
 CHECK(doc.undo());const auto undone=doc.project_snapshot();CHECK(undone);
 CHECK(scene::world_position(*scene::find_object_by_id(undone.snapshot,3))==original);
 CHECK(doc.revision().content==initialHash);CHECK(doc.redo());
 const auto projected=doc.project_snapshot();CHECK(projected);
 const auto text=scene::serialize_snapshot_text(projected.snapshot);CHECK(text.starts_with("epoch_snapshot 7\n"));
 CHECK(text.find(" anchor 0\n")!=std::string::npos&&text.find(" anchor 1 meters ")!=std::string::npos);
 const auto parsed=scene::parse_snapshot_text(text);CHECK(parsed);
 const auto* read=scene::find_object_by_id(parsed.snapshot,3);CHECK(read&&read->position_anchor==anchor);
 CHECK(scene::world_position(*read)==movedPivot&&read->rotation==moved->rotation&&read->scale==moved->scale);
 CHECK(scene::serialize_snapshot_text(parsed.snapshot)==text);
 const auto reopened=document::SceneDocument::from_snapshot(parsed.snapshot);CHECK(reopened);
 CHECK(reopened.value.revision().content==doc.revision().content);
 const auto path=root/"anchored.epoch";CHECK(scene::persistence::save_scene_snapshot_atomic(path,projected.snapshot));
 const auto disk=scene::persistence::load_scene_snapshot(path);CHECK(disk.result);
 CHECK(scene::persistence::scene_snapshot_semantically_equal(projected.snapshot,disk.snapshot));
 runtime::SceneRuntime compiledRuntime;CHECK(compiledRuntime.replace(disk.snapshot).committed());
 const auto* runtimeTree=compiledRuntime.projection()->find(3);CHECK(runtimeTree);
 CHECK(runtimeTree->position_anchor==anchor&&runtimeTree->transform.position==read->position
  &&runtimeTree->transform.rotation==read->rotation&&runtimeTree->transform.scale==read->scale);
 CHECK(scene::world_position(runtimeTree->transform.position,runtimeTree->position_anchor)==movedPivot);
 const auto bytesBefore=bytes(path);
 for(double value:{1e15+1.0,-1e15-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
  auto bad=source;(*bad.objects.back().position_anchor)[0]=value;
  CHECK(!scene::world_position(bad.objects.back()));
  CHECK(!scene::serialize_snapshot_text(bad).size());CHECK(!document::SceneDocument::from_snapshot(bad));
  CHECK(!scene::persistence::save_scene_snapshot_atomic(path,bad));CHECK(bytes(path)==bytesBefore);
 }
 auto overflow=source;overflow.objects.back().position_anchor=std::array<double,3>{1e15,0,0};overflow.objects.back().position[0]=1;
 CHECK(!scene::world_position(overflow.objects.back()));
 CHECK(!scene::parse_snapshot_text(replace_once(text," anchor 1 meters "," anchor 2 meters ")));
 CHECK(!scene::parse_snapshot_text(replace_once(text," anchor 1 meters "," anchor 1 meters nan,")));
 CHECK(!scene::parse_snapshot_text(text+"anchor 0\n"));
 auto unanchored=legacy_source();auto v6=unanchored;v6.objects.back().mesh_asset->coordinates=scene::SceneMeshCoordinates{1,{1,2,3}};
 CHECK(scene::serialize_snapshot_text(v6).starts_with("epoch_snapshot 6\n"));
 CHECK(scene::parse_snapshot_text(scene::serialize_snapshot_text(v6)));
 auto mixed=source;mixed.objects.push_back(unanchored.objects.back());mixed.objects.back().id=4;mixed.objects.back().name="Plain";
 const auto mixedRead=scene::parse_snapshot_text(scene::serialize_snapshot_text(mixed));CHECK(mixedRead);
 CHECK(!scene::find_object_by_id(mixedRead.snapshot,4)->position_anchor);
 CHECK(scene::world_position(*scene::find_object_by_id(mixedRead.snapshot,4))==scene::world_position(unanchored.objects.back()));
}
'''


# Compile the real editor diff builder with the real reversible scene document.
_descriptor_ops=definition((ROOT/'Engine/src/editor/application/editor.application_scene_transactions.inl').read_text(encoding='utf-8'),
    '        void append_editor_descriptor_operations(')
_geometry_contract=r'''
namespace authoring=epochengine::authoring;
ACTUAL_DESCRIPTOR_OPS
void geometry_change_contract(){
 auto original=legacy_source();auto built=document::SceneDocument::from_snapshot(original);CHECK(built);
 auto doc=built.value;const auto handle=doc.find({3});CHECK(handle);
 const auto initial=doc.object(*handle)->descriptor;auto next=initial;
 next.mesh_asset->logical_path="Assets/Mesh/revised.epoch_mesh";next.mesh_asset->artifact_key={9,8,7,6};
 next.mesh_asset->vertex_count=6;next.mesh_asset->index_count=6;
 next.position_anchor=std::array<double,3>{6371000,0,0};
 next.texture_material=scene::SceneTextureMaterialSnapshot{.logical_path="Textures/Forest/bark",.artifact_key={1,2,3,4},.stable_key=123};
 scene::SceneForestRegionInstance region;region.region_id=1;region.terrain_object_id=2;region.plant_genome=1;region.plant_revision=2;region.plant_hash=3;
 region.candidate_count=1;region.radius_meters=12;region.density_per_square_meter=.01;region.body_radius_meters=6371000;
 region.center_direction={1,0,0};region.body_local_root_meters={6371000,0,0};region.terrain_artifact_key={1,2,3,4};
 region.provider_content_hash=std::string(64,'a');region.recipe="recipe";region.plant_source="Assets/Forest/default.epoch_forest";CHECK(region.valid());next.forest_region=region;
 std::vector<document::SceneOperation>ops;append_editor_descriptor_operations(ops,*doc.object(*handle),next);CHECK(ops.size()==2);
 CHECK(doc.apply_transaction(ops,"Update mesh and material"));const auto current=doc.object(*handle);CHECK(current);
 CHECK(current->descriptor.mesh_asset==next.mesh_asset&&current->descriptor.texture_material==next.texture_material&&current->descriptor.forest_region==next.forest_region);
 CHECK(current->descriptor.id==initial.id&&current->descriptor.transform==initial.transform);
 const auto saved=doc.project_snapshot();CHECK(saved);const auto text=scene::serialize_snapshot_text(saved.snapshot);const auto parsed=scene::parse_snapshot_text(text);CHECK(parsed);
 auto reopened=document::SceneDocument::from_snapshot(parsed.snapshot);CHECK(reopened);CHECK(reopened.value.object(*reopened.value.find({3}))->descriptor.mesh_asset==next.mesh_asset);
 CHECK(doc.undo());CHECK(doc.object(*handle)->descriptor.mesh_asset==initial.mesh_asset&&!doc.object(*handle)->descriptor.texture_material&&!doc.object(*handle)->descriptor.forest_region);
 CHECK(doc.redo());CHECK(doc.object(*handle)->descriptor.mesh_asset==next.mesh_asset&&doc.object(*handle)->descriptor.position_anchor==next.position_anchor);
 ops.clear();append_editor_descriptor_operations(ops,*doc.object(*handle),next);CHECK(ops.empty());
 auto bad=next;bad.position_anchor=std::array<double,3>{std::numeric_limits<double>::quiet_NaN(),0,0};
 append_editor_descriptor_operations(ops,*doc.object(*handle),bad);const auto revision=doc.revision().content;
 CHECK(!doc.apply_transaction(ops,"Reject invalid geometry"));CHECK(doc.revision().content==revision&&doc.object(*handle)->descriptor.mesh_asset==next.mesh_asset);
 for(unsigned fault=0;fault<2;++fault){auto rejected=next;if(fault==0)rejected.mesh_asset->logical_path="../outside.epoch_mesh";else rejected.forest_region->radius_meters=-1;
  ops.clear();append_editor_descriptor_operations(ops,*doc.object(*handle),rejected);
  CHECK(!doc.apply_transaction(ops,"Reject invalid payload"));CHECK(doc.revision().content==revision&&doc.object(*handle)->descriptor.forest_region==next.forest_region);}

}
'''.replace('ACTUAL_DESCRIPTOR_OPS',_descriptor_ops)
CODE=CODE.replace('ACTUAL_SPAWN_FOCUS', SPAWN_FOCUS)
CODE=CODE.replace('int main(int argc,char** argv)',_geometry_contract+'\nint main(int argc,char** argv)')

class ScenePositionAnchor(unittest.TestCase):
 def test_exact_pivot_legacy_codec_and_history(self):
  if os.name != "nt": self.skipTest("Installed MSVC CPU module lane")
  proof=ROOT/"Artifacts/Reports/scene-position-anchor-nov01";proof.mkdir(parents=True,exist_ok=True)
  receipt=proof/"result.json";receipt.write_text(json.dumps({"pass":False,"status":"running"})+"\n")
  build=ROOT/"Artifacts/Intermediate/tests/scene-position-anchor"
  source=proof/"scene.position_anchor_contract.cpp";source.write_text(CODE,encoding="utf-8")
  roots=[ROOT/"Engine/modules",ROOT/"Engine/dep/EpochPlatformEngine/modules"]
  found={}
  for folder in roots:
   for path in folder.rglob("*.ixx"):
    match=re.search(r"^export module\s+([\w.:]+);",path.read_text(encoding="utf-8-sig"),re.M)
    if match: found[match.group(1)]=path
  selected={};pending=["scene.snapshot","scene.serializer","scene.document","scene.persistence","scene.runtime","scene.interaction","forest.preview_mesh"]
  while pending:
   name=pending.pop()
   if name in selected or name in ("std","std.compat"):continue
   self.assertIn(name,found);selected[name]=found[name]
   pending+=re.findall(r"^(?:export )?import\s+([\w.]+);",selected[name].read_text(encoding="utf-8-sig"),re.M)
  paths=lambda ps:" ".join('"'+p.as_posix()+'"' for p in ps)
  (proof/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ScenePositionAnchor LANGUAGES CXX)
add_executable(anchor "{source.as_posix()}")
target_compile_features(anchor PRIVATE cxx_std_23)
target_compile_options(anchor PRIVATE /W3 /WX /utf-8 /Z7)
target_include_directories(anchor PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(anchor PRIVATE FILE_SET CXX_MODULES BASE_DIRS {paths(roots)} FILES {paths(selected.values())})
enable_testing()
add_test(NAME actual_scene_anchor COMMAND anchor "{(proof/'fixture').as_posix()}/$<CONFIG>")
''',encoding="utf-8")
  commands=[["cmake","-S",str(proof),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
  for config in ("Debug","Release"):
   commands += [["cmake","--build",str(build),"--config",config,"--parallel","1"],["ctest","--test-dir",str(build),"-C",config,"--output-on-failure"]]
  results=[]
  for index,command in enumerate(commands):
   completed=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=180)
   (proof/f"lane-{index}.log").write_text(completed.stdout+completed.stderr,encoding="utf-8")
   results.append({"command":command,"exit_code":completed.returncode})
   receipt.write_text(json.dumps({"pass":False,"status":"failed" if completed.returncode else "running","runs":results},indent=2)+"\n")
   self.assertEqual(completed.returncode,0,completed.stdout[-8500:]+completed.stderr)
  receipt.write_text(json.dumps({"pass":True,"scope":"Actual scene/document/serializer/persistence/interaction/runtime and forest orientation modules; no native renderer or Editor acceptance.","runs":results,"sources":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in selected.values()}},indent=2)+"\n")

if __name__ == "__main__":unittest.main()
