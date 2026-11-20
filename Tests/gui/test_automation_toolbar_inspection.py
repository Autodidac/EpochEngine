"""Actual toolbar dispatch under the existing inspection scope; no native GUI."""
from pathlib import Path
import json
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class AutomationToolbarInspection(unittest.TestCase):
    def test_command_menu_toggle_and_routes(self):
        folder=ROOT/'Engine/src/editor/application'
        self.assertEqual((folder/'editor.application_state.inl').read_text(encoding='utf-8').count('bool showCommandMenu{ true };'), 1)
        self.assertEqual((ROOT/'Engine/modules/editor/editor.core.ixx').read_text(encoding='utf-8').count('bool show_command_menu{ true };'), 1)
        snapshot=(folder/'editor.application_context_snapshot.inl').read_text(encoding='utf-8')
        self.assertEqual(snapshot.count('snapshot.show_command_menu = editor.showCommandMenu;'), 1)
        self.assertEqual(snapshot.count('editor.showCommandMenu = snapshot.show_command_menu;'), 1)
        source=(ROOT/'Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl').read_text()
        self.assertIn('.icon = editor.showCommandMenu ? gui::ButtonIcon::chevron_left : gui::ButtonIcon::chevron_right,', source)
        start=source.index('        if (commandToggle.selected_index && !menusBlockedByModal)')
        toggle=source[start:source.index('        gui::ChromeBarResult updateChromeResult',start)]
        route=definition(source, '        const auto chrome_menu_route =')
        geometry=source[source.index('        float commandVisibleRight ='):source.index('        const auto commandToggle =')]
        preferred=source[source.index('        float desiredMenuWidth ='):source.index('        auto visibleStatusItems =')]
        budget=source[source.index('        const float namedMenuWidth ='):source.index('        const gui::ChromeBarResult chromeResult =')]
        code=r'''#include <algorithm>
#include <vector>
#include <optional>
#include <array>
#include <cstddef>
#include <string_view>
namespace gui {struct Vec2{float x,y;};struct WidgetBounds{Vec2 position,size;};}
enum class TopMenu{None,File,Edit};
int main(){
 constexpr float toolbarHorizontalPadding=12,toolbarItemGap=4,toolbarToggleInset=4;
 struct ChromeItem{float preferred_width;std::string_view label="Menu";};
 for (const float availableMenuWidth:{548.f,640.f,700.f}) {
  std::vector<ChromeItem> chromeLeft{{68},{68},{76},{92},{76},{76},{72}};
  const ChromeItem applicationLabel{118,"Epoch"};
  PREFERRED
  BUDGET
  if(desiredMenuWidth!=576 || showApplicationLabel!=(availableMenuWidth==700))return 10;
  if(chromeLeft.size()!=7+firstMenuIndex || chromeLeft.back().preferred_width!=72)return 11;
  if(commandMenuWidth!=(availableMenuWidth==700?698:std::min(576.f,availableMenuWidth)))return 12;
 }
 for(float scale:{1.f,1.5f,2.f}){gui::Vec2 toolbar_pos{0,0};float commandMenuWidth=720*scale;
 struct{std::vector<gui::WidgetBounds>left_bounds,center_bounds;gui::WidgetBounds left_overflow{},center_overflow{};}chromeResult;
 chromeResult.left_bounds.push_back({{0,0},{500*scale,32}});chromeResult.left_overflow={{510*scale,0},{80*scale,32}};
 GEOMETRY
 if(commandToggleX+toolbarToggleInset!=590*scale+toolbarItemGap)return 20;
 }
 struct{bool showCommandMenu=true;TopMenu openMenu=TopMenu::File,suppressedMenuHover=TopMenu::Edit;}editor;
 struct{bool scene_input_captured=false;}result;
 struct{std::optional<std::size_t>selected_index;}commandToggle{0};
 bool menusBlockedByModal=false;
 struct Item{TopMenu menu;};std::array<Item,2>topMenus{{{TopMenu::File},{TopMenu::Edit}}};
 for(int i=0;i<2;++i){TOGGLE
 if(editor.showCommandMenu!=(i==1)||editor.openMenu!=TopMenu::None||!result.scene_input_captured)return 1;
 }
 menusBlockedByModal=true;TOGGLE
 if(!editor.showCommandMenu)return 2;
 std::size_t firstMenuIndex=1;
 ROUTE;
 menusBlockedByModal=false;commandToggle.selected_index.reset();
 if(chrome_menu_route(1)!=TopMenu::File||chrome_menu_route(2)!=TopMenu::Edit||chrome_menu_route(0)!=TopMenu::None||chrome_menu_route(3)!=TopMenu::None)return 3;
 firstMenuIndex=0;
 if(chrome_menu_route(0)!=TopMenu::File||chrome_menu_route(1)!=TopMenu::Edit||chrome_menu_route(2)!=TopMenu::None)return 6;
 editor.showCommandMenu=false;if(chrome_menu_route(1)!=TopMenu::None)return 4;
 editor.showCommandMenu=true;commandToggle.selected_index=0;if(chrome_menu_route(1)!=TopMenu::None)return 5;
 return 0;}
'''.replace('TOGGLE',toggle).replace('ROUTE',route).replace('GEOMETRY',geometry).replace('PREFERRED',preferred).replace('BUDGET',budget)
        report=ROOT/'Artifacts/Reports/command-menu-collapse'/str(time.time_ns());report.mkdir(parents=True)
        unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_camera_readout_semantics(self):
        source=(ROOT/'Engine/src/editor/application/editor.application_scripting_renderers.inl').read_text(encoding='utf-8')
        owner=definition(source,'        [[nodiscard]] std::string preview_zoom_text(')
        code=r'''#include <memory>
#include <string>
#include <format>
#include <cmath>
namespace core{struct Context{};}
namespace previewgrid{enum class CameraMode{Editor,FPS};enum class EditorView{Perspective,Front};
CameraMode mode=CameraMode::Editor;EditorView view=EditorView::Perspective;double distance=20756706;
CameraMode camera_mode_for(void*){return mode;}EditorView editor_view_for(void*){return view;}double camera_distance_for(void*){return distance;}}
namespace epochengine{template<class...T>std::string format_text(std::format_string<T...> f,T&&...args){return std::format(f,std::forward<T>(args)...);}}
OWNER
int main(){auto ctx=std::make_shared<core::Context>();
if(preview_zoom_text(ctx)!="Distance 20.8M units")return 1;
previewgrid::view=previewgrid::EditorView::Front;previewgrid::distance=1200;if(preview_zoom_text(ctx)!="Span 1.2k units")return 2;
previewgrid::mode=previewgrid::CameraMode::FPS;if(preview_zoom_text(ctx)!="Free camera")return 3;
if(preview_zoom_text({})!="No camera")return 4;return 0;}
'''.replace('OWNER',owner)
        report=ROOT/'Artifacts/Reports/camera-readout'/str(time.time_ns());report.mkdir(parents=True)
        unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_header_overflow_and_short_dock(self):
        chrome=(ROOT/'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
        start=chrome.index('        const auto largestZone =')
        measured=chrome[start:chrome.index('        const gui_lib::ChromeBarLayout layout =',start)]
        viewport=(ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        start=viewport.index('            if (meshDrawSecondRow && scene_size.y <')
        guard=viewport[start:viewport.index('            const auto previewControlWidth',start)]
        self.assertIn('.overflow_width = measuredOverflowWidth,',chrome)
        width_start=viewport.index('            const auto previewControlWidth =')
        width_helper=viewport[width_start:viewport.index('            const float resetButtonWidth',width_start)]
        self.assertIn('.closed_label = viewControlLabel',viewport)
        self.assertIn('"Move: Select", "Move: Free", "Move: X", "Move: Y", "Move: Z"',viewport)
        self.assertIn('inspectingLod ? "Exit Inspect" : "Reset"',viewport)

        code=r'''#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>
float glyph=8;float kFontScale=1,kContentPadding=6;
float measure_text_width(std::string_view s,float){return s.size()*glyph;}
namespace gui {struct SegmentedButtonSpec{std::string_view label;};
float segmented_button_row_width(const std::array<SegmentedButtonSpec,1>&m){return measure_text_width(m[0].label,1)+16;}}
int main(){
WIDTH_HELPER
for(float scale:{8.f,14.f,24.f}){glyph=scale;
 for(auto text:{"Reset","Exit Inspect","Move: Select","View: Orthographic"})
  if(previewControlWidth(text)<measure_text_width(text,1)+20)return 3;
}
for(float scale:{8.f,14.f,24.f})for(unsigned count:{1u,7u,12u}){
glyph=scale;struct{std::vector<int>left_items,center_items,right_items;float overflow_width=82;}options;
options.left_items.resize(count);
MEASURE
if(measuredOverflowWidth<96||measuredOverflowWidth<measure_text_width(overflowMeasure,1)+38)return 1;
}
for(float headerControlHeight:{26.f,32.f,48.f,68.f})for(float height:{24.f,30.f,47.f,48.f,100.f,200.f}){const float headerRowStep=headerControlHeight+6;struct{float y;}scene_size{height};bool showMeshDrawMode=true,meshDrawSecondRow=true;
GUARD
if(showMeshDrawMode!=(height>=2+headerRowStep+headerControlHeight+4)||meshDrawSecondRow!=showMeshDrawMode)return 2;
}return 0;}
'''.replace('MEASURE',measured).replace('GUARD',guard).replace('WIDTH_HELPER',width_helper)
        report=ROOT/'Artifacts/Reports/header-overflow'/str(time.time_ns());report.mkdir(parents=True)
        unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_optional_polling_and_context_snapshot(self):
        folder=ROOT/'Engine/src/editor/application'
        source=(folder/'editor.frame_bootstrap.inl').read_text()
        start=source.index('        const unsigned int telemetryDetail =')
        gates=source[start:source.index('        if (editor.showTelemetry)',start)]
        start=source.index('            const auto toolbarActivity =')
        tasks=source[start:source.index(';',start)+1]
        start=source.index('            const auto toolbarResources =')
        polling=source[start:source.index('            const double toolbarCpuFraction',start)]
        source=(folder/'editor.application_context_snapshot.inl').read_text()
        start=source.index('        snapshot.show_telemetry =')
        capture=source[start:source.index(';',source.index('        snapshot.show_epoch_resource_usage =',start))+1]
        start=source.index('        editor.showTelemetry =')
        restore=source[start:source.index(';',source.index('        editor.showEpochResourceUsage =',start))+1]
        code=r'''
#include <algorithm>
#include <cstdlib>
#define CHECK(...) do{if(!(__VA_ARGS__))std::exit(EXIT_FAILURE);}while(false)
namespace platform::work_admission {
unsigned hostCalls{},epochCalls{};
struct ResourceSample{};struct EpochResourceSample{int gpu{};};
ResourceSample sample_host_resources(){++hostCalls;return {};}
EpochResourceSample sample_epoch_resources(){++epochCalls;return {};}
}
namespace editor_tasks {struct ActivitySnapshot{};ActivitySnapshot activity_snapshot(int){return {};}}
int main(){for(unsigned stage=0;stage<5;++stage)for(bool epochUsage:{false,true}){
 struct{bool showTelemetry=true;unsigned telemetryDetail{};bool showEpochResourceUsage{};}editor{stage!=4,stage,epochUsage};
 struct{bool show_telemetry=true;unsigned telemetry_detail{};bool show_epoch_resource_usage{};}snapshot;
 CAPTURE
 editor={};RESTORE
 CHECK(editor.showTelemetry==(stage!=4)&&editor.telemetryDetail==stage&&editor.showEpochResourceUsage==epochUsage);
 GATES
 unsigned taskCalls{};const auto observe_frame_tasks=[&]{++taskCalls;return 0;};
 TASKS
 unsigned gpuCalls{};const auto observe_frame_gpu=[&]{++gpuCalls;return 0;};
 using namespace platform::work_admission;hostCalls=epochCalls=0;
 POLLING
 CHECK(hostCalls==(stage<3&&!epochUsage?1u:0u));
 CHECK(epochCalls==(stage<3&&epochUsage?1u:0u));
 CHECK(gpuCalls==(stage<3&&!epochUsage?1u:0u));
 CHECK(taskCalls==(stage<2?1u:0u));
}}
'''.replace('CAPTURE',capture).replace('RESTORE',restore).replace('GATES',gates).replace('POLLING',polling).replace('TASKS',tasks)
        report=ROOT/'Artifacts/Reports/telemetry-stage-polling'/str(time.time_ns())
        report.mkdir(parents=True);unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_stage_codec_and_legacy_boolean(self):
        from Tests.gui.test_theme_persistence import contract_source
        code = contract_source()
        code = code[:code.index('int main(')] + r'''
#define VERIFY(...) do{if(!(__VA_ARGS__))return 91;}while(false)
int main(){
 for(unsigned stage=0;stage<5;++stage)for(bool scope:{false,true}){
  EditorState state;state.telemetryDetail=stage;state.showTelemetry=stage!=4;state.showEpochResourceUsage=scope;
  const auto encoded=serialize_editor_layout_preferences(state);EditorState restored;
  VERIFY(decode_editor_layout_preferences(restored,encoded));
  VERIFY(restored.telemetryDetail==stage&&restored.showTelemetry==(stage!=4));
  VERIFY(restored.showEpochResourceUsage==scope);
  auto v14=encoded;const auto key=v14.find("telemetry_epoch_scope ");
  v14.erase(key,v14.find('\n',key)-key+1);
  v14.replace(0,v14.find('\n'),"EPOCH_EDITOR_LAYOUT 14");
  EditorState prior; prior.showEpochResourceUsage=true;
  VERIFY(decode_editor_layout_preferences(prior,v14)&&prior.telemetryDetail==stage&&!prior.showEpochResourceUsage);
  auto old=v14;const auto b=old.find("telemetry_detail "),e=old.find('\n',b);old.erase(b,e-b+1);
  old.replace(0,old.find('\n'),"EPOCH_EDITOR_LAYOUT 12");EditorState legacy;
  VERIFY(decode_editor_layout_preferences(legacy,old));
  VERIFY(legacy.telemetryDetail==(stage==4?4u:0u)&&legacy.showTelemetry==(stage!=4));
  if(stage!=2){
   auto v13=v14;v13.replace(0,v13.find('\n'),"EPOCH_EDITOR_LAYOUT 13");
   const auto at=v13.find("telemetry_detail ");
   v13.replace(at,v13.find('\n',at)-at,"telemetry_detail "+std::to_string(stage>=3?stage-1:stage));
   EditorState migrated;VERIFY(decode_editor_layout_preferences(migrated,v13));
   VERIFY(migrated.telemetryDetail==stage&&migrated.showTelemetry==(stage!=4));
  }
  auto invalid=encoded;invalid.replace(invalid.find("telemetry_detail "),18,"telemetry_detail 8");
  VERIFY(!decode_editor_layout_preferences(restored,invalid));
  auto invalid_scope=encoded;invalid_scope[key+22]='2';
  VERIFY(!decode_editor_layout_preferences(restored,invalid_scope)&&restored.showEpochResourceUsage==scope);
 }
}
'''
        report = ROOT/'Artifacts/Reports/telemetry-stage-codec'/str(time.time_ns())
        report.mkdir(parents=True);unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_running_worker_labels_exclude_external_activity(self):
        source=(ROOT/'Engine/src/editor/application/editor.frame_bootstrap.inl').read_text(encoding='utf-8')
        begin=source.index('epochengine::format_text("Total {} / Ready {} / Queued {}"')
        end=source.index('),',begin)+1
        expression=source[begin:end]
        compact=source.index('toolbarCompactStatus = {{')
        compact_begin=source.index('epochengine::format_text("Total {} / Ready {} / Queued {}"',compact)
        compact_end=source.index('),',compact_begin)+1
        self.assertEqual(expression,source[compact_begin:compact_end])
        automation=(ROOT/'Engine/dep/EpochSelfCoding/integration/fragments/editor.private_editor_engine_workspace_surface_585d30cf.inl').read_text(encoding='utf-8')
        summary=automation.index('gui::property_row("All work", ')
        summary_begin=automation.index('epochengine::format_text(',summary)
        summary_end=automation.index('),',summary_begin)+1
        summary_expression=automation[summary_begin:summary_end]
        code=r"""
#include <format>
#include <string>
#include <cstdlib>
namespace epochengine {template<class... T>std::string format_text(std::format_string<T...> fmt,T&&...args){return std::format(fmt,std::forward<T>(args)...);}}
int main(){
 struct {unsigned worker_count=4,running_tasks=0,idle_workers=4;} toolbarActivity;
 unsigned toolbarQueued=0;
 const unsigned toolbarRunning=1; // A separate model is active; no pool worker is added.
 auto label=[&]{return EXPR;};
 auto summary_label=[&]{
  const auto& taskActivity=toolbarActivity;
  const auto runningTasks=taskActivity.running_tasks,queuedTasks=toolbarQueued;
  return SUMMARY_EXPR;
 };
 if(label()!="Total 0 / Ready 4 / Queued 0"||summary_label()!=label())return 1;
 toolbarActivity.running_tasks=2;toolbarActivity.idle_workers=2;toolbarQueued=3;
 if(label()!="Total 2 / Ready 2 / Queued 3"||summary_label()!=label())return 2;
}
""".replace('SUMMARY_EXPR',summary_expression).replace('EXPR',expression)
        report=ROOT/'Artifacts/Reports/toolbar-running-workers'/str(time.time_ns())
        report.mkdir(parents=True);unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_five_stage_membership_and_narrow_drop(self):
        source = (ROOT / 'Engine/src/editor/application/editor.frame_toolbar_dock_targets.inl').read_text()
        arrow = source.split('.icon = editor.showTelemetry ?',1)[1].split(',',1)[0]
        selection = source[source.index('        auto visibleStatusItems ='):source.index('        const float minimumStatusWidth =')]
        drop = source[source.index('        constexpr std::array<std::size_t, 10> statusDropOrder'):source.index('        const auto visibleStatus =')]
        code = r'''
#include <array>
#include <span>
#include <string_view>
#include <algorithm>
#include <cstdlib>
#define CHECK(x) do{if(!(x))std::exit(EXIT_FAILURE);}while(false)
namespace gui {enum class ButtonIcon{chevron_left,chevron_right};struct Options{std::span<int>items;};float status_strip_minimum_width(Options o){return o.items.size()*10.0f;}}
int main(){for(unsigned telemetryDetail=0;telemetryDetail<5;++telemetryDetail){
 struct{bool showTelemetry;}editor{telemetryDetail!=4};
 CHECK((editor.showTelemetry ? ARROW)==(telemetryDetail==4?gui::ButtonIcon::chevron_left:gui::ButtonIcon::chevron_right));
 std::array<int,11>toolbarStatusItems{0,1,2,3,4,5,6,7,8,9,10};
 constexpr std::size_t toolbarScopeIndex=4,toolbarContextIndex=5,toolbarFpsIndex=6,toolbarTaskIndex=7,toolbarVersionIndex=8,toolbarBuildIndex=9,toolbarTierIndex=10;
 SELECT
 CHECK(visibleStatusCount==std::array<unsigned,5>{11,8,5,2,0}[telemetryDetail]);
 if(telemetryDetail==1)for(unsigned i=0;i<8;++i)CHECK(visibleStatusIndices[i]==i);
 if(telemetryDetail==3)CHECK(visibleStatusIndices[0]==5&&visibleStatusIndices[1]==6);
 const float statusBudget=10;
 DROP
 CHECK(visibleStatusCount==(telemetryDetail==4?0u:1u));
 if(visibleStatusCount)CHECK(visibleStatusIndices[0]==(telemetryDetail==2?0u:toolbarContextIndex));
}}
'''.replace('ARROW',arrow).replace('SELECT', selection).replace('DROP', drop).replace('#define CHECK(x)', '#define CHECK(...)').replace('if(!(x))','if(!(__VA_ARGS__))')
        report = ROOT/'Artifacts/Reports/telemetry-stages'/str(time.time_ns())
        report.mkdir(parents=True)
        unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps(records,indent=2))

    def test_status_scope_and_collapse_preserve_automation_lock(self):
        folder = ROOT / 'Engine/src/editor/application'
        source = (folder / 'editor.frame_toolbar_dock_targets.inl').read_text()
        body = source[source.index('        const auto statusResult ='):source.index('        const float tab_y =')]
        private_marker = '// EPOCH_' + 'PRIVATE_SOURCE_'
        private_start = body.index(private_marker + 'BEGIN:')
        private_end = body.index(private_marker + 'END:', private_start)
        body = body[:private_start] + body[body.index('\n', private_end):]
        scope = 'template<class Draw>\n' + definition(
            (folder / 'editor.application_context_session.inl').read_text(),
            '        decltype(auto) render_automation_inspection(')
        code = r'''
#include <array>
#include <algorithm>
#include <optional>
#include <string_view>
#include <cstdlib>
#define CHECK(x) do {if(!(x)) std::exit(EXIT_FAILURE);} while(false)
namespace gui {
bool blocked{}; int requested=-1; bool collapse{}; unsigned calls{};
struct ScopedInputSuppression { ScopedInputSuppression(bool=true,bool=true){blocked=true;} ~ScopedInputSuppression(){blocked=false;} };
struct Vec2{float x{},y{};};
struct StatusStripOptions{std::array<int,11> items{};Vec2 position{};float available_width{},item_height{},horizontal_padding{};bool align_right{},single_row{};};
struct StatusStripResult {std::optional<std::size_t> selected_index;};
StatusStripResult status_strip(StatusStripOptions options) {
 CHECK(options.position.y==8 && options.item_height==32);
 const bool toggle=calls++%2==1;
 if(blocked)return {};
 if(toggle){CHECK(options.position.x==12 && options.horizontal_padding==4);return collapse?StatusStripResult{0u}:StatusStripResult{};}
 return requested>=0?StatusStripResult{static_cast<std::size_t>(requested)}:StatusStripResult{};
}
}
SCOPE
enum class EditorMainSurface{AISandbox,Systems};unsigned navigations{};
void open_editor_surface(EditorMainSurface,std::string_view){++navigations;}
struct Editor{bool showTelemetry=true,showEpochResourceUsage=false;unsigned int telemetryDetail{};} editor;
void frame(bool busy,bool inspect,bool menusBlockedByModal=false,bool toolbarControlsBlockedByMenu=false){
 std::optional<gui::ScopedInputSuppression> automationBackgroundInput;
 if(busy)automationBackgroundInput.emplace();
 const bool automationInputReserved=busy;
 const auto canInspectAutomation=[&]{return inspect;};
 const std::array<int,11>visibleStatus{},telemetryToggleItems{};
 const std::array<std::size_t,11>visibleStatusIndices{0,1,2,3,4,5,6,7,8,9,10};
 constexpr std::size_t visibleStatusCount=11,toolbarTaskIndex=7,toolbarScopeIndex=4;
 constexpr float toolbarStatusLeft=0,telemetryToggleWidth=20,toolbar_button_y=8,toolbar_button_h=32,toolbarStatusContentWidth=500;
 constexpr float toolbarHorizontalPadding=12,toolbarToggleInset=4;
 const gui::Vec2 toolbar_pos{};struct{bool scene_input_captured{};}result;
 BODY
 CHECK(gui::blocked==busy); // Both widgets retire their temporary input scope.
}
int main(){
 for(int item=0;item<5;++item){editor={};gui::requested=item;gui::calls=0;frame(true,true);CHECK(editor.showEpochResourceUsage&&navigations==0);}
 for(int item=5;item<11;++item){gui::requested=item;gui::calls=0;frame(true,true);CHECK(navigations==0);}
 gui::requested=7;gui::calls=0;frame(false,true);CHECK(navigations==1);
 for(int blocked=0;blocked<3;++blocked){editor={};gui::requested=0;gui::calls=0;
  frame(true,blocked!=0,blocked==1,blocked==2);CHECK(!editor.showEpochResourceUsage&&editor.showTelemetry);}
 gui::requested=-1;gui::collapse=true;gui::calls=0;editor={};frame(true,true);CHECK(editor.showTelemetry&&editor.telemetryDetail==1u);
 gui::calls=0;frame(true,true);CHECK(editor.showTelemetry&&editor.telemetryDetail==2u);
 gui::calls=0;frame(true,true);CHECK(editor.showTelemetry&&editor.telemetryDetail==3u);
 gui::calls=0;frame(true,true);CHECK(!editor.showTelemetry&&editor.telemetryDetail==4u);
 gui::calls=1;frame(true,true);CHECK(editor.showTelemetry&&editor.telemetryDetail==0u); // Hidden strip does not submit a widget.
 CHECK(!gui::blocked);
}
'''.replace('SCOPE', scope).replace('BODY', body)
        report = ROOT/'Artifacts/Reports/automation-toolbar-inspection'/str(time.time_ns())
        report.mkdir(parents=True)
        unit=report/'contract.cpp';unit.write_text(code)
        records=compile_and_run(self,unit,report)
        (report/'result.json').write_text(json.dumps({'passed':True,'records':records,
            'scope':'Actual toolbar actions and inspection wrapper with explicit GUI/input ports; no native acceptance'},indent=2))


if __name__ == '__main__':
    unittest.main()
