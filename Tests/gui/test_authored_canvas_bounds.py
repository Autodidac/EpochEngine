"""Actual Editor canvas traversal and GUI clip/input owners with real documents."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/application/editor.application_gui_authoring.inl"
GUI = ROOT / "Engine/src/epochgui/gui.engine.cpp"
FRAME = ROOT / "Engine/src/epochgui/controls/gui.frame_lifecycle.inl"
API = ROOT / "Engine/modules/gui/gui.engine.ixx"


def actual_fixture(owner_text=None):
    owner, gui, frame, api = (p.read_text(encoding="utf-8") for p in (OWNER, GUI, FRAME, API))
    if owner_text is not None:
        owner = owner_text
    chrome = (ROOT / "Engine/src/epochgui/controls/gui.navigation_chrome.inl").read_text(encoding="utf-8")
    state = (ROOT / "Engine/src/editor/application/editor.application_state.inl").read_text(encoding="utf-8")
    view_state = definition(state, "            struct GuiCanvasViewState final", True)
    scopes = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export class ContentClipScope", "    export class ScopedTextScale",
        "    export struct DragSurfaceState", "    export struct DragSurfaceOptions",
        "    export struct DragSurfaceResult", "    export struct WidgetBounds",
        "    export enum class EventType", "    export struct InputEvent",
        "    export enum class ButtonIcon", "    export struct InlineButtonSpec",
        "    export enum class InlineButtonSizing",
        "    export enum class InlineButtonAlignment", "    export struct InlineButtonRowOptions"))
    clip = "\n".join(definition(gui, marker) for marker in (
        "    ContentClipScope::ContentClipScope(", "    ContentClipScope::~ContentClipScope()",
        "    ScopedTextScale::ScopedTextScale(", "    ScopedTextScale::~ScopedTextScale()",
        "        [[nodiscard]] static bool point_in_rect(",
        "        [[nodiscard]] static bool point_in_active_clip("))
    pointer_trace = frame[frame.index("    struct PointerFrameTraceState"):
        frame.index("    static thread_local PointerFrameTraceState g_pointerFrameTrace{};")
        + len("    static thread_local PointerFrameTraceState g_pointerFrameTrace{};")]
    pointer = pointer_trace + "\n" + "\n".join(definition(frame, marker) for marker in (
        "    Vec2 mouse_position()", "    std::optional<Vec2> mouse_press_position(",
        "    bool drag_surface_hit_test(", "    DragSurfaceResult drag_surface(",
        "    bool is_mouse_over_content_clip()", "    bool was_mouse_pressed()", "    bool is_mouse_down()", "    bool was_mouse_released()"))
    controls = "\n".join(definition(chrome, marker, semi) for marker, semi in (
        ("    [[nodiscard]] static std::vector<float> resolve_inline_button_widths(", False),
        ("    struct InlineButtonRowLayout", True),
        ("    [[nodiscard]] static InlineButtonRowLayout make_inline_button_layout(", False),
        ("    [[nodiscard]] static InlineButtonRowLayout resolve_inline_button_layout(", False),
        ("    float inline_button_row_height(", False)))
    widget = definition(owner, "        [[nodiscard]] bool render_gui_document_widget(")
    prefix = widget[widget.index("{\n") + 2:widget.index("            const gui::ContentClipScope widgetClip{")]
    fallback_marker = "            const auto selectionPress" if "            const auto selectionPress" in widget else "            if (!activated && interactive"
    fallback = widget[widget.index(fallback_marker):widget.rindex("            return activated;")]
    canvas = "\n".join(definition(owner, marker, semicolon) for marker, semicolon in (
        ("        struct GuiDocumentCanvasFrame final", True),
        ("        [[nodiscard]] GuiDocumentCanvasFrame prepare_gui_document_canvas(", False),
        ("        void render_gui_document_canvas(", False)))
    canvas += owner[owner.index("        [[nodiscard]] GuiDocumentCanvasFrame compute_gui_document_canvas("):owner.index("        [[nodiscard]] GuiDocumentCanvasFrame prepare_gui_document_canvas(")]
    # Keep definition ordering: the compute function has braced default arguments.
    compute = canvas[canvas.index("        [[nodiscard]] GuiDocumentCanvasFrame compute_gui_document_canvas("):]
    canvas = canvas[:canvas.index("        [[nodiscard]] GuiDocumentCanvasFrame compute_gui_document_canvas(")]
    boundary = canvas.index("        [[nodiscard]] GuiDocumentCanvasFrame prepare_gui_document_canvas(")
    canvas = canvas[:boundary] + compute + canvas[boundary:]
    return r'''
#include <algorithm>
#include <array>
#include <span>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <compare>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
import authoring.gui_document;
import epoch.gui.input;
void require(bool accepted,int line){if(!accepted){std::fprintf(stderr,"Canvas owner contract failed at line %d\n",line);std::exit(81);}}
#undef assert
#define assert(value) require(static_cast<bool>(value),__LINE__)
namespace authoring = epochengine::authoring;
namespace gui_lib = epochengine::gui_lib;
namespace epochengine::logger { void info(std::string_view, std::string_view) {} }
namespace gui {
struct Vec2 {float x{},y{};};
SCOPES
inline float kFontScale=1;
inline struct Frame {void* ctx=reinterpret_cast<void*>(1);bool insideWindow=true;
 Vec2 contentMin{0,0},contentMax{2000,2000},mousePos{};
 bool mouseDown{},prevMouseDown{},justPressed{},justReleased{},mousePressConsumed{},mouseReleaseConsumed{},pointerInputAllowed=true;
 bool selectPopupCapturedThisFrame{},selectPopupOwnsInput{},menuPopupBlocksBackground{},menuPopupOwnsInput{};
 std::vector<InputEvent> events;Vec2 cursor{};}g_frame;
bool has_content_clip(){return g_frame.insideWindow;}
bool point_in_modal_input_capture(Vec2){return g_frame.pointerInputAllowed;}
bool left_press_available(){return g_frame.justPressed&&!g_frame.mousePressConsumed;}
bool left_release_available(){return g_frame.justReleased;}
void consume_left_release(){}
CLIP
POINTER
void panel_rect(Vec2,Vec2){}
void set_cursor(Vec2 p){g_frame.cursor=p;}
float content_available_width(float x){return (std::max)(1.0f,g_frame.contentMax.x-x);}
float space_advance(float scale){return 8*scale;}
float base_line_height(float scale){return 21*scale;}
float measure_text_width(std::string_view text,float scale){return static_cast<float>(text.size())*8*scale;}
constexpr float kContentPadding=6,kBoxInnerPadding=5,kButtonTextClipInset=2;
CONTROLS
inline std::optional<std::size_t> nextAction;
inline float toolbarInset{};
inline InlineButtonRowLayout lastToolbar;
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> items,float height,float gap,const InlineButtonRowOptions& options){
 lastToolbar=resolve_inline_button_layout(items,height,gap,options);toolbarInset=lastToolbar.height+4;
 auto action=std::exchange(nextAction,{});if(action)assert(*action<items.size()&&items[*action].enabled);return action;
}
}
using namespace authoring::gui;
struct EditorState {
 GuiDocument* document{};VIEW_STATE GuiCanvasViewState guiCanvasView{};gui::DragSurfaceState guiCanvasWidgetDrag;
 WidgetHandle guiCanvasWidgetDragHandle{};
 float guiCanvasWidgetDragOriginX{},guiCanvasWidgetDragOriginY{},guiCanvasWidgetDragPreviewX{},guiCanvasWidgetDragPreviewY{};
 std::string guiDocumentStatus;std::vector<WidgetHandle> selected;
};
GuiDocument* synchronize_gui_document(EditorState& state){return state.document;}
std::string gui_widget_display_text(const WidgetView& widget){return widget.descriptor.name;}
bool gui_widget_selected(const EditorState&,WidgetHandle){return false;}
void select_gui_widget(EditorState& state,WidgetHandle handle){state.selected.push_back(handle);}
bool project_gui_document_layout(EditorState&){return true;}
// Bounds owns a document-only port; grouped scene-journal behavior has its
// own actual-owner contract in test_grouped_gui_world_history.
bool commit_gui_layout_edit(EditorState& state,WidgetHandle widget,LayoutDescriptor layout){
 return static_cast<bool>(state.document->update_layout(widget,std::move(layout)));}
bool editor_point_in_rect(gui::Vec2 p,gui::Vec2 origin,gui::Vec2 size){
 return gui::point_in_rect(p,origin.x,origin.y,size.x,size.y);
}
struct Paint {WidgetHandle handle;gui::Vec2 position,size,clipMin,clipMax;};
inline std::vector<Paint> paints;
bool render_gui_document_widget(EditorState& state,GuiDocument& document,const WidgetView& widget,
 gui::Vec2& position,gui::Vec2 size,gui::Vec2 virtualParentSize,LayoutFlow parentFlow,float canvasScale,bool interactive){
 PREFIX
 // Rendering is a recorder port; the traversal and pointer branches above/below
 // are extracted from the real owner. The real widget clip owner is unchanged.
 {const gui::ContentClipScope widgetClip{position,{position.x+size.x,position.y+size.y}};
 paints.push_back({widget.handle,position,size,gui::g_frame.contentMin,gui::g_frame.contentMax});}
 FALLBACK
 return activated;
}
CANVAS
bool close(float a,float b){return std::abs(a-b)<0.0001f;}
bool same(gui::Vec2 a,gui::Vec2 b){return close(a.x,b.x)&&close(a.y,b.y);}
const Paint& painted(WidgetHandle handle){auto it=std::ranges::find(paints,handle,&Paint::handle);assert(it!=paints.end());return *it;}
WidgetDescriptor descriptor(WidgetKind kind,std::string name,float x,float y,float width,float height){
 WidgetDescriptor d;d.kind=kind;d.name=std::move(name);d.layout.x=x;d.layout.y=y;d.layout.width=width;d.layout.height=height;
 switch(kind){case WidgetKind::canvas:d.content=CanvasContent{1000,1000,false};break;
 case WidgetKind::scroll_area:d.content=ScrollAreaContent{};break;
 case WidgetKind::tab_set:d.content=TabSetContent{};break;
 case WidgetKind::tab_page:d.content=TabPageContent{"Page",false};break;
 case WidgetKind::text:d.content=TextContent{"Text",false};break;
 default:d.content=PanelContent{};break;}return d;
}
WidgetHandle add(GuiDocument& doc,WidgetDescriptor d,WidgetHandle parent={}){const auto r=doc.create_widget(std::move(d),parent);assert(r);return r.widget;}
void reset(EditorState& state){paints.clear();state.selected.clear();state.guiCanvasWidgetDrag={};state.guiCanvasWidgetDragHandle={};gui::g_frame={};gui::toolbarInset=0;gui::lastToolbar={};gui::nextAction={};}
GuiDocumentCanvasFrame viewed(EditorState& state,gui::Vec2 p,gui::Vec2 size){
 p.y+=gui::toolbarInset;size.y-=gui::toolbarInset;return compute_gui_document_canvas(*state.document,p,size,state.guiCanvasView.zoom,state.guiCanvasView.pan);
}
void bounds_and_inheritance(){
 GuiDocument doc{DocumentHandle{1,1},BranchIdentity{2,1}};EditorState state{&doc};
 const auto root=add(doc,descriptor(WidgetKind::canvas,"Canvas",0,0,1000,1000));
 auto d=descriptor(WidgetKind::panel,"Panel",100,100,200,160);d.layout.clip_children=true;
 const auto panel=add(doc,d,root);const auto child=add(doc,descriptor(WidgetKind::text,"Overflow",180,140,80,80),panel);
 const auto outside=add(doc,descriptor(WidgetKind::text,"OutsideCanvas",990,990,100,100),root);
 const auto sibling=add(doc,descriptor(WidgetKind::panel,"Sibling",400,100,80,30),root);
 auto scroll=descriptor(WidgetKind::scroll_area,"Scroll",100,400,200,160);
 const auto scrollHandle=add(doc,scroll,root);const auto scrollChild=add(doc,descriptor(WidgetKind::text,"ScrollOverflow",180,140,80,80),scrollHandle);
 auto nested=descriptor(WidgetKind::panel,"Nested",150,20,150,100);nested.layout.clip_children=true;
 const auto nestedHandle=add(doc,nested,panel);const auto nestedChild=add(doc,descriptor(WidgetKind::text,"NestedOverflow",100,60,100,80),nestedHandle);
 const auto revision=doc.revision();
 for(float extent:{40.0f,100.0f,320.0f,1000.0f}){
  reset(state);render_gui_document_canvas(state,{31,77},{extent,extent},false);
  const auto frame=viewed(state,{31,77},{extent,extent});
  const auto& p=painted(panel);const auto& c=painted(child);const auto& o=painted(outside);const auto& s=painted(sibling);
  assert(same(p.position,{frame.position.x+100*frame.scale,frame.position.y+100*frame.scale}));
  assert(same(p.size,{200*frame.scale,160*frame.scale}));assert(same(s.size,{80*frame.scale,30*frame.scale}));
  assert(same(c.clipMax,{p.position.x+p.size.x,p.position.y+p.size.y}));
  assert(same(o.clipMax,{frame.position.x+frame.size.x,frame.position.y+frame.size.y}));
  assert(same(s.clipMin,s.position)); // prior clipped siblings do not leak their scopes
  const auto& sc=painted(scrollChild);const auto& sp=painted(scrollHandle);assert(same(sc.clipMax,{sp.position.x+sp.size.x,sp.position.y+sp.size.y}));
  const auto& nc=painted(nestedChild);assert(close(nc.clipMax.x,p.position.x+p.size.x));
  assert(same(gui::g_frame.contentMin,{0,0})&&same(gui::g_frame.contentMax,{2000,2000}));
  assert(doc.revision()==revision&&doc.widget(child)->descriptor.layout.x==180);
 }
 // Unclipped containers may overflow, but the canvas always bounds the paint.
 d.layout.clip_children=false;assert(doc.update_layout(panel,d.layout));reset(state);
 render_gui_document_canvas(state,{31,77},{100,100},false);
 const auto& c=painted(child);assert(same(c.clipMax,{c.position.x+c.size.x,c.position.y+c.size.y}));
 // The enclosing window remains the stronger owner and is restored afterwards.
 reset(state);gui::g_frame.contentMin={50,100};gui::g_frame.contentMax={90,140};
 render_gui_document_canvas(state,{31,77},{100,100},false);
 for(const auto& p:paints){assert(p.clipMin.x>=50&&p.clipMin.y>=100&&p.clipMax.x<=90&&p.clipMax.y<=140);}
 assert(same(gui::g_frame.contentMin,{50,100})&&same(gui::g_frame.contentMax,{90,140}));
}
void tab_page_and_flow(){
 GuiDocument doc{DocumentHandle{3,1},BranchIdentity{4,1}};EditorState state{&doc};
 const auto root=add(doc,descriptor(WidgetKind::canvas,"Canvas",0,0,1000,1000));
 const auto tabs=add(doc,descriptor(WidgetKind::tab_set,"Tabs",50,50,300,30),root);
 const auto page=add(doc,descriptor(WidgetKind::tab_page,"Page",0,0,300,30),tabs);assert(doc.update_content(tabs,TabSetContent{page}));
 auto d=descriptor(WidgetKind::panel,"FillPage",0,0,100,20);d.layout.height_rule=SizeRule::fill;const auto content=add(doc,d,page);
 auto flow=descriptor(WidgetKind::panel,"Flow",100,400,400,100);flow.layout.flow=LayoutFlow::horizontal;flow.layout.spacing=7;flow.layout.padding.left=3;
 const auto parent=add(doc,flow,root);const auto first=add(doc,descriptor(WidgetKind::text,"First",0,0,50,20),parent);const auto second=add(doc,descriptor(WidgetKind::text,"Second",0,0,60,20),parent);
 reset(state);const auto frame=compute_gui_document_canvas(doc,{0,0},{100,100});render_gui_document_canvas(state,{0,0},{100,100},false);
 const auto& t=painted(tabs);const auto& p=painted(content);assert(close(p.size.y,t.size.y*0.65f)&&p.size.y<18);
 assert(close(painted(second).position.x-painted(first).position.x,57*frame.scale));
}
void pointer_clipping_and_child_selection(){
 GuiDocument doc{DocumentHandle{5,1},BranchIdentity{6,1}};EditorState state{&doc};
 const auto root=add(doc,descriptor(WidgetKind::canvas,"Canvas",0,0,1000,1000));
 auto d=descriptor(WidgetKind::panel,"Panel",100,100,200,200);d.layout.clip_children=true;
 const auto panel=add(doc,d,root);const auto child=add(doc,descriptor(WidgetKind::text,"Child",30,30,80,80),panel);
 const auto overflow=add(doc,descriptor(WidgetKind::text,"Overflow",180,20,100,100),panel);
 const auto outside=add(doc,descriptor(WidgetKind::text,"Outside",990,990,100,100),root);
 const auto frame=compute_gui_document_canvas(doc,{0,0},{100,100});
 auto click=[&](float x,float y){reset(state);gui::g_frame.mousePos={frame.position.x+x*frame.scale,frame.position.y+y*frame.scale};gui::g_frame.justPressed=gui::g_frame.mouseDown=true;render_gui_document_canvas(state,{0,0},{100,100},true);};
 click(150,150);assert(state.guiCanvasWidgetDrag.pending&&state.guiCanvasWidgetDragHandle==child);assert(std::ranges::find(state.selected,child)!=state.selected.end());
 click(350,150);assert(std::ranges::find(state.selected,overflow)==state.selected.end()); // inside authored child, outside its clipping parent
 click(1050,1050);assert(std::ranges::find(state.selected,outside)==state.selected.end()); // inside authored child, outside canvas
 click(298,150);assert(std::ranges::find(state.selected,overflow)!=state.selected.end()); // visible part remains interactive
}
void view_zoom_pan_and_mapping(){
 GuiDocument doc{DocumentHandle{7,1},BranchIdentity{8,1}};EditorState state{&doc};
 const auto root=add(doc,descriptor(WidgetKind::canvas,"Canvas",0,0,1000,1000));
 const auto child=add(doc,descriptor(WidgetKind::text,"Readable",480,480,100,40),root);
 const auto home=add(doc,descriptor(WidgetKind::text,"Home",28,20,160,34),root);
 const auto revision=doc.revision();
 reset(state);render_gui_document_canvas(state,{20,40},{600,400},true);
 const auto fit=viewed(state,{20,40},{600,400});assert(fit.scale<1&&state.guiCanvasView.zoom==0);
 for(const auto& button:gui::lastToolbar.items)assert(button.size.x<200); // content widths, not screen-wide
 gui::nextAction=1;paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);
 assert(state.guiCanvasView.zoom==1&&close(painted(child).size.x,100));
 const auto one=viewed(state,{20,40},{600,400});assert(one.scale==1&&one.size.x==1000);
 assert(one.position.x>=20&&one.position.y>=40+gui::toolbarInset); // preserve the visible authored top-left, not a blank geometric center
 assert(painted(home).clipMin.x<painted(home).clipMax.x&&painted(home).clipMin.y<painted(home).clipMax.y);
 for(const auto& p:paints){assert(p.clipMin.x>=20&&p.clipMin.y>=40+gui::toolbarInset&&p.clipMax.x<=620&&p.clipMax.y<=440);}
 gui::nextAction=4;paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);
 assert(close(state.guiCanvasView.zoom,1.25f)&&close(painted(child).size.x,125));
 gui::nextAction=2;paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(close(state.guiCanvasView.zoom,1));
 // Manual zoom survives resize; Fit recomputes without authoring mutations.
 paints.clear();render_gui_document_canvas(state,{20,40},{420,300},true);assert(state.guiCanvasView.zoom==1&&close(painted(child).size.x,100));
 gui::nextAction=0;paints.clear();render_gui_document_canvas(state,{20,40},{420,300},true);
 assert(state.guiCanvasView.zoom==0&&same(state.guiCanvasView.pan,{}));const auto small=viewed(state,{20,40},{420,300});
 paints.clear();render_gui_document_canvas(state,{20,40},{800,700},true);const auto large=viewed(state,{20,40},{800,700});assert(large.scale>small.scale);
 // Pan uses the existing drag owner, never a document/widget mutation.
 state.guiCanvasView.zoom=2;state.guiCanvasView.pan={100000,-100000};paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);
 const auto clamped=viewed(state,{20,40},{600,400});assert(same(state.guiCanvasView.pan,clamped.pan)&&std::abs(clamped.pan.x)<1000&&std::abs(clamped.pan.y)<1000);
 state.guiCanvasView.pan={};gui::nextAction=5;paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(state.guiCanvasView.panMode);
 gui::g_frame.mousePos={320,240};gui::g_frame.mouseDown=gui::g_frame.justPressed=true;
 paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(state.guiCanvasView.drag.pending&&state.selected.empty());
 gui::g_frame.justPressed=false;gui::g_frame.mousePos={340,255};paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);
 assert(same(state.guiCanvasView.pan,{20,15})&&doc.revision()==revision&&state.selected.empty());
 gui::g_frame.mouseDown=false;gui::g_frame.justReleased=true;gui::g_frame.mousePos={350,263};
 paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);
 assert(same(state.guiCanvasView.pan,{30,23})&&!state.guiCanvasView.drag.pending&&doc.revision()==revision);
 gui::g_frame.mousePressConsumed=false;gui::g_frame.justReleased=false;gui::g_frame.mouseDown=gui::g_frame.justPressed=true;gui::g_frame.mousePos={320,240};
 paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(state.guiCanvasView.drag.pending);
 gui::g_frame.pointerInputAllowed=false;paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(!state.guiCanvasView.drag.pending&&!state.guiCanvasView.drag.dragging);
 gui::g_frame.pointerInputAllowed=true;gui::g_frame.mouseDown=false;gui::g_frame.justReleased=true;gui::g_frame.mousePos={1900,1900};
 paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(!state.guiCanvasView.drag.pending&&doc.revision()==revision);
 // Existing widget dragging maps screen delta through the full chosen scale.
 reset(state);state.guiCanvasView.panMode=false;state.guiCanvasView.zoom=2;state.guiCanvasView.pan={};
 render_gui_document_canvas(state,{20,40},{600,400},true);const auto target=painted(child);
 gui::g_frame.mousePos={target.position.x+10,target.position.y+10};gui::g_frame.justPressed=gui::g_frame.mouseDown=true;
 paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(state.guiCanvasWidgetDrag.pending);
 gui::g_frame.justPressed=false;gui::g_frame.mousePos.x+=20;gui::g_frame.mousePos.y+=10;
 paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);assert(close(state.guiCanvasWidgetDragPreviewX,490)&&close(state.guiCanvasWidgetDragPreviewY,485));
 gui::g_frame.mouseDown=false;gui::g_frame.justReleased=true;paints.clear();render_gui_document_canvas(state,{20,40},{600,400},true);
 assert(close(doc.widget(child)->descriptor.layout.x,490)&&close(doc.widget(child)->descriptor.layout.y,485));
}
void deepest_child_drag_owner(){
 GuiDocument doc{DocumentHandle{21,1},BranchIdentity{22,1}};EditorState state{&doc};state.guiCanvasView.zoom=1;
 const auto root=add(doc,descriptor(WidgetKind::canvas,"Canvas",0,0,1000,1000));
 auto panel=descriptor(WidgetKind::panel,"Navigation",100,100,500,300);panel.layout.clip_children=true;
 const auto navigation=add(doc,panel,root);
 auto button=descriptor(WidgetKind::button,"Home",20,20,120,60);button.content=ButtonContent{"Home"};
 const auto home=add(doc,button,navigation);button.name="Settings";button.layout.x=180;button.content=ButtonContent{"Settings"};
 const auto settings=add(doc,button,navigation);
 for(bool enabled:{true,false}){
  auto interaction=doc.widget(home)->descriptor.interaction;interaction.enabled=enabled;(void)doc.update_interaction(home,interaction);
  reset(state);render_gui_document_canvas(state,{20,40},{1000,1000},false);
  const auto target=painted(home);const auto before=doc.revision();
  const gui::Vec2 origin{target.position.x+target.size.x/2,target.position.y+target.size.y/2};
  const gui::Vec2 end{origin.x+90,origin.y+60};
  gui::g_frame.mousePos=end;gui::g_frame.events={
   {.type=gui::EventType::MouseDown,.mouse_pos=origin},
   {.type=gui::EventType::MouseMove,.mouse_pos=end},
   {.type=gui::EventType::MouseUp,.mouse_pos=end}};
  paints.clear();render_gui_document_canvas(state,{20,40},{1000,1000},true);
  assert(doc.widget(navigation)->descriptor.layout.x==100&&doc.widget(navigation)->descriptor.layout.y==100);
  assert(doc.widget(home)->descriptor.layout.x==110&&doc.widget(home)->descriptor.layout.y==80);
  assert(doc.widget(settings)->descriptor.layout.x==180&&doc.widget(settings)->descriptor.layout.y==20);
  assert(doc.revision().sequence==before.sequence+1&&state.selected.back()==home);
  assert(!state.guiCanvasWidgetDrag.pending&&!state.guiCanvasWidgetDrag.dragging);
  assert(doc.undo());assert(doc.widget(home)->descriptor.layout.x==20&&doc.widget(home)->descriptor.layout.y==20);
 }
 // A real parent rectangle wins over a nearby child's padding-only target,
 // even if that child was selected before this gesture.
 reset(state);render_gui_document_canvas(state,{20,40},{1000,1000},false);state.selected.push_back(home);
 const auto homePaint=painted(home);
 const gui::Vec2 parentOrigin{homePaint.position.x-3,homePaint.position.y+homePaint.size.y/2};
 const gui::Vec2 parentEnd{parentOrigin.x+30,parentOrigin.y+20};
 gui::g_frame.mousePos=parentEnd;gui::g_frame.events={{.type=gui::EventType::MouseDown,.mouse_pos=parentOrigin},{.type=gui::EventType::MouseUp,.mouse_pos=parentEnd}};
 paints.clear();render_gui_document_canvas(state,{20,40},{1000,1000},true);
 assert(doc.widget(navigation)->descriptor.layout.x==130&&doc.widget(navigation)->descriptor.layout.y==120);
 assert(doc.widget(home)->descriptor.layout.x==20&&doc.widget(home)->descriptor.layout.y==20&&state.selected.back()==navigation);
 assert(doc.undo());
 // Ending on the selected child cannot admit a press that began on empty canvas.
 reset(state);render_gui_document_canvas(state,{20,40},{1000,1000},false);state.selected.push_back(home);
 const auto current=painted(home);const auto canvas=compute_gui_document_canvas(doc,{20,40+gui::toolbarInset},{1000,1000-gui::toolbarInset},1,{});
 const gui::Vec2 outsideOrigin{canvas.position.x+800,canvas.position.y+800},insideEnd{current.position.x+30,current.position.y+20};
 const auto outsideRevision=doc.revision();gui::g_frame.mousePos=insideEnd;
 gui::g_frame.events={{.type=gui::EventType::MouseDown,.mouse_pos=outsideOrigin},{.type=gui::EventType::MouseMove,.mouse_pos=insideEnd},{.type=gui::EventType::MouseUp,.mouse_pos=insideEnd}};
 paints.clear();render_gui_document_canvas(state,{20,40},{1000,1000},true);
 assert(doc.revision()==outsideRevision&&doc.widget(navigation)->descriptor.layout.x==100&&doc.widget(home)->descriptor.layout.x==20);
 assert(state.selected.size()==1&&state.selected.back()==home&&!state.guiCanvasWidgetDragHandle);
 // A Down outside the canvas widgets followed by a held move over Home must
 // not let the earlier-painted canvas steal a later pane-title gesture.
 reset(state);render_gui_document_canvas(state,{20,40},{1000,1000},false);state.selected.push_back(settings);
 const auto heldRevision=doc.revision();gui::g_frame.mousePos=insideEnd;gui::g_frame.mouseDown=gui::g_frame.justPressed=true;
 gui::g_frame.events={{.type=gui::EventType::MouseDown,.mouse_pos=outsideOrigin},{.type=gui::EventType::MouseMove,.mouse_pos=insideEnd}};
 paints.clear();render_gui_document_canvas(state,{20,40},{1000,1000},true);
 assert(doc.revision()==heldRevision&&state.selected.size()==1&&state.selected.back()==settings&&!gui::g_frame.mousePressConsumed);
 // Later visible siblings are painted above earlier ones and win overlap.
 button.name="Overlay";button.layout.x=20;button.content=ButtonContent{"Overlay"};
 const auto overlay=add(doc,button,navigation);reset(state);render_gui_document_canvas(state,{20,40},{1000,1000},false);
 const auto target=painted(overlay);const gui::Vec2 origin{target.position.x+30,target.position.y+20},end{origin.x+40,origin.y+30};
 gui::g_frame.mousePos=end;gui::g_frame.events={{.type=gui::EventType::MouseDown,.mouse_pos=origin},{.type=gui::EventType::MouseUp,.mouse_pos=end}};
 paints.clear();render_gui_document_canvas(state,{20,40},{1000,1000},true);
 assert(doc.widget(overlay)->descriptor.layout.x==60&&doc.widget(overlay)->descriptor.layout.y==50);
 assert(doc.widget(home)->descriptor.layout.x==20&&doc.widget(navigation)->descriptor.layout.x==100&&state.selected.back()==overlay);
}
int main(){bounds_and_inheritance();tab_page_and_flow();pointer_clipping_and_child_selection();view_zoom_pan_and_mapping();deepest_child_drag_owner();}
'''.replace("SCOPES", scopes).replace("CLIP", clip).replace("POINTER", pointer).replace("PREFIX", prefix).replace("FALLBACK", fallback).replace("CANVAS", canvas).replace("VIEW_STATE", view_state).replace("CONTROLS", controls)


class AuthoredCanvasBounds(unittest.TestCase):
    def test_actual_canvas_and_pointer_owners_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC independent C++23 module consumer")
        proof = ROOT / "build/validation/authored-canvas-bounds-oct26"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.canvas_bounds_contract.cpp"
        source.write_text(actual_fixture(), encoding="utf-8")
        modules = [ROOT / "Engine/modules/authoring/authoring.document.ixx", ROOT / "Engine/modules/authoring/authoring.gui_document.ixx"]
        portable = ROOT / "Engine/dep/EpochGui"
        modules += [portable / "modules/epoch.gui.ixx", portable / "modules/epoch.gui.input.ixx"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(AuthoredCanvasBounds LANGUAGES CXX)
add_executable(canvas_bounds "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(canvas_bounds PRIVATE cxx_std_23)
target_compile_options(canvas_bounds PRIVATE /W3 /WX /utf-8)
target_sources(canvas_bounds PRIVATE FILE_SET CXX_MODULES
 BASE_DIRS "{(ROOT / 'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_canvas_bounds COMMAND canvas_bounds)
set_tests_properties(actual_canvas_bounds PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(proof / "msvc"), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(proof / "msvc"), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(proof / "msvc"), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual Editor canvas traversal + clip/input functions with real authoring modules; independent CPU Debug/Release",
            "passed": True, "nativeAcceptance": False,
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [OWNER, GUI, FRAME, API, *modules]},
            "commands": results,
            "cases": ["scaled authored sizes below18px", "outer canvas clipping", "parent clip_children and permitted overflow",
                      "scroll-area clipping without flag", "nested clip intersection and sibling restoration", "stronger window clip restored",
                      "tiny selected-tab content size", "flow spacing uses authoring units", "document coordinates/revision unchanged",
                      "visible child first-click selection while parent owns shared drag", "clipped child/canvas pointer refusal", "visible partial child pointer acceptance", "whole-canvas Fit and 100%/incremental zoom", "responsive content-width view controls",
                      "manual zoom retained and Fit recomputed on resize", "bounded pan does not mutate document", "focus/release cancels pan",
                      "existing widget drag divides screen delta by selected zoom", "100% retains visible starter label anchor", "active pan applies final release movement before settling",
                      "queued Home child drag moves child90x60 instead of Navigation parent", "disabled authored widget retains editing policy", "overlap resolves in existing paint order", "one canonical mutation and undo restores child"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
