"""Execute the real select adapter geometry and standalone EpochGui placement.

This compiles CPU-only production code with a controlled native extent; it does
not create a window, renderer, model or hidden runtime service.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tools.source.owned_source_text import read_owned_source


ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / 'Engine/dep/EpochGui'


class SelectPopupPlacement(unittest.TestCase):
    def test_actual_adapter_and_standalone_placement(self):
        select = read_owned_source(ROOT / 'Engine/src/epochgui/controls/gui.scroll_controls.inl')
        geometry = select.split('        const std::size_t visibleCount =', 1)[1].split(
            '        if (state.alignSelectedOnOpen)', 1)[0]
        geometry = '        const std::size_t visibleCount =' + geometry
        portable = (GUI / 'src/epochgui/popup_layout.cpp').read_text(encoding='utf-8')
        portable = portable.replace('module;', '', 1).replace('module epoch.gui;', '', 1)
        contract = (GUI / 'tests/popup_layout_tests.cpp').read_text(encoding='utf-8')
        engine = read_owned_source(ROOT / 'Engine/src/epochgui/gui.engine.cpp')
        ownership = 'struct SelectBoxState' + engine.split('struct SelectBoxState', 1)[1].split('        struct PendingSelectPopup', 1)[0]
        ownership += 'bool any_select_box_open(' + engine.split('bool any_select_box_open(', 1)[1].split('        [[nodiscard]] static bool is_select_box_list_id', 1)[0]
        lifecycle = read_owned_source(ROOT / 'Engine/src/epochgui/controls/gui.frame_lifecycle.inl')
        begin_visit = lifecycle.split('        for (auto& [key, state] : g_selectBoxStates) {', 1)[1].split('    static void render_pending_select_popups', 1)[0]
        begin_visit = 'for (auto& [key, state] : g_selectBoxStates) {' + begin_visit.rsplit('    }', 1)[0]
        end_visit = lifecycle.split('    void end_frame() noexcept', 1)[1].split('        for (auto& [key, state] : g_selectBoxStates) {', 1)[1].split('        render_pending_select_popups();', 1)[0]
        end_visit = 'for (auto& [key, state] : g_selectBoxStates) {' + end_visit
        toolbar = read_owned_source(ROOT / 'Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl')
        compact_bounds = 'for (std::size_t index = 0; index < topMenus.size(); ++index)' + toolbar.split('open_chrome_menu(chromeResult.selected_left);', 1)[1].split('for (std::size_t index = 0; index < topMenus.size(); ++index)', 1)[1].split('        const bool toolbarControlsBlockedByMenu', 1)[0]
        code = r'''
#include <unordered_map>
#include <string>
namespace ownership_test {
namespace core {struct Context {};}
struct SelectBoxState;
extern thread_local std::unordered_map<std::string, SelectBoxState> g_selectBoxStates;
OWNERSHIP
thread_local std::unordered_map<std::string, SelectBoxState> g_selectBoxStates;
struct {const core::Context* ctx{};} g_frame;
void begin_visit(const core::Context* rawCtx){BEGIN_VISIT}
void end_visit(){END_VISIT}
}

#include "gui/popup_layout.hpp"
#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <vector>
PORTABLE
#define EPOCHGUI_TEST_NO_MAIN 1
STANDALONE
namespace epochengine::gui_lib::input {
enum class ModalInputMode {inactive,bounded,blocked_until_clear};
}
namespace gui_lib=epochengine::gui_lib;
using Vec2=gui_lib::Vec2;
struct Context {
    int width=800,height=600;
    int get_width_safe()const{return width;}
    int get_height_safe()const{return height;}
};
struct Modal {
    gui_lib::input::ModalInputMode current=gui_lib::input::ModalInputMode::inactive;
    gui_lib::Rect rectangle{};
    auto mode()const{return current;}
    auto bounds()const{return rectangle;}
};
struct Frame {Context* ctx{};Modal modalInput;} g_frame;
constexpr float kFontScale=1.0f,kContentPadding=4.0f;
float measure_text_width(std::string_view text,float) {return static_cast<float>(text.size())*8.0f;}
struct Options {std::vector<std::string_view> options{"60 FPS","120 FPS","Unlimited"};std::size_t max_visible_options=3;bool fit_content{};};
struct Result {gui_lib::Rect rectangle;float pitch,contentHeight;};
Result actual_geometry(const Options& options,Vec2 start,float width=250.0f){
    const float rowHeight=28.0f,closedHeight=30.0f;
    const Vec2 afterClosedCursor{start.x,start.y+closedHeight+4.0f};
    const std::string key="cpu-owner";
GEOMETRY
    return {{listPos,{listWidth,listHeight}},optionPitch,contentHeight};
}
#define CHECK(...) do{if(!(__VA_ARGS__))return __LINE__;}while(false)
int main(){
    {using namespace ownership_test;
    core::Context first,second;
    g_selectBoxStates["first"]={&first,true,false,true};
    g_selectBoxStates["second"]={&second,false,false,true};
    CHECK(any_select_box_open(&first));
    CHECK(!any_select_box_open(&second));
    begin_visit(&second);ownership_test::g_frame.ctx=&second;end_visit();
    CHECK(g_selectBoxStates["first"].open&&g_selectBoxStates["first"].visited_this_frame);
    begin_visit(&first);ownership_test::g_frame.ctx=&first;end_visit();
    CHECK(!g_selectBoxStates["first"].open);
    }

    {
        struct Menu {float x{},width{92.0f};};
        std::vector<Menu> topMenus(2);
        struct {std::vector<gui_lib::Rect> left_bounds;gui_lib::Rect left_overflow;} chromeResult;
        chromeResult.left_bounds={{{0,0},{118,32}},{{118,0},{48,32}},{{0,0},{0,0}}};
        chromeResult.left_overflow={{166,0},{82,32}};
COMPACT_BOUNDS
        CHECK(topMenus[0].x==118&&topMenus[0].width==48);
        CHECK(topMenus[1].x==166&&topMenus[1].width==82);
    }
    CHECK(popup_layout_contract()==0);
    Context ctx;g_frame.ctx=&ctx;
    Options options;
    auto normal=actual_geometry(options,{20.0f,60.0f});
    CHECK(normal.rectangle.position.y==94.0f&&normal.rectangle.size.y==88.0f);
    CHECK(normal.contentHeight==88.0f&&normal.pitch==30.0f);
    options.fit_content=true;
    auto fitted=actual_geometry(options,{200.0f,60.0f},96.0f);
    CHECK(fitted.rectangle.size.x>96.0f);
    CHECK(fitted.rectangle.size.x>=measure_text_width("Unlimited",1.0f)+2*kContentPadding+22);
    options.fit_content=false;
    g_frame.modalInput.current=gui_lib::input::ModalInputMode::bounded;
    g_frame.modalInput.rectangle={{100.0f,100.0f},{300.0f,250.0f}};
    auto modal=actual_geometry(options,{120.0f,320.0f});
    CHECK(modal.rectangle.position.y<320.0f);
    CHECK(modal.rectangle.position.x>=104.0f&&modal.rectangle.position.y>=104.0f);
    CHECK(modal.rectangle.position.x+modal.rectangle.size.x<=396.0f);
    CHECK(modal.rectangle.position.y+modal.rectangle.size.y<=346.0f);
    // The final option's center is inside both render and modal input geometry.
    const Vec2 finalOption{modal.rectangle.position.x+20.0f,
        modal.rectangle.position.y+2.0f*modal.pitch+14.0f};
    CHECK(gui_lib::contains(modal.rectangle,finalOption));
    CHECK(gui_lib::contains(g_frame.modalInput.rectangle,finalOption));
    g_frame.modalInput.rectangle={{100.0f,100.0f},{140.0f,70.0f}};
    auto compressed=actual_geometry(options,{120.0f,130.0f});
    CHECK(compressed.rectangle.size.x==132.0f&&compressed.rectangle.size.y==62.0f);
    CHECK(compressed.contentHeight>compressed.rectangle.size.y+1.0f);
    CHECK(compressed.rectangle.position.x>=104.0f&&compressed.rectangle.position.y>=104.0f);
    CHECK(compressed.rectangle.position.x+compressed.rectangle.size.x<=236.0f);
    CHECK(compressed.rectangle.position.y+compressed.rectangle.size.y<=166.0f);
    // A partly offscreen modal is intersected with the actual native surface.
    g_frame.modalInput.rectangle={{700.0f,500.0f},{200.0f,200.0f}};
    auto edge=actual_geometry(options,{720.0f,540.0f});
    CHECK(edge.rectangle.position.x>=704.0f&&edge.rectangle.position.y>=504.0f);
    CHECK(edge.rectangle.position.x+edge.rectangle.size.x<=796.0f);
    CHECK(edge.rectangle.position.y+edge.rectangle.size.y<=596.0f);
    return 0;
}
'''.replace('COMPACT_BOUNDS', compact_bounds).replace('OWNERSHIP', ownership).replace('BEGIN_VISIT', begin_visit).replace('END_VISIT', end_visit).replace('PORTABLE', portable).replace('STANDALONE', contract).replace('GEOMETRY', geometry)
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='select-popup-placement-', dir=parent))
        try:
            source = scratch / 'contract.cpp'
            source.write_text(code, encoding='utf-8')
            if os.name == 'nt':
                toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = sorted((kitroot / 'Include').glob('*'))
                if not toolsets or not kits:
                    self.skipTest('Installed MSVC/SDK required')
                toolset, kit = toolsets[-1], kits[-1]
                binary = scratch / 'contract.exe'
                args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(GUI / 'include'), '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'),
                    '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'), '/Fo' + str(scratch / 'contract.obj'),
                    '/Fe' + str(binary), str(source), '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                binary = scratch / 'contract'
                args = [compiler, '-std=c++23', '-I' + str(GUI / 'include'), str(source), '-o', str(binary)]
            built = subprocess.run(args, cwd=scratch, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout[-3500:] + built.stderr)
            run = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            lines = code.splitlines()
            failure = lines[run.returncode - 1] if 0 < run.returncode <= len(lines) else ''
            self.assertEqual(run.returncode, 0, 'Actual popup geometry CPU contract: ' + failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_hit_and_deferred_draw_share_the_placed_rectangle(self):
        source = read_owned_source(ROOT / 'Engine/src/epochgui/controls/gui.scroll_controls.inl')
        select = source.split('    SelectBoxResult select_box(', 1)[1].split(
            '    static void draw_progress_bar_layout(', 1)[0]
        self.assertIn('gui_lib::place_popup(gui_lib::PopupOptions{', select)
        self.assertIn('listPos.x, listPos.y, listWidth, listHeight', select)
        self.assertIn('popup.position = listPos;', select)
        self.assertIn('popup.width = listWidth;', select)
        self.assertIn('popup.height = listHeight;', select)
        self.assertIn('const bool showScrollbar = contentHeight > listHeight + 1.0f;', select)


if __name__ == '__main__':
    unittest.main()
