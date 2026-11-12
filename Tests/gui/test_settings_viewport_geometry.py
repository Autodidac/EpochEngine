"""Compile actual Settings geometry: no renderer/window/model is started."""
from pathlib import Path
import json
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]

class SettingsViewportGeometry(unittest.TestCase):
    def test_editor_and_project_camera_selection_are_independent(self):
        settings=(ROOT / "Engine/src/editor/application/editor.frame_consent_settings.inl").read_text(encoding="utf-8")
        owner=(ROOT / "Engine/src/editor/application/editor.application_scripting_renderers.inl").read_text(encoding="utf-8")
        start=settings.index("            const auto settingsCameraChoices = project_camera_choices();")
        block=settings[start:settings.index("            const std::array previewActions",start)]
        choices=definition(owner,"        [[nodiscard]] std::span<const ProjectCameraChoice> project_camera_choices()")
        label=definition(owner,"        [[nodiscard]] std::string_view project_camera_label(")
        code=r'''
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace previewgrid {enum class CameraMode {Editor,FPS,Canvas2D};int calls{};CameraMode selected{};
void set_camera_mode(void*,CameraMode mode){++calls;selected=mode;}}
struct ProjectCameraChoice{std::string_view label;previewgrid::CameraMode mode;};
CHOICES
LABEL
namespace gui {
struct Vec2{float x{},y{};};
struct SelectBoxOptions{std::string_view id,placeholder,selected;std::span<const std::string_view> options;Vec2 size;float row_height;std::size_t max_visible_options;bool enabled=true;};
struct SelectBoxResult{bool changed{};std::optional<std::size_t> selected_index;};
std::string_view chosenId;std::size_t chosenIndex{};bool wasEnabled{};
void set_cursor(Vec2){}void label(std::string_view){}
SelectBoxResult select_box(const SelectBoxOptions& options){if(options.id!=chosenId)return {};wasEnabled=options.enabled;return {true,chosenIndex};}
}
struct Editor{previewgrid::CameraMode viewportCameraMode=previewgrid::CameraMode::Editor,projectCameraMode=previewgrid::CameraMode::Editor;bool reserved{};};
bool editor_project_viewport_input_reserved(const Editor& editor){return editor.reserved;}
void ensure_2d_canvas_entity(Editor&){}void push_editor_log(Editor&,const std::string&){}
void draw(Editor& editor,const std::shared_ptr<int>& ctx){const gui::Vec2 contentPos{};const float settingsControlWidth=320;const auto settings_row=[](float=8.0f){};
BLOCK
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){Editor editor;auto ctx=std::make_shared<int>();
gui::chosenId="settings-editor-camera-select";gui::chosenIndex=1;draw(editor,ctx);
CHECK(gui::wasEnabled&&editor.viewportCameraMode==previewgrid::CameraMode::FPS);
CHECK(editor.projectCameraMode==previewgrid::CameraMode::Editor&&previewgrid::calls==1);
CHECK(previewgrid::selected==previewgrid::CameraMode::FPS);
gui::chosenId="settings-project-camera-select";gui::chosenIndex=2;draw(editor,ctx);
CHECK(editor.viewportCameraMode==previewgrid::CameraMode::FPS&&editor.projectCameraMode==previewgrid::CameraMode::Canvas2D&&previewgrid::calls==1);
editor.reserved=true;gui::chosenId="settings-editor-camera-select";gui::chosenIndex=0;draw(editor,ctx);
CHECK(!gui::wasEnabled&&editor.viewportCameraMode==previewgrid::CameraMode::FPS&&previewgrid::calls==1);
editor.reserved=false;draw(editor,{});CHECK(!gui::wasEnabled&&previewgrid::calls==1);
return 0;}
'''.replace("CHOICES",choices).replace("LABEL",label).replace("BLOCK",block)
        report=ROOT/"Artifacts/Reports/editor-camera-switch-oct31"/str(time.time_ns())
        report.mkdir(parents=True)
        source=report/"contract.cpp";source.write_text(code,encoding="utf-8")
        records=compile_and_run(self,source,report)
        (report/"result.json").write_text(json.dumps({"scope":"Actual Settings selection owner: independent Editor/Project camera and active Run/null-context refusal; CPU only","records":records},indent=2)+"\n",encoding="utf-8")

    def test_actual_viewport_fit_body_and_footer(self):
        bootstrap = (ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl").read_text(encoding="utf-8")
        settings = (ROOT / "Engine/src/editor/application/editor.frame_consent_settings.inl").read_text(encoding="utf-8")
        shell = bootstrap[bootstrap.index("        const gui::Vec2 editorSettingsDefaultSize"):bootstrap.index("        const gui::Vec2 aiModelManagerDefaultSize")]
        block = settings[settings.index("        if (editor.showSettingsModal && editorSettingsOwnsInput)"):]
        core = (ROOT / "Engine/dep/EpochGui/src/epochgui/floating_window.cpp").read_text(encoding="utf-8").replace("module;", "").replace("module epoch.gui;", "")
        module = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
        navigation = (ROOT / "Engine/src/epochgui/controls/gui.navigation_chrome.inl").read_text(encoding="utf-8")
        layout = definition(navigation, "    struct InlineButtonRowLayout", True) + definition(navigation, "    [[nodiscard]] static InlineButtonRowLayout make_inline_button_layout(")
        types = "\n".join(definition(module, "    export " + kind + " " + name, True).replace("export ", "") for kind,name in (
            ("struct","FloatingWindowOptions"),("struct","InlineButtonSpec"),
            ("enum class","InlineButtonSizing"),("enum class","InlineButtonAlignment"),("struct","InlineButtonRowOptions")))
        code = r"""
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "layout_primitives.hpp"
CORE
namespace lib=epochengine::gui_lib;
#define REQUIRE(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
namespace gui {
struct Vec2{float x{},y{};};struct WidgetBounds{Vec2 position,size;};
using FloatingWindowState=lib::FloatingWindowState;
enum class ButtonIcon{none};
TYPES
LAYOUT
struct Scope{};struct ScopedInputSuppression{};
Vec2 cursor{},bodyStart{},bodySize{};float contentRight{},lastBottom{},textHeight=24,footerY{};bool body{};int ended{};
std::vector<std::string> seen;
std::string_view clicked{},chosenId{};std::size_t chosenIndex{};
Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 p){cursor=p;}
void record(std::string_view id,Vec2 size){
 REQUIRE(size.x>0&&size.y>0);
 if(body){REQUIRE(cursor.y>=lastBottom);REQUIRE(cursor.x>=bodyStart.x);REQUIRE(cursor.x+size.x<=contentRight+0.01f);lastBottom=cursor.y+size.y;}
 seen.emplace_back(id);cursor.y+=size.y+6;
}
void label(std::string_view text){record(text,{100,textHeight});}
void wrapped_label(std::string_view text,float width){record(text,{width,textHeight*std::max(1.0f,std::ceil(float(text.size())*9/width))});}
void property_row(std::string_view a,std::string_view,float){label(a);}
void section_heading(std::string_view text){record(text,{100,textHeight+18});}
void normalize_floating_window(FloatingWindowState& state,const FloatingWindowOptions& o){lib::normalize_floating_window(state,{
 .default_position={o.default_position.x,o.default_position.y},.default_size={o.default_size.x,o.default_size.y},
 .min_size={o.min_size.x,o.min_size.y},.viewport_size={o.viewport_size.x,o.viewport_size.y},.constrain_to_viewport=o.constrain_to_viewport});}
struct FloatingWindowResult{bool begun=true,close_requested{};WidgetBounds content;};
FloatingWindowResult begin_floating_window(FloatingWindowState& state,const FloatingWindowOptions&){cursor={state.position.x+6,state.position.y+38};return {true,false,{cursor,{state.size.x-12,state.size.y-44}}};}
void end_floating_window(){++ended;}
struct ScrollAreaOptions{std::string_view id;Vec2 size;float content_height{};bool draw_background{},show_scrollbar{};};
bool begin_scroll_area(const ScrollAreaOptions& o){body=true;bodyStart=cursor;bodySize=o.size;contentRight=cursor.x+o.size.x-12;lastBottom=cursor.y;return true;}
void end_scroll_area(){body=false;footerY=bodyStart.y+bodySize.y;}
InlineButtonRowLayout row(std::span<const InlineButtonSpec> items,float h,float gap,const InlineButtonRowOptions& o){std::vector<float> widths;for(auto item:items)widths.push_back(float(item.label.size())*9+30);return make_inline_button_layout(widths,o.available_width,std::max(h,textHeight+8),gap,o.wrap,o.alignment);}
float inline_button_row_height(std::span<const InlineButtonSpec> items,float h,float gap,const InlineButtonRowOptions& o){return row(items,h,gap,o).height;}
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> items,float h,float gap,const InlineButtonRowOptions& o){
 auto layout=row(items,h,gap,o);REQUIRE(layout.valid);const auto start=cursor;std::optional<std::size_t> result;
 if(body)REQUIRE(cursor.y>=lastBottom);else REQUIRE(cursor.y>=footerY+12);
 for(std::size_t i=0;i<items.size();++i){seen.emplace_back(items[i].label);auto bounds=layout.items[i];REQUIRE(bounds.position.x>=0&&bounds.position.x+bounds.size.x<=o.available_width);if(items[i].label==clicked)result=i;}
 cursor={start.x,start.y+layout.height+6};if(body)lastBottom=start.y+layout.height;return result;}
struct SelectBoxOptions{std::string_view id,placeholder,selected;std::span<const std::string_view> options;Vec2 size;float row_height{};std::size_t max_visible_options{};bool enabled=true;};
struct SelectBoxResult{bool changed{};std::optional<std::size_t> selected_index;};
SelectBoxResult select_box(const SelectBoxOptions& o){record(o.id,o.size);return o.id==chosenId&&o.enabled?SelectBoxResult{true,chosenIndex}:SelectBoxResult{};}
bool toggle_switch(std::string_view text,bool&,Vec2 size){record(text,size);return false;}
enum class ThemePreference{System,ThemeCycle};struct ThemeChoice{std::string_view label;ThemePreference preference;};
auto theme_preference_choices(){return std::array{ThemeChoice{"System",ThemePreference::System},ThemeChoice{"Cycle",ThemePreference::ThemeCycle}};}
std::string_view theme_preference_label(ThemePreference p){return p==ThemePreference::System?"System":"Cycle";}
struct CycleChoice{std::string_view label;double seconds;};auto theme_cycle_interval_choices(){return std::array{CycleChoice{"One minute",60}};}
std::string_view theme_cycle_interval_label(double){return "One minute";}void set_theme_cycle_interval(double){}
}
namespace core {enum class ScenePreviewMode{Editor,None};}
namespace perf {enum class frame_pacing_mode{target_hz,uncapped};}
namespace input {void set_active_profile(int){}}
namespace previewgrid {enum class CameraMode{Editor,FPS,Canvas2D};void set_camera_mode(void*,CameraMode){}}
struct Context{struct {struct {void publish(perf::frame_pacing_mode,double){}} editorFramePacingOverride;} data;decltype(data)*windowData=&data;bool focused=true;bool has_input_focus_safe(){return focused;}void set_scene_preview_mode(core::ScenePreviewMode){}};
enum class TopMenu{None};enum class EditorMainSurface{Project};
struct Editor {bool showSettingsModal=true,showAiModelManagerModal{},roundedRectangles=true,editorFrameVsyncRequested{},helpersVisible=true,reserved{};
 gui::FloatingWindowState editorSettingsWindow{};TopMenu openMenu{};int projectRunBackend{},inputProfilePreset{},projectWorkspaceSection{},aiModelManagerSection{},aiModelManagerTab{},aiMethodSelection{};
 gui::ThemePreference themePreference=gui::ThemePreference::System;double themeCycleIntervalSeconds=60,editorFrameLimitFps=120,projectRunFrameLimitFps=30;bool editorFrameLimitEdit{},projectFrameLimitEdit{};
 std::string contextSelectionStatus="Ready";core::ScenePreviewMode previewMode=core::ScenePreviewMode::Editor;
 previewgrid::CameraMode viewportCameraMode{},projectCameraMode{};};
struct ContextBackendOption{bool selectable=true;std::string_view label="OpenGL";};
auto available_context_backend_options(){return std::array{ContextBackendOption{}};}bool editor_context_replacement_in_progress(){return false;}
void request_context_backend_selection(const ContextBackendOption&){}std::string_view renderer_name(const auto&){return "OpenGL";}
std::string context_backend_summary(const auto&){return "OpenGL Vulkan SDL SFML Raylib Software";}
struct Profile{int renderer_capability{};};const Profile& active_project_profile(Editor&){static Profile p;return p;}
int renderer_backend_kind(const auto&){return 0;}std::string renderer_admission_summary(int,int){return "Admitted";}
std::string disabled_context_backend_summary(const auto&){return "none";}void push_editor_log(Editor&,const std::string&){}
struct InputChoice{std::string_view label;int preset;};auto input_profile_choices(){return std::array{InputChoice{"Default",0}};}
std::string_view input_profile_label(int){return "Default";}bool has_open_project(Editor&){return true;}void open_editor_surface(EditorMainSurface,std::string_view){}
std::string_view frame_limit_label(double){return "120";}std::string frame_limit_argument(double){return "30";}
bool frame_limit_control(std::string_view id,double&,bool& manual,float width,bool=false){gui::record(id,{width,manual?96.0f:32.0f});return false;}
struct CameraChoice{std::string_view label;previewgrid::CameraMode mode;};auto project_camera_choices(){return std::array{CameraChoice{"Editor",previewgrid::CameraMode::Editor},CameraChoice{"FPS",previewgrid::CameraMode::FPS},CameraChoice{"2D",previewgrid::CameraMode::Canvas2D}};}
std::string_view project_camera_label(previewgrid::CameraMode){return "Editor";}bool editor_project_viewport_input_reserved(Editor& e){return e.reserved;}
void persist_project_run_settings(Editor&){}
bool editor_project_viewport_covered(Editor&){return false;}void ensure_2d_canvas_entity(Editor&){}void handle_scene_tool(Editor&,void*,std::string_view){}void reset_editor_layout(Editor&){}
void draw(Editor& editor,std::shared_ptr<Context> ctx,float w,float h) {
SHELL
bool editorSettingsOwnsInput=true;
const bool automationInputReserved=false;
std::optional<gui::ScopedInputSuppression> editorSettingsBackgroundInput;editorSettingsBackgroundInput.emplace();
BLOCK
REQUIRE(editorSettingsBackgroundInput.has_value());
REQUIRE(editor.editorSettingsWindow.position.x>=0&&editor.editorSettingsWindow.position.y>=0);
REQUIRE(editor.editorSettingsWindow.position.x+editor.editorSettingsWindow.size.x<=w+0.01f);
REQUIRE(editor.editorSettingsWindow.position.y+editor.editorSettingsWindow.size.y<=h+0.01f);
REQUIRE(gui::cursor.y<=editor.editorSettingsWindow.position.y+editor.editorSettingsWindow.size.y);
}
int main(){auto ctx=std::make_shared<Context>();
for(auto viewport:{gui::Vec2{320,220},gui::Vec2{480,270},gui::Vec2{600,360},gui::Vec2{960,540},gui::Vec2{1920,1080},gui::Vec2{3840,2160}})
 for(bool cycling:{false,true})for(bool manual:{false,true})for(float font:{18.0f,30.0f}){
 Editor e;e.themePreference=cycling?gui::ThemePreference::ThemeCycle:gui::ThemePreference::System;e.editorFrameLimitEdit=e.projectFrameLimitEdit=manual;
 gui::textHeight=font;gui::seen.clear();draw(e,ctx,viewport.x,viewport.y);
 REQUIRE(e.themePreference==(cycling?gui::ThemePreference::ThemeCycle:gui::ThemePreference::System));
 for(auto id:{"editor-settings-context-select","editor-theme-select","editor-input-profile-select","editor-core-frame-limit-select","settings-project-frame-limit-select","settings-editor-camera-select","settings-project-camera-select","Open Project Input Manager","Reset Layout","Model Manager","Close","Reset Camera"})
  REQUIRE(std::find(gui::seen.begin(),gui::seen.end(),id)!=gui::seen.end());
 REQUIRE((std::find(gui::seen.begin(),gui::seen.end(),"editor-theme-cycle-interval-select")!=gui::seen.end())==cycling);
 e.editorSettingsWindow.position={10000,10000};e.editorSettingsWindow.size={10000,10000};draw(e,ctx,viewport.x,viewport.y);
 }
for(auto action:{"Close","Model Manager","Open Project Input Manager"}){Editor e;gui::clicked=action;draw(e,ctx,960,540);REQUIRE(!e.showSettingsModal);if(gui::clicked=="Model Manager")REQUIRE(e.showAiModelManagerModal);}
return 0;}
""".replace("CORE",core).replace("TYPES",types).replace("LAYOUT",layout).replace("SHELL",shell).replace("BLOCK",block)
        report = ROOT / "Artifacts/Reports/settings-responsive-nov01" / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "contract.cpp"
        source.write_text(code, encoding="utf-8")
        records = compile_and_run(self, source, report, includes=(ROOT/"Engine/dep/EpochGui/include/gui",), windows_flags=("/WX",), compile_timeout=90, run_timeout=10)
        (report / "result.json").write_text(json.dumps({"scope":"Actual Settings owner, actual shared floating normalizer and fitted row layout; viewport fit, non-overlapping body, measured footer, System/Cycle/manual/large-font/all-options; CPU only", "records":records}, indent=2) + "\n", encoding="utf-8")

if __name__=="__main__":unittest.main()
