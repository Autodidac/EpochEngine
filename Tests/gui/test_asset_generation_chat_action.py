"""Actual literal-image ingress, immutable control binding and dispatch gates.

CPU only. Recording GUI/scheduling ports never start a worker, model or renderer.
"""
from pathlib import Path
import hashlib
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class ChatImageAction(unittest.TestCase):
    def test_actual_binding_literal_ingress_and_dispatch(self):
        paths = {
            "generation": ROOT / "Engine/src/editor/editor.asset_generation.cpp",
            "interface": ROOT / "Engine/modules/editor.asset_generation.ixx",
            "planning": ROOT / "Engine/src/editor/application/editor.frame_assistant_planning.inl",
            "runtime": ROOT / "Engine/src/editor/application/editor.frame_project_runtime.inl",
            "conversation": ROOT / "Engine/src/editor/application/editor.frame_assistant_conversation.inl",
            "state": ROOT / "Engine/src/editor/application/editor.application_state.inl",
            "mcp": ROOT / "Engine/modules/ai.mcp.ixx",
            "image": ROOT / "Engine/src/ai/ai.image_generation.cpp",
            "image_interface": ROOT / "Engine/modules/ai.image_generation.ixx",
            "model": ROOT / "Engine/src/ai/ai.model_generation.cpp",
            "model_interface": ROOT / "Engine/modules/ai.model_generation.ixx",
            "sha": ROOT / "Engine/modules/core.sha256.ixx",
            "process": ROOT / "Engine/modules/platform.child_process.ixx",
            "queue": ROOT / "Engine/src/editor/ai/editor.ai_chat_composer_queue.hpp",
            "agent_routing": ROOT / "Engine/src/editor/ai/editor.ai_agent_routing.hpp",
        }
        owners = {key: path.read_text(encoding="utf-8") for key, path in paths.items()}
        hashes = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in paths.values()}
        ingress = definition(owners["planning"], "            if (ai::asset_generation_tool(generationTool))")
        tool_owner = definition(owners["planning"], "        auto request_ai_tool_plan =")
        preflight = tool_owner[tool_owner.index("{")+1:tool_owner.index("            chat.statusRequestKind =")]
        binding_start = tool_owner.index('            chat.append_user_message("Tool plan: "')
        binding_setup = tool_owner[binding_start:tool_owner.index("            if (ai::asset_generation_tool(generationTool))")]
        self.assertLess(tool_owner.index("editor.assetGenerationPending"), binding_start)
        self.assertLess(tool_owner.index('ai::asset_generation_tool(editor.aiToolExecution)'), binding_start)
        text_action = definition(owners["planning"], "        const auto queue_text_message_action =", True)
        branch = definition(owners["runtime"], '            else if(ai::asset_generation_tool(call.tool))')
        gate_start = owners["runtime"].index("            if (const auto expected = ai::project_lifecycle_request_tool(editor.aiToolBoundRequest);")
        gate_end = owners["runtime"].index('            if((call.tool=="forest.configure"', gate_start)
        gates = owners["runtime"][gate_start:gate_end]
        watch = definition(owners["runtime"], "            auto watch_execution =", True)
        image_finish = definition(owners["runtime"], '                else if(ai::asset_generation_tool(editor.aiToolExecution))')
        execution_owner = definition(owners["runtime"], '        if (!editor.aiToolExecution.empty())')
        stale_finish = definition(execution_owner, '                if (!current)')
        queue_start = owners["conversation"].index("        const bool queuedAssetHostAction =")
        queue_literal = owners["conversation"][queue_start:owners["conversation"].index(";", queue_start)+1]
        dispatch_start = owners["conversation"].index("        if (queueOwnerReady && queuedMethodMatches")
        queue_dispatch = owners["conversation"][dispatch_start:owners["conversation"].index("\n        {", dispatch_start)].strip()[3:]
        self.assertNotIn("submit_ai_request", ingress)
        self.assertNotIn("dispatch_asset_generation", ingress)
        self.assertNotIn("confirm_model", ingress + branch + queue_literal)
        self.assertIn("gui::replace_text_in_edit_box", ingress)
        self.assertLess(ingress.index("replace_text_in_edit_box"), ingress.index("image_request_binding"))
        mcp = owners["mcp"].replace("module;", "").replace("export module ai.mcp;", "").replace("export namespace", "namespace")
        sha = owners["sha"].replace("module;", "").replace("export module core.sha256;", "").replace("export namespace", "namespace")
        code = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "core.utf8.hpp"
