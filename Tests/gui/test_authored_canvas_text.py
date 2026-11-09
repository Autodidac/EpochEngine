"""Actual canvas drawing branches and scoped GUI state; no renderer starts."""
from pathlib import Path
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui import test_forest_preview_actions as harness

ROOT = Path(__file__).resolve().parents[2]


class AuthoredCanvasText(unittest.TestCase):
    def test_scaled_text_container_chrome_and_clip_restore(self):
        api = (ROOT / 'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
        gui = (ROOT / 'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
        canvas = (ROOT / 'Engine/src/editor/application/editor.application_gui_authoring.inl').read_text(encoding='utf-8')
        scopes = '\n'.join(definition(api, marker, True).replace('export ', '') for marker in (
            '    export class ScopedTextScale', '    export class ContentClipScope'))
        methods = '\n'.join(definition(gui, marker) for marker in (
            '    ContentClipScope::ContentClipScope(', '    ContentClipScope::~ContentClipScope()',
            '    ScopedTextScale::ScopedTextScale(', '    ScopedTextScale::~ScopedTextScale()'))
        branch = canvas[canvas.index('            else if (widget.descriptor.kind == WidgetKind::panel'):canvas.index('            if (selected || gui_widget_selected')]
        branch = branch.replace('else if', 'if', 1)
        scale_variable = next(line for line in gui.splitlines() if 'kFontScale = 1.0f' in line)
        source = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
namespace gui {
struct Vec2 {float x{},y{};};
SCALE_VARIABLE
inline thread_local struct {Vec2 contentMin{0,0},contentMax{500,500};} g_frame;
SCOPES
METHODS
inline unsigned panels,labels;inline Vec2 cursor;inline float glyphHeight;
void panel_rect(Vec2,Vec2){++panels;}
void set_cursor(Vec2 p){cursor=p;}
void wrapped_label(std::string_view,float){++labels;glyphHeight=24*kFontScale;}
void label(std::string_view){++labels;glyphHeight=24*kFontScale;}
}
enum class WidgetKind {panel,scroll_area,text,other};
void paint(WidgetKind kind,float canvasScale){
using namespace gui;struct {struct {WidgetKind kind;std::string name="Container";}descriptor;}widget{{kind}};
const Vec2 position{100,100},size{360*canvasScale,34*canvasScale};const std::string label="Actual title";
const ScopedTextScale scale{canvasScale};const ContentClipScope clip{position,{position.x+size.x,position.y+size.y}};
panels=labels=0;
BRANCH
assert(panels==1);
if(kind==WidgetKind::panel||kind==WidgetKind::scroll_area)assert(labels==0);
else {assert(labels==1);assert(std::abs(glyphHeight-24*canvasScale)<0.0001f);assert(cursor.y+glyphHeight<=position.y+size.y);}
}
int main(){using namespace gui;
for(float scale:{0.2f,0.5f,1.0f,1.7f})for(auto kind:{WidgetKind::panel,WidgetKind::scroll_area,WidgetKind::text,WidgetKind::other}){
paint(kind,scale);assert(kFontScale==1&&g_frame.contentMin.x==0&&g_frame.contentMax.x==500);}
{ScopedTextScale outer{0.5f};{ScopedTextScale inner{0.5f};assert(kFontScale==0.25f);}assert(kFontScale==0.5f);
std::thread thread([]{assert(kFontScale==1);ScopedTextScale own{2};assert(kFontScale==2);});thread.join();assert(kFontScale==0.5f);
for(float invalid:{0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){ScopedTextScale refused{invalid};assert(kFontScale==0.5f);}}
assert(kFontScale==1);
try{ScopedTextScale scale{0.2f};ContentClipScope clip{{-10,100},{600,300}};assert(g_frame.contentMin.x==0&&g_frame.contentMin.y==100&&g_frame.contentMax.x==500&&g_frame.contentMax.y==300);
{ContentClipScope nested{{50,150},{200,250}};assert(g_frame.contentMin.x==50&&g_frame.contentMax.x==200);}assert(g_frame.contentMin.x==0&&g_frame.contentMax.x==500);throw 7;}
catch(int){assert(kFontScale==1&&g_frame.contentMin.y==0&&g_frame.contentMax.y==500);}
}
'''.replace('SCALE_VARIABLE', scale_variable).replace('SCOPES', scopes).replace('METHODS', methods).replace('BRANCH', branch)
        harness.ForestPreviewActions()._compile_and_run(source)


if __name__ == '__main__':
    unittest.main()
