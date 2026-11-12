"""Actual bounded stream consumer and final admission, with recorded decoder ports."""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class PlanetaryFinalAdmission(unittest.TestCase):
    def test_one_final_source_scan_and_decode_races(self):
        owner = ROOT / 'Engine/src/project/project.planetary.cpp'
        source = owner.read_text(encoding='utf-8-sig')
        stream = definition(source, '    StreamResult stream_step(')
        start = stream.index('            if (cancellation.stop_requested()||recipe_guard(project_root)!=observedRecipe)')
        end = stream.index('            operation.ok = true;', start) + len('            operation.ok = true;')
        actual = stream[start:end]
        # Reconstruct only the preceding production admission placement. The
        # complete parser/decode/materialization block is identical in both.
        final_water = definition(actual, '            if(providerWater)')
        baseline = actual.replace(final_water, '')
        baseline = baseline.replace('if (cancellation.stop_requested()||recipe_guard(project_root)!=observedRecipe)',
            'if (!source_generation_is_current(project_root, *admitted, cancellation)||recipe_guard(project_root)!=observedRecipe)', 1)
        previous_water = '''            if(providerWater)
            {
                const auto current=package_source::load_project_attachment(project_root,"fft_ocean",cancellation);
                if(!current.source_attached()||current.content_hash!=water.content_hash)
                {operation.cancelled=cancellation.stop_requested();operation.message="Approved Water source changed during streaming. Worker evidence was retained; this snapshot was not accepted.";return result;}
            }
'''
        baseline = baseline.replace('            const auto receipt =', previous_water + '            const auto receipt =', 1)
        code = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return __LINE__;}}while(false)
std::string read(const fs::path& path){std::ifstream in{path,std::ios::binary};return {std::istreambuf_iterator<char>{in},{}};}
void write(const fs::path& path,const std::string& value){fs::create_directories(path.parent_path());std::ofstream out{path,std::ios::binary|std::ios::trunc};out<<value;}
std::atomic<unsigned> source_checks{},water_checks{},mesh_decodes{},metadata_decodes{},materializations{};
std::promise<void>* entered{};std::shared_future<void> release;unsigned hold_stage{};
bool malformed_mesh{},malformed_metadata{},refused_transition{},refused_materialize{},memory_refused{};
void decode_port(unsigned stage){if(entered&&hold_stage==stage){entered->set_value();release.wait();}}
namespace asset {
namespace mesh {
struct Limits{};
struct Artifact {std::array<int,3> vertices()const{return {};}std::array<int,3> indices()const{return {};}std::string key()const{return "artifact-key";}};
struct Decoded {std::shared_ptr<const Artifact> artifact;explicit operator bool()const{return bool(artifact);}};
Decoded read_artifact_file(const fs::path&,Limits){++mesh_decodes;decode_port(0);return {malformed_mesh?nullptr:std::make_shared<const Artifact>()};}
}
namespace texture {std::string content_hash_hex(const std::string& value){return value;}}
}
namespace project_planetary_snapshot {
SHARED_ORIGIN_HELPER
struct Identity{std::string key,source,recipe;std::uint64_t request;};
struct Limits {std::uint64_t maximum_metadata_bytes{1024};};
struct Cloud {bool density{};unsigned body{};double radius_meters{};};
struct Metadata {std::optional<Cloud> cloud;double water_radius_meters{10},water_meters_to_artifact{1};std::array<double,3> water_origin_meters{1,2,3};};
std::optional<Metadata> decode(std::span<const std::byte>,const asset::mesh::Artifact&,const Identity&,Limits,std::stop_token){
 ++metadata_decodes;decode_port(1);return malformed_metadata?std::nullopt:std::optional<Metadata>{Metadata{}};}
struct Snapshot {std::shared_ptr<const asset::mesh::Artifact> artifact;Metadata metadata;};
enum class InitialCut{selected_patches};
struct Transition {
 explicit Transition(Limits){}
 bool admit(const std::shared_ptr<const Snapshot>&,std::stop_token,InitialCut){return !refused_transition;}
 std::shared_ptr<const asset::mesh::Artifact> materialize(std::stop_token){++materializations;decode_port(2);return refused_materialize?nullptr:std::make_shared<const asset::mesh::Artifact>();}
};
}
namespace package_source {
struct Attachment {std::string content_hash;bool source_attached()const{return !content_hash.empty();}};
Attachment load_project_attachment(const fs::path& root,const char*,std::stop_token stop){++water_checks;return {stop.stop_requested()?"":read(root/"water")};}
}
struct Settings{bool ocean_surface{true};unsigned body_index{},resident_megabytes{128},size_reference_body{};double reference_radius_meters{10},patch_pixels{210},lod_hysteresis{.15};};
struct View{std::uint64_t request_id{42};std::array<double,3> camera{1,2,3};std::optional<double> simulation_seconds{1};std::optional<std::array<double,3>> forward{{0,0,-1}},up{{0,1,0}};double aspect_ratio{1};};
struct Source{fs::path project_root;std::string project_manifest,content_hash;};
struct Binding{std::string mesh{"mesh-a"};};
bool mesh_generation_receipt_matches(const fs::path& root,const Source&,const Settings&,const std::string& mesh){return read(root/"mesh")==mesh;}
struct GeneratedMeshFrame{double scale;std::array<double,3> origin;};
struct Operation {bool ok{},cancelled{};std::string message;fs::path artifact_path;std::optional<GeneratedMeshFrame> mesh_frame;};
struct StreamResult {
 Operation operation;bool waiting_for_worker{},snapshot_ready{};std::uint64_t request_id{},resident_bytes{},maximum_bytes{};
 unsigned built{},reused{},resident{},pending{},draw{},minimum_level{},maximum_level{};
 std::shared_ptr<const asset::mesh::Artifact> snapshot;std::shared_ptr<const project_planetary_snapshot::Snapshot> patch_snapshot;std::optional<Source> verified_source;
};
bool source_generation_is_current(const fs::path& root,const Source& pinned,std::stop_token stop){++source_checks;
 return !stop.stop_requested()&&root==pinned.project_root&&read(root/"manifest")==pinned.project_manifest&&read(root/"planet")==pinned.content_hash;}