#include "editor.ai_chat_composer_queue.hpp"
ACTUAL_SHA
ACTUAL_MCP
namespace ai=epochengine::ai;
namespace queue=epochengine::editor::ai_chat_queue;
namespace epochengine::platform::child_process {ACTUAL_EXECUTABLE_IDENTITY}
namespace epochengine::ai::image_generation {
ACTUAL_IMAGE_REQUEST
ACTUAL_IMAGE_CONFIGURATION
ACTUAL_IMAGE_VALID_TEXT
ACTUAL_IMAGE_VALIDATION
}
namespace epochengine::ai::model_generation {
ACTUAL_MODEL_REQUEST
ACTUAL_MODEL_CONFIGURATION
ACTUAL_MODEL_DIGEST_VALID
ACTUAL_MODEL_VALIDATION
}
namespace epochengine::ai::asset_jobs {
enum class Action:std::uint8_t{image,model};
struct ModelBackend{std::string id,revision;bool terms_accepted{};};
struct PreparedJob {Action action{Action::image};image_generation::Request image;image_generation::Configuration image_backend;
 model_generation::Request model;model_generation::Configuration model_backend;std::optional<ModelBackend>alternative_model_backend;
 std::filesystem::path work_directory;std::string backend_name;};
}
namespace editor_asset_generation {
namespace jobs=ai::asset_jobs;namespace core=epochengine::core;
enum class Stage:std::uint8_t{idle};enum class View:std::uint8_t{textured,solid,wireframe};
enum class Action:std::uint8_t{generate_image,generate_model};
struct SpriteHandle{};namespace previewgrid{struct ObjectMarker{};}
struct ProjectIdentity{std::string root,id;std::uint64_t generation{};bool operator==(const ProjectIdentity&)const=default;};
struct Completion{bool image{},coding_restored{},retirement_verified{};struct{
 std::filesystem::path output,work_directory;std::string output_sha256;bool valid{};
 explicit operator bool()const{return valid;}}result;ProjectIdentity project;};
ACTUAL_STATE
ACTUAL_PATH_TEXT
ACTUAL_DIGEST
ACTUAL_READINESS
ACTUAL_IMAGE_BINDING
ACTUAL_CURRENT_PROJECT
ACTUAL_MODEL_BINDING
ACTUAL_PREPARE
}
enum class EditorMainSurface{Scene,Project,AssetGeneration};
enum class AiDeferredRequestKind{None,Chat,Tooling};
namespace editor_ai_agent_routing{ACTUAL_ROUTING_TYPES}
struct CodexSession{bool pending{};std::vector<bool>decisions;
 bool approval_pending()const{return pending;}void answer_approval(bool value){decisions.push_back(value);pending=false;}};
struct GuiDoc{struct Revision{std::uint64_t sequence{};}value;const Revision& revision()const{return value;}};
struct Editor {
 editor_asset_generation::State assetGeneration;
 bool assetGenerationPending{},projectBuildPending{},scriptBuildPending{},forestSurfacePending{},planetBusy{};
 bool aiToolPlanApplied{},aiToolAwaitingReply{};
 AiDeferredRequestKind aiDeferredRequestKind{AiDeferredRequestKind::None};
 int aiMethodSelection{};std::shared_ptr<CodexSession>aiCodexSession;
 editor_ai_agent_routing::Settings aiAgentRouting;
 bool aiAuthoringPlanApplied{},aiAuthoringApplyPending{};
 std::string aiProjectScriptDraft;ai::ToolPlanParseResult aiAuthoringPlan;
 std::optional<int>aiToolTestPending;unsigned aiQueuedMessageActionIndex{99};
 std::string aiToolStatus,aiToolAssetBinding,aiToolBoundRequest,aiToolExecution;
 std::filesystem::path aiToolAssetWork;
 std::uint64_t aiToolAssetActivityId{};
 ai::ToolPlanParseResult aiToolPlan;
 std::optional<EditorMainSurface>requestedMainSurface;
 unsigned projectWorkspaceSection{};
 std::string projectId{"fixture"},projectRoot{"project-root"},aiToolProjectId{"fixture"},aiToolProjectRoot{"project-root"};
 std::uint64_t aiToolProjectGeneration{4},aiToolSceneRevision{8},sceneDocumentRevision{8},aiToolGuiRevision{6};
 std::string aiToolExecutionProjectId,aiToolExecutionProjectRoot;
 std::uint64_t aiToolExecutionProjectGeneration{},aiToolExecutionBuildGeneration{};
 std::shared_ptr<GuiDoc>guiDocument=std::make_shared<GuiDoc>(GuiDoc{{6}});
 struct Lifecycle{struct Evidence{struct Generations{struct{std::uint64_t generation{4};}selected_project;
 struct{std::uint64_t build_generation{};}active_build;}generations;}value;
 const Evidence& evidence()const{return value;}}projectLifecycle;
};
using EditorState=Editor;
editor_asset_generation::ProjectIdentity asset_generation_project(const Editor& editor){
 return {editor.projectRoot,editor.projectId,editor.projectLifecycle.evidence().generations.selected_project.generation};}
