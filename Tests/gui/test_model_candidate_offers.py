"""Actual pinned plans and Editor offer handoff; tiny files and explicit queue ports."""
from pathlib import Path
import hashlib,json,os,subprocess,time,unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import configure_arguments,executable
ROOT=Path(__file__).resolve().parents[2]
# Publisher/LFS metadata researched2026-10-29, not locally downloaded weight proof.
PINS=[{'id': 'hauhau-qwen3-6-35b-a3b-uncensored-iq3-m', 'package_id': 'os_model_hauhau_qwen3_6_35b_iq3_m', 'label': 'Hauhau Qwen3.6 35B-A3B Uncensored IQ3_M', 'repo': 'HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive', 'revision': 'f12a584fecbeb5f20001130d8ecd66c9327ae685', 'filename': 'Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-IQ3_M.gguf', 'sha256': '4dd9a27c68bd778ede351a553680a1a6e4ec62e5d383b549892860ea337be0c0', 'license': 'Apache-2.0 (publisher model card)', 'license_url': 'https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/blob/f12a584fecbeb5f20001130d8ecd66c9327ae685/README.md', 'source_url': 'https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/resolve/f12a584fecbeb5f20001130d8ecd66c9327ae685/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-IQ3_M.gguf', 'research_date': '2026-10-29', 'architecture': 'qwen35moe', 'blocked_reason': '', 'notes': 'Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.', 'bytes': 15440519296, 'parameter_count': 34660610688, 'file_count': 1, 'format': 'gguf', 'availability': 'pinned_download'}, {'id': 'hauhau-qwen3-6-35b-a3b-uncensored-q4-k-m', 'package_id': 'os_model_hauhau_qwen3_6_35b_q4_k_m', 'label': 'Hauhau Qwen3.6 35B-A3B Uncensored Q4_K_M', 'repo': 'HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive', 'revision': 'f12a584fecbeb5f20001130d8ecd66c9327ae685', 'filename': 'Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-Q4_K_M.gguf', 'sha256': 'bbef58c37ce88820be9d98b6437f1cf4bac890c947bd55fc7b68e22098574231', 'license': 'Apache-2.0 (publisher model card)', 'license_url': 'https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/blob/f12a584fecbeb5f20001130d8ecd66c9327ae685/README.md', 'source_url': 'https://huggingface.co/HauhauCS/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive/resolve/f12a584fecbeb5f20001130d8ecd66c9327ae685/Qwen3.6-35B-A3B-Uncensored-HauhauCS-Aggressive-Q4_K_M.gguf', 'research_date': '2026-10-29', 'architecture': 'qwen35moe', 'blocked_reason': '', 'notes': 'Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.', 'bytes': 21166758016, 'parameter_count': 34660610688, 'file_count': 1, 'format': 'gguf', 'availability': 'pinned_download'}, {'id': 'davidau-qwen3-6-27b-fable-fusion-uncensored-iq3-m', 'package_id': 'os_model_davidau_qwen3_6_27b_iq3_m', 'label': 'DavidAU Qwen3.6 27B Fable Fusion Uncensored IQ3_M', 'repo': 'DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF', 'revision': '81e6f49c3faf5b83c57a740521bd2974b6362fd1', 'filename': 'Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-IQ3_M.gguf', 'sha256': '4408a4815176070afb560ddadcf5883f524e159056e7e830274777a3a9c9b176', 'license': 'Apache-2.0 (publisher model card)', 'license_url': 'https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/blob/81e6f49c3faf5b83c57a740521bd2974b6362fd1/README.md', 'source_url': 'https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/resolve/81e6f49c3faf5b83c57a740521bd2974b6362fd1/Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-IQ3_M.gguf', 'research_date': '2026-10-29', 'architecture': 'qwen35', 'blocked_reason': '', 'notes': 'Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.', 'bytes': 14080729632, 'parameter_count': 26895998464, 'file_count': 1, 'format': 'gguf', 'availability': 'pinned_download'}, {'id': 'davidau-qwen3-6-27b-fable-fusion-uncensored-q4-k-m', 'package_id': 'os_model_davidau_qwen3_6_27b_q4_k_m', 'label': 'DavidAU Qwen3.6 27B Fable Fusion Uncensored Q4_K_M', 'repo': 'DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF', 'revision': '81e6f49c3faf5b83c57a740521bd2974b6362fd1', 'filename': 'Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-Q4_K_M.gguf', 'sha256': '8440f2a076f149f65bf16975f3a08359936ba2658fc1827b51a30b717de49acb', 'license': 'Apache-2.0 (publisher model card)', 'license_url': 'https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/blob/81e6f49c3faf5b83c57a740521bd2974b6362fd1/README.md', 'source_url': 'https://huggingface.co/DavidAU/Qwen3.6-27B-Fable-Fusion-711-Uncensored-Heretic-NM-DAU-NEO-MAX-MTP-GGUF/resolve/81e6f49c3faf5b83c57a740521bd2974b6362fd1/Qwen3.6-27B-Fable-Fus-711-UnHeretic-NM-DAU-NEO-MAX-NEO-Q4_K_M.gguf', 'research_date': '2026-10-29', 'architecture': 'qwen35', 'blocked_reason': '', 'notes': 'Publisher variant; not benchmarked in Epoch. Reserve memory for renderer, KV caches and temporary allocations; GPU/RAM placement must be tested.', 'bytes': 18047255072, 'parameter_count': 26895998464, 'file_count': 1, 'format': 'gguf', 'availability': 'pinned_download'}, {'id': 'research-qwen3-8-flash-next-abliterated-q4-k-m', 'package_id': '', 'label': 'Qwen3.8 Flash Next Abliterated Q4_K_M', 'repo': 'windowsxp811203/Qwen3.8-Flash-Next-Abliterated-GGUF', 'revision': 'c3365c410baa29bdd3d7cc8cbc2bf9bee0de2f3a', 'filename': 'Qwen3.8-Flash-Next-Abliterated-Q4_K_M.gguf', 'sha256': '324c85132e04654480ac93923f444b760b2950eb8c84a346dd0ec70e680ecde2', 'license': 'Qwen Community License 1.0 (conditions apply)', 'license_url': 'https://huggingface.co/windowsxp811203/Qwen3.8-Flash-Next-Abliterated-GGUF/blob/c3365c410baa29bdd3d7cc8cbc2bf9bee0de2f3a/LICENSE', 'source_url': 'https://huggingface.co/windowsxp811203/Qwen3.8-Flash-Next-Abliterated-GGUF/tree/c3365c410baa29bdd3d7cc8cbc2bf9bee0de2f3a', 'research_date': '2026-10-29', 'architecture': 'qwen4exp', 'blocked_reason': 'Weights alone exceed the 64 GiB target system RAM. Not offered for download on this target.', 'notes': 'Installed backend support is unverified; upstream support claims are not a local compatibility test. License review is separate.', 'bytes': 119150722112, 'parameter_count': 176943899520, 'file_count': 1, 'format': 'gguf', 'availability': 'blocked'}, {'id': 'research-rvn-qwen3-8-flash-next-uncensored-bf16', 'package_id': '', 'label': 'RVN Qwen3.8 Flash Next Abliterated Uncensored BF16', 'repo': '0bserverx/RVN-Qwen3.8-Flash-Next-Abliterated-Uncensored', 'revision': '38b6e951065008472ebfb72d1736c9df64bb0e9a', 'filename': '', 'sha256': '', 'license': 'Qwen Community License 1.0 (conditions apply)', 'license_url': 'https://huggingface.co/0bserverx/RVN-Qwen3.8-Flash-Next-Abliterated-Uncensored/blob/38b6e951065008472ebfb72d1736c9df64bb0e9a/LICENSE', 'source_url': 'https://huggingface.co/0bserverx/RVN-Qwen3.8-Flash-Next-Abliterated-Uncensored/tree/38b6e951065008472ebfb72d1736c9df64bb0e9a', 'research_date': '2026-10-29', 'architecture': 'qwen4_exp_text', 'blocked_reason': 'No GGUF artifact in the researched revision; this native path cannot load the safetensors bundle. It also exceeds target RAM.', 'notes': 'Bundle size is the sum of 98 BF16 weight shards, not one downloadable GGUF. No single-file hash is claimed.', 'bytes': 353887987296, 'parameter_count': 176943899520, 'file_count': 98, 'format': 'safetensors_bundle', 'availability': 'blocked'}]


