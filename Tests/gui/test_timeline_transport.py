"""Actual timeline transport + central Editor sync, with explicit GUI input ports."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,time,unittest
from Tests.build.test_opengl_sprite_stream import definition

ROOT=Path(__file__).resolve().parents[2]

class TimelineTransport(unittest.TestCase):
 def test_actual_fast_icons_bounds_direction_and_theme_ink(self):
  api=(ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
  paint=(ROOT/'Engine/src/epochgui/controls/gui.buttons_images.inl').read_text(encoding='utf-8')
  code=r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
struct Vec2{float x{},y{};};
ICON_API
struct Palette{bool isLight{};}palette;
const Palette&active_palette(){return palette;}
struct Resources{int contrastBlack=1,contrastWhite=2;}g_resources;
struct Draw{int ink;float x,y,w,h;};std::vector<Draw>draws;
void draw_sprite(int ink,float x,float y,float w,float h){draws.push_back({ink,x,y,w,h});}
PAINT
int main(){
 static_assert(static_cast<unsigned>(ButtonIcon::reverse)==6&&static_cast<unsigned>(ButtonIcon::fast_reverse)==7);
 for(bool light:{false,true})for(auto icon:{ButtonIcon::fast_reverse,ButtonIcon::fast_forward}){
  palette.isLight=light;draws.clear();draw_button_icon(icon,{10,20},{24,24});
  CHECK(draws.size()==24);
  for(const auto&d:draws)CHECK(d.ink==(light?1:2)&&d.x>=10&&d.y>=20&&d.x+d.w<=34&&d.y+d.h<=44&&d.w>0&&d.h>0);
  CHECK(draws[12].x-draws[0].x==6.f);
  if(icon==ButtonIcon::fast_reverse)CHECK(draws[0].x>draws[5].x&&draws[0].x+draws[0].w==draws[5].x+draws[5].w);
  else CHECK(draws[0].x==draws[5].x&&draws[0].w<draws[5].w);
 }
 draws.clear();draw_button_icon(ButtonIcon::start,{10,20},{24,24});CHECK(draws.size()==25&&draws.front().w==2);
 draws.clear();draw_button_icon(ButtonIcon::reverse,{10,20},{24,24});CHECK(draws.size()==12);
 draws.clear();draw_button_icon(ButtonIcon::fast_forward,{10,20},{4,4});CHECK(draws.empty());
 return 0;
}
'''.replace('ICON_API',definition(api,'    export enum class ButtonIcon').replace('export ','',1)+';').replace('PAINT',definition(paint,'    static void draw_button_icon('))
  proof=ROOT/'Artifacts/Reports/timeline-fast-transport-oct29/icons'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  cpp=proof/'icons.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,proof)
  (proof/'result.json').write_text(json.dumps({'passed':True,'results':results,'scope':'Actual icon enum and vector paint owner: two directions, dark/light contrast ink, exact bounds, distinct start/reverse glyphs; normal failure returns, no native renderer.'},indent=2),encoding='utf-8')

 def test_real_transport_and_editor_commands_debug_release(self):
  module=(ROOT/'Engine/modules/timeline.system.ixx').read_text(encoding='utf-8')
  clock=(ROOT/'Engine/dep/EpochPlatformEngine/modules/core.time.ixx').read_text(encoding='utf-8')
  core=(ROOT/'Engine/modules/editor.core.ixx').read_text(encoding='utf-8')
  inspector=(ROOT/'Engine/src/editor/application/editor.frame_inspector.inl').read_text(encoding='utf-8')
  diagnostics=(ROOT/'Engine/src/editor/application/editor.application_runtime_diagnostics.inl').read_text(encoding='utf-8')
  bootstrap=(ROOT/'Engine/src/editor/application/editor.frame_bootstrap.inl').read_text(encoding='utf-8')
  viewport=(ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
  chrome=(ROOT/'Engine/src/epochgui/controls/gui.navigation_chrome.inl').read_text(encoding='utf-8')
  gui_api=(ROOT/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
  self.assertEqual(bootstrap.count('advance_editor_timeline(editor);'),1)
  self.assertLess(bootstrap.index('advance_editor_timeline(editor);'),bootstrap.index('pump_script_build(editor);'))
  self.assertNotIn('sync_to_simulation(',inspector+viewport)
  app=(ROOT/'Engine/src/editor/editor.application.cpp').read_text(encoding='utf-8')
  self.assertLess(app.index('editor.frame_bootstrap.inl'),app.index('editor.frame_viewport.inl'))
  owner=module[module.index('export namespace epochengine::timeline'):module.index('    inline void clamp_view_config')].replace('export namespace','namespace',1)
  owner+=definition(module,'    inline void sort_events(')+definition(module,'    [[nodiscard]] inline std::vector<TimelineTrack> default_editor_tracks(')+'}\n'
  helpers='\n'.join(definition(diagnostics,sig) for sig in (
   '        [[nodiscard]] static epochengine::core::time::simulation_stats timeline_stats_from_editor(',
   '        static void ensure_timeline_defaults(', '        static void advance_editor_timeline('))
  controls=inspector[inspector.index('        bool timelineControlsPrepared ='):inspector.index('        auto render_timeline_time_controls =')]
  render=inspector[inspector.index('        auto render_timeline_time_controls ='):inspector.index('            const std::string pacingHealth =')]+ '        };\n'
  row_layout=definition(chrome,'    struct InlineButtonRowLayout')+';\n'+''.join(definition(chrome,s) for s in (
   '    [[nodiscard]] static std::vector<float> resolve_inline_button_widths(',
   '    [[nodiscard]] static InlineButtonRowLayout make_inline_button_layout(',
   '    [[nodiscard]] static InlineButtonRowLayout resolve_inline_button_layout(',
   '    float inline_button_row_height('))
  code=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
namespace epochengine::core::time { CLOCK_STATS; }
OWNER
EDITOR_TYPES
struct EditorState{EditorTimeSnapshot timeSnapshot;EditorTimeControl timeControl;
 epochengine::timeline::TimelineState timelineState;
 std::vector<epochengine::timeline::TimelineTrack> timelineTracks;
 std::vector<epochengine::timeline::TimelineEvent> timelineEvents;};
bool request_project_runtime_presentation(EditorState&){return true;}
HELPERS
namespace epochengine{
std::string format_text(const char*,double,double,const char* state,double rate){return std::string(state)+" "+std::to_string(rate)+"x";}
std::string format_text(const char*,double value,const char* direction,double rate){return std::to_string(value)+"s "+direction+std::to_string(rate)+"x";}}
namespace gui{
struct Vec2{float x{},y{};};struct WidgetBounds{Vec2 position{},size{};};GUI_ICON
struct InlineButtonSpec{std::string_view label;float width{};bool enabled{true};ButtonIcon icon{ButtonIcon::none};std::string_view tooltip;};
enum class InlineButtonSizing{Fill,FitContent};enum class InlineButtonAlignment{Start,End,Center};
struct InlineButtonRowOptions{InlineButtonSizing sizing;InlineButtonAlignment alignment;bool wrap;float available_width;};
struct Frame{Vec2 cursor{};bool insideWindow{true};void*ctx{reinterpret_cast<void*>(1)};}g_frame;
float availableWidth{320};constexpr float kFontScale=1,kContentPadding=8,kBoxInnerPadding=4,kButtonTextClipInset=1;
float content_available_width(float){return availableWidth;}float space_advance(float){return 8.f;}float base_line_height(float){return 18.f;}
float measure_text_width(std::string_view s,float){return s.size()*8.f;}
GUI_LAYOUT
float line_height(){return 18.f;}float measure_wrapped_label_height(std::string_view,float){return 18.f;}
std::string readout;std::vector<std::string>labels,tooltips;std::optional<std::size_t>action;std::optional<float>scrub;
float compactHeight{};unsigned progressCount{};std::vector<ButtonIcon>icons;std::vector<WidgetBounds>buttonBounds;
void wrapped_label(std::string_view s,float){readout=s;}void label(std::string_view s){readout=s;}
void advance_cursor(Vec2 v){g_frame.cursor.x+=v.x;g_frame.cursor.y+=v.y;}Vec2 cursor_position(){return g_frame.cursor;}void set_cursor(Vec2 v){g_frame.cursor=v;}
struct ProgressBarOptions{float value;Vec2 size;bool show_percent;};void progress_bar(ProgressBarOptions){++progressCount;}
struct SliderOptions{std::string_view id;float minimum,maximum,value,step;Vec2 size;};
struct SliderResult{bool changed;float value;};SliderResult slider(SliderOptions o){return {scrub.has_value(),scrub.value_or(o.value)};}
template<std::size_t N>std::optional<std::size_t>inline_button_row(const std::array<InlineButtonSpec,N>&spec,float height,float gap,InlineButtonRowOptions options){
 labels.clear();icons.clear();tooltips.clear();for(const auto&button:spec){labels.emplace_back(button.label);icons.push_back(button.icon);tooltips.emplace_back(button.tooltip);}
 const auto layout=resolve_inline_button_layout(spec,height,gap,options);buttonBounds=layout.items;
 if(options.sizing!=InlineButtonSizing::FitContent||options.alignment!=InlineButtonAlignment::Center||!options.wrap)return {};
 return action;
}}
void actual_controls(EditorState&editor,bool compact,float width=320.f){gui::availableWidth=width;gui::g_frame.cursor={};CONTROLS RENDER gui::compactHeight=timeline_compact_controls_height(width);render_timeline_time_controls(width,compact);}
int main(){
 namespace t=epochengine::timeline;using C=t::PlaybackCommand;using D=t::PlaybackDirection;
 static_assert(static_cast<unsigned>(C::rewind)==0&&static_cast<unsigned>(C::toggle_forward)==2&&static_cast<unsigned>(C::toggle_reverse)==4);
 static_assert(static_cast<unsigned>(gui::ButtonIcon::reverse)==6);
 EditorState editor;editor.timeSnapshot.fixed_dt_seconds=.25;editor.timeSnapshot.simulated_seconds=10;
 advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==0&&editor.timelineTracks.size()>0);
 const auto eventCount=editor.timelineEvents.size();
 t::apply_playback_command(editor.timelineState,C::toggle_forward);
 editor.timeSnapshot.simulated_seconds=12;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==2);
 advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==2); // repeated snapshot is idempotent.
 t::apply_playback_command(editor.timelineState,C::toggle_reverse);
 editor.timeSnapshot.simulated_seconds=13;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==1&&editor.timelineState.direction==D::reverse);
 CHECK(editor.timeSnapshot.simulated_seconds==13&&editor.timelineEvents.size()==eventCount); // no reverse physics or event emission.
 t::pause(editor.timelineState);editor.timeSnapshot.simulated_seconds=23;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==1);
 t::seek_seconds(editor.timelineState,3.5);CHECK(editor.timelineState.playhead_seconds==3.5&&!editor.timelineState.playing);
 t::apply_playback_command(editor.timelineState,C::toggle_forward);editor.timeSnapshot.simulated_seconds=24;advance_editor_timeline(editor);
 const auto forwardSample=editor.timelineState.playhead_seconds;CHECK(forwardSample==4.5);
 t::seek_seconds(editor.timelineState,5.5);t::apply_playback_command(editor.timelineState,C::toggle_reverse);
 editor.timeSnapshot.simulated_seconds=25;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==forwardSample);
 t::seek_seconds(editor.timelineState,forwardSample);CHECK(editor.timelineState.playhead_seconds==forwardSample); // same sampling time, any direction.
 t::seek_seconds(editor.timelineState,-999);CHECK(editor.timelineState.playhead_seconds==0);
 t::apply_playback_command(editor.timelineState,C::toggle_reverse);CHECK(!editor.timelineState.playing);
 t::seek_seconds(editor.timelineState,999999);CHECK(editor.timelineState.playhead_seconds==editor.timelineState.duration_seconds);
 t::apply_playback_command(editor.timelineState,C::toggle_forward);CHECK(!editor.timelineState.playing);
 t::seek_seconds(editor.timelineState,1);t::apply_playback_command(editor.timelineState,C::toggle_reverse);
 editor.timeSnapshot.simulated_seconds=30;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==0&&!editor.timelineState.playing);
 const auto max=(std::numeric_limits<double>::max)(),nan=(std::numeric_limits<double>::quiet_NaN)(),inf=(std::numeric_limits<double>::infinity)();
 t::seek_seconds(editor.timelineState,6);t::seek_seconds(editor.timelineState,nan);t::scrub_seconds(editor.timelineState,inf);
 CHECK(editor.timelineState.playhead_seconds==6);editor.timeSnapshot.simulated_seconds=inf;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==6);
 editor.timeSnapshot.simulated_seconds=31;t::apply_playback_command(editor.timelineState,C::toggle_forward);advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==7);
 editor.timeSnapshot.simulated_seconds=-1;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==7);
 editor.timeSnapshot.simulated_seconds=1;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==7); // clock restart rebases without a jump.
 editor.timeSnapshot.simulated_seconds=2;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==8);
 editor.timeSnapshot.simulated_seconds=max;advance_editor_timeline(editor);CHECK(editor.timelineState.playhead_seconds==120&&!editor.timelineState.playing);
 t::scrub_seconds(editor.timelineState,-max);CHECK(editor.timelineState.playhead_seconds==0);
 auto malformed=editor.timelineState;malformed.duration_seconds=nan;malformed.fixed_dt_seconds=inf;malformed.playhead_seconds=nan;
 t::clamp_state(malformed);CHECK(malformed.duration_seconds==120&&std::isfinite(malformed.fixed_dt_seconds)&&malformed.playhead_frame==0);
 for(const auto direction:{D::forward,D::reverse}){
  t::TimelineState fast;t::seek_seconds(fast,50);epochengine::core::time::simulation_stats stats;
  stats.simulated_seconds=0;t::sync_to_simulation(fast,stats);
  const auto command=direction==D::forward?C::fast_forward:C::fast_reverse;
  const auto normal=direction==D::forward?C::toggle_forward:C::toggle_reverse;
  const double sign=direction==D::forward?1.0:-1.0;double expected=50;
  for(double rate:{2.0,4.0,8.0,2.0}){
   t::apply_playback_command(fast,command);CHECK(fast.playing&&fast.direction==direction&&fast.playback_rate==rate);
   stats.simulated_seconds+=.25;t::sync_to_simulation(fast,stats);expected+=.25*rate*sign;
   CHECK(fast.playhead_seconds==expected);t::sync_to_simulation(fast,stats);CHECK(fast.playhead_seconds==expected);
  }
  t::apply_playback_command(fast,normal);CHECK(fast.playing&&fast.playback_rate==1.0);
  stats.simulated_seconds+=1;t::sync_to_simulation(fast,stats);CHECK(fast.playhead_seconds==expected+sign);
  t::apply_playback_command(fast,normal);CHECK(!fast.playing&&fast.playback_rate==1.0);
  t::apply_playback_command(fast,command);CHECK(fast.playback_rate==2.0&&fast.playing);
  t::apply_playback_command(fast,direction==D::forward?C::fast_reverse:C::fast_forward);
  CHECK(fast.playback_rate==2.0&&fast.direction!=direction);
  fast.playback_rate=8.0;stats.simulated_seconds=max;t::sync_to_simulation(fast,stats);
  CHECK(std::isfinite(fast.playhead_seconds)&&!fast.playing);
  CHECK(fast.playhead_seconds==(fast.direction==D::forward?fast.duration_seconds:0.0));
 }
 for(double rate:{nan,inf,-inf,-1.0,0.0,max}){
  t::TimelineState invalid;invalid.playback_rate=rate;t::clamp_state(invalid);
  CHECK(std::isfinite(invalid.playback_rate)&&invalid.playback_rate>=1.0&&invalid.playback_rate<=8.0);
 }
 for(bool compact:{false,true}){
  EditorState ui;t::seek_seconds(ui.timelineState,5);gui::scrub.reset();gui::action=3;actual_controls(ui,compact);
  CHECK(ui.timelineState.playing&&ui.timelineState.direction==D::reverse&&!ui.timeControl.paused&&gui::labels.size()==7);
  CHECK(gui::labels[2]=="Fast Reverse"&&gui::labels[3]=="Reverse"&&gui::labels[4]=="Play"&&gui::labels[5]=="Fast Forward");
  CHECK(gui::tooltips[2].find("2x, 4x or 8x")!=std::string::npos&&gui::tooltips[5].find("2x, 4x or 8x")!=std::string::npos);
  gui::action=3;actual_controls(ui,compact);CHECK(!ui.timelineState.playing&&ui.timeControl.paused);
  gui::action=4;actual_controls(ui,compact);CHECK(ui.timelineState.playing&&ui.timelineState.direction==D::forward&&!ui.timeControl.paused);
  gui::action=1;actual_controls(ui,compact);CHECK(ui.timelineState.playhead_seconds==4&&!ui.timelineState.playing&&ui.timeControl.paused);
  gui::action=6;actual_controls(ui,compact);CHECK(ui.timelineState.playhead_seconds==5&&!ui.timelineState.playing);
  gui::action=99;actual_controls(ui,compact);CHECK(ui.timelineState.playhead_seconds==5&&!ui.timelineState.playing&&ui.timeControl.paused);
  gui::action=5;actual_controls(ui,compact);CHECK(ui.timelineState.playing&&ui.timelineState.playback_rate==2.0&&!ui.timeControl.paused);
  gui::action.reset();actual_controls(ui,compact);CHECK(gui::readout.find("2.000000x")!=std::string::npos);
  gui::action=2;actual_controls(ui,compact);CHECK(ui.timelineState.playing&&ui.timelineState.direction==D::reverse&&ui.timelineState.playback_rate==2.0);
  gui::action.reset();actual_controls(ui,compact);CHECK(gui::readout.find(compact?"-2.000000x":"Playing backward")!=std::string::npos);
  gui::action=4;actual_controls(ui,compact);CHECK(ui.timelineState.playing&&ui.timelineState.direction==D::forward&&ui.timelineState.playback_rate==1.0);
  gui::action.reset();gui::scrub=2.25f;actual_controls(ui,compact);CHECK(ui.timelineState.playhead_seconds==2.25&&!ui.timelineState.playing&&ui.timeControl.paused);
  gui::scrub.reset();gui::action=0;actual_controls(ui,compact);CHECK(ui.timelineState.playhead_seconds==0&&!ui.timelineState.playing);
 }
 for(float width:{80.f,160.f,280.f,304.f,320.f,800.f}){
  EditorState ui;t::seek_seconds(ui.timelineState,5);gui::action.reset();gui::scrub.reset();gui::progressCount=0;actual_controls(ui,true,width);
  CHECK(gui::progressCount==0&&gui::icons.size()==7&&gui::icons[2]==gui::ButtonIcon::fast_reverse&&gui::icons[3]==gui::ButtonIcon::reverse&&gui::icons[4]==gui::ButtonIcon::play&&gui::icons[5]==gui::ButtonIcon::fast_forward);
  CHECK(gui::g_frame.cursor.y==gui::compactHeight);for(const auto&b:gui::buttonBounds)CHECK(b.position.x>=0&&b.size.x>=20&&b.position.x+b.size.x<=width);
  if(width>=304)CHECK(gui::compactHeight==54.f);else CHECK(gui::compactHeight<=160.f);
  gui::action=4;actual_controls(ui,true,width);gui::action.reset();actual_controls(ui,true,width);CHECK(gui::icons[4]==gui::ButtonIcon::pause&&gui::icons[3]==gui::ButtonIcon::reverse);
  gui::action=3;actual_controls(ui,true,width);gui::action.reset();actual_controls(ui,true,width);CHECK(gui::icons[3]==gui::ButtonIcon::pause&&gui::icons[4]==gui::ButtonIcon::play);
 }
}
'''.replace('CLOCK_STATS',definition(clock,'    struct simulation_stats')).replace('OWNER',owner).replace('EDITOR_TYPES',
   definition(core,'    export struct EditorTimeSnapshot').replace('export ','',1)+';\n'+definition(core,'    export struct EditorTimeControl').replace('export ','',1)+';')
  code=code.replace('HELPERS',helpers).replace('CONTROLS',controls).replace('RENDER',render).replace('GUI_LAYOUT',row_layout).replace('GUI_ICON',definition(gui_api,'    export enum class ButtonIcon').replace('export ','',1)+';')
  proof=ROOT/'Artifacts/Reports/timeline-reverse-oct29'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  cpp=proof/'transport.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,proof)
  (proof/'result.json').write_text(json.dumps({'passed':True,'results':results,'scope':'Actual timeline commands, core snapshot type, central Editor helper and shared compact/full playback prefix; explicit GUI input ports. No water/physics replay or native rendering claim.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
