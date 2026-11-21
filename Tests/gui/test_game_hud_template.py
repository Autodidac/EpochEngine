"""Compile the stock HUD through its real document, artifact and runtime owners."""
from pathlib import Path
import os
import unittest

from Tests.support.gui_contract import run_gui_contract
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]

CONSUMER = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
import authoring.gui_document;
import authoring.gui_compiler;
import asset.gui_artifact;
import project.gui_runtime;
namespace author=epochengine::authoring::gui;
namespace asset=epochengine::asset::gui;
namespace rt=epochengine::project_gui_runtime;
HOST_OWNERS
#define CHECK(value) do {if(!(value)){std::fprintf(stderr,"HUD contract failed line %d\n",__LINE__);return 81;}}while(false)
bool overlaps(rt::Rect a,rt::Rect b){return a.x<b.x+b.width&&b.x<a.x+a.width&&a.y<b.y+b.height&&b.y<a.y+a.height;}
int main(){
 auto document=author::make_template_document(author::TemplatePreset::game_hud);CHECK(document&&!document->validate());
 auto encoded=author::serialize_gui_document(document->snapshot());CHECK(encoded);
 auto decoded=author::deserialize_gui_document(encoded.bytes);CHECK(decoded);
 auto compiled=author::compile_gui_document(*decoded.snapshot);CHECK(compiled);
 rt::RuntimeSession runtime{compiled.artifact};CHECK(runtime.valid());
 // New starters are visibly examples; they invent neither player telemetry nor
 // objectives. Existing authored documents are not template migrations.
 bool sample=false;unsigned buttons=0;
 for(const auto& widget:compiled.artifact.widgets){
  sample|=widget.primary_text.find("Sample HUD")!=std::string::npos;
  CHECK(widget.name!="Health"&&widget.name!="Energy"&&widget.name!="ObjectiveText");
  CHECK(widget.kind!=asset::WidgetKind::slider);
  if(widget.kind==asset::WidgetKind::button){++buttons;CHECK(widget.focusable&&widget.accepts_pointer);}
 }
 CHECK(sample&&buttons==3);
 CHECK(!author::is_legacy_generated_root_only_document(*decoded.snapshot));
 CHECK(!author::migrate_legacy_generated_root_only_document(*decoded.snapshot,author::TemplatePreset::desktop_app));
 const auto second=author::serialize_gui_document(*decoded.snapshot);CHECK(second&&second.bytes==encoded.bytes);
 // Projection remains inside each letterboxed viewport, with the central
 // view unobstructed. Decorations do not eat camera input; actions do.
 for(const auto viewport:{rt::Float2{640,360},rt::Float2{1280,720},rt::Float2{1920,1080},rt::Float2{3440,1440},rt::Float2{720,1280}}){
  auto frame=runtime.build_frame(viewport);CHECK(frame);
  std::array<rt::Rect,3> actionBounds{};unsigned actionIndex=0;
  const rt::Rect clearView{frame.offset.x+384*frame.scale,frame.offset.y+144*frame.scale,512*frame.scale,432*frame.scale};
  for(const auto& view:frame.widgets){
   CHECK(view.visible&&view.bounds.x>=0&&view.bounds.y>=0);
   CHECK(view.bounds.x+view.bounds.width<=viewport.x+.01f&&view.bounds.y+view.bounds.height<=viewport.y+.01f);
   if(view.kind==asset::WidgetKind::canvas)continue;
   CHECK(!overlaps(view.bounds,clearView));
   if(view.kind==asset::WidgetKind::panel){
    CHECK(!view.accepts_pointer);
    CHECK(!runtime.pointer_captured({view.bounds.x+frame.scale,view.bounds.y+frame.scale}));
   }
   if(view.kind!=asset::WidgetKind::button)continue;
   CHECK(actionIndex<actionBounds.size());actionBounds[actionIndex++]=view.bounds;
   const rt::Float2 point{view.bounds.x+view.bounds.width/2,view.bounds.y+view.bounds.height/2};
   CHECK(runtime.pointer_captured(point));(void)runtime.drain_events();
   CHECK(runtime.pointer_release(point)==rt::RuntimeCode::ready);
   const auto events=runtime.drain_events();unsigned activations=0;
   for(const auto& event:events)if(event.kind==rt::EventKind::activated){
    ++activations;const auto& widget=compiled.artifact.widgets[view.index];CHECK(event.action==widget.action);
    CHECK(event.action=="interact"||event.action=="jump"||event.action=="pause");
   }
   CHECK(activations==1);
   CHECK(runtime.activate_focused()==rt::RuntimeCode::ready);
   const auto keyboard=runtime.drain_events();CHECK(keyboard.size()==1&&keyboard[0].kind==rt::EventKind::activated);
  }
  CHECK(actionIndex==3);
  for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b)CHECK(!overlaps(actionBounds[a],actionBounds[b]));
  CHECK(!runtime.pointer_captured({viewport.x/2,viewport.y/2}));
 }
 // Actual ProjectPlayScene capability admission and dispatch, backed by the
 // real GUI session. Gameplay/controller delivery is the only host port.
 Owner host;host.m_projectGuiRuntime=std::make_unique<rt::RuntimeSession>(compiled.artifact);
 host.refresh_project_gui_actions();
 auto unavailable=host.m_projectGuiRuntime->build_frame({1280,720});CHECK(unavailable);
 asset::WidgetId pause{},jump{};
 for(const auto& view:unavailable.widgets)if(view.kind==asset::WidgetKind::button){
  CHECK(!view.enabled);CHECK(host.m_projectGuiRuntime->focus(view.id)==rt::RuntimeCode::disabled_widget);
  const auto& action=compiled.artifact.widgets[view.index].action;
  if(action=="pause")pause=view.id;if(action=="jump")jump=view.id;
 }
 CHECK(pause.value&&jump.value&&host.m_projectGuiRuntime->focus_next()==rt::RuntimeCode::no_change);
 host.m_explorationController=true;host.refresh_project_gui_actions();
 auto planetary=host.m_projectGuiRuntime->build_frame({1280,720});CHECK(planetary);
 for(const auto& view:planetary.widgets)if(view.kind==asset::WidgetKind::button)CHECK(view.enabled==(view.id==pause));
 CHECK(host.m_projectGuiRuntime->focus_next()==rt::RuntimeCode::ready&&host.m_projectGuiRuntime->focused_widget()==pause);
 (void)host.m_projectGuiRuntime->drain_events();
 CHECK(host.m_projectGuiRuntime->activate_focused()==rt::RuntimeCode::ready);
 for(const auto& event:host.m_projectGuiRuntime->drain_events())host.queue_project_gui_action(event);
 CHECK(host.m_explorationInput.command==project_planetary_runtime::Command::toggle_time_pause);
 host.m_explorationInput.command=project_planetary_runtime::Command::none;
 host.queue_project_gui_action({.kind=rt::EventKind::slider_changed,.action="pause"});
 CHECK(host.m_explorationInput.command==project_planetary_runtime::Command::none);
 host.queue_project_gui_action({.kind=rt::EventKind::activated,.action="jump"});
 CHECK(!host.m_pendingGuiActions[2]);
 // Losing a capability retires focus/pending activation before a layout
 // refresh, so the old button rectangle cannot deliver stale commands.
 CHECK(host.m_projectGuiRuntime->activate_focused()==rt::RuntimeCode::ready);
 host.m_explorationController=false;host.refresh_project_gui_actions();
 CHECK(!host.m_projectGuiRuntime->keyboard_captured()&&host.m_projectGuiRuntime->drain_events().empty());
 for(const auto& view:planetary.widgets)if(view.id==pause){
  const rt::Float2 point{view.bounds.x+1,view.bounds.y+1};
  CHECK(!host.m_projectGuiRuntime->pointer_captured(point));
  CHECK(host.m_projectGuiRuntime->pointer_release(point)==rt::RuntimeCode::disabled_widget);
 }
 // Any project owns GUI bindings; no 2D gameplay object is required. A
 // matching name with the wrong stable action ID stays disabled.
 CHECK(!host.m_gameplayRuntime);host.m_projectInput.emplace();
 auto& profile=*host.m_projectInput;
 profile.actions.push_back({project_input::stable_action_id(project_input::ActionSemantic::jump),project_input::ActionSemantic::jump,project_input::ActionValueKind::button});
 profile.actions.push_back({{123},project_input::ActionSemantic::pause,project_input::ActionValueKind::button});
 host.refresh_project_gui_actions();CHECK(host.project_gui_action_available("jump")&&!host.project_gui_action_available("pause"));
 host.queue_project_gui_action({.kind=rt::EventKind::activated,.action="jump"});
 CHECK(host.m_pendingGuiActions[2]&&host.m_pendingGuiActions[2]->pressed);
 profile.actions.back().id=project_input::stable_action_id(project_input::ActionSemantic::pause);
 host.refresh_project_gui_actions();CHECK(host.project_gui_action_available("pause"));
 host.queue_project_gui_action({.kind=rt::EventKind::activated,.action="pause"});CHECK(host.m_pendingGuiActions[4]);
 host.m_gameplayRuntime=std::make_unique<GameplayPort>();
 CHECK(host.project_gui_action_available("jump")&&host.project_gui_action_available("pause"));
 host.m_packagedShoreline.emplace();
 CHECK(host.project_gui_action_available("pause"));
 host.m_packagedShoreline->allow_flight=true;
 host.refresh_project_gui_actions();
 CHECK(host.m_projectGuiRuntime->focus(pause)==rt::RuntimeCode::disabled_widget);
 CHECK(host.project_gui_action_available("jump"));
 CHECK(!host.project_gui_action_available("editor.return"));
 host.m_canReturnToEditor=true;
 CHECK(host.project_gui_action_available("editor.return")&&!host.project_gui_action_available("unknown"));
 CHECK(host.m_projectGuiRuntime->artifact()->identity==compiled.artifact.identity);
 // Runtime overlay opt-out does not delete authored content. Invalid existing
 // manifests fail closed, while absent legacy settings retain their default.
 const auto root=std::filesystem::current_path()/"gui-gate";
 std::filesystem::create_directories(root);host.m_projectRoot=root.generic_string();
 const auto manifest=root/"project.epoch.json";
 std::filesystem::remove(manifest);epochengine::testRunSettings.reset();
 host.prepare_project_gui(epochengine::testRunSettings);CHECK(host.guiLoaded);host.guiLoaded=false;
 {std::ofstream file(manifest);file<<"malformed";}
 host.prepare_project_gui(epochengine::testRunSettings);CHECK(!host.guiLoaded);
 epochengine::testRunSettings=epochengine::RunSettingsPort{false};
 host.prepare_project_gui(epochengine::testRunSettings);CHECK(!host.guiLoaded&&std::filesystem::exists(manifest));
 epochengine::testRunSettings=epochengine::RunSettingsPort{true};
 host.prepare_project_gui(epochengine::testRunSettings);CHECK(host.guiLoaded&&std::filesystem::exists(manifest));
 // Screen-space authoring proxies stay out of 3D; categorizing an actual mesh
 // as UI must not hide world-space interfaces or ordinary 3D content.
 CHECK(is_screen_space_gui_entity({"GuiButton","UI"}));
 CHECK(is_screen_space_gui_entity({"GuiDocument","UI"}));
 CHECK(!is_screen_space_gui_entity({"Mesh","UI"}));
 CHECK(!is_screen_space_gui_entity({"WorldSpaceUI","UI"}));
 return 0;
}
'''


def consumer():
    profile = (ROOT / "Engine/modules/project/project.input_profile.ixx").read_text(encoding="utf-8")
    input_owner = (ROOT / "Engine/src/project/project.input_profile.cpp").read_text(encoding="utf-8")
    play = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8")
    planet = (ROOT / "Engine/modules/project.planetary_runtime.ixx").read_text(encoding="utf-8")
    documents = (ROOT / "Engine/src/editor/application/editor.application_project_documents.inl").read_text(encoding="utf-8")
    viewport = (ROOT / "Engine/src/editor/application/editor.frame_viewport.inl").read_text(encoding="utf-8")
    preview = (ROOT / "Engine/src/editor/application/editor.application_scene_preview.inl").read_text(encoding="utf-8")
    api = "\n".join(definition(profile, marker, True) for marker in (
        "    struct ContentHash final", "    struct ActionId final", "    enum class ActionSemantic",
        "    enum class ActionValueKind", "    struct ActionDefinition final", "    struct ActionImpulse final"))
    api += "\n" + "\n".join(definition(profile, marker) for marker in (
        "    [[nodiscard]] constexpr std::string_view action_semantic_name(",
        "    [[nodiscard]] constexpr ActionSemantic action_semantic_from_name("))
    api += "\n" + definition(input_owner, "        class HashBuilder final", True)
    api += "\n" + definition(input_owner, "        [[nodiscard]] constexpr bool valid_semantic(")
    api += "\n" + definition(input_owner, "    ActionId stable_action_id(")
    host = 'namespace epochengine::project_input { inline constexpr std::int32_t normalized_unit{32767};\n' + api + '\n}\n'
    host += 'namespace project_input=epochengine::project_input;\nnamespace project_planetary_runtime {\n'
    host += definition(planet, "    enum class Command", True) + '\n}\n'
    host += r'''
