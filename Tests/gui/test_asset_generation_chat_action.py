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
            "assets": ROOT / "Engine/src/editor/application/editor.application_asset_browsing.inl",
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
            "goal": ROOT / "Engine/src/editor/ai/editor.ai_chat_goal.hpp",
            "scene_preview": ROOT / "Engine/src/editor/application/editor.application_scene_preview.inl",
        }
        owners = {key: path.read_text(encoding="utf-8") for key, path in paths.items()}
        hashes = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in paths.values()}
        ingress = definition(owners["planning"], "            if (ai::asset_generation_tool(generationTool))")
        tool_owner = definition(owners["planning"], "        auto request_ai_tool_plan =")
        preflight = tool_owner[tool_owner.index("{")+1:tool_owner.index("            chat.statusRequestKind =")]
        beacon_ingress = definition(tool_owner, '            if (forestAssetRequest)')
        beacon_approval = definition(owners["planning"], '        const auto has_forest_asset_approval =', True)
        beacon_route_start = owners["conversation"].index('            if (epochengine::editor::ai_chat_goal::forest_asset_request(input)')
        beacon_approval_start = owners["conversation"].index('            if (has_forest_asset_approval(input)', beacon_route_start)
        beacon_route_end = owners["conversation"].index('\n', beacon_approval_start)
        beacon_route = owners["conversation"][beacon_route_start:beacon_route_end]
        binding_start = tool_owner.index('            chat.append_user_message("Tool plan: "')
        binding_setup = tool_owner[binding_start:tool_owner.index('            if (ai::scene_placement_tool(generationTool))')]
        objective_ingress = definition(tool_owner, '            if (ai::scene_placement_tool(generationTool))')
        objective_dispatch = definition(owners["runtime"], '            else if (ai::scene_placement_tool(call.tool))')
        review_ingress = definition(tool_owner, '            if (ai::asset_review_tool(generationTool))')
        review_dispatch = definition(owners["runtime"], '            else if (ai::asset_review_tool(call.tool))')
        import_ingress = definition(tool_owner, '            if (generationTool == "forest.material.import")')
        import_dispatch = definition(owners["runtime"], '                if (call.tool == "forest.material.import")')
        update_ingress = definition(tool_owner, '            if (generationTool == "forest.update")')
        update_dispatch = definition(owners["runtime"], '            else if (call.tool == "forest.update")')
        species_ingress = definition(tool_owner, '            if (generationTool == "forest.species.add")')
        species_dispatch = definition(owners["runtime"], '            else if (call.tool == "forest.species.add")')
        self.assertLess(tool_owner.index("editor.assetGenerationPending"), binding_start)
        self.assertLess(tool_owner.index('ai::asset_generation_tool(editor.aiToolExecution)'), binding_start)
        text_action = definition(owners["planning"], "        const auto queue_text_message_action =", True)
        branch = definition(owners["runtime"], '            else if(ai::asset_generation_tool(call.tool))')
        gate_start = owners["runtime"].index("            if (const auto expected = ai::project_lifecycle_request_tool(editor.aiToolBoundRequest);")
        gate_end = owners["runtime"].index("            if(const auto selected=validate_ai_project_tool_selection", gate_start)
        gates = owners["runtime"][gate_start:gate_end]
        goal_gates = owners["runtime"][owners["runtime"].index("            if (editor.aiGoalActive && (editor.aiGoalTools.round_trip"):gate_start]
        finish_start = owners["runtime"].index("            editor.aiToolPlanApplied = true;")
        finish = owners["runtime"][finish_start:owners["runtime"].index('            editor.aiToolStatus += " Only this approved', finish_start)]
        watch = definition(owners["runtime"], "            auto watch_execution =", True)
        image_finish = definition(owners["runtime"], '                else if(ai::asset_generation_tool(editor.aiToolExecution))')
        execution_owner = definition(owners["runtime"], '        if (!editor.aiToolExecution.empty())')
        stale_finish = definition(execution_owner, '                if (!current)')
        queue_start = owners["conversation"].index("        const bool queuedProjectHostAction =")
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
#include <future>
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
#include "core.format_text.hpp"
#include "editor.ai_chat_composer_queue.hpp"
#include "editor.ai_chat_goal.hpp"
ACTUAL_SHA
ACTUAL_MCP
namespace ai=epochengine::ai;
namespace queue=epochengine::editor::ai_chat_queue;
using namespace std::chrono_literals;
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
struct Completion{bool image{},model{},coding_restored{},retirement_verified{};struct{
 std::filesystem::path output,work_directory;std::string output_sha256;bool valid{};
 explicit operator bool()const{return valid;}}result;ProjectIdentity project;std::string reference_sha256;};
ACTUAL_STATE
ACTUAL_PATH_TEXT
ACTUAL_DIGEST
ACTUAL_READINESS
ACTUAL_IMAGE_BINDING
ACTUAL_CURRENT_PROJECT
ACTUAL_MODEL_BINDING
ACTUAL_PREPARE
bool referenceBytesCurrent=true;
bool reference_current(const State&,std::string& error){
 if(referenceBytesCurrent)return true;
 error="The reference changed. Choose and review it again.";return false;}
ACTUAL_REFERENCE_APPROVAL
}
enum class EditorMainSurface{Scene,Project,AssetGeneration,ForestFactory};
enum class AiDeferredRequestKind{None,Chat,Tooling};
namespace editor_ai_agent_routing{ACTUAL_ROUTING_TYPES}
struct CodexSession{bool pending{};std::vector<bool>decisions;
 bool approval_pending()const{return pending;}void answer_approval(bool value){decisions.push_back(value);pending=false;}};
struct GuiDoc{struct Revision{std::uint64_t sequence{};}value;const Revision& revision()const{return value;}};
inline constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
struct ForestRegion{std::string_view plant_source;double radius_meters{20};};
struct EditorEntity{std::string_view category,type;std::optional<ForestRegion> forestRegion;
 std::uint64_t sceneObjectId{};std::string_view name;bool terrainAttachment{};};
