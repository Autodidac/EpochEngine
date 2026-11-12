"""Real Tile Map journal and Editor global history registration/dispatch; no GUI launch."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/application/editor.application_document_editing.inl"


def fixture():
    source = OWNER.read_text(encoding="utf-8")
    preview = (ROOT / "Engine/src/editor/application/editor.application_scene_preview.inl").read_text(encoding="utf-8")
    enum = preview[preview.index("        enum class SurfaceEditAction"):] + source[:source.index("        struct SurfaceEditAvailability")]
    constants = source[source.index("        inline constexpr editor_authoring_history::BranchId"):
                       source.index("        [[nodiscard]] editor_authoring_history::BranchId code_history_branch")]
    markers = (
        "        [[nodiscard]] bool editor_project_viewport_input_reserved(",
        "        [[nodiscard]] constexpr std::string_view surface_edit_action_name(",
        "        [[nodiscard]] editor_workspace_commands::Surface workspace_command_surface(",
        "        [[nodiscard]] std::optional<editor_workspace_commands::CommandId>\n        command_for_surface_edit(",
        "        [[nodiscard]] std::optional<editor_workspace_commands::Surface>\n        selected_edit_surface(",
        "        [[nodiscard]] editor_workspace_commands::Surface\n        edit_surface_for_action(",
        "        [[nodiscard]] editor_authoring_history::BranchId code_history_branch(",
        "        [[nodiscard]] std::string plant_history_label(",
        "        [[nodiscard]] std::optional<editor_authoring_history::BranchSnapshot>\n        tilemap_authoring_history_branch(",
        "        [[nodiscard]] std::vector<editor_authoring_history::BranchSnapshot>\n        authoring_history_branches(",
        "        [[nodiscard]] std::optional<editor_authoring_history::BranchId>\n        visible_authoring_history_branch(",
        "        void synchronize_authoring_history(",
        "        [[nodiscard]] std::optional<editor_code_workspace::DocumentSnapshot>\n        code_document_for_history(",
        "        void finish_authoring_history_transition(",
        "        [[nodiscard]] SurfaceEditAvailability resolve_surface_edit(",
        "        [[nodiscard]] bool execute_surface_edit(")
    functions = "\n".join(definition(source, marker) for marker in markers)
    availability = definition(source, "        struct SurfaceEditAvailability", True)
    return CODE.replace("__ENUM__", enum).replace("__CONSTANTS__", constants).replace("__AVAILABILITY__", availability).replace("__FUNCTIONS__", functions)


CODE = r'''
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
import authoring.tilemap;
import authoring.gui_document;
import scene.document;
import scene.snapshot;
import forest.factory;
import editor.application;
import editor.authoring_history;
import editor.workspace_commands;
import editor.code_workspace;
namespace epochengine { namespace core { struct Context; }
template<class...Args>std::string format_text(std::string_view text,Args&&...){return std::string(text);} }
namespace scene=epochengine::scene;
namespace authoring=epochengine::authoring;
namespace forest=epochengine::forest;
namespace editor_authoring_history=epochengine::editor_authoring_history;
namespace editor_workspace_commands=epochengine::editor_workspace_commands;
namespace editor_code_workspace=epochengine::editor_code_workspace;
namespace core=epochengine::core;
using EditorMainSurface=epochengine::EditorApplicationSurface;
void check(bool value,int line){if(!value){std::fprintf(stderr,"Tile Map global history contract failed line %d\n",line);std::exit(93);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),__LINE__)
// The unchanged controller boundary delegates to its real authoring document.
// Selection/preview normalization is outside this routing test.
struct TileMapPort {
    std::unique_ptr<authoring::tilemap::Document> value;
    std::string message;unsigned undos{},redos{};
    bool has_document()const{return value && value->valid();}
    const authoring::tilemap::Document* document()const{return value.get();}
    auto undo(){++undos;auto result=value->undo();message=result?"Tile Map undo committed":"No map edit";return result;}
    auto redo(){++redos;auto result=value->redo();message=result?"Tile Map redo committed":"No map edit";return result;}
    std::string_view status()const{return message;}
};
// Code text I/O is an unrelated port; its exact branch identity and selection
// still go through the production registration/coordinator/dispatcher below.
struct CodePort {
    editor_code_workspace::WorkspaceSnapshot value;unsigned undos{},redos{};
    auto snapshot()const{return value;}
    std::optional<editor_code_workspace::DocumentSnapshot> active_document()const{
        return value.documents.empty()?std::nullopt:std::optional{value.documents.front()};}
    editor_code_workspace::OperationResult change(bool undo){auto& doc=value.documents.front();++doc.revision;
        if(undo){++undos;--doc.history_cursor;}else{++redos;++doc.history_cursor;}
        return {.code=editor_code_workspace::ResultCode::success,.reason="Code history changed"};}
    auto undo(editor_code_workspace::DocumentHandle,const editor_code_workspace::WorkspaceAuthority&,std::uint64_t){return change(true);}
    auto redo(editor_code_workspace::DocumentHandle,const editor_code_workspace::WorkspaceAuthority&,std::uint64_t){return change(false);}
};
struct EditorState {
    struct {bool valid()const{return false;}}projectRuntimeProcess;std::optional<int>projectBuildPending;
    EditorMainSurface mainSurface=EditorMainSurface::Scene;bool game2DGuiWorkspace{};unsigned assetWorkspaceSection{};
    authoring::scene::SceneDocument sceneDocument;std::unique_ptr<authoring::gui::GuiDocument> guiDocument;
    std::unique_ptr<TileMapPort> tileMapWorkspace;std::string tileMapStatus;
    bool plantLabDocumentInitialized{};forest::ForestAssetDocument plantLabDocument;
    std::uint64_t plantLabCompiledRevision{},plantLabCompiledContentHash{};
    editor_authoring_history::Controller authoringHistory;
    struct {CodePort controller;editor_code_workspace::WorkspaceAuthority authority;
        std::optional<editor_code_workspace::DocumentHandle> draftHandle;std::string status;} projectCodeWorkspace;
    std::uint64_t selectedEntityId{};std::unordered_map<std::uint64_t,int> guiDocumentWidgets;
    std::vector<std::string> logs;
};
void push_editor_log(EditorState& state,const std::string& message){state.logs.push_back(message);}
std::optional<std::size_t> editor_entity_index(const EditorState&,std::uint64_t){return {};}
const int* selected_gui_widget(const EditorState&){return nullptr;}
void duplicate_selected_entity(EditorState&){CHECK(false);}void delete_selected_entity(EditorState&){CHECK(false);}
authoring::gui::GuiDocument* synchronize_gui_document(EditorState& state){return state.guiDocument.get();}
bool duplicate_selected_gui_widget(EditorState&,authoring::gui::GuiDocument&){CHECK(false);return false;}
bool remove_selected_gui_widget(EditorState&,authoring::gui::GuiDocument&){CHECK(false);return false;}
void handle_scene_tool(EditorState&,const core::Context*,std::string_view){CHECK(false);}
bool undo_editor_scene(EditorState& state){return bool(state.sceneDocument.undo());}
bool redo_editor_scene(EditorState& state){return bool(state.sceneDocument.redo());}
bool undo_gui_document(EditorState& state){return bool(state.guiDocument->undo());}
bool redo_gui_document(EditorState& state){return bool(state.guiDocument->redo());}
__ENUM__
__AVAILABILITY__
__CONSTANTS__
__FUNCTIONS__
void create_map(EditorState& state,unsigned generation=1){
    auto created=authoring::tilemap::Document::create({17,generation},{});CHECK(created);
    state.tileMapWorkspace=std::make_unique<TileMapPort>();state.tileMapWorkspace->value=std::move(created.document);
    CHECK(state.tileMapWorkspace->value->create_layer({.name="Ground"}));
}
void make_world(EditorState& state){
    scene::SceneSnapshot seed;seed.scene_id="history-world";seed.project_id="history-project";
    auto built=authoring::scene::SceneDocument::from_snapshot(seed);CHECK(built);state.sceneDocument=std::move(built.value);
    authoring::scene::SceneObjectDescriptor cube;cube.id={71};cube.metadata.name="Cube";
    const std::array<authoring::scene::SceneOperation,1> operations{authoring::scene::ObjectCreatedOperation{.object=cube}};
    CHECK(state.sceneDocument.apply_transaction(operations,"Add cube"));
}
int main(){
    using Kind=editor_authoring_history::BranchKind;using Direction=editor_authoring_history::Direction;
    using Command=editor_workspace_commands::CommandId;using Surface=editor_workspace_commands::Surface;
    CHECK(editor_workspace_commands::run_aggregate_contract()==editor_workspace_commands::ContractFailure::none);
    CHECK(editor_authoring_history::run_contract()==editor_authoring_history::ContractFailure::none);
    CHECK(static_cast<unsigned>(Surface::systems)==9 && static_cast<unsigned>(Surface::tilemap_canvas)==10);
    CHECK(editor_workspace_commands::find_command(Command::tilemap_undo)->surface==Surface::tilemap_canvas);
    EditorState state;make_world(state);create_map(state);state.mainSurface=EditorMainSurface::Game2D;
    CHECK(workspace_command_surface(state)==Surface::tilemap_canvas);
    auto mapBefore=state.tileMapWorkspace->value->revision();const auto worldBefore=state.sceneDocument.revision();
    CHECK(state.tileMapWorkspace->value->create_layer({.name="Overlay"}));
    auto mapAfter=state.tileMapWorkspace->value->revision();
    auto branch=tilemap_authoring_history_branch(state);CHECK(branch && branch->cursor==2 && branch->retained_entries==2);
    CHECK(branch->kind==Kind::tilemap_document && !branch->undo_label.empty());
    auto available=resolve_surface_edit(state,SurfaceEditAction::undo);
    CHECK(available.enabled && available.command==Command::tilemap_undo && available.history->branch==kTileMapHistoryBranch);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));
    CHECK(state.tileMapWorkspace->value->revision().content==mapBefore.content && state.sceneDocument.revision()==worldBefore);
    CHECK(state.tileMapWorkspace->undos==1 && state.tileMapStatus=="Tile Map undo committed");
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::redo));
    CHECK(state.tileMapWorkspace->value->revision().content==mapAfter.content && state.sceneDocument.revision()==worldBefore);
    CHECK(state.tileMapWorkspace->redos==1 && state.authoringHistory.active_branch()==kTileMapHistoryBranch);
    // Switching to a real visible World branch keeps the established selection policy.
    state.mainSurface=EditorMainSurface::Scene;
    CHECK(resolve_surface_edit(state,SurfaceEditAction::undo).command==Command::world_undo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));
    CHECK(state.sceneDocument.objects().empty() && state.tileMapWorkspace->value->revision().content==mapAfter.content);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::redo));
    // Switching back does not require changing map selection or recreating its controller.
    state.mainSurface=EditorMainSurface::Game2D;
    CHECK(resolve_surface_edit(state,SurfaceEditAction::undo).command==Command::tilemap_undo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));
    CHECK(state.tileMapWorkspace->value->create_layer({.name="Replacement"}));
    CHECK(!state.tileMapWorkspace->value->can_redo());
    CHECK(!resolve_surface_edit(state,SurfaceEditAction::redo).enabled); // no unrelated redo exists
    CHECK(state.sceneDocument.objects().size()==1);
    // The existing GUI and Plant branches still take priority when visible.
    state.guiDocument=std::make_unique<authoring::gui::GuiDocument>(authoring::gui::DocumentHandle{3,1},authoring::gui::BranchIdentity{1,1});
    CHECK(state.guiDocument->create_widget({.kind=authoring::gui::WidgetKind::canvas,.name="Canvas",.content=authoring::gui::CanvasContent{}}));
    state.game2DGuiWorkspace=true;
    CHECK(resolve_surface_edit(state,SurfaceEditAction::undo).command==Command::gui_undo);
    const auto tileRevision=state.tileMapWorkspace->value->revision();
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));CHECK(state.tileMapWorkspace->value->revision()==tileRevision);
    state.plantLabDocument=forest::make_default_plant_lab_document();state.plantLabDocumentInitialized=true;
    const auto height=forest::forest_profile_value(state.plantLabDocument.profile,forest::ForestProfileProperty::TargetHeightMeters);
    CHECK(forest::apply_forest_profile_edit(state.plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,height+1));
    state.mainSurface=EditorMainSurface::PlantLab;
    CHECK(resolve_surface_edit(state,SurfaceEditAction::undo).command==Command::plant_undo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));CHECK(state.tileMapWorkspace->value->revision()==tileRevision);
    CHECK(forest::apply_forest_material_edit(state.plantLabDocument,forest::ForestTextureSlot::bark,
        {"Assets/birch.png",{1,2,3,4}}));
    auto textureUndo=resolve_surface_edit(state,SurfaceEditAction::undo);CHECK(textureUndo.enabled&&textureUndo.command==Command::plant_undo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));CHECK(state.plantLabDocument.profile.barkTexture.empty());
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::redo));CHECK(state.plantLabDocument.profile.barkTexture.artifactKey[3]==4);
    state.projectCodeWorkspace.controller.value.configured=true;
    editor_code_workspace::DocumentSnapshot code;code.handle={0,1};code.revision=1;code.history_entries=1;code.history_cursor=1;
    code.writable=true;code.label="Script";state.projectCodeWorkspace.controller.value.documents.push_back(code);
    state.mainSurface=EditorMainSurface::Assets;state.assetWorkspaceSection=2;
    CHECK(resolve_surface_edit(state,SurfaceEditAction::undo).command==Command::code_undo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));CHECK(state.projectCodeWorkspace.controller.undos==1);
    CHECK(state.tileMapWorkspace->value->revision()==tileRevision);
    // A replaced map generation invalidates an older plan without mutating it.
    state.mainSurface=EditorMainSurface::Game2D;state.game2DGuiWorkspace=false;
    available=resolve_surface_edit(state,SurfaceEditAction::undo);CHECK(available.history);
    const auto oldPlan=*available.history;create_map(state,2);synchronize_authoring_history(state);
    const auto newBranch=tilemap_authoring_history_branch(state);CHECK(newBranch && newBranch->generation!=oldPlan.branch_generation);
    CHECK(state.authoringHistory.accept(oldPlan,*newBranch)!=editor_authoring_history::PlanCode::ready);
    // Explicit local map undo updates the same shared branch on its next sync.
    CHECK(state.tileMapWorkspace->value->create_layer({.name="Local"}));
    CHECK(state.tileMapWorkspace->undo());synchronize_authoring_history(state);
    CHECK(resolve_surface_edit(state,SurfaceEditAction::redo).command==Command::tilemap_redo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::redo));
    // A non-document workspace retains the selected/latest branch policy.
    state.mainSurface=EditorMainSurface::Project;
    CHECK(resolve_surface_edit(state,SurfaceEditAction::undo).command==Command::tilemap_undo);
    CHECK(execute_surface_edit(state,nullptr,SurfaceEditAction::undo));
    state.tileMapWorkspace.reset();synchronize_authoring_history(state);
    CHECK(!tilemap_authoring_history_branch(state));
    CHECK(state.authoringHistory.branch(kTileMapHistoryBranch)==std::nullopt);
}
'''


class TileMapGlobalHistory(unittest.TestCase):
    def test_actual_journal_global_routing_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 CPU module consumer")
        proof = ROOT / "Artifacts/Reports/tilemap-global-history-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/tilemap-global-history"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.tilemap_history_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        available = {}
        for directory in (ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"):
            for path in directory.rglob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    available[match.group(1)] = path
        pending = ["authoring.tilemap", "authoring.gui_document", "scene.document", "scene.snapshot", "forest.factory",
                   "editor.application", "editor.authoring_history", "editor.workspace_commands", "editor.code_workspace", "core.sha256"]
        selected = {}
        while pending:
            name = pending.pop()
            if name in selected:
                continue
            self.assertIn(name, available)
            selected[name] = available[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", selected[name].read_text(encoding="utf-8-sig"), re.M)
        modules = " ".join('"'+path.as_posix()+'"' for path in selected.values())
        implementations = ("Engine/src/authoring/tilemap/authoring.tilemap_document.cpp",
                           "Engine/src/assets/tilemap/asset.tilemap_artifact.cpp",
                           "Engine/src/authoring/gui/authoring.gui_document_codec.cpp",
                           "Engine/src/authoring/gui/authoring.gui_templates.cpp")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(TileMapGlobalHistory LANGUAGES CXX)
add_executable(tilemap_global_history "{source.as_posix()}" {" ".join('"'+(ROOT/name).as_posix()+'"' for name in implementations)})
target_compile_features(tilemap_global_history PRIVATE cxx_std_23)
target_compile_options(tilemap_global_history PRIVATE /W3 /WX /utf-8)
target_compile_definitions(tilemap_global_history PRIVATE EPOCH_ENABLE_AUTHORING_PLATFORM=1 EPOCH_ENABLE_TILEMAP_EDITOR=1)
target_include_directories(tilemap_global_history PRIVATE "{(ROOT/'Engine/include').as_posix()}")
target_sources(tilemap_global_history PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {modules})
enable_testing()
add_test(NAME actual_tilemap_global_history COMMAND tilemap_global_history)
set_tests_properties(actual_tilemap_global_history PROPERTIES TIMEOUT 20)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for configuration in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", configuration, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", configuration, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "modules": len(selected), "commands": results,
            "scope": "Actual Editor registration/selection/resolution/execution function bodies; real Tile Map/World/GUI/Plant documents and shared coordinator/catalog. Unchanged TileMap controller boundary and code text I/O are ports; no renderer or filesystem project execution.",
            "sourceIdentity": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in [OWNER, *selected.values(), *(ROOT/name for name in implementations)]},
            "cases": ["visible map Undo/Redo changes map, not World", "same journal local/global transitions and redo invalidation",
                      "World/GUI/Plant/code visible branches preserve priority", "non-document view preserves latest branch",
                      "replacement generation and closed map refuse stale branch", "bounded journal metadata without deep history copying"],
            "limits": "Does not complete grouped cross-document transactions, persisted Undo history or native menu/shortcut acceptance.",
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