def consumer():
    text=(ROOT/'Engine/src/editor/application/editor.application_packages_creation.inl').read_text(encoding='utf-8')
    state=(ROOT/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
    def function(marker):
        # A braced default path is part of the signature, not the function body.
        start=text.index(marker);body=text.index('\n        {',start)+9
        return text[start:body]+definition(text[body:],'{')
    definitions='\n'.join(function(marker) for marker in (
        '        [[nodiscard]] bool start_model_package_install(',
        '        [[nodiscard]] bool start_model_offer_install(',
        '        [[nodiscard]] bool start_model_offer_registration(',
        '        void cancel_model_offer_operation('))
    poll=definition(text,'        void poll_model_package_install(')
    publication=definition(poll,'            if (editor.packageModelInstall->publishPending)')
    return r'''#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "ai.backend_models_candidate_catalog.hpp"
import ai.model_install;
#define CHECK(test) do{if(!(test))std::exit(__LINE__);}while(false)
namespace fs=std::filesystem;
namespace catalog=epoch_ai::model::catalog;
namespace platform::child_process{
 struct ProcessHandle{bool live{};bool valid()const{return live;}};
 enum class StopMode{graceful};inline unsigned stops{};
 bool stop(ProcessHandle,StopMode){++stops;return true;}
}
namespace ai{
 namespace model_install=epochengine::ai::model_install;
 namespace native_runtime{
  inline unsigned registrations{};inline bool accept{true},missingOnly{},writable{true};inline std::string id,path;inline fs::path installRoot;
  fs::path model_install_cache_root()noexcept{return writable?installRoot:fs::path{};}
  bool register_model(std::string key,std::string value,bool onlyIfAbsent){++registrations;id=std::move(key);path=std::move(value);missingOnly=onlyIfAbsent;return accept;}
 }
}
STATE
struct EditorState{std::optional<EditorModelInstallState>packageModelInstall;std::string packageInstallStatus;float packageInstallProgress{};};
namespace package_registry{struct PackageDescriptor{std::string id;};}
fs::path cache;unsigned launches{},cheapChecks{},tasks{};bool throwQueue{};
std::unique_ptr<std::promise<ai::model_install::PublishResult>>promise;
std::function<ai::model_install::PublishResult()>queuedOperation;
fs::path model_cache_root_path(){return cache/"project-resources";}
std::string display_project_path(const fs::path&path){return path.generic_string();}
bool model_snapshot_installed(const ai::model_install::InstallPlan&){++cheapChecks;return true;}
std::uint64_t model_plan_bytes(const ai::model_install::InstallPlan&plan){std::uint64_t bytes{};for(const auto&a:plan.artifacts)bytes+=a.bytes;return bytes;}
bool launch_next_model_artifact(EditorState&){++launches;return true;}
void push_editor_log(EditorState&,const std::string&){}
template<class Op>auto schedule_editor_task(EditorState&,std::string_view,Op&&op){
 if(throwQueue)throw std::runtime_error("fixture queue admission failure");
 ++tasks;queuedOperation=std::forward<Op>(op);promise=std::make_unique<std::promise<ai::model_install::PublishResult>>();return promise->get_future();
}
DEFINITIONS
void poll_model_package_install(EditorState&editor){if(!editor.packageModelInstall)return;PUBLICATION}
auto exact_result(const EditorState&e){const auto&op=*e.packageModelInstall;
 const auto root=ai::model_install::version_root(op.cacheRoot,op.plan);
 return ai::model_install::PublishResult{.code=ai::model_install::Code::already_installed,.installed_root=root,.receipt_path=root/ai::model_install::receipt_filename};}
void complete(EditorState&e,ai::model_install::PublishResult result){promise->set_value(std::move(result));poll_model_package_install(e);CHECK(!e.packageModelInstall);}
int main(int argc,char**argv){CHECK(argc==2);cache=fs::absolute(argv[1]);fs::create_directories(cache);ai::native_runtime::installRoot=cache;
 unsigned eligible{};for(const auto&offer:catalog::offers()){
  CHECK(catalog::find(offer.id)==&offer&&!offer.license_url.empty()&&!offer.source_url.empty());
  const auto plan=ai::model_install::plan_for(offer.package_id);
  if(catalog::download_allowed(offer)){++eligible;CHECK(plan&&ai::model_install::valid_plan(*plan)&&plan->artifacts.front().source_url==offer.source_url);}
  else CHECK(!plan&&!ai::model_install::plan_for(offer.id));
  std::cout<<offer.id<<'\t'<<offer.repo<<'\t'<<offer.revision<<'\t'<<offer.filename<<'\t'<<offer.sha256<<'\t'<<offer.bytes<<'\t'<<offer.parameter_count<<'\t'<<offer.file_count<<'\n';
 }
 CHECK(eligible==4&&!catalog::find("unknown"));
 const auto&id=catalog::offers()[0].id;EditorState e;
 ai::native_runtime::writable=false;CHECK(!start_model_offer_install(e,id)&&!start_model_offer_registration(e,id)&&!e.packageModelInstall&&launches==0&&tasks==0);ai::native_runtime::writable=true;
 CHECK(!start_model_offer_install(e,catalog::offers()[4].id)&&!e.packageModelInstall&&launches==0);
 CHECK(!start_model_offer_registration(e,catalog::offers()[5].id)&&!e.packageModelInstall&&tasks==0);
 CHECK(start_model_offer_install(e,id)&&launches==1&&e.packageModelInstall->plan.package_id==catalog::offers()[0].package_id&&e.packageModelInstall->cacheRoot==cache);
 const auto retained=e.packageModelInstall->plan;CHECK(!start_model_offer_install(e,catalog::offers()[1].id)&&e.packageModelInstall->plan==retained);e.packageModelInstall.reset();
 package_registry::PackageDescriptor legacyPackage;legacyPackage.id=std::string{catalog::offers()[0].package_id};CHECK(start_model_package_install(e,legacyPackage)&&e.packageModelInstall->cacheRoot==cache/"project-resources");e.packageModelInstall.reset();
 const auto plan=*ai::model_install::plan_for(catalog::offers()[0].package_id);fs::create_directories(ai::model_install::version_root(cache,plan));
 CHECK(start_model_offer_install(e,id)&&cheapChecks==0&&e.packageModelInstall->artifactIndex==plan.artifacts.size());e.packageModelInstall.reset();
 // No verification runs in the frame; it is queued and polling preserves it.
 CHECK(start_model_offer_registration(e,id)&&tasks==1&&ai::native_runtime::registrations==0);poll_model_package_install(e);CHECK(e.packageModelInstall);
 const auto missing=queuedOperation();CHECK(!missing.accepted());complete(e,missing);CHECK(ai::native_runtime::registrations==0);
 CHECK(start_model_offer_registration(e,id));auto good=exact_result(e);const auto expectedPath=good.installed_root/plan.artifacts.front().file;
 complete(e,good);CHECK(ai::native_runtime::registrations==1&&ai::native_runtime::missingOnly&&ai::native_runtime::id==id&&ai::native_runtime::path==expectedPath.generic_string());
 CHECK(start_model_offer_registration(e,id));good=exact_result(e);cancel_model_offer_operation(e);complete(e,good);CHECK(ai::native_runtime::registrations==1);
 CHECK(start_model_offer_registration(e,id));good=exact_result(e);good.installed_root/="foreign";complete(e,good);CHECK(ai::native_runtime::registrations==1);
 CHECK(start_model_offer_registration(e,id));good=exact_result(e);good.receipt_path/="foreign";complete(e,good);CHECK(ai::native_runtime::registrations==1);
 CHECK(start_model_offer_registration(e,id));good=exact_result(e);e.packageModelInstall->registrationOfferId=std::string{catalog::offers()[1].id};complete(e,good);CHECK(ai::native_runtime::registrations==1);
 CHECK(start_model_offer_registration(e,id));good=exact_result(e);e.packageModelInstall->plan.revision[0]='0';complete(e,good);CHECK(ai::native_runtime::registrations==1);
 // A failed read-only verification has no staging directory to delete from.
 std::ofstream("sentinel.gguf")<<"unrelated fixture bytes";
 CHECK(start_model_offer_registration(e,id));good=exact_result(e);good.code=ai::model_install::Code::sha256_mismatch;good.artifact="sentinel.gguf";complete(e,good);
 CHECK(fs::is_regular_file("sentinel.gguf")&&ai::native_runtime::registrations==1);
 throwQueue=true;CHECK(!start_model_offer_registration(e,id)&&!e.packageModelInstall);throwQueue=false;
 ai::native_runtime::accept=false;CHECK(start_model_offer_registration(e,id));complete(e,exact_result(e));CHECK(e.packageInstallStatus.find("not queued")!=std::string::npos);ai::native_runtime::accept=true;
 CHECK(start_model_offer_install(e,id));e.packageModelInstall->process.live=true;cancel_model_offer_operation(e);CHECK(e.packageModelInstall->cancelled&&platform::child_process::stops==1);
 return 0;
}
'''.replace('STATE',definition(state,'        struct EditorModelInstallState',True)).replace('DEFINITIONS',definitions).replace('PUBLICATION',publication)


class ModelCandidateOffers(unittest.TestCase):
    def test_catalog_pins_and_existing_install_registration_owners(self):
        code=consumer();identity=hashlib.sha256(code.encode()).hexdigest()[:12]
        report=ROOT/'Artifacts/Reports/model-candidates-oct29/cpu'/identity;report.mkdir(parents=True,exist_ok=True)
        build=ROOT/'Artifacts/Intermediate/cmake/model-candidate-offers'/identity
        (report/'consumer.cpp').write_text(code,encoding='utf-8')
        modules=['core.sha256','platform.filesystem','ai.model_install']
        sources=' '.join('"'+(ROOT/'Engine/modules'/(name+'.ixx')).as_posix()+'"' for name in modules)
        (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 4.4)
project(ModelCandidates LANGUAGES CXX)
add_library(candidate_plans STATIC "{(ROOT/'Engine/src/ai/ai.model_install.cpp').as_posix()}")
target_compile_features(candidate_plans PUBLIC cxx_std_23)
target_sources(candidate_plans PUBLIC FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {sources})
add_executable(offers consumer.cpp)
target_link_libraries(offers PRIVATE candidate_plans)
target_include_directories(offers PRIVATE "{(ROOT/'Engine/src/ai/native').as_posix()}")
add_executable(install_contract "{(ROOT/'Engine/src/ai/ai.model_install_contract.cpp').as_posix()}")
target_link_libraries(install_contract PRIVATE candidate_plans)
target_compile_definitions(install_contract PRIVATE EPOCH_AI_MODEL_INSTALL_CONTRACT_MAIN=1)
foreach(target candidate_plans offers install_contract)
 if(MSVC)
  target_compile_options(${{target}} PRIVATE /utf-8 /EHsc /Z7 /UNDEBUG)
 else()
  target_compile_options(${{target}} PRIVATE -UNDEBUG)
 endif()
endforeach()
''',encoding='utf-8')
        commands=[configure_arguments(report,build)]
        fixture_data=ROOT/'Artifacts/Tests'/f'o{time.time_ns():x}'
        for config in ('Debug','Release'):
            data=fixture_data/config;data.mkdir(parents=True)
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],[str(executable(build,config,'install_contract'))],[str(executable(build,config,'offers')),str(data/'models')]]
        # This contract appends its own unique versioned-cache tree. Keep the
        # fixture prefix short for ordinary Windows executables without a
        # longPathAware manifest; all temporary data still stays checkout-local.
        temporary=fixture_data/'tmp';temporary.mkdir(parents=True,exist_ok=True)
        environment=dict(os.environ,TMP=str(temporary),TEMP=str(temporary),TMPDIR=str(temporary))
        records=[];started=time.monotonic()
        for index,command in enumerate(commands):
            ran=subprocess.run(command,cwd=report,env=environment,capture_output=True,text=True,timeout=120)
            (report/f'phase-{index}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8');records.append({'command':command,'exit':ran.returncode})
            self.assertEqual(ran.returncode,0,(ran.stdout+ran.stderr)[-10000:])
            if Path(command[0]).name in ('offers','offers.exe'):
                actual=[line.split('\t') for line in ran.stdout.splitlines()]
                wanted=[[str(pin[key]) for key in ('id','repo','revision','filename','sha256','bytes','parameter_count','file_count')] for pin in PINS]
                self.assertEqual(actual,wanted)
        (report/'result.json').write_text(json.dumps({'passed':True,'commands':records,'elapsed_seconds':time.monotonic()-started,
            'scope':'Real catalog, compiled pinned plan/publication/hash contract and actual Editor offer-start/cancel/publication-poll blocks. Native register queue/HTTPS transfer/scheduler are explicit recording ports. Tiny files only; no weights, model, GUI, GPU or download.'},indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':unittest.main()
