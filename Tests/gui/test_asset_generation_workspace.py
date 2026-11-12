"""Actual workspace state/approval/handoff algorithms with explicit GUI/lifecycle ports.

Real job, hash, PNG, GLB and artifact owners are compiled independently in D/R.
No GPU, model, native window, or user model configuration is touched.
"""
from pathlib import Path
import hashlib
import json
import time
import unittest
from Tests.support import asset_job_fixture as jobs_test
from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]
ORIGINAL_FIXTURE=lambda:jobs_test.fixture(ROOT)


def workspace_fixture():
    fixture=ORIGINAL_FIXTURE().split("int main(int argc,char**argv)")[0]
    api=(ROOT/"Engine/modules/editor/editor.asset_generation.ixx").read_text(encoding="utf-8")
    api=api[api.index("export namespace epochengine::editor_asset_generation"):].replace("export namespace","namespace",1)
    owner=(ROOT/"Engine/src/editor/editor.asset_generation.cpp").read_text(encoding="utf-8")
    body=owner[owner.index("namespace epochengine::editor_asset_generation"):owner.index("    bool prepare_preview(State& state)")]+"}\n"
    adapter=(ROOT/"Engine/src/editor/application/editor.application_asset_browsing.inl").read_text(encoding="utf-8")
    dispatch=definition(adapter,'        void dispatch_asset_generation(')
    guards=definition(dispatch,'            if(editor.assetGenerationPending)')
    branches=definition(dispatch,'            if(action==generation::Action::save_draft)')+definition(dispatch,'            if(action==generation::Action::load_draft)')+definition(dispatch,'            if(action==generation::Action::load_image_settings||action==generation::Action::load_model_settings)')
    branches=branches.replace('platform::filesystem::','draft_dialog::').replace('core::path::executable_dir()','draft_dialog::initial')
    branches=branches.replace('ai::asset_jobs::','epochengine::ai::asset_jobs::')
    selection_helpers='\n'.join(definition(adapter,marker) for marker in (
        '        [[nodiscard]] std::filesystem::path asset_generation_cache_root(',
        '        void restore_asset_generation_backend_selection(',
        '        void remember_asset_generation_backend_selection('))
    selection_helpers=selection_helpers.replace('EditorState&','DraftAdapter&').replace('core::path::candidate_data_root()','draft_dialog::initial').replace('core::path::executable_dir()','draft_dialog::initial')
    image_codec = (ROOT/'Engine/src/assets/image.decode.hpp').as_posix()
    return '#include <atomic>\n#include <charconv>\n#include <limits>\n#include <iomanip>\n#include <unordered_set>\n#include "'+str(ROOT/'Engine/include/epoch.edition.hpp').replace('\\','/')+'"\n#include "'+str(ROOT/'Engine/include/core.utf8.hpp').replace('\\','/')+'"\n#include "'+image_codec+'"\n'+fixture+r'''
namespace epochengine {
// Explicit UI type ports only; draw/marker geometry are not executed here.
struct SpriteHandle{};
namespace gui{struct WidgetBounds{};}
namespace previewgrid{struct ObjectMarker{};}
}
namespace epochengine::ai::native_runtime {
struct AssetStageCompletion{bool succeeded{},workers_retired{};};
struct AssetStageOutcome{bool stage_started{},restored{},workers_retired{};std::string diagnostic;};
unsigned handoffs{};bool restore_ok=true,retire_ok=true;std::stop_source* cancel_after{};
AssetStageOutcome run_asset_stage(std::function<AssetStageCompletion(std::stop_token,const AssetStagePermit&)> operation,std::stop_token stop){
 ++handoffs;if(stop.stop_requested())return {false,true,true,"cancelled before handoff"};
 AssetStagePermit permit;permit.admitted=true;const auto completed=operation(stop,permit);
 if(cancel_after)cancel_after->request_stop();
 return {true,restore_ok,completed.workers_retired&&retire_ok,restore_ok?"restored":"restore failed"};
}
}
'''+api+body+r'''
namespace draft_dialog{
 struct Result{bool accepted{true},cancelled{};fs::path path;std::string diagnostic;};
 Result selected;fs::path initial;unsigned opens{},saves{};
 template<class...A>Result open_file_dialog(A&&...){++opens;return selected;}
 template<class...A>Result save_file_dialog(A&&...){++saves;return selected;}
}
struct DraftAdapter{epochengine::editor_asset_generation::State assetGeneration;bool assetGenerationPending{},assetGenerationFocusPending{};};
namespace editor_asset_generation=epochengine::editor_asset_generation;
std::vector<std::string>asset_messages;void push_editor_log(DraftAdapter&,std::string message){asset_messages.push_back(std::move(message));}
'''+selection_helpers+r'''
void actual_draft_dispatch(DraftAdapter& editor,epochengine::editor_asset_generation::Action action){
 namespace generation=epochengine::editor_asset_generation;auto& state=editor.assetGeneration;
'''+guards+branches+r'''
}
int main(int argc,char**argv){
 CHECK(argc==2);const fs::path root=fs::absolute(argv[1]);fs::create_directories(root/"jobs");fs::create_directories(root/"shared/model-cache");
 draft_dialog::initial=root;
 epochengine::ai::native_storage::fixture_cache=root/"shared/model-cache";
 namespace workspace=epochengine::editor_asset_generation;namespace native=epochengine::ai::native_runtime;
 workspace::State state;const workspace::ProjectIdentity project{"project-root","project",17};
 const auto fixtureSource=fixture();const auto offset=fixtureSource.json["bufferViews"][4]["byteOffset"].get<std::size_t>();
 const std::string png(reinterpret_cast<const char*>(fixtureSource.binary.data()+offset),80);file(root/"reference.png",png);
 const auto hardware=test_admission::samples,queries=epoch_ai::process::queries,launches=test_child::launches;
 auto reference=workspace::inspect_reference(root/"reference.png",project,{});CHECK(reference.result&&reference.image&&reference.result.width==2);
 CHECK(workspace::current_project(reference,project));auto changedProject=project;++changedProject.generation;CHECK(!workspace::current_project(reference,changedProject));
 workspace::accept_completion(state,std::move(reference));CHECK(state.reference&&!state.reference_approved&&!state.generated_model);
 std::string reason;CHECK(workspace::reference_current(state,reason)&&!state.reference_approved); // standalone texture check grants no 3D consent.
 CHECK(workspace::save_output_copy(*state.reference,root/"kept-reference.png",reason,{}));
 CHECK(contents(root/"kept-reference.png")==png&&!state.reference_approved);
 CHECK(!workspace::save_output_copy(*state.reference,root/"kept-reference.png",reason,{}));
 CHECK(contents(root/"kept-reference.png")==png);
 CHECK(!workspace::save_output_copy(*state.reference,root/"wrong.glb",reason,{}));
 CHECK(!workspace::save_output_copy(*state.reference,fs::path{"relative.png"},reason,{}));
 std::stop_source cancelledExport;cancelledExport.request_stop();
 CHECK(!workspace::save_output_copy(*state.reference,root/"cancelled.png",reason,cancelledExport.get_token()));
 CHECK(!fs::exists(root/"cancelled.png"));
 workspace::Completion exported;exported.output_copy=true;exported.result.diagnostic="Saved verified copy";
 const auto originalImage=state.reference->result.output_sha256;
 workspace::accept_completion(state,std::move(exported));
 CHECK(state.reference&&state.reference->result.output_sha256==originalImage&&!state.reference_approved&&!state.generated_model);
 CHECK(workspace::approve_reference(state,reason)&&state.reference_approved);
 file(root/"reference.png","changed");CHECK(!workspace::approve_reference(state,reason)&&!state.reference_approved);file(root/"reference.png",png);
 file(root/"reference.png","changed");CHECK(!workspace::save_output_copy(*state.reference,root/"changed.png",reason,{}));
 CHECK(!fs::exists(root/"changed.png"));file(root/"reference.png",png);
 CHECK(workspace::approve_reference(state,reason));
 file(root/"settings.json",R"({"schema":"epoch.asset_job.v1","action":"generate_2d","image":{"prompt":"one ceramic lantern","width":512,"height":512,"steps":4,"seed":72},"backend":{"executable":"missing.exe","bytes":1,"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"models_cache_root":"missing-cache","work_directory":"unused"})");
 CHECK(workspace::load_settings(state,root/"settings.json",jobs::Action::image));CHECK(state.image_seed=="72");
 CHECK(!workspace::load_settings(state,root/"settings.json",jobs::Action::model));CHECK(!state.model_settings);
 // Selecting a generator after writing a prompt must keep the visible request.
 workspace::State authored;authored.image_request.prompt="white birch texture";
 authored.image_request.negative_prompt="no text or border";authored.image_seed="95";
 authored.image_request.width=512;authored.image_request.height=768;authored.image_request.steps=3;
 CHECK(workspace::load_settings(authored,root/"settings.json",jobs::Action::image));
 CHECK(authored.image_settings&&authored.image_request.prompt=="white birch texture"
  &&authored.image_request.negative_prompt=="no text or border"&&authored.image_seed=="95"
  &&authored.image_request.width==512&&authored.image_request.height==768&&authored.image_request.steps==3);
 // Actual setup dispatch + real request-file reader. An accepted local JSON
 // selection configures a stage without loading models or starting a worker.
 DraftAdapter setup;draft_dialog::selected.path=root/"settings.json";
 actual_draft_dispatch(setup,workspace::Action::load_image_settings);
 CHECK(setup.assetGeneration.image_settings&&setup.assetGeneration.image_seed=="72"&&!setup.assetGeneration.model_settings);
 CHECK(setup.assetGeneration.image_settings_path==root/"settings.json"&&!asset_messages.empty());
 const auto remembered=root/"cache/asset-generation/selected-backends.epoch-generation";
 CHECK(fs::is_regular_file(remembered));const auto rememberedBytes=contents(remembered);
 CHECK(rememberedBytes.find("one ceramic lantern")==std::string::npos&&rememberedBytes.find("reference.png")==std::string::npos);
 CHECK(rememberedBytes.find("reference_approved")==std::string::npos&&rememberedBytes.find("terms_accepted")==std::string::npos);
 DraftAdapter restarted;restarted.assetGeneration.image_request.prompt="Keep this newly typed prompt";
 restarted.assetGeneration.reference=state.reference;restarted.assetGeneration.reference_approved=true;
 restarted.assetGeneration.active_work_directory=root/"keep-current-work";
 const auto phase=restarted.assetGeneration.phase;
 restore_asset_generation_backend_selection(restarted);
 CHECK(restarted.assetGeneration.image_settings&&restarted.assetGeneration.image_settings_path==root/"settings.json");
 CHECK(restarted.assetGeneration.image_request.prompt=="Keep this newly typed prompt"&&restarted.assetGeneration.reference_approved);
 CHECK(restarted.assetGeneration.reference->result.output_sha256==state.reference->result.output_sha256
  &&restarted.assetGeneration.active_work_directory==root/"keep-current-work"&&restarted.assetGeneration.phase==phase);
 CHECK(restarted.assetGeneration.backend_selection_attempted&&native::handoffs==0&&test_child::launches==launches);
 const auto restoredSettings=restarted.assetGeneration.image_settings_sha256;
 restore_asset_generation_backend_selection(restarted);CHECK(restarted.assetGeneration.image_settings_sha256==restoredSettings);
 auto selected=setup.assetGeneration;draft_dialog::selected.accepted=false;draft_dialog::selected.cancelled=true;
 actual_draft_dispatch(setup,workspace::Action::load_image_settings);
 CHECK(contents(remembered)==rememberedBytes);
 CHECK(setup.assetGeneration.image_settings_path==selected.image_settings_path&&setup.assetGeneration.image_request.prompt==selected.image_request.prompt);
 draft_dialog::selected.accepted=true;draft_dialog::selected.cancelled=false;
 actual_draft_dispatch(setup,workspace::Action::load_model_settings);
 CHECK(contents(remembered)==rememberedBytes);
 CHECK(!setup.assetGeneration.model_settings&&setup.assetGeneration.image_settings_path==selected.image_settings_path);
 CHECK(setup.assetGeneration.status.find("other generation stage")!=std::string::npos);
 CHECK(workspace::readiness_diagnostic(setup.assetGeneration,jobs::Action::image).empty());
 auto startPlan=workspace::prepare_job(setup.assetGeneration,jobs::Action::image,root/"jobs/accepted-ui-request",reason);
 CHECK(startPlan&&reason.empty()&&startPlan->image.prompt=="one ceramic lantern"&&startPlan->image.seed==72u);
 CHECK(startPlan->image_backend.expected_executable.sha256==std::string(64,'a'));
 CHECK(startPlan->work_directory==root/"jobs/accepted-ui-request"&&!fs::exists(startPlan->work_directory));
 CHECK(native::handoffs==0&&test_admission::samples==hardware&&epoch_ai::process::queries==queries&&test_child::launches==launches);
 // Real bounded draft codec + real exclusive/atomic filesystem owners. No
 // callbacks, model loading, hardware query, source context or approval restore.
 const auto settingsBytes=contents(root/"settings.json");
 state.image_request.prompt="Amber \"glass\"\nCopper\\handle — 樹";
 state.image_request.negative_prompt="no watermark\nno writing";state.image_request.width=512;state.image_request.height=512;
 state.model_request.face_budget=18000;state.model_request.atlas=512;state.model_seed="91";
 const auto draft=root/fs::path{u8"生成_樹.epoch-generation"};
 CHECK(workspace::save_draft(state,draft,reason));const auto draftBytes=contents(draft);
 CHECK(draftBytes.find("EPOCH_GENERATION_DRAFT 1")==0);
 CHECK(draftBytes.find("reference.png")==std::string::npos&&draftBytes.find(hash(png))==std::string::npos);
 workspace::State restored;restored.reference=state.reference;restored.reference_approved=true;
 CHECK(workspace::load_draft(restored,draft,reason));
 CHECK(restored.image_request.prompt==state.image_request.prompt&&restored.image_request.negative_prompt==state.image_request.negative_prompt);
 CHECK(restored.image_seed=="72"&&restored.model_seed=="91"&&restored.model_request.face_budget==18000&&restored.model_request.atlas==512);
 CHECK(restored.image_settings&&!restored.model_settings&&restored.image_settings_path==root/"settings.json");
 CHECK(!restored.reference&&!restored.generated_model&&!restored.reference_approved);
 CHECK(restored.model_request.reference_path.empty()&&restored.model_request.approved_reference_sha256.empty());
 CHECK(native::handoffs==0&&test_admission::samples==hardware&&epoch_ai::process::queries==queries&&test_child::launches==launches);
 const auto malformed=[&](std::string bad){file(root/"malformed.epoch-generation",bad);
  restored.reference_approved=true;const auto prior=restored.image_request.prompt;
  CHECK(!workspace::load_draft(restored,root/"malformed.epoch-generation",reason));
  CHECK(restored.image_request.prompt==prior&&restored.image_settings&&restored.reference_approved);};
 const auto changed=[&](std::string old,std::string replacement){auto copy=draftBytes;
  const auto at=copy.find(old);CHECK(at!=std::string::npos);copy.replace(at,old.size(),replacement);return copy;};
 malformed(changed("EPOCH_GENERATION_DRAFT 1","EPOCH_GENERATION_DRAFT 2"));
 malformed(changed("image_size 512 512 4","image_size -1 512 4"));
 malformed(changed("image_size 512 512 4","image_size 512.5 512 4"));
 malformed(changed("image_size 512 512 4","image_size 4294967296 512 4"));
 malformed(changed("image_size 512 512 4","image_size 8192 512 4"));
 malformed(changed("image_seed \"72\"","image_seed \"4294967296\""));
 malformed(changed("model_limits 512 12 18000 512","model_limits 512 12 18000 513"));
 malformed(changed("image_prompt \"","image_prompt unquoted \""));
 malformed(draftBytes+"image_seed \"1\"\n");malformed(std::string(65537,'x'));
 malformed(changed("Copper",std::string(1,'\0')));malformed(changed("Copper",std::string(1,char(0xff))));
 restored.phase->store(workspace::Stage::generating);CHECK(!workspace::load_draft(restored,draft,reason));restored.phase->store(workspace::Stage::idle);
 auto changedSettings=settingsBytes;const auto oldSeed=changedSettings.find("\"seed\":72");CHECK(oldSeed!=std::string::npos);
 changedSettings.replace(oldSeed,9,"\"seed\":73");file(root/"settings.json",changedSettings);
 CHECK(workspace::load_draft(restored,draft,reason));CHECK(!restored.image_settings&&restored.image_request.prompt==state.image_request.prompt);
 CHECK(!restored.reference_approved&&reason.find("changed or missing 2D")!=std::string::npos);
 file(root/"settings.json",settingsBytes);CHECK(workspace::load_draft(restored,draft,reason)&&restored.image_settings);
 const auto unicodeSettings=root/fs::path{u8"設定_樹.json"};file(unicodeSettings,settingsBytes);
 CHECK(workspace::load_settings(restored,unicodeSettings,jobs::Action::image));restored.image_request.prompt="Unicode backend";
 CHECK(workspace::save_draft(restored,draft,reason));workspace::State unicodeRestored;
 CHECK(workspace::load_draft(unicodeRestored,draft,reason)&&unicodeRestored.image_settings_path==unicodeSettings&&unicodeRestored.image_settings);
 auto invalidDraft=restored;invalidDraft.image_request.width=1;const auto published=contents(draft);
 CHECK(!workspace::save_draft(invalidDraft,draft,reason)&&contents(draft)==published);
 CHECK(!workspace::save_draft(restored,root/"settings.json",reason)&&contents(root/"settings.json")==settingsBytes);
 Json modelSettings={{"schema","epoch.asset_job.v1"},{"action","generate_3d"},
  {"models_cache_root",(root/"shared/model-cache").generic_string()},{"work_directory","unused"},
  {"model",{{"reference_path",(root/"reference.png").generic_string()},{"approved_reference_sha256",hash(png)}}},
  {"backend",{{"executable","missing.exe"},{"bytes",1},{"sha256",std::string(64,'a')},{"dinov3_terms_accepted",true}}}};
 file(root/"model-settings.json",modelSettings.dump());workspace::State withConsent;
 CHECK(workspace::load_settings(withConsent,root/"model-settings.json",jobs::Action::model));
 CHECK(withConsent.model_settings&&!withConsent.model_settings->model_backend.dinov3_terms_accepted);
 withConsent.model_settings->model_backend.dinov3_terms_accepted=true;
 withConsent.reference=state.reference;withConsent.reference_approved=true;
 CHECK(workspace::save_draft(withConsent,root/"model.epoch-generation",reason));
 CHECK(contents(root/"model.epoch-generation").find("dinov3_terms_accepted")==std::string::npos);
 CHECK(workspace::load_draft(restored,root/"model.epoch-generation",reason));
 CHECK(restored.model_settings&&!restored.model_settings->model_backend.dinov3_terms_accepted&&!restored.reference&&!restored.reference_approved);
 // Restoration is stage-independent and never replaces a current selection,
 // prompt, output, approval, active phase or job identity.
 auto both=state;both.model_settings=withConsent.model_settings;
 both.model_settings_path=withConsent.model_settings_path;both.model_settings_sha256=withConsent.model_settings_sha256;
 CHECK(workspace::remember_backend_selection(both,remembered,reason));const auto combinedPins=contents(remembered);
 CHECK(combinedPins.find("one ceramic lantern")==std::string::npos&&combinedPins.find("reference.png")==std::string::npos);
 workspace::State busySelection;busySelection.phase->store(workspace::Stage::generating);
 CHECK(!workspace::restore_backend_selection(busySelection,remembered,reason)&&!busySelection.backend_selection_attempted&&!busySelection.image_settings);
 busySelection.phase->store(workspace::Stage::idle);CHECK(workspace::restore_backend_selection(busySelection,remembered,reason));
 CHECK(busySelection.image_settings&&busySelection.model_settings&&!busySelection.model_settings->model_backend.dinov3_terms_accepted);
 workspace::State keepSelected;keepSelected.image_settings=state.image_settings;keepSelected.image_settings_path=root/"current-explicit.json";
 keepSelected.image_settings_sha256=std::string(64,'b');keepSelected.reference=state.reference;keepSelected.reference_approved=true;
 keepSelected.image_request.prompt="Current prompt";
 file(root/"settings.json","changed settings");
 CHECK(workspace::restore_backend_selection(keepSelected,remembered,reason));
 CHECK(keepSelected.image_settings_path==root/"current-explicit.json"&&keepSelected.image_settings_sha256==std::string(64,'b'));
 CHECK(keepSelected.model_settings&&!keepSelected.model_settings->model_backend.dinov3_terms_accepted&&keepSelected.reference_approved&&keepSelected.image_request.prompt=="Current prompt");
 workspace::State missingImage;CHECK(workspace::restore_backend_selection(missingImage,remembered,reason));
 CHECK(!missingImage.image_settings&&missingImage.model_settings&&reason.find("remembered 2D")!=std::string::npos);
 file(root/"settings.json",settingsBytes);
 const auto modelSettingsBytes=contents(root/"model-settings.json");file(root/"model-settings.json","changed 3D settings");
 workspace::State missingModel;CHECK(workspace::restore_backend_selection(missingModel,remembered,reason));
 CHECK(missingModel.image_settings&&!missingModel.model_settings&&reason.find("remembered 3D")!=std::string::npos);
 file(root/"model-settings.json",modelSettingsBytes);
 file(remembered,"malformed");workspace::State malformedSelection;malformedSelection.image_request.prompt="Retain this";
 CHECK(!workspace::restore_backend_selection(malformedSelection,remembered,reason)&&!malformedSelection.image_settings&&malformedSelection.image_request.prompt=="Retain this");
 file(remembered,combinedPins);workspace::State none;CHECK(!workspace::restore_backend_selection(none,root/"not-saved.epoch-generation",reason)&&reason.empty());
 DraftAdapter adapter;adapter.assetGeneration=state;adapter.assetGeneration.preview_markers.emplace_back();
 draft_dialog::selected.path=root/"model.epoch-generation";const auto dialogs=draft_dialog::opens;
 adapter.assetGenerationPending=true;actual_draft_dispatch(adapter,workspace::Action::load_draft);
 CHECK(draft_dialog::opens==dialogs&&adapter.assetGeneration.reference_approved&&!adapter.assetGeneration.preview_markers.empty());
 adapter.assetGenerationPending=false;adapter.assetGeneration.phase->store(workspace::Stage::generating);
 actual_draft_dispatch(adapter,workspace::Action::load_draft);CHECK(adapter.assetGeneration.reference_approved&&!adapter.assetGeneration.preview_markers.empty());
 adapter.assetGeneration.phase->store(workspace::Stage::idle);actual_draft_dispatch(adapter,workspace::Action::load_draft);
 CHECK(!adapter.assetGeneration.reference&&!adapter.assetGeneration.generated_model&&!adapter.assetGeneration.reference_approved&&adapter.assetGeneration.preview_markers.empty());
 CHECK(adapter.assetGenerationFocusPending); // Existing publisher cannot republish any prior model from this fresh state.
 draft_dialog::selected.path=root/"adapter.epoch-generation";actual_draft_dispatch(adapter,workspace::Action::save_draft);
 CHECK(draft_dialog::saves==1&&fs::is_regular_file(root/"adapter.epoch-generation"));
 CHECK(!workspace::save_draft(restored,fs::path{"relative.epoch-generation"},reason));
 fs::create_directory(root/"directory.epoch-generation");CHECK(!workspace::save_draft(restored,root/"directory.epoch-generation",reason));
 fs::create_directories(root/"outside");std::error_code linkError;fs::create_directory_symlink(root/"outside",root/"redirected",linkError);
 if(!linkError){CHECK(!workspace::save_draft(restored,root/"redirected/escape.epoch-generation",reason));CHECK(!fs::exists(root/"outside/escape.epoch-generation"));
  file(root/"outside/backend.json",settingsBytes);auto redirectedPins=state;
  redirectedPins.image_settings_path=root/"redirected/backend.json";redirectedPins.image_settings_sha256=hash(settingsBytes);
  CHECK(workspace::remember_backend_selection(redirectedPins,root/"redirected-pins.epoch-generation",reason));
  workspace::State redirected;CHECK(!workspace::restore_backend_selection(redirected,root/"redirected-pins.epoch-generation",reason)&&!redirected.image_settings);
 }
 std::error_code fileLink;fs::create_symlink(remembered,root/"selection-link.epoch-generation",fileLink);
 if(!fileLink){workspace::State redirected;CHECK(!workspace::restore_backend_selection(redirected,root/"selection-link.epoch-generation",reason)&&!redirected.image_settings);}
 DraftAdapter ownedJob;ownedJob.assetGenerationPending=true;restore_asset_generation_backend_selection(ownedJob);
 CHECK(!ownedJob.assetGeneration.backend_selection_attempted&&!ownedJob.assetGeneration.image_settings);
 ownedJob.assetGenerationPending=false;restore_asset_generation_backend_selection(ownedJob);CHECK(ownedJob.assetGeneration.image_settings);
 CHECK(native::handoffs==0&&test_admission::samples==hardware&&epoch_ai::process::queries==queries&&test_child::launches==launches);
 state.image_seed="4294967296";CHECK(!workspace::prepare_job(state,jobs::Action::image,root/"jobs/no",reason));
 state.image_seed="1.5";CHECK(!workspace::prepare_job(state,jobs::Action::image,root/"jobs/no",reason));
 state.image_seed="0";state.image_request.width=513;CHECK(!workspace::prepare_job(state,jobs::Action::image,root/"jobs/no",reason));
 state.image_request.width=512;reason.clear();auto imageJob=workspace::prepare_job(state,jobs::Action::image,root/"jobs/no",reason);CHECK(imageJob&&imageJob->image.seed==0);
 auto invalid=workspace::execute(*imageJob,project,state.phase,{});CHECK(!invalid.result&&native::handoffs==0&&state.phase->load()==workspace::Stage::idle);
 CHECK(test_admission::samples==hardware&&epoch_ai::process::queries==queries&&test_child::launches==launches);
 jobs::PreparedJob backend;backend.ready=true;backend.action=jobs::Action::model;backend.backend_name="explicit CPU worker port";
 backend.alternative_model_backend=jobs::ModelBackend{.id="fixture-supervisor-port",.revision="explicit CPU fixture",
  .model_package_ids={"os_model_trellis_2_cpp_q4_512"},.terms_accepted=true,.preflight=[](const auto&){return std::string{};},
  .prepare=[&](const auto& approved,const fs::path& work){
   CHECK(approved.reference_path==work/"approved-reference.png"&&contents(approved.reference_path)==png);
   epochengine::ai::model_generation::Invocation invocation;invocation.ready=true;invocation.output=work/"generated.glb";
   invocation.required_face_budget=approved.face_budget;invocation.reference_sha256=approved.approved_reference_sha256;
   invocation.launch.executable=root/"cpu-worker.exe";invocation.launch.expected_executable={32,std::string(64,'a')};
   invocation.launch.working_directory=work;invocation.launch.merged_output_path=work/"generation.log";
   invocation.launch.disconnect_standard_input=true;invocation.launch.arguments={invocation.output.generic_string()};return invocation;}};
 state.model_settings=backend;state.model_seed="42";
 CHECK(workspace::prepare_job(state,jobs::Action::model,root/"jobs/model",reason));state.reference_approved=false;
 CHECK(!workspace::prepare_job(state,jobs::Action::model,root/"jobs/model",reason));state.reference_approved=true;
 reason.clear();auto job=workspace::prepare_job(state,jobs::Action::model,root/"jobs/model",reason);CHECK(job);
 CHECK(job->model.reference_path==root/"reference.png"&&job->model.approved_reference_sha256==hash(png));
 auto opaque=fixtureSource;auto& material=opaque.json["materials"][0];
 for(const char* key:{"normalTexture","occlusionTexture","emissiveTexture","emissiveFactor"})material.erase(key);
 material["alphaMode"]="OPAQUE";material["doubleSided"]=false;auto& pbr=material["pbrMetallicRoughness"];
 pbr.erase("metallicRoughnessTexture");pbr["baseColorFactor"]=Json::array({1,1,1,1});pbr["metallicFactor"]=0;pbr["roughnessFactor"]=1;
 test_child::payload=glb(opaque);auto completed=workspace::execute(*job,project,state.phase,{});
 CHECK(completed.result&&completed.model&&completed.coding_restored&&completed.retirement_verified&&native::handoffs==1);
 CHECK(completed.reference_sha256==hash(png)&&completed.backend==backend.backend_name&&!completed.result.compiled_assets.empty());
 workspace::accept_completion(state,std::move(completed));CHECK(state.generated_model&&state.generated_model->model&&state.reference_approved);
 const auto savedOutput=state.generated_model->result.output;
 // New exact-project/edition record restores actual PNG and GLB decoders,
 // never prior approval, callbacks, backend terms or native model residency.
 fs::create_directory(root/"recall");const auto recall=root/"recall/last-completed.epoch-generation-result";
 CHECK(!workspace::begin_recall(state,recall,project));
 workspace::accept_completion(state,*state.generated_model);CHECK(fs::is_regular_file(recall));
 const auto recallBytes=contents(recall),savedGlb=contents(savedOutput);
 CHECK(recallBytes.find("reference_approved")==std::string::npos&&recallBytes.find("terms_accepted")==std::string::npos);
 workspace::State reopened;auto nextSession=project;++nextSession.generation;
 CHECK(workspace::begin_recall(reopened,recall,nextSession));CHECK(!workspace::begin_recall(reopened,recall,nextSession));
 auto recovered=workspace::restore_last_completed(recall,nextSession,{});
 CHECK(recovered.result&&recovered.model&&recovered.recalled_reference&&recovered.recalled_reference->image);
 CHECK(recovered.result.output==savedOutput&&recovered.reference_sha256==hash(png)&&recovered.result.faces>0);
 workspace::accept_completion(reopened,std::move(recovered));
 CHECK(reopened.reference&&reopened.generated_model&&!reopened.reference_approved&&reopened.preview_dirty);
 CHECK(reopened.reference->project==nextSession&&reopened.generated_model->project==nextSession);
 CHECK(!reopened.image_settings&&!reopened.model_settings&&reopened.model_request.approved_reference_sha256.empty());
 const auto recalledModel=reopened.generated_model->result.output_sha256;
 const auto rejectRecall=[&](std::string bytes){file(recall,bytes);auto failure=workspace::restore_last_completed(recall,nextSession,{});
  CHECK(!failure.result);workspace::accept_completion(reopened,std::move(failure));
  CHECK(reopened.generated_model&&reopened.generated_model->result.output_sha256==recalledModel&&!reopened.reference_approved);};
 auto badRecall=Json::parse(recallBytes);badRecall["edition"]=1-EPOCH_SOURCE_EDITION_PUBLIC;rejectRecall(badRecall.dump());
 badRecall=Json::parse(recallBytes);badRecall["project_id"]="other";rejectRecall(badRecall.dump());
 badRecall=Json::parse(recallBytes);badRecall["model"]["sha256"]=std::string(64,'a');rejectRecall(badRecall.dump());
 badRecall=Json::parse(recallBytes);badRecall["model"]["reference_sha256"]=std::string(64,'a');rejectRecall(badRecall.dump());
 badRecall=Json::parse(recallBytes);badRecall["reference"]["approved"]=true;rejectRecall(badRecall.dump());
 rejectRecall("{\"schema\":\"one\",\"schema\":\"two\"}");rejectRecall(std::string(65537,'x'));
 file(recall,recallBytes);file(savedOutput,"tampered");auto corrupt=workspace::restore_last_completed(recall,nextSession,{});CHECK(!corrupt.result);
 workspace::accept_completion(reopened,std::move(corrupt));CHECK(reopened.generated_model->result.output_sha256==recalledModel);
 file(savedOutput,savedGlb);fs::rename(savedOutput,root/"model-held.glb");CHECK(!workspace::restore_last_completed(recall,nextSession,{}).result);
 fs::rename(root/"model-held.glb",savedOutput);std::stop_source recallStop;recallStop.request_stop();
 CHECK(!workspace::restore_last_completed(recall,nextSession,recallStop.get_token()).result);
 const auto priorHandoffs=native::handoffs,priorLaunches=test_child::launches;
 auto oldResult=workspace::inspect_completed_generation(state.generated_model->result.receipt,nextSession,{});
 CHECK(oldResult.result&&oldResult.model&&oldResult.recalled_reference&&oldResult.recalled_reference->image);
 CHECK(oldResult.result.output_sha256==recalledModel&&oldResult.recalled_workspace);
 auto legacyRecord=Json::parse(contents(state.generated_model->result.receipt));legacyRecord["output"]=(root/"outside.glb").generic_string();
 file(root/"foreign-result.json",legacyRecord.dump());CHECK(!workspace::inspect_completed_generation(root/"foreign-result.json",nextSession,{}).result);
 CHECK(native::handoffs==priorHandoffs&&test_child::launches==priorLaunches);
 auto second=*job;second.work_directory=root/"jobs/model-restore-failed";native::restore_ok=false;
 auto restoreFailure=workspace::execute(second,project,state.phase,{});CHECK(restoreFailure.result&&!restoreFailure.coding_restored&&restoreFailure.model);
 workspace::accept_completion(state,std::move(restoreFailure));CHECK(state.status.find("restore failed")!=std::string::npos&&fs::exists(savedOutput));native::restore_ok=true;
 std::stop_source stop;stop.request_stop();second.work_directory=root/"jobs/cancelled";
 const auto starts=test_child::launches;auto cancelled=workspace::execute(second,project,state.phase,stop.get_token());CHECK(!cancelled.result&&test_child::launches==starts);
 std::stop_source late;native::cancel_after=&late;second.work_directory=root/"jobs/cancel-after-output";
 auto lateResult=workspace::execute(second,project,state.phase,late.get_token());CHECK(!lateResult.result&&lateResult.result.code==jobs::Code::cancelled&&fs::exists(lateResult.result.output));native::cancel_after=nullptr;
 second.work_directory=root/"jobs/prelaunch-refused";second.alternative_model_backend->preflight=[](const auto&){return std::string{"missing backend"};};
 const auto calls=native::handoffs;auto refused=workspace::execute(second,project,state.phase,{});CHECK(!refused.result&&native::handoffs==calls);
 CHECK(!fs::exists(root/"Assets")&&!fs::exists(root/"worlds")&&!native::idle::work_gate.protected_work());
}
'''


class AssetGenerationWorkspace(unittest.TestCase):
    def test_real_controller_and_job_owner_debug_release(self):
        started=time.time_ns()
        newest=jobs_test.run_fixture(self,workspace_fixture,ROOT)
        result=json.loads((newest/"result.json").read_text())
        report=ROOT/"Artifacts/Reports/asset-generation-workspace-oct28"
        report.mkdir(parents=True,exist_ok=True)
        (report/"controller-result.json").write_text(json.dumps({"passed":True,"elapsed_seconds":(time.time_ns()-started)/1e9,
            "actual_job_fixture":str(newest),"commands":result["commands"],"owners":{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in (
                "Engine/modules/editor/editor.asset_generation.ixx","Engine/src/editor/editor.asset_generation.cpp","Engine/modules/ai/ai.asset_jobs.ixx","Engine/src/ai/ai.asset_jobs.cpp")},
            "scope":"Actual controller and job production algorithms, real PNG/GLB/artifact/hash decoders. Explicit GUI type and native handoff/hardware/supervisor ports; no GUI/native worker/model/GPU. Native draw and actual model restoration require separate integration acceptance."},indent=2),encoding="utf-8")

if __name__=="__main__":unittest.main()
