"""Actual planetary size codec/binding/transport admission, without graphics/models."""
from pathlib import Path
import hashlib
import copy
import json
import os
import subprocess
import time
import unittest
import shutil
import xml.etree.ElementTree as ET

from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]

CODE = r'''
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include "EDITION_HEADER"
import project.planetary;
import project.planetary_runtime;
import project.planetary_snapshot;
import asset.mesh_artifact;
import asset.texture_artifact;
import platform.filesystem;
import platform.child_process;
import package.source_install;
namespace planet=epochengine::project_planetary;
namespace runtime=epochengine::project_planetary_runtime;
namespace mesh=epochengine::asset::mesh;
namespace fs=std::filesystem;
namespace source_owner=epochengine::package_source;
namespace package_source=epochengine::package_source;
namespace project_planetary_snapshot=epochengine::project_planetary_snapshot;
namespace host {
 using namespace planet;
 namespace asset=epochengine::asset;
 namespace platform=epochengine::platform;
 namespace process=epochengine::platform::child_process;
 namespace fs=std::filesystem;
 std::atomic_uint64_t sequence{};
 FUNCTIONS
 planet::StreamResult admit_staged_only(const fs::path& project_root,const source_owner::ProjectAttachment& attached) {
  planet::StreamResult result;auto& operation=result.operation;fs::path producer,working;
  const std::string_view water_hash{}; // This fixture deliberately stages a dry provider.
  std::optional<process::ExecutableIdentity> expectedProvider;
  PRODUCER_NAMES
  ADMIT_STAGED
  operation.ok=true;operation.artifact_path=producer;return result;
 }
 planet::StreamResult parse_receipt(Settings settings,StreamView view,std::string receipt) {
  planet::StreamResult result;auto& operation=result.operation;
  constexpr bool patchTransport=false; // These cases cover the retained v1-v5 receipts.
  constexpr bool metricTransport=false;
  constexpr bool boundedMetricOrigin=false;
  const bool customLod=requires_lod_controls(settings),customSize=requires_size_controls(settings);
  PARSE_RECEIPT
  operation.ok=true;return result;
 }
}
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
int main(int argc,char**argv) {
 if(argc==4&&std::string_view{argv[1]}=="--body-stream") {
  const fs::path project=fs::absolute(argv[2]);const auto authored=planet::load_settings(project);CHECK(authored&&authored->ocean_surface);
  const auto before=host::read_small(project/planet::recipe_relative_path,1024);
  std::uint32_t body{};const std::string_view text{argv[3]};
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),body);CHECK(parsed.ec==std::errc{});
  const auto selected=planet::active_body_stream_settings(*authored,body);CHECK(selected);
  CHECK(selected->ocean_surface==(body==0u));
  planet::StreamView view;view.camera={0,26.5,0};view.request_id=1;
  const auto result=planet::stream_step(project,*selected,view);
  // An intentional CPU fixture exit proves the real stream request passed exact
  // provider admission. It does not claim terrain generation or native drawing.
  CHECK(!result.operation.ok&&!result.operation.build_log.empty());
  CHECK(host::read_small(result.operation.build_log,16384).find("exit=17")!=std::string::npos);
  CHECK(host::read_small(project/planet::recipe_relative_path,1024)==before);
  CHECK(!epochengine::platform::child_process::active_in_group("epoch.project.planet"));
  return 0;
 }
 if(argc==4&&std::string_view{argv[1]}.starts_with("--stage-")) {
  const fs::path sdk=fs::canonical(argv[2]),base=fs::absolute(argv[3]),project=base/"project";
  const std::string_view action=argv[1];
  if(action=="--stage-admit") {
   const auto attached=source_owner::load_project_attachment(project,"planetary_terrain");
   if(!attached.source_attached())return 71;
   const auto admitted=host::admit_staged_only(project,attached);
   std::ofstream(base/"admission.txt")<<admitted.operation.message<<'\n'<<admitted.operation.artifact_path.generic_string();
   return admitted.operation.ok?0:72;
  }
  if(action!="--stage-prepare"&&action!="--stage-update"&&action!="--stage-wet")return 73;
  const auto manifest=sdk/"Engine/dep/EpochEngineExtensions/packages/planetary_terrain/package.epoch-package.json";
  auto reviewed=source_owner::inspect_local_source(manifest);if(!reviewed.review_ready())return 74;
  if(action=="--stage-update") {
   const auto variant=base/"variant";
   for(const auto& file:reviewed.files) {const auto to=variant/file.relative_path;fs::create_directories(to.parent_path());
    if(!fs::copy_file(reviewed.repository_root/file.relative_path,to))return 75;}
   {std::ofstream edit(variant/reviewed.source_path/"CMakeLists.txt",std::ios::app);edit<<"\n# reviewed staging-contract source revision\n";}
   reviewed=source_owner::inspect_local_source(variant/reviewed.source_path/"package.epoch-package.json");if(!reviewed.review_ready())return 76;
  } else {fs::create_directories(project);std::ofstream(project/"project.epoch.json")<<"{\"id\":\"provider-stage-contract\"}\n";}
  const auto staged=source_owner::stage_local_source(reviewed,reviewed.content_hash,base/"cache");if(!staged.source_staged()){std::ofstream(base/"preparation-error.txt")<<staged.message;return 77;}
  const auto previous=source_owner::load_project_attachment(project,"planetary_terrain");
  const auto attached=action=="--stage-update"?source_owner::update_project_source(reviewed,staged,reviewed.content_hash,project,previous.content_hash)
   :source_owner::attach_project_source(reviewed,staged,reviewed.content_hash,project);
  if(!attached.source_attached()){std::ofstream(base/"preparation-error.txt")<<attached.message;return 78;}
  if(action=="--stage-wet") {
   const auto waterManifest=sdk/"Engine/dep/EpochEngineExtensions/packages/fft_ocean/package.epoch-package.json";
   const auto waterReview=source_owner::inspect_local_source(waterManifest);if(!waterReview.review_ready())return 81;
   const auto waterStage=source_owner::stage_local_source(waterReview,waterReview.content_hash,base/"water-cache");if(!waterStage.source_staged())return 82;
   if(!source_owner::attach_project_source(waterReview,waterStage,waterReview.content_hash,project).source_attached())return 83;
  }
  if(action=="--stage-prepare"||action=="--stage-wet"){planet::Settings settings;settings.cells_per_tile=4;settings.ocean_surface=action=="--stage-wet";if(!planet::save_settings(project,settings))return 79;}
  const auto prepared=planet::prepare_runtime_dependency(project);if(!prepared.ok){std::ofstream(base/"preparation-error.txt")<<prepared.message;return 80;}
  return 0;
 }
 // Retained real provider exports can be fed through these same production
 // parsers without regenerating terrain or starting an Editor/model.
 if(argc==3&&std::string_view{argv[1]}=="--provider-static") {
  planet::Settings expected;expected.cells_per_tile=4;expected.size_reference_body=5;expected.reference_radius_meters=25000;
  const auto directory=std::filesystem::path{argv[2]};
  const auto artifact=mesh::read_artifact_file(directory/"terrain.epoch_mesh",planet::artifact_limits(expected));CHECK(artifact);
  CHECK(host::admit_generated_mesh_frame(host::read_small(directory/"terrain.frame",4096),expected,*artifact.artifact));
  auto foreign=expected;foreign.reference_radius_meters=50000;
  CHECK(!host::admit_generated_mesh_frame(host::read_small(directory/"terrain.frame",4096),foreign,*artifact.artifact));
  return 0;
 }
 if(argc==4&&std::string_view{argv[1]}=="--provider-stream") {
  planet::Settings expected;expected.cells_per_tile=4;expected.maximum_level=12;expected.maximum_leaves=96;
  expected.resident_megabytes=64;expected.size_reference_body=5;expected.reference_radius_meters=25000;
  planet::StreamView view;view.viewport_height=1080;view.camera={0,2500,0};
  const std::string_view id{argv[3]};const auto parsed=std::from_chars(id.data(),id.data()+id.size(),view.request_id);
  CHECK(parsed.ec==std::errc{}&&parsed.ptr==id.data()+id.size());
  if(view.request_id==5)view.simulation_seconds=12345.125;
  const auto receipt=host::read_small(std::filesystem::path{argv[2]}/"terrain.stream",4096);
  CHECK(host::parse_receipt(expected,view,receipt).operation.ok);
  auto foreign=expected;foreign.size_reference_body=0;CHECK(!host::parse_receipt(foreign,view,receipt).operation.ok);
  return 0;
 }
 CHECK(argc==2);const auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
 runtime::Binding binding{.project_root=root.generic_string(),.project_id="size-contract",
  .source_hash="sha256:"+std::string(64,'a')};
 CHECK(runtime::valid_binding(binding));
 auto custom=binding;custom.size_reference_body=5;custom.reference_radius_meters=25000;
 CHECK(runtime::valid_binding(custom)&&custom!=binding);
 CHECK(runtime::bind_admitted_project(custom)&&!runtime::bind_admitted_project(binding));
 for(double radius:{249.,100000001.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
  auto invalid=custom;invalid.reference_radius_meters=radius;CHECK(!runtime::valid_binding(invalid));
 }
 auto invalid=custom;invalid.size_reference_body=6;CHECK(!runtime::valid_binding(invalid));
 planet::Settings settings;settings.size_reference_body=5;settings.reference_radius_meters=25000;
 std::vector<std::string> args{"planet","output"};host::append_size_arguments(args,settings);
 CHECK(args==std::vector<std::string>({"planet","output","--size-reference-body","5","--reference-radius-meters","25000"}));
 std::vector<std::string> oldArgs{"planet","output"};host::append_size_arguments(oldArgs,{});CHECK(oldArgs.size()==2);
 CHECK(host::stream_cache_identity({})=="1337-16-0");
 const auto cache=host::stream_cache_identity(settings);CHECK(cache=="1337-16-0-size-5-25000");
 auto changed=settings;changed.reference_radius_meters=25000.000000000004;CHECK(host::stream_cache_identity(changed)!=cache);
 changed=settings;changed.size_reference_body=4;CHECK(host::stream_cache_identity(changed)!=cache);
 changed=settings;changed.body_index=3;CHECK(host::stream_cache_identity(changed)!=cache);
 const auto provider=root/"source";std::filesystem::create_directories(provider/"tools");
 {std::ofstream out(provider/"tools/planet.mesh_export.cpp");out<<"--size-reference-body --reference-radius-meters epoch.planet.mesh-frame.v1";}
 {std::ofstream out(provider/"tools/planet.stream_export.cpp");out<<"epoch.planet.stream.v5";}
 CHECK(host::source_supports_size_controls(provider)&&host::source_supports_mesh_frame(provider));
 {std::ofstream out(provider/"tools/planet.mesh_export.cpp");out<<"--size-reference-body";}
 CHECK(!host::source_supports_size_controls(provider)&&!host::source_supports_mesh_frame(provider));
 CHECK(!host::source_supports_original_sunlight(provider));
 std::filesystem::create_directories(provider/"src");
 {std::ofstream out(provider/"src/planet.exploration_runtime.cpp");out<<"epoch.planet.exploration.v3 binding_.original_sunlight frame.sunlight=";}
 CHECK(host::source_supports_original_sunlight(provider));
 {std::ofstream out(provider/"src/planet.exploration_runtime.cpp");out<<"epoch.planet.exploration.v2 binding_.original_sunlight";}
 CHECK(!host::source_supports_original_sunlight(provider));
 {std::ofstream out(provider/"tools/planet.mesh_export.cpp");out<<std::string(65537,'x')<<" --reference-radius-meters epoch.planet.mesh-frame.v1";}
 CHECK(!host::source_supports_size_controls(provider)&&!host::source_supports_mesh_frame(provider));
 std::array<mesh::Vertex,3> vertices{};vertices[1].position={1,0,0};vertices[2].position={0,1,0};
 const std::array<std::uint32_t,3> indices{0,1,2};const auto artifact=mesh::build_artifact(vertices,indices);CHECK(artifact);
 const auto key=epochengine::asset::texture::content_hash_hex(artifact.artifact->key());
 const auto frame="epoch.planet.mesh-frame.v1\nmode=planet\nseed=1337\nbody_index=0\ncells_per_tile=16\ntiles_per_axis=4\ntile_span_meters=128\nseconds=0\nsize_reference_body=5\nreference_radius_meters=25000\nmeters_to_preview_units=0.0004\norigin_meters=1000000000,-2000000000,3000000000\ncontent_key="+key+"\nvertices=3\nindices=3\n";
 const auto admitted=host::admit_generated_mesh_frame(frame,settings,*artifact.artifact);
 CHECK(admitted&&std::abs(admitted->meters_per_unit-2500)<1e-9&&admitted->source_origin_meters[1]==-2000000000.0);
 const auto replace=[](std::string text,std::string_view from,std::string_view to) {
  const auto at=text.find(from);if(at==std::string::npos)return std::string{};text.replace(at,from.size(),to);return text;
 };
 for(const auto& bad:std::vector<std::string>{frame+"seed=1337\n",frame.substr(0,frame.size()-1),
  replace(frame,"size_reference_body=5","size_reference_body=4"),replace(frame,"reference_radius_meters=25000","reference_radius_meters=25001"),
  replace(frame,"mode=planet","mode=universe"),replace(frame,"seconds=0","seconds=1"),replace(frame,"vertices=3","vertices=4"),
  replace(frame,"meters_to_preview_units=0.0004","meters_to_preview_units=nan"),
  replace(frame,"meters_to_preview_units=0.0004","meters_to_preview_units=0"),
  replace(frame,"meters_to_preview_units=0.0004","meters_to_preview_units=1e-13"),
  replace(frame,"origin_meters=1000000000,-2000000000,3000000000","origin_meters=0,0,0,0"),
  replace(frame,"origin_meters=1000000000,-2000000000,3000000000","origin_meters=1e16,0,0"),
  replace(frame,"content_key="+key,"content_key="+std::string(64,'0')),std::string(4097,'x')})
  CHECK(!host::admit_generated_mesh_frame(bad,settings,*artifact.artifact));
 CHECK(!host::admit_generated_mesh_frame(frame,{},*artifact.artifact));
 planet::StreamView view;view.request_id=91;
 const std::string receipt="epoch.planet.stream.v5\nrequest_id=91\nsnapshot_ready=0\nbuilt=0\nreused=0\nresident=0\npending=6\ndraw=0\nmin_level=0\nmax_level=0\nresident_bytes=0\nmaximum_bytes=16777216\nmeters_to_preview_units=0.0004\nmesh_file=-\ncontent_key=-\nvertices=0\nindices=0\nocean_surface=0\nsimulation_seconds=0\npatch_pixels=210\nlod_hysteresis=0.14999999999999999\nsize_reference_body=5\nreference_radius_meters=25000\n";
 CHECK(host::parse_receipt(settings,view,receipt).operation.ok);
 for(const auto& bad:std::vector<std::string>{replace(receipt,"size_reference_body=5","size_reference_body=0"),
  replace(receipt,"reference_radius_meters=25000","reference_radius_meters=25001"),
  replace(receipt,"reference_radius_meters=25000","reference_radius_meters=nan"),receipt+"seed=1337\n",
  replace(receipt,"request_id=91","request_id=90"),replace(receipt,"epoch.planet.stream.v5","epoch.planet.stream.v4")})
  CHECK(!host::parse_receipt(settings,view,bad).operation.ok);
 CHECK(!host::parse_receipt({},view,receipt).operation.ok);
 CHECK(host::supported_session_envelope("epoch.planet.exploration.v1\n"));
 CHECK(host::supported_session_envelope("epoch.planet.exploration.v2\n"));
 CHECK(host::supported_session_envelope("epoch.planet.exploration.v3\n"));
 CHECK(host::supported_session_envelope("epoch.planet.exploration.v4\n"));
 CHECK(!host::supported_session_envelope("epoch.planet.exploration.v5\n"));
 CHECK(!host::supported_session_envelope("epoch.planet.exploration.v2\n"+std::string(16384,'x')));
 const auto project=root/"project";std::filesystem::create_directories(project);
 {std::ofstream manifest(project/"project.epoch.json");manifest<<"{\"id\":\"size-cas\"}\n";}
 const auto absent=host::recipe_guard(project);CHECK(absent&&absent->empty());
 CHECK(host::save_settings_observed(project,settings,&absent));
 const auto saved=host::recipe_guard(project);CHECK(saved&&*saved==planet::encode_settings(settings));
 const auto lockPath=project/"Assets/Worlds/Planet.epoch_planet.lock";
 auto held=epochengine::platform::filesystem::ExclusiveFileLease::acquire(lockPath);CHECK(held);
 auto revised=settings;revised.reference_radius_meters=50000;
 CHECK(!planet::save_settings(project,revised)&&host::recipe_guard(project)==saved);
 CHECK(!host::save_settings_observed(project,revised,&saved)&&host::recipe_guard(project)==saved);
 held.reset();CHECK(host::save_settings_observed(project,revised,&saved));
 const auto latest=host::recipe_guard(project);CHECK(latest&&*latest==planet::encode_settings(revised));
 CHECK(planet::decode_settings(*latest)==revised);
 CHECK(!host::save_settings_observed(project,settings,&saved)&&host::recipe_guard(project)==latest);
 std::stop_source cancelled;cancelled.request_stop();
 CHECK(!host::save_settings_observed(project,settings,&latest,cancelled.get_token())&&host::recipe_guard(project)==latest);
 CHECK(!host::save_settings_observed(project,settings,&absent)&&host::recipe_guard(project)==latest);
 auto refused=settings;refused.reference_radius_meters=0;
 CHECK(!host::save_settings(project,refused)&&host::recipe_guard(project)==latest);
 const auto linkedProject=root/"linked-project";
 std::filesystem::create_directories(linkedProject/"Assets/Worlds");
 {std::ofstream manifest(linkedProject/"project.epoch.json");manifest<<"{\"id\":\"linked-refusal\"}\n";}
 const auto shared=root/"shared-recipe";
 {std::ofstream content(shared,std::ios::binary);content<<*latest;}
 const auto linkedRecipe=linkedProject/"Assets/Worlds/Planet.epoch_planet";
 std::filesystem::create_hard_link(shared,linkedRecipe);
 CHECK(std::filesystem::hard_link_count(linkedRecipe)==2);
 CHECK(!planet::save_settings(linkedProject,settings));
 const auto retained=[](const auto& path) {
  std::ifstream input(path,std::ios::binary);return std::string{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
 };
 CHECK(retained(shared)==*latest&&retained(linkedRecipe)==*latest
  &&std::filesystem::hard_link_count(linkedRecipe)==2);
 const auto redirectedProject=root/"redirected-project";
 std::filesystem::create_directories(redirectedProject/"Assets");
 {std::ofstream manifest(redirectedProject/"project.epoch.json");manifest<<"{\"id\":\"redirected-refusal\"}\n";}
 // A directory junction is an actual Windows redirect and requires no
 // Developer-Mode symbolic-link privilege. The Python runner creates it.
 const auto redirect=redirectedProject/"Assets/Worlds";
 CHECK(std::filesystem::exists(redirect));CHECK(!planet::save_settings(redirectedProject,settings));
 CHECK(std::filesystem::is_empty(root/"redirect-target"));
 return 0;
}
'''