std::optional<std::string> recipe_guard(const fs::path& root){return read(root/"recipe");}
std::string read_small(const fs::path& path,std::size_t maximum){const auto value=read(path);return value.size()<=maximum?value:std::string{};}
bool admit_preview_memory(const Settings&,Operation&){return !memory_refused;}
asset::mesh::Limits artifact_limits(const Settings&){return {};}
project_planetary_snapshot::Limits snapshot_transition_limits(const Settings&){return {};}
std::string receipt(){return "epoch.planet.stream.v7\nrequest_id=42\nsnapshot_ready=1\nbuilt=1\nreused=5\nresident=6\npending=0\ndraw=6\nmin_level=0\nmax_level=0\nresident_bytes=1024\nmaximum_bytes=134217728\nmeters_to_preview_units=1\nbody_radius_meters=10\nsource_origin_x_meters=1\nsource_origin_y_meters=2\nsource_origin_z_meters=3\nmesh_file=terrain.epoch_mesh\ncontent_key=artifact-key\nvertices=3\nindices=3\nocean_surface=1\nsimulation_seconds=1\npatch_file=terrain.epoch_planet_patch\n";}
void reset(const fs::path& root){
 write(root/"manifest","manifest-a");write(root/"planet","planet-a");write(root/"recipe","recipe-a");write(root/"water","water-a");write(root/"mesh","mesh-a");
 write(root/"generated/terrain.stream",receipt());write(root/"generated/terrain.epoch_planet_patch","metadata");
 source_checks=water_checks=mesh_decodes=metadata_decodes=materializations=0;
 malformed_mesh=malformed_metadata=refused_transition=refused_materialize=memory_refused=false;
 entered=nullptr;release={};
}
StreamResult consume(const fs::path& project_root,std::stop_token cancellation,bool old=false,bool providerWater=true){
 const auto run=project_root;const Settings settings;const View view;
 const bool footprintTransport=false,viewTransport=false,metricTransport=true,boundedMetricOrigin=false,patchTransport=true,customSize=false,customLod=false,meshFrame=true,cloudTransport=false,densityTransport=false;
 const std::optional<Source> admitted{Source{project_root,"manifest-a","planet-a"}};
 const std::optional<std::string> observedRecipe{"recipe-a"};const std::string recipeKey{"recipe-a"};
 const package_source::Attachment attached{"planet-a"},water{"water-a"};
 const Binding captured;const auto* binding=old?nullptr:&captured;
 [[maybe_unused]] double meshDecodeMs{},patchDecodeMs{},cutBuildMs{};
 const auto elapsedMs=[](Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
 StreamResult result;auto& operation=result.operation;
 if(old){BASELINE}else{ACTUAL}
 return result;
}
int main(){
 const auto root=fs::absolute("owned-admission-fixture");reset(root);
 auto result=consume(root,{},true);CHECK(result.operation.ok&&source_checks==2&&water_checks==1&&materializations==1);
 reset(root);result=consume(root,{});CHECK(result.operation.ok&&result.verified_source&&source_checks==1&&water_checks==1&&materializations==1);
 reset(root);result=consume(root,{},false,false);CHECK(result.operation.ok&&source_checks==1&&water_checks==0);
 // The actor mutates the real fixture only after the recorded decode port is
 // entered. This exercises the production block's final guards, not timing.
 for(unsigned stage=0;stage<3;++stage)for(unsigned mutation=0;mutation<6;++mutation){
  reset(root);std::promise<void> arrived,proceed;entered=&arrived;release=proceed.get_future().share();hold_stage=stage;
  std::stop_source stop;auto pending=std::async(std::launch::async,[&]{return consume(root,stop.get_token());});
  arrived.get_future().wait();
  if(mutation==4)stop.request_stop();
  else write(root/(mutation==0?"planet":mutation==1?"water":mutation==2?"manifest":mutation==3?"recipe":"mesh"),"changed");
  proceed.set_value();result=pending.get();
  CHECK(!result.operation.ok&&!result.snapshot_ready&&!result.snapshot&&!result.patch_snapshot&&!result.verified_source);
  CHECK(result.operation.cancelled==(mutation==4));
  reset(root);result=consume(root,{});CHECK(result.operation.ok&&result.snapshot&&result.patch_snapshot&&result.verified_source&&source_checks==1);
 }
 // The old Water guard ran before decode and demonstrably missed this race.
 reset(root);std::promise<void> arrived,proceed;entered=&arrived;release=proceed.get_future().share();hold_stage=1;
 auto old=std::async(std::launch::async,[&]{return consume(root,{},true);});arrived.get_future().wait();write(root/"water","changed");proceed.set_value();
 result=old.get();CHECK(result.operation.ok&&source_checks==2&&water_checks==1);
 for(unsigned malformed=0;malformed<9;++malformed){
  reset(root);
  if(malformed==0)write(root/"generated/terrain.stream","invalid");
  if(malformed==1){auto text=receipt();text.replace(text.find("request_id=42"),13,"request_id=43");write(root/"generated/terrain.stream",text);}
  if(malformed==2){auto text=receipt();text.replace(text.find("content_key=artifact-key"),24,"content_key=other-key");write(root/"generated/terrain.stream",text);}
  malformed_mesh=malformed==3;malformed_metadata=malformed==4;refused_transition=malformed==5;refused_materialize=malformed==6;memory_refused=malformed==7;
  if(malformed==8)write(root/"generated/terrain.stream",receipt()+"unexpected=1\n");
  result=consume(root,{});CHECK(!result.operation.ok&&!result.verified_source);
  reset(root);result=consume(root,{});CHECK(result.operation.ok&&source_checks==1&&water_checks==1);
 }
 reset(root);write(root/"recipe","changed");result=consume(root,{});CHECK(!result.operation.ok&&source_checks==0&&mesh_decodes==0);
 reset(root);std::stop_source stopped;stopped.request_stop();result=consume(root,stopped.get_token());CHECK(result.operation.cancelled&&!result.operation.ok&&source_checks==0&&mesh_decodes==0);
 std::puts("baseline_source_scans=2 candidate_source_scans=1; decode_stage_source_water_manifest_recipe_cancel_mesh_receipt_and_retry=18; malformed_refusal_and_retry=9; old_water_decode_race_reproduced=1");
}
'''.replace('BASELINE', baseline).replace('ACTUAL', actual)
        shared_origin = definition((ROOT / 'Engine/src/project/project.planetary_snapshot.cpp').read_text(encoding='utf-8-sig'),
            '    std::optional<std::array<double,3>> metric_source_origin(')
        code = code.replace('SHARED_ORIGIN_HELPER', shared_origin)
        proof = ROOT / 'Artifacts/Reports/planet-final-admission82-nov07'
        proof.mkdir(parents=True, exist_ok=True)
        cpp = proof / 'consumer.cpp'
        cpp.write_text(code, encoding='utf-8')
        runs = compile_and_run(self, cpp, proof)
        (proof / 'result.json').write_text(json.dumps(dict(passed=True,
            scope='Actual stream receipt/parser/decode/materialization and final admission block; recorded artifact/metadata/transition and source-verifier ports; real filesystem mutations synchronized at three decode stages; no provider/renderer/model',
            source_sha256=hashlib.sha256(owner.read_bytes()).hexdigest(),
            baseline_source_scans=2, candidate_source_scans=1,
            old_water_decode_race_reproduced=True, runs=runs), indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
