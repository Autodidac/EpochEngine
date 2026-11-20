"""Exercise the calculator as GUI-only software through its real runtime owner."""
from pathlib import Path
import os
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.gui_contract import run_gui_contract
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
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <iterator>
APPLICATION_OWNER
import authoring.gui_document;
import authoring.gui_compiler;
import asset.gui_artifact;
import project.gui_runtime;
namespace author=epochengine::authoring::gui;
namespace asset=epochengine::asset::gui;
namespace rt=epochengine::project_gui_runtime;
namespace project=epochengine::project;
namespace project_calculator=calculator_example;
HOST_OWNERS
#define CHECK(value) do {if(!(value)){std::fprintf(stderr,"Calculator contract failed line %d\n",__LINE__);return 81;}}while(false)
int check_calculator(){
 std::ifstream source("GUI_SOURCE",std::ios::binary);
 const std::vector<char> bytes{std::istreambuf_iterator<char>{source},{}};
 const auto reopened=author::deserialize_gui_document(std::as_bytes(std::span{bytes}));CHECK(reopened);
 const auto compiled=author::compile_gui_document(*reopened.snapshot);CHECK(compiled);
 CalculatorHost host;
 host.m_projectGuiRuntime=std::make_unique<rt::RuntimeSession>(compiled.artifact);
 auto& runtime=*host.m_projectGuiRuntime;
 CHECK(runtime.valid());
 asset::WidgetId expression,scientificResult,angleStatus,workedSteps,scientificTip,coachStatus;
 asset::WidgetId coachQuestion,coachReply,coachSteps;
 std::size_t keys=0,scientificButtons=0,coachButtons=0;
 for(const auto& widget:compiled.artifact.widgets){
  if(widget.name==project_calculator::display_widget){
   CHECK(widget.kind==asset::WidgetKind::text_input&&!widget.enabled&&!widget.focusable&&!widget.accepts_pointer);
   host.m_calculatorDisplay=widget.id;
  }
  if(widget.name=="CalculatorExpression"){
   CHECK(widget.kind==asset::WidgetKind::text_input&&widget.enabled&&widget.focusable);
   expression=widget.id;
  }
  if(widget.name=="CalculatorScientificResult"||widget.name=="CalculatorAngleStatus"){
   CHECK(widget.kind==asset::WidgetKind::text_input&&!widget.enabled&&!widget.read_only);
   (widget.name=="CalculatorScientificResult"?scientificResult:angleStatus)=widget.id;
  }
  if(widget.name=="CalculatorWorkedSteps"){
   CHECK(widget.kind==asset::WidgetKind::text_input&&!widget.enabled&&!widget.read_only&&widget.multiline);
   workedSteps=widget.id;
  }
  if(widget.name==project_calculator::help_tip_widget){
   CHECK(widget.kind==asset::WidgetKind::text_input&&!widget.enabled&&!widget.read_only&&widget.multiline);
   scientificTip=widget.id;
  }
  if(widget.name==project_calculator::coach_question_widget){
   CHECK(widget.kind==asset::WidgetKind::text_input&&widget.enabled&&widget.focusable&&!widget.multiline);
   coachQuestion=widget.id;
  }
  if(widget.name==project_calculator::coach_reply_widget)coachReply=widget.id;
  if(widget.name=="CalculatorCoachWorkedSteps")coachSteps=widget.id;
  if(widget.name==project_calculator::coach_status_widget){
   CHECK(widget.kind==asset::WidgetKind::text_input&&!widget.enabled&&!widget.read_only);
   coachStatus=widget.id;
  }
  if(widget.kind==asset::WidgetKind::button){
   CHECK(widget.focusable&&widget.accepts_pointer);
   if(project_calculator::find_key(widget.action))++keys;
   else if(widget.action=="calculator.coach.ask"||widget.action=="calculator.coach.stop")++coachButtons;
   else{
    CHECK(widget.action=="calculator.scientific.evaluate"||widget.action=="calculator.scientific.angle"
          ||widget.action=="calculator.scientific.next_tip"||widget.action=="calculator.scientific.run_example");
    ++scientificButtons;
   }
  }
 }
 CHECK(host.m_calculatorDisplay.value!=0u&&keys==project_calculator::keys.size());
 CHECK(expression.value&&scientificResult.value&&angleStatus.value&&workedSteps.value
       &&scientificTip.value&&scientificButtons==4&&coachStatus.value&&coachButtons==2);
 CHECK(runtime.focus(host.m_calculatorDisplay)==rt::RuntimeCode::disabled_widget);
 const auto context=std::make_shared<core::Context>();
 const auto press=[&](std::string_view action){
  const auto frame=runtime.build_frame({520,440});
  const auto button=std::find_if(frame.widgets.begin(),frame.widgets.end(),[&](const auto& widget){
   return compiled.artifact.widgets[widget.index].action==action;
  });
  if(button==frame.widgets.end())return false;
  const rt::Float2 center{button->bounds.x+button->bounds.width/2,button->bounds.y+button->bounds.height/2};
  if(!runtime.pointer_captured(center)||runtime.pointer_release(center)!=rt::RuntimeCode::ready)return false;
  host.frame_gui_application(context,.016f,{},false,{520,440},{});
  return true;
 };
 const auto expect=[&](std::string_view value){
  const auto frame=runtime.build_frame({520,440});
  const auto display=std::find_if(frame.widgets.begin(),frame.widgets.end(),[&](const auto& widget){
   return widget.id==host.m_calculatorDisplay;
  });
  return display!=frame.widgets.end()&&display->visible&&display->primary_text==value;
 };
 const auto sequence=[&](std::initializer_list<std::string_view> actions,std::string_view value){
  for(auto action:actions)if(!press("calculator."+std::string(action)))return false;
  return expect(value);
 };
 CHECK(expect("0"));
 CHECK(sequence({"1","2","decimal","5","add","3","decimal","5","equals"},"16"));
 CHECK(runtime.text(coachStatus)=="No Coach connected. See Reference for offline help.");
 CHECK(sequence({"multiply","2","equals","subtract","3","equals"},"29"));
 CHECK(sequence({"clear","sign","7","divide","2","equals"},"-3.5"));
 CHECK(sequence({"clear","5","divide","0","equals"},"Cannot divide by zero"));
 CHECK(sequence({"9","divide","4","equals"},"2.25"));
 CHECK(sequence({"clear","8","decimal","decimal","3","backspace","2","equals"},"8.2"));
 CHECK(context->clears==context->presents&&context->presents>0u);
 auto unknown=project_calculator::find_key("calculator.unknown");CHECK(!unknown);
 host.m_projectGuiAdapter->input={{.type=gui::EventType::KeyDown,.key=46},{.type=gui::EventType::TextInput,.text="1"},{.type=gui::EventType::TextInput,.text="2"},{.type=gui::EventType::TextInput,.text="+"},{.type=gui::EventType::TextInput,.text="3"},{.type=gui::EventType::KeyDown,.key=13},{.type=gui::EventType::TextInput,.text="\r"}};
 host.frame_gui_application(context,0.016f,{},false,{800,600},{20,30});CHECK(expect("15"));
 CHECK(host.m_projectGuiAdapter->origin.x==20 && host.m_projectGuiAdapter->origin.y==30);
 CHECK(!runtime.text({}));
 CHECK(runtime.text(angleStatus)=="Degrees");
 CHECK(runtime.set_text(expression,"2 + (3 * 4)^2")==rt::RuntimeCode::ready);
 CHECK(press("calculator.scientific.evaluate")&&runtime.text(scientificResult)=="146"&&expect("15"));
 CHECK(runtime.text(workedSteps)=="3 * 4 = 12\n(3 * 4)^2 = 144\n2 + (3 * 4)^2 = 146");
 CHECK(runtime.text(scientificTip)==project_calculator::scientific_tips.front().text);
 for(std::size_t i=1;i<=project_calculator::scientific_tips.size();++i){
  CHECK(press("calculator.scientific.next_tip"));
  CHECK(runtime.text(scientificTip)==project_calculator::scientific_tips[i%project_calculator::scientific_tips.size()].text);
  CHECK(runtime.text(scientificResult)=="146"&&runtime.text(angleStatus)=="Degrees");
  CHECK(runtime.text(expression)=="2 + (3 * 4)^2");
  CHECK(runtime.text(workedSteps)=="3 * 4 = 12\n(3 * 4)^2 = 144\n2 + (3 * 4)^2 = 146");
 }
 CHECK(press("calculator.scientific.run_example"));
 CHECK(runtime.text(expression)=="2^8"&&runtime.text(scientificResult)=="256"&&runtime.text(workedSteps)=="2^8 = 256");
 CHECK(expect("15")&&runtime.text(angleStatus)=="Degrees");
 CHECK(press("calculator.scientific.next_tip")&&press("calculator.scientific.next_tip"));
 CHECK(press("calculator.scientific.run_example"));
 CHECK(runtime.text(expression)=="sin(30)"&&runtime.text(scientificResult)=="0.5"&&runtime.text(workedSteps)=="sin(30) = 0.5");
 CHECK(press("calculator.scientific.angle")&&runtime.text(angleStatus)=="Radians");
 CHECK(press("calculator.scientific.run_example"));
 CHECK(runtime.text(expression)=="sin(pi/6)"&&runtime.text(scientificResult)=="0.5");
 CHECK(runtime.text(workedSteps)->ends_with("sin(pi/6) = 0.5")&&expect("15"));
 CHECK(press("calculator.scientific.angle")&&runtime.text(angleStatus)=="Degrees");
 CHECK(runtime.set_text(expression,"sin(30)")==rt::RuntimeCode::ready);
 CHECK(press("calculator.scientific.evaluate")&&runtime.text(scientificResult)=="0.5");
 CHECK(press("calculator.scientific.angle")&&runtime.text(angleStatus)=="Radians");
 CHECK(runtime.text(scientificResult)==""&&runtime.text(workedSteps)=="");
 CHECK(runtime.set_text(expression,"sin(pi/2)")==rt::RuntimeCode::ready);
 CHECK(press("calculator.scientific.evaluate")&&runtime.text(scientificResult)=="1");
 CHECK(runtime.text(workedSteps)=="pi/2 = 1.5707963267949\nsin(pi/2) = 1");
 CHECK(runtime.set_text(expression,"1/0")==rt::RuntimeCode::ready);
 CHECK(press("calculator.scientific.evaluate")&&runtime.text(scientificResult)=="Cannot divide by zero");
 CHECK(runtime.text(workedSteps)=="");
 CHECK(runtime.set_text(expression,"1+2+3+4+5+6")==rt::RuntimeCode::ready);
 CHECK(press("calculator.scientific.evaluate")&&runtime.text(scientificResult)=="21");
 CHECK(runtime.text(workedSteps)=="1+2 = 3\n3 steps omitted\n1+2+3+4+5+6 = 21");
 CHECK(runtime.set_text(expression,"0000000000000000000000000000000000000000001+2")==rt::RuntimeCode::ready);
 CHECK(press("calculator.scientific.evaluate")&&runtime.text(scientificResult)=="3");
 CHECK(runtime.text(workedSteps)->size()<=38u&&runtime.text(workedSteps)->ends_with("... = 3"));
 CHECK(expect("15"));

 const auto scientificFrame=runtime.build_frame({520,440});
 const auto coachTab=std::ranges::find(scientificFrame.tab_headers,"Coach",&rt::TabHeaderView::label);
 CHECK(coachTab!=scientificFrame.tab_headers.end());
 const auto tabBounds=coachTab->bounds;
 CHECK(runtime.pointer_release({tabBounds.x+tabBounds.width/2,tabBounds.y+tabBounds.height/2})
       ==rt::RuntimeCode::ready);
 CHECK(coachQuestion.value&&coachReply.value&&coachSteps.value);
 CHECK(runtime.set_text(coachQuestion,"How do I calculate a distance?")==rt::RuntimeCode::ready);
 CHECK(runtime.set_text(coachStatus,"Waiting for Ask")==rt::RuntimeCode::ready);
 CHECK(press("calculator.coach.ask"));
 CHECK(runtime.text(coachStatus)=="No Coach connected. See Reference for offline help.");
 CHECK(runtime.text(coachReply)==""&&runtime.text(coachSteps)==""&&expect("15"));

 return 0;
}
int main(){return check_calculator();}
'''


def consumer():
    play = (ROOT / 'Engine/src/epoch/implementation/epoch.project_play_scene.inl').read_text(encoding='utf-8')
    host = r'''