namespace scene {
inline constexpr std::uint64_t kInvalidSceneObjectId=0u;
struct SceneReachObjective{std::uint64_t target{};double radius_meters{};};
}
namespace project_planetary {
enum class SurfaceCode{ready,invalid};
struct ShorelineResult{SurfaceCode code{};struct{std::string message;}operation;bool land{true};std::optional<bool>playerStart;};
}
struct Editor {
 editor_asset_generation::State assetGeneration;
 bool assetGenerationPending{},projectBuildPending{},scriptBuildPending{},forestSurfacePending{},planetBusy{},planetaryGeneratePending{};
 std::optional<std::shared_future<project_planetary::ShorelineResult>> modelShorelinePending;
 int modelShorelineCancellation{};
 bool aiToolPlanApplied{},aiToolAwaitingReply{};
 bool aiGoalActive{},aiGoalRunning{},aiGoalPlanNextQueued{};
 unsigned aiGoalCompletedMilestones{};
 std::string aiGoal,aiGoalDraft,aiGoalLastMilestoneReceipt;
 epochengine::editor::ai_chat_goal::ToolSequence aiGoalTools;
 std::uint64_t aiToolPlantRevision{};
 struct PlantDocument{std::uint64_t revision{1},contentHash{10};} plantLabDocument;
 struct PlantArtifact{std::uint64_t source_revision{},source_content_hash{};
  explicit operator bool()const{return source_revision!=0;}} plantLabPublishedArtifact;
 bool forestTerrainPainting{},aiGoalForestReceipt{};
 std::vector<EditorEntity> entities;
 std::uint64_t selectedEntityId{},aiToolPlacementTarget{},spawnSource{};
 bool walkingSolid{};
 struct SceneDocument{bool ready{true};bool initialized()const{return ready;}}sceneDocument;
 std::optional<scene::SceneReachObjective> reachObjective;
 struct RuntimeProcess{bool running{};bool valid()const{return running;}}projectRuntimeProcess;
 struct RuntimeSnapshot{bool active()const{return true;}};
 std::optional<RuntimeSnapshot> projectRuntimeSnapshot;
 std::string plantLabStatus,projectStatus,meshImportPathDraft;
 struct ModelReview{std::filesystem::path source;std::string sourceSha256,requestedPath;int compiled{};
  struct Shoreline{struct{struct{unsigned id{};std::string name{"Verdant"};}body;}query;int transform{};std::uint64_t sceneRevision{};unsigned terrain{1};
   std::optional<ForestRegion>forest;std::uint64_t forestTree{};};
  std::optional<Shoreline> shoreline;};
 std::optional<ModelReview> modelImportReview;
 AiDeferredRequestKind aiDeferredRequestKind{AiDeferredRequestKind::None};
 int aiMethodSelection{};std::shared_ptr<CodexSession>aiCodexSession;
 editor_ai_agent_routing::Settings aiAgentRouting;
 bool aiAuthoringPlanApplied{},aiAuthoringApplyPending{};
 bool aiPackageRequest{};
 std::string aiProjectScriptDraft;ai::ToolPlanParseResult aiAuthoringPlan;
 std::optional<int>aiToolTestPending;unsigned aiQueuedMessageActionIndex{99};
 std::string aiToolStatus,aiToolAssetBinding,aiToolBoundRequest,aiToolExecution,aiToolRequest;
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
 struct Lifecycle{struct Evidence{bool scene_committed{};struct Generations{
 struct{std::uint64_t generation{4};}selected_project;
 struct{std::uint64_t build_generation{};}active_build,output_artifact;
 std::uint64_t committed_scene_generation{};}generations;}value;
 const Evidence& evidence()const{return value;}}projectLifecycle;
};
using EditorState=Editor;
editor_asset_generation::ProjectIdentity asset_generation_project(const Editor& editor){
 return {editor.projectRoot,editor.projectId,editor.projectLifecycle.evidence().generations.selected_project.generation};}
std::string display_project_path(const std::filesystem::path&path){return path.generic_string();}
bool has_open_project(const Editor& editor){return !editor.projectId.empty()&&!editor.projectRoot.empty();}
bool capture_ai_forest_recipe_binding(Editor& editor,std::string_view){editor.aiToolPlantRevision=editor.plantLabDocument.revision;return true;}
bool ai_forest_recipe_binding_current(const Editor& editor){return editor.aiToolPlantRevision==editor.plantLabDocument.revision;}
bool gui_document_has_unsaved_changes(const Editor&){return false;}
const EditorEntity* selected_editor_reach_model(const Editor& editor){
 const auto selected=std::find_if(editor.entities.begin(),editor.entities.end(),[&](const auto& entity){
  return entity.sceneObjectId==editor.selectedEntityId&&entity.type=="IndexedMesh";});
 return selected==editor.entities.end()?nullptr:&*selected;}
const EditorEntity* selected_terrain_attachment_refresh_target(const Editor& editor){
 const auto selected=std::ranges::find(editor.entities,editor.selectedEntityId,&EditorEntity::sceneObjectId);
 return selected!=editor.entities.end()&&(selected->terrainAttachment||selected->category=="Planetary Terrain")?&*selected:nullptr;}
bool begin_forest_surface_refresh(Editor& editor,bool attachments){
 if(!attachments||editor.forestSurfacePending)return false;
 editor.forestSurfacePending=true;editor.projectStatus="Sampling current ground";return true;}
const EditorEntity* selected_forest_species_target(const Editor& editor){
 const auto selected=std::ranges::find(editor.entities,editor.selectedEntityId,&EditorEntity::sceneObjectId);
 return selected!=editor.entities.end()&&selected->forestRegion?&*selected:nullptr;}
unsigned speciesPlacements{};
bool add_plant_lab_species_to_forest(Editor& editor){
 if(editor.forestSurfacePending)return false;
 ++speciesPlacements;editor.forestSurfacePending=true;editor.plantLabStatus="Sampling selected forest";return true;}
bool set_editor_reach_objective(Editor& editor,const std::optional<scene::SceneReachObjective>& objective){
 editor.reachObjective=objective;++editor.sceneDocumentRevision;return true;}
