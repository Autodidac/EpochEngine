"""Exercise the real GUI adapter's original-index geometry and event mapping."""
from pathlib import Path
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui import test_forest_preview_actions as harness

ROOT = Path(__file__).resolve().parents[2]


class ResponsiveToolTabs(unittest.TestCase):
    def test_original_routes_survive_overflow_and_selection(self):
        owner = (ROOT / 'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
        actual = '\n'.join(definition(owner, marker) for marker in ('    [[nodiscard]] static float tab_button_width(', '    float responsive_tab_bar_height(', '    TabBarResult responsive_tab_bar_buttons('))
        portable=(ROOT/'Engine/dep/EpochGui/modules/epoch.gui.ixx').read_text(encoding='utf-8')
        width_policy=definition(portable,'    struct ResponsiveTabWidthOptions',True)+'\n'+definition(portable,'    [[nodiscard]] inline float resolve_responsive_tab_width(')
        source = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace gui_lib {
struct ResponsiveTabStripLayout {bool valid=true,overflowed=true;std::vector<std::uint32_t> visible_indices{1,3},overflow_indices{0,2,4};float visible_width=120,overflow_width=100;};
inline ResponsiveTabStripLayout forced;
WIDTH_POLICY
struct Input {std::span<const float> item_widths;std::size_t active_index;float available_width,gap,overflow_width;};
ResponsiveTabStripLayout make_responsive_tab_strip_layout(Input){return forced;}
}
namespace gui {
struct Vec2 {float x{},y{};};
struct TabButtonSpec {std::string_view id,label;float width{};bool active{},closable{},dirty{},enabled=true;};
enum class TabBarPresentation {Document,Workbench};
struct DragSurfaceResult{Vec2 delta;bool pressed{},dragging{},released{};};
struct TabDragOptions{void* state{};std::string_view active_id;};
struct TabBarResult {struct ItemGeometry {Vec2 position,size;};std::optional<std::size_t> pressed_index,selected_index,closed_index;std::vector<ItemGeometry> items;std::optional<std::size_t> dragged_index;DragSurfaceResult drag;Vec2 drag_origin;};
struct ResponsiveTabBarOptions {std::string_view overflow_id="overflow",overflow_label="Tools";std::span<const TabButtonSpec> tabs;float available_width=300,overflow_width=80,height=30,gap=1;TabBarPresentation presentation{};bool keyboard_navigation{};TabDragOptions drag;bool wrap{};};
struct SelectBoxOptions {std::string_view id,placeholder,selected;std::span<const std::string_view> options;Vec2 size;float row_height;unsigned max_visible_options;};
struct SelectBoxResult {bool changed{};std::optional<std::size_t> selected_index;};
inline constexpr float kFontScale=1,kContentPadding=8,kBoxInnerPadding=5;
inline struct {Vec2 cursor{10,20};} g_frame;
inline TabBarResult next;inline SelectBoxResult pick;inline std::vector<std::string_view> shown;inline unsigned disabledButtons;
float measure_text_width(std::string_view s,float){return float(s.size()*8)+.25f;}
float base_line_height(float){return 12;}
Vec2 cursor_position(){return g_frame.cursor;}void set_cursor(Vec2 p){g_frame.cursor=p;}
std::optional<std::size_t> navigate_tab_keys(std::span<const TabButtonSpec>,Vec2,Vec2,Vec2,bool){return {};}
TabBarResult tab_bar_buttons(std::span<const TabButtonSpec> tabs,float,float,TabBarPresentation,Vec2,const TabDragOptions&){auto result=next;for(std::size_t i=0;i<tabs.size();++i)result.items.push_back({{float(10+i*60),20},{59,30}});return result;}
bool button_with_state(std::string_view,Vec2,bool,bool enabled){assert(!enabled);++disabledButtons;return false;}
SelectBoxResult select_box(const SelectBoxOptions& o){shown.assign(o.options.begin(),o.options.end());return pick;}
ACTUAL
}
int main(){using namespace gui;
std::vector<TabButtonSpec> tabs{{"a","A"},{"b","B"},{"c","C"},{"d","D"},{"e","E"}};
tabs[2].enabled=false;ResponsiveTabBarOptions options;options.tabs=tabs;
next.pressed_index=1;next.selected_index=0;next.closed_index=1;next.dragged_index=1;next.drag.released=true;next.drag_origin={25,20};
auto r=responsive_tab_bar_buttons(options);
assert(r.items.size()==5&&r.items[0].size.x==0&&r.items[2].size.x==0&&r.items[4].size.x==0);
assert(r.items[1].position.x==10&&r.items[3].position.x==70);
assert(r.pressed_index==3&&r.selected_index==1&&r.closed_index==3);
assert(r.dragged_index==3&&r.drag.released&&r.drag_origin.x==25);
assert((shown==std::vector<std::string_view>{"A","E"}));
next={};pick={true,1};r=responsive_tab_bar_buttons(options);assert(r.selected_index==4);
pick={};tabs[0].enabled=false;tabs[4].enabled=false;options.tabs=tabs;r=responsive_tab_bar_buttons(options);assert(disabledButtons==1&&!r.selected_index);
gui_lib::forced.overflowed=false;next.pressed_index=4;r=responsive_tab_bar_buttons(options);assert(r.items.size()==5&&r.pressed_index==4);
options.tabs={};r=responsive_tab_bar_buttons(options);assert(r.items.empty()&&!r.pressed_index);
TabButtonSpec longTitle{"long","A complete long title that cannot be clipped",32,false,true,true};
for(const auto presentation:{TabBarPresentation::Document,TabBarPresentation::Workbench}){
 const float width=tab_button_width(longTitle,presentation);
 const float padding=presentation==TabBarPresentation::Workbench?24:2*kContentPadding;
 assert(width>=measure_text_width(longTitle.label,1)+padding+18+8&&std::floor(width)==width);
}
options.tabs=tabs;options.wrap=true;options.available_width=120;next={};
assert(responsive_tab_bar_height(options)==5*30+4*1);
r=responsive_tab_bar_buttons(options);assert(r.items.size()==5);
for(const auto& item:r.items)assert(item.size.x>0);
}
'''.replace('ACTUAL', actual).replace('WIDTH_POLICY',width_policy)
        harness.ForestPreviewActions()._compile_and_run(source)


if __name__ == '__main__':
    unittest.main()