std::string display_project_path(const std::filesystem::path&path){return path.generic_string();}
unsigned backendRestores{};
void restore_asset_generation_backend_selection(Editor&){++backendRestores;}
struct Chat{std::vector<int>activeReferenceImages;std::vector<std::string>messages,userMessages;std::string queuedInput,input;
 bool pending{};AiDeferredRequestKind statusRequestKind{AiDeferredRequestKind::Chat};
 std::uint64_t inlineHostActivityId{37};bool lastActivityFailed{};unsigned activityEnds{};
 struct Activity{bool stream_live{true};}activity;
 Activity*ensure_host_activity(){return inlineHostActivityId?&activity:nullptr;}
 void end_host_activity(bool failed=false,bool=false){activity.stream_live=false;lastActivityFailed=failed;++activityEnds;inlineHostActivityId=0;}
 void append_status(std::string text){messages.push_back(std::move(text));}
 void append_user_message(std::string text,std::string={}){userMessages.push_back(std::move(text));}};
namespace gui {std::string oldPrompt;unsigned replacements{};
 bool replace_text_in_edit_box(std::string& value,std::string_view next,std::size_t maximum){
 if(next.size()>maximum)return false;oldPrompt=value;value=next;++replacements;return true;}}
ai::ToolPlanParseResult admit_ai_project_tool_reply(std::string_view,std::string_view reply){return ai::parse_tool_plan(reply);}
ACTUAL_CONFIRMATION
bool planetary_project_busy(const Editor& editor){return editor.planetBusy;}
unsigned dispatched{};bool schedulerAccepts=true;std::optional<ai::asset_jobs::PreparedJob>lastJob;
void dispatch_asset_generation(Editor& editor,int,editor_asset_generation::Action action){
 ++dispatched;
 const auto kind=action==editor_asset_generation::Action::generate_image?ai::asset_jobs::Action::image:ai::asset_jobs::Action::model;
 lastJob=editor_asset_generation::prepare_job(editor.assetGeneration,kind,"owned-working-job",editor.assetGeneration.status);
 if(lastJob&&schedulerAccepts){editor.assetGenerationPending=true;editor.assetGeneration.active_work_directory=lastJob->work_directory;}}
bool propose(Editor& editor,Chat& chat,std::string_view request){
 ACTUAL_PREFLIGHT
 ACTUAL_BINDING_SETUP
 ACTUAL_INGRESS return false;
}
bool execute(Editor& editor,Chat& chat){const auto call=editor.aiToolPlan.plan.call;int ctx{};
 ACTUAL_GATES
 ACTUAL_WATCH
 if(false){}ACTUAL_BRANCH
 return true;
}
void completed(Editor&editor,Chat&chat){if(false){}ACTUAL_IMAGE_FINISH}
void stale_completed(Editor&editor,Chat&chat){const bool current=false;ACTUAL_STALE_FINISH}
namespace epochengine::ai{bool is_model_use_confirmed(){return false;}}
std::string_view trim_ai_command(std::string_view value){
 while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.front())))value.remove_prefix(1);
 while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.back())))value.remove_suffix(1);return value;}