bool set_editor_surface_spawn(Editor& editor,const EditorEntity& model){
 if(!model.terrainAttachment)return false;editor.spawnSource=model.sceneObjectId;++editor.sceneDocumentRevision;return true;}
bool set_editor_walk_box(Editor& editor,const EditorEntity& model,bool solid){
 if(!model.terrainAttachment)return false;editor.walkingSolid=solid;++editor.sceneDocumentRevision;return true;}
ACTUAL_PAINTED_TREE_OWNER
ACTUAL_TREE_UPDATE_RESULT
unsigned paintedUpdates{};
bool savePaintedTrees{};
PaintedTreeUpdateResult refresh_forest_factory_instances(Editor& editor){
 ++paintedUpdates;
 editor.plantLabStatus=savePaintedTrees?"Painted trees saved":"Painted trees updated; saving failed. Retry Save before closing.";
 return {.applied=true,.saved=savePaintedTrees};}
bool model_import_review_current(const Editor& editor){
 return editor.modelImportReview&&editor.modelImportReview->requestedPath==editor.meshImportPathDraft;}
bool modelBytesCurrent=true;
unsigned modelReviews{},modelImports{};
bool review_project_model(Editor& editor,const std::filesystem::path& source){
 ++modelReviews;
 editor.modelImportReview=Editor::ModelReview{source,modelBytesCurrent
     ?editor.assetGeneration.generated_model->result.output_sha256:std::string(64,'0'),editor.meshImportPathDraft};
 return true;}
struct ModelSurfacePose{std::array<double,3> pivot{1.25,2,3};bool playerStart{};};
unsigned landingImports{};
bool approve_project_model_import(Editor& editor,const ModelSurfacePose* pose = nullptr){
 ++modelImports;editor.modelImportReview.reset();++editor.sceneDocumentRevision;
 if(pose&&pose->playerStart)++landingImports;
 editor.projectStatus="Reviewed model imported";return true;}
ACTUAL_REVIEW_BINDINGS
unsigned shorelineSearches{};
std::optional<std::promise<project_planetary::ShorelineResult>> shorelineResponse;
bool prepare_model_shoreline_review(Editor& editor,bool landing){
 auto& target=editor.modelImportReview->shoreline.emplace();target.sceneRevision=editor.sceneDocumentRevision;
 if(!landing)return true;
 const auto tree=std::ranges::find(editor.entities,editor.selectedEntityId,&EditorEntity::sceneObjectId);
 if(tree==editor.entities.end()||!tree->forestRegion){editor.projectStatus="Select one painted tree";return false;}
 target.forest=tree->forestRegion;target.forestTree=tree->sceneObjectId;return true;}
bool model_shoreline_review_current(const Editor& editor){
 return generated_model_review_current(editor)&&editor.modelImportReview->shoreline
   &&editor.modelImportReview->shoreline->sceneRevision==editor.sceneDocumentRevision&&!model_import_scene_busy(editor)
   &&(!editor.modelImportReview->shoreline->forest||editor.modelImportReview->shoreline->forestTree==editor.selectedEntityId);}
bool begin_model_shoreline_import(Editor& editor){
 if(!model_shoreline_review_current(editor))return false;
 ++shorelineSearches;
 editor.modelShorelinePending.emplace(shorelineResponse.emplace().get_future().share());
 editor.projectStatus="Searching for dry ground";return true;}
std::optional<ModelSurfacePose> model_surface_pose(int,int,const auto&,bool land,unsigned,const bool* start){
 return land?std::optional{ModelSurfacePose{.playerStart=start&&*start}}:std::nullopt;}
namespace forest {
enum class ForestTextureSlot{bark,foliage};
struct Reference{std::string path;std::array<std::uint64_t,4> key;};
}
struct CatalogEntry{std::string logical_path{"Assets/Images/reviewed.png"};struct{std::array<std::uint64_t,4> words{1,2,3,4};}artifact_key;};
unsigned imports{},applications{};
bool acceptImport=true,acceptRecipe=true;
forest::ForestTextureSlot appliedSlot{};
forest::Reference appliedTexture;
std::optional<CatalogEntry> import_asset_generation_texture(Editor& editor,int){
 ++imports;if(acceptImport)return CatalogEntry{};editor.assetGeneration.status="Import refused";return {};}
bool change_plant_lab_texture(Editor& editor,forest::ForestTextureSlot slot,const forest::Reference& texture){
 ++applications;if(!acceptRecipe){editor.plantLabStatus="Recipe refused";return false;}
 ++editor.plantLabDocument.revision;++editor.plantLabDocument.contentHash;
 editor.plantLabPublishedArtifact={editor.plantLabDocument.revision,editor.plantLabDocument.contentHash};
 appliedSlot=slot;appliedTexture=texture;editor.plantLabStatus="Recipe saved";return true;}
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
using AiChat=Chat;
void push_editor_log(Editor&,std::string_view){}
ACTUAL_SHORELINE_COMPLETION
namespace gui {std::string oldPrompt;unsigned replacements{};
 bool replace_text_in_edit_box(std::string& value,std::string_view next,std::size_t maximum){
 if(next.size()>maximum)return false;oldPrompt=value;value=next;++replacements;return true;}}
ai::ToolPlanParseResult admit_ai_project_tool_reply(std::string_view,std::string_view reply){return ai::parse_tool_plan(reply);}
ACTUAL_CONFIRMATION
bool planetary_project_busy(const Editor& editor){return editor.planetBusy;}
unsigned dispatched{};bool schedulerAccepts=true,cutoutRequested=false;std::optional<ai::asset_jobs::PreparedJob>lastJob;
void dispatch_asset_generation(Editor& editor,int,editor_asset_generation::Action action,bool foliageCutout=false){
 ++dispatched;
 cutoutRequested=foliageCutout;
 const auto kind=action==editor_asset_generation::Action::generate_image?ai::asset_jobs::Action::image:ai::asset_jobs::Action::model;
 lastJob=editor_asset_generation::prepare_job(editor.assetGeneration,kind,"owned-working-job",editor.assetGeneration.status);
 if(lastJob&&schedulerAccepts){editor.assetGenerationPending=true;editor.assetGeneration.active_work_directory=lastJob->work_directory;}}
