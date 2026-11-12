"""Actual appearance query and layout codec; launcher has no palette override."""
from pathlib import Path
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_theme_persistence import contract_source
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class LauncherTheme(unittest.TestCase):
    def test_saved_and_live_appearance_without_editor_initialization(self):
        owner = (ROOT / 'Engine/src/editor/application/editor.application_context_lifecycle.inl').read_text(encoding='utf-8')
        query = definition(owner, '    EditorAppearancePreferences editor_appearance_preferences(')
        self.assertNotIn('editor_state_for(', query)
        api = (ROOT / 'Engine/modules/editor/editor.core.ixx').read_text(encoding='utf-8')
        appearance = definition(api, '    export struct EditorAppearancePreferences', True).replace('export ', '', 1)
        codec = contract_source().split('int main() {', 1)[0]
        code = '#include <map>\n#include <mutex>\n' + codec + '\n' + appearance + r'''
namespace core {struct Context {};}
struct Storage {std::mutex mutex;std::map<const core::Context*,EditorState> states;};
Storage& editor_storage(){static Storage value;return value;}
unsigned loads{};std::string saved;
// File access is an explicit port; the actual shared codec performs admission.
void load_editor_layout_preferences(EditorState& value){++loads;(void)decode_editor_layout_preferences(value,saved);}
QUERY
int main(int argc,char**){
 EditorState authored;authored.themePreference=gui::ThemePreference::GraphiteCopper;
 authored.themeCycleIntervalSeconds=60;authored.roundedRectangles=false;
 saved=argc>1?"invalid layout":serialize_editor_layout_preferences(authored);
 core::Context first,second;
 auto cold=editor_appearance_preferences(&first);
 CHECK(loads==1&&editor_storage().states.empty());
 CHECK(cold.theme==(argc>1?gui::ThemePreference::FollowSystemDark:gui::ThemePreference::GraphiteCopper));
 CHECK(cold.theme_cycle_seconds==(argc>1?30u:60u));
 CHECK(cold.rounded_rectangles==(argc>1));
 for(unsigned i=0;i<100;++i){CHECK(editor_appearance_preferences(&second).theme==cold.theme);}
 CHECK(loads==1&&editor_storage().states.empty());
 auto& live=editor_storage().states[&first];
 for(const auto choice:gui::theme_preference_choices()){
  live.themePreference=choice.preference;live.themeCycleIntervalSeconds=120;live.roundedRectangles=true;
  const auto value=editor_appearance_preferences(&first);
  CHECK(value.theme==choice.preference&&value.theme_cycle_seconds==120&&value.rounded_rectangles);
 }
 live.themePreference=gui::ThemePreference::FollowSystemDark;
 gui::os_dark=false;CHECK(gui::resolve_theme_preference(editor_appearance_preferences(&first).theme)==gui::ThemeVariant::DefaultLight);
 gui::os_dark=true;CHECK(gui::resolve_theme_preference(editor_appearance_preferences(&first).theme)==gui::ThemeVariant::DefaultDark);
 live.themePreference=gui::ThemePreference::ThemeCycle;
 gui::g_themeCycle.index=4;CHECK(gui::resolve_theme_preference(editor_appearance_preferences(&first).theme)==gui::ThemeVariant::EmberForge);
 CHECK(loads==1&&editor_storage().states.size()==1&&saved==(argc>1?"invalid layout":serialize_editor_layout_preferences(authored)));
}
'''.replace('QUERY', query)
        report = ROOT / 'Artifacts/Reports/launcher-theme-oct31/appearance'
        report.mkdir(parents=True, exist_ok=True)
        source = report / 'appearance.cpp'
        source.write_text(code, encoding='utf-8')
        results = compile_and_run(self, source, report)
        for mode in ('Debug', 'Release'):
            binary = report / (mode + ('.exe' if os.name == 'nt' else ''))
            result = subprocess.run([str(binary), 'malformed'], cwd=report, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        (report / 'result.json').write_text(json.dumps({'actual_query_and_codec': results,
            'malformed_fallback': 'Debug/Release PASS', 'native_visual_acceptance': False}, indent=2)+'\n')

    def test_all_launcher_surfaces_inherit_the_owner_theme(self):
        menu = (ROOT / 'Engine/modules/gui/gui.menu.ixx').read_text(encoding='utf-8')
        self.assertNotIn('gui::push_theme(', menu)
        self.assertNotIn('gui::pop_theme(', menu)
        for name, begin in (('epoch.session_runtime.inl', 'case SessionMode::Menu:'),
                            ('epoch.session_editor_launcher.inl', 'case SceneID::Menu:')):
            source = (ROOT / 'Engine/src/epoch/implementation' / name).read_text(encoding='utf-8')
            launcher = source[source.index(begin):]
            self.assertNotIn('gui::ThemeVariant::ClassicLauncher', launcher)
            self.assertIn('editor_appearance_preferences(ctx.get())', launcher)
            self.assertIn('gui::ScopedTheme launcherTheme{launcherAppearance.theme}', launcher)
            self.assertIn('gui::publish_native_window_theme(launcherAppearance.theme)', launcher)


if __name__ == '__main__':
    unittest.main()