namespace core {
enum class ContextType { OpenGL, Software };
struct Context {
 ContextType type{ContextType::Software};
 unsigned clears{},presents{};
 void clear_safe(){++clears;}
 void present_safe(){++presents;}
};
}
namespace gui {
struct Vec2 {float x{},y{};};
enum class EventType {TextInput,KeyDown};
struct InputEvent{EventType type{};bool ctrl_down{},alt_down{};int key{};std::string text;};
void begin_frame(const std::shared_ptr<core::Context>&,float,Vec2,bool){}
void begin_window(std::string_view,Vec2,Vec2,bool,bool){}
void wrapped_label(std::string_view,float){}
void end_window(){}
void end_frame(){}
}
namespace project_gui_runtime=rt;
namespace project_gui {
enum class PointerDelivery { frame_snapshot, ordered };
enum class TextScaling { canvas, desktop };
}
struct GuiPaintPort {
 struct Result {std::vector<rt::RuntimeEvent>events;};
 std::vector<gui::InputEvent>input;
 rt::Float2 origin{};
 template<class Command>Result render(rt::RuntimeSession& session,rt::Float2,Command command,
     project_gui::PointerDelivery delivery,project_gui::TextScaling text,rt::Float2 viewportOrigin){
  origin=viewportOrigin;
  if(delivery!=project_gui::PointerDelivery::ordered||text!=project_gui::TextScaling::desktop)return {};
  for(const auto& event:input)command(event);
  input.clear();return {session.drain_events()};
 }
};
struct CalculatorHost {
 calculator_example::Application application;
 project::GuiApplication* m_guiApplication{&application};
 std::unique_ptr<rt::RuntimeSession>m_projectGuiRuntime;
 std::unique_ptr<GuiPaintPort>m_projectGuiAdapter{std::make_unique<GuiPaintPort>()};
 asset::WidgetId m_calculatorDisplay;
 std::string m_projectGuiStatus;
 void finish_runtime_screenshot(core::Context*){}
''' + '\n'.join(definition(play, signature) for signature in (
        '            static bool set_application_widget_text(',
        '            static bool get_application_widget_text(',
        '            void frame_gui_application(')) + '\n};\n'
    example = ROOT / 'Engine/examples/BasicSoftwareExampleCalculator'
    application = (example / 'source/project.gui_application.hpp').read_text(encoding='utf-8')
    for include, path in (
        ('<epoch.engine.hpp>', ROOT / 'Engine/include/epoch.engine.hpp'),
        ('"calculator.arithmetic.hpp"', example / 'source/calculator.arithmetic.hpp'),
        ('"calculator.coach.hpp"', example / 'source/calculator.coach.hpp'),
        ('"calculator.scientific.hpp"', example / 'source/calculator.scientific.hpp'),
    ):
        application = application.replace(include, f'"{path.as_posix()}"')
    application = application.replace('#pragma once', '')
    return (CONSUMER.replace("HOST_OWNERS", host).replace("APPLICATION_OWNER", application)
            .replace("GUI_SOURCE", (example / 'Assets/Gui/main.epochgui').as_posix()))


class CalculatorSoftware(unittest.TestCase):
    def test_software_open_selects_gui_workspace(self):
        from Tests.support.cpp import compile_and_run
        from Tests.platform.test_child_close_router import definition as block
        import tempfile
        owner=(ROOT/'Engine/src/editor/application/editor.application_project_documents.inl').read_text(encoding='utf-8')
        opening=block(owner,'        void set_project(')
        route=block(opening,'            if (profile->kind == EditorProjectKind::Tool)')
        source=r'''#include <optional>
enum class EditorProjectKind { Game, Tool, Editor };
enum class EditorMainSurface { Scene, Game2D };
enum class OutlinerToolTab { Scene, Gui };
namespace previewgrid { enum class CameraMode { Editor, Canvas2D }; }
struct Profile {EditorProjectKind kind;};
struct State {EditorMainSurface mainSurface{EditorMainSurface::Scene};std::optional<EditorMainSurface> requestedMainSurface;bool game2DGuiWorkspace{},workspaceCompanionsPending{};OutlinerToolTab outlinerToolTab{OutlinerToolTab::Scene};previewgrid::CameraMode viewportCameraMode{previewgrid::CameraMode::Editor};};
void open(State&state,const Profile*profile){ROUTE}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 for(auto kind:{EditorProjectKind::Tool,EditorProjectKind::Game,EditorProjectKind::Editor}){
  State state;Profile profile{kind};open(state,&profile);
  if(kind==EditorProjectKind::Tool){CHECK(state.mainSurface==EditorMainSurface::Game2D);CHECK(state.requestedMainSurface==EditorMainSurface::Game2D);CHECK(state.game2DGuiWorkspace&&state.workspaceCompanionsPending);CHECK(state.outlinerToolTab==OutlinerToolTab::Gui);CHECK(state.viewportCameraMode==previewgrid::CameraMode::Canvas2D);}
  else {CHECK(state.mainSurface==EditorMainSurface::Scene);CHECK(!state.requestedMainSurface&&!state.game2DGuiWorkspace);}
 }
}'''.replace('ROUTE',route)
        parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='software-workspace-',dir=parent) as temporary:
            directory=Path(temporary);cpp=directory/'contract.cpp';cpp.write_text(source,encoding='utf-8')
            compile_and_run(self,cpp,directory/'proof')
        viewport=(ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        types=viewport.split('constexpr std::string_view kinds[]{',1)[1].split('};',1)[0]
        self.assertEqual(types,'"Game", "Software / Tool", "Epoch Editor"')
        self.assertNotIn('Create Calculator Example', viewport)
        self.assertIn('createProject(projectKinds[selectedKind])',viewport)
        packages=(ROOT/'Engine/src/editor/application/editor.application_packages_creation.inl').read_text(encoding='utf-8')
        self.assertIn('if (kind != "tool") editor.requestedMainSurface = EditorMainSurface::Project;',packages)

    def test_actual_document_keypad_and_software_frame(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU module consumer")
        owners = [ROOT / name for name in (
            "Engine/src/editor/implementation/editor.project_shell_generation.inl",
            "Engine/src/epoch/implementation/epoch.project_play_scene.inl",
            "Engine/include/epoch.engine.hpp",
            "Engine/examples/BasicSoftwareExampleCalculator/source/calculator.arithmetic.hpp",
            "Engine/examples/BasicSoftwareExampleCalculator/source/calculator.coach.hpp",
            "Engine/examples/BasicSoftwareExampleCalculator/source/calculator.scientific.hpp",
            "Engine/examples/BasicSoftwareExampleCalculator/source/project.gui_application.hpp",
            "Engine/examples/BasicSoftwareExampleCalculator/Assets/Gui/main.epochgui")] + [Path(__file__)]
        run_gui_contract(self, "calculator-software", consumer(), owners,
                         "Saved basic and scientific controls, actual GUI compilation and frame dispatch, with a recording software context.")

if __name__ == "__main__":
    unittest.main()