bool propose(Editor& editor,Chat& chat,std::string_view request){
 ACTUAL_PREFLIGHT
 const auto forestAssetRequest=epochengine::editor::ai_chat_goal::forest_asset_request(request);
 ACTUAL_BEACON_INGRESS
 ACTUAL_BINDING_SETUP
 ACTUAL_OBJECTIVE_INGRESS
 ACTUAL_REVIEW_INGRESS
 ACTUAL_IMPORT_INGRESS
 ACTUAL_UPDATE_INGRESS
 ACTUAL_SPECIES_INGRESS
 ACTUAL_INGRESS return false;
}
bool execute(Editor& editor,Chat& chat){
 ACTUAL_GOAL_COMPLETE
 const auto dispatch_ai_tool_plan_impl=[&]()->bool{
 if(!editor.aiToolPlan||editor.aiToolPlanApplied)return false;
 const auto call=editor.aiToolPlan.plan.call;int ctx{};bool synchronousSuccess=true;
 ACTUAL_GOAL_GATES
 ACTUAL_GATES
 ACTUAL_IMPORT_DISPATCH
 ACTUAL_WATCH
 if(false){}ACTUAL_BRANCH ACTUAL_UPDATE_DISPATCH ACTUAL_SPECIES_DISPATCH ACTUAL_REVIEW_DISPATCH ACTUAL_OBJECTIVE_DISPATCH
 ACTUAL_FINISH
 return true;};
 ACTUAL_DISPATCH_WRAPPER
 return dispatch_ai_tool_plan();
}
bool completed(Editor&editor,Chat&chat){bool assetSucceeded=false;if(false){}ACTUAL_IMAGE_FINISH return assetSucceeded;}
void stale_completed(Editor&editor,Chat&chat){const bool current=false;ACTUAL_STALE_FINISH}
namespace epochengine::ai{bool is_model_use_confirmed(){return false;}}
std::string_view trim_ai_command(std::string_view value){
 while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.front())))value.remove_prefix(1);
 while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.back())))value.remove_suffix(1);return value;}
