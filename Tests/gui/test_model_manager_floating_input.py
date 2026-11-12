"""Real floating normalizer, queued GUI scope and Editor early input reservation."""
from pathlib import Path
import hashlib
import json
import subprocess
import time
import unittest

from Tests.support.cpp import configure_arguments
from Tests.gui.test_gui_queued_drag import fixture as queued_fixture
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    api = (ROOT/'Engine/modules/gui/gui.engine.ixx').read_text(encoding='utf-8')
    frame = (ROOT/'Engine/src/epochgui/controls/gui.frame_lifecycle.inl').read_text(encoding='utf-8')
    windows = (ROOT/'Engine/src/epochgui/controls/gui.window_controls.inl').read_text(encoding='utf-8')
    adapters = (ROOT/'Engine/src/epochgui/gui.docking_adapter.cpp').read_text(encoding='utf-8')
    bootstrap = (ROOT/'Engine/src/editor/application/editor.frame_bootstrap.inl').read_text(encoding='utf-8')
    pane = (ROOT/'Engine/src/editor/application/editor.application_pane_routing.inl').read_text(encoding='utf-8')
    base = queued_fixture().rsplit('int main(){',1)[0].replace('import epoch.gui.input;', 'import epoch.gui.input;\nimport epoch.gui;')
    types = '\n'.join(definition(api,marker,True).replace('export ','') for marker in (
        '    export struct FloatingWindowState','    export struct FloatingWindowOptions','    export class ScopedInputSuppression'))
    scope = frame[frame.index('    ScopedInputSuppression::ScopedInputSuppression('):frame.index('    Vec2 mouse_position()')]
    conversions = '\n'.join(definition(adapters,marker) for marker in (
        '    [[nodiscard]] gui_lib::Vec2 to_lib(Vec2', '    [[nodiscard]] Vec2 from_lib(gui_lib::Vec2',
        '    [[nodiscard]] gui_lib::FloatingWindowState to_lib(', '    void from_lib(const gui_lib::FloatingWindowState&'))
    facade = '\n'.join(definition(windows,marker) for marker in (
        '    [[nodiscard]] static gui_lib::FloatingWindowOptions floating_window_core_options(', '    void normalize_floating_window('))
    mouse = '\n'.join(definition(frame,marker) for marker in ('    Vec2 mouse_position()', '    bool is_mouse_over_content_clip()'))
    geometry = bootstrap[bootstrap.index('        const gui::Vec2 aiModelManagerDefaultSize'):bootstrap.index('        const core::RoutedPanelDockDragProjection')]
    capture = bootstrap[bootstrap.index('        if (modalVisible)\n'):bootstrap.index('        if (editor.outlinerContextMenu.menu.popup.open\n')]
    reserve = bootstrap[bootstrap.index('        std::optional<gui::ScopedInputSuppression> aiModelManagerBackgroundInput;'):bootstrap.index('        // A host resize must not reinterpret')]
    point = definition(pane,'        [[nodiscard]] static bool editor_point_in_rect(')
    return base + '\nnamespace gui {\n'+types+'\n'+scope+'\n'+conversions+'''
constexpr float kTitleScale=1.07f,kTitleBarPadding=6,kContentPadding=6;
float line_advance_amount(float){return 21;} // Caption metric port; geometry is the real portable normalizer.
'''+facade+'\n'+mouse+'\n}\n'+point+r'''
enum class TopMenu {None,File};
struct EditorState {
 bool showAiModelManagerModal=true,aiModelManagerOpenLogged{};
 gui::FloatingWindowState aiModelManagerWindow;
 TopMenu openMenu{};
 struct MenuOwner {struct Menu {struct Popup {bool open{};} popup;} menu;} outlinerContextMenu;
};
struct Observation {bool scene_input_captured{},covered{},background_press{},foreground_press{};int background_wheel{},foreground_wheel{};gui::FloatingWindowOptions options;};
Observation route(EditorState& editor,const std::shared_ptr<core::Context>& ctx,float w,float h,bool modalVisible=false,bool otherPointerGrabActive=false){
 Observation result;
GEOMETRY
CAPTURE
 result.covered=aiModelManagerInputCovered;result.options=aiModelManagerWindowOptions;
RESERVE
 result.background_press=gui::mouse_press_position().has_value();result.background_wheel=gui::g_frame.mouseWheelDelta;
 aiModelManagerBackgroundInput.reset(); // Same hook at the real floating owner's frame entry.
 result.foreground_press=gui::mouse_press_position().has_value();result.foreground_wheel=gui::g_frame.mouseWheelDelta;
 return result;
}
void screen(const std::shared_ptr<core::Context>& ctx,std::initializer_list<gui::InputEvent> events,bool down=false,gui::Vec2 final={}){
 fresh(ctx);tick(ctx,events,down,final);gui::g_frame.insideWindow=false;
}
int main(){
 auto ctx=std::make_shared<core::Context>();EditorState editor;
 screen(ctx,{event(gui::EventType::MouseDown,400,300),gui::InputEvent{.type=gui::EventType::MouseWheel,.mouse_pos={400,300},.wheel_delta=3}});
 auto result=route(editor,ctx,800,600);
 CHECK(editor.aiModelManagerWindow.initialized&&result.covered&&result.scene_input_captured);
 CHECK(!result.background_press&&result.foreground_press&&result.background_wheel==0&&result.foreground_wheel==3);
 CHECK(result.options.capture_input&&result.options.constrain_to_viewport);
 CHECK(editor.aiModelManagerWindow.position.x>=0&&editor.aiModelManagerWindow.position.y>=0);
 CHECK(editor.aiModelManagerWindow.position.x+editor.aiModelManagerWindow.size.x<=800);
 CHECK(editor.aiModelManagerWindow.position.y+editor.aiModelManagerWindow.size.y<=600);
 screen(ctx,{event(gui::EventType::MouseDown,1,1),gui::InputEvent{.type=gui::EventType::MouseWheel,.mouse_pos={1,1},.wheel_delta=2}});
 result=route(editor,ctx,800,600);
 CHECK(!result.covered&&!result.scene_input_captured&&result.background_press&&result.foreground_press);
 CHECK(!result.options.capture_input&&result.background_wheel==2&&result.foreground_wheel==2);
 // Queued press origin, not final pointer position, reserves the actual owner.
 screen(ctx,{event(gui::EventType::MouseDown,400,300),event(gui::EventType::MouseMove,1,1)},true,{1,1});
 result=route(editor,ctx,800,600);CHECK(result.covered&&!result.background_press&&result.foreground_press);
 editor.aiModelManagerWindow.dragging=true;
 screen(ctx,{event(gui::EventType::MouseMove,-40,-40)},true,{-40,-40});
 result=route(editor,ctx,800,600);CHECK(result.covered);
 editor.aiModelManagerWindow.dragging=false;
 screen(ctx,{event(gui::EventType::MouseMove,400,300)},true,{400,300});
 result=route(editor,ctx,800,600,false,true);CHECK(!result.covered); // A foreign pane/divider capture keeps its gesture.
 screen(ctx,{event(gui::EventType::MouseDown,400,300)});
 gui::g_frame.modalInput.begin({{0,0},{800,600}});gui::g_frame.modalInput.require_owner_scope();
 editor.aiModelManagerWindow.dragging=true;
 result=route(editor,ctx,800,600,true);
 CHECK(!result.covered&&!editor.aiModelManagerWindow.dragging&&!result.options.capture_input);
 CHECK(!gui::g_frame.modalInput.owner_input_allowed()); // Genuine modal ownership survives.
 gui::g_frame.modalInput.clear();
 editor.aiModelManagerWindow.dragging=true;editor.aiModelManagerWindow.resizing=true;
 ctx->focus=false;tick(ctx,{});gui::g_frame.insideWindow=false;
 result=route(editor,ctx,800,600);
 CHECK(!editor.aiModelManagerWindow.dragging&&!editor.aiModelManagerWindow.resizing&&!result.covered);
 ctx->focus=true;
 editor.aiModelManagerWindow.position={700,500};editor.aiModelManagerWindow.size={1200,900};
 screen(ctx,{});result=route(editor,ctx,320,180);
 CHECK(editor.aiModelManagerWindow.size.x==320&&editor.aiModelManagerWindow.size.y==180);
 CHECK(editor.aiModelManagerWindow.position.x==0&&editor.aiModelManagerWindow.position.y==0);
 namespace lib=epochengine::gui_lib;
 lib::FloatingWindowState legacy;legacy.position={900,600};legacy.size={1000,800};legacy.initialized=true;
 lib::FloatingWindowOptions options{.default_size={600,400},.min_size={440,320},.viewport_size={600,400}};
 lib::normalize_floating_window(legacy,options);
 CHECK(legacy.size.x==1000&&legacy.size.y==800&&legacy.position.x==552&&legacy.position.y==352); // Default legacy behavior unchanged.
 options.constrain_to_viewport=true;lib::normalize_floating_window(legacy,options);
 CHECK(legacy.size.x==600&&legacy.size.y==400&&legacy.position.x==0&&legacy.position.y==0);
 options.viewport_size={800,600};options.default_size={500,400};options.default_position={100,50};
 lib::FloatingWindowState state;
 auto layout=lib::update_floating_window(state,options,{});
 CHECK(layout.close_button.position.x+layout.close_button.size.x<=800&&layout.close_button.position.y+layout.close_button.size.y<=600);
 layout=lib::update_floating_window(state,options,{{state.position.x+state.size.x-1,state.position.y+state.size.y-1},true,true,false});
 CHECK(state.resizing);
 layout=lib::update_floating_window(state,options,{{4000,4000},true,false,false});
 CHECK(state.position.x>=0&&state.position.y>=0&&state.position.x+state.size.x<=800&&state.position.y+state.size.y<=600);
 (void)lib::update_floating_window(state,options,{{4000,4000},false,false,true});
 CHECK(!state.dragging&&!state.resizing);
 options.viewport_size={180,140};lib::normalize_floating_window(state,options);
 CHECK(state.size.x==180&&state.size.y==140&&state.position.x==0&&state.position.y==0);
 layout=lib::update_floating_window(state,options,{{40,10},true,true,false});CHECK(state.dragging);
 layout=lib::update_floating_window(state,options,{{-400,-500},true,false,false});
 CHECK(state.position.x==0&&state.position.y==0);
 return 0;
}
'''.replace('GEOMETRY',geometry).replace('CAPTURE',capture).replace('RESERVE',reserve)


