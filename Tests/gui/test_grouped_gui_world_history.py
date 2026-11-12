"""Actual document journals, group coordinator and Editor layout/Undo owners; no GUI launch."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

FIXTURE=r'''

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
import authoring.gui_document;
import scene.document;
import scene.snapshot;
import editor.authoring_history;
namespace authoring=epochengine::authoring;
namespace scene=epochengine::scene;
namespace editor_authoring_history=epochengine::editor_authoring_history;
struct EditorEntity {scene::SceneObjectId sceneObjectId{};std::array<float,3> position{},rotation{},scale{1,1,1};std::string name,type;std::optional<scene::SceneTextureMaterialSnapshot> textureMaterial;};
struct EditorState {
 struct SchedulerPort {bool cancel(unsigned){return true;}};
 authoring::scene::SceneDocument sceneDocument;std::unique_ptr<authoring::gui::GuiDocument> guiDocument;
 std::vector<EditorEntity> entities;std::unordered_map<scene::SceneObjectId,authoring::gui::WidgetHandle> guiDocumentWidgets;
 editor_authoring_history::Controller authoringHistory;
 scene::SceneObjectId selectedEntityId{7};std::uint64_t sceneDocumentRevision{};
 struct {std::uint64_t frame_index{};double simulated_seconds{};} timeSnapshot;
 bool sceneInteractionDirty{},sceneLightingDirty{};std::string guiDocumentStatus;
 authoring::gui::WidgetHandle guiDocumentRoot{};int guiSelectedWidget{},guiContentEditorWidget{},guiCanvasView{};
 bool guiDocumentLoadAttempted{true},guiDocumentReplaceArmed{};std::uint64_t guiDocumentSavedRevision{};
 std::string guiDocumentSourcePath;
 std::unique_ptr<SchedulerPort> taskScheduler;
 std::optional<int> forestSurfacePending,forestSurfacePlacement;
 unsigned forestSurfaceCancellation{};
 scene::SceneObjectId forestBrushTerrain{scene::kInvalidSceneObjectId};
 std::vector<std::array<double,3>> forestBrushDirections;

};
void push_editor_log(EditorState&,const std::string&){}
std::optional<std::size_t> editor_entity_index(const EditorState& state,scene::SceneObjectId id){
 for(std::size_t i=0;i<state.entities.size();++i)if(state.entities[i].sceneObjectId==id)return i;return {};}

namespace epochengine {template<class...Args>std::string format_text(std::string_view value,const Args&...){return std::string{value};}}
bool load_project_gui_document(EditorState&){return false;}
authoring::gui::TemplatePreset project_gui_starter_template(EditorState&){return authoring::gui::TemplatePreset::blank_canvas;}
std::filesystem::path project_gui_document_path(EditorState&){return {};}
std::string display_project_path(const std::filesystem::path& value){return value.string();}
unsigned fallback_projection_calls{};
bool project_scene_document_into_editor(EditorState&){++fallback_projection_calls;return false;}
void reconcile_gui_document_projections(EditorState&,authoring::gui::GuiDocument&){++fallback_projection_calls;}
__ACTUAL_OWNER_FUNCTIONS__


#define REQUIRE(value) do{if(!(value)){std::fprintf(stderr,"failure line %d: %s\n",__LINE__,#value);return __LINE__;}}while(false)
using namespace authoring::gui;
using namespace editor_authoring_history;
struct Fixture {
 EditorState state;WidgetHandle button;LayoutDescriptor initial;
 Fixture(){
  state.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{0,1},BranchIdentity{1,1});
  auto canvas=state.guiDocument->create_widget({.kind=WidgetKind::canvas,.name="Canvas",.content=CanvasContent{}});
  WidgetDescriptor descriptor{.kind=WidgetKind::button,.name="Button",.layout={.x=40,.y=20,.width=100,.height=30},.content=ButtonContent{"Home"}};
  button=state.guiDocument->create_widget(descriptor,canvas.widget).widget;initial=descriptor.layout;state.guiDocumentRoot=canvas.widget;
  scene::SceneSnapshot snapshot;snapshot.scene_id="grouped_layout";snapshot.project_id="history_project";
  snapshot.objects.push_back({.id=7,.name="ButtonProxy",.type="GuiButton",.category="UI",
   .position={initial.x/100.0f-3.2f,1.8f-initial.y/100.0f,0},.scale={initial.width/100.0f,initial.height/100.0f,1}});
  snapshot.objects.push_back({.id=8,.name="Cube",.type="Cube",.scale={1,1,1}});
  auto built=authoring::scene::SceneDocument::from_snapshot(snapshot);
  state.sceneDocument=std::move(built.value);
  for(const auto& object:snapshot.objects)state.entities.push_back({.sceneObjectId=object.id,.position=object.position,.rotation=object.rotation,.scale=object.scale,.name=object.name,.type=object.type});
  state.guiDocumentWidgets.emplace(7,button);
 }
 LayoutDescriptor changed(float delta=50)const{auto value=initial;value.x+=delta;return value;}
};
int layout_roundtrip(){
 Fixture f;auto& s=f.state;const auto* address=s.guiDocument.get();
 const auto wb=s.sceneDocument.revision().content;const auto gb=s.guiDocument->revision().content;
 const auto wc=s.sceneDocument.metrics().applied_transactions;const auto gc=s.guiDocument->metrics().applied_operations;
 REQUIRE(commit_gui_layout_edit(s,f.button,f.changed()));
 REQUIRE(s.sceneDocument.metrics().applied_transactions==wc+1&&s.guiDocument->metrics().applied_operations==gc+1);
 const auto wa=s.sceneDocument.revision().content;const auto ga=s.guiDocument->revision().content;
 const auto sequence=s.guiDocument->revision().sequence;
 REQUIRE(synchronize_gui_document(s));REQUIRE(s.guiDocument->revision().sequence==sequence);

 REQUIRE(s.entities[0].position[0]==f.changed().x/100.0f-3.2f&&s.entities[1].position[0]==0);
 for(int repeat=0;repeat<8;++repeat){
  bool undo=repeat%2==0?undo_gui_document(s):undo_editor_scene(s);REQUIRE(undo);
  REQUIRE(s.sceneDocument.revision().content==wb&&s.guiDocument->revision().content==gb);
  REQUIRE(s.sceneDocument.metrics().applied_transactions==wc&&s.guiDocument->metrics().applied_operations==gc);
  REQUIRE(s.sceneDocument.metrics().history_transactions==wc+1&&s.guiDocument->metrics().retained_operations==gc+1);
  REQUIRE(s.entities[0].position[0]==f.initial.x/100.0f-3.2f&&s.selectedEntityId==7&&address==s.guiDocument.get());
  const auto sequence_before_sync=s.guiDocument->revision().sequence;
  REQUIRE(synchronize_gui_document(s));REQUIRE(s.guiDocument->revision().sequence==sequence_before_sync);

  bool redo=repeat%2==0?redo_editor_scene(s):redo_gui_document(s);REQUIRE(redo);
  REQUIRE(s.sceneDocument.revision().content==wa&&s.guiDocument->revision().content==ga);
 }
 return 0;
}
int stale_and_redo(){
 Fixture f;auto& s=f.state;REQUIRE(commit_gui_layout_edit(s,f.button,f.changed()));
 REQUIRE(s.guiDocument->update_content(f.button,ButtonContent{"Later"}));
 auto wb=s.sceneDocument.revision();auto gb=s.guiDocument->revision();
 const auto blocked=try_grouped_gui_world_history(s,{1},Direction::undo);
 REQUIRE(blocked&&!*blocked&&!s.guiDocumentStatus.empty());
 REQUIRE(wb==s.sceneDocument.revision()&&gb==s.guiDocument->revision());
 REQUIRE(s.guiDocument->undo());
 const auto undone=try_grouped_gui_world_history(s,{2},Direction::undo);REQUIRE(undone&&*undone);
 REQUIRE(commit_gui_layout_edit(s,f.button,f.changed(80)));
 REQUIRE(!try_grouped_gui_world_history(s,{1},Direction::redo));
 REQUIRE(!try_grouped_gui_world_history(s,{2},Direction::redo));
 // A later World transaction also blocks half a GUI Undo, until explicitly undone.
 auto handle=s.sceneDocument.find(authoring::scene::ObjectId{8});REQUIRE(handle);
 auto object=s.sceneDocument.object(*handle);REQUIRE(object);auto transform=object->descriptor.transform;transform.position.x=3;
 const std::array<authoring::scene::SceneOperation,1> operations{
  authoring::scene::ObjectTransformChangedOperation{.object=*handle,.value=transform}};
 REQUIRE(s.sceneDocument.apply_transaction(operations,"Move cube"));
 wb=s.sceneDocument.revision();gb=s.guiDocument->revision();
 const auto blocked_gui=try_grouped_gui_world_history(s,{2},Direction::undo);
 REQUIRE(blocked_gui&&!*blocked_gui&&wb==s.sceneDocument.revision()&&gb==s.guiDocument->revision());
 REQUIRE(s.sceneDocument.undo());const auto restored=try_grouped_gui_world_history(s,{2},Direction::undo);REQUIRE(restored&&*restored);
 return 0;
}
int failure_preserves(){
 Fixture f;auto& s=f.state;
 auto wb=s.sceneDocument.revision();auto gb=s.guiDocument->revision();
 auto invalid=f.changed();invalid.x=std::numeric_limits<float>::quiet_NaN();
 REQUIRE(!commit_gui_layout_edit(s,f.button,invalid));REQUIRE(wb==s.sceneDocument.revision()&&gb==s.guiDocument->revision());
 s.entities[1].sceneObjectId=99; // Stale disposable projection, after staging both authoritative edits.
 REQUIRE(!commit_gui_layout_edit(s,f.button,f.changed()));REQUIRE(wb==s.sceneDocument.revision()&&gb==s.guiDocument->revision());
 s.entities[1].sceneObjectId=8;REQUIRE(commit_gui_layout_edit(s,f.button,f.changed()));
 wb=s.sceneDocument.revision();gb=s.guiDocument->revision();s.entities[1].sceneObjectId=99;
 const auto rejected=try_grouped_gui_world_history(s,{2},Direction::undo);
 REQUIRE(rejected&&!*rejected&&wb==s.sceneDocument.revision()&&gb==s.guiDocument->revision());
 s.entities[1].sceneObjectId=8;const auto accepted=try_grouped_gui_world_history(s,{2},Direction::undo);REQUIRE(accepted&&*accepted);
 return 0;
}
int identity_acceptance(){
 Fixture f;auto& s=f.state;REQUIRE(commit_gui_layout_edit(s,f.button,f.changed()));
 auto states=gui_world_group_states(s.sceneDocument,*s.guiDocument);
 auto foreign=states;foreign[1].identity[3]++;REQUIRE(s.authoringHistory.plan_group({1},Direction::undo,foreign).code==GroupCode::stale);
 foreign=states;foreign[1].content[0]++;REQUIRE(s.authoringHistory.plan_group({1},Direction::undo,foreign).code==GroupCode::stale);
 auto plan=s.authoringHistory.plan_group({2},Direction::undo,states);REQUIRE(plan.code==GroupCode::ready);
 auto world=s.sceneDocument;auto gui=*s.guiDocument;REQUIRE(world.undo()&&gui.undo());
 auto after=gui_world_group_states(world,gui);
 auto tampered=after;tampered[1].content[0]++;
 REQUIRE(s.authoringHistory.accept_group(plan,states,tampered)==GroupCode::stale);
 auto forged=plan;forged.changes[1].before.content=tampered[1].content;
 REQUIRE(s.authoringHistory.accept_group(forged,states,tampered)==GroupCode::stale);
 auto stale=plan;auto controller=s.authoringHistory;
 std::array<BranchSnapshot,1> ordinary{{BranchSnapshot{.id={1},.generation=1,.revision=1,.label="World",.available=true,.writable=true}}};
 REQUIRE(controller.synchronize(ordinary));REQUIRE(controller.accept_group(stale,states,after)==GroupCode::stale);
 REQUIRE(s.authoringHistory.accept_group(plan,states,after)==GroupCode::ready);
 return 0;
}
int bounded_retention(){
 Fixture f;auto& s=f.state;
 for(std::size_t i=0;i<Controller::maximum_groups;++i){
  const auto base=100u+2u*i;
  GroupState a{.branch={base},.identity={base,1,1,1},.content={1,1,1,1},.revision=1};
  GroupState b{.branch={base+1},.identity={base+1,1,1,1},.content={1,1,1,1},.revision=1};
  auto aa=a;auto bb=b;for(auto* after:{&aa,&bb}){after->revision++;after->cursor=1;after->entries={1};after->content[0]++;}
  std::array<GroupChange,2> change{{{a,aa},{b,bb}}};REQUIRE(s.authoringHistory.register_group(change,"Bounded fixture")==GroupCode::ready);
 }
 const auto wb=s.sceneDocument.revision();const auto gb=s.guiDocument->revision();
 REQUIRE(!commit_gui_layout_edit(s,f.button,f.changed()));REQUIRE(wb==s.sceneDocument.revision()&&gb==s.guiDocument->revision());
 REQUIRE(s.guiDocumentStatus.find("full")!=std::string::npos);
 return 0;
}
int main(){
 REQUIRE(editor_authoring_history::run_contract()==editor_authoring_history::ContractFailure::none);
 for(auto test:{layout_roundtrip,stale_and_redo,failure_preserves,identity_acceptance,bounded_retention})if(const int result=test())return result;
 REQUIRE(fallback_projection_calls==0);
 std::puts("grouped_layout_contract=PASS; paired_journals, repeat, stale, redo, identity, staged_failure, capacity");return 0;
}

'''

def fixture():
    docs=(ROOT/"Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8-sig")
    transactions=(ROOT/"Engine/src/editor/application/editor.application_scene_transactions.inl").read_text(encoding="utf-8-sig")
    functions="\n".join(definition(docs,marker) for marker in (
        "        [[nodiscard]] bool is_gui_authoring_entity(",
        "        [[nodiscard]] authoring::gui::WidgetDescriptor\n        gui_widget_descriptor_from_entity(",
        "        [[nodiscard]] bool gui_layout_matches_scene_projection(",
        "        [[nodiscard]] authoring::gui::GuiDocument*\n        synchronize_gui_document(",
        "        [[nodiscard]] std::array<editor_authoring_history::GroupState, 2>\n        gui_world_group_states(",
        "        [[nodiscard]] bool prepare_gui_world_projection(",
        "        void publish_gui_world_history(",
        "        [[nodiscard]] bool commit_gui_layout_edit(",
        "        [[nodiscard]] std::optional<bool> try_grouped_gui_world_history(",
        "        [[nodiscard]] bool undo_gui_document(",
        "        [[nodiscard]] bool redo_gui_document("))
    functions+="\n"+"\n".join(definition(transactions,marker) for marker in (
        "        [[nodiscard]] bool undo_editor_scene(",
        "        [[nodiscard]] bool redo_editor_scene("))
    return FIXTURE.replace("__ACTUAL_OWNER_FUNCTIONS__",functions)

class GroupedGuiWorldHistory(unittest.TestCase):
    def test_layout_controls_reach_the_grouped_owner(self):
        app=ROOT/"Engine/src/editor/application"
        canvas=(app/"editor.application_gui_authoring.inl").read_text(encoding="utf-8")
        release=canvas[canvas.index("                    if (drag.released)"):canvas.index("                    if (!state.guiCanvasWidgetDrag.pending")]
        self.assertIn("commit_gui_layout_edit(",release)
        self.assertNotIn("document.update_layout(",release)
        inspector=(app/"editor.frame_inspector.inl").read_text(encoding="utf-8")
        layout=inspector[inspector.index("                    if (layoutChanged)"):inspector.index('                    gui::label("Interaction")')]
        self.assertIn("commit_gui_layout_edit(",layout)
        outliner=(app/"editor.frame_outliner.inl").read_text(encoding="utf-8")
        flow=outliner[outliner.index('                            gui::label("Layout Flow")'):outliner.index('                            gui::label("Appearance")')]
        self.assertIn("commit_gui_layout_edit(",flow)

    def test_actual_layout_and_history_owners(self):
        proof=ROOT/"build/validation/gui-world-history-repair-oct27/actual"
        proof.mkdir(parents=True,exist_ok=True)
        (proof/"editor.grouped_history_contract.cpp").write_text(fixture(),encoding="utf-8")
        modules={}
        for path in (ROOT/"Engine/modules").rglob("*.ixx"):
            match=re.search(r"^export module\s+([\w.:]+);",path.read_text(encoding="utf-8-sig"),re.M)
            if match:modules[match.group(1)]=path
        pending=["authoring.gui_document","scene.document","editor.authoring_history","core.sha256"]
        closure=set()
        while pending:
            name=pending.pop()
            if name in closure:continue
            closure.add(name)
            pending+=re.findall(r"^(?:export\s+)?import\s+([\w.:]+);",modules[name].read_text(encoding="utf-8-sig"),re.M)
        files=" ".join('"'+modules[name].as_posix()+'"' for name in sorted(closure))
        (proof/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(GroupedGuiWorldHistory LANGUAGES CXX)
add_executable(grouped_history editor.grouped_history_contract.cpp
 "{(ROOT/"Engine/src/authoring/gui/authoring.gui_document_codec.cpp").as_posix()}"
 "{(ROOT/"Engine/src/authoring/gui/authoring.gui_templates.cpp").as_posix()}")
target_compile_features(grouped_history PRIVATE cxx_std_23)
target_sources(grouped_history PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/"Engine/modules").as_posix()}" FILES {files})
if(MSVC)
 target_compile_options(grouped_history PRIVATE /utf-8 /EHsc /permissive-)
endif()
enable_testing()
add_test(NAME grouped_layout_owners COMMAND grouped_history)
set_tests_properties(grouped_layout_owners PROPERTIES TIMEOUT 30)
''',encoding="utf-8")
        configure=["cmake","-S",str(proof),"-B",str(proof/"cmake")]
        configure += ["-G","Visual Studio 17 2022","-A","x64"] if os.name=="nt" else ["-G","Ninja","-DCMAKE_BUILD_TYPE=Release"]
        commands=[("configure",configure)]
        for config in (("Debug","Release") if os.name=="nt" else ("Release",)):
            build=["cmake","--build",str(proof/"cmake"),"--config",config,"--parallel","1"]
            if os.name=="nt":build += ["--","/nr:false","/p:UseMultiToolTask=false","/p:CL_MPCount=1"]
            commands += [(config+"-build",build),(config+"-contract",["ctest","--test-dir",str(proof/"cmake"),"-C",config,"--output-on-failure"])]
        receipt={"actual_modules":len(closure),"scope":"Actual layout/journal/coordinator and raw Undo/Redo owners; source catalog/template loading and renderer are recorded ports; no Engine/native claim","phases":[]}
        for label,command in commands:
            result=subprocess.run(command,cwd=ROOT,env=dict(os.environ,MSBUILDDISABLENODEREUSE="1"),capture_output=True,text=True,timeout=180)
            (proof/(label+".log")).write_text(result.stdout+result.stderr,encoding="utf-8")
            receipt["phases"].append({"phase":label,"exit":result.returncode,"command":command})
            (proof/"result.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
            self.assertEqual(result.returncode,0,(result.stdout+result.stderr)[-12000:])

if __name__=="__main__":unittest.main()