bool submit(Editor&editor,Chat&chat,std::string_view input){
 const auto ai_confirmation=[&](std::string_view value){return ai_chat_confirmation(value);};
 #define EPOCH_EDITION_PUBLIC 1
 ACTUAL_TEXT_ACTION
 #undef EPOCH_EDITION_PUBLIC
 if(queue_text_message_action(input))return true;
 if(ai::asset_generation_tool(ai::project_lifecycle_request_tool(input)))return propose(editor,chat,input);
 return false;
}
bool queue_allowed(const Editor& editor,Chat& chat,bool queueOwnerReady,bool queuedMethodMatches,bool queuedModelMatches,const queue::Status& status){
 const bool queuedProviderReady=queue::ready_to_dispatch(status,std::chrono::steady_clock::now());
 const bool queuedIdleReloadConsentReady=false;
 ACTUAL_QUEUE_LITERAL
 return ACTUAL_QUEUE_DISPATCH;
}
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
Editor ready(){Editor editor;auto&state=editor.assetGeneration;state.image_settings.emplace();
 state.image_settings->backend_name="Fixture local image runtime";state.image_settings_path="settings.json";
 state.image_settings_sha256=std::string(64,'a');auto&backend=state.image_settings->image_backend;
 backend.executable="generator.exe";backend.models_cache_root="models";backend.expected_executable.size_bytes=123;
 backend.expected_executable.sha256=std::string(64,'b');state.image_request.width=512;state.image_request.height=512;
 state.image_request.prompt="Old UI draft";state.image_request.negative_prompt="No text, no border";return editor;}