class ModelManagerFloatingInput(unittest.TestCase):
    def test_real_normalizer_and_early_input_reservation(self):
        owner=(ROOT/'Engine/src/editor/application/editor.frame_model_manager.inl').read_text(encoding='utf-8')
        self.assertLess(owner.index('aiModelManagerBackgroundInput.reset();'),owner.index('if (editor.showAiModelManagerModal)'))
        self.assertIn('editor.aiModelManagerWindow,\n                aiModelManagerWindowOptions)',owner)
        self.assertIn('if (!modal_visible_now())',owner)
        self.assertNotIn('editor.openMenu = TopMenu::None;',owner)
        report=ROOT/'Artifacts/Reports/model-manager-floating-input-oct29'/str(time.time_ns())
        report.mkdir(parents=True)
        source=report/'consumer.cpp';source.write_text(fixture(),encoding='utf-8')
        portable=ROOT/'Engine/dep/EpochGui'
        (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(ModelManagerFloatingInput LANGUAGES CXX)
if(MSVC)
 set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG:NONE /INCREMENTAL:NO")
endif()
set(BUILD_TESTING OFF CACHE BOOL "Only the floating-input owner contract" FORCE)
add_subdirectory("{portable.as_posix()}" gui-dependency)
add_executable(floating_input "{source.as_posix()}")
target_link_libraries(floating_input PRIVATE EpochGui)
target_compile_features(floating_input PRIVATE cxx_std_23)
if(MSVC)
 target_compile_options(EpochGui PRIVATE /utf-8 /UNDEBUG)
 target_compile_options(floating_input PRIVATE /W3 /WX /utf-8 /UNDEBUG)
else()
 target_compile_options(EpochGui PRIVATE -UNDEBUG)
 target_compile_options(floating_input PRIVATE -UNDEBUG)
endif()
enable_testing()
add_test(NAME real_floating_input COMMAND floating_input)
set_tests_properties(real_floating_input PROPERTIES TIMEOUT 10)
''',encoding='utf-8')
        build=report/'build';commands=[configure_arguments(report,build)]
        for config in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',config,'--parallel','1'],
                ['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']]
        results=[]
        for i,command in enumerate(commands):
            run=subprocess.run(command,cwd=report,capture_output=True,text=True,timeout=120)
            log=report/f'lane-{i}.log';log.write_text(run.stdout+run.stderr,encoding='utf-8')
            results.append({'command':command,'exit':run.returncode,'log':str(log)})
            (report/'result.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
            self.assertEqual(run.returncode,0,run.stdout[-7000:]+run.stderr)


if __name__=='__main__':unittest.main()
