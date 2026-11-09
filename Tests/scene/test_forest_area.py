"""Actual bounded forest distribution, real geometry, scene history/serde/persistence."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest
from Tests.scene.test_position_anchor import CODE as ANCHOR_CODE

ROOT=Path(__file__).resolve().parents[2]
CODE=ANCHOR_CODE[:ANCHOR_CODE.index("int main(")]+r'''
import forest.factory;
namespace forest=epochengine::forest;
void distribution_contract() {
 forest::ForestAreaDistribution area;
 CHECK(forest::forest_area_candidate_count(area)==5);
 const std::array<std::array<double,3>,8> centers{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{1,2,3},{-3,2,-1}}};
 for(auto center:centers) {
  const auto length=std::hypot(center[0],center[1],center[2]);for(auto& v:center)v/=length;
  const auto directions=forest::forest_area_directions(area,center,6371000);
  CHECK(directions.size()==5&&directions==forest::forest_area_directions(area,center,6371000));
  auto other=area;++other.seed;CHECK(directions!=forest::forest_area_directions(other,center,6371000));
  for(auto direction:directions) {
   CHECK(std::abs(std::hypot(direction[0],direction[1],direction[2])-1)<1e-12);
   const auto distance=std::hypot(direction[0]-center[0],direction[1]-center[1],direction[2]-center[2])*6371000;
   CHECK(distance>0&&distance<=area.radiusMeters*1.000001);
  }
 }
 area.radiusMeters=1000;area.densityPerSquareMeter=1;
 CHECK(forest::forest_area_candidate_count(area)==64&&forest::forest_area_directions(area,{0,1,0},6371000).size()==64);
 area.radiusMeters=.25;area.densityPerSquareMeter=.001;
 const auto tiny=forest::forest_area_directions(area,{0,1,0},100000000);CHECK(tiny.size()==1&&std::hypot(tiny[0][0],tiny[0][2])>0);
 CHECK(forest::forest_area_directions(area,{0,0,0},6371000).empty());
 CHECK(forest::forest_area_directions(area,{0,1,0},.25).empty());
 area.densityPerSquareMeter=0;CHECK(!forest::forest_area_candidate_count(area));
 area.densityPerSquareMeter=1;area.radiusMeters=std::numeric_limits<double>::infinity();CHECK(!forest::forest_area_candidate_count(area));
 const auto compiled=forest::compile_forest_asset(forest::make_default_plant_lab_document());CHECK(compiled.valid);
 const auto mesh=forest::preview_mesh::build(compiled.graph,compiled.sample);CHECK(mesh.valid&&mesh.vertices.size()>100&&mesh.indices.size()>300);
 CHECK(std::any_of(mesh.vertices.begin(),mesh.vertices.end(),[](const auto& vertex){return vertex.material==forest::preview_mesh::MaterialSlot::wood;}));
 CHECK(std::any_of(mesh.vertices.begin(),mesh.vertices.end(),[](const auto& vertex){return vertex.material==forest::preview_mesh::MaterialSlot::foliage;}));
}
scene::SceneForestRegionInstance region(unsigned index) {
 scene::SceneForestRegionInstance r;
 r.region_id=9;r.terrain_object_id=2;r.seed=1337;r.plant_genome=21;r.plant_revision=3;r.plant_hash=42;
 r.candidate_index=index;r.candidate_count=64;r.radius_meters=12;r.density_per_square_meter=.2;
 r.body_radius_meters=6371000;r.center_direction={0,1,0};r.body_local_root_meters={0,6371001,0};r.terrain_artifact_key={1,2,3,4};
 r.provider_content_hash=std::string(64,'a');r.recipe="epoch.planet.recipe.v7\nseed=1337\n";
 r.plant_source="Assets/Forest/default.epoch_forest";return r;
}
int main(int argc,char** argv) {
 CHECK(argc==2);legacy_contract();distribution_contract();
 const fs::path root{argv[1]};fs::create_directories(root);
 auto snapshot=legacy_source();auto canonical=document::SceneDocument::from_snapshot(snapshot);CHECK(canonical);
 snapshot=canonical.value.project_snapshot().snapshot;
 auto built=document::SceneDocument::from_snapshot(snapshot);CHECK(built);
 auto doc=built.value;const auto originalHash=doc.revision().content;
 std::vector<document::SceneOperation> operations;
 for(unsigned i=0;i<64;++i) {
  scene::SceneObjectSnapshot tree;tree.id=100+i;tree.name="Tree"+std::to_string(i);tree.type="ForestTrunk";tree.category="ForestFactory";
  tree.position_anchor=std::array<double,3>{6371000.125+double(i),.25,.5};tree.forest_region=region(i);
  tree.mesh_asset=snapshot.objects.back().mesh_asset;
  const auto descriptor=document::descriptor_from_snapshot_object(tree,static_cast<std::uint32_t>(snapshot.objects.size())+i);CHECK(descriptor);
  operations.emplace_back(document::ObjectCreatedOperation{.object=*descriptor});
 }
 CHECK(doc.apply_transaction(operations,"Paint forest area"));CHECK(doc.objects().size()==snapshot.objects.size()+64);
 const auto paintedHash=doc.revision().content;CHECK(paintedHash!=originalHash);
 CHECK(doc.undo()&&doc.revision().content==originalHash);CHECK(doc.redo()&&doc.revision().content==paintedHash);
 const auto projected=doc.project_snapshot();CHECK(projected);
 const auto text=scene::serialize_snapshot_text(projected.snapshot);CHECK(text.starts_with("epoch_snapshot 8\n"));
 const auto parsed=scene::parse_snapshot_text(text);CHECK(parsed&&scene::serialize_snapshot_text(parsed.snapshot)==text);
 const auto* tree=scene::find_object_by_id(parsed.snapshot,100);CHECK(tree&&tree->forest_region==region(0)&&tree->position_anchor);
 CHECK(scene::persistence::save_scene_snapshot_atomic(root/"forest.epoch",projected.snapshot));
 const auto disk=scene::persistence::load_scene_snapshot(root/"forest.epoch");CHECK(disk.result);
 CHECK(scene::persistence::scene_snapshot_semantically_equal(projected.snapshot,disk.snapshot));
 auto reopened=document::SceneDocument::from_snapshot(disk.snapshot);CHECK(reopened&&reopened.value.revision().content==paintedHash);
 auto changed=disk.snapshot;++changed.objects.back().forest_region->seed;
 auto changedDoc=document::SceneDocument::from_snapshot(changed);CHECK(changedDoc&&changedDoc.value.revision().content!=paintedHash);
 CHECK(!scene::persistence::scene_snapshot_semantically_equal(changed,disk.snapshot));
 runtime::SceneRuntime runtime;CHECK(runtime.replace(disk.snapshot).committed());CHECK(runtime.projection()->find(100)->position_anchor==tree->position_anchor);CHECK(runtime.projection()->find(100)->forest_region==tree->forest_region);
 operations.clear();for(const auto& object:doc.objects())if(object.descriptor.forest_region)
  operations.emplace_back(document::ObjectDestroyedOperation{object.handle});
 CHECK(operations.size()==64&&doc.apply_transaction(operations,"Erase forest area"));
 CHECK(doc.objects().size()==snapshot.objects.size());CHECK(doc.undo()&&doc.revision().content==paintedHash);
 CHECK(doc.redo()&&doc.objects().size()==snapshot.objects.size());
 for(unsigned kind=0;kind<8;++kind) {
  auto bad=projected.snapshot;auto& r=*bad.objects.back().forest_region;
  if(kind==0)r.candidate_count=65;if(kind==1)r.candidate_index=64;if(kind==2)r.radius_meters=0;
  if(kind==3)r.density_per_square_meter=std::numeric_limits<double>::quiet_NaN();if(kind==4)r.center_direction={0,0,0};
  if(kind==5)r.provider_content_hash="bad";if(kind==6)r.plant_source="../outside";if(kind==7)r.region_id=0;
  CHECK(!scene::serialize_snapshot_text(bad).size()&&!document::SceneDocument::from_snapshot(bad));
 }
 CHECK(!scene::parse_snapshot_text(text+"forest_region 0\n"));
 CHECK(!scene::parse_snapshot_text(replace_once(text," forest_region 1 "," forest_region 2 ")));
 CHECK(!scene::parse_snapshot_text(replace_once(text,"epoch_snapshot 8\n","epoch_snapshot 7\n")));
 CHECK(scene::serialize_snapshot_text(snapshot).starts_with("epoch_snapshot 4\n"));
}
'''

class ForestArea(unittest.TestCase):
 def test_distribution_real_mesh_region_history_and_reopen(self):
  if os.name != "nt": self.skipTest("Installed MSVC CPU module lane")
  proof=ROOT/"Artifacts/Reports/forest-area-nov01";proof.mkdir(parents=True,exist_ok=True)
  receipt=proof/"result.json";receipt.write_text(json.dumps({"pass":False,"status":"running"})+"\n")
  build=ROOT/"Artifacts/Intermediate/tests/forest-area"
  source=proof/"forest.area_contract.cpp";source.write_text(CODE,encoding="utf-8")
  roots=[ROOT/"Engine/modules",ROOT/"Engine/dep/EpochPlatformEngine/modules"]
  found={}
  for folder in roots:
   for path in folder.glob("*.ixx"):
    match=re.search(r"^export module\s+([\w.:]+);",path.read_text(encoding="utf-8-sig"),re.M)
    if match: found[match.group(1)]=path
  selected={};pending=["scene.snapshot","scene.serializer","scene.document","scene.persistence","scene.runtime","scene.interaction","forest.preview_mesh","forest.factory"]
  while pending:
   name=pending.pop()
   if name in selected or name in ("std","std.compat"):continue
   self.assertIn(name,found);selected[name]=found[name]
   pending+=re.findall(r"^(?:export )?import\s+([\w.]+);",selected[name].read_text(encoding="utf-8-sig"),re.M)
  paths=lambda ps:" ".join('"'+p.as_posix()+'"' for p in ps)
  (proof/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ForestArea LANGUAGES CXX)
add_executable(anchor "{source.as_posix()}" "{(ROOT/'Engine/src/forest/forest.preview_mesh.cpp').as_posix()}")
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
