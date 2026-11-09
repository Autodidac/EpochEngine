"""Compile the real registry/parser and Project browser with recording UI adapters.

No Engine build, window, child, model, or operator project is touched. Filesystem
discovery and admission run on checkout-local manifests; only UI/native/session
boundaries are recording substitutes.
"""
from pathlib import Path
import json
import time
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class ProjectBrowser(unittest.TestCase):
    def test_actual_discovery_picker_selection_refresh_and_admission(self):
        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        core = (ROOT / "Engine/modules/editor.core.ixx").read_text(encoding="utf-8")
        app = (ROOT / "Engine/src/editor/application/editor.application_scripting_renderers.inl").read_text(encoding="utf-8")
        menus = (ROOT / "Engine/src/editor/application/editor.frame_command_menus.inl").read_text(encoding="utf-8")
        gui = (ROOT / "Engine/modules/gui.engine.ixx").read_text(encoding="utf-8")
        def take(text, marker, semicolon=False):
            # The shared extractor treats a default path's {} as a body.
            if "resolve_projects_root(" in marker or "resolve_repo_relative_path(" in marker:
                text = text.replace("hint = {}", "hint = fs::path()")
            return definition(text, marker, semicolon).replace("export ", "", 1)
        types = "\n".join(take(core, "    export " + marker, True) for marker in (
            "enum class EditorProjectKind", "struct EditorProjectProfile", "struct EditorProjectAdmissionResult"))
        registry_code = take(registry, "    struct OwnedProjectProfile", True)
        registry_code += '\nconst std::array<EditorProjectProfile,1> kProjectProfiles{{{.id="builtin"}}};\n'
        registry_code += 'const EditorProjectProfile kEngineDevelopmentCompatibilityProfile{.id="development"};\n'
        registry_code += "\n".join(take(registry, marker, semicolon) for marker, semicolon in (
            ("    [[nodiscard]] static std::vector<OwnedProjectProfile>& discovered_project_profiles()", False),
            ("    [[nodiscard]] static std::vector<OwnedProjectProfile>& admitted_project_profiles()", False),
            ("    [[nodiscard]] static std::vector<EditorProjectProfile>& cached_project_profiles()", False),
            ("    [[nodiscard]] static bool& project_profiles_cache_dirty()", False),
            ("    static void invalidate_project_profile_cache()", False),
            ("    [[nodiscard]] static std::string read_text_file(", False),
            ("    [[nodiscard]] static bool project_path_below(", False),
            ("    [[nodiscard]] static fs::path candidate_project_path_lexical(", False),
            ("    [[nodiscard]] static fs::path candidate_project_path(\n", False),
            ("    [[nodiscard]] static fs::path resolve_projects_root(", False),
            ("    [[nodiscard]] static fs::path resolve_project_root_path(", False),
            ("    [[nodiscard]] static fs::path resolve_repo_relative_path(", False),
            ("    enum class JsonStringFieldState", True),
            ("    struct JsonStringFieldResult", True),
            ("    [[nodiscard]] static JsonStringFieldResult inspect_json_string_field(", False),
            ("    [[nodiscard]] static std::optional<std::string> extract_json_string_field(", False),
            ("    [[nodiscard]] static std::optional<EditorProjectKind> parse_project_manifest_kind(", False),
            ("    [[nodiscard]] static std::string parse_world_name(", False),
            ("    [[nodiscard]] static bool is_builtin_project_id(", False),
            ("    [[nodiscard]] static bool is_launcher_application_id(", False),
            ("    [[nodiscard]] static std::optional<OwnedProjectProfile>\n        parse_manifest_project_profile(", False),
            ("    static void rebuild_project_profile_cache()", False),
            ("    [[nodiscard]] static const std::vector<EditorProjectProfile>& live_project_profiles()", False),
        ))
        registry_code += "\n".join(take(scene, marker) for marker in (
            "    EditorProjectAdmissionResult editor_admit_project_manifest(",
            "    void editor_refresh_project_profiles()", "    std::filesystem::path editor_projects_root()",
            "    std::span<const EditorProjectProfile> editor_project_profiles()"))
        gui_types = "\n".join(take(gui, "    export " + marker, True) for marker in (
            "enum class ButtonIcon", "struct InlineButtonSpec", "enum class InlineButtonSizing",
            "enum class InlineButtonAlignment", "struct InlineButtonRowOptions", "struct SelectBoxOptions", "struct SelectBoxResult"))
        app_code = "\n".join(take(app, marker) for marker in (
            "        [[nodiscard]] bool open_project_manifest_path(",
            "        [[nodiscard]] bool open_project_manifest_from_dialog(", "        void draw_project_browser("))
        file_menu = take(menus, '            menu_item("Open Project...", [&]()').split("{", 1)[1][:-1]
        report = ROOT / "Artifacts/Reports/project-browser-oct31" / ("cpu-" + str(time.time_ns()))
        report.mkdir(parents=True)
        body = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace fs=std::filesystem;
