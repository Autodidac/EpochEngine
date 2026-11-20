"""Independent real-module GUI hierarchy adapter consumer; no Engine/UI startup."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition


ROOT = Path(__file__).resolve().parents[2]


def actual_owner_fixture():
    pane = (ROOT / "Engine/src/editor/application/editor.application_pane_routing.inl").read_text(encoding="utf-8")
    docs = (ROOT / "Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8")
    state = (ROOT / "Engine/src/editor/application/editor.application_state.inl").read_text(encoding="utf-8")
    scene = (ROOT / "Engine/src/editor/application/editor.application_scene_selection.inl").read_text(encoding="utf-8")
    snapshot = (ROOT / "Engine/src/editor/application/editor.application_context_snapshot.inl").read_text(encoding="utf-8")
    inspector = (ROOT / "Engine/src/editor/application/editor.frame_inspector.inl").read_text(encoding="utf-8")
    texture = (ROOT / "Engine/src/editor/application/editor.application_texture_authoring.inl").read_text(encoding="utf-8")
    properties = inspector[inspector.index("                auto* document = synchronize_gui_document(editor);"):
        inspector.index("                if (document && widget)")]
    identity = definition(state, "            struct GuiWidgetIdentity final", True)
    scene_functions = definition(scene, "        void clear_editor_selection(")
    scene_functions += definition(scene, "        void select_editor_entity(").replace(
        "void select_editor_entity(", "void select_editor_entity_impl(")
    scene_functions += r'''
void select_editor_entity(EditorState& state,std::size_t index){
 REQUIRE(locks==0);select_editor_entity_impl(state,index);++selections;
}
'''
    scene_functions += "\nvoid normalize_editor_entity_ids(EditorState&){}\n"
    scene_functions += "\n".join(definition(scene, marker) for marker in (
        "        [[nodiscard]] bool editor_entity_selected(",
        "        void apply_editor_primary_selection_side_effects(",
        "        void synchronize_editor_selection("))
    types = "\n".join(definition(pane, marker, True) for marker in (
        "        struct DetachedPaneProjection", "        struct DetachedGuiHierarchySelection final"))
    functions = "\n".join(definition(pane, marker) for marker in (
        "        [[nodiscard]] std::shared_ptr<const editor_hierarchy::GuiHierarchyProjection>",
        "        [[nodiscard]] bool queue_detached_gui_hierarchy_selection(",
        "        [[nodiscard]] std::vector<DetachedGuiHierarchySelection>"))
    functions += "\n" + "\n".join(definition(docs, marker) for marker in (
        "        [[nodiscard]] EditorState::GuiWidgetIdentity gui_widget_identity(",
        "        [[nodiscard]] bool gui_widget_identity_matches(",
        "        [[nodiscard]] std::optional<authoring::gui::WidgetHandle>",
        "        [[nodiscard]] bool gui_widget_selected(",
        "        bool select_gui_widget(",
        "        [[nodiscard]] bool select_gui_hierarchy_widget(",
        "        void apply_detached_gui_hierarchy_selections(",
        "        [[nodiscard]] bool create_gui_widget_projection(",
        "        void remember_gui_projection_tombstone(",
        "        void reconcile_gui_document_projections(",
        "        [[nodiscard]] bool remove_selected_gui_widget(",
        "        [[nodiscard]] bool duplicate_selected_gui_widget(",
        "        [[nodiscard]] bool undo_gui_document(",
        "        [[nodiscard]] bool redo_gui_document(",
        "        void hydrate_gui_content_editor(",
        "        [[nodiscard]] bool apply_gui_content_editor("))
    functions = next(line for line in docs.splitlines()
        if "inline constexpr std::string_view kLegacyUnassignedGuiImage" in line) + "\n" + functions
    functions += "\n" + "\n".join(definition(docs, marker) for marker in (
        "        [[nodiscard]] bool is_gui_authoring_entity(",
        "        [[nodiscard]] std::string_view gui_widget_asset_path(",
        "        [[nodiscard]] bool update_gui_widget_image_path(",
        "        void seed_legacy_gui_widget_image(",
        "        [[nodiscard]] authoring::gui::WidgetDescriptor\n        gui_widget_descriptor_from_entity(",
        "        [[nodiscard]] bool gui_layout_matches_scene_projection(",
        "        [[nodiscard]] authoring::gui::MutationResult create_authored_gui_widget("))
    # Keep the hierarchy tests' narrow state stub and run the real synchronizer
    # where the image ownership test needs the scene-to-GUI boundary.
    functions += "\nauthoring::gui::GuiDocument*\n" + definition(
        docs, "        synchronize_gui_document(EditorState& state)").replace(
            "synchronize_gui_document(", "synchronize_image_fixture_document(")
    functions += "\n" + definition(texture, "        [[nodiscard]] std::optional<bool> edit_selected_gui_image_path(").replace(
        "synchronize_gui_document(", "synchronize_image_fixture_document(")
    snapshot_reset = snapshot[snapshot.index("        editor.guiSelectedWidget = {};"):
        snapshot.index("        editor.guiCanvasWidgetDrag = {};")]
    return r'''
namespace owner_contract {
namespace authoring=epochengine::authoring;
namespace editor_hierarchy=epochengine::editor_hierarchy;
namespace editor_authoring_history=epochengine::editor_authoring_history;
namespace core {struct Context{};enum class ScenePreviewMode{Editor};}
namespace scene {using SceneObjectId=std::uint64_t;inline constexpr auto kInvalidSceneObjectId=SceneObjectId{};}
namespace gui {struct ChatMessageRunInfo{};}
enum class EditorApplicationKind{Standard};
enum class OutlinerToolTab{World,Gui};enum class InspectorToolTab{Properties};
struct Material{int filter{},address_u{},address_v{},alpha{},color_space{};float alpha_cutoff{};std::string logical_path;};
struct EditorEntity{std::uint64_t sceneObjectId{};std::optional<Material> textureMaterial{};std::string name{},type{};
 std::array<float,3> position{},scale{1.0f,1.0f,1.0f};};
struct FakeTextures{struct Texture{std::filesystem::path source_path;};bool select(std::string_view){return false;}const Texture* selected(){return nullptr;}};
struct GuiProjectionTombstone{authoring::gui::WidgetHandle widget{};EditorEntity entity{};};
struct EditorState {
 IDENTITY
 GuiWidgetIdentity guiSelectedWidget{},guiContentEditorWidget{};
 std::string projectId,projectRoot,guiDocumentSourcePath,guiDocumentStatus;
 std::unique_ptr<authoring::gui::GuiDocument> guiDocument;
 authoring::gui::WidgetHandle guiDocumentRoot{};
 std::map<std::uint64_t,authoring::gui::WidgetHandle> guiDocumentWidgets;
 std::vector<EditorEntity> entities;
 std::uint64_t selectedEntityId{};
 std::size_t selectedEntity{};std::vector<std::uint64_t> selectedEntityIds;
 bool sceneDragActive{},sceneDragHasPlaneHit{},sceneMarqueeActive{},textureBrushActive{},game2DGuiWorkspace{};
 std::uint64_t sceneDragEntity{},sceneDragEntityId{};std::vector<int> sceneDragStartPositions;
 int textureFilter{},textureAddressU{},textureAddressV{},textureAlpha{},textureColorSpace{};float textureAlphaCutoff{};
 std::string selectedAssetPath,selectedProjectFile;
 std::unique_ptr<FakeTextures> projectTextures;
 OutlinerToolTab outlinerToolTab{};InspectorToolTab inspectorToolTab{};std::size_t guiAuthoringSection{2};
 std::vector<GuiProjectionTombstone> guiProjectionTombstones;
 std::string guiNameEditor,guiContentPrimary,guiContentSecondary,guiActionEditor,guiTooltipEditor;
 std::uint64_t guiDocumentSavedRevision{};
 bool guiDocumentReplaceArmed{},guiDocumentLoadAttempted{};
 struct {} guiCanvasView{};
 std::shared_ptr<const editor_hierarchy::GuiHierarchyProjection> guiHierarchyProjection;
 std::string guiHierarchyProjectId,guiHierarchyProjectRoot,guiHierarchySourcePath;
 bool refuseProjectionCreation{};
};
TYPES
inline unsigned locks{},selections{};
struct TrackedMutex {std::mutex mutex;void lock(){mutex.lock();++locks;}void unlock(){--locks;mutex.unlock();}};
struct EditorStorage {
 TrackedMutex mutex;std::map<const core::Context*,EditorState> states;
 std::map<std::string,bool> detachedPaneRoutes;
 DetachedPaneProjection detachedPaneProjection;
 std::vector<DetachedGuiHierarchySelection> detachedGuiHierarchySelections;
};
EditorStorage& editor_storage(){static EditorStorage storage;return storage;}
std::optional<std::size_t> editor_entity_index(const EditorState& state,std::uint64_t id){
 for(std::size_t i=0;i<state.entities.size();++i)if(state.entities[i].sceneObjectId==id)return i;return {};
}
SCENE_FUNCTIONS
namespace epochengine{template<class...Args>std::string format_text(std::string_view value,Args&&...){return std::string{value};}}
void push_editor_log(EditorState&,const std::string&){}
bool project_gui_document_layout(EditorState&){return true;}
// This fixture has no World document. Its GUI-only Undo reaches the existing
// fallback; paired World/GUI admission is tested by the grouped-history owner.
std::optional<bool> try_grouped_gui_world_history(EditorState&,
 editor_authoring_history::BranchId,editor_authoring_history::Direction){return std::nullopt;}
authoring::gui::GuiDocument* synchronize_gui_document(EditorState& state){return state.guiDocument.get();}
bool load_project_gui_document(EditorState&){return false;}
authoring::gui::TemplatePreset project_gui_starter_template(const EditorState&){return authoring::gui::TemplatePreset::desktop_app;}
std::filesystem::path project_gui_document_path(const EditorState& state){return state.guiDocumentSourcePath;}
std::string display_project_path(const std::filesystem::path& path){return path.generic_string();}
bool destroy_editor_scene_entity(EditorState& state,std::uint64_t id,std::string_view){
 std::erase_if(state.entities,[&](const auto& entity){return entity.sceneObjectId==id;});
 if(state.selectedEntityId==id)clear_editor_selection(state);return true;
}
std::uint64_t allocate_editor_scene_object_id(const EditorState& state,std::string_view){
 std::uint64_t id=1001;
 while(editor_entity_index(state,id))++id;
 return id;
}
bool create_editor_scene_entity(EditorState& state,EditorEntity entity,std::string_view){
 if(state.refuseProjectionCreation)return false;
 REQUIRE(entity.sceneObjectId!=0&&!editor_entity_index(state,entity.sceneObjectId));
 state.entities.push_back(std::move(entity));
 clear_editor_selection(state); // Document restoration must not depend on focus.
 return true;
}
FUNCTIONS
void reset_snapshot_gui(EditorState& editor){SNAPSHOT_RESET}
std::optional<authoring::gui::WidgetView> resolve_gui_properties(EditorState& editor){
PROPERTIES
 return widget;
}
DetachedPaneProjection show(const core::Context* owner,EditorState& state){
 DetachedPaneProjection projection;
 projection.ready=true;projection.ownerContext=owner;projection.projectId=state.projectId;
 projection.projectRoot=state.projectRoot;projection.guiDocumentSourcePath=state.guiDocumentSourcePath;
 projection.guiHierarchy=gui_hierarchy_projection(state);
 editor_storage().detachedPaneProjection=projection;return projection;
}
void verify_projection_creation_with_no_selection(){
 using namespace authoring::gui;
 EditorState state;
 state.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{6,1},BranchIdentity{8,1});
 auto& document=*state.guiDocument;
 state.guiDocumentRoot=document.create_widget(canvas("Root")).widget;
 const auto first=document.create_widget(panel("First"),state.guiDocumentRoot).widget;
 const auto second=document.create_widget(panel("Second"),state.guiDocumentRoot).widget;
 const auto rejected=document.create_widget(panel("Rejected"),state.guiDocumentRoot).widget;
 const auto savedRevision=document.revision();
 REQUIRE(create_gui_widget_projection(state,{.name="First"},first,"Restore saved widget"));
 REQUIRE(create_gui_widget_projection(state,{.name="Second"},second,"Restore saved widget"));
 REQUIRE(state.selectedEntityId==0&&state.entities.size()==2&&state.guiDocumentWidgets.size()==2);
 REQUIRE(!state.guiDocumentWidgets.contains(0));
 REQUIRE(state.guiDocumentWidgets.at(state.entities[0].sceneObjectId)==first);
 REQUIRE(state.guiDocumentWidgets.at(state.entities[1].sceneObjectId)==second);
 state.refuseProjectionCreation=true;
 REQUIRE(!create_gui_widget_projection(state,{.name="Rejected"},rejected,"Restore saved widget"));
 REQUIRE(state.entities.size()==2&&state.guiDocumentWidgets.size()==2);
 REQUIRE(document.revision()==savedRevision);
}
void verify_scene_selection_reconciliation(){
 EditorState state;
 state.entities={{101},{102},{103}};
 state.selectedEntityId=102;
 synchronize_editor_selection(state);
 REQUIRE(state.selectedEntityId==102&&state.selectedEntity==1);
 REQUIRE(state.selectedEntityIds==std::vector<std::uint64_t>{102});

 state.selectedEntityIds={103,101,999,103};
 synchronize_editor_selection(state);
 REQUIRE(state.selectedEntityId==102&&state.selectedEntity==1);
 const std::vector<std::uint64_t> expected{101,102,103};
 REQUIRE(std::ranges::is_permutation(state.selectedEntityIds,expected));
 synchronize_editor_selection(state);
 REQUIRE(state.selectedEntityId==102&&state.selectedEntityIds==expected);
}
void verify_legacy_gui_image_ownership(){
 using namespace authoring::gui;
 constexpr std::string_view oldPath="Assets/Textures/legacy.epoch_texture";
 constexpr std::string_view newPath="Assets/Textures/selected.epoch_texture";
 for(const auto type:{"GuiImage","GuiImageButton"}){
  EditorState state;
  state.projectId="image-owner";state.projectRoot="/project/image-owner";
  state.guiDocumentSourcePath="/project/image-owner/Assets/Gui/main.epochgui";
  state.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{12,1},BranchIdentity{13,1});
  auto& document=*state.guiDocument;
  state.guiDocumentRoot=document.create_widget(canvas("Root")).widget;
  const EditorEntity oldImage{.sceneObjectId=401,.textureMaterial=Material{.logical_path=std::string{oldPath}},
   .name="Legacy",.type=type,.position={-3.2f,1.8f,0.0f}};
  state.entities={oldImage,oldImage};
  state.entities[1].sceneObjectId=402;state.entities[1].name="Scene only";
  auto placeholder=gui_widget_descriptor_from_entity(oldImage);
  placeholder.interaction.action="retained.action";
  std::visit([](auto& content){
   if constexpr(requires {content.asset_path;}) content.asset_path=kLegacyUnassignedGuiImage;
  },placeholder.content);
  const auto widget=document.create_widget(placeholder,state.guiDocumentRoot).widget;
  state.guiDocumentWidgets.emplace(oldImage.sceneObjectId,widget);
  seed_legacy_gui_widget_image(document,*document.widget(widget),oldImage);
  REQUIRE(gui_widget_asset_path(*document.widget(widget))==oldPath);
  REQUIRE(synchronize_image_fixture_document(state));
  REQUIRE(gui_widget_asset_path(*document.widget(state.guiDocumentWidgets.at(402)))==oldPath);

  hydrate_gui_content_editor(state,*document.widget(widget));
  state.guiContentPrimary=newPath;
  REQUIRE(apply_gui_content_editor(state,document,*document.widget(widget)));
  REQUIRE(synchronize_image_fixture_document(state));
  REQUIRE(gui_widget_asset_path(*document.widget(widget))==newPath);
  seed_legacy_gui_widget_image(document,*document.widget(widget),oldImage);
  REQUIRE(gui_widget_asset_path(*document.widget(widget))==newPath);

  state.selectedEntityId=oldImage.sceneObjectId;
  REQUIRE(edit_selected_gui_image_path(state,{}).value_or(false));
  REQUIRE(synchronize_image_fixture_document(state));
  REQUIRE(gui_widget_asset_path(*document.widget(widget)).empty());
  REQUIRE(document.widget(widget)->descriptor.interaction.action=="retained.action");
  GuiDocument reopened(document.snapshot());
  seed_legacy_gui_widget_image(reopened,*reopened.widget(widget),oldImage);
  REQUIRE(gui_widget_asset_path(*reopened.widget(widget)).empty());
  REQUIRE(document.undo());
  REQUIRE(synchronize_image_fixture_document(state));
  REQUIRE(gui_widget_asset_path(*document.widget(widget))==newPath);
  REQUIRE(state.entities[0].textureMaterial->logical_path==oldPath);
 }
}
void run(){
 using namespace authoring::gui;
 verify_projection_creation_with_no_selection();
 verify_scene_selection_reconciliation();
 verify_legacy_gui_image_ownership();
 core::Context a,b;auto& storage=editor_storage();auto& state=storage.states[&a];
 auto& other=storage.states[&b];
 state.projectId="A";state.projectRoot="/project/A";state.guiDocumentSourcePath="/project/A/Assets/gui.epoch_gui";
 state.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{7,1},BranchIdentity{9,1});
 state.guiDocumentRoot=state.guiDocument->create_widget(canvas("Root")).widget;
 const auto first=state.guiDocument->create_widget(panel("First"),state.guiDocumentRoot).widget;
 const auto second=state.guiDocument->create_widget(panel("Second"),state.guiDocumentRoot).widget;
 const auto documentOnly=state.guiDocument->create_widget(panel("DocOnly"),state.guiDocumentRoot).widget;
 state.entities={{101},{102}};state.guiDocumentWidgets={{101,first},{102,second}};
 state.selectedEntityId=101;
 auto cached=gui_hierarchy_projection(state);REQUIRE(cached&&cached->rows.size()==3);
 REQUIRE(gui_hierarchy_projection(state)==cached);
 state.selectedEntityId=102;REQUIRE(gui_hierarchy_projection(state)==cached);
 state.projectId="A-new";auto changed=gui_hierarchy_projection(state);REQUIRE(changed!=cached);
 cached=changed;state.guiDocumentSourcePath+=".new";
 changed=gui_hierarchy_projection(state);REQUIRE(changed!=cached);
 cached=changed;REQUIRE(state.guiDocument->reorder_widget(second,0));
 REQUIRE(gui_hierarchy_projection(state)!=cached);
 storage.detachedPaneRoutes["pane.gui_hierarchy"]=true;
 auto projection=show(&a,state);
 REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 REQUIRE(queue_detached_gui_hierarchy_selection(projection,second));
 REQUIRE(storage.detachedGuiHierarchySelections.size()==1);
 apply_detached_gui_hierarchy_selections(&a,state);
 REQUIRE(state.selectedEntityId==102&&selections==1&&storage.detachedGuiHierarchySelections.empty());
 REQUIRE(!queue_detached_gui_hierarchy_selection(projection,WidgetHandle{999,1}));
 // A click arriving after a canonical focus takeover must not enqueue.
 storage.detachedPaneProjection.ownerContext=&b;
 REQUIRE(!queue_detached_gui_hierarchy_selection(projection,first));
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 storage.detachedPaneProjection.ownerContext=&b;
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(state.selectedEntityId==102&&selections==1);
 // Each of the canonical admission identities is rechecked at application.
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 state.projectRoot+="/replacement";
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 state.projectId+="-replacement";
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 state.guiDocumentSourcePath+=".replacement";
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 REQUIRE(state.guiDocument->reorder_widget(first,0));
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,first));
 REQUIRE(state.guiDocument->remove_widget(first));
 const auto reused=state.guiDocument->create_widget(panel("Reused"),state.guiDocumentRoot).widget;
 REQUIRE(reused!=first);apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 // Same project/document slots in a new branch are still a different document.
 projection=show(&a,state);REQUIRE(queue_detached_gui_hierarchy_selection(projection,second));
 state.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{7,1},BranchIdentity{9,2});
 state.guiDocumentRoot=state.guiDocument->create_widget(canvas("Root")).widget;
 const auto branchChild=state.guiDocument->create_widget(panel("Branch"),state.guiDocumentRoot).widget;
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 state.guiDocumentWidgets={{101,branchChild}};projection=show(&a,state);
 REQUIRE(queue_detached_gui_hierarchy_selection(projection,branchChild));
 storage.detachedPaneRoutes.erase("pane.gui_hierarchy");
 apply_detached_gui_hierarchy_selections(&a,state);REQUIRE(selections==1);
 storage.detachedPaneRoutes["pane.gui_hierarchy"]=true;
 REQUIRE(queue_detached_gui_hierarchy_selection(projection,branchChild));
 storage.states.erase(&a);REQUIRE(consume_detached_gui_hierarchy_selections(&a).empty());
 REQUIRE(storage.detachedGuiHierarchySelections.empty());
 // Canonical document-only rows select the document without creating entities.
 other.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{8,1},BranchIdentity{9,1});
 other.guiDocumentRoot=other.guiDocument->create_widget(canvas("Other")).widget;
 const auto only=other.guiDocument->create_widget(panel("Only"),other.guiDocumentRoot).widget;
 projection=show(&b,other);REQUIRE(queue_detached_gui_hierarchy_selection(projection,only));
 apply_detached_gui_hierarchy_selections(&b,other);
 REQUIRE(selections==1&&other.selectedEntityId==0&&other.entities.empty());
 REQUIRE(selected_gui_widget(other)==only&&gui_widget_selected(other,only));
 REQUIRE(resolve_gui_properties(other)->handle==only);
 synchronize_editor_selection(other);REQUIRE(selected_gui_widget(other)==only);
 REQUIRE(other.guiAuthoringSection==2&&other.outlinerToolTab==OutlinerToolTab::Gui);
 const auto selectedRevision=other.guiDocument->revision();
 auto widget=other.guiDocument->widget(only);REQUIRE(widget);
 hydrate_gui_content_editor(other,*widget);REQUIRE(other.guiNameEditor==widget->descriptor.name);
 other.guiContentPrimary="Edited scratch panel";
 REQUIRE(apply_gui_content_editor(other,*other.guiDocument,*widget));
 REQUIRE(other.guiDocument->revision()!=selectedRevision&&other.entities.empty());
 widget=other.guiDocument->widget(only);REQUIRE(std::get<PanelContent>(widget->descriptor.content).title=="Edited scratch panel");
 // A new revision does not erase an uncommitted Properties draft.
 other.guiContentPrimary="Uncommitted draft";hydrate_gui_content_editor(other,*widget);
 REQUIRE(other.guiContentPrimary=="Uncommitted draft");
 REQUIRE(undo_gui_document(other));widget=other.guiDocument->widget(only);
 hydrate_gui_content_editor(other,*widget);REQUIRE(other.guiContentPrimary!="Uncommitted draft");
 REQUIRE(redo_gui_document(other));widget=other.guiDocument->widget(only);
 hydrate_gui_content_editor(other,*widget);REQUIRE(other.guiContentPrimary=="Edited scratch panel");
 REQUIRE(other.entities.empty()&&selected_gui_widget(other)==only);
 const auto beforeSelection=other.guiDocument->revision();
 REQUIRE(select_gui_widget(other,only)&&other.guiDocument->revision()==beforeSelection);
 REQUIRE(remove_selected_gui_widget(other,*other.guiDocument));
 REQUIRE(!selected_gui_widget(other)&&!other.guiContentEditorWidget.widget&&other.entities.empty());
 REQUIRE(undo_gui_document(other)&&other.guiDocument->widget(only));
 REQUIRE(!selected_gui_widget(other)); // Undo does not silently resurrect a UI selection.
 REQUIRE(select_gui_widget(other,only));
 REQUIRE(duplicate_selected_gui_widget(other,*other.guiDocument));
 const auto duplicate=selected_gui_widget(other);REQUIRE(duplicate&&*duplicate!=only&&other.entities.empty());
 REQUIRE(undo_gui_document(other)&&!selected_gui_widget(other));
 REQUIRE(redo_gui_document(other)&&other.entities.empty());
 REQUIRE(!select_gui_widget(other,WidgetHandle{999,1}));
 REQUIRE(select_gui_widget(other,only));
 other.projectId+="changed";REQUIRE(!selected_gui_widget(other));other.projectId.resize(other.projectId.size()-7);
 other.projectRoot+="changed";REQUIRE(!selected_gui_widget(other));other.projectRoot.resize(other.projectRoot.size()-7);
 other.guiDocumentSourcePath+="changed";REQUIRE(!selected_gui_widget(other));other.guiDocumentSourcePath.resize(other.guiDocumentSourcePath.size()-7);
 REQUIRE(selected_gui_widget(other)==only);
 other.guiDocument=std::make_unique<GuiDocument>(DocumentHandle{8,1},BranchIdentity{9,2});
 other.guiDocumentRoot=other.guiDocument->create_widget(canvas("Replacement")).widget;
 const auto replacement=other.guiDocument->create_widget(panel("New"),other.guiDocumentRoot).widget;
 REQUIRE(replacement==only&&!selected_gui_widget(other));
 REQUIRE(select_gui_widget(other,replacement));
 // A real scene selection clears the document-only authority and its form key.
 other.entities={{333}};select_editor_entity(other,0);
 REQUIRE(!other.guiSelectedWidget.widget&&!other.guiContentEditorWidget.widget&&!selected_gui_widget(other));
 other.guiDocumentWidgets={{333,replacement}};
 REQUIRE(selected_gui_widget(other)==replacement);
 REQUIRE(select_gui_widget(other,replacement)&&other.selectedEntityId==333);
 clear_editor_selection(other);REQUIRE(!selected_gui_widget(other));
 REQUIRE(select_gui_widget(other,replacement));reset_snapshot_gui(other);
 REQUIRE(!other.guiSelectedWidget.widget&&!other.guiContentEditorWidget.widget&&!selected_gui_widget(other));
 other.guiDocument.reset();REQUIRE(!gui_hierarchy_projection(other));
 REQUIRE(locks==0);
}
}
'''.replace("TYPES", types).replace("IDENTITY", identity).replace("SCENE_FUNCTIONS", scene_functions).replace("FUNCTIONS", functions).replace("SNAPSHOT_RESET", snapshot_reset).replace("PROPERTIES", properties)


class GuiHierarchyProjection(unittest.TestCase):
    def test_actual_document_rows_debug_and_release(self):
        if os.name != "nt":
            self.skipTest("Standalone installed MSVC C++23 module consumer lane")
        proof = ROOT / "build/validation/gui-hierarchy-projection-oct26"
        proof.mkdir(parents=True, exist_ok=True)
        consumer = ROOT / "Tests/gui/editor.gui_hierarchy_projection_contract.cpp"
        (proof / "gui_owner_contract.hpp").write_text(actual_owner_fixture(), encoding="utf-8")
        owners = [
            ROOT / "Engine/modules/authoring/authoring.document.ixx",
            ROOT / "Engine/modules/authoring/authoring.gui_document.ixx",
            ROOT / "Engine/modules/editor/editor.hierarchy_adapter.ixx",
            ROOT / "Engine/src/authoring/gui/authoring.gui_document_codec.cpp",
            ROOT / "Engine/src/authoring/gui/authoring.gui_templates.cpp",
            ROOT / "Engine/src/editor/editor.hierarchy_adapter.cpp",
            ROOT / "Engine/dep/EpochGui/src/epochgui/hierarchy_tree.cpp",
            consumer,
        ]
        modules = "\n".join(f'"{path.as_posix()}"' for path in owners[:3])
        modules += '\n"' + (ROOT / "Engine/modules/editor/editor.authoring_history.ixx").as_posix() + '"'
        modules += '\n"' + (ROOT / "Engine/modules/core/core.sha256.ixx").as_posix() + '"'
        sources = "\n".join(f'"{path.as_posix()}"' for path in owners[3:])
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(GuiHierarchyProjection LANGUAGES CXX)
add_executable(gui_hierarchy_projection {sources})
target_compile_features(gui_hierarchy_projection PRIVATE cxx_std_23)
target_compile_options(gui_hierarchy_projection PRIVATE /W3 /WX /utf-8)
set_property(TARGET gui_hierarchy_projection PROPERTY MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:Embedded>")
target_include_directories(gui_hierarchy_projection PRIVATE "{(ROOT / 'Engine/dep/EpochGui/include').as_posix()}")
target_include_directories(gui_hierarchy_projection PRIVATE "{proof.as_posix()}")
target_sources(gui_hierarchy_projection PRIVATE FILE_SET CXX_MODULES
    BASE_DIRS "{(ROOT / 'Engine/modules').as_posix()}" FILES {modules})
enable_testing()
add_test(NAME actual_gui_hierarchy_projection COMMAND gui_hierarchy_projection)
set_tests_properties(actual_gui_hierarchy_projection PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(proof / "msvc"),
            "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [
                ["cmake", "--build", str(proof / "msvc"), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(proof / "msvc"), "-C", config, "--output-on-failure"],
            ]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True,
                text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode,
                "log": str(log)})
            self.assertEqual(completed.returncode, 0,
                completed.stdout[-10000:] + completed.stderr)
        for path in owners[:3]:
            self.assertRegex(path.read_text(encoding="utf-8-sig"), r"(?m)^export module [\w.]+;")
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual authoring GUI document + shared Editor row adapter; Debug and Release independent CPU consumer",
            "passed": True,
            "nativeAcceptance": False,
            "owners": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in owners + [
                    ROOT / "Engine/src/editor/application/editor.application_pane_routing.inl",
                    ROOT / "Engine/src/editor/application/editor.application_scene_selection.inl",
                    ROOT / "Engine/src/editor/application/editor.application_project_documents.inl"]},
            "commands": results,
            "cases": ["nested preorder and sibling order", "true hierarchy depths",
                "root isolation and canvas omission", "metadata excludes widget text",
                "reorder and undo revision", "removed/reused generational handle",
                "empty and invalid roots/documents", "actual document count/name/depth limits",
                "canonical cache reuses unchanged rows with separate selection",
                "canonical selection runs outside storage mutex", "latest click supersedes older click",
                "stale owner/project/root/source/branch/revision/generation refused",
                "redock and retired owner refuse commands", "scratch selection/edit/duplicate leaves entities unchanged",
                "mapped selection synchronizes actual scene helper", "saved widget projections retain distinct bindings without selection",
                "an existing primary is retained with empty or incomplete scene selection membership",
                "scene selection reconciliation preserves valid members and removes stale and duplicate entries",
                "refused scene creation leaves bindings and revision unchanged", "content hydration follows document/handle and preserves drafts",
                "real document undo/redo invalidates form and removed selection", "replacement branch and snapshot reject stale handles",
                "legacy scene image seeds once and authored replacement/clear survives sync, snapshot and Undo"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
