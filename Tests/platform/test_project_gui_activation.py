"""Compose real Save/Run orchestration with real GUI codec/library publication.

The accepted module/library closure is reused without rebuilding Engine or
starting a renderer. Project selection and scene UI state are small substitutes;
the owning activation check, save implementation and shell-repair control flow
are compiled verbatim. All files remain in a checkout-local fixture.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class ProjectGuiActivation(unittest.TestCase):
    def test_actual_build_cancel_dispatch_retains_distinct_disabled_slots(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU consumer")
        viewport = (ROOT / "Engine/src/editor/application/editor.frame_viewport.inl").read_text(encoding="utf-8")
        start = viewport.index("                    const bool projectBuildWasPending =")
        stop = viewport.index("                    const bool runtimeActive =", start)
        action = viewport[start:stop]
        api = (ROOT / "Engine/modules/gui.engine.ixx").read_text(encoding="utf-8")
        structures = "\n".join(definition(api, marker, True).replace("export ", "", 1) for marker in (
            "    export struct InlineButtonSpec", "    export enum class InlineButtonSizing",
            "    export enum class InlineButtonAlignment", "    export struct InlineButtonRowOptions"))
        body = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
struct EditorState{bool projectBuildPending{};unsigned builds{},cancels{};};
namespace gui{
''' + structures + r'''
inline std::optional<std::size_t> forced;
inline std::array<InlineButtonSpec,2> last;
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> items,float height,float gap,const InlineButtonRowOptions& options){
assert(items.size()==2&&height==30&&gap==6&&options.wrap);
assert(options.alignment==InlineButtonAlignment::Center);
std::copy(items.begin(),items.end(),last.begin());return forced;
}
}
void start_project_build(EditorState& state,bool,std::string_view){++state.builds;state.projectBuildPending=true;}
bool cancel_project_build(EditorState& state){++state.cancels;state.projectBuildPending=false;return true;}
void actual_controls(EditorState& editor,float centerWidth){
''' + action + r'''
}
int main(){
for(const float width:{180.f,700.f}){
    EditorState state;
    gui::forced.reset();actual_controls(state,width);
    assert(gui::last[0].label=="Build Active Project"&&gui::last[1].label=="Cancel Project Build");
    assert(gui::last[0].enabled&&!gui::last[1].enabled);
    const auto idle=gui::last;
    state.projectBuildPending=true;actual_controls(state,width);
    assert(!gui::last[0].enabled&&gui::last[1].enabled);
    for(std::size_t index=0;index<2;++index){
        assert(gui::last[index].label==idle[index].label);
        assert(gui::last[index].width==idle[index].width);
    }
    // Completion happens after the user targeted Cancel. Even a stale returned
    // index cannot dispatch Build or Cancel after that slot becomes disabled.
    state.projectBuildPending=false;gui::forced=1;actual_controls(state,width);
    assert(state.builds==0&&state.cancels==0);
    state.projectBuildPending=true;gui::forced=0;actual_controls(state,width);
    assert(state.builds==0&&state.cancels==0);
    gui::forced=1;actual_controls(state,width);assert(state.cancels==1&&!state.projectBuildPending);
    gui::forced=0;actual_controls(state,width);assert(state.builds==1&&state.projectBuildPending);
}
}
'''
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix="project-build-actions-", dir=parent))
        try:
            source, binary = scratch / "contract.cpp", scratch / "contract.exe"
            source.write_text(body, encoding="utf-8")
            tool = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))[-1]
            sdk = Path("C:/Program Files (x86)/Windows Kits/10")
            kit = sorted((sdk / "Include").glob("*"))[-1]
            result = subprocess.run([str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/EHsc",
                "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/Fo" + str(scratch / "contract.obj"),
                "/Fe" + str(binary), str(source), "/link", "/LIBPATH:" + str(tool / "lib/x64"),
                "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")],
                cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)

    def test_actual_project_save_run_does_not_activate_an_absent_gui(self):
        prepared = ROOT / "build/validation/generated-project-application-oct26"
        if os.name != "nt" or not (prepared / "generator-cl.rsp").is_file():
            self.skipTest("Accepted Windows module/library closure unavailable")
        documents = (ROOT / "Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8")
        packages = (ROOT / "Engine/src/editor/application/editor.application_packages_creation.inl").read_text(encoding="utf-8")
        runtime = (ROOT / "Engine/src/editor/application/editor.frame_project_runtime.inl").read_text(encoding="utf-8")
        scene_source = (ROOT / "Engine/src/editor/application/editor.application_runtime_source.inl").read_text(encoding="utf-8")
        selection = (ROOT / "Engine/src/editor/application/editor.application_scene_selection.inl").read_text(encoding="utf-8")
        selected = "\n".join(definition(documents, marker) for marker in (
            "        [[nodiscard]] bool is_gui_authoring_entity(",
            "        [[nodiscard]] bool project_gui_document_is_active(",
            "        [[nodiscard]] std::filesystem::path project_gui_document_path(",
            "        [[nodiscard]] std::optional<std::vector<std::byte>>\n        read_project_gui_document_bytes("))
        widget_identity = "\n".join(definition(documents, marker) for marker in (
            "        [[nodiscard]] EditorState::GuiWidgetIdentity gui_widget_identity(",
            "        [[nodiscard]] bool gui_widget_identity_matches("))
        save = definition(documents, "        [[nodiscard]] bool save_project_gui_document(")
        repair = definition(packages, "        void repair_project_evidence(")
        scene_save = definition(scene_source, "        [[nodiscard]] bool save_editor_scene_snapshot(")
        scene_projection = definition(selection, "        [[nodiscard]] scene::SceneSnapshot capture_editor_scene_projection(")
        run_start = runtime.index("            (void)project_gui_document_layout(editor);")
        run_end = runtime.index("            const auto dependency=project_planetary::", run_start)
        run_save = runtime[run_start:run_end]
        prefix = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <crtdbg.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "core.format_text.hpp"
#include "@GOAL_HEADER@"
import authoring.gui_document;
import authoring.gui_compiler;
import project.gui_library;
import platform.filesystem;
import scene.document;
import scene.snapshot;
import scene.persistence;
#undef NDEBUG
#include <cassert>
namespace ui {
namespace authoring=epochengine::authoring;
namespace project_gui=epochengine::project_gui;
namespace platform=epochengine::platform;
namespace scene=epochengine::scene;
namespace fs=std::filesystem;
authoring::scene::SceneDocument seed_scene(std::size_t count){
    scene::SceneSnapshot snapshot{.scene_id="fixture",.project_id="fixture",.world_name="World"};
    for(std::size_t i=0;i<count;++i){
        scene::SceneObjectSnapshot object;
        object.id=i+1;object.name="Original"+std::to_string(i);object.type="StaticMesh";
        object.category="World";object.scale={1,1,1};snapshot.objects.push_back(object);
    }
    auto built=authoring::scene::SceneDocument::from_snapshot(snapshot);assert(built);
    return std::move(built.value);
}
struct EditorEntity { std::string type="Cube"; };
struct Lifecycle {
 struct {struct {struct {std::uint64_t generation{1};}selected_project;}generations;} data;
 const auto& evidence()const{return data;}
 bool record_shell_materialized(){return true;} bool record_scene_commit(std::uint64_t){return true;}
};
struct EditorState {
    std::string projectId="fixture",projectRoot,guiDocumentStatus,guiDocumentSourcePath,
        projectStatus,projectBuildStatus,aiContinuousBuildFingerprint,aiContinuousBuildStatus,projectRunBackend,projectScenePath="worlds/main.epoch";
    authoring::scene::SceneDocument sceneDocument=seed_scene(6),diskScene=seed_scene(6);
    std::uint64_t sceneDocumentRevision=1;
    bool sceneInteractionDirty{},sceneLightingDirty{};
    int applicationKind{};
    struct{std::uint64_t frame_index{};double simulated_seconds{};}timeSnapshot;
    std::unique_ptr<authoring::gui::GuiDocument> guiDocument;
    authoring::gui::WidgetHandle guiDocumentRoot{};
    std::map<std::uint64_t,authoring::gui::WidgetHandle> guiDocumentWidgets;
    std::vector<unsigned> guiProjectionTombstones;
    struct GuiWidgetIdentity {
        authoring::gui::DocumentHandle document{};
        authoring::gui::BranchIdentity branch{};
        authoring::gui::WidgetHandle widget{};
        std::string projectId,projectRoot,sourcePath;
    } guiSelectedWidget,guiContentEditorWidget;
    struct {float zoom{};struct {float x{},y{};}pan;bool panMode{};
        struct {bool active{};}drag;struct {float x{},y{};}panOrigin;}guiCanvasView;
    std::size_t guiDocumentTab{};
    std::string guiContentPrimary,guiContentSecondary,guiActionEditor,guiTooltipEditor;
    std::uint64_t guiDocumentSavedRevision{};
    bool guiDocumentLoadAttempted{};
    std::vector<EditorEntity> entities=std::vector<EditorEntity>(6);
    std::size_t selectedEntity{};
    std::uint64_t selectedEntityId{};
    std::vector<std::uint64_t> selectedEntityIds;
    int previewMode{},workspaceTab{},projectWorkspaceSection=3,dockStatusTab{},mainSurface{},canvas2dProject{},projectCameraMode{},inputProfilePreset{};
    int outlinerToolTab{},inspectorToolTab{},viewportCameraMode{},aiAuthoringSurface{};
    bool game2DGuiWorkspace{},workspaceCompanionsPending{},aiAuthoringSurfaceRestoreQueued{};
    bool forestTerrainPainting{};
    bool showOutliner{},showInspector{},showConsoleDock{},showAiChat{},helpersVisible{};
    double projectRunFrameLimitFps{};
    int projectFrameLimitEdit{};
    int planetarySettings{},planetaryAttachment{},planetaryMeshPolicy{},planetaryBodySelection{};
    std::string planetarySeedDraft,planetaryRadiusDraft,planetaryCheckedRoot;
    double planetaryRadiusSynced{};
    std::uint32_t planetaryRadiusReferenceSynced{};
    bool planetaryInitialFramePending{};
    std::uint64_t planetaryInitialFrameGeneration{1};
    epochengine::editor::ai_chat_goal::ToolSequence aiGoalTools;
    bool aiGoalActive{},aiGoalRunning{},aiGoalPlanNextQueued{};
    std::size_t aiGoalCompletedMilestones{};
    std::string aiGoal,aiGoalDraft,aiGoalLastMilestoneReceipt;
    Lifecycle projectLifecycle;
};
struct Profile { std::string gui_path; };
inline Profile profile;
const Profile* editor_find_project_profile(std::string_view){return &profile;}
fs::path resolve_editor_path(const fs::path& path){return fs::absolute(path).lexically_normal();}
std::string display_project_path(const fs::path& path){return path.generic_string();}
void push_editor_log(EditorState&,std::string_view){}
bool persist_project_run_settings(EditorState&){return true;}
inline int documentInitializations{};
authoring::gui::GuiDocument* synchronize_gui_document(EditorState& state){
    if(!state.guiDocument||!state.guiDocument->valid()){
        ++documentInitializations;
        auto starter=authoring::gui::make_template_document(authoring::gui::TemplatePreset::game_hud);
        assert(starter);state.guiDocument=std::make_unique<authoring::gui::GuiDocument>(std::move(*starter));
        state.guiDocumentRoot=state.guiDocument->roots().front();
    }
    return state.guiDocument.get();
}
constexpr std::uint64_t kMaximumProjectGuiDocumentBytes=64ull*1024ull*1024ull;
namespace input { void set_active_profile(int){} }
bool project_gui_document_layout(EditorState&){return true;}
struct ApplicationProfile{bool allow_project_run=false;};
ApplicationProfile editor_application_profile(int){return {};}
fs::path active_editor_scene_path(const EditorState& state){return fs::path{state.projectRoot}/state.projectScenePath;}
void synchronize_editor_selection(EditorState&){}
scene::SceneSnapshot capture_editor_entity_projection(const EditorState&){assert(false);return {};}
struct Ensure { bool succeeded=true;std::string project_id,summary="materialized"; };
Ensure editor_ensure_project_shell(std::string_view id){return {true,std::string{id},"materialized"};}
void set_project(EditorState& state,std::string_view id,bool){
    state.projectId=id;state.projectWorkspaceSection=0;
    ++state.projectLifecycle.data.generations.selected_project.generation;
    state.planetaryInitialFramePending=true;
    state.planetaryInitialFrameGeneration=state.projectLifecycle.data.generations.selected_project.generation;
    state.aiGoalTools={};state.aiGoalActive=state.aiGoalRunning=state.aiGoalPlanNextQueued=false;
    state.aiGoalCompletedMilestones=0;state.aiGoal.clear();state.aiGoalDraft.clear();state.aiGoalLastMilestoneReceipt.clear();
    state.planetarySettings=state.planetaryAttachment=state.planetaryMeshPolicy=state.planetaryBodySelection=0;
    state.planetarySeedDraft.clear();state.planetaryRadiusDraft.clear();state.planetaryCheckedRoot.clear();
    state.planetaryRadiusSynced=0;state.planetaryRadiusReferenceSynced=0;
    // Real set_project applies its application profile then resets project-bound
    // GUI state. A same-project Save must not turn that into a view change.
    state.game2DGuiWorkspace=false;state.workspaceCompanionsPending=true;
    state.outlinerToolTab=0;state.viewportCameraMode=0;
    state.aiAuthoringSurface=0;state.aiAuthoringSurfaceRestoreQueued=false;
    state.showOutliner=state.showInspector=state.showConsoleDock=state.showAiChat=true;
    state.helpersVisible=true;
    state.guiDocumentTab=0;state.guiCanvasView={};
    state.guiSelectedWidget={};state.guiContentEditorWidget={};
    state.guiProjectionTombstones.clear();
    state.guiContentPrimary.clear();state.guiContentSecondary.clear();
    state.guiActionEditor.clear();state.guiTooltipEditor.clear();
    state.sceneDocument=state.diskScene;
    state.sceneDocumentRevision=state.sceneDocument.revision().sequence;
    state.entities.resize(state.sceneDocument.objects().size());
    state.guiDocument.reset();state.guiDocumentWidgets.clear();
    state.guiDocumentRoot={};state.guiDocumentSavedRevision=0;state.guiDocumentLoadAttempted=false;
}
void select_editor_entity(EditorState& state,std::size_t selected){
    state.selectedEntity=selected;state.selectedEntityId=selected+1;
    state.selectedEntityIds.assign(1,state.selectedEntityId);
    state.guiSelectedWidget={};state.guiContentEditorWidget={};
}
void normalize_editor_entity_ids(EditorState&){}
void append_project_note(EditorState&,std::string_view,std::string_view,std::string_view){}
'''
        prefix = prefix.replace("@GOAL_HEADER@", (ROOT / "Engine/src/editor/ai/editor.ai_chat_goal.hpp").as_posix())
        body = prefix + selected + widget_identity + save + scene_projection + scene_save + repair + r'''
void repair_active_project_evidence(EditorState& state){repair_project_evidence(state,state.projectId);}
void actual_run_save_preamble(EditorState& editor){
    // A real created shell already owns its worlds directory.
    fs::create_directories(fs::path{editor.projectRoot}/"worlds");
''' + run_save + r'''
}
}
int main(){
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    namespace fs=std::filesystem;
    using namespace ui;
    EditorState ordinary;ordinary.projectRoot=(fs::current_path()/"ordinary").generic_string();
    fs::create_directories(ordinary.projectRoot);
    for(int attempt=0;attempt<3;++attempt){
        actual_run_save_preamble(ordinary);
        assert(ordinary.entities.size()==6);
        assert(ordinary.projectWorkspaceSection==3);
        assert(!ordinary.guiDocument&&documentInitializations==0);
        assert(!fs::exists(fs::path{ordinary.projectRoot}/"Assets/Gui"));
        assert(!fs::exists(fs::path{ordinary.projectRoot}/"Library/Gui"));
    }
    // A real authored in-memory document remains active even in a game whose
    // original manifest did not declare a GUI. Preserve it across shell refresh.
    EditorState authored;authored.projectRoot=(fs::current_path()/"authored").generic_string();
    auto created=authoring::gui::make_template_document(authoring::gui::TemplatePreset::game_hud);
    assert(created);authored.guiDocument=std::make_unique<authoring::gui::GuiDocument>(std::move(*created));
    const auto guiRoot=authored.guiDocument->roots().front();
    auto interaction=authored.guiDocument->widget(guiRoot)->descriptor.interaction;
    interaction.tooltip="Retain history across Save";
    assert(authored.guiDocument->update_interaction(guiRoot,interaction));
    const auto guiHistory=authored.guiDocument->journal().size();
    assert(guiHistory>0&&authored.guiDocument->can_undo());
    const auto original=authored.guiDocument->snapshot();
    // Real load/synchronize binds the active project document before selection.
    authored.guiDocumentSourcePath=display_project_path(project_gui_document_path(authored));
    authored.guiSelectedWidget=gui_widget_identity(authored,guiRoot);
    authored.guiContentEditorWidget=authored.guiSelectedWidget;
    authored.guiDocumentTab=2;authored.guiCanvasView.zoom=1.75f;
    authored.guiCanvasView.pan={42.0f,-7.0f};authored.guiCanvasView.panMode=true;
    authored.guiCanvasView.drag.active=true;
    authored.guiContentPrimary="Unsaved inspector draft";
    authored.guiContentSecondary="Secondary";authored.guiActionEditor="scene.start";
    authored.guiTooltipEditor="Tooltip draft";authored.guiProjectionTombstones={7};
    authored.game2DGuiWorkspace=true;authored.outlinerToolTab=3;
    authored.inspectorToolTab=2;authored.viewportCameraMode=2;
    authored.aiAuthoringSurface=4;authored.aiAuthoringSurfaceRestoreQueued=true;
    authored.selectedEntity=2;authored.selectedEntityId=3;authored.selectedEntityIds={2,3,5};
    authored.planetarySettings=99;authored.planetaryAttachment=7;authored.planetaryMeshPolicy=8;authored.planetaryBodySelection=9;
    authored.planetarySeedDraft="987";authored.planetaryRadiusDraft="25000.";
    authored.planetaryCheckedRoot=authored.projectRoot;authored.planetaryRadiusSynced=25000;authored.planetaryRadiusReferenceSynced=2;
    for(int attempt=0;attempt<2;++attempt){
        actual_run_save_preamble(authored);
        assert(authored.entities.size()==6&&authored.projectWorkspaceSection==3);
        assert(!authored.planetaryInitialFramePending&&authored.planetaryInitialFrameGeneration==0);
        assert(authored.planetarySettings==99&&authored.planetaryAttachment==7&&authored.planetaryMeshPolicy==8&&authored.planetaryBodySelection==9);
        assert(authored.planetarySeedDraft=="987"&&authored.planetaryRadiusDraft=="25000.");
        assert(authored.planetaryCheckedRoot==authored.projectRoot&&authored.planetaryRadiusSynced==25000&&authored.planetaryRadiusReferenceSynced==2);
        assert(authored.guiDocument&&authored.guiDocument->snapshot()==original);
        assert(authored.guiDocument->journal().size()==guiHistory&&authored.guiDocument->can_undo());
        assert(authored.game2DGuiWorkspace&&authored.outlinerToolTab==3);
        assert(authored.inspectorToolTab==2&&authored.viewportCameraMode==2);
        assert(!authored.workspaceCompanionsPending);
        assert(!authored.showOutliner&&!authored.showInspector&&!authored.showConsoleDock&&!authored.showAiChat);
        assert(!authored.helpersVisible);
        assert(authored.aiAuthoringSurface==4&&authored.aiAuthoringSurfaceRestoreQueued);
        assert(authored.selectedEntity==2&&authored.selectedEntityId==3);
        assert((authored.selectedEntityIds==std::vector<std::uint64_t>{2,3,5}));
        assert(gui_widget_identity_matches(authored,authored.guiSelectedWidget,guiRoot));
        assert(gui_widget_identity_matches(authored,authored.guiContentEditorWidget,guiRoot));
        assert(authored.guiDocumentTab==2&&authored.guiCanvasView.zoom==1.75f);
        assert(authored.guiCanvasView.pan.x==42.0f&&authored.guiCanvasView.pan.y==-7.0f);
        assert(authored.guiCanvasView.panMode&&!authored.guiCanvasView.drag.active);
        assert(authored.guiContentPrimary=="Unsaved inspector draft"&&authored.guiContentSecondary=="Secondary");
        assert(authored.guiActionEditor=="scene.start"&&authored.guiTooltipEditor=="Tooltip draft");
        assert(authored.guiProjectionTombstones==std::vector<unsigned>{7});
        std::string failure;
        auto bytes=read_project_gui_document_bytes(project_gui_document_path(authored),failure);
        assert(bytes);auto decoded=authoring::gui::deserialize_gui_document(*bytes);
        assert(decoded&&*decoded.snapshot==original);
        const auto compiled=authoring::gui::compile_gui_document(original);assert(compiled);
        project_gui::ArtifactLibrary library{authored.projectId,authored.projectRoot};
        assert(library.load_exact(project_gui::canonical_source_path,compiled.artifact.identity.key));
    }
    assert(documentInitializations==0);
    // Changing project identity retains normal application-profile selection;
    // the previous project's document, view and widget binding must not leak.
    EditorState different;different.projectRoot=(fs::current_path()/"different").generic_string();
    fs::create_directories(fs::path{different.projectRoot}/"worlds");
    different.game2DGuiWorkspace=true;different.guiCanvasView.zoom=3;
    different.planetarySettings=99;different.planetarySeedDraft="987";
    repair_project_evidence(different,"another-project");
    assert(different.projectId=="another-project"&&!different.game2DGuiWorkspace);
    assert(different.planetarySettings==0&&different.planetarySeedDraft.empty());
    assert(different.workspaceCompanionsPending&&different.showInspector);
    assert(!different.guiDocument&&different.guiCanvasView.zoom==0);
    // The native model plan modified the canonical owner, not only its vector.
    // Save's shell refresh may reload an older five-object disk scene, but must
    // serialize the complete edited owner and retain its semantic Undo history.
    EditorState edited;edited.projectRoot=(fs::current_path()/"edited").generic_string();
    fs::create_directories(fs::path{edited.projectRoot}/"worlds");
    edited.diskScene=seed_scene(5);edited.sceneDocument=edited.diskScene;
    std::vector<authoring::scene::SceneOperation> creates;
    for(std::uint64_t i=0;i<12;++i){
        authoring::scene::SceneObjectDescriptor object;
        object.id={100+i};object.metadata.name="Added"+std::to_string(i);
        object.metadata.generic_type="StaticMesh";object.metadata.category="Gameplay";
        object.metadata.sort_order=static_cast<std::uint32_t>(5+i);
        object.transform.position={float(i%3)*3-3,0.5f,float(i/3)*3-4.5f};
        creates.push_back(authoring::scene::ObjectCreatedOperation{.object=object});
    }
    assert(edited.sceneDocument.apply_transaction(creates,"Approved twelve-cube plan"));
    edited.sceneDocumentRevision=edited.sceneDocument.revision().sequence;
    edited.entities.resize(17);
    const auto editedRevision=edited.sceneDocumentRevision;
    const auto editedHistory=edited.sceneDocument.metrics().applied_transactions;
    repair_active_project_evidence(edited);
    auto reopened=scene::persistence::load_scene_snapshot(active_editor_scene_path(edited),
        {.requirement=scene::SceneDocumentRequirement::authoring});
    assert(reopened.result&&reopened.snapshot.objects.size()==17);
    assert(reopened.snapshot.revision==editedRevision);
    for(std::size_t i=0;i<5;++i)assert(reopened.snapshot.objects[i].id==i+1);
    assert(edited.sceneDocument.metrics().applied_transactions==editedHistory);
    assert(edited.sceneDocument.undo()&&edited.sceneDocument.objects().size()==5);
    assert(edited.sceneDocument.redo()&&edited.sceneDocument.objects().size()==17);
    // An intentionally empty authored scene is also state; do not resurrect
    // project seeds merely because its disposable vector is empty.
    EditorState empty;empty.projectRoot=(fs::current_path()/"empty").generic_string();
    fs::create_directories(fs::path{empty.projectRoot}/"worlds");
    empty.sceneDocument=seed_scene(0);empty.entities.clear();
    repair_active_project_evidence(empty);
    reopened=scene::persistence::load_scene_snapshot(active_editor_scene_path(empty),
        {.requirement=scene::SceneDocumentRequirement::authoring});
    assert(reopened.result&&reopened.snapshot.objects.empty()&&empty.entities.empty());
    // A declared GUI and explicit Save GUI retain their existing initialization
    // behavior. Actual serialization and immutable artifact publication run.
    EditorState declared;declared.projectRoot=(fs::current_path()/"declared").generic_string();
    profile.gui_path=project_gui::canonical_source_path;
    assert(save_project_gui_document(declared,false));assert(documentInitializations==1);
    assert(fs::is_regular_file(project_gui_document_path(declared)));
    profile.gui_path.clear();
    EditorState explicitGui;explicitGui.projectRoot=(fs::current_path()/"explicit-gui").generic_string();
    assert(save_project_gui_document(explicitGui,true));assert(documentInitializations==2);
    assert(fs::is_regular_file(project_gui_document_path(explicitGui)));
    EditorState entityGui;entityGui.projectRoot=(fs::current_path()/"entity-gui").generic_string();
    entityGui.entities.front().type="GuiButton";
    assert(save_project_gui_document(entityGui,false));assert(documentInitializations==3);
    assert(fs::is_regular_file(project_gui_document_path(entityGui)));
}
'''
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix="project-gui-activation-", dir=parent))
        try:
            source, obj, binary = scratch / "contract.cpp", scratch / "contract.obj", scratch / "contract.exe"
            source.write_text(body, encoding="utf-8")
            compiler = (prepared / "generator-cl.rsp").read_text(encoding="utf-8")
            compiler = compiler.replace(str(prepared / "source/generator.cpp"), str(source))
            linker = (prepared / "generator-link.rsp").read_text(encoding="utf-8")
            linker = linker.replace(str(prepared / "generator.obj"), str(obj))
            # This retained compile closure predates the dependency-root move.
            # Rebind only that moved installation, leaving all module/library
            # identities in the accepted response files unchanged.
            former_dependencies = re.escape(str(ROOT / "Engine/vcpkg_installed"))
            dependencies = str(ROOT / "Artifacts/Dependencies/vcpkg_installed")
            compiler = re.sub(former_dependencies, lambda _: dependencies, compiler, flags=re.I)
            linker = re.sub(former_dependencies, lambda _: dependencies, linker, flags=re.I)
            for flag, value in (("Fo", obj), ("Fd", scratch / "compiler.pdb"), ("Fa", scratch / "compiler.asm")):
                compiler = re.sub(r"/" + flag + r'(?:"[^"]*"|\S+)', lambda _: f'/{flag}"{value}"', compiler, flags=re.I)
            for flag, value in (("OUT", binary), ("PDB", scratch / "contract.pdb"), ("IMPLIB", scratch / "contract.lib"),
                    ("MANIFESTFILE", scratch / "contract.manifest")):
                linker = re.sub(r"/" + flag + r':(?:"[^"]*"|\S+)', lambda _: f'/{flag}:"{value}"', linker, flags=re.I)
            self.assertIn(str(source), compiler)
            self.assertIn(str(obj), linker)
            (scratch / "cl.rsp").write_text(compiler, encoding="utf-8")
            (scratch / "link.rsp").write_text(linker, encoding="utf-8")
            tool = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))[-1]
            sdk = Path("C:/Program Files (x86)/Windows Kits/10")
            kit = sorted((sdk / "Include").glob("*"))[-1]
            environment = dict(os.environ)
            environment["INCLUDE"] = ";".join(map(str, (tool / "include", kit / "ucrt", kit / "shared", kit / "um", kit / "winrt")))
            environment["LIB"] = ";".join(map(str, (tool / "lib/x64", sdk / "Lib" / kit.name / "um/x64", sdk / "Lib" / kit.name / "ucrt/x64")))
            environment["PATH"] = str(tool / "bin/Hostx64/x64") + ";" + str(sdk / "bin" / kit.name / "x64") + ";" + environment.get("PATH", "")
            environment["TMP"] = environment["TEMP"] = str(scratch)
            for executable, response in (("cl.exe", "cl.rsp"), ("link.exe", "link.rsp")):
                result = subprocess.run([str(tool / "bin/Hostx64/x64" / executable), "@" + str(scratch / response)], cwd=ROOT,
                    env=environment, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-8000:] + result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == "__main__":
    unittest.main()