class PlanetarySizeSettings(unittest.TestCase):
    def test_source_versioned_runtime_stage_and_admission(self):
        provider=os.environ.get('EPOCH_TEST_PLANET_PROVIDER')
        if os.name!='nt' or not provider or not Path(provider).is_file():
            self.skipTest('Explicit retained native producer required; staging/admission only, never executed')
        self._stage_provider=Path(provider).resolve()
        self.test_actual_size_codec_binding_and_transport(stage_only=True)

    def _stage_cases(self, build, report):
        runs=[]
        def run(command, expected=0, timeout=60):
            result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,timeout=timeout)
            log=report/f'stage-{len(runs)}.log';log.write_text(result.stdout+result.stderr,encoding='utf-8')
            runs.append({'command':[str(x) for x in command],'exit':result.returncode,'log':str(log)})
            self.assertEqual(result.returncode,expected,result.stdout[-5000:]+result.stderr[-3000:])
            return result.stdout+result.stderr
        for config in ('Debug','Release'):
            base=ROOT/'Artifacts/pst'/f'{time.time_ns()}-{config}';project=base/'project'
            exe=build/config/'size_admission.exe'
            run([exe,'--stage-prepare',ROOT,base])
            packages=project/'Packages';script=packages/'epoch.stage_planet.cmake'
            dependency=packages/'planetary_terrain/project.dependency.json'
            first=json.loads(dependency.read_text())['content_hash']
            producer=project/'build/input'/config/'epoch_planet_mesh_export.exe'
            producer.parent.mkdir(parents=True);shutil.copy2(self._stage_provider,producer)
            command=['cmake',f'-DEPOCH_PLANET_PROVIDER={producer}',f'-DEPOCH_PLANET_CONFIGURATION={config}','-P',script]
            run(command)
            runtime=packages/'.runtime/planetary_terrain/development'
            original=runtime/f'{config}-{first[7:19]}'
            before={p.name:p.read_bytes() for p in original.iterdir()}
            legacy=runtime/config;legacy.mkdir()
            for name,data in before.items():(legacy/name).write_bytes(data)
            run([exe,'--stage-admit',ROOT,base])
            run([exe,'--stage-update',ROOT,base])
            second=json.loads(dependency.read_text())['content_hash'];self.assertNotEqual(first,second)
            run(command)
            current=runtime/f'{config}-{second[7:19]}'
            self.assertNotEqual(current,original)
            run([exe,'--stage-admit',ROOT,base])
            self.assertIn(current.as_posix(),(base/'admission.txt').read_text().replace('\\','/'))
            identity=current/'provider.identity';correct=identity.read_bytes()
            identity.write_bytes(correct.replace(second.encode(),first.encode()))
            run([exe,'--stage-admit',ROOT,base],72)
            # Invoke the actual emitted staging Exec+Error nodes under the same
            # ErrorsOnly logger that hid the original native failure. No
            # configure/compile Execs are copied or run by this fixture.
            ns='{http://schemas.microsoft.com/developer/msbuild/2003}'
            ET.register_namespace('',ns[1:-1])
            emitted=ET.parse(packages/'epoch.dependencies.props').getroot()
            target=next(x for x in emitted if x.tag==ns+'Target')
            root=ET.Element(ns+'Project',{'DefaultTargets':'StageOnly'})
            group=ET.SubElement(root,ns+'PropertyGroup')
            for name,value in {'EpochPlanetBuild':str(project/'build/input'),'EpochPlanetStageInput':str(project/'build/input'/config),'EpochPlanetConfiguration':config,'EpochPlanetRuntimeConfiguration':config}.items():
                ET.SubElement(group,ns+name).text=value
            stage=ET.SubElement(root,ns+'Target',{'Name':'StageOnly'})
            for node in list(target)[-2:]:stage.append(copy.deepcopy(node))
            msproject=packages/'stage-only.proj';ET.ElementTree(root).write(msproject,encoding='utf-8',xml_declaration=True)
            msbuild=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe')
            output=run([msbuild,msproject,'/nologo','/v:minimal','/clp:ErrorsOnly'],1)
            self.assertIn('existing planetary runtime is not the admitted managed provider',output)
            self.assertIn('Planetary provider staging failed:',output)
            self.assertEqual(identity.read_bytes(),correct.replace(second.encode(),first.encode()))
            identity.write_bytes(correct)
            unmanaged=current/'unmanaged.txt';unmanaged.write_text('untouched')
            run([exe,'--stage-admit',ROOT,base],72)
            output=run(command,1);self.assertIn('Unmanaged files exist',output)
            self.assertEqual(unmanaged.read_text(),'untouched');unmanaged.unlink()
            run([exe,'--stage-admit',ROOT,base])
            self.assertEqual({p.name:p.read_bytes() for p in original.iterdir()},before)
            self.assertEqual({p.name:p.read_bytes() for p in legacy.iterdir()},before)
        # Use the actual emitted MSBuild dependency target in a project path as
        # deep as an isolated Editor project. Compiler outputs must stay short;
        # the complete project/source/edition cache receipt still owns reuse.
        base=ROOT/'Artifacts/pst'/f'{time.time_ns()}-link'
        base=base/('p'*max(1,144-len(str(base/'project'))))
        project=base/'project'
        run([build/'Release/size_admission.exe','--stage-prepare',ROOT,base])
        ns='{http://schemas.microsoft.com/developer/msbuild/2003}'
        packages=project/'Packages'
        emitted=ET.parse(packages/'epoch.dependencies.props').getroot()
        wrapper=ET.Element(ns+'Project',{'DefaultTargets':'EpochBuildPlanetaryDependency'})
        group=ET.SubElement(wrapper,ns+'PropertyGroup')
        ET.SubElement(group,ns+'EpochRepoRoot').text=ROOT.as_posix()+'/'
        ET.SubElement(group,ns+'Configuration').text='Release'
        for node in emitted:
            if node.tag in (ns+'PropertyGroup',ns+'Target'):wrapper.append(copy.deepcopy(node))
        record=ET.SubElement(wrapper,ns+'Target',{'Name':'RecordCache','AfterTargets':'EpochBuildPlanetaryDependency'})
        ET.SubElement(record,ns+'WriteLinesToFile',{'File':str(base/'cache.path'),'Lines':'$(EpochPlanetBuild)','Overwrite':'true'})
        proof=packages/'link-path-proof.proj'
        ET.ElementTree(wrapper).write(proof,encoding='utf-8',xml_declaration=True)
        msbuild=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe')
        run([msbuild,proof,'/nologo','/v:minimal'],timeout=300)
        cache=Path((base/'cache.path').read_text(encoding='utf-8-sig').strip())
        self.assertEqual(cache.parent,ROOT/'Artifacts/Intermediate/planet-link')
        self.assertLess(len(str(cache)),len(str(project/'build/plink')))
        identity=(cache/'build.identity').read_bytes()
        self.assertIn(project.resolve().as_posix().encode(),identity)
        self.assertIn(ROOT.resolve().as_posix().encode(),identity)
        self.assertIn(b'epoch.development.site.v1',identity)
        runtime=list((packages/'.runtime/planetary_terrain/development').glob('Release-*/provider.identity'))
        self.assertEqual(len(runtime),1)
        self.assertIn(b'configuration=Release',runtime[0].read_bytes())
        # Reuse is exact. A foreign identity cannot be silently adopted or
        # overwritten, even if its short locator happens to collide.
        (cache/'build.identity').write_bytes(b'foreign owned cache\n')
        run([msbuild,proof,'/nologo','/v:minimal'],1)
        self.assertEqual((cache/'build.identity').read_bytes(),b'foreign owned cache\n')
        (cache/'build.identity').write_bytes(identity)
        # An actual generated wet runtime must link at the MSBuild executable
        # boundary; building its CMake static targets alone does not propagate
        # those targets' dependencies to the external generated project.
        wet_records=[]
        for config in ('Debug','Release'):
            base=ROOT/'Artifacts/pwt'/f'{time.time_ns()}-{config}'
            run([build/'Release/size_admission.exe','--stage-wet',ROOT,base])
            project=base/'project';packages=project/'Packages'
            emitted=ET.parse(packages/'epoch.dependencies.props').getroot()
            dependencies=next(emitted.iter(ns+'AdditionalDependencies')).text
            expected_libraries=('epoch_extension_planetary_water.lib','epoch_extension_water_fft.lib',
                'epoch_extension_fft_ocean.lib','epoch_water_wave.lib')
            for library in expected_libraries:self.assertIn(library,dependencies)
            source=project/'wet.link_contract.cpp'
            source.write_text('extern "C" bool epoch_install_planetary_runtime(const char*,const char*) noexcept;\n'
                'extern "C" bool epoch_bind_admitted_planetary_runtime(const char*,const char*) noexcept{return false;}\n'
                'int main(){return epoch_install_planetary_runtime(nullptr,nullptr)?1:0;}\n',encoding='utf-8')
            def linked_project(path, props):
                wrapper=ET.Element(ns+'Project',{'DefaultTargets':'Build','ToolsVersion':'Current'})
                configurations=ET.SubElement(wrapper,ns+'ItemGroup',{'Label':'ProjectConfigurations'})
                configuration=ET.SubElement(configurations,ns+'ProjectConfiguration',{'Include':config+'|x64'})
                ET.SubElement(configuration,ns+'Configuration').text=config
                ET.SubElement(configuration,ns+'Platform').text='x64'
                group=ET.SubElement(wrapper,ns+'PropertyGroup',{'Label':'Globals'})
                for name,value in {'ProjectGuid':'{971F5D93-B449-4C2A-9DDE-50369C50B7F1}',
                    'Configuration':config,'Platform':'x64','EpochRepoRoot':ROOT.as_posix()+'/',
                    'VcpkgEnabled':'false','VcpkgManifestInstall':'false'}.items():ET.SubElement(group,ns+name).text=value
                ET.SubElement(wrapper,ns+'Import',{'Project':'$(VCTargetsPath)\\Microsoft.Cpp.Default.props'})
                group=ET.SubElement(wrapper,ns+'PropertyGroup',{'Label':'Configuration'})
                for name,value in {'ConfigurationType':'Application','PlatformToolset':'v143',
                    'UseDebugLibraries':'true' if config=='Debug' else 'false'}.items():ET.SubElement(group,ns+name).text=value
                ET.SubElement(wrapper,ns+'Import',{'Project':'$(VCTargetsPath)\\Microsoft.Cpp.props'})
                ET.SubElement(wrapper,ns+'Import',{'Project':str(props)})
                group=ET.SubElement(wrapper,ns+'PropertyGroup')
                for name,value in {'OutDir':str(base/config)+'\\','IntDir':str(base/(config+'-obj-'+path.stem))+'\\',
                    'TargetName':'wet-link-contract'}.items():ET.SubElement(group,ns+name).text=value
                definitions=ET.SubElement(wrapper,ns+'ItemDefinitionGroup')
                compile=ET.SubElement(definitions,ns+'ClCompile')
                ET.SubElement(compile,ns+'LanguageStandard').text='stdcpplatest'
                ET.SubElement(compile,ns+'RuntimeLibrary').text='MultiThreadedDebugDLL' if config=='Debug' else 'MultiThreadedDLL'
                link=ET.SubElement(definitions,ns+'Link');ET.SubElement(link,ns+'SubSystem').text='Console'
                # This standalone fixture supplies the same SDK mesh/runtime contract symbols
                # that EpochEngine.lib owns in ordinary generated projects.
                ET.SubElement(link,ns+'AdditionalDependencies').text='$(EpochPlanetBuild)\\$(EpochPlanetConfiguration)\\epoch_planet_mesh_format.lib;%(AdditionalDependencies)'
                files=ET.SubElement(wrapper,ns+'ItemGroup');ET.SubElement(files,ns+'ClCompile',{'Include':str(source)})
                ET.SubElement(wrapper,ns+'Import',{'Project':'$(VCTargetsPath)\\Microsoft.Cpp.targets'})
                ET.ElementTree(wrapper).write(path,encoding='utf-8',xml_declaration=True)
            proof=packages/'wet-link-proof.vcxproj';linked_project(proof,packages/'epoch.dependencies.props')
            run([msbuild,proof,'/nologo','/v:minimal','/m:1','/p:PreferredToolArchitecture=x64'],timeout=300)
            run([base/config/'wet-link-contract.exe'])
            if config=='Release':
                negative=copy.deepcopy(emitted)
                link=next(negative.iter(ns+'AdditionalDependencies'))
                link.text=';'.join(part for part in link.text.split(';') if not any(library in part for library in expected_libraries))
                negative_props=packages/'wet-link-negative.props'
                ET.ElementTree(negative).write(negative_props,encoding='utf-8',xml_declaration=True)
                negative_project=packages/'wet-link-negative.vcxproj';linked_project(negative_project,negative_props)
                output=run([msbuild,negative_project,'/nologo','/v:minimal','/m:1','/p:PreferredToolArchitecture=x64'],1,timeout=300)
                self.assertIn('LNK2019',output);self.assertIn('Implementation',output)
            wet_records.append({'configuration':config,'passed':True,'props':str(packages/'epoch.dependencies.props'),'project':str(proof)})
        (report/'wet-link-result.json').write_text(json.dumps({'passed':True,'scope':'Actual generated MSBuild wet runtime executable link and no-render admission-port call; Release missing-dependency counterfactual reproduces LNK2019. No graphics or models.','configurations':wet_records},indent=2)+'\n',encoding='utf-8')
        (report/'stage-result.json').write_text(json.dumps({'passed':True,'scope':'Actual SourceInstaller approvals, emitted provider-stage/ErrorsOnly MSBuild nodes and extracted stream admission. Actual emitted Release dependency target configures/builds/stages the provider from a deep project through a short full-identity cache; foreign identity refuses. Retained producer byte fixtures are never executed. No models or graphics.','commands':runs},indent=2)+'\n',encoding='utf-8')

    def test_actual_size_codec_binding_and_transport(self, stage_only=False):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        owner = ROOT / "Engine/src/project/project.planetary.cpp"
        source = owner.read_text(encoding="utf-8")
        # The shared brace extractor expects a function body as its first '{'.
        # Spell only this equivalent default argument without braces; retain
        # every production statement and the default-construction semantics.
        extraction_source = source.replace("std::stop_token cancellation={})", "std::stop_token cancellation=std::stop_token())")
        functions = "\n".join(definition(extraction_source, marker) for marker in (
            "        std::string read_small(", "        bool source_supports_size_controls(",
            "        bool source_supports_mesh_frame(", "        bool source_supports_original_sunlight(", "        std::string real_text(",
            "        void append_size_arguments(", "        std::string stream_cache_identity(",
            "        std::optional<GeneratedMeshFrame> admit_generated_mesh_frame(",
            "        std::optional<fs::path> ordinary_directory(", "        std::optional<fs::path> project_directory(", "        bool write_owned(",
            "        std::optional<std::string> recipe_guard(", "        std::string water_source_requirement(", "        bool save_settings_observed(",
            "        bool supported_session_envelope(", "    bool save_settings("))
        start = source.index("            std::string_view remaining{receipt};")
        end = source.index("            if (ready)\n", start)
        admitted='const auto& content_hash=attached.content_hash;\n'+definition(source[source.index('        std::optional<AdmittedProvider> admitted_provider('):],'            if (producer.empty())')
        names=source.index('            constexpr std::string_view producerName',source.index('        std::optional<AdmittedProvider> admitted_provider('))
        producer_names=source[source.rfind('#ifdef _WIN32',0,names):source.index('#endif',names)+len('#endif')]
        code = CODE.replace("FUNCTIONS", functions).replace("PARSE_RECEIPT", source[start:end]).replace('ADMIT_STAGED',admitted).replace('PRODUCER_NAMES',producer_names).replace('EDITION_HEADER',(ROOT/'Engine/include/epoch.edition.hpp').as_posix())
        self.assertIn("append_size_arguments(generationArguments,settings);", source)
        self.assertIn("append_size_arguments(streamArguments,settings);", source)
        self.assertIn("recipe_guard(project_root)!=observedRecipe", source)
        self.assertIn("save_settings_observed(project_root,settings,&observedRecipe,cancellation)", source)
        self.assertIn("result.mesh_frame=admit_generated_mesh_frame", source)
        identity = hashlib.sha256(code.encode()+owner.read_bytes()+Path(__file__).read_bytes()
            +(ROOT/'Tests/packages/project.planetary_generation_contract.cpp').read_bytes()).hexdigest()[:12]
        report = ROOT / ("Artifacts/Reports/planet-provider-stage-oct28" if stage_only else "Artifacts/Reports/planet-size-host-oct28") / identity
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / "data" / str(time.time_ns())
        generation_fixture = ROOT / "Artifacts/pgi" / str(time.time_ns())
        (report / "consumer.cpp").write_text(code, encoding="utf-8")
        (report / "provider_refusal.cpp").write_text("int main(){return 17;}\n", encoding="utf-8")
        modules = [ROOT / "Engine/modules" / name for name in (
            "asset.texture_artifact.ixx", "asset.mesh_artifact.ixx", "asset.cloud_volume.ixx",
            "project.planetary.ixx", "project.planetary_snapshot.ixx", "project.planetary_runtime.ixx", "water.wave.ixx", "platform.filesystem.ixx",
            "package.catalog.ixx", "package.registry.ixx", "package.source_install.ixx",
            "core.sha256.ixx", "platform.child_process.ixx", "platform.work_admission.ixx")]
        modules.append(ROOT / "Engine/dep/EpochPlatformEngine/modules/core.env.ixx")
        owners = [ROOT / path for path in (
            "Engine/src/assets/texture/asset.texture_artifact.cpp", "Engine/src/project/project.planetary.cpp",
            "Engine/src/project/project.planetary_snapshot.cpp",
            "Engine/src/packages/package.catalog.cpp", "Engine/src/packages/package.source_install.cpp",
            "Engine/src/platform/platform.child_process.cpp", "Engine/src/platform/platform.work_admission.cpp",
            "Engine/dep/EpochPlatformEngine/src/core/core.env.cpp")]
        paths = "\n".join('"' + path.as_posix() + '"' for path in modules)
        cpp_paths = "\n".join('"' + path.as_posix() + '"' for path in owners)
        (report / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(PlanetSizeHost LANGUAGES CXX)
add_library(owners STATIC {cpp_paths})
target_compile_features(owners PUBLIC cxx_std_23)
target_compile_options(owners PUBLIC /utf-8 /EHsc)
target_compile_definitions(owners PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(owners PUBLIC "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_link_libraries(owners PUBLIC winhttp shell32 userenv advapi32 bcrypt comdlg32 windowscodecs ole32)
target_sources(owners PUBLIC FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {paths})
add_executable(size_admission consumer.cpp)
add_executable(provider_refusal provider_refusal.cpp)
add_executable(settings_contract "{(ROOT/'Tests/packages/project.planetary_settings_contract.cpp').as_posix()}")
add_executable(generation_identity "{(ROOT/'Tests/packages/project.planetary_generation_contract.cpp').as_posix()}")
target_link_libraries(size_admission PRIVATE owners)
target_link_options(size_admission PRIVATE /MANIFEST:EMBED "/MANIFESTINPUT:{(ROOT/'Engine/resource/epoch.manifest').as_posix()}")
target_link_libraries(settings_contract PRIVATE owners)
target_link_libraries(generation_identity PRIVATE owners)
target_link_options(generation_identity PRIVATE /MANIFEST:EMBED "/MANIFESTINPUT:{(ROOT/'Engine/resource/epoch.manifest').as_posix()}")
enable_testing()
add_test(NAME actual_size_transport_binding COMMAND size_admission "{fixture.as_posix()}/$<CONFIG>")
add_test(NAME actual_legacy_and_size_recipe COMMAND settings_contract)
add_test(NAME actual_generation_recipe_mesh_identity COMMAND generation_identity "{ROOT.as_posix()}" "{(ROOT/'Engine/dep/EpochEngineExtensions/packages/planetary_terrain/package.epoch-package.json').as_posix()}" "{generation_fixture.as_posix()}/$<CONFIG>" --generation-identity)
set_tests_properties(actual_generation_recipe_mesh_identity PROPERTIES TIMEOUT 180)
''', encoding="utf-8")
        build = ROOT / ("Artifacts/Intermediate/cmake/planet-provider-stage" if stage_only else "Artifacts/Intermediate/cmake/planet-size-host") / identity
        for config in ("Debug", "Release"):
            data = fixture / config
            target = data / "redirect-target"
            target.mkdir(parents=True, exist_ok=True)
            redirected = data / "redirected-project/Assets/Worlds"
            redirected.parent.mkdir(parents=True, exist_ok=True)
            if not redirected.exists():
                junction = subprocess.run(["cmd", "/c", "mklink", "/J", str(redirected), str(target)],
                                          cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(junction.returncode, 0, junction.stdout+junction.stderr)
        commands = [["cmake", "-S", str(report), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"]+(["--target","size_admission"] if stage_only else [])]
            if not stage_only:commands += [["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        runs = []
        for number, command in enumerate(commands):
            started = time.monotonic()
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
            log = report / f"lane-{number}.log"
            log.write_text(run.stdout + run.stderr, encoding="utf-8")
            runs.append({"command": command, "exit": run.returncode, "seconds": time.monotonic()-started, "log": str(log)})
            (report / "result.json").write_text(json.dumps({"passed": run.returncode == 0 and number == len(commands)-1,
                "scope": "Actual full project.planetary CPP and12module host codec/binding/CLI/cache/stream/frame/filesystem admission, real reviewed optional-provider CPU generate/Save/regenerate identity checks. Synthetic malformed packets; no model or graphics process.",
                "commands": runs}, indent=2), encoding="utf-8")
            self.assertEqual(run.returncode, 0, run.stdout[-6000:] + run.stderr[-2000:])
        if stage_only:
            self._stage_cases(build,report)
        else:
            self._wet_scope_cases(build,report)

    def _wet_scope_cases(self, build, report):
        scope_runs=[]
        for config in ("Debug", "Release"):
            base=ROOT/"Artifacts/pwt"/f'{time.time_ns()}-scope-{config}'
            project=base/"project";exe=build/config/"size_admission.exe"
            producer=project/"build/input"/config/"epoch_planet_mesh_export.exe"
            producer.parent.mkdir(parents=True);shutil.copy2(build/config/"provider_refusal.exe",producer)
            commands=[[exe,"--stage-wet",ROOT,base],
                ["cmake",f'-DEPOCH_PLANET_PROVIDER={producer}',
                 f'-DEPOCH_PLANET_CONFIGURATION={config}',"-P",project/"Packages/epoch.stage_planet.cmake"],
                [exe,"--body-stream",project,"1"],[exe,"--body-stream",project,"0"]]
            for command in commands:
                run=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,timeout=60)
                log=report/f'wet-scope-{len(scope_runs)}.log';log.write_text(run.stdout+run.stderr,encoding="utf-8")
                scope_runs.append({"command":[str(x) for x in command],"exit":run.returncode,"log":str(log)})
                (report/"wet-scope-result.json").write_text(json.dumps({"passed":run.returncode==0 and len(scope_runs)==8,
                    "scope":"Actual immutable source installation, emitted provider staging and full stream_step provider identity for wet authored project/dry selected body and wet body. Intentional CPU provider exit17, no graphics/models; authored recipe unchanged.",
                    "commands":scope_runs},indent=2)+"\n",encoding="utf-8")
                self.assertEqual(run.returncode,0,run.stdout[-5000:]+run.stderr[-2500:])


if __name__ == "__main__":
    unittest.main()
