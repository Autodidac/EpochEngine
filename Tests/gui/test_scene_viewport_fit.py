"""Compile the actual Editor scene sizing and transparent 2D header draw blocks."""
from pathlib import Path
import re
import unittest
from Tests.gui import test_forest_preview_actions as harness

ROOT = Path(__file__).resolve().parents[2]


class SceneViewportFit(unittest.TestCase):
    def test_disabled_preview_erases_old_scene_and_resized_panel_pixels(self):
        viewport = (ROOT / 'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        begin = viewport.index('            // None stops scene submissions')
        actual = viewport[begin:viewport.index('            // Ordinary scenes run', begin)]
        source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
namespace core {enum class ScenePreviewMode{None,Editor};}
struct Editor {bool covered{};core::ScenePreviewMode previewMode{core::ScenePreviewMode::Editor};};
bool editor_project_viewport_input_reserved(const Editor&e){return e.covered;}
namespace gui {
struct Vec2{float x{},y{};};struct Bounds{Vec2 position,size;};
inline std::array<int,320*200> pixels;inline unsigned draws;
void panel_rect(Vec2 p,Vec2 s){++draws;for(int y=int(p.y);y<int(p.y+s.y);++y)
 for(int x=int(p.x);x<int(p.x+s.x);++x)pixels[y*320+x]=7;}
}
void paint(Editor editor,gui::Bounds area,bool sceneFits=true){
struct{gui::Bounds scene_viewport;}result{area};
ACTUAL
}
int main(){
// Old scene=1, stale Properties=2. Child occupies only the old left area
// during a resize. Every newly exposed pixel must have deliberate backing.
for(bool covered:{false,true})for(auto mode:{core::ScenePreviewMode::None,core::ScenePreviewMode::Editor}){
 gui::pixels.fill(2);gui::draws=0;
 for(int y=20;y<150;++y)for(int x=10;x<100;++x)gui::pixels[y*320+x]=1;
 paint({covered,mode},{{10,20},{250,130}});
 const bool opaque=covered||mode==core::ScenePreviewMode::None;
 assert(gui::draws==unsigned(opaque));
 for(int y=20;y<150;++y)for(int x=10;x<260;++x)
  assert(gui::pixels[y*320+x]==(opaque?7:x<100?1:2));
 assert(gui::pixels[0]==2&&gui::pixels[150*320+10]==2);
 assert(mode==core::ScenePreviewMode::None||mode==core::ScenePreviewMode::Editor);
}
gui::draws=0;paint({true},{{10,20},{0,0}},false);assert(!gui::draws);
// After Stop, the retained enabled mode leaves the viewport for the scene pass.
gui::draws=0;paint({},{{10,20},{250,130}});assert(!gui::draws);
}
'''.replace('ACTUAL', actual)
        harness.ForestPreviewActions()._compile_and_run(source)

    def test_tab_backing_clears_old_controls_and_stays_before_scene(self):
        viewport = (ROOT / 'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        backing = viewport[viewport.index('            // The transparent scene window'):viewport.index('            const std::array modeTabs')]
        owner = (ROOT / 'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        inner = re.search(r'constexpr float\s+kBoxInnerPadding\s*=\s*([0-9.]+)f', owner).group(1)
        gap = re.search(r'constexpr float\s+kContentPadding\s*=\s*([0-9.]+)f', owner).group(1)
        source = r'''
#include <algorithm>
#include <cassert>
namespace gui {
struct Vec2{float x{},y{};};inline float advance;inline Vec2 panelPosition,panelSize;
float line_height(){return advance;}Vec2 cursor_position(){return {35,110};}
void panel_rect(Vec2 position,Vec2 size){panelPosition=position;panelSize=size;}
}
int main(){for(float glyphHeight:{12.0f,18.0f,30.0f,50.0f})for(float lineGap:{0.0f,4.0f,8.0f}){
const gui::Vec2 viewport_pos{31,71},viewport_size{320,900};bool guiDocumentWorkspace=false;
gui::advance=glyphHeight+lineGap;
ACTUAL
const float actualRowAdvance=std::max(31.0f,glyphHeight+2.0f*INNER)+GAP;
assert(gui::panelPosition.x==viewport_pos.x&&gui::panelSize.x==viewport_size.x);
assert(gui::panelSize.y>=actualRowAdvance);
// Any conservative overdraw ends within the following instruction band,
// which is painted before the scene; it cannot cover scene pixels.
assert(gui::panelSize.y<=actualRowAdvance+gui::line_height()+8.0f);
}}
'''.replace('ACTUAL', backing).replace('INNER', inner+'f').replace('GAP', gap+'f')
        harness.ForestPreviewActions()._compile_and_run(source)

    def test_scene_never_extends_beyond_compressed_dock(self):
        viewport = (ROOT / 'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        sizing = viewport[viewport.index('            const std::string_view sceneTitle ='):viewport.index('            editor.planetaryViewportHeight =')]
        source = r'''
#include <algorithm>
#include <cassert>
#include <string_view>
namespace gui {
struct Vec2{float x{},y{};};
struct WidgetBounds{Vec2 position,size;};
inline Vec2 cursor;inline unsigned draws;
Vec2 cursor_position(){return cursor;}
float line_height(){return 20;}
float titled_window_total_height(float){return 50;}
WidgetBounds scene_viewport(std::string_view,Vec2 position,Vec2 size){++draws;return {position,size};}
}
enum class EditorMainSurface{Scene,Game2D,ForestFactory};
std::string_view main_surface_title(EditorMainSurface){return "Scene";}
float timeline_compact_controls_height(float width){return width<300?180.0f:120.0f;}
void check(float width,float height,float x,float y,EditorMainSurface surface){
struct{EditorMainSurface mainSurface;bool game2DGuiWorkspace{};}editor{surface};
struct{gui::WidgetBounds scene_viewport;}result;
const gui::Vec2 viewport_pos{31,71},viewport_size{width,height};const bool projectViewportMaximized=false;
gui::cursor={viewport_pos.x+x,viewport_pos.y+y};gui::draws=0;
ACTUAL
assert(scene_pos.x>=viewport_pos.x&&scene_pos.y>=viewport_pos.y);
assert(scene_pos.x+scene_size.x<=viewport_pos.x+width+0.001f);
assert(scene_pos.y+scene_size.y+timelineStripHeight<=viewport_pos.y+height+0.001f);
assert(scene_size.x>=0&&scene_size.y>=0);
assert(result.scene_viewport.position.x>=viewport_pos.x&&result.scene_viewport.position.y>=viewport_pos.y);
assert(result.scene_viewport.position.x+result.scene_viewport.size.x<=viewport_pos.x+width+0.001f);
assert(result.scene_viewport.position.y+result.scene_viewport.size.y<=viewport_pos.y+height+0.001f);
if(!sceneFits){assert(gui::draws==0&&result.scene_viewport.size.x==0&&result.scene_viewport.size.y==0);}
else assert(gui::draws==1);
if(surface==EditorMainSurface::ForestFactory)assert(!showSceneTimeline);
if(showSceneTimeline)assert(scene_size.y>=sceneMinHeightWithTimeline);
}
int main(){for(float w:{0.0f,48.0f,160.0f,320.0f,900.0f})
for(float h:{0.0f,24.0f,48.0f,71.0f,99.0f,320.0f,900.0f})
for(float x:{-10.0f,0.0f,8.0f,950.0f})for(float y:{-10.0f,0.0f,40.0f,180.0f,950.0f})
for(auto s:{EditorMainSurface::Scene,EditorMainSurface::Game2D,EditorMainSurface::ForestFactory})check(w,h,x,y,s);}
'''.replace('ACTUAL', sizing)
        harness.ForestPreviewActions()._compile_and_run(source)

    def test_tilemap_instruction_has_backing_without_covering_scene(self):
        viewport = (ROOT / 'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
        instruction = viewport[viewport.index('            const gui::Vec2 instructionPos ='):viewport.index('        if (active_center_uses_scene)')]
        # This fragment includes the closing brace of the enclosing workspace branch.
        source = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <string_view>
#include <vector>
namespace gui {
struct Vec2{float x{},y{};};struct Rect{Vec2 position,size;};
inline Vec2 cursor;inline std::vector<Rect> panels;inline bool requireBacking;
Vec2 cursor_position(){return cursor;}
float measure_wrapped_label_height(std::string_view text,float width){return 20*std::max(1.0f,std::ceil(text.size()*8.0f/width));}
void panel_rect(Vec2 p,Vec2 size){panels.push_back({p,size});}
void wrapped_label(std::string_view text,float width){if(!requireBacking)return;
assert(panels.size()==1);const auto& panel=panels[0];
assert(panel.position.y<=cursor.y&&panel.position.y+panel.size.y>=cursor.y+measure_wrapped_label_height(text,width));}
void set_cursor(Vec2 p){cursor=p;}
}
void check(float width,bool guiDocumentWorkspace){
const gui::Vec2 viewport_pos{31,71},viewport_size{width,900};
const bool active_center_uses_scene=!guiDocumentWorkspace;
gui::cursor={35,110};gui::panels.clear();gui::requireBacking=active_center_uses_scene;
{
ACTUAL
if(active_center_uses_scene){assert(gui::panels.size()==1);const auto& panel=gui::panels[0];
assert(panel.position.x==viewport_pos.x&&panel.size.x==width);
assert(panel.position.y+panel.size.y==gui::cursor.y);}
else assert(gui::panels.empty());
}
int main(){for(float width:{160.0f,320.0f,900.0f})for(bool overlay:{false,true})check(width,overlay);}
'''.replace('ACTUAL', instruction)
        harness.ForestPreviewActions()._compile_and_run(source)


if __name__ == '__main__':
    unittest.main()
