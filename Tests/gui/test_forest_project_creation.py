"""Actual Plant intent and existing New Project confirmation; CPU-only ports."""
from pathlib import Path
import re
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui import test_forest_preview_actions as forest_fixture

ROOT = Path(__file__).resolve().parents[2]


class ForestProjectCreation(unittest.TestCase):
    def test_confirmation_binds_profile_and_project_and_cancel_is_inert(self):
        base = ROOT / "Engine/src/editor/application"
        state = (base / "editor.application_state.inl").read_text(encoding="utf-8")
        owner = (base / "editor.application_scene_preview.inl").read_text(encoding="utf-8")
        viewport = (base / "editor.frame_viewport.inl").read_text(encoding="utf-8")
        intent = definition(state, "            struct ForestProjectCreationIntent final", True)
        functions = "\n".join(definition(owner, marker) for marker in (
            "        [[nodiscard]] bool begin_forest_project_creation(",
            "        [[nodiscard]] std::optional<forest::ForestAssetDocument>\n        take_forest_project_creation(",
            "        [[nodiscard]] std::size_t finish_forest_project_creation("))
        def button_branch(marker):
            start = viewport.index(marker)
            # Button size uses a braced Vec2 inside the condition. The action
            # body starts after the closing condition, not at that initializer.
            body = re.search(r'\)\)\s*\n\s*\{', viewport[start:])
            self.assertIsNotNone(body)
            brace = start + body.end() - 1
            return viewport[start:brace] + definition(viewport[brace:], '{')

        create = button_branch('                    if (gui::button("Create Project",')
        region_prefix = "EPOCH_" + "PRIVATE_SOURCE_"
        create = re.sub(r"// " + region_prefix + r"BEGIN:.*?// " + region_prefix + r"END:[^\n]*",
                        "", create, flags=re.S)
        cancel = button_branch('                if (gui::button(editor.projectCreationOpen ? "Cancel New Project"')
        source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace forest {struct ForestAssetDocument {
 std::uint64_t revision{7},contentHash{42};std::string profile{"tall oak"};
 std::vector<int> journal{1,2,3};friend bool operator==(const ForestAssetDocument&,const ForestAssetDocument&)=default;
};}
namespace project_assets {std::uint64_t stable_project_identity(std::string_view id){return id=="new-project"?10u:11u;}}
struct EditorState {
 INTENT
 std::optional<ForestProjectCreationIntent> forestProjectCreationIntent;
 std::string projectId,projectRoot,projectManifest,projectStatus;
 struct {struct {bool project_selected{};struct {struct {std::uint64_t generation{3},project_key{};}selected_project;}generations;}value;
  const auto& evidence()const{return value;}}projectLifecycle;
 forest::ForestAssetDocument plantLabDocument;
 bool plantLabDocumentInitialized{true},projectCreationOpen{};std::size_t projectCreationKind{};
 int plantLabCompiled{};std::uint64_t plantLabCompiledRevision{},plantLabCompiledContentHash{};
 std::vector<int> forestPreviewEntities;std::uint64_t forestPreviewRevision{},forestPreviewContentHash{};
};
unsigned placementCalls{};forest::ForestAssetDocument placedDocument;
std::size_t place_forest_factory_asset(EditorState& s){++placementCalls;placedDocument=s.plantLabDocument;return 12;}
bool inspectedIdentityValid{true};unsigned identityInspections{};
struct Identity {bool succeeded;};
Identity editor_inspect_project_manifest_identity(std::string_view manifest,std::string_view id,std::string_view root){
 ++identityInspections;namespace fs=std::filesystem;
 return {inspectedIdentityValid&&id=="new-project"
  &&fs::path{manifest}.lexically_normal()==fs::path{"C:/owned/new-project/project.epoch.json"}
  &&fs::path{root}.lexically_normal()==fs::path{"C:/owned/new-project"}};}
FUNCTIONS
enum class EditorProjectKind {Game,Tool,Editor};enum class EditorProjectInputProvision {project_default};
enum class EditorMainSurface {Project,Scene};struct Context {};
namespace gui {struct Vec2 {float x{},y{};};bool clicked{};
 bool button(std::string_view,Vec2){return clicked;}}