void reset_ports(){dispatched=0;lastJob.reset();schedulerAccepts=true;gui::replacements=0;gui::oldPrompt.clear();}
int main(){
 const std::string prompt="Generate a birch bark texture\ncream-white bark, charcoal marks; seed belongs to the reviewed controls. \"Exact text\"";
 auto editor=ready();Chat chat;reset_ports();
 CHECK(propose(editor,chat,prompt));CHECK(editor.aiToolPlan&&editor.aiToolPlan.plan.call.tool=="asset.image.generate");
 CHECK(editor.assetGeneration.image_request.prompt==prompt&&gui::oldPrompt=="Old UI draft"&&gui::replacements==1);
 CHECK(dispatched==0&&!editor.assetGenerationPending&&!editor.assetGeneration.reference_approved);
 CHECK(editor.aiToolAssetBinding.size()==64&&editor.requestedMainSurface==EditorMainSurface::AssetGeneration);
 CHECK(chat.messages.back().find("No chat-model request was needed")!=std::string::npos);
 CHECK(!chat.activity.stream_live&&chat.activityEnds==0&&!chat.lastActivityFailed&&backendRestores==1);
 const auto original=editor;const auto binding=editor.aiToolAssetBinding;
 std::string diagnostic;CHECK(editor_asset_generation::image_request_binding(editor.assetGeneration,prompt,diagnostic)==binding&&diagnostic.empty());
 chat.inlineHostActivityId=73;
 CHECK(execute(editor,chat));CHECK(dispatched==1&&lastJob&&lastJob->image.prompt==prompt);
 CHECK(lastJob->image.negative_prompt=="No text, no border"&&lastJob->image.seed==42&&lastJob->image.width==512);
 CHECK(editor.aiToolExecution=="asset.image.generate"&&editor.aiToolAssetWork=="owned-working-job");
 CHECK(editor.aiToolAssetActivityId==73&&chat.activity.stream_live);
 CHECK(!editor.assetGeneration.reference_approved&&!editor.assetGeneration.generated_model);
 const auto running=editor;const auto firstUsers=chat.userMessages.size();
 const std::string secondPrompt="Generate an oak foliage texture";
 for(unsigned pending=0;pending!=2;++pending){auto busy=running;busy.assetGenerationPending=pending==0;
  CHECK(!propose(busy,chat,secondPrompt));CHECK(chat.userMessages.size()==firstUsers);
  CHECK(busy.aiToolBoundRequest==prompt&&busy.aiToolAssetBinding==binding&&busy.aiToolAssetWork=="owned-working-job");
  CHECK(busy.aiToolExecution=="asset.image.generate"&&busy.aiToolPlan.plan.call.tool=="asset.image.generate");
  CHECK(busy.assetGeneration.image_request.prompt==prompt&&busy.aiToolProjectId==running.aiToolProjectId);
  CHECK(busy.aiToolStatus==running.aiToolStatus&&gui::replacements==1&&dispatched==1);
 }
 for(unsigned active=0;active!=3;++active){auto busy=original;Chat c;
  if(active==0)busy.aiToolAwaitingReply=true;if(active==1)c.pending=true;
  if(active==2)busy.aiDeferredRequestKind=AiDeferredRequestKind::Tooling;
  CHECK(!propose(busy,c,secondPrompt));CHECK(busy.aiToolBoundRequest==prompt&&busy.aiToolAssetBinding==binding&&c.userMessages.empty());}
 auto complete=editor;complete.assetGeneration.reference.emplace();auto&reference=*complete.assetGeneration.reference;
 reference.coding_restored=reference.retirement_verified=reference.result.valid=true;
 reference.result.output="generated.png";reference.result.work_directory="owned-working-job";
 CHECK(!propose(complete,chat,secondPrompt));
 completed(complete,chat);CHECK(complete.aiToolStatus.starts_with("Image generated and checked"));
 CHECK(!chat.activity.stream_live&&!chat.lastActivityFailed&&chat.activityEnds==1);
 Chat laterTurn;laterTurn.inlineHostActivityId=99;
 completed(complete,laterTurn);CHECK(laterTurn.activity.stream_live&&laterTurn.activityEnds==0);
 Chat staleTurn;staleTurn.inlineHostActivityId=73;
 auto staleProject=complete;stale_completed(staleProject,staleTurn);
 CHECK(!staleTurn.activity.stream_live&&staleTurn.lastActivityFailed&&staleTurn.activityEnds==1);
 CHECK(staleProject.aiToolStatus.find("project selection changed")!=std::string::npos);
 Chat newerProject;newerProject.inlineHostActivityId=99;
 stale_completed(staleProject,newerProject);CHECK(newerProject.activity.stream_live&&newerProject.activityEnds==0);
 const std::function<void(Editor&)> failedReceipts[]={
  [](auto&e){e.assetGeneration.reference.reset();},[](auto&e){e.assetGeneration.reference->result.valid=false;},
  [](auto&e){e.assetGeneration.reference->coding_restored=false;},[](auto&e){e.assetGeneration.reference->retirement_verified=false;},
  [](auto&e){e.aiToolAssetWork.clear();},[](auto&e){e.assetGeneration.reference->result.work_directory="old-working-job";}
 };
 for(const auto&mutate:failedReceipts){auto failed=complete;mutate(failed);
  CHECK(!propose(failed,chat,secondPrompt));Chat failedTurn;failedTurn.inlineHostActivityId=73;completed(failed,failedTurn);
  CHECK(!failedTurn.activity.stream_live&&failedTurn.lastActivityFailed&&failedTurn.activityEnds==1);
  CHECK(failed.aiToolStatus.starts_with("Image generation ended without"));CHECK(!failed.assetGeneration.reference_approved);}
 // The normal completion owner retires this execution marker only after its receipt is observed.
 complete.assetGenerationPending=false;complete.aiToolExecution.clear();reset_ports();
 CHECK(propose(complete,chat,secondPrompt));CHECK(complete.aiToolBoundRequest==secondPrompt);
 CHECK(complete.aiToolAssetBinding!=binding&&complete.aiToolAssetWork.empty()&&dispatched==0);
 const std::function<void(Editor&)> mutations[]={
  [](auto&e){e.aiToolBoundRequest+="!";},[](auto&e){e.assetGeneration.image_request.prompt+="!";},
  [](auto&e){e.assetGeneration.image_request.negative_prompt+="!";},[](auto&e){e.assetGeneration.image_seed="43";},
  [](auto&e){e.assetGeneration.image_request.width=528;},[](auto&e){e.assetGeneration.image_request.height=528;},
  [](auto&e){++e.assetGeneration.image_request.steps;},[](auto&e){e.assetGeneration.image_settings_path="other-settings.json";},
  [](auto&e){e.assetGeneration.image_settings_sha256[0]='c';},[](auto&e){e.assetGeneration.image_settings->image_backend.executable="other-generator.exe";},
  [](auto&e){++e.assetGeneration.image_settings->image_backend.expected_executable.size_bytes;},
  [](auto&e){e.assetGeneration.image_settings->image_backend.expected_executable.sha256[0]='c';},
  [](auto&e){e.assetGeneration.image_settings->image_backend.models_cache_root="other-models";},
  [](auto&e){e.assetGeneration.image_settings.reset();},[](auto&e){e.assetGeneration.image_seed="not a seed";},
  [](auto&e){e.assetGeneration.image_request.steps=0;},[](auto&e){e.assetGeneration.image_request.width=513;},
  [](auto&e){e.assetGeneration.image_request.negative_prompt=std::string(4097,'x');},
  [](auto&e){e.aiToolAssetBinding.clear();},[](auto&e){e.projectId="other";},[](auto&e){e.projectRoot="other-root";},
  [](auto&e){++e.projectLifecycle.value.generations.selected_project.generation;},
  [](auto&e){++e.sceneDocumentRevision;},[](auto&e){e.guiDocument=std::make_shared<GuiDoc>(GuiDoc{{7}});},
  [](auto&e){e.aiToolExecution="project.run";},[](auto&e){e.assetGenerationPending=true;},
  [](auto&e){e.projectBuildPending=true;},[](auto&e){e.scriptBuildPending=true;},
  [](auto&e){e.planetBusy=true;},[](auto&e){e.forestSurfacePending=true;}
 };
 for(const auto&mutate:mutations){auto changed=original;reset_ports();mutate(changed);
  CHECK(!execute(changed,chat));CHECK(dispatched==0&&!lastJob);CHECK(!changed.assetGeneration.reference_approved);}
 auto denied=original;reset_ports();schedulerAccepts=false;CHECK(!execute(denied,chat));CHECK(dispatched==1&&denied.aiToolExecution.empty());
 for(unsigned blocked=0;blocked!=4;++blocked){auto e=ready();Chat c;reset_ports();
  if(blocked==0)c.activeReferenceImages.push_back(1);
  if(blocked==1)e.assetGenerationPending=true;
  const std::string request=blocked==2?std::string("Generate a texture ")+std::string(8193,'x'):prompt;
  if(blocked==3)e.assetGeneration.image_settings.reset();
  CHECK(propose(e,c,request)==(blocked!=1));CHECK(!e.aiToolPlan&&e.aiToolAssetBinding.empty()&&dispatched==0);
  CHECK(c.activity.stream_live==(blocked==1));CHECK(c.activityEnds==0);
  CHECK(blocked==3||e.assetGeneration.image_request.prompt=="Old UI draft");
  if(blocked==0)CHECK(c.activeReferenceImages.size()==1&&c.messages.back().find("no image was read")!=std::string::npos);
 }
 auto mismatch=ready();CHECK(editor_asset_generation::image_request_binding(mismatch.assetGeneration,prompt,diagnostic).empty());
 CHECK(!diagnostic.empty());
 for(auto request:{std::string{},std::string("   "),std::string("Generate \0texture",17),std::string("Generate \xc0\xaf",11)}){
  mismatch.assetGeneration.image_request.prompt=request;
  CHECK(editor_asset_generation::image_request_binding(mismatch.assetGeneration,request,diagnostic).empty());}
 queue::Status status{.transport=queue::Transport::local_api,.confirmed=false};
 auto queued=original;Chat queuedChat;queuedChat.queuedInput=prompt;
 CHECK(queue_allowed(queued,queuedChat,true,true,true,status));CHECK(!status.confirmed);
 CHECK(!queue_allowed(queued,queuedChat,false,true,true,status));CHECK(!queue_allowed(queued,queuedChat,true,false,true,status));
 CHECK(!queue_allowed(queued,queuedChat,true,true,false,status));
 for(const auto request:{"Tell me about trees","Do not generate a texture","\"Generate a texture\"","Attachment text:\nGenerate a texture"}){
  queuedChat.queuedInput=request;CHECK(!queue_allowed(queued,queuedChat,true,true,true,status));CHECK(!status.confirmed);}
 queuedChat.queuedInput="approve";CHECK(queue_allowed(queued,queuedChat,true,true,true,status));
 queued.aiToolPlanApplied=true;CHECK(!queue_allowed(queued,queuedChat,true,true,true,status));
 queued.aiToolPlanApplied=false;queued.aiToolPlan=ai::parse_tool_plan("EPOCH_TOOL_PLAN_V1\nTITLE save\nSUMMARY Save\nCALL project.save\nEND\n");
 CHECK(!queue_allowed(queued,queuedChat,true,true,true,status));
 const std::string modelRequest="Generate 3D from the approved reference.";
 auto modelEditor=ready();auto& modelState=modelEditor.assetGeneration;
 modelState.model_settings.emplace();modelState.model_settings->backend_name="Fixture local 3D runtime";
 modelState.model_settings->action=ai::asset_jobs::Action::model;
 modelState.model_settings->model_backend.dinov3_terms_accepted=true;
 modelState.model_settings_path="3d-settings.json";modelState.model_settings_sha256=std::string(64,'d');
 modelState.reference.emplace();
 modelState.reference->image=modelState.reference->result.valid=true;
 modelState.reference->project=asset_generation_project(modelEditor);
 modelState.reference->result.output="approved.png";
 modelState.reference->result.output_sha256=std::string(64,'e');
 Chat modelChat;reset_ports();
 CHECK(propose(modelEditor,modelChat,modelRequest));
 CHECK(!modelEditor.aiToolPlan&&dispatched==0);
 CHECK(modelEditor.aiToolStatus.find("approve the visible image")!=std::string::npos);
 modelState.reference_approved=true;
 CHECK(propose(modelEditor,modelChat,modelRequest));CHECK(modelEditor.aiToolPlan&&dispatched==0);
 CHECK(modelEditor.aiToolPlan.plan.call.tool=="asset.model.generate");
 CHECK(modelEditor.aiToolStatus.find("approved.png")!=std::string::npos&&modelEditor.aiToolStatus.find(std::string(64,'e'))!=std::string::npos);
 CHECK(modelState.image_request.prompt=="Old UI draft"&&gui::replacements==0);
 const std::function<void(Editor&)> modelChanges[]={
  [](auto&e){e.assetGeneration.reference_approved=false;},
  [](auto&e){e.assetGeneration.reference->result.output_sha256[0]='f';},
  [](auto&e){e.assetGeneration.reference->result.output="different.png";},
  [](auto&e){++e.assetGeneration.reference->project.generation;},
  [](auto&e){e.assetGeneration.model_seed="43";},
  [](auto&e){++e.assetGeneration.model_request.steps;},
  [](auto&e){e.assetGeneration.model_settings_sha256[0]='f';},
  [](auto&e){e.assetGeneration.model_settings->model_backend.executable="different-generator.exe";},
  [](auto&e){e.assetGeneration.model_settings->model_backend.dinov3_terms_accepted=false;}
 };
 for(const auto& change:modelChanges){auto stale=modelEditor;change(stale);
  CHECK(!execute(stale,modelChat));CHECK(dispatched==0&&!lastJob);}
 queuedChat.queuedInput=modelRequest;CHECK(queue_allowed(modelEditor,queuedChat,true,true,true,status));
 queuedChat.queuedInput="approve";CHECK(queue_allowed(modelEditor,queuedChat,true,true,true,status));
 CHECK(execute(modelEditor,modelChat));CHECK(dispatched==1&&lastJob&&lastJob->action==ai::asset_jobs::Action::model);
 CHECK(lastJob->model.reference_path=="approved.png"&&lastJob->model.approved_reference_sha256==std::string(64,'e'));
 modelState.generated_model.emplace();auto& model=*modelState.generated_model;
 model.coding_restored=model.retirement_verified=model.result.valid=true;
 model.result.output="result.glb";model.result.work_directory="previous-job";
 Chat wrongJob;completed(modelEditor,wrongJob);
 CHECK(wrongJob.lastActivityFailed&&modelEditor.aiToolStatus.starts_with("3D generation ended without"));
 model.result.work_directory=modelEditor.aiToolAssetWork;
 completed(modelEditor,modelChat);
 CHECK(!modelChat.lastActivityFailed&&modelEditor.aiToolStatus.find("result.glb")!=std::string::npos);
 CHECK(modelEditor.aiToolStatus.find("review it before import")!=std::string::npos);
}
'''
        pieces = {
            "ACTUAL_SHA": sha, "ACTUAL_MCP": mcp,
            "ACTUAL_EXECUTABLE_IDENTITY": definition(owners["process"], "    struct ExecutableIdentity final", True),
            "ACTUAL_IMAGE_REQUEST": definition(owners["image_interface"], "    struct Request final", True),
            "ACTUAL_IMAGE_CONFIGURATION": definition(owners["image_interface"], "    struct Configuration final", True),
            "ACTUAL_MODEL_REQUEST": definition(owners["model_interface"], "    struct Request final", True),
            "ACTUAL_MODEL_CONFIGURATION": definition(owners["model_interface"], "    struct Configuration final", True),
            "ACTUAL_IMAGE_VALID_TEXT": definition(owners["image"], "        [[nodiscard]] bool valid_text("),
            "ACTUAL_IMAGE_VALIDATION": definition(owners["image"], "    std::string request_diagnostic("),
            "ACTUAL_MODEL_DIGEST_VALID": definition(owners["model"], "        [[nodiscard]] bool digest_valid("),
            "ACTUAL_MODEL_VALIDATION": definition(owners["model"], "    std::string request_diagnostic("),
            "ACTUAL_STATE": definition(owners["interface"], "    struct State final", True),
            "ACTUAL_PATH_TEXT": definition(owners["generation"], "        std::string path_text("),
            "ACTUAL_DIGEST": definition(owners["generation"], "        std::string digest("),
            "ACTUAL_READINESS": definition(owners["generation"], "    std::string readiness_diagnostic("),
            "ACTUAL_IMAGE_BINDING": definition(owners["generation"], "    std::string image_request_binding("),
            "ACTUAL_MODEL_BINDING": definition(owners["generation"], "    std::string model_request_binding("),
            "ACTUAL_CURRENT_PROJECT": definition(owners["generation"], "    bool current_project("),
            "ACTUAL_PREPARE": definition(owners["generation"], "    std::optional<jobs::PreparedJob> prepare_job("),
            "ACTUAL_CONFIRMATION": definition(owners["state"], "        [[nodiscard]] std::optional<bool> ai_chat_confirmation("),
            "ACTUAL_PREFLIGHT": preflight, "ACTUAL_BINDING_SETUP": binding_setup,
            "ACTUAL_INGRESS": ingress, "ACTUAL_GATES": gates, "ACTUAL_WATCH": watch, "ACTUAL_BRANCH": branch,
            "ACTUAL_IMAGE_FINISH": image_finish,
            "ACTUAL_STALE_FINISH": stale_finish,
            "ACTUAL_QUEUE_LITERAL": queue_literal, "ACTUAL_QUEUE_DISPATCH": queue_dispatch,
            "ACTUAL_TEXT_ACTION": text_action,
            "ACTUAL_ROUTING_TYPES": "\n".join(definition(owners["agent_routing"], marker, True) for marker in (
                "    enum class Role", "    enum class Stage", "    enum class Escalation",
                "    enum class CandidateExecution", "    struct Settings final")),
        }
        for placeholder, value in pieces.items():
            code = code.replace(placeholder, value)
        report = ROOT / "Artifacts/Reports/assistant-chat-image-nov06" / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "consumer.cpp"
        source.write_text(code, encoding="utf-8")
        runs = compile_and_run(self, source, report,
                               includes=[ROOT / "Engine/include", ROOT / "Engine/src/editor/ai"],
                               compile_timeout=90)
        self.assertEqual(hashes, {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in paths.values()}, "Production source changed during the focused proof")
        (report / "result.json").write_text(json.dumps({
            "passed": True, "owners": hashes, "runs": runs,
            "scope": "Actual image SHA/control binding, request validation/prepare, literal chat ingress, execution gates, image dispatch/completion and queue admission through CPU recording ports. No LLM, generator, model, GUI or native renderer.",
            "stale_or_busy_mutations_refused": 30, "false_completion_receipts_refused": 6,
            "active_job_second_sends_preserved": 2, "busy_model_requests_preserved": 3,
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