namespace epochengine{
struct RunSettingsPort{bool screen_space_gui_enabled{true};};
std::optional<RunSettingsPort>testRunSettings;
}
using EditorProjectRunSettings=epochengine::RunSettingsPort;
struct GameplayPort{
 struct Profile{std::vector<project_input::ActionDefinition>actions;}profile;
 const Profile*input_profile()const{return &profile;}
};
struct Owner{
 std::unique_ptr<rt::RuntimeSession>m_projectGuiRuntime;
 std::unique_ptr<GameplayPort>m_gameplayRuntime;
 std::optional<GameplayPort::Profile>m_projectInput;
 struct ShorelinePort{bool allow_flight{};};
 std::optional<ShorelinePort>m_packagedShoreline;
 bool m_explorationController{},m_canReturnToEditor{};
 struct Input{project_planetary_runtime::Command command{};}m_explorationInput;
 std::string m_projectGuiStatus,m_projectRoot;bool guiLoaded{};
 std::array<std::optional<project_input::ActionImpulse>,6>m_pendingGuiActions;
'''
    host += "\n".join(definition(play, marker) for marker in (
        "            [[nodiscard]] bool project_gui_action_available(",
        "            void refresh_project_gui_actions(", "            void queue_project_gui_action("))
    guard = definition(play, "            void prepare_project_gui(")
    guard = guard[:guard.index("                epochengine::project_gui::ArtifactLibrary library{")]
    host += '\n' + guard + '\n guiLoaded=true;\n}\n'
    host += '\n};\n'
    host += 'struct EditorEntity{std::string type,category;};\n'
    host += definition(documents, "        [[nodiscard]] bool is_gui_authoring_entity(") + '\n'
    host += definition(documents, "        [[nodiscard]] bool is_screen_space_gui_entity(") + '\n'
    assert play.index('refresh_project_gui_actions();') < play.index('m_projectGuiRuntime->build_frame(')
    gui_case = viewport[viewport.index('            case EditorMainSurface::Game2D:'):]
    assert gui_case.count('render_gui_document_canvas(') == viewport.count('render_gui_document_canvas(')
    assert 'if (!guiDocumentWorkspace)' in gui_case
    assert 'is_screen_space_gui_entity(entity)' in preview
    return CONSUMER.replace("HOST_OWNERS", host)


class GameHudTemplate(unittest.TestCase):
    def test_actual_template_roundtrip_layout_and_actions(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        owners = [ROOT / name for name in (
            "Engine/src/epoch/implementation/epoch.project_play_scene.inl",
            "Engine/modules/project/project.input_profile.ixx",
            "Engine/src/project/project.input_profile.cpp",
            "Engine/modules/project.planetary_runtime.ixx",
            "Engine/src/editor/application/editor.application_project_documents.inl",
            "Engine/src/editor/application/editor.application_scene_preview.inl",
            "Engine/src/editor/application/editor.frame_viewport.inl")] + [Path(__file__)]
        run_gui_contract(self, "game-hud-template", consumer(), owners,
                         "Gameplay HUD: actual document/compiler/runtime, five viewport layouts, capability/action and focus retirement; recording host ports only.")


if __name__ == "__main__":
    unittest.main()