fs::path fixtureRoot, privateData;
namespace epochengine {
struct EditorProjectCapabilityPolicy {};
std::optional<EditorProjectCapabilityPolicy> editor_project_capability_policy(std::string_view s) { if(s=="supported")return EditorProjectCapabilityPolicy{};return {}; }
namespace core::path { fs::path candidate_data_root(){return privateData;} fs::path engine_asset_dir(){return fixtureRoot/"Engine/assets";} }
namespace updater::distribution { constexpr std::string_view edition_id="development"; }
namespace project_input { constexpr std::string_view canonical_source_path="Assets/Config/input_profile.epochinput"; }
namespace project_sprite_animation { constexpr std::string_view canonical_source_path="Assets/Animations/sprite_animations.epochanim"; }
namespace project_audio { constexpr std::string_view canonical_source_path="Assets/Audio/project_audio.epochaudio"; }
namespace project_gui { constexpr std::string_view canonical_source_path="Assets/Gui/project_gui.epochgui"; }
namespace previewgrid { void set_camera_mode(void*,int){} }
}
using namespace epochengine;
TYPES
constexpr std::string_view kProjectManifestFormat="epoch-project-v1", kStaticRuntimeBuildProfile="epoch-runtime-static";
fs::path resolve_epoch_repo_root(const fs::path&){return fixtureRoot;}
std::string read_env_var(std::string_view){return {};}
std::string default_world_name(EditorProjectKind){return "Main";}
REGISTRY
namespace epochengine::core { struct Context{}; }
enum class EditorApplicationKind { Editor, GuiEditor };
enum class TopMenu { None, File };
enum class EditorMainSurface { Project };
struct EditorState {
 std::string projectId,projectRoot,projectPickerId,projectManifestPathDraft,projectStatus;
 bool projectPathAdvancedOpen{},projectBuildPending{},scriptBuildPending{},planetaryAttachmentPending{},planetaryGeneratePending{},planetaryStreamPending{};
 EditorApplicationKind applicationKind{};TopMenu openMenu{};int viewportCameraMode{};
};
int openCount{},closeCount{},logCount{},surfaceCount{};
std::string openedId;
bool sessionAccepted=true;
bool has_open_project(const EditorState& e){return !e.projectId.empty();}
bool editor_open_project(const std::shared_ptr<core::Context>&,std::string_view id){++openCount;openedId=id;return sessionAccepted;}
bool editor_close_project(const std::shared_ptr<core::Context>&){++closeCount;return true;}
void push_editor_log(EditorState&,const std::string&){++logCount;}
void apply_application_profile(EditorState&,int){} int gui_editor_application(){return 0;}
fs::path resolve_editor_path(const std::string& p){return fs::path{p};}
void open_editor_surface(EditorMainSurface,std::string_view){++surfaceCount;}
namespace platform::filesystem {
struct Dialog {bool accepted{},cancelled{};fs::path path{};std::string diagnostic{};};
Dialog nextDialog;int dialogCount{};fs::path initial;
Dialog open_file_dialog(const wchar_t*,const wchar_t*,const wchar_t*,const fs::path& p){++dialogCount;initial=p;return nextDialog;}
}
namespace gui {
struct Vec2{float x{},y{};};
GUI_TYPES
std::string click;std::optional<std::size_t> choose;int edits{};
std::vector<std::string> texts,options,buttons;
void begin(std::string label={},std::optional<std::size_t> selection={}){click=label;choose=selection;edits=0;texts.clear();options.clear();buttons.clear();}
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> specs,float,float,const InlineButtonRowOptions& row){
 assert(row.sizing==InlineButtonSizing::FitContent&&row.wrap);
 for(std::size_t i=0;i<specs.size();++i){buttons.emplace_back(specs[i].label);if(specs[i].enabled&&click==specs[i].label){click.clear();return i;}}return {};
}
void label(std::string_view s){texts.emplace_back(s);}void wrapped_label(std::string_view s,float){texts.emplace_back(s);}
SelectBoxResult select_box(const SelectBoxOptions& o){for(auto x:o.options)options.emplace_back(x);return {.changed=choose.has_value(),.selected_index=choose};}
bool button(std::string_view s,Vec2){buttons.emplace_back(s);if(click==s){click.clear();return true;}return false;}
bool edit_box(std::string&,Vec2,std::size_t,bool){++edits;return false;}
}
APP
void file_open(EditorState& editor,const std::shared_ptr<core::Context>& ctx){FILE_MENU}
std::string read(const fs::path& p){return read_text_file(p);}
void write(const fs::path& p,std::string_view s){fs::create_directories(p.parent_path());std::ofstream(p)<<s;}
std::string manifest(std::string_view id,std::string_view name){return "{\"id\":\""+std::string{id}+"\",\"display_name\":\""+std::string{name}+"\",\"kind\":\"game\",\"scene\":\"worlds/main.epoch\"}";}
bool contains(const std::vector<std::string>& a,std::string_view v){return std::find(a.begin(),a.end(),v)!=a.end();}
int main(int argc,char**argv){
 assert(argc==2);fixtureRoot=fs::absolute(argv[1]);fs::create_directories(fixtureRoot);
 const auto one=fixtureRoot/"Projects/GameProject01/project.epoch.json",two=fixtureRoot/"Projects/GameProject02/project.epoch.json";
 const auto oneText=manifest("game01","First Game"),twoText=manifest("game02","Second Game");
 write(one,oneText);write(two,twoText);write(fixtureRoot/"Projects/Templates/Hidden/project.epoch.json",manifest("hidden","Hidden"));
 write(fixtureRoot/"Projects/Broken/project.epoch.json","{broken");write(fixtureRoot/"Projects/Launcher/project.epoch.json",manifest("plantlab","Plant Lab"));
 assert(editor_projects_root()==fixtureRoot/"Projects");
 EditorState e;auto ctx=std::make_shared<core::Context>();
 gui::begin();draw_project_browser(e,ctx,300);assert(openCount==0&&gui::edits==0&&gui::options.size()==2);
 assert(contains(gui::options,"First Game")&&contains(gui::options,"Second Game"));
 const auto second=std::find(gui::options.begin(),gui::options.end(),"Second Game")-gui::options.begin();
 gui::begin({},second);draw_project_browser(e,ctx,300);assert(e.projectPickerId=="game02"&&openCount==0);
 assert(contains(gui::texts,two.parent_path().generic_string()));
 gui::begin("Open Selected");draw_project_browser(e,ctx,300);assert(openCount==1&&openedId=="game02");assert(read(one)==oneText&&read(two)==twoText);
 const auto third=fixtureRoot/"Projects/Third/project.epoch.json";write(third,manifest("third","Third Game"));
 gui::begin();draw_project_browser(e,ctx,300);assert(gui::options.size()==2);
 gui::begin("Refresh");draw_project_browser(e,ctx,300);assert(openCount==1);
 gui::begin();draw_project_browser(e,ctx,300);assert(gui::options.size()==3&&contains(gui::options,"Third Game"));
 e.projectPickerId="removed";gui::begin("Open Selected");draw_project_browser(e,ctx,300);assert(openCount==1);
 platform::filesystem::nextDialog={.cancelled=true,.diagnostic="Cancelled"};gui::begin("Open Project...");draw_project_browser(e,ctx,300);
 assert(platform::filesystem::dialogCount==1&&openCount==1&&platform::filesystem::initial==fixtureRoot/"Projects");
 e.openMenu=TopMenu::File;file_open(e,ctx);assert(e.openMenu==TopMenu::None&&surfaceCount==1&&platform::filesystem::dialogCount==2&&openCount==1);
 platform::filesystem::nextDialog={.accepted=true,.path=one};file_open(e,ctx);assert(openCount==2&&openedId=="game01");
 for(int guard=0;guard<5;++guard){bool* flags[]={&e.projectBuildPending,&e.scriptBuildPending,&e.planetaryAttachmentPending,&e.planetaryGeneratePending,&e.planetaryStreamPending};*flags[guard]=true;
  assert(!open_project_manifest_path(e,ctx,two)&&openCount==2);*flags[guard]=false;}
 const auto bad=fixtureRoot/"Projects/Bad/project.epoch.json";write(bad,"{broken");assert(!open_project_manifest_path(e,ctx,bad)&&openCount==2);
 const auto collision=fixtureRoot/"Elsewhere/project.epoch.json";write(collision,manifest("game01","Collision"));assert(!open_project_manifest_path(e,ctx,collision)&&openCount==2);
 gui::begin("Advanced: Open by Path...");draw_project_browser(e,ctx,300);assert(e.projectPathAdvancedOpen&&gui::edits==1);
 e.projectManifestPathDraft=two.generic_string();gui::begin("Open Path");draw_project_browser(e,ctx,300);assert(openCount==3&&openedId=="game02");
 gui::begin("Hide Advanced Path");draw_project_browser(e,ctx,300);assert(!e.projectPathAdvancedOpen&&gui::edits==0);
 e.projectRoot=one.parent_path().generic_string();platform::filesystem::nextDialog={.cancelled=true};file_open(e,ctx);assert(platform::filesystem::initial==one.parent_path().parent_path());
 privateData=fixtureRoot/"private-data";fs::create_directories(privateData);write(privateData/"Projects/Private/project.epoch.json",manifest("private","Private Game"));
 assert(editor_projects_root()==privateData/"Projects");editor_refresh_project_profiles();e.projectRoot.clear();e.projectPickerId.clear();
 gui::begin();draw_project_browser(e,ctx,300);assert(gui::options.size()==1&&gui::options.front()=="Private Game");
 assert(!open_project_manifest_path(e,ctx,one)&&openCount==3);
 platform::filesystem::nextDialog={.cancelled=true};file_open(e,ctx);assert(platform::filesystem::initial==privateData/"Projects");
 assert(read(one)==oneText&&read(two)==twoText);
 std::ofstream(fixtureRoot/"result.json")<<R"({"discovery_two_saved_projects":true,"selection_never_opens":true,"explicit_refresh_discovers_new":true,"file_menu_uses_same_picker":true,"cancel_preserves_session":true,"busy_and_bad_manifest_refused":true,"manual_path_optional":true,"candidate_root_isolated":true,"project_bytes_unchanged":true})";
}
'''
        body = body.replace("GUI_TYPES", gui_types).replace("TYPES", types).replace("REGISTRY", registry_code).replace("APP", app_code).replace("FILE_MENU", file_menu)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        records = compile_and_run(self, source, report, args=lambda config: [report / "data" / config])
        (report / "records.json").write_text(json.dumps(records, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
