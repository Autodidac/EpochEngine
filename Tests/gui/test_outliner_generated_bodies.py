"""Execute the owning pane's body extent, search and focus routing without a GUI."""
from pathlib import Path
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
PANE = ROOT / 'Engine/src/editor/application/editor.frame_outliner.inl'


class GeneratedBodyOutliner(unittest.TestCase):
    def test_actual_bodies_share_the_hierarchy_scroll_and_keep_focus_routing(self):
        pane = PANE.read_text(encoding='utf-8')
        api = (ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
        declarations = '\n'.join(definition(api, '    export '+kind+' '+name, True).replace('export ', '', 1)
            for kind, name in [('struct', 'Vec2'), ('struct', 'WidgetBounds'),
                ('enum class', 'MenuItemIndicator'), ('struct', 'MenuItemOptions')])
        extent = pane[pane.index('        const bool planetaryObjectPresent='):
            pane.index('        if (editor.selectedEntityId != editor.worldHierarchyScrollSelection)')]
        bodies = pane[pane.index('        std::size_t generatedBodyRow = 0u;'):
            pane.index('\n\n        }\n        else if (editor.outlinerToolTab == OutlinerToolTab::Assets)')]
        self.assertNotIn('world-outliner-status-bodies', pane)
        self.assertLess(pane.index('        std::size_t generatedBodyRow = 0u;'),
            pane.index('        gui::end_scroll_area();', pane.index('        std::size_t generatedBodyRow = 0u;')))
        source = r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace gui_lib {template<class T>using CommandMenuItemsView=std::span<const T>;}
namespace gui {
enum class SemanticTone{neutral};
DECLARATIONS
Vec2 cursor{};std::vector<MenuItemOptions> drawn;std::vector<float> rowY;
std::string click;int ends{};
Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 p){cursor=p;}
float measure_wrapped_label_height(std::string_view,float){return 22.f;}
void label(std::string_view){cursor.y+=22.f;}
void wrapped_label(std::string_view,float){cursor.y+=22.f;}
bool menu_item(MenuItemOptions o){drawn.push_back(o);rowY.push_back(cursor.y);cursor.y+=26.f;return o.label==click;}
void end_scroll_area(){++ends;}
}
struct Body{std::uint32_t id;int parent;std::string name;};
struct Mesh{std::string artifact_key="accepted";};
struct Entity{bool visible=true;std::string category="Planetary Terrain";std::optional<Mesh> meshAsset=Mesh{};int sceneObjectId=7;};
struct Policy{std::vector<Body> generated_bodies;};
struct BodySelection{int entity;std::string mesh_key;std::uint32_t body;};
struct Hierarchy{void synchronize_selection(int) {}};
struct Editor{std::vector<Entity> entities{Entity{}};std::optional<Policy> planetaryMeshPolicy=Policy{};
 std::string worldHierarchyFilter,worldHierarchyStatus="Hierarchy synchronized.";int selectedEntityId=-1,worldHierarchyScrollSelection=-1;
 std::optional<BodySelection> planetaryBodySelection;Hierarchy worldHierarchy;};
bool identity=true,admitted=true;int focusCalls{},selectedBody=-1;
bool editor_planetary_stream_identity_matches(const Editor&,const Entity&){return identity;}
std::string_view editor_planetary_restore_progress(const Editor&){return {};}
bool select_editor_planetary_body(Editor& e,std::size_t i,std::uint32_t id){
 if(!admitted)return false;e.selectedEntityId=e.entities[i].sceneObjectId;
 e.planetaryBodySelection=BodySelection{e.selectedEntityId,"accepted",id};selectedBody=static_cast<int>(id);return true;}
void handle_scene_tool(Editor&,int*,std::string_view action){if(action=="focus_selection")++focusCalls;}
struct Context{int* get(){return nullptr;}};
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 Editor editor;Context ctx;
 for(int i=0;i<12;++i)editor.planetaryMeshPolicy->generated_bodies.push_back({static_cast<std::uint32_t>(i),i==11?0:-1,"Body "+std::to_string(i)});
 const auto containsNoCase=[](std::string_view text,std::string_view filter){return filter.empty()||text.find(filter)!=std::string_view::npos;};
 const float hierarchyRowHeight=26.f;const std::string_view hierarchyEmptyText="No objects";
 struct Snapshot{std::size_t visible_rows=8;} hierarchySnapshot;
 for(int scenario=0;scenario<5;++scenario){
  const gui::Vec2 pane_pos{10,20},pane_size{200,scenario==0?80.f:240.f};
  const float outlinerWidth=188.f;gui::cursor={10,54};
  editor.worldHierarchyFilter=scenario==1?"Body 11":"";
  if(scenario==3)editor.planetaryMeshPolicy->generated_bodies.clear();
  if(scenario==4)identity=false;
EXTENT
  CHECK(hierarchyViewportHeight==pane_pos.y+pane_size.y-54.f-4.f);
  if(scenario==0)CHECK(generatedBodyRows==13&&hierarchyContentHeight==8*26+13*26);
  if(scenario==1)CHECK(generatedBodyRows==2&&hierarchyContentHeight==8*26+2*26);
  if(scenario==3)CHECK(bodyInventoryHeight>0&&generatedBodyRows==0);
  const gui::Vec2 hierarchyOrigin{10,54-(hierarchyContentHeight-hierarchyViewportHeight)};
  gui::drawn.clear();gui::rowY.clear();gui::click="Body 11";admitted=scenario!=2;
  const int previousFocus=focusCalls;
BODIES
  CHECK(gui::ends==scenario+1);
  CHECK(gui::cursor.y<=pane_pos.y+pane_size.y-4.f+.01f);
  if(scenario<3){
   CHECK(!gui::drawn.empty()&&gui::drawn.back().label=="Body 11");
   CHECK(gui::rowY.back()+hierarchyRowHeight<=pane_pos.y+pane_size.y-4.f+.01f);
   CHECK(gui::drawn.back().shortcut=="Satellite");
   if(scenario==2){CHECK(focusCalls==previousFocus);CHECK(editor.worldHierarchyStatus.starts_with("Body selection refused"));}
   else {CHECK(focusCalls==previousFocus+1&&selectedBody==11);CHECK(editor.worldHierarchyScrollSelection==7);}
  }else CHECK(gui::drawn.empty());
 }
}
'''.replace('DECLARATIONS', declarations).replace('EXTENT', extent).replace('BODIES', bodies)
        report = ROOT/'Artifacts/Reports/outliner-generated-bodies'/str(time.time_ns())
        report.mkdir(parents=True)
        cpp = report/'consumer.cpp'
        cpp.write_text(source, encoding='utf-8')
        result = compile_and_run(self, cpp, report, windows_flags=('/W4', '/WX'))
        (report/'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