bool submit(Editor&editor,Chat&chat,std::string_view input){
 const auto ai_confirmation=[&](std::string_view value){return ai_chat_confirmation(value);};
 ACTUAL_BEACON_APPROVAL
 #define EPOCH_EDITION_PUBLIC 1
 ACTUAL_TEXT_ACTION
 #undef EPOCH_EDITION_PUBLIC
 const auto request_ai_tool_plan=[&]{return propose(editor,chat,editor.aiToolRequest);};
 ACTUAL_BEACON_ROUTE
 if(queue_text_message_action(input))return true;
 if(!ai::local_project_request_tool(input).empty())return propose(editor,chat,input);
 return false;
}
bool queue_allowed(const Editor& editor,Chat& chat,bool queueOwnerReady,bool queuedMethodMatches,bool queuedModelMatches,const queue::Status& status){
 const bool queuedProviderReady=queue::ready_to_dispatch(status,std::chrono::steady_clock::now());
 const bool queuedIdleReloadConsentReady=false;
 ACTUAL_BEACON_APPROVAL
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
 {
  auto editor=ready();Chat chat;
  constexpr auto request="Set the selected model as the reach objective within 7.5 m.";
  constexpr auto clear="Clear the reach objective.";
  CHECK(propose(editor,chat,request)&&!editor.aiToolPlan&&!editor.reachObjective);
  editor.entities={{"World","IndexedMesh",{},17,"Beacon"},{"World","IndexedMesh",{},18,"Wrong target"}};
  editor.selectedEntityId=17;
  CHECK(propose(editor,chat,"Set the selected model as the reach objective within NaN m.")&&!editor.aiToolPlan);
  CHECK(propose(editor,chat,request)&&editor.aiToolPlan.plan.call.tool=="scene.objective.set_reach");
  CHECK(editor.aiToolStatus.find("Beacon within 7.5 m")!=std::string::npos&&!editor.reachObjective);
  auto changedSelection=editor;changedSelection.selectedEntityId=18;
  CHECK(!execute(changedSelection,chat)&&!changedSelection.reachObjective);
  auto runtimeBusy=editor;runtimeBusy.projectRuntimeProcess.running=true;
  CHECK(!execute(runtimeBusy,chat)&&!runtimeBusy.reachObjective);
  CHECK(execute(editor,chat)&&editor.reachObjective&&editor.reachObjective->target==17&&editor.reachObjective->radius_meters==7.5);
  CHECK(propose(editor,chat,clear)&&editor.aiToolPlan.plan.call.tool=="scene.objective.clear"&&editor.reachObjective);
  CHECK(execute(editor,chat)&&!editor.reachObjective);
  constexpr auto spawn="Set the player start at the selected shoreline model.";
  CHECK(propose(editor,chat,spawn)&&!editor.aiToolPlan&&!editor.spawnSource);
  editor.entities[0].terrainAttachment=true;
  CHECK(propose(editor,chat,spawn)&&editor.aiToolPlan.plan.call.tool=="scene.spawn.set_surface");
  CHECK(editor.aiToolStatus.find("PlayerStart to Beacon")!=std::string::npos&&!editor.spawnSource);
  auto staleTerrain=editor;++staleTerrain.sceneDocumentRevision;
  CHECK(!execute(staleTerrain,chat)&&!staleTerrain.spawnSource);
  CHECK(execute(editor,chat)&&editor.spawnSource==17);
  constexpr auto collision="Enable walking collision on the selected shoreline model.";
  CHECK(propose(editor,chat,collision)&&editor.aiToolPlan.plan.call.tool=="scene.collision.enable"&&!editor.walkingSolid);
  auto changed=editor;changed.selectedEntityId=18;
  CHECK(!execute(changed,chat)&&!changed.walkingSolid);
  CHECK(execute(editor,chat)&&editor.walkingSolid);
  CHECK(propose(editor,chat,"Disable walking collision on the selected shoreline model.")&&editor.aiToolPlan.plan.call.tool=="scene.collision.disable");
  CHECK(execute(editor,chat)&&!editor.walkingSolid);
  constexpr auto refresh="Refresh attached model and spawn placement.";
  CHECK(propose(editor,chat,refresh)&&editor.aiToolPlan.plan.call.tool=="scene.attachments.refresh");
  auto changedTarget=editor;changedTarget.selectedEntityId=18;
  CHECK(!execute(changedTarget,chat)&&!changedTarget.forestSurfacePending);
  CHECK(execute(editor,chat)&&editor.forestSurfacePending&&editor.aiToolExecution=="scene.attachments.refresh");
 }
 {
  auto editor=ready();Chat chat;
  constexpr auto request="Update Painted Trees.";
  CHECK(propose(editor,chat,request)&&!editor.aiToolPlan&&!paintedUpdates);
  editor.entities={{"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath}},
   {"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath}},
   {"ForestFactory","ForestTrunk",ForestRegion{"Assets/Forest/another.epoch_forest"}},
   {"ForestFactory","ForestTrunk",{}},{"Other","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath}}};
  CHECK(propose(editor,chat,request)&&editor.aiToolPlan&&editor.aiToolPlan.plan.call.tool=="forest.update");
  CHECK(editor.aiToolStatus.find("2 painted trees using Plant Lab recipe revision 1")!=std::string::npos);
  CHECK(editor.requestedMainSurface==EditorMainSurface::ForestFactory&&!chat.activity.stream_live&&!paintedUpdates);
  auto staleScene=editor;++staleScene.sceneDocumentRevision;
  CHECK(!execute(staleScene,chat)&&!paintedUpdates);
  auto staleRecipe=editor;++staleRecipe.plantLabDocument.revision;
  CHECK(!execute(staleRecipe,chat)&&!paintedUpdates);
  auto busy=editor;busy.projectRuntimeSnapshot.emplace();
  CHECK(!execute(busy,chat)&&!paintedUpdates);
  editor.forestTerrainPainting=true;
  CHECK(execute(editor,chat)&&paintedUpdates==1&&!editor.forestTerrainPainting);
  CHECK(editor.aiToolStatus==editor.plantLabStatus&&editor.aiToolStatus.find("saving failed")!=std::string::npos);
 }
 {
  auto editor=ready();Chat chat;
  constexpr auto request="Add the current Plant Lab tree to the selected forest.";
  CHECK(propose(editor,chat,request)&&!editor.aiToolPlan&&!speciesPlacements);
  editor.entities={{"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath}}};
  editor.entities.front().sceneObjectId=72;editor.selectedEntityId=72;
  CHECK(propose(editor,chat,request)&&editor.aiToolPlan.plan.call.tool=="forest.species.add");
  CHECK(!speciesPlacements&&!chat.activity.stream_live);
  auto changed=editor;changed.selectedEntityId=73;
  CHECK(!execute(changed,chat)&&!speciesPlacements);
  changed=editor;++changed.plantLabDocument.revision;
  CHECK(!execute(changed,chat)&&!speciesPlacements);
  CHECK(execute(editor,chat)&&speciesPlacements==1&&editor.aiToolExecution=="forest.species.add");
 }
 {
  constexpr std::string_view command="Import and apply the current PNG as foliage.";
  auto textureEditor=ready();Chat textureChat;
  auto& png=textureEditor.assetGeneration.reference.emplace();
  png.project=asset_generation_project(textureEditor);png.image=true;png.result.valid=true;
  png.result.output="generated.png";png.result.work_directory="reviewed-job";png.result.output_sha256=std::string(64,'a');
  CHECK(propose(textureEditor,textureChat,command));
  CHECK(textureEditor.aiToolPlan&&textureEditor.aiToolPlan.plan.call.tool=="forest.material.import");
  CHECK(!imports&&!applications&&!textureChat.activity.stream_live);
  CHECK(textureEditor.aiToolStatus.find(png.result.output_sha256)!=std::string::npos);
  auto changedJob=textureEditor;changedJob.assetGeneration.reference->result.work_directory="new-job";
  CHECK(!execute(changedJob,textureChat)&&!imports&&!applications);
  auto changedProject=textureEditor;++changedProject.projectLifecycle.value.generations.selected_project.generation;
  CHECK(!execute(changedProject,textureChat)&&!imports&&!applications);
  auto changedRecipe=textureEditor;++changedRecipe.plantLabDocument.revision;
  CHECK(!execute(changedRecipe,textureChat)&&!imports&&!applications);
  acceptImport=false;CHECK(!execute(textureEditor,textureChat)&&imports==1&&!applications);
  acceptImport=true;acceptRecipe=false;CHECK(!execute(textureEditor,textureChat)&&imports==2&&applications==1);
  CHECK(textureEditor.aiToolStatus.find("Assets/Images/reviewed.png")!=std::string::npos);
  acceptRecipe=true;CHECK(execute(textureEditor,textureChat)&&imports==3&&applications==2);
  CHECK(appliedSlot==forest::ForestTextureSlot::foliage&&appliedTexture.path=="Assets/Images/reviewed.png");
  CHECK(appliedTexture.key==CatalogEntry{}.artifact_key.words);
  constexpr auto combined="Import and apply the current PNG as bark, then update painted trees.";
  textureEditor.aiGoalActive=textureEditor.aiGoalRunning=true;
  textureEditor.aiGoal=combined;
  textureEditor.aiGoalTools.bind_request(combined,textureEditor.projectId,textureEditor.projectRoot,4,true);
  textureEditor.entities={{"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath}}};
  CHECK(propose(textureEditor,textureChat,combined)&&textureEditor.aiToolPlan.plan.call.tool=="forest.material.import");
  CHECK(execute(textureEditor,textureChat)&&imports==4&&applications==3&&paintedUpdates==1);
  CHECK(appliedSlot==forest::ForestTextureSlot::bark&&textureEditor.aiGoalPlanNextQueued);
  CHECK(textureEditor.aiGoalCompletedMilestones==1&&textureEditor.aiGoalTools.operation()=="forest.update");
  const auto next=textureEditor.aiGoalTools.next_request(combined);
  auto changedHash=textureEditor;++changedHash.plantLabDocument.contentHash;
  CHECK(propose(changedHash,textureChat,next)&&!changedHash.aiToolPlan&&!changedHash.aiGoalRunning);
  CHECK(propose(textureEditor,textureChat,next)&&textureEditor.aiToolPlan.plan.call.tool=="forest.update");
  CHECK(paintedUpdates==1&&imports==4);
  auto staleUpdate=textureEditor;++staleUpdate.sceneDocumentRevision;
  CHECK(!execute(staleUpdate,textureChat)&&!staleUpdate.aiToolPlan&&!staleUpdate.aiGoalRunning);
  auto failedSave=textureEditor;
  CHECK(execute(failedSave,textureChat)&&paintedUpdates==2&&failedSave.aiGoalTools.stopped);
  CHECK(failedSave.aiGoalCompletedMilestones==1&&!failedSave.aiGoalPlanNextQueued&&!failedSave.aiGoalRunning);
  savePaintedTrees=true;
  CHECK(execute(textureEditor,textureChat)&&paintedUpdates==3&&imports==4&&applications==3);
  CHECK(textureEditor.aiGoalCompletedMilestones==2&&!textureEditor.aiGoalRunning&&!textureEditor.aiGoalPlanNextQueued);
 }
 {
  auto editor=ready();Chat chat;
  constexpr auto approveReference="Approve the current PNG as the 3D reference.";
  constexpr auto importModel="Import the current generated 3D model.";
  CHECK(propose(editor,chat,approveReference)&&!editor.aiToolPlan);
  auto& reference=editor.assetGeneration.reference.emplace();
  reference.project=asset_generation_project(editor);reference.image=reference.result.valid=true;
  reference.result.output="beacon.png";reference.result.work_directory="image-job";
  reference.result.output_sha256=std::string(64,'b');
  chat.activeReferenceImages.push_back(1);
  CHECK(propose(editor,chat,approveReference)&&!editor.aiToolPlan&&!editor.assetGeneration.reference_approved);
  chat.activeReferenceImages.clear();
  CHECK(propose(editor,chat,approveReference)&&editor.aiToolPlan.plan.call.tool=="asset.reference.approve");
  CHECK(!editor.assetGeneration.reference_approved&&!chat.activity.stream_live);
  auto changedReference=editor;changedReference.assetGeneration.reference->result.work_directory="replacement-image-job";
  CHECK(!execute(changedReference,chat)&&!changedReference.assetGeneration.reference_approved);
  editor_asset_generation::referenceBytesCurrent=false;
  CHECK(!execute(editor,chat)&&!editor.assetGeneration.reference_approved);
  editor_asset_generation::referenceBytesCurrent=true;
  CHECK(execute(editor,chat)&&editor.assetGeneration.reference_approved&&!dispatched&&!modelImports);
  CHECK(propose(editor,chat,importModel)&&!editor.aiToolPlan&&!modelReviews);
  auto& model=editor.assetGeneration.generated_model.emplace();
  model.project=asset_generation_project(editor);model.model=model.result.valid=true;
  model.result.output="beacon.glb";model.result.work_directory="model-job";
  model.result.output_sha256=std::string(64,'a');
  modelBytesCurrent=false;
  CHECK(propose(editor,chat,importModel)&&!editor.aiToolPlan&&!editor.modelImportReview&&modelReviews==1);
  modelBytesCurrent=true;
  CHECK(propose(editor,chat,importModel)&&editor.aiToolPlan.plan.call.tool=="asset.model.import");
  CHECK(editor.modelImportReview&&modelReviews==2&&!modelImports);
  auto changedModel=editor;changedModel.assetGeneration.generated_model->result.output_sha256=std::string(64,'d');
  CHECK(!execute(changedModel,chat)&&!modelImports);
  auto changedReview=editor;changedReview.modelImportReview->source="another.glb";
  CHECK(!execute(changedReview,chat)&&!modelImports);
  auto reopened=editor;++reopened.projectLifecycle.value.generations.selected_project.generation;
  CHECK(!execute(reopened,chat)&&!modelImports);
  auto busy=editor;busy.projectRuntimeProcess.running=true;
  CHECK(!execute(busy,chat)&&!modelImports);
  const auto scene=editor.sceneDocumentRevision;
  CHECK(execute(editor,chat)&&modelImports==1&&editor.sceneDocumentRevision==scene+1);
  CHECK(!execute(editor,chat)&&modelImports==1&&!dispatched);
  constexpr auto shoreline="Import the current generated 3D model on the ocean shoreline.";
  CHECK(propose(editor,chat,shoreline)&&editor.aiToolPlan.plan.call.tool=="asset.model.import_shoreline");
  CHECK(editor.aiToolStatus.find("Verdant")!=std::string::npos&&shorelineSearches==0&&modelImports==1);
  CHECK(editor.aiToolStatus.find("Approval searches for dry terrain")!=std::string::npos);
  auto changedTerrain=editor;++changedTerrain.sceneDocumentRevision;
  CHECK(!execute(changedTerrain,chat)&&shorelineSearches==0&&modelImports==1);
  CHECK(execute(editor,chat)&&editor.modelShorelinePending&&shorelineSearches==1&&modelImports==1);
  CHECK(!propose(editor,chat,importModel)&&shorelineSearches==1&&modelImports==1);
  CHECK(chat.messages.back().find("press Escape")!=std::string::npos);
  CHECK(!poll_model_shoreline_import(editor,chat)&&editor.modelShorelinePending&&modelImports==1);
  auto cancelled=editor;cancelled.modelImportReview->shoreline.reset();
  auto stale=editor;++stale.sceneDocumentRevision;
  shorelineResponse->set_value({});
  CHECK(!poll_model_shoreline_import(cancelled,chat)&&!cancelled.modelShorelinePending&&modelImports==1);
  CHECK(!poll_model_shoreline_import(stale,chat)&&!stale.modelShorelinePending&&modelImports==1);
  CHECK(stale.aiToolStatus.find("changed during the shoreline search")!=std::string::npos);
  CHECK(poll_model_shoreline_import(editor,chat)&&!editor.modelShorelinePending&&modelImports==2);
  CHECK(editor.aiToolStatus.find("Body 0 at (1.25, 2, 3) m")!=std::string::npos);
  CHECK(!poll_model_shoreline_import(editor,chat)&&modelImports==2);
  constexpr auto landing="Set up landing near the selected forest with the current generated 3D model.";
  CHECK(propose(editor,chat,landing)&&!editor.aiToolPlan&&!landingImports);
  editor.entities={{"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath},22,"Landing forest"}};
  editor.selectedEntityId=22;
  CHECK(propose(editor,chat,landing)&&editor.aiToolPlan.plan.call.tool=="asset.model.setup_landing");
  CHECK(editor.aiToolStatus.find("Verdant (20 m radius)")!=std::string::npos&&editor.aiToolStatus.find("5 m reach objective")!=std::string::npos);
  auto changedForest=editor;changedForest.selectedEntityId=23;
  CHECK(!execute(changedForest,chat)&&!landingImports&&shorelineSearches==1);
  CHECK(execute(editor,chat)&&shorelineSearches==2&&!landingImports);
  shorelineResponse->set_value({});
  CHECK(!poll_model_shoreline_import(editor,chat)&&!landingImports&&modelImports==2);
  CHECK(propose(editor,chat,landing)&&execute(editor,chat)&&shorelineSearches==3);
  shorelineResponse->set_value({.playerStart=true});
  CHECK(poll_model_shoreline_import(editor,chat)&&landingImports==1&&modelImports==3);
 }
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
 {
  constexpr auto command="Generate and place a landing beacon near the selected forest: a yellow tripod with a blue light";
  auto beacon=ready();Chat beaconChat;reset_ports();
  beaconChat.queuedInput=command;
  CHECK(queue_allowed(beacon,beaconChat,true,true,true,status));
  CHECK(propose(beacon,beaconChat,command)&&!beacon.aiGoalTools.forest_asset_target&&!dispatched);
  beacon.entities={{"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath},47,"Landing forest"}};
  beacon.selectedEntityId=47;
  CHECK(propose(beacon,beaconChat,command)&&beacon.aiGoalPlanNextQueued&&!beacon.aiToolPlan);
  CHECK(beacon.aiGoalTools.forest_asset_target==47&&beacon.aiGoalTools.stage==0);
  auto next=[&]{return beacon.aiGoalTools.next_request(beacon.aiGoal);};
  auto receipt=[&](bool success){return beacon.aiGoalTools.receipt(beacon.aiToolExecution,success,
    beacon.projectId,beacon.projectRoot,4,beacon.sceneDocumentRevision);};
  CHECK(propose(beacon,beaconChat,next())&&beacon.aiToolPlan.plan.call.tool=="asset.image.generate"&&!dispatched);
  CHECK(execute(beacon,beaconChat)&&dispatched==1&&beacon.aiGoalTools.waiting);
  beacon.assetGenerationPending=false;
  auto& png=beacon.assetGeneration.reference.emplace();
  png.result.valid=png.coding_restored=png.retirement_verified=png.image=true;
  png.project=asset_generation_project(beacon);png.result.output="beacon.png";png.result.output_sha256=std::string(64,'a');
  png.result.work_directory=beacon.aiToolAssetWork;
  CHECK(receipt(completed(beacon,beaconChat)));
  beacon.aiToolExecution.clear();
  CHECK(propose(beacon,beaconChat,next())&&beacon.aiToolPlan.plan.call.tool=="asset.reference.approve");
  auto replaced=beacon;replaced.assetGeneration.reference->result.output="other.png";
  CHECK(!execute(replaced,beaconChat)&&!replaced.assetGeneration.reference_approved);
  CHECK(execute(beacon,beaconChat)&&beacon.assetGeneration.reference_approved);
  beacon.assetGeneration.model_settings=modelState.model_settings;
  beacon.assetGeneration.model_settings_path=modelState.model_settings_path;
  beacon.assetGeneration.model_settings_sha256=modelState.model_settings_sha256;
  CHECK(propose(beacon,beaconChat,next())&&beacon.aiToolPlan.plan.call.tool=="asset.model.generate");
  CHECK(execute(beacon,beaconChat)&&dispatched==2);
  beacon.assetGenerationPending=false;
  auto& glb=beacon.assetGeneration.generated_model.emplace();
  glb.result.valid=glb.coding_restored=glb.retirement_verified=glb.model=true;
  glb.project=asset_generation_project(beacon);glb.result.output="beacon.glb";glb.result.output_sha256=std::string(64,'b');
  glb.result.work_directory=beacon.aiToolAssetWork;glb.reference_sha256=std::string(64,'c');
  auto wrongLineage=beacon;
  CHECK(!completed(wrongLineage,beaconChat)&&wrongLineage.aiGoalTools.stage==2);
  glb.reference_sha256=png.result.output_sha256;
  CHECK(receipt(completed(beacon,beaconChat)));
  beacon.aiToolExecution.clear();
  CHECK(propose(beacon,beaconChat,next())&&beacon.aiToolPlan.plan.call.tool=="asset.model.setup_landing");
  auto otherForest=beacon;otherForest.selectedEntityId=99;
  CHECK(!execute(otherForest,beaconChat));
  CHECK(execute(beacon,beaconChat)&&beacon.aiToolExecution=="asset.model.setup_landing"&&beacon.modelShorelinePending);
  shorelineResponse->set_value({.playerStart=true});
  CHECK(receipt(poll_model_shoreline_import(beacon,beaconChat)));
  CHECK(beacon.aiGoalTools.operation()=="project.save"&&beacon.aiGoalTools.local_review_plan().find("Save the landing area")!=std::string::npos);
  CHECK(forest_asset_step_current(beacon,"project.save"));
  auto changedScene=beacon;++changedScene.sceneDocumentRevision;
  CHECK(!forest_asset_step_current(changedScene,"project.save"));
  beacon.aiGoalTools.waiting=true;
  auto failedSave=beacon.aiGoalTools;
  CHECK(!failedSave.receipt("project.save",false,beacon.projectId,beacon.projectRoot,4,beacon.sceneDocumentRevision));
  CHECK(failedSave.stopped&&failedSave.operation()=="project.save");
  CHECK(!beacon.aiGoalTools.receipt("project.save",true,beacon.projectId,beacon.projectRoot,4,beacon.sceneDocumentRevision));
  CHECK(!beacon.aiGoalTools.stopped&&beacon.aiGoalTools.operation().empty());
 }
 {
  constexpr auto command="Add a custom textured tree to the selected forest:\nBark: silver birch bark\nFoliage: amber cutout leaves";
  auto tree=ready();Chat treeChat;reset_ports();
  tree.entities={{"ForestFactory","ForestTrunk",ForestRegion{kDefaultPlantLabLogicalPath},48,"Original tree"}};
  tree.selectedEntityId=48;
  const auto originalScene=tree.sceneDocumentRevision;
  const auto previousUpdates=paintedUpdates;
  CHECK(submit(tree,treeChat,command)&&tree.aiGoalPlanNextQueued&&!tree.aiToolPlan);
  auto& goal=tree.aiGoalTools;
  const auto next=[&]{return goal.next_request(tree.aiGoal);};
  const auto receipt=[&](bool success){return goal.receipt(tree.aiToolExecution,success,
    tree.projectId,tree.projectRoot,4,tree.sceneDocumentRevision,0,tree.plantLabDocument.revision,tree.plantLabDocument.contentHash);};
  for(unsigned texture=0;texture<2;++texture){
   CHECK(next().find(texture==0?"silver birch bark":"amber cutout leaves")!=std::string::npos);
   CHECK(propose(tree,treeChat,next())&&tree.aiToolPlan.plan.call.tool=="asset.image.generate");
   CHECK(execute(tree,treeChat)&&goal.waiting);
   CHECK(cutoutRequested==(texture==1));
   tree.assetGenerationPending=false;
   auto& png=tree.assetGeneration.reference.emplace();
   png.result.valid=png.coding_restored=png.retirement_verified=png.image=true;
   png.project=asset_generation_project(tree);png.result.output=texture==0?"bark.png":"foliage.png";
   png.result.output_sha256=std::string(64,texture==0?'a':'b');png.result.work_directory=tree.aiToolAssetWork;
   CHECK(receipt(completed(tree,treeChat)));tree.aiToolExecution.clear();
   CHECK(propose(tree,treeChat,next())&&tree.aiToolPlan.plan.call.tool=="forest.material.import");
   CHECK(tree.aiToolPlan.plan.call.arguments.front().value==(texture==0?"bark":"foliage"));
   auto wrongPng=tree;wrongPng.assetGeneration.reference->result.output="substitute.png";
   CHECK(!execute(wrongPng,treeChat));
   CHECK(execute(tree,treeChat));
   CHECK(appliedSlot==(texture==0?forest::ForestTextureSlot::bark:forest::ForestTextureSlot::foliage));
   CHECK(tree.sceneDocumentRevision==originalScene&&paintedUpdates==previousUpdates);
  }
  CHECK(propose(tree,treeChat,next())&&tree.aiToolPlan.plan.call.tool=="forest.species.add");
  auto edited=tree;++edited.plantLabDocument.revision;
  CHECK(!execute(edited,treeChat));
  const auto priorPlacements=speciesPlacements;
  CHECK(execute(tree,treeChat)&&speciesPlacements==priorPlacements+1&&goal.waiting);
  auto failed=goal;
  CHECK(!failed.receipt("forest.species.add",false,tree.projectId,tree.projectRoot,4,originalScene));
  CHECK(failed.stopped&&failed.stage==4);
  ++tree.sceneDocumentRevision;
  CHECK(!receipt(true)&&!goal.stopped&&goal.operation().empty());
  CHECK(tree.entities.front().name=="Original tree"&&paintedUpdates==previousUpdates);
 }
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
            "ACTUAL_BEACON_INGRESS": beacon_ingress,
            "ACTUAL_BEACON_APPROVAL": beacon_approval, "ACTUAL_BEACON_ROUTE": beacon_route,
            "ACTUAL_REVIEW_BINDINGS": "\n".join(definition(owners["assets"], marker) for marker in (
                "        [[nodiscard]] std::string asset_output_review_binding(",
                "        [[nodiscard]] bool forest_asset_step_current(",
                "        [[nodiscard]] bool generated_model_review_current(",
                "        [[nodiscard]] bool review_generated_project_model(",
                "        [[nodiscard]] bool model_import_scene_busy(",
                "        [[nodiscard]] bool import_reviewed_generated_model(")),
            "ACTUAL_REFERENCE_APPROVAL": definition(owners["generation"], "    bool approve_reference("),
            "ACTUAL_REVIEW_INGRESS": review_ingress, "ACTUAL_REVIEW_DISPATCH": review_dispatch,
            "ACTUAL_OBJECTIVE_INGRESS": objective_ingress, "ACTUAL_OBJECTIVE_DISPATCH": objective_dispatch,
            "ACTUAL_SHORELINE_COMPLETION": definition(owners["assets"], "        [[nodiscard]] bool poll_model_shoreline_import("),
            "ACTUAL_IMPORT_INGRESS": import_ingress, "ACTUAL_IMPORT_DISPATCH": import_dispatch,
            "ACTUAL_UPDATE_INGRESS": update_ingress, "ACTUAL_UPDATE_DISPATCH": update_dispatch,
            "ACTUAL_SPECIES_INGRESS": species_ingress, "ACTUAL_SPECIES_DISPATCH": species_dispatch,
            "ACTUAL_PAINTED_TREE_OWNER": definition(owners["scene_preview"], "        [[nodiscard]] bool painted_tree_uses_plant_lab("),
            "ACTUAL_TREE_UPDATE_RESULT": definition(owners["scene_preview"], "        struct PaintedTreeUpdateResult final", True),
            "ACTUAL_GOAL_COMPLETE": definition(owners["runtime"], "        const auto complete_tool_goal =", True),
            "ACTUAL_GOAL_GATES": goal_gates, "ACTUAL_FINISH": finish,
            "ACTUAL_DISPATCH_WRAPPER": definition(owners["runtime"], "        auto dispatch_ai_tool_plan =", True),
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
