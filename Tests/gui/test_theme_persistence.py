"""Execute the real theme codec and resolver; native window acceptance is separate."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


def contract_source(root=ROOT):
    api = (root/'Engine/modules/gui.engine.ixx').read_text(encoding='utf-8')
    gui = (root/'Engine/src/epochgui/gui.engine.cpp').read_text(encoding='utf-8')
    state = (root/'Engine/src/editor/application/editor.application_state.inl').read_text(encoding='utf-8')
    routing = (root/'Engine/src/editor/application/editor.application_pane_routing.inl').read_text(encoding='utf-8')
    inventory = state[state.index('        struct EditorWorkspaceSpec final'):state.index('        struct EditorState',state.index('        struct EditorWorkspaceSpec final'))]
    workspace_helpers = '\n'.join(definition(routing, marker) for marker in (
        '        [[nodiscard]] static bool editor_workspace_supported(',
        '        [[nodiscard]] static bool editor_workspace_active(',
        '        [[nodiscard]] static bool editor_workspace_visible(',
        '        static bool set_editor_workspace_visible(',
        '        static void reveal_editor_workspace_surface(',
        '        [[nodiscard]] static const EditorWorkspaceSpec* editor_workspace_fallback('))
    default_theme = re.search(r'gui::ThemePreference themePreference\{[^;]+;', state).group(0)
    codec = (root/'Engine/src/editor/application/editor.application_layout_persistence.inl').read_text(encoding='utf-8')
    # Only the Public codec is needed. Its theme bytes/branches are unchanged;
    # unrelated private project-iteration settings are not part of this fixture.
    codec = re.sub(r'// EPOCH_' r'PRIVATE_SOURCE_BEGIN:[^\n]*\n.*?// EPOCH_'
                   r'PRIVATE_SOURCE_END:[^\n]*\n', '', codec, flags=re.S)
    enums = '\n'.join(definition(api, '    export enum class '+name, True)
        .replace('export enum class', 'enum class', 1)
        for name in ('ThemePreference', 'ThemeVariant'))
    choices = '\n'.join(definition(api, '    export struct '+name, True)
        .replace('export struct', 'struct', 1)
        for name in ('ThemePreferenceChoice', 'ThemeCycleIntervalChoice'))
    functions = '\n'.join(definition(gui, marker) for marker in (
        '    std::span<const ThemePreferenceChoice> theme_preference_choices(',
        '    std::string_view theme_preference_label(',
        '    std::span<const ThemeCycleIntervalChoice> theme_cycle_interval_choices(',
        '    std::uint32_t normalize_theme_cycle_interval(',
        '    ThemeVariant resolve_theme_preference('))
    codec_functions = '\n'.join(definition(codec, marker) for marker in (
        '        [[nodiscard]] static gui::ThemePreference snapshot_theme_preference(',
        '        [[nodiscard]] static std::string serialize_editor_layout_preferences(',
        '        [[nodiscard]] static bool valid_layout_boolean(',
        '        [[nodiscard]] static bool valid_active_pane_route(',
        '        [[nodiscard]] static std::string first_open_pane_route(',
        '        [[nodiscard]] static bool decode_editor_layout_preferences('))
    return r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace gui {
ENUMS
CHOICES
bool os_dark=true;
bool system_prefers_dark_palette() {return os_dark;}
struct CycleProbe {
 std::size_t index{};bool deactivated{};
 std::size_t slot(std::chrono::steady_clock::time_point) {deactivated=false;return index;}
 void deactivate() {deactivated=true;}
} g_themeCycle;
FUNCTIONS
}
// Small pane-state fixture: geometry/focus are unrelated to theme persistence.
enum class EditorApplicationKind {Standard};
enum class EditorMainSurface {Scene,Game2D,Assets,Project,ForestFactory,PlantLab,Timeline,AISandbox,Systems,AssetGeneration};
struct EditorApplicationProfile{};
EditorApplicationProfile editor_application_profile(EditorApplicationKind){return {};}
bool editor_application_supports_surface(EditorApplicationProfile,EditorMainSurface){return true;}
bool editor_application_owns_dedicated_gui_workspace(EditorApplicationKind){return false;}
enum class EditorToolPane {Outliner,Properties,Output,AiChat};
enum class EditorToolDockRegion {Left,Right,BottomLeft,BottomRight};
constexpr std::size_t kEditorMainSurfaceCount=1,kEditorToolPaneCount=4;
std::size_t editor_main_surface_index(EditorMainSurface) {return 0;}
std::size_t tool_pane_index(EditorToolPane p) {return static_cast<std::size_t>(p);}
constexpr auto editor_tool_pane_routes() {return std::array<std::string_view,4>{"pane.world_outliner","pane.properties","pane.output","pane.ai_chat"};}
std::optional<EditorToolPane> editor_tool_pane_from_route(std::string_view route) {
 const auto routes=editor_tool_pane_routes();
 for(std::size_t i=0;i<routes.size();++i) if(routes[i]==route)return static_cast<EditorToolPane>(i);
 return {};
}
auto default_tool_pane_tab_order() {return std::array<std::uint32_t,4>{0,1,2,3};}
namespace gui_lib {float normalize_bottom_dock_column_fraction(float f) {return std::clamp(f,0.25f,0.75f);}}
WORKSPACE_INVENTORY
struct EditorState {
 bool workspaceVisibilityCustomized{},game2DGuiWorkspace{};std::vector<std::string>visibleWorkspaceIds;
 EditorApplicationKind applicationKind{};EditorMainSurface mainSurface{},dockPreferenceSurface{};
 float bottomGridSplit=.5f,outlinerSplit=.2f,inspectorSplit=.2f,dockSplit=.3f;
 std::array<float,1> workspaceBottomGridSplits{.5f},workspaceDockSplits{.3f};
 std::array<bool,1> workspaceOutputFollow{true},workspaceAiChatFollow{true};
 bool outputFollowTail=true,aiChatFollowTail=true,showOutliner=true,showInspector=true,showConsoleDock=true,showAiChat=true,roundedRectangles=true;
 std::array<bool,4> toolPaneOpen{true,true,true,true};
 std::array<EditorToolDockRegion,4> toolPaneDockRegions{EditorToolDockRegion::Left,EditorToolDockRegion::Right,EditorToolDockRegion::BottomLeft,EditorToolDockRegion::BottomRight};
 std::array<std::uint32_t,4> toolPaneTabOrder{0,1,2,3};
 std::string activeLeftPaneRoute="pane.world_outliner",activeRightPaneRoute="pane.properties",activeBottomLeftPaneRoute="pane.output",activeBottomRightPaneRoute="pane.ai_chat";
 DEFAULT_THEME
 std::uint32_t themeCycleIntervalSeconds=30;
};
WORKSPACE_HELPERS
CODEC
bool contract_check(bool value) noexcept {return value;}
#define CHECK(x) do {if(!contract_check(static_cast<bool>(x)))return __LINE__;}while(false)
int main() {
 using gui::ThemePreference;using gui::ThemeVariant;
 EditorState workspace;reveal_editor_workspace_surface(workspace,EditorMainSurface::Scene);
 CHECK(editor_workspace_fallback(workspace,{})!=nullptr);
 CHECK(static_cast<unsigned>(ThemePreference::GraphiteCopper)==14u);
 CHECK(static_cast<unsigned>(ThemePreference::SlateOrchard)==15u);
 CHECK(static_cast<unsigned>(ThemePreference::IndigoQuartz)==16u);
 CHECK(static_cast<unsigned>(ThemePreference::SignalPlum)==17u);
 CHECK(static_cast<unsigned>(ThemePreference::ForgedSteel)==18u);
 CHECK(EditorState{}.themePreference==ThemePreference::FollowSystemDark);
 const std::array<unsigned,13> migrated{0,1,2,3,4,5,6,7,8,3,10,8,12};
 for(unsigned ordinal=0;ordinal<=18;++ordinal) {
  EditorState original;original.themePreference=static_cast<ThemePreference>(ordinal);
  original.themeCycleIntervalSeconds=600;original.roundedRectangles=false;
  const auto encoded=serialize_editor_layout_preferences(original);
  CHECK(encoded.find("\ntheme "+std::to_string(ordinal)+" 0\n")!=std::string::npos);
  EditorState reopened;CHECK(decode_editor_layout_preferences(reopened,encoded));
  const unsigned expected=ordinal<13?migrated[ordinal]:ordinal==13?0u:ordinal;
  CHECK(static_cast<unsigned>(reopened.themePreference)==expected);
  CHECK(reopened.themeCycleIntervalSeconds==600 && !reopened.roundedRectangles);
  const auto again=serialize_editor_layout_preferences(reopened);
  EditorState repeated;CHECK(decode_editor_layout_preferences(repeated,again));
  CHECK(repeated.themePreference==reopened.themePreference);
 }
 EditorState rejected;rejected.themePreference=ThemePreference::GraphiteCopper;
 EditorState invalid;invalid.themePreference=static_cast<ThemePreference>(19);
 CHECK(!decode_editor_layout_preferences(rejected,serialize_editor_layout_preferences(invalid)));
 CHECK(rejected.themePreference==ThemePreference::GraphiteCopper);
 CHECK(gui::theme_preference_label(ThemePreference::GraphiteCopper)=="Graphite Copper");
 CHECK(gui::resolve_theme_preference(ThemePreference::GraphiteCopper)==ThemeVariant::GraphiteCopper);
 CHECK(gui::theme_preference_label(ThemePreference::SlateOrchard)=="Slate Orchard");
 CHECK(gui::resolve_theme_preference(ThemePreference::SlateOrchard)==ThemeVariant::SlateOrchard);
 CHECK(gui::theme_preference_label(ThemePreference::IndigoQuartz)=="Indigo Quartz");
 CHECK(gui::resolve_theme_preference(ThemePreference::IndigoQuartz)==ThemeVariant::IndigoQuartz);
 CHECK(gui::theme_preference_label(ThemePreference::SignalPlum)=="Signal Plum");
 CHECK(gui::resolve_theme_preference(ThemePreference::SignalPlum)==ThemeVariant::SignalPlum);
 CHECK(gui::theme_preference_label(ThemePreference::ForgedSteel)=="Forged Steel");
 CHECK(gui::resolve_theme_preference(ThemePreference::ForgedSteel)==ThemeVariant::ForgedSteel);
 CHECK(gui::theme_preference_label(ThemePreference::CodexBlack)=="Codex Black");
 CHECK(gui::theme_preference_label(ThemePreference::ClassicLauncher)=="Classic Theme");
 unsigned slateChoices{};
 for(const auto& choice:gui::theme_preference_choices())if(choice.preference==ThemePreference::SlateOrchard){CHECK(choice.label=="Slate Orchard");++slateChoices;}
 CHECK(slateChoices==1);
 const std::array expectedCycle{ThemeVariant::DefaultDark,ThemeVariant::DefaultLight,ThemeVariant::ClassicLauncher,ThemeVariant::MidnightBlue,ThemeVariant::EmberForge,ThemeVariant::ForestTerminal,ThemeVariant::AuroraSteel,ThemeVariant::CodexBlack,ThemeVariant::GraphiteCopper,ThemeVariant::SlateOrchard,ThemeVariant::IndigoQuartz,ThemeVariant::SignalPlum,ThemeVariant::ForgedSteel};
 for(std::size_t i=0;i<expectedCycle.size()*2;++i) {
  gui::g_themeCycle.index=i;
  CHECK(gui::resolve_theme_preference(ThemePreference::ThemeCycle)==expectedCycle[i%expectedCycle.size()]);
 }
 gui::os_dark=false;CHECK(gui::resolve_theme_preference(ThemePreference::FollowSystemDark)==ThemeVariant::DefaultLight);
 gui::os_dark=true;CHECK(gui::resolve_theme_preference(ThemePreference::FollowSystemDark)==ThemeVariant::DefaultDark);
 CHECK(gui::g_themeCycle.deactivated);
}
'''.replace('ENUMS', enums).replace('CHOICES', choices).replace('FUNCTIONS', functions).replace('CODEC', codec_functions).replace('DEFAULT_THEME', default_theme).replace('WORKSPACE_INVENTORY',inventory).replace('WORKSPACE_HELPERS',workspace_helpers)


class ThemePersistence(unittest.TestCase):
    def test_actual_codec_retains_new_and_legacy_theme_identities(self):
        parent = ROOT/'Artifacts/Reports/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='theme-persistence-', dir=parent))
        try:
            cpp = scratch/'contract.cpp'
            cpp.write_text(contract_source(), encoding='utf-8')
            if os.name == 'nt':
                toolsets = msvc_2022_toolsets()
                sdk = Path('C:/Program Files (x86)/Windows Kits/10')
                kits = windows_sdk_includes(sdk)
                if not toolsets or not kits:
                    self.skipTest('Installed MSVC/SDK required')
                tool, kit = toolsets[-1], kits[-1]
                compiler = str(tool/'bin/Hostx64/x64/cl.exe')
                common = [compiler,'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX',
                    '/I'+str(tool/'include'),'/I'+str(kit/'ucrt')]
                link = ['/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
                modes = [('Debug',['/Od','/MDd','/D_DEBUG']),('Release',['/O2','/MD','/DNDEBUG'])]
            else:
                compiler = shutil.which('clang++') or shutil.which('g++')
                if not compiler:
                    self.skipTest('C++23 compiler required')
                common,link,modes = [compiler,'-std=c++23'],[],[('Debug',['-O0']),('Release',['-O2','-DNDEBUG'])]
            for name, flags in modes:
                with self.subTest(configuration=name):
                    binary=scratch/(name+('.exe' if os.name=='nt' else ''))
                    output=['/Fo'+str(scratch/(name+'.obj')),'/Fe'+str(binary)] if os.name=='nt' else ['-o',str(binary)]
                    built=subprocess.run(common+flags+output+[str(cpp)]+link,cwd=scratch,capture_output=True,text=True,timeout=60)
                    self.assertEqual(built.returncode,0,built.stdout[-9000:]+built.stderr)
                    ran=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
                    self.assertEqual(ran.returncode,0,'Actual codec/resolver check failed at generated C++ line '+str(ran.returncode)+': '+ran.stdout+ran.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)


if __name__ == '__main__':
    unittest.main()