void push_editor_log(EditorState&,std::string_view){}
struct Creation {bool succeeded;std::string summary,project_id,root_path,manifest_path;};
struct Admission {bool succeeded;std::string project_id,manifest_path,summary;};
void run(int fault){
 EditorState editor;placementCalls=identityInspections=0;inspectedIdentityValid=fault!=15;
 const auto authored=editor.plantLabDocument;
 assert(begin_forest_project_creation(editor)&&editor.projectCreationOpen&&editor.forestProjectCreationIntent);
 assert(placementCalls==0&&editor.plantLabDocument==authored);
 unsigned creates{},opens{},saves{},surfaces{},focuses{};
 const float centerWidth=900;auto ctx=std::make_shared<Context>();
 constexpr EditorProjectKind projectKinds[]{EditorProjectKind::Game,EditorProjectKind::Tool,EditorProjectKind::Editor};
 const std::size_t selectedKind=0;
 auto editor_create_project_shell=[&](EditorProjectKind kind,EditorProjectInputProvision){
  assert(kind==EditorProjectKind::Game);++creates;
  return Creation{fault!=7,"created","new-project",fault==12?"C:\\owned\\new-project":"C:/owned/new-project","C:/owned/new-project/project.epoch.json"};};
 auto editor_admit_project_manifest=[&](std::string_view){return Admission{fault!=8,
  fault==16?"foreign-project":"new-project","C:/owned/new-project/project.epoch.json","admitted"};};
 auto editor_open_project=[&](const auto&,std::string_view id){++opens;
  editor.projectId=id;editor.projectRoot=fault==9?"C:/wrong-project":"C:/owned/new-project";
  editor.projectManifest=fault==13?"C:/foreign/project.epoch.json":"C:/owned/new-project/project.epoch.json";
  editor.projectLifecycle.value.project_selected=true;
  editor.projectLifecycle.value.generations.selected_project.project_key=fault==17?11u:10u;
  if(fault!=14)++editor.projectLifecycle.value.generations.selected_project.generation;
  editor.plantLabDocument={};editor.plantLabDocument.profile="default instead of author input";
  editor.plantLabDocumentInitialized=false;editor.forestProjectCreationIntent.reset();return true;};
 auto save_editor_scene_snapshot=[&](EditorState&){++saves;return true;};
 auto apply_editor_surface=[&](EditorMainSurface surface,std::string_view){assert(surface==EditorMainSurface::Scene);++surfaces;};
 auto focus_forest_factory_placement=[&](const Context*,EditorState&){++focuses;return true;};
 if(fault==1)++editor.plantLabDocument.revision;
 if(fault==2)++editor.plantLabDocument.contentHash;
 if(fault==3)++editor.projectLifecycle.value.generations.selected_project.generation;
 if(fault==4)editor.projectRoot="C:/other-project";
 if(fault==5){editor.projectId="other";editor.projectLifecycle.value.project_selected=true;}
 if(fault==6)editor.projectCreationKind=1;
 gui::clicked=fault!=10;
 if(fault==11){CANCEL assert(!editor.projectCreationOpen&&!editor.forestProjectCreationIntent);}
 else {CREATE}
 // Native Windows separators are not portable path aliases. A Linux name
 // containing backslashes must not be treated as the admitted slash path.
 const bool nativePathAlias=std::filesystem::path{R"(C:\owned\new-project)"}.lexically_normal()
     ==std::filesystem::path{"C:/owned/new-project"}.lexically_normal();
 if(fault==0||(fault==12&&nativePathAlias)){assert(creates==1&&opens==1&&placementCalls==1&&saves==1&&surfaces==1&&focuses==1);
  assert(placedDocument==authored&&!editor.projectCreationOpen&&!editor.forestProjectCreationIntent&&identityInspections==1);}
 else if(fault>=1&&fault<=6){assert(creates==0&&opens==0&&placementCalls==0&&!editor.forestProjectCreationIntent);
  assert(editor.projectStatus.find("changed")!=std::string::npos);}
 else if(fault==10){assert(creates==0&&placementCalls==0&&editor.forestProjectCreationIntent);}
 else {assert(placementCalls==0&&saves==0&&surfaces==0&&focuses==0);}
 if(fault==7||fault==8||fault==10||fault==11)assert(editor.plantLabDocument==authored);
 if((fault==12&&!nativePathAlias)||fault==13||fault==14||fault==15||fault==17)assert(editor.projectStatus.find("could not be verified")!=std::string::npos);
 if(fault==16)assert(opens==0);
}
int main(){for(int fault=0;fault<=17;++fault)run(fault);}
'''
        for token, value in (("INTENT", intent), ("FUNCTIONS", functions),
                             ("CANCEL", cancel), ("CREATE", create)):
            source = source.replace(token, value)
        forest_fixture.ForestPreviewActions()._compile_and_run(source)


if __name__ == "__main__":
    unittest.main()
