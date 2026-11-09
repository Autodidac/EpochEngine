"""Actual companion projection/cleanup owners with two canonical contexts; CPU only."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tools.source.owned_source_text import read_owned_source

ROOT = Path(__file__).resolve().parents[2]


class DetachedProjectionOwnership(unittest.TestCase):
    def test_focused_takeover_detached_retention_interleaving_and_cleanup(self):
        owner = ROOT/'Engine/src/editor/application/editor.application_pane_routing.inl'
        source = owner.read_text(encoding='utf-8')
        types = '\n'.join(definition(source, marker, True) for marker in (
            '        enum class DetachedAiChatCommandKind',
            '        struct DetachedAiChatCommand final',
            '        struct DetachedPaneProjection',
            '        struct DetachedGuiHierarchySelection final'))
        publish = definition(source, '        void publish_detached_pane_projection(')
        cleanup_owner = ROOT/'Engine/src/editor/application/editor.application_context_lifecycle.inl'
        cleanup_source = read_owned_source(cleanup_owner)
        cleanup = definition(cleanup_source, '    void cleanup_chat_context(')
        shutdown = definition(cleanup_source, '    void shutdown_chat_system(')
        shutdown_storage = definition(shutdown, '        {\n            std::scoped_lock lock(chatStorage.mutex, editorStorage.mutex);')
        cpp = r'''
#define EPOCH_EDITION_PUBLIC 1
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#undef assert
#define assert(value) do{if(!(value)){std::fprintf(stderr,"Projection contract line %d: %s\n",__LINE__,#value);std::exit(81);}}while(false)
inline unsigned lockDepth{},copies{},cleanupCallbacks{};
struct TrackedMutex {
 std::mutex value;std::function<void()> before_lock;
 void lock(){auto hook=std::exchange(before_lock,{});if(hook)hook();value.lock();++lockDepth;}
 bool try_lock(){if(!value.try_lock())return false;++lockDepth;return true;}
 void unlock(){--lockDepth;value.unlock();}
};
namespace core {
enum class ScenePreviewMode {Editor,None};
struct Context;
inline const Context* focused{};
struct Context {
 mutable unsigned calls{};mutable std::function<void()> on_focus;
 bool has_input_focus_safe()const{
  assert(lockDepth==0);++calls;if(on_focus)on_focus();return focused==this;
 }
};
}
namespace scene {using SceneObjectId=std::uint64_t;inline constexpr SceneObjectId kInvalidSceneObjectId=0;}
namespace gui {struct ChatMessageRunInfo {std::uint64_t run{};};}
namespace authoring::gui {using WidgetHandle=std::uint64_t;}
namespace editor_hierarchy {struct GuiHierarchyProjection {bool ready{};};}
enum class EditorApplicationKind {Standard,GuiEditor};
enum class AiDeferredRequestKind {None,Chat};
enum class EditorToolPane {WorldOutliner,GuiHierarchy,Properties,Output,AiChat};
std::optional<EditorToolPane> editor_tool_pane_from_route(std::string_view route){
 if(route=="pane.gui_hierarchy")return EditorToolPane::GuiHierarchy;
 if(route=="pane.properties")return EditorToolPane::Properties;
 if(route=="pane.output")return EditorToolPane::Output;
 if(route=="pane.ai_chat")return EditorToolPane::AiChat;return {};
}
bool is_outliner_tool_pane(EditorToolPane p){return p==EditorToolPane::WorldOutliner||p==EditorToolPane::GuiHierarchy;}
bool is_inspector_tool_pane(EditorToolPane p){return p==EditorToolPane::Properties;}
bool is_console_tool_pane(EditorToolPane p){return p==EditorToolPane::Output;}
struct EditorEntity {
 unsigned id{};std::string name;
 EditorEntity()=default;EditorEntity(unsigned i,std::string n):id(i),name(std::move(n)){}
 EditorEntity(const EditorEntity& e):id(e.id),name(e.name){++copies;}
 EditorEntity& operator=(const EditorEntity&)=default;
};
struct EditorState {
 EditorApplicationKind applicationKind{};std::string projectName,projectId,projectRoot,guiDocumentSourcePath,activeWorld,aiAuthoringStatus;
 std::vector<EditorEntity> entities;std::uint64_t selectedEntityId{},sceneDocumentRevision{};
 std::map<std::uint64_t,authoring::gui::WidgetHandle> guiDocumentWidgets;
 std::vector<std::string> logLines;AiDeferredRequestKind aiDeferredRequestKind{};
 bool aiDeferredDispatchQueued{},automation{};core::ScenePreviewMode previewMode=core::ScenePreviewMode::Editor;
 std::shared_ptr<const std::vector<gui::ChatMessageRunInfo>> detachedChatRunSnapshot;
 std::uint64_t detachedChatRawSequence{},detachedChatCompletionGeneration{};
 std::chrono::steady_clock::time_point detachedChatRunSnapshotAt{};
};
struct AiChat {
 std::uint64_t nativeRawSequence{},completionGeneration{};
 std::vector<std::string> lines;std::vector<gui::ChatMessageRunInfo> lineRunDetails;
 std::shared_ptr<int> pending;bool requestRetirementFailed{},cancelled{},retired{};
 bool cancel_pending(){assert(lockDepth==0);cancelled=true;++cleanupCallbacks;return true;}
};
bool editor_automation_input_reserved(const EditorState& state,const AiChat&){return state.automation;}
TYPES
struct EditorStorage {
 TrackedMutex mutex;std::map<const core::Context*,EditorState> states;
 std::unordered_map<std::string,bool> detachedPaneRoutes,paneRedockRequests;
 std::vector<DetachedAiChatCommand> detachedAiChatCommands;
 std::vector<DetachedGuiHierarchySelection> detachedGuiHierarchySelections;
 DetachedPaneProjection detachedPaneProjection;std::uint64_t detachedProjectionGeneration{};int passiveContextScores{};
};
struct ChatStorage {TrackedMutex mutex;std::map<const core::Context*,std::shared_ptr<AiChat>> chats;bool bot_initialized{};};
EditorStorage& editor_storage(){static EditorStorage s;return s;}
ChatStorage& chat_storage(){static ChatStorage s;return s;}
void persist_editor_layout_preferences(EditorState&,bool){assert(lockDepth==0);++cleanupCallbacks;}
void request_editor_task_session_stop(EditorState&){assert(lockDepth==0);}
void retire_detached_chat(const std::shared_ptr<AiChat>& chat){assert(lockDepth==0);if(chat)chat->retired=true;}
void finish_ai_work_owner_close(EditorState&,bool){assert(lockDepth==0);++cleanupCallbacks;}
namespace epochengine {
namespace canvas2d::scene_content {bool retire(const core::Context*){assert(lockDepth==0);return true;}}
namespace previewgrid {void cleanup_context(const core::Context*){assert(lockDepth==0);}}
}
std::shared_ptr<const editor_hierarchy::GuiHierarchyProjection> gui_hierarchy_projection(EditorState&){return {};}
std::optional<authoring::gui::WidgetHandle> selected_gui_widget(const EditorState& state){
 const auto found=state.guiDocumentWidgets.find(state.selectedEntityId);
 return found==state.guiDocumentWidgets.end()?std::nullopt:std::optional{found->second};
}
PUBLISH
template<class... Args> void publish_focus_change(bool& changed,Args&&... args){
 changed=false;
 if constexpr(requires{publish_detached_pane_projection(std::forward<Args>(args)...,&changed);})
  publish_detached_pane_projection(std::forward<Args>(args)...,&changed);
 else publish_detached_pane_projection(std::forward<Args>(args)...);
}
CLEANUP
void shutdown_storage_contract(){
 auto& chatStorage=chat_storage();auto& editorStorage=editor_storage();
 decltype(chatStorage.chats) retiredChats;decltype(editorStorage.states) retiredStates;bool shutdownAi{};
 const auto before=editorStorage.detachedProjectionGeneration;
 SHUTDOWN_STORAGE
 assert(editorStorage.states.empty()&&chatStorage.chats.empty()&&!editorStorage.detachedPaneProjection.ready
  &&editorStorage.detachedPaneProjection.ownerContext==nullptr&&editorStorage.detachedProjectionGeneration==before+1);
 (void)shutdownAi;
}
int main(){
 core::Context a,b,unknown;auto& s=editor_storage();auto& chats=chat_storage();
 auto& ea=s.states[&a];ea.projectName="A";ea.activeWorld="WorldA";
 ea.entities.emplace_back(1,"A-object");ea.selectedEntityId=1;ea.sceneDocumentRevision=10;
 auto& eb=s.states[&b];eb.projectName="B";eb.activeWorld="WorldB";
 eb.entities.emplace_back(2,"B-object");eb.selectedEntityId=2;eb.sceneDocumentRevision=20;
 auto ca=std::make_shared<AiChat>(),cb=std::make_shared<AiChat>();
 ca->lines={"A-chat"};cb->lines={"B-chat"};ca->lineRunDetails={{1}};cb->lineRunDetails={{2}};
 chats.chats[&a]=ca;chats.chats[&b]=cb;
 core::focused=&a;
 const auto dockedCopies=copies;
 publish_detached_pane_projection(&a,ea,*ca);
 assert(a.calls==1&&!s.detachedPaneProjection.ready&&copies==dockedCopies);
 assert(s.detachedPaneProjection.ownerContext==&a);
 const auto dockedGeneration=s.detachedProjectionGeneration;
 // Creating the first popout transfers native focus before its source's next
 // frame. Retain only the last focused identity while docked, not a deep copy.
 core::focused=nullptr;
 for(unsigned i=0;i<100;++i)publish_detached_pane_projection(&b,eb,*cb);
 assert(s.detachedPaneProjection.ownerContext==&a&&!s.detachedPaneProjection.ready&&copies==dockedCopies
  &&s.detachedProjectionGeneration==dockedGeneration);
 s.detachedPaneRoutes={{"pane.gui_hierarchy",true},{"pane.ai_chat",true},{"pane.output",true}};
 const auto before=copies;publish_detached_pane_projection(&b,eb,*cb);
 assert(!s.detachedPaneProjection.ready&&copies==before); // Background cannot claim first ownership.
 publish_detached_pane_projection(&a,ea,*ca);
 assert(s.detachedPaneProjection.ownerContext==&a&&s.detachedPaneProjection.projectName=="A");
 assert(s.detachedPaneProjection.entities[0].name=="A-object"&&s.detachedPaneProjection.chatLines[0]=="A-chat");
 ea.automation=true;publish_detached_pane_projection(&a,ea,*ca);assert(s.detachedPaneProjection.automationInputReserved);
 ea.automation=false;publish_detached_pane_projection(&a,ea,*ca);assert(!s.detachedPaneProjection.automationInputReserved);
 core::focused=nullptr;ea.sceneDocumentRevision=11;publish_detached_pane_projection(&a,ea,*ca);
 const auto retained=copies;for(unsigned i=0;i<100;++i)publish_detached_pane_projection(&b,eb,*cb);
 assert(s.detachedPaneProjection.ownerContext==&a&&s.detachedPaneProjection.sceneRevision==11&&copies==retained);
 s.detachedAiChatCommands={{&a,DetachedAiChatCommandKind::submit,"A-only"}};
 bool focusChanged{};core::focused=&b;publish_focus_change(focusChanged,&b,eb,*cb);
 if(!focusChanged)return 97; // Before repair, takeover changed data but raised no companions.
 assert(s.detachedPaneProjection.ownerContext==&b&&s.detachedPaneProjection.selectedEntityId==2);
 assert(!s.detachedPaneProjection.aiDetachedSubmitQueued);
 focusChanged=true;publish_focus_change(focusChanged,&b,eb,*cb);assert(!focusChanged);
 core::focused=nullptr;focusChanged=true;publish_focus_change(focusChanged,&b,eb,*cb);assert(!focusChanged);
 focusChanged=true;publish_focus_change(focusChanged,&a,ea,*ca);assert(!focusChanged);
 core::focused=&b;
 s.detachedAiChatCommands.push_back({&b,DetachedAiChatCommandKind::submit,"B-only"});
 publish_detached_pane_projection(&b,eb,*cb);assert(s.detachedPaneProjection.aiDetachedSubmitQueued);
 // Interleave B's focused takeover after A copied its payload but before A commits.
 core::focused=&a;publish_detached_pane_projection(&a,ea,*ca);
 unsigned phase{};a.on_focus=[&]{if(++phase==2){core::focused=&b;publish_detached_pane_projection(&b,eb,*cb);}};
 focusChanged=true;publish_focus_change(focusChanged,&a,ea,*ca);a.on_focus={};
 assert(!focusChanged);
 assert(s.detachedPaneProjection.ownerContext==&b&&s.detachedPaneProjection.projectName=="B"
  &&s.detachedPaneProjection.entities[0].name=="B-object");
 // A redock during copying cannot resurrect a closed companion or an owner.
 phase=0;b.on_focus=[&]{if(++phase==2)s.detachedPaneRoutes.clear();};
 publish_detached_pane_projection(&b,eb,*cb);b.on_focus={};assert(!s.detachedPaneProjection.ready);
 s.detachedPaneRoutes["pane.gui_hierarchy"]=true;publish_detached_pane_projection(&b,eb,*cb);
 // Unknown/focused producers cannot publish; cleanup of a foreign context
 // removes only its commands and leaves the current canonical projection live.
 core::focused=&unknown;focusChanged=true;publish_focus_change(focusChanged,&unknown,eb,*cb);
 assert(!focusChanged);
 assert(unknown.calls==0&&s.detachedPaneProjection.ownerContext==&b);
 DetachedGuiHierarchySelection aSelection,bSelection;
 aSelection.ownerContext=&a;bSelection.ownerContext=&b;
 s.detachedGuiHierarchySelections={aSelection,bSelection};
 cleanup_chat_context(&a);assert(!s.states.contains(&a)&&ca->cancelled&&ca->retired);
 assert(s.detachedPaneProjection.ownerContext==&b&&s.detachedAiChatCommands.size()==1);
 assert(s.detachedAiChatCommands[0].ownerContext==&b);
 assert(s.detachedGuiHierarchySelections.size()==1&&s.detachedGuiHierarchySelections[0].ownerContext==&b);
 // The real cleanup owner can retire B while its immutable copy is in flight.
 // Even a stale native focus report cannot authorize a retired producer.
 core::focused=&b;phase=0;b.on_focus=[&]{if(++phase==2)cleanup_chat_context(&b);};
 focusChanged=true;publish_focus_change(focusChanged,&b,eb,*cb);b.on_focus={};
 assert(!focusChanged);
 assert(s.states.empty()&&s.detachedAiChatCommands.empty()&&!s.detachedPaneProjection.ready
  &&s.detachedPaneProjection.ownerContext==nullptr);
 assert(s.detachedGuiHierarchySelections.empty());
 assert(cb->cancelled&&cb->retired&&cleanupCallbacks==6&&lockDepth==0);
 // A producer can retire during the docked-path focus query too. Do not
 // publish its identity or touch its extracted EditorState afterward.
 s.detachedPaneRoutes.clear();auto& retired=s.states[&unknown];
 core::focused=&unknown;unknown.on_focus=[&]{s.states.erase(&unknown);};
 publish_detached_pane_projection(&unknown,retired,*cb);unknown.on_focus={};
 assert(s.states.empty()&&s.detachedPaneProjection.ownerContext==nullptr&&lockDepth==0);
 // Retirement during the FIRST focus callback of an already detached owner
 // must be refused before reading the now-extracted EditorState payload.
 core::Context c;auto& ec=s.states[&c];ec.projectName="C";ec.entities.emplace_back(3,"C-object");
 auto cc=std::make_shared<AiChat>();chats.chats[&c]=cc;
 s.detachedPaneRoutes["pane.properties"]=true;s.detachedPaneProjection={.ownerContext=&c};
 core::focused=&c;c.on_focus=[&]{cleanup_chat_context(&c);};
 const auto firstRetireCopies=copies;publish_detached_pane_projection(&c,ec,*cc);c.on_focus={};
 assert(!s.states.contains(&c)&&cc->cancelled&&cc->retired&&copies==firstRetireCopies
  &&!s.detachedPaneProjection.ready&&s.detachedPaneProjection.ownerContext==nullptr);
 // Keep the extracted node alive to guarantee a different EditorState address:
 // a reused context key is not authority to publish the former state's bytes.
 core::Context replacement;auto& initial=s.states[&replacement];initial.projectName="Old";
 initial.entities.emplace_back(4,"Old-object");
 decltype(s.states)::node_type oldState,secondState,thirdState;
 s.detachedPaneProjection={.ownerContext=&replacement};core::focused=&replacement;
 phase=0;replacement.on_focus=[&]{if(++phase==1){oldState=s.states.extract(&replacement);s.states[&replacement].projectName="New";}};
 const auto replaceCopies=copies;publish_detached_pane_projection(&replacement,initial,*cc);replacement.on_focus={};
 assert(s.states.at(&replacement).projectName=="New"&&!s.detachedPaneProjection.ready&&copies==replaceCopies);
 const auto replaceCalls=replacement.calls;
 publish_detached_pane_projection(&replacement,oldState.mapped(),*cc);
 assert(replacement.calls==replaceCalls&&copies==replaceCopies); // initial exact-state guard
 // Replacement during the final native query may not commit a stale copy.
 auto& second=s.states.at(&replacement);second.entities.emplace_back(5,"New-object");
 phase=0;replacement.on_focus=[&]{if(++phase==2){secondState=s.states.extract(&replacement);s.states[&replacement].projectName="Newest";}};
 publish_detached_pane_projection(&replacement,second,*cc);replacement.on_focus={};
 assert(s.states.at(&replacement).projectName=="Newest"&&!s.detachedPaneProjection.ready);
 // The no-detached identity-only path also rechecks exact state after query.
 s.detachedPaneRoutes.clear();auto& third=s.states.at(&replacement);
 third.detachedChatRunSnapshot=std::make_shared<const std::vector<gui::ChatMessageRunInfo>>();
 phase=0;replacement.on_focus=[&]{if(++phase==1){thirdState=s.states.extract(&replacement);s.states[&replacement].projectName="Newest docked";}};
 publish_detached_pane_projection(&replacement,third,*cc);replacement.on_focus={};
 assert(thirdState.mapped().detachedChatRunSnapshot&&s.states.at(&replacement).projectName=="Newest docked"
  &&!s.detachedPaneProjection.ready&&lockDepth==0);
 // B refreshes B AFTER A's second focus query returned true but before its
 // commit lock. Pointer-only observed-owner comparison cannot detect B->B.
 core::Context d,e;auto& ed=s.states[&d];auto& ee=s.states[&e];
 ed.projectName="D";ed.entities.emplace_back(6,"D-object");
 ee.projectName="E";ee.entities.emplace_back(7,"E-object");
 s.detachedPaneRoutes["pane.properties"]=true;core::focused=&e;
 publish_detached_pane_projection(&e,ee,*cc);const auto readyGeneration=s.detachedProjectionGeneration;
 core::focused=&d;phase=0;d.on_focus=[&]{if(++phase==2)s.mutex.before_lock=[&]{core::focused=&e;publish_detached_pane_projection(&e,ee,*cc);};};
 publish_detached_pane_projection(&d,ed,*cc);d.on_focus={};
 assert(s.detachedPaneProjection.ownerContext==&e&&s.detachedPaneProjection.projectName=="E"
  &&s.detachedProjectionGeneration==readyGeneration+1&&lockDepth==0);
 // The identity-only branch has the same window after its one native query.
 s.detachedPaneRoutes.clear();core::focused=&e;publish_detached_pane_projection(&e,ee,*cc);
 const auto identityGeneration=s.detachedProjectionGeneration;const auto identityCopies=copies;
 core::focused=&d;d.on_focus=[&]{s.mutex.before_lock=[&]{core::focused=&e;publish_detached_pane_projection(&e,ee,*cc);};};
 publish_detached_pane_projection(&d,ed,*cc);d.on_focus={};
 assert(s.detachedPaneProjection.ownerContext==&e&&!s.detachedPaneProjection.ready
  &&s.detachedProjectionGeneration==identityGeneration+1&&copies==identityCopies);
 // Actual locked shutdown storage invalidates in-flight publication tokens.
 shutdown_storage_contract();
 // Saturate rather than wrap: after the last valid publication every new
 // publisher refuses without native calls or payload copies; cleanup still clears.
 core::Context f;auto& ef=s.states[&f];ef.projectName="F";core::focused=&f;
 s.detachedProjectionGeneration=(std::numeric_limits<std::uint64_t>::max)()-1;
 publish_detached_pane_projection(&f,ef,*cc);
 assert(s.detachedProjectionGeneration==(std::numeric_limits<std::uint64_t>::max)()
  &&s.detachedPaneProjection.ownerContext==&f);
 const auto exhaustedCalls=f.calls;const auto exhaustedCopies=copies;
 focusChanged=true;publish_focus_change(focusChanged,&f,ef,*cc);
 assert(f.calls==exhaustedCalls&&copies==exhaustedCopies);
 assert(!focusChanged);
 cleanup_chat_context(&f);
 assert(!s.detachedPaneProjection.ready&&s.detachedPaneProjection.ownerContext==nullptr
  &&s.detachedProjectionGeneration==(std::numeric_limits<std::uint64_t>::max)());
 publish_detached_pane_projection(nullptr,EditorState{},*cb);
}
'''.replace('TYPES',types).replace('PUBLISH',publish).replace('CLEANUP',cleanup).replace('SHUTDOWN_STORAGE',shutdown_storage)
        # A null producer does not read EditorState; keep a named lvalue for its API.
        cpp = cpp.replace('publish_detached_pane_projection(nullptr,EditorState{},*cb);',
                          'EditorState unused;publish_detached_pane_projection(nullptr,unused,*cb);')
        proof = ROOT/'Artifacts/Reports/companion-focus-oct27/projection'
        proof.mkdir(parents=True,exist_ok=True)
        path = proof/'contract.cpp';path.write_text(cpp,encoding='utf-8')
        if os.name=='nt':
            tool=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))[-1]
            sdk=Path('C:/Program Files (x86)/Windows Kits/10');kit=sorted((sdk/'Include').glob('*'))[-1]
            binary=proof/'contract.exe'
            command=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX',
                '/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/Fo'+str(proof/'contract.obj'),'/Fe'+str(binary),str(path),'/link',
                '/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
        else:
            compiler=shutil.which('clang++') or shutil.which('g++')
            if not compiler:self.skipTest('C++23 compiler required')
            binary=proof/'contract';command=[compiler,'-std=c++23',str(path),'-o',str(binary)]
        build=subprocess.run(command,cwd=proof,capture_output=True,text=True,timeout=60)
        (proof/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
        self.assertEqual(build.returncode,0,build.stdout[-6000:]+build.stderr)
        run=subprocess.run([str(binary)],cwd=proof,capture_output=True,text=True,timeout=10)
        (proof/'run.log').write_text(run.stdout+run.stderr,encoding='utf-8')
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        (proof/'result.json').write_text(json.dumps({'compile_exit':build.returncode,'cpu_exit':run.returncode,
            'owner_sha256':hashlib.sha256(owner.read_bytes()).hexdigest(),
            'scope':'Actual publish_detached_pane_projection + cleanup_chat_context; two canonical context identities, no GUI',
            'cases':['first popout after source loses focus','docked owner identity retained without payload copying',
                'focused takeover','unfocused current owner retained with detached focus','background publication refused before copies',
                'takeover rechecked after payload copy','redock during publication','unknown owner rejected',
                'foreign cleanup preserves live projection','cleanup during publication rejects retired owner',
                'retirement during docked focus query','first focus query retirement before detached payload read',
                'same context key with replaced state refused initially/after first query/at commit/while docked',
                'stale focus after query return before commit rejects foreign ready snapshot/identity refresh',
                'foreign same-owner refresh advances generation', 'unfocused docked no-op does not advance',
                'shutdown clear invalidates generation', 'generation exhaustion refuses without ABA wrap',
                'no native callback under storage locks'],
            'native_acceptance':False,'deferred':['Detached GUI document native acceptance','Compressed2D native bounds/header acceptance']},indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':unittest.main()
