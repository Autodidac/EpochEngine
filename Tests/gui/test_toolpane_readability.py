"""Execute actual pane sizing, action layout and browser filtering; no GUI launch."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run
import json, time

ROOT = Path(__file__).resolve().parents[2]
PANE = ROOT/'Engine/src/editor/application/editor.frame_outliner.inl'


def statement(source, marker):
    start = source.index(marker)
    return source[start:source.index(';', start)+1]


def fixture():
    pane = PANE.read_text(encoding='utf-8')
    api = (ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
    navigation = (ROOT/'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
    types = '\n'.join(definition(api, '    export '+kind+' '+name, True).replace('export ', '', 1)
        for kind, name in [('enum class','ButtonIcon'),('struct','Vec2'),('struct','WidgetBounds'),('struct','InlineButtonSpec'),
            ('enum class','InlineButtonSizing'),('enum class','InlineButtonAlignment'),('struct','InlineButtonRowOptions')])
    start = navigation.index('    [[nodiscard]] static std::vector<float> resolve_inline_button_widths(')
    end = navigation.index('    [[nodiscard]] static InlineButtonRowLayout resolve_inline_button_layout(', start)
    geometry = navigation[start:end] + definition(navigation, '    [[nodiscard]] static InlineButtonRowLayout resolve_inline_button_layout(')
    setup = '\n'.join(statement(pane, marker) for marker in (
        '        const float outlinerScrollHeight =',
        '        const float outlinerWidth =',
        '        const float paneLabelWidth ='))
    scroll = pane[pane.index('        (void)gui::begin_scroll_area('):pane.index('        const float outlinerWidth =')]
    # End after the real scroll call, excluding comments between it and width.
    scroll = scroll[:scroll.index('});')+3]
    options = definition(pane, '        const gui::InlineButtonRowOptions paneActionLayout', True)
    actions = '\n'.join(definition(pane, '            const std::array '+name, True)
        for name in ('scriptActions',))
    actions += '\n'+definition(pane, '                const std::array primaryActions', True)
    search = definition(pane, '        const auto containsNoCase =', True)
    start = pane.index('                        if (!asset_category_visible(editor.assetCategory, entry.kind)')
    skip = definition(pane[start:], '                        if (')
    return r'''
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#define CHECK(x) do {if(!(x))return __LINE__;}while(false)
namespace gui {
TYPES
float recordedAvailableWidth=0,fontAdvance=8;
constexpr float kFontScale=1,kContentPadding=4,kButtonTextClipInset=1,kBoxInnerPadding=3;
float content_available_width(float) {return recordedAvailableWidth;}
float space_advance(float) {return fontAdvance;}
float base_line_height(float) {return fontAdvance*1.5f;}
float measure_text_width(std::string_view s,float) {return static_cast<float>(s.size())*fontAdvance;}
struct {Vec2 cursor{};}g_frame;
struct ScrollAreaOptions {std::string_view id{};Vec2 size{};bool draw_background{},show_scrollbar{};};
ScrollAreaOptions recordedScroll;
void begin_scroll_area(ScrollAreaOptions o) {recordedScroll=o;}
GEOMETRY
}
bool asset_category_visible(int category,std::string_view kind) {return category==0 || kind=="SCRIPT";}
int count_visible(std::string_view query,int category) {
 SEARCH
 struct {int assetCategory;std::string_view assetSearch;}editor{category,query};
 struct Entry {std::string_view kind,label,path;};
 const std::array entries{Entry{"SCRIPT","Camera Controller","Scripts/camera.cpp"},Entry{"IMAGE","Forest Bark","Textures/bark.png"}};
 int visible=0;
 for(const auto& entry:entries) {SKIP ++visible;}
 return visible;
}
int main() {
 enum class OutlinerToolTab{World,Assets};const auto pane_tab=OutlinerToolTab::Assets;
 for(float width:{48.f,100.f,186.f,320.f,520.f,960.f}) for(float height:{24.f,100.f,720.f}) {
  const gui::Vec2 pane_pos{15,20},pane_size{width,height},outlinerScrollStart{19,24};
  SETUP
  SCROLL
  CHECK(outlinerWidth>0 && outlinerWidth<=gui::recordedScroll.size.x);
  CHECK(outlinerScrollHeight>0 && outlinerScrollHeight<=height);
  CHECK(outlinerWidth<=std::max(1.f,gui::recordedScroll.size.x-12.f));
  CHECK(paneLabelWidth>=76 && paneLabelWidth<=112);
 }
 OPTIONS
 ACTIONS
 for(float font:{6.f,10.f,15.f})for(float width:{128.f,224.f,320.f,640.f}) {
  gui::fontAdvance=font;gui::recordedAvailableWidth=width;
  const auto check=[&](std::span<const gui::InlineButtonSpec> items) {
   const auto layout=gui::resolve_inline_button_layout(items,28.f,6.f,paneActionLayout);
   if(!layout.valid || layout.items.size()!=items.size())return false;
   float rowLeft=0,rowRight=0,lastY=-1;
   for(std::size_t i=0;i<items.size();++i) {
    const auto& box=layout.items[i];
    if(items[i].width!=0 || box.size.x<=0 || box.position.x<-.01f || box.position.x+box.size.x>width+.01f)return false;
    const float measured=std::ceil(gui::measure_text_width(items[i].label,1)+12.f);
    if(std::abs(box.size.x-std::min(width,measured))>.01f)return false;
    if(box.position.y!=lastY) {
     if(lastY>=0 && std::abs(rowLeft-(width-rowRight))>1.01f)return false;
     rowLeft=box.position.x;lastY=box.position.y;
    }
    rowRight=box.position.x+box.size.x;
   }
   return std::abs(rowLeft-(width-rowRight))<=1.01f;
  };
  CHECK(check(primaryActions));CHECK(check(scriptActions));
 }
 CHECK(count_visible("",0)==2);
 CHECK(count_visible("cAmErA",0)==1);
 CHECK(count_visible("textures/",0)==1);
 CHECK(count_visible("bark",1)==0);
 CHECK(count_visible("no match",0)==0);
}
'''.replace('TYPES',types).replace('GEOMETRY',geometry).replace('SETUP',setup).replace('SCROLL',scroll).replace('OPTIONS',options).replace('ACTIONS',actions).replace('SEARCH',search).replace('SKIP',skip)


class ToolpaneReadability(unittest.TestCase):
    def test_actual_compact_tree_layout_labels_and_disclosure_geometry(self):
        pane = PANE.read_text(encoding='utf-8')
        buttons = (ROOT/'Engine/src/epochgui/controls/gui.buttons_images.inl').read_text(encoding='utf-8')
        api = (ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
        declarations = '\n'.join(definition(api, '    export '+kind+' '+name, True).replace('export ', '', 1)
            for kind,name in [('struct','Vec2'),('struct','WidgetBounds'),('enum class','MenuItemIndicator'),('struct','MenuItemOptions')])
        layout = definition(buttons,'    struct MenuItemLayout',True)+definition(buttons,'    [[nodiscard]] static MenuItemLayout make_menu_item_layout(')
        viewport = pane[pane.index('        const float hierarchyViewportHeight ='):pane.index('        const float hierarchyContentHeight =')]
        row = pane[pane.index('                const std::string_view label = row.group'):pane.index('                if (row.group)\n',pane.index('                const std::string_view label = row.group'))]
        self.assertNotIn('std::string label(row.depth',pane)
        self.assertNotIn('gui::section_heading("Create")',pane)
        self.assertNotIn('gui::section_heading("Selection")',pane)
        self.assertIn('if (pane_tab != OutlinerToolTab::World)\n        (void)gui::begin_scroll_area',pane)
        code = r"""
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
namespace gui_lib {template<class T>using CommandMenuItemsView=std::span<const T>;}
namespace gui {
enum class SemanticTone{neutral,info};
DECLARATIONS
Vec2 cursor{};Vec2 cursor_position(){return cursor;}
float measure_wrapped_label_height(std::string_view,float){return 22;}
float menu_item_row_height(){return 24;}
MenuItemOptions observed;
bool menu_item(const MenuItemOptions& item){observed=item;return false;}
LAYOUT
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
struct Row{std::string label;std::size_t depth{};bool group{},has_children{},expanded{},selected{},locked{};};
struct Editor{std::string projectName="Readable project",worldHierarchyStatus="Hierarchy synchronized.";};
int main(){
 Editor editor;
 for(float width:{100.f,200.f,480.f})for(float height:{80.f,240.f,720.f}){
  const gui::Vec2 pane_pos{10,20},pane_size{width,height};
  gui::cursor={10,54};
VIEWPORT
  CHECK(hierarchyViewportHeight>0&&hierarchyViewportHeight<=height);
  CHECK(std::abs(gui::cursor.y+hierarchyViewportHeight-(pane_pos.y+pane_size.y-4))<.01f);
 }
 const float outlinerWidth=300,hierarchyRowHeight=26;
ROW_CHECKS
 return 0;
}
"""
        # Keep the exact production menu_item call; only its host loop control is removed.
        recorded = row.replace('                    continue;','')
        code = code.replace('DECLARATIONS',declarations).replace('LAYOUT',layout).replace('VIEWPORT',viewport)
        row_checks=r"""
 for(const Row row:std::array{Row{"canonical.scene.id",0,true,true,true},Row{"Gameplay",1,true,true,false},Row{"Cube",2,false,false,false,true}}){
  const auto canonical=row.label;
ROW_RECORD
  CHECK(gui::observed.label==(row.depth==0?editor.projectName:row.label));
  CHECK(gui::observed.tone==gui::SemanticTone::neutral);
  CHECK(gui::observed.indentation==static_cast<float>(row.depth)*14.f);
  CHECK(row.label==canonical);
  CHECK(gui::observed.indicator==(row.group?gui::MenuItemIndicator::Disclosure:gui::MenuItemIndicator::None));
  CHECK(gui::observed.checked==(row.group?row.expanded:row.selected));
  for(float width:{30.f,120.f,300.f}){
   const auto base=gui::make_menu_item_layout({10,20},{width,26},18,80,0);
   const auto indented=gui::make_menu_item_layout({10,20},{width,26},18,80,0,gui::observed.indentation);
   CHECK(indented.valid&&indented.row.position.x==base.row.position.x&&indented.row.size.x==width);
   CHECK(indented.label.position.x>=base.label.position.x&&indented.label.position.x+indented.label.size.x<=10+width);
  }
 }
 CHECK(!gui::make_menu_item_layout({},{200,26},18,80,0,std::numeric_limits<float>::quiet_NaN()).valid);
"""
        code=code.replace('ROW_CHECKS',row_checks).replace('ROW_RECORD',recorded)
        report=ROOT/'Artifacts/Reports/outliner-compact-tree-oct29'/str(time.time_ns());report.mkdir(parents=True)
        source=report/'consumer.cpp';source.write_text(code,encoding='utf-8')
        result=compile_and_run(self,source,report,windows_flags=('/W4','/WX'))
        (report/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')

    def test_actual_size_actions_and_asset_search(self):
        parent=ROOT/'Artifacts/Reports/contract-tests'
        parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='toolpane-readability-',dir=parent))
        try:
            cpp=scratch/'contract.cpp';cpp.write_text(fixture(),encoding='utf-8')
            if os.name=='nt':
                tools=sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
                sdk=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((sdk/'Include').glob('*'))
                if not tools or not kits:self.skipTest('MSVC/SDK required')
                tool,kit=tools[-1],kits[-1]
                base=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX','/I'+str(tool/'include'),'/I'+str(kit/'ucrt')]
                link=['/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
                modes=[('Debug',['/Od','/MDd','/D_DEBUG']),('Release',['/O2','/MD','/DNDEBUG'])]
            else:
                compiler=shutil.which('clang++')or shutil.which('g++')
                if not compiler:self.skipTest('C++23 compiler required')
                base=[compiler,'-std=c++23'];link=[];modes=[('Debug',['-O0']),('Release',['-O2','-DNDEBUG'])]
            for mode,flags in modes:
                with self.subTest(mode=mode):
                    binary=scratch/(mode+('.exe'if os.name=='nt'else''))
                    output=['/Fo'+str(scratch/(mode+'.obj')),'/Fe'+str(binary)]if os.name=='nt'else['-o',str(binary)]
                    built=subprocess.run(base+flags+output+[str(cpp)]+link,cwd=scratch,capture_output=True,text=True,timeout=60)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    ran=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
                    self.assertEqual(ran.returncode,0,f'Actual pane geometry/search failed at C++ line {ran.returncode}')
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__=='__main__':unittest.main()
