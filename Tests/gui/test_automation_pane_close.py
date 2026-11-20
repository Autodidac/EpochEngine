"""Execute the actual dock-tab owner with recording GUI ports; no Editor launch."""
from pathlib import Path
import json
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class AutomationPaneClose(unittest.TestCase):
    def test_only_ordinary_pane_close_is_admitted_during_work(self):
        folder = ROOT / 'Engine/src/editor/application'
        owner = definition((folder / 'editor.frame_viewport.inl').read_text(),
                           '        auto render_tool_group_tabs =', True)
        scope = 'template<class Draw>\n' + definition(
            (folder / 'editor.application_context_session.inl').read_text(),
            '        decltype(auto) render_automation_inspection(')
        fallback = definition((folder / 'editor.frame_bootstrap.inl').read_text(),
                              '        auto active_pane_for_region =', True)
        code = r'''
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <span>
#include <algorithm>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) std::exit(EXIT_FAILURE); } while(false)
namespace gui {
bool blocked{}; int requested=-1; bool injectProtected{};
struct ScopedInputSuppression { ScopedInputSuppression(bool=true,bool=true){blocked=true;} ~ScopedInputSuppression(){blocked=false;} };
struct Vec2 {float x{},y{};};struct Rect{Vec2 position,size;};
struct TabButtonSpec{std::string_view id,label;float width{};bool active{},closable{},dirty{},enabled{};};
struct TabDragOptions{int* state{};std::string_view active_id;float threshold{};};
enum class TabBarPresentation{Document};
struct ResponsiveTabBarOptions{std::span<const TabButtonSpec> tabs;float available_width{},height{},gap{};TabBarPresentation presentation{};bool keyboard_navigation{};TabDragOptions drag{};bool wrap{};};
struct TabBarResult {std::vector<Rect> items;std::optional<std::size_t> dragged_index,pressed_index,selected_index,closed_index;struct{bool pressed{};}drag;};
float responsive_tab_bar_height(const ResponsiveTabBarOptions&){return 30;}
void begin_window(std::string_view,Vec2,Vec2){}void set_cursor(Vec2){}void end_window(){}
bool reserved{},assistantWorking{};
TabBarResult responsive_tab_bar_buttons(const ResponsiveTabBarOptions&o){
 if(reserved){CHECK(!o.keyboard_navigation&&!o.drag.state);}
 CHECK(o.tabs[0].closable&&o.tabs[1].closable==!assistantWorking);
 TabBarResult r;r.items.resize(o.tabs.size());if(blocked)return r;
 // Deliberately emit selection/drag as well: the Editor must reject these while busy.
 r.pressed_index=r.selected_index=r.dragged_index=0;r.drag.pressed=true;
 if(requested>=0&&(o.tabs[requested].closable||injectProtected))r.closed_index=requested;
 return r;
}
}
SCOPE
enum class EditorToolDockRegion{Bottom};enum class TopMenu{None,File};
unsigned selections{},drags{},closes{},outputRequests{},messages{};std::string closedRoute;
void run(bool busy,bool inspect,bool menu,int request,bool inject=false,bool settings=false,bool assistant=false){
 gui::reserved=busy;gui::requested=request;gui::injectProtected=inject;
 gui::assistantWorking=assistant;
 selections=drags=closes=outputRequests=messages=0;closedRoute.clear();
 const bool automationInputReserved=busy;
 std::optional<gui::ScopedInputSuppression> automationBackgroundInput;
 if(busy)automationBackgroundInput.emplace();
 const auto canInspectAutomation=[&]{return inspect;};
 struct {TopMenu openMenu;bool paneTitleDragActive{};std::string paneTitleDragRoute;int paneTitleDrag{};}editor{menu?TopMenu::File:TopMenu::None};
 const int chat=0;
 const auto editor_assistant_work_active=[&](const auto&,int){return assistant;};
 struct{bool scene_input_captured{};}result;
 std::array<std::string_view,2>toolPaneRoutes{"pane.output","pane.ai_chat"},toolPaneLabels{"Output","Assistant Chat"};
 if(settings)toolPaneRoutes[0]="pane.world_settings";
 std::array<float,1>toolGroupTabHeights{};float toolGroupTabHeight=30;
 std::array<std::vector<std::string_view>,1>directTabRoutes;
 std::array<std::vector<gui::Rect>,1>directTabBounds;std::array<gui::Rect,1>directTabStripBounds;
 std::array<bool,2>shown{true,true};std::string activeRoute="pane.output";
 auto region_has_visible_pane=[](auto){return true;};
 auto active_pane_storage=[&](auto)->std::string&{return activeRoute;};
 auto pane_available=[&](auto route){return route==toolPaneRoutes[0]?shown[0]:route==toolPaneRoutes[1]&&shown[1];};
 auto pane_dock_region=[](auto){return EditorToolDockRegion::Bottom;};
 auto ordered_pane_indices_for_region=[&](auto){std::vector<std::size_t>v;for(std::size_t i=0;i<2;++i)if(shown[i])v.push_back(i);return v;};
 FALLBACK
 auto tool_group_position=[](auto){return gui::Vec2{};};auto tool_group_size=[](auto){return gui::Vec2{400,300};};
 auto modal_visible_now=[&]{return !inspect;};auto floating_gui_visible=[] {return false;};
 auto reset_pane_title_drag=[]{};auto tool_dock_region_index=[](auto){return 0u;};
 auto set_active_pane=[](auto,auto){++selections;};auto set_pane_shown=[&](auto route,bool visible){CHECK(!visible);shown[route==toolPaneRoutes[0]?0:1]=visible;++closes;closedRoute=route;};
 auto show_project_run_output=[] {++outputRequests;};
 auto push_editor_log=[](auto&,auto){++messages;};auto tool_pane_label=[](auto route){return route;};
 auto capture_pane_title_drag=[](auto,auto,const auto&){++drags;};
 OWNER
 render_tool_group_tabs(EditorToolDockRegion::Bottom);
 CHECK(gui::blocked==busy);
 if(closes&&closedRoute=="pane.output")CHECK(active_pane_for_region(EditorToolDockRegion::Bottom)=="pane.ai_chat");
}
int main(){
 run(true,true,false,0);CHECK(closes==1&&closedRoute=="pane.output"&&!selections&&!drags);
 run(true,true,false,1);CHECK(closes==1&&closedRoute=="pane.ai_chat"&&!selections&&!drags);
 run(true,true,false,1,false,false,true);CHECK(!closes&&!selections&&!drags);
 run(true,true,false,1,true,false,true);CHECK(!closes); // Dispatch also rejects a stale protected close.
 run(false,true,false,1,true,false,true);CHECK(!closes);
 run(true,true,false,-1);CHECK(!closes&&!selections&&!drags);
 run(true,false,false,0);CHECK(!closes);
 run(true,true,true,0);CHECK(!closes);
 run(false,true,false,1);CHECK(closes==1&&closedRoute=="pane.ai_chat"&&selections==1&&drags==1);
 run(true,true,false,-1,false,true);CHECK(outputRequests==1&&messages==1&&!selections);
 run(true,false,false,-1,false,true);CHECK(!outputRequests&&!messages);
 CHECK(!gui::blocked);
}
'''.replace('SCOPE', scope).replace('OWNER', owner).replace('FALLBACK', fallback)
        report = ROOT / 'Artifacts/Reports/automation-pane-close' / str(time.time_ns())
        report.mkdir(parents=True)
        unit = report / 'contract.cpp'
        unit.write_text(code)
        records = compile_and_run(self, unit, report)
        (report / 'result.json').write_text(json.dumps({'passed': True, 'records': records,
            'scope': 'Actual dock-tab owner and inspection scope; recording ports, native acceptance pending'}, indent=2))


if __name__ == '__main__':
    unittest.main()
