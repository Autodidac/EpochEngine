"""Actual idle capture/reload queue and Editor request-lease contracts; CPU only."""
from pathlib import Path
import hashlib,json,re,time,unittest
from Tests.support.cpp import compile_and_run,dependency_include
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

class NativeIdleReload(unittest.TestCase):
 def test_actual_idle_queue_cancel_single_use_and_selection_lease(self):
  owner=ROOT/'Engine/src/ai/ai.native_runtime.cpp';source=owner.read_text(encoding='utf-8')
  state=(ROOT/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
  planning=(ROOT/'Engine/src/editor/application/editor.frame_assistant_planning.inl').read_text(encoding='utf-8')
  chat_source=(ROOT/'Engine/src/editor/application/editor.frame_assistant_chat.inl').read_text(encoding='utf-8')
  conversation=(ROOT/'Engine/src/editor/application/editor.frame_assistant_conversation.inl').read_text(encoding='utf-8')
  entry=(ROOT/'Engine/src/editor/application/editor.frame_assistant_entry.inl').read_text(encoding='utf-8')
  stage=entry[entry.index('            editor.aiDeferredPrompt = std::move(prompt);'):].split('return true;',1)[0]+'return true;'
  queue_owner=conversation[conversation.index('        const bool queueOwnerReady ='):].split(';',1)[0]+';'
  boundary='// EPOCH_'+'PRIVATE_SOURCE_'
  public_owner=lambda text:re.sub(boundary+r'BEGIN:.*?'+boundary+r'END:[^\n]*', '', text,flags=re.S)
  actual={
   'IDLE_STATE':definition(source,'    struct IdleResume final',True),
   'WORK_CONTROL':source.split('    Service();\n',1)[1].split('    void request_shutdown()',1)[0],
   'RETIRE':definition(source,'void poll_idle_models(Service& s) {'),
   'START':'template<bool WithHost = true, typename F>'+source.split('template<bool WithHost = true, typename F>',1)[1].split('template<typename F> bool queue(',1)[0],
   'QUEUE':definition(source,'template<typename F> bool queue('),
   'RELOAD':definition(source,'bool reload_idle_selected('),
   'SELECT':definition(source,'bool select_for_epoch_requests('),
   'CANCEL':definition(source,'void cancel_setup() {'),
   'EDITOR_LEASE':definition(state,'        struct AiModelSelectionLease final',True),
   'EDITOR_GATE':definition(planning.split('auto dispatch_deferred_ai_request',1)[1],'            if (editor.aiMethodSelection == 0)'),
   'EDITOR_ACTION':definition(chat_source,'else if (idleReloadConsentPending)').removeprefix('else '),
   'EDITOR_QUEUE_GATE':conversation[conversation.index('        const bool queuedIdleReloadConsentReady ='):].split(';',1)[0]+';',
   'EDITOR_STAGING':public_owner(stage),
   'EDITOR_QUEUE_OWNER':public_owner(queue_owner),
  }
  code=r'''
#include "ai.native_runtime.hpp"
#include "native/ai.backend_core_error.hpp"
#include "editor/ai/editor.ai_chat_composer_queue.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
using json=nlohmann::json;
namespace epoch_ai::model {struct NativeResidency{std::string id="one";};}
namespace epoch_ai::runtime {
using json=nlohmann::json;
struct RuntimeHost {
 bool one_active=true,two_active=true;
 std::atomic_bool cache_valid{true},profile_valid{true},selection_valid{true},hold{},entered{};
 unsigned starts{},stops{},captures{};
 std::function<void()>after_restore;
 auto error(){return epoch_ai::core::Error{std::make_error_code(std::errc::operation_canceled),"inert cache/profile refusal"};}
 auto capture_native_residency(std::string_view id)->std::expected<std::shared_ptr<const model::NativeResidency>,core::Error>{
  ++captures;if(id!="one"||!selection_valid)return std::unexpected(error());
  return std::make_shared<const model::NativeResidency>();}
 json retire_idle_native_models(std::chrono::steady_clock::time_point,const std::function<void(std::string_view)>&before){
  epochengine::ai::native_runtime::idle::RetirementClaim claim;if(!claim||!one_active)return json::array();
  before("one");one_active=false;return json::array({{{"model","one"},{"retired",true}}});}
 bool native_residency_settings_match(const model::NativeResidency&)const{return profile_valid&&selection_valid;}
 json native_model_library()const{return {{"models",json::array({{{"id","one"},{"active",one_active},{"cache",{{"expected_bytes",1024}}}},{{"id","two"},{"active",two_active}}})}};}
 auto restore_native_residency(const model::NativeResidency&,const epochengine::ai::native_runtime::idle::RetirementClaim&claim,
  std::chrono::steady_clock::time_point,std::stop_token stop,const std::function<bool()>&current)->std::expected<void,core::Error>{
  if(!claim||!current()||!cache_valid)return std::unexpected(error());
  ++starts;entered=true;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{2};
  while(hold&&!stop.stop_requested()&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
  one_active=true;if(after_restore)after_restore();return {}; // Deliberate late-success port, even after cancel.
 }
 void unload_native_model(std::string_view id){if(id!="one")throw std::runtime_error("wrong slot cleanup");one_active=false;++stops;}
 void cancel_native_installation(){} // Existing Cancel owner; this fixture has no installation API.
};}
namespace epochengine::ai::native_runtime {
namespace {
struct Service {
 std::mutex state_mutex,host_mutex,work_mutex;
 std::unique_ptr<epoch_ai::runtime::RuntimeHost>host=std::make_unique<epoch_ai::runtime::RuntimeHost>();
 std::atomic<epoch_ai::runtime::RuntimeHost*>published_host{host.get()};
 std::jthread work;std::atomic_bool busy{},stopping{},loaded{true},enabled{true};
 std::atomic<std::uint64_t>preference_generation{1};std::string model="one",phase;
 IDLE_STATE
 IdleResume idle_resume;
 std::shared_ptr<idle::RetirementClaim>idle_reload_quarantine;
 WORK_CONTROL
 ~Service(){stopping=true;work_cancellation().request_stop();join_work();}
};
Service* active_service;
Service&service(){return *active_service;}
thread_local bool on_native_operation_thread{};
void phase(std::string value){auto&s=service();std::scoped_lock lock{s.state_mutex};s.phase=std::move(value);}
std::vector<std::string>load_traces;
void trace_load_stage(std::string value){load_traces.push_back(std::move(value));}
std::string string_field(const json&value,const char*key){return value.value(key,std::string{});}
auto&ensure_host(Service&s){if(!s.host)throw std::runtime_error("no implicit host");return *s.host;}
bool memory_available=true;
bool safe_to_start_another_model(std::uint64_t,std::string&error){if(!memory_available)error="inert memory admission refusal";return memory_available;}
RETIRE
START
QUEUE
}
RELOAD
SELECT
CANCEL
Snapshot snapshot(bool){auto&s=service();std::scoped_lock lock{s.state_mutex};Snapshot value;
 value.model=s.model;value.selected=s.enabled;value.loaded=s.loaded;value.busy=s.busy;
 value.idle_reload_available=s.idle_resume.captured&&!s.idle_resume.pending&&s.idle_resume.generation==s.preference_generation&&s.idle_resume.model==s.model;
 value.idle_reload_generation=s.idle_resume.generation;value.idle_reload_pending=s.idle_resume.pending;value.idle_reload_error=s.idle_resume.error;return value;}
}
namespace ai=epochengine::ai;
std::mutex aiModelSelectionMutex;
unsigned aiModelSelectionUsers{};
EDITOR_LEASE
enum class AiDeferredRequestKind{None,Chat,Authoring};
struct EditorPort{int aiMethodSelection{};std::shared_ptr<AiModelSelectionLease>aiDeferredModelLease=std::make_shared<AiModelSelectionLease>();
 AiDeferredRequestKind aiDeferredRequestKind=AiDeferredRequestKind::Chat;bool aiDeferredDispatchQueued=true,aiAuthoringPlanForGoal{},aiGoalRunning{},aiAuthoringAwaitingReply{},aiAuthoringApplyPending{};
 std::string aiAuthoringStatus,aiDeferredPrompt="exact model prompt",aiDeferredDisplay="original draft";};
struct ChatPort{std::string input,activeDraftText;bool suppressUserEcho{};std::vector<std::string>status;void append_status(std::string value){status.push_back(std::move(value));}};
bool ai_source_native_work_active(const EditorPort&,const ChatPort&){return false;} // No private source work in this neutral queue lane.
bool queue_owner_ready(EditorPort&editor,ChatPort&chat){EDITOR_QUEUE_OWNER return queueOwnerReady;}
bool stage_request(EditorPort&editor,ChatPort&chat,std::string prompt,std::string display,AiDeferredRequestKind requestKind){EDITOR_STAGING}
bool request_gate(EditorPort&editor,ChatPort&chat){EDITOR_GATE return true;}
void request_action(EditorPort&editor,ChatPort&chat,std::size_t index){const auto idleReloadLease=editor.aiDeferredModelLease;
 const bool idleReloadConsentPending=idleReloadLease&&!idleReloadLease->idleReloadModel.empty()&&!idleReloadLease->idleReloadApproved;
 std::optional<std::size_t>messageActionIndex=index;EDITOR_ACTION}
bool queue_may_offer_reload(ai::native_runtime::Snapshot queuedNative,epochengine::editor::ai_chat_queue::Transport queuedTransport){
 namespace queue=epochengine::editor::ai_chat_queue;EDITOR_QUEUE_GATE return queuedIdleReloadConsentReady;}
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(){using namespace ai::native_runtime;
 const auto retired=[](Service&s){active_service=&s;poll_idle_models(s);};
 const auto join=[](Service&s){if(s.work.joinable())s.work.join();};
 const auto wait=[](Service&s){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{2};while(!s.host->entered&&s.busy&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();};
 {Service s;retired(s);CHECK(!s.loaded&&s.enabled&&s.idle_resume.captured&&s.host->starts==0&&s.host->two_active);
  {auto lease=std::make_shared<AiModelSelectionLease>(false);CHECK(aiModelSelectionUsers==1&&!idle::work_gate.protected_work());
   s.host->hold=true;CHECK(reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));wait(s);CHECK(s.host->entered);
   CHECK(reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));s.host->hold=false;join(s);
   CHECK(s.loaded&&s.enabled&&s.host->starts==1&&s.host->stops==0&&s.host->two_active&&!s.idle_resume.pending);
   CHECK(!reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));}
  CHECK(aiModelSelectionUsers==0);AiModelSelectionLease ordinary;CHECK(idle::work_gate.protected_work());}
 CHECK(aiModelSelectionUsers==0&&!idle::work_gate.protected_work());
 for(unsigned failure=0;failure<5;++failure){Service s;retired(s);auto lease=std::make_shared<AiModelSelectionLease>(false);
  if(failure==0)s.host->cache_valid=false;if(failure==1)s.host->profile_valid=false;
  if(failure==2)s.host->selection_valid=false;if(failure==3)memory_available=false;
  if(failure==4)s.preference_generation++;
  const auto accepted=reload_idle_selected("one",1,lease->idleReloadCancellation.get_token());join(s);
  CHECK(!s.loaded&&s.host->starts==0&&s.host->stops==(failure==0?1u:0u)&&s.host->two_active);
  CHECK(failure==4?!accepted:accepted);CHECK(!reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));memory_available=true;
 }
 {Service s;retired(s);auto lease=std::make_shared<AiModelSelectionLease>(false);s.host->hold=true;
  CHECK(reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));wait(s);CHECK(s.host->entered);lease.reset();join(s);
  CHECK(!s.loaded&&s.host->starts==1&&s.host->stops==1&&s.host->two_active&&!s.idle_resume.pending);}
 {Service s;retired(s);auto lease=std::make_shared<AiModelSelectionLease>(false);
  s.host->after_restore=[&]{s.preference_generation++;s.enabled=false;};
  CHECK(reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));join(s);
  CHECK(!s.loaded&&!s.enabled&&s.host->starts==1&&s.host->stops==1&&s.host->two_active);}
 {Service s;retired(s);auto lease=std::make_shared<AiModelSelectionLease>(false);
  s.host->after_restore=[&]{s.host->selection_valid=false;};
  CHECK(reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));join(s);
  CHECK(!s.loaded&&s.host->stops==1&&s.host->two_active);}
 {Service s;retired(s);auto lease=std::make_shared<AiModelSelectionLease>(false);s.host->hold=true;
  CHECK(reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));wait(s);CHECK(s.host->entered);cancel_setup();join(s);
  CHECK(!s.loaded&&s.host->stops==1&&s.host->two_active&&!s.idle_resume.pending&&!s.idle_resume.captured);
  CHECK(s.idle_resume.error.find("cancelled")!=std::string::npos&&!reload_idle_selected("one",1,lease->idleReloadCancellation.get_token()));}
 {Service s;retired(s);AiModelSelectionLease protected_request;
  CHECK(reload_idle_selected("one",1,protected_request.idleReloadCancellation.get_token()));join(s);
  CHECK(!s.loaded&&s.host->starts==0&&s.host->two_active);}
 {Service s;retired(s);std::stop_source cancelled;cancelled.request_stop();CHECK(!reload_idle_selected("one",1,cancelled.get_token())&&s.host->starts==0);}
 {Service s;active_service=&s;s.host->selection_valid=false;poll_idle_models(s);CHECK(!s.enabled&&!s.loaded&&!s.idle_resume.captured&&s.host->starts==0);
  CHECK(!s.idle_resume.error.empty()&&load_traces.size()>1&&load_traces[load_traces.size()-2].find("capture refused")!=std::string::npos);}
 {Service s;retired(s);EditorPort editor;ChatPort chat;
  const auto queued=ai::native_runtime::snapshot(false);using epochengine::editor::ai_chat_queue::Transport;
  CHECK(queue_may_offer_reload(queued,Transport::native)&&!queue_may_offer_reload(queued,Transport::local_api));
  auto changed=queued;changed.idle_reload_available=false;CHECK(!queue_may_offer_reload(changed,Transport::native));
  changed=queued;changed.busy=true;CHECK(!queue_may_offer_reload(changed,Transport::native));
  changed=queued;changed.selected=false;CHECK(!queue_may_offer_reload(changed,Transport::native));
  for(unsigned i=0;i<4;++i)CHECK(!request_gate(editor,chat));
  CHECK(s.host->starts==0&&chat.status.size()==1&&chat.status.back().find("one")!=std::string::npos);
  CHECK(chat.status.back().find("Load-time estimate unavailable")!=std::string::npos&&!idle::work_gate.protected_work());
  CHECK(!reload_idle_selected("two",1,{})&&!reload_idle_selected("one",2,{}));
  request_action(editor,chat,0);CHECK(editor.aiDeferredModelLease->idleReloadApproved&&s.host->starts==0);
  CHECK(!request_gate(editor,chat));join(s);CHECK(s.loaded&&s.host->starts==1&&s.host->two_active);
  CHECK(request_gate(editor,chat));}
 {Service s;retired(s);EditorPort editor;ChatPort chat;CHECK(!request_gate(editor,chat));
  editor.aiDeferredRequestKind=AiDeferredRequestKind::Authoring;
  editor.aiAuthoringAwaitingReply=true; // Deferred Plan owns this flag before model admission.
  request_action(editor,chat,1);CHECK(chat.input=="original draft"&&editor.aiDeferredPrompt.empty()&&!editor.aiDeferredDispatchQueued);
  CHECK(editor.aiDeferredRequestKind==AiDeferredRequestKind::None&&!editor.aiDeferredModelLease&&s.host->starts==0);
  CHECK(!editor.aiAuthoringAwaitingReply);}
 {Service s;retired(s);EditorPort editor;ChatPort chat;chat.input="newer draft";CHECK(!request_gate(editor,chat));
  s.preference_generation++;request_action(editor,chat,0);CHECK(!editor.aiDeferredModelLease->idleReloadApproved&&s.host->starts==0);
  request_action(editor,chat,1);CHECK(chat.input=="newer draft"&&s.host->starts==0);}
 {Service s;retired(s);EditorPort editor;ChatPort chat;
  const std::string raw="Reply with exactly: reload confirmed. Do not use tools or change the project.";
  chat.activeDraftText=raw;chat.suppressUserEcho=true;
  CHECK(stage_request(editor,chat,"protocol-for-same-request","Plan: "+raw,AiDeferredRequestKind::Authoring));
  editor.aiAuthoringAwaitingReply=true;CHECK(!queue_owner_ready(editor,chat)&&!request_gate(editor,chat));
  const auto cancelledLease=editor.aiDeferredModelLease;
  CHECK(cancelledLease->retainedDraft==raw&&editor.aiDeferredPrompt=="protocol-for-same-request");
  request_action(editor,chat,1);
  CHECK(chat.input==raw&&queue_owner_ready(editor,chat)&&cancelledLease->idleReloadCancellation.stop_requested());
  CHECK(s.idle_resume.captured&&ai::native_runtime::snapshot(false).idle_reload_available&&s.host->starts==0);
  chat.activeDraftText=chat.input;chat.input.clear();
  CHECK(stage_request(editor,chat,"new-request-protocol","Plan: "+raw,AiDeferredRequestKind::Authoring));
  editor.aiAuthoringAwaitingReply=true;CHECK(editor.aiDeferredModelLease!=cancelledLease&&!request_gate(editor,chat));
  CHECK(!editor.aiDeferredModelLease->idleReloadApproved&&s.host->starts==0);
  request_action(editor,chat,0);CHECK(editor.aiDeferredModelLease->idleReloadApproved&&s.host->starts==0);
  CHECK(!request_gate(editor,chat));join(s);CHECK(s.loaded&&s.host->starts==1&&s.host->two_active);}
 return 0;
}
'''
  for key,value in sorted(actual.items(),key=lambda item:len(item[0]),reverse=True):code=code.replace(key,value)
  report=ROOT/'Artifacts/Reports/native-idle-reload-oct29'/str(time.time_ns());report.mkdir(parents=True)
  cpp=report/'consumer.cpp';cpp.write_text(code,encoding='utf-8')
  cases=compile_and_run(self,cpp,report,includes=[ROOT/'Engine/src/ai',ROOT/'Engine/src',dependency_include(ROOT,'nlohmann/json.hpp')],compile_timeout=90)
  (report/'result.json').write_text(json.dumps({'passed':True,'cases':cases,'owner_sha256':hashlib.sha256(owner.read_bytes()).hexdigest(),
   'scope':'Actual idle poll, single-use native queue/reload and Editor selection lease with real idle gate and cancellation; inert Host/cache/process/resource ports. No GUI/model/network/provision.'},indent=2),encoding='utf-8')
