"""Neutral real asset-job CPU fixture shared by Public and Development tests.

Contains only ordinary generation/import contracts and explicit recording ports.
No own-engine-source AI, candidate protocol, native launcher or model execution.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
from Tests.scene.test_glb_model_import import consumer as glb_consumer
from Tests.support.asset_job_requests import invalid_requests
from Tests.support.cpp import configure_arguments, executable, dependency_include
ROOT=Path(__file__).resolve().parents[2]


def fixture(root=None):
    root = Path(root) if root is not None else ROOT
    owner = (root/"Engine/src/ai/ai.asset_jobs.cpp").read_text(encoding="utf-8")
    body = owner[owner.index("namespace epochengine::ai::asset_jobs"):]
    body = body.replace("namespace child = platform::child_process;", "namespace child = test_child;")
    body = body.replace("namespace admission = platform::work_admission;", "namespace admission = test_admission;")
    api = (root / "Engine/modules/ai.asset_jobs.ixx").read_text(encoding="utf-8")
    api = api[api.index("export namespace epochengine::ai::asset_jobs"):].replace("export namespace", "namespace", 1)
    glb = glb_consumer().split("int main(int argc,char** argv)")[0]
    return glb + r'''
#include <chrono>
#include <cctype>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <stop_token>
#include <unordered_set>
#include <unordered_map>
#include "ai.native_idle_policy.hpp"
#include "ai.native_shared_inputs.hpp"
import ai.image_generation;
import ai.model_generation;
import ai.triposr_generation;
import ai.model_install;
import core.sha256;
import asset.texture_import;
import asset.texture_artifact;
import platform.filesystem;
import platform.child_process;
import platform.work_admission;
namespace fs=std::filesystem;
namespace epochengine::ai::native_storage {
struct Paths{fs::path model_cache;bool shared_native_inputs_disabled{},shared_model_inputs_bound{};};
fs::path fixture_cache;bool disabled{},bound{};Paths defaults(){return {fixture_cache,disabled,bound};}}
namespace epochengine::logger {
enum class LogLevel {INFO};struct Port{void log(LogLevel,const std::string&)const{}};Port get(const char*){return {};}}
namespace epochengine::core::env {std::optional<std::string> get(const char*){return std::string{"C:/Windows"};}}
namespace epochengine::ai::native_runtime {
using WorkLease=idle::WorkLease;
struct Snapshot{bool busy{},loaded{};};bool busy{},loaded{};unsigned snapshots{};
Snapshot snapshot(bool){++snapshots;return {busy,loaded};}
// This is an explicit permit-validity port. Native private-mint/thread/generation
// behavior is proved separately by its actual lifecycle owner tests.
class AssetStagePermit{public:bool admitted{};bool valid()const noexcept{return admitted;}};
}
namespace epoch_ai::process {
struct Request{std::string executable;std::vector<std::string>arguments;std::chrono::seconds timeout;std::size_t max_output_bytes;};
struct QueryResult{int exit_code{};bool cancelled{},timed_out{};std::string stdout_text{"15360\n"};};
unsigned queries{};bool lowGpu{};std::optional<QueryResult>run(const Request&,std::stop_token){++queries;QueryResult r;if(lowGpu)r.stdout_text="1024\n";return r;}}
namespace test_admission {
namespace real=epochengine::platform::work_admission;
using Reason=real::Reason;using real::assess_memory_budget;using real::reason_message;
unsigned samples{};bool lowHost{};
std::uint64_t now_milliseconds(){return 1000;}
real::ResourceSample sample_host_resources(){++samples;return {.captured_at_ms=1000,.total_memory_bytes=64ull<<30,
 .available_memory_bytes=lowHost?1ull<<30:32ull<<30,.cpu_busy_fraction=.1,.memory_valid=true,.cpu_valid=true};}}
namespace test_child {
namespace real=epochengine::platform::child_process;
using LaunchRequest=real::LaunchRequest;using ProcessSnapshot=real::ProcessSnapshot;
using ProcessHandle=real::ProcessHandle;using StopMode=real::StopMode;using WaitCode=real::WaitCode;
std::string_view wait_code_name(WaitCode code)noexcept{return code==WaitCode::exited?"exited":"not_exited";}
enum class Scenario{success,failure,timeout,unretired,release_failure,cancel_after_worker};
Scenario scenario{};Bytes payload;LaunchRequest last;unsigned launches{},stops{},releases{};std::stop_source* cancelSource{};
std::optional<std::vector<real::EnvironmentVariable>>prepare_workspace_environment(const fs::path& work){return std::vector<real::EnvironmentVariable>{{"TMP",work.generic_string()}};}
real::LaunchResult launch_or_focus(const LaunchRequest& request){++launches;last=request;
 std::ofstream out(request.arguments.at(0),std::ios::binary);out.write(reinterpret_cast<const char*>(payload.data()),std::streamsize(payload.size()));CHECK(out);
 return {.code=real::LaunchCode::started,.handle={1,launches}};}
real::StopCode stop(ProcessHandle,StopMode){++stops;return real::StopCode::requested;}
real::WaitResult wait(ProcessHandle handle,std::stop_token,std::uint64_t){
 if(scenario==Scenario::cancel_after_worker&&cancelSource)cancelSource->request_stop();
 ProcessSnapshot snapshot{.handle=handle,.state=real::ProcessState::exited,.runtime_ns=2'000'000,
   .exit_code=scenario==Scenario::failure?1:0,.exit_code_valid=true};
 if(scenario==Scenario::unretired||(scenario==Scenario::timeout&&stops==0)){snapshot.state=real::ProcessState::running;return {.code=WaitCode::timed_out,.process=snapshot};}
 return {.code=WaitCode::exited,.process=snapshot};}
bool release(ProcessHandle){++releases;if(scenario==Scenario::release_failure)return false;last.retirement_guard.reset();return true;}
}
''' + api + "\n" + body + r'''
namespace jobs=epochengine::ai::asset_jobs;
namespace native=epochengine::ai::native_runtime;
std::string hash(std::string_view bytes){return epochengine::core::sha256::hex(epochengine::core::sha256::hash(bytes));}
void file(const fs::path& path,std::string_view bytes){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(bytes.data(),std::streamsize(bytes.size()));CHECK(out);}
std::string contents(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{}};}
int main(int argc,char**argv){
 CHECK(argc==2);const auto root=fs::absolute(fs::path(argv[1]));fs::create_directories(root/"jobs");
 epochengine::ai::native_storage::fixture_cache=root/"shared/model-cache";fs::create_directories(epochengine::ai::native_storage::fixture_cache);
 const auto gpuLock=root/"shared/native-gpu.lock";
 const auto sourceFixture=fixture();const auto view=sourceFixture.json["bufferViews"][4]["byteOffset"].get<std::size_t>();
 const std::string png(reinterpret_cast<const char*>(sourceFixture.binary.data()+view),80);file(root/"reference.png",png);
 file(root/"runtime/fixture.exe","fixture executable never launched");
 const auto exe=root/"runtime/fixture.exe";
 epochengine::ai::model_generation::Request request{.reference_path=root/"reference.png",.approved_reference_sha256=hash(png)};
 unsigned next{};std::function<void()>duringPrepare;
 jobs::ModelBackend backend{.id="fixture-supervisor-port",.revision="explicit CPU fixture",
 .model_package_ids={"os_model_trellis_2_cpp_q4_512"},.terms_accepted=true,
 .preflight=[](const auto&){return std::string{};},
 .prepare=[&](const auto& frozen,const fs::path& work){
   CHECK(frozen.reference_path==work/"approved-reference.png"&&contents(frozen.reference_path)==png&&frozen.approved_reference_sha256==hash(png));
   if(duringPrepare)duringPrepare();
   epochengine::ai::model_generation::Invocation result;
   result.ready=true;result.output=work/"generated.glb";result.required_face_budget=frozen.face_budget;result.reference_sha256=frozen.approved_reference_sha256;
   result.launch.executable=exe;result.launch.expected_executable=epochengine::platform::child_process::ExecutableIdentity{32,std::string(64,'a')};
   result.launch.working_directory=work;result.launch.merged_output_path=work/"generation.log";result.launch.disconnect_standard_input=true;
   result.launch.arguments={result.output.generic_string()};return result;}};
 const auto run=[&](const Bytes& bytes){test_child::payload=bytes;test_child::stops=0;return jobs::generate_model(request,backend,root/"jobs"/std::to_string(++next));};
 auto result=run(glb(sourceFixture));CHECK(result&&result.code==jobs::Code::awaiting_model_approval&&result.worker_retired);
 CHECK(result.output.filename()=="validated.glb"&&result.faces==1&&result.vertices==3&&result.materials==1&&result.textures==1);
 CHECK(result.worker_runtime_ns==2'000'000&&result.decoded_texture_bytes==32);
 auto receipt=Json::parse(contents(result.receipt));CHECK(receipt["state"]=="awaiting_model_approval"&&!receipt["imported"].get<bool>()&&!receipt["model_approved"].get<bool>());
 CHECK(receipt["textures"].size()==2&&receipt["textures"][0]["color_space"]=="linear"&&receipt["textures"][1]["color_space"]=="srgb");
 CHECK(receipt["textures"][0]["compiled_artifact"]!=receipt["textures"][1]["compiled_artifact"]);
 CHECK(result.compiled_assets.empty()&&!receipt["project_import_bundle"]["available"].get<bool>());
 auto opaque=sourceFixture;auto& material=opaque.json["materials"][0];
 for(const char* key:{"normalTexture","occlusionTexture","emissiveTexture","emissiveFactor"})material.erase(key);
 material["alphaMode"]="OPAQUE";material["doubleSided"]=false;
 auto& pbr=material["pbrMetallicRoughness"];pbr.erase("metallicRoughnessTexture");pbr["baseColorFactor"]=Json::array({1,1,1,1});pbr["metallicFactor"]=0;pbr["roughnessFactor"]=1;
 const auto sourceResult=result;result=run(glb(opaque));CHECK(result&&!result.compiled_assets.empty());
 const auto compiledBundle=Json::parse(contents(result.compiled_assets));
 CHECK(compiledBundle["schema"]=="epoch.generated-model.base-color.v1"&&compiledBundle["source_glb_sha256"]==result.output_sha256);
 CHECK(!compiledBundle["model_approved"].get<bool>()&&!compiledBundle["imported"].get<bool>());
 CHECK(compiledBundle["primitives"].size()==1&&compiledBundle["textures"].size()==1&&compiledBundle["primitives"][0]["texture"]==0);
 const auto textureBytes=contents(result.work_directory/compiledBundle["textures"][0]["file"].get<std::string>());
 const auto texture=epochengine::asset::texture::deserialize_compiled_artifact(std::as_bytes(std::span{textureBytes.data(),textureBytes.size()}));
 CHECK(texture&&texture.artifact.identity.profile.color_space==epochengine::asset::texture::ColorSpace::srgb);
 const auto mesh=epochengine::asset::mesh::read_artifact_file(result.work_directory/compiledBundle["primitives"][0]["file"].get<std::string>());
 CHECK(mesh&&mesh.artifact->vertices()[1].uv[0]==1&&mesh.artifact->vertices()[2].uv[1]==1);
 CHECK(Json::parse(contents(result.receipt))["project_import_bundle"]["manifest_sha256"]==hash(contents(result.compiled_assets)));
 const auto precompiled=epochengine::asset::model_import::compile_base_color_glb(glb(opaque));CHECK(precompiled);
 fs::create_directory(root/"cancelled-bundle");jobs::Result cancelledBundle{.work_directory=root/"cancelled-bundle"};Json cancelledEvidence;
 std::stop_source cancelledPublication;cancelledPublication.request_stop();
 CHECK(!jobs::retain_base_color_artifacts(precompiled,cancelledBundle,cancelledEvidence,cancelledPublication.get_token(),nullptr)&&cancelledBundle.compiled_assets.empty()&&fs::is_empty(cancelledBundle.work_directory));
 fs::create_directory(root/"occupied-bundle");jobs::Result occupiedBundle{.work_directory=root/"occupied-bundle"};Json occupiedEvidence;
 file(occupiedBundle.work_directory/"compiled-base-color.json","preserve unrelated evidence");
 CHECK(!jobs::retain_base_color_artifacts(precompiled,occupiedBundle,occupiedEvidence,{},nullptr)&&occupiedBundle.compiled_assets.empty());
 CHECK(contents(occupiedBundle.work_directory/"compiled-base-color.json")=="preserve unrelated evidence");
 result=sourceResult;
 CHECK(hash(contents(result.output))==result.output_sha256&&contents(result.work_directory/"approved-reference.png")==png);
 CHECK(!native::idle::work_gate.protected_work());
 {auto available=epochengine::platform::filesystem::ExclusiveFileLease::acquire(gpuLock);CHECK(available);}
 {auto external=epochengine::platform::filesystem::ExclusiveFileLease::acquire(gpuLock);CHECK(external);const auto starts=test_child::launches;
  auto refused=run(glb(sourceFixture));CHECK(!refused&&refused.code==jobs::Code::model_work_active&&test_child::launches==starts);}
 {const auto starts=test_child::launches;file(gpuLock,"invalid ownership file");auto refused=run(glb(sourceFixture));CHECK(!refused&&refused.code==jobs::Code::resources_unavailable&&test_child::launches==starts);file(gpuLock,"");}
 {
  namespace storage=epochengine::ai::native_storage;
  const auto original=storage::fixture_cache;
  storage::fixture_cache=root/"clean-user/AI/model-cache";CHECK(!fs::exists(storage::fixture_cache));
  CHECK(run(glb(sourceFixture))&&fs::is_empty(storage::fixture_cache)&&fs::exists(root/"clean-user/AI/native-gpu.lock"));
  storage::bound=storage::disabled=true;CHECK(run(glb(sourceFixture))); // No Qwen SDK, independent generator still works.
  storage::fixture_cache=root/"missing-parent/AI/model-cache";const auto starts=test_child::launches;
  auto refused=run(glb(sourceFixture));CHECK(!refused&&refused.code==jobs::Code::resources_unavailable&&!fs::exists(root/"missing-parent")&&test_child::launches==starts);
  storage::bound=storage::disabled=false;storage::fixture_cache="relative-store";CHECK(!run(glb(sourceFixture))&&test_child::launches==starts);
  fs::create_directories(root/"redirect-target");fs::create_directory_symlink(root/"redirect-target",root/"redirected-store");
  storage::fixture_cache=root/"redirected-store/model-cache";CHECK(!run(glb(sourceFixture))&&!fs::exists(root/"redirect-target/model-cache")&&test_child::launches==starts);
  storage::fixture_cache=original;
 }
 const auto hardware=test_admission::samples,queries=epoch_ai::process::queries,launches=test_child::launches;
 auto denied=backend;denied.terms_accepted=false;CHECK(!jobs::generate_model(request,denied,root/"jobs/denied"));
 auto changed=request;changed.approved_reference_sha256=std::string(64,'b');CHECK(!jobs::generate_model(changed,backend,root/"jobs/bad-hash"));
 file(root/"reference.png","not PNG");changed=request;changed.approved_reference_sha256=hash("not PNG");CHECK(!jobs::generate_model(changed,backend,root/"jobs/bad-png"));file(root/"reference.png",png);
 CHECK(test_admission::samples==hardware&&epoch_ai::process::queries==queries&&test_child::launches==launches);
 native::loaded=true;CHECK(!run(glb(sourceFixture)));native::loaded=false;CHECK(test_admission::samples==hardware);
 native::AssetStagePermit invalid;CHECK(!jobs::generate_model(request,backend,root/"jobs/bad-permit",{},&invalid));CHECK(test_admission::samples==hardware);
 native::loaded=true;native::AssetStagePermit admitted;admitted.admitted=true;test_child::payload=glb(sourceFixture);
 CHECK(jobs::generate_model(request,backend,root/"jobs/permit-port",{},&admitted));native::loaded=false;
 auto noMaterial=sourceFixture;noMaterial.json["meshes"][0]["primitives"][0].erase("material");CHECK(!run(glb(noMaterial)));
 auto noTexture=sourceFixture;noTexture.json["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");CHECK(!run(glb(noTexture)));
 auto badPng=sourceFixture;badPng.json["bufferViews"][4]["byteLength"]=8;result=run(glb(badPng));CHECK(!result&&result.code==jobs::Code::invalid_output&&fs::exists(result.receipt));
 CHECK(Json::parse(contents(result.receipt))["state"]=="failed"&&!fs::exists(result.work_directory/"validated.glb"));
 auto colors=sourceFixture;colors.json["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]=0;CHECK(!run(glb(colors)));
 const auto originalBudget=request.face_budget;request.face_budget=1;auto twice=sourceFixture;twice.json["nodes"].push_back({{"mesh",0}});twice.json["scenes"][0]["nodes"].push_back(1);CHECK(!run(glb(twice)));request.face_budget=originalBudget;
 test_child::scenario=test_child::Scenario::failure;CHECK(!run(glb(sourceFixture))&&test_child::releases>0);
 test_child::scenario=test_child::Scenario::timeout;result=run(glb(sourceFixture));CHECK(!result&&result.code==jobs::Code::timed_out&&result.worker_retired&&test_child::stops==1);
 test_child::scenario=test_child::Scenario::unretired;result=run(glb(sourceFixture));CHECK(!result&&!result.worker_retired&&result.worker.valid());CHECK(!epochengine::platform::filesystem::ExclusiveFileLease::acquire(gpuLock));test_child::last.retirement_guard.reset();
 test_child::scenario=test_child::Scenario::release_failure;CHECK(!run(glb(sourceFixture)));CHECK(!epochengine::platform::filesystem::ExclusiveFileLease::acquire(gpuLock));test_child::last.retirement_guard.reset();
 test_child::scenario=test_child::Scenario::success;
 duringPrepare=[&]{file(root/"reference.png","changed after snapshot");};CHECK(run(glb(sourceFixture)));duringPrepare={};file(root/"reference.png",png);
 std::stop_source cancelled;duringPrepare=[&]{cancelled.request_stop();};const auto before=test_child::launches;test_child::payload=glb(sourceFixture);
 result=jobs::generate_model(request,backend,root/"jobs/cancel-before-launch",cancelled.get_token());CHECK(!result&&result.code==jobs::Code::cancelled&&test_child::launches==before);duringPrepare={};
 std::stop_source after;test_child::cancelSource=&after;test_child::scenario=test_child::Scenario::cancel_after_worker;test_child::payload=glb(sourceFixture);
 result=jobs::generate_model(request,backend,root/"jobs/cancel-before-commit",after.get_token());CHECK(!result&&result.code==jobs::Code::cancelled&&!fs::exists(result.work_directory/"validated.glb"));test_child::cancelSource=nullptr;test_child::scenario=test_child::Scenario::success;
 for(const auto& entry:fs::directory_iterator(root/"invalid-cli"))if(entry.path().extension()==".json"){
  const auto record=Json::parse(contents(entry.path()));const auto path=root/"invalid-cli"/(entry.path().stem().string()+".request");
  CHECK(jobs::run_file(path)==record["expected"].get<int>());
 }
 CHECK(!native::idle::work_gate.protected_work());
}
'''


def run_fixture(test, fixture_factory=None, root=None):
    ROOT=root if root is not None else globals()["ROOT"]
    fixture_factory=fixture_factory or (lambda:fixture(ROOT))
    proof = ROOT / "Artifacts/Reports/asset-model-jobs-oct28" / str(time.time_ns())
    build = ROOT / "Artifacts/Intermediate/cmake/asset-model-jobs" / proof.name
    proof.mkdir(parents=True)
    source = proof / "ai.asset_jobs_contract.cpp"
    source.write_text(fixture_factory(), encoding="utf-8")
    modules = [ROOT/"Engine/modules"/(name+".ixx") for name in (
        "core.sha256", "platform.filesystem", "platform.child_process", "platform.work_admission",
        "ai.model_install", "ai.image_generation", "ai.model_generation", "ai.triposr_generation", "asset.texture_artifact",
        "asset.texture_import", "asset.mesh_artifact", "asset.model_import")]
    implementations = [ROOT/p for p in ("Engine/src/ai/ai.model_install.cpp", "Engine/src/ai/ai.image_generation.cpp",
        "Engine/src/ai/ai.model_generation.cpp", "Engine/src/ai/ai.triposr_generation.cpp", "Engine/src/assets/texture/asset.texture_artifact.cpp",
        "Engine/src/assets/texture/asset.texture_import.cpp", "Engine/src/assets/image.stb.cpp",
        "Engine/src/assets/mesh/asset.model_import.cpp")]
    includes = dependency_include(ROOT)
    (proof/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(AssetModelJobs LANGUAGES CXX)
add_executable(asset_jobs "{source.as_posix()}" {" ".join('"'+p.as_posix()+'"' for p in implementations)})
target_compile_features(asset_jobs PRIVATE cxx_std_23)
if(MSVC)
 target_compile_options(asset_jobs PRIVATE /W3 /WX /utf-8 /EHsc /UNDEBUG /Z7)
else()
 target_compile_options(asset_jobs PRIVATE -UNDEBUG)
endif()
target_include_directories(asset_jobs PRIVATE "{includes.as_posix()}" "{(ROOT/'Engine/src/ai').as_posix()}")
target_sources(asset_jobs PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
''', encoding="utf-8")
    cases = invalid_requests(proof)
    semantic = {"backend-bytes-zero", "width-zero", "height-below-minimum", "width-not-multiple", "dimensions-over-pixel-budget",
                "steps-zero", "prompt-empty", "prompt-nul", "prompt-over-limit", "negative-prompt-nul", "malformed-executable-hash", "relative-work-directory", "nul-model-path"}
    commands = [configure_arguments(proof,build)]
    for config in ("Debug", "Release"):
        # Keep Windows fixture data shallow enough for full hash-named artifacts.
        data = ROOT / "Artifacts/Tests" / ("aj-" + proof.name) / config
        invalid = data/"invalid-cli"; invalid.mkdir(parents=True)
        for index, (name, payload) in enumerate(cases):
            (invalid/f"{index:02d}-{name}.request").write_bytes(payload)
            (invalid/f"{index:02d}-{name}.json").write_text(json.dumps({
                "expected":1 if name in semantic else 2}), encoding="utf-8")
        commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"], [str(executable(build,config,"asset_jobs")), str(data)]]
    records=[]
    for index, command in enumerate(commands):
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=180)
        (proof/f"lane-{index}.log").write_text(result.stdout+result.stderr,encoding="utf-8")
        records.append({"command":command,"exit":result.returncode})
        test.assertEqual(result.returncode,0,result.stdout[-12000:]+result.stderr)
    (proof/"result.json").write_text(json.dumps({"passed":True,"commands":records,"ownerSha256":hashlib.sha256((ROOT/"Engine/src/ai/ai.asset_jobs.cpp").read_bytes()).hexdigest(),
        "cliRequestFixturesPrepared":len(cases),"scope":"Actual asset job source/API bodies with explicit native-state/permit-validity/hardware/supervisor/user-store path ports; real twelve module model/importer/hash/PNG/artifact dependencies and OS GPU file lease. Request fixture count is preparation only; the supplied main determines which cases execute. No GPU/child process/model. Actual private token minting and real supervisor retirement require their separate owner tests/native acceptance."},indent=2),encoding="utf-8")
    return proof
