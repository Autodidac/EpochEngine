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
from Tests.support.cpp import compile_and_run, dependency_include, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class ProjectBrowser(unittest.TestCase):
    def test_open_defaults_only_a_new_authored_canvas_project_to_tile_map(self):
        application = ROOT / "Engine/src/editor/application"
        lifecycle = (application / "editor.application_context_lifecycle.inl").read_text(encoding="utf-8")
        owner = definition(lifecycle, "    bool editor_open_project(")
        report = ROOT / "Artifacts/Reports/project-browser-oct31" / ("open-canvas-" + str(time.time_ns()))
        report.mkdir(parents=True)
        body = r'''
#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace epochengine {
namespace core { struct Context {}; }
namespace previewgrid {
enum class CameraMode { Fps, Canvas2D };
int setCount{}, resetCount{};
void set_camera_mode(core::Context*, CameraMode) { ++setCount; }
void reset_camera(core::Context*) { ++resetCount; }
}
}
using namespace epochengine;
using previewgrid::CameraMode;
enum class EditorProjectKind { Game, Tool };
enum class EditorApplicationKind { Standard, GuiEditor };
enum class EditorMainSurface { Scene, Game2D, Assets };

struct Profile {
    std::string_view id;
    EditorProjectKind kind;
    std::string_view template_family, tilemap_path;
    std::optional<CameraMode> savedCamera;
    bool selected{true};
};
constexpr auto map = "Assets/Maps/main.epochmap";
constexpr auto game = EditorProjectKind::Game;
constexpr Profile profiles[]{
    {"canvas", game, "game-2d-project", map, CameraMode::Canvas2D},
    {"automatic", game, "game-2d-project", map, std::nullopt},
    {"fps", game, "game-2d-project", map, CameraMode::Fps},
    {"3d", game, "game-project", map, CameraMode::Canvas2D},
    {"missing-map", game, "game-2d-project", "", CameraMode::Canvas2D},
    {"calculator", EditorProjectKind::Tool, "game-2d-project", map, CameraMode::Canvas2D},
    {"failed", game, "game-2d-project", map, CameraMode::Canvas2D, false},
};
const Profile* editor_find_project_profile(std::string_view id) {
    const auto found = std::ranges::find(profiles, id, &Profile::id);
    return found == std::end(profiles) ? nullptr : found;
}
struct Lifecycle {
    struct Evidence { bool project_selected{}; } receipt;
    const Evidence& evidence() const { return receipt; }
};
struct EditorState {
    std::string projectId{"3d"}, projectStatus;
    EditorApplicationKind applicationKind{EditorApplicationKind::Standard};
    Lifecycle projectLifecycle;
    std::optional<CameraMode> projectCameraMode;
    std::optional<EditorMainSurface> requestedMainSurface;
    CameraMode viewportCameraMode{CameraMode::Fps};
    bool game2DGuiWorkspace{}, planetaryBusy{};
    int projectSelections{};
} state;

EditorState& editor_state_for(const std::shared_ptr<core::Context>&) { return state; }
bool planetary_project_busy(const EditorState& editor) { return editor.planetaryBusy; }
bool editor_application_owns_dedicated_gui_workspace(EditorApplicationKind application) {
    return application == EditorApplicationKind::GuiEditor;
}
void set_project(EditorState& editor, std::string_view id, bool writeLog) {
    ++editor.projectSelections;
    const auto& profile = *editor_find_project_profile(id);
    editor.projectId = id;
    editor.applicationKind = EditorApplicationKind::Standard;
    editor.projectLifecycle.receipt.project_selected = profile.selected;
    editor.projectCameraMode = profile.savedCamera;
    editor.requestedMainSurface.reset();
    editor.game2DGuiWorkspace = profile.kind == EditorProjectKind::Tool;
    if (editor.game2DGuiWorkspace)
        editor.requestedMainSurface = EditorMainSurface::Game2D;
}

#define EPOCH_ENABLE_AUTHORING_PLATFORM 1
#define EPOCH_ENABLE_TILEMAP_EDITOR 1
OPEN_OWNER
#undef EPOCH_ENABLE_TILEMAP_EDITOR
#define EPOCH_ENABLE_TILEMAP_EDITOR 0
OPEN_OWNER_WITHOUT_TILEMAP

int main() {
    auto context = std::make_shared<core::Context>();
    assert(editor_open_project(context, "canvas"));
    assert(state.requestedMainSurface == EditorMainSurface::Game2D);
    assert(!state.game2DGuiWorkspace);
    assert(previewgrid::setCount == 1 && previewgrid::resetCount == 1);

    // A consumed one-time workspace request is not queued again on same-ID Open.
    state.requestedMainSurface.reset();
    assert(editor_open_project(context, "canvas"));
    assert(!state.requestedMainSurface);

    for (auto id : {"automatic", "fps", "missing-map", "3d"}) {
        assert(editor_open_project(context, id));
        assert(!state.requestedMainSurface);
    }
    assert(editor_open_project(context, "calculator"));
    assert(state.requestedMainSurface == EditorMainSurface::Game2D);
    assert(state.game2DGuiWorkspace);

    state.applicationKind = EditorApplicationKind::GuiEditor;
    assert(editor_open_project(context, "canvas"));
    assert(!state.requestedMainSurface);
    assert(!editor_open_project(context, "failed"));
    assert(!state.requestedMainSurface);

    state.planetaryBusy = true;
    const int prior = state.projectSelections;
    assert(!editor_open_project(context, "canvas"));
    assert(!editor_open_project(context, "absent"));
    assert(!editor_open_project(context, ""));
    assert(!editor_open_project({}, "canvas"));
    assert(state.projectSelections == prior);
    state.planetaryBusy = false;
    assert(editor_open_project_without_tilemap(context, "canvas"));
    assert(!state.requestedMainSurface);
}
'''
        gated_owner = owner.replace("editor_open_project(", "editor_open_project_without_tilemap(", 1)
        body = body.replace("OPEN_OWNER_WITHOUT_TILEMAP", gated_owner).replace("OPEN_OWNER", owner)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        records = compile_and_run(self, source, report)
        (report / "records.json").write_text(json.dumps(records, indent=2), encoding="utf-8")

    def test_actual_discovery_picker_selection_refresh_and_admission(self):
        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        shell = (ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl").read_text(encoding="utf-8")
        filesystem = (ROOT / "Engine/modules/platform/platform.filesystem.ixx").read_text(encoding="utf-8")
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        core = (ROOT / "Engine/modules/editor/editor.core.ixx").read_text(encoding="utf-8")
        app = (ROOT / "Engine/src/editor/application/editor.application_scripting_renderers.inl").read_text(encoding="utf-8")
        assets = (ROOT / "Engine/src/editor/application/editor.application_asset_browsing.inl").read_text(encoding="utf-8")
        runtime = (ROOT / "Engine/src/editor/application/editor.application_runtime_source.inl").read_text(encoding="utf-8")
        outliner = (ROOT / "Engine/src/editor/application/editor.frame_outliner.inl").read_text(encoding="utf-8")
        menus = (ROOT / "Engine/src/editor/application/editor.frame_command_menus.inl").read_text(encoding="utf-8")
        gui = (ROOT / "Engine/modules/gui/gui.engine.ixx").read_text(encoding="utf-8")
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
            ("    [[nodiscard]] static std::optional<nlohmann::ordered_json>\n        parse_project_build_configuration_manifest(", False),
            ("    [[nodiscard]] static std::optional<EditorProjectKind> parse_project_manifest_kind(", False),
            ("    [[nodiscard]] static std::string parse_world_name(", False),
            ("    [[nodiscard]] static bool is_builtin_project_id(", False),
            ("    [[nodiscard]] static bool is_launcher_application_id(", False),
            ("    [[nodiscard]] static std::optional<OwnedProjectProfile>\n        parse_manifest_project_profile(", False),
            ("    static void rebuild_project_profile_cache()", False),
            ("    [[nodiscard]] static const std::vector<EditorProjectProfile>& live_project_profiles()", False),
            ("    [[nodiscard]] static std::string next_generated_project_name(", False),
        ))
        registry_code += "\n".join(take(scene, marker) for marker in (
            "    EditorProjectAdmissionResult editor_admit_project_manifest(",
            "    void editor_refresh_project_profiles()", "    std::filesystem::path editor_projects_root()",
            "    const EditorProjectProfile* editor_find_project_profile(",
            "    std::span<const EditorProjectProfile> editor_project_profiles()"))
        registry_code += "\n" + take(shell, "    [[nodiscard]] static const EditorProjectProfile* find_project_profile_by_root(")
        registry_code += "\n".join(take(scene, marker) for marker in (
            "    [[nodiscard]] static std::optional<std::string> read_project_build_configuration_manifest(",
            "    bool editor_record_published_tilemap("))
        native_publication = "\n".join(take(filesystem, marker) for marker in (
            "    [[nodiscard]] inline bool exclusive_create_and_write(",
            "    [[nodiscard]] inline bool atomic_publish_new_same_filesystem(",
            "    [[nodiscard]] inline bool atomic_replace_same_filesystem("))
        shell_claim = "\n".join(take(shell, marker) for marker in (
            "    [[nodiscard]] static bool claim_new_project_root(",
            "    [[nodiscard]] static bool publish_new_project_manifest("))
        writer = take(shell, "    [[nodiscard]] static EditorProjectCreationResult write_project_shell(")
        self.assertLess(writer.index("claim_new_project_root(root)"), writer.index("fs::create_directories(worlds"))
        self.assertLess(writer.index("ok = ensure_project_gui_assets("), writer.index("publish_new_project_manifest(manifest,"))
        gui_types = "\n".join(take(gui, "    export " + marker, True) for marker in (
            "enum class ButtonIcon", "struct InlineButtonSpec", "enum class InlineButtonSizing",
            "enum class InlineButtonAlignment", "struct InlineButtonRowOptions", "struct SelectBoxOptions", "struct SelectBoxResult"))
        app_code = "\n".join(take(app, marker) for marker in (
            "        [[nodiscard]] bool open_project_manifest_path(",
            "        [[nodiscard]] bool open_project_manifest_from_dialog(", "        void draw_project_browser("))
        app_code += "\n" + take(assets,
            "        editor_tilemaps::TileMapWorkspaceController*\n            tilemap_workspace_controller(")
        app_code += "\n" + take(runtime, "        [[nodiscard]] bool publish_editor_tilemap(")
        self.assertIn("publish_editor_tilemap(state, *tileMap)", take(runtime, "        [[nodiscard]] bool save_editor_scene_snapshot("))
        self.assertIn("publish_editor_tilemap(editor, *tileMap)", outliner)
        file_menu = take(menus, '            menu_item("Open Project...", [&]()').split("{", 1)[1][:-1]
        report = ROOT / "Artifacts/Reports/project-browser-oct31" / ("cpu-" + str(time.time_ns()))
        report.mkdir(parents=True)
        body = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
#undef assert
#define assert(condition) do { if (!(condition)) { \
 std::fprintf(stderr,"Project browser fixture line %d: %s\n",__LINE__,#condition); std::exit(91); \
} } while(false)
namespace fs=std::filesystem;
fs::path fixtureRoot, privateData;
namespace epochengine {
namespace platform::filesystem { NATIVE_PUBLICATION }
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
SHELL_CLAIM
namespace epochengine::core { struct Context{}; }
namespace epochengine::editor_tilemaps {
struct TileMapWorkspaceController {
 std::string project, logical;
 fs::path root;
 int opens{};
 bool publish{true};
 TileMapWorkspaceController(std::string id,fs::path path,std::string logicalPath)
  : project(std::move(id)), logical(std::move(logicalPath)), root(std::move(path)) {}
 bool valid() const {return !project.empty() && !root.empty();}
 bool open_or_create() {++opens; return true;}
 std::string_view status() const {return "Tile map ready";}
 std::string_view logical_path() const {return logical;}
 bool save_and_publish() const {
  if(!publish)return false;
  const auto target=root/logical;
  fs::create_directories(target.parent_path());
  std::ofstream stream(target);
  stream<<"Published tile-map fixture";
  return static_cast<bool>(stream);
 }
};
}
enum class EditorApplicationKind { Editor, GuiEditor };
enum class TopMenu { None, File };
enum class EditorMainSurface { Project };
struct EditorState {
 std::string projectId,projectRoot,projectPickerId,projectManifestPathDraft,projectStatus;
 std::string tileMapStatus;
 std::unique_ptr<editor_tilemaps::TileMapWorkspaceController> tileMapWorkspace;
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
fs::path resolve_editor_path(const fs::path& p){return p;}
void open_editor_surface(EditorMainSurface,std::string_view){++surfaceCount;}
namespace epochengine::platform::filesystem {
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
 const auto external=fixtureRoot/"Elsewhere/External/project.epoch.json";
 write(external,manifest("gameproject03","External Game"));
 assert(editor_admit_project_manifest(external.generic_string()).succeeded);
 assert(next_generated_project_name(EditorProjectKind::Game,fixtureRoot/"Projects")=="GameProject04");
 privateData=fixtureRoot/"private-data";fs::create_directories(privateData);write(privateData/"Projects/Private/project.epoch.json",manifest("private","Private Game"));
 assert(editor_projects_root()==privateData/"Projects");editor_refresh_project_profiles();e.projectRoot.clear();e.projectPickerId.clear();
 gui::begin();draw_project_browser(e,ctx,300);assert(gui::options.size()==1&&gui::options.front()=="Private Game");
 assert(!open_project_manifest_path(e,ctx,one)&&openCount==3);
 platform::filesystem::nextDialog={.cancelled=true};file_open(e,ctx);assert(platform::filesystem::initial==privateData/"Projects");
 assert(read(one)==oneText&&read(two)==twoText);
 const auto projects=privateData/"Projects";
 fs::create_directories(projects/"gameproject01");
 const auto selected=next_generated_project_name(EditorProjectKind::Game,projects);
 assert(selected=="GameProject02"&&next_generated_project_name(EditorProjectKind::Game,projects)==selected);
 const auto reserved=projects/selected, freshManifest=reserved/"project.epoch.json";
 bool firstClaim{},secondClaim{};
 {
  std::jthread firstCreator([&]{firstClaim=claim_new_project_root(reserved);});
  std::jthread secondCreator([&]{secondClaim=claim_new_project_root(reserved);});
 }
 assert(firstClaim!=secondClaim&&fs::is_directory(reserved));
 assert(next_generated_project_name(EditorProjectKind::Game,projects)=="GameProject03");
 assert(!fs::exists(freshManifest));
 editor_refresh_project_profiles();gui::begin();draw_project_browser(e,ctx,300);
 assert(gui::options.size()==1&&gui::options.front()=="Private Game");
 const auto firstLanding=manifest(selected,"First Landing");
 assert(publish_new_project_manifest(freshManifest,firstLanding));
 assert(!claim_new_project_root(reserved));
 assert(!publish_new_project_manifest(freshManifest,manifest(selected,"Wrong project")));
 assert(read(freshManifest)==firstLanding&&!fs::exists(reserved/"project.epoch.json.pending"));
 editor_refresh_project_profiles();gui::begin();draw_project_browser(e,ctx,300);
 assert(gui::options.size()==2&&contains(gui::options,"First Landing"));
 const auto mapManifest=projects/"Cartography/project.epoch.json";
 auto mapText=manifest("maps", "Cartography");
 mapText.pop_back();
 mapText+=R"(,"tilemap":"Assets/Maps/Chapters/Coast.epochmap"})";
 write(mapManifest,mapText);
 assert(editor_admit_project_manifest(mapManifest.generic_string()).succeeded);
 const auto* mapProfile=editor_find_project_profile("maps");
 assert(mapProfile && mapProfile->tilemap_path=="Assets/Maps/Chapters/Coast.epochmap");
 e.projectId="maps";
 e.projectRoot=mapManifest.parent_path().generic_string();
 auto* workspace=tilemap_workspace_controller(e);
 assert(workspace && workspace->root==mapManifest.parent_path());
 assert(workspace->project==e.projectId && workspace->logical==mapProfile->tilemap_path);
 assert(tilemap_workspace_controller(e)==workspace && workspace->opens==1);
 assert(publish_editor_tilemap(e,*workspace));
 assert(read(mapManifest)==mapText);
 e.tileMapWorkspace.reset();
 e.projectId=selected;
 e.projectRoot=reserved.generic_string();
 auto* unconfiguredWorkspace=tilemap_workspace_controller(e);
 assert(unconfiguredWorkspace && unconfiguredWorkspace->root==reserved);
 assert(unconfiguredWorkspace->logical=="Assets/Maps/main.epochmap" && read(freshManifest)==firstLanding);
 unconfiguredWorkspace->publish=false;
 assert(!publish_editor_tilemap(e,*unconfiguredWorkspace));
 assert(read(freshManifest)==firstLanding&&editor_find_project_profile(selected)->tilemap_path.empty());
 unconfiguredWorkspace->publish=true;
 assert(publish_editor_tilemap(e,*unconfiguredWorkspace));
 const auto afterMapSave=read(freshManifest);
 auto afterMapDocument=nlohmann::ordered_json::parse(afterMapSave);
 assert(afterMapDocument["tilemap"]=="Assets/Maps/main.epochmap");
 assert(editor_find_project_profile(selected)->tilemap_path=="Assets/Maps/main.epochmap");
 afterMapDocument.erase("tilemap");
 assert(afterMapDocument==nlohmann::ordered_json::parse(firstLanding));
 assert(publish_editor_tilemap(e,*unconfiguredWorkspace)&&read(freshManifest)==afterMapSave);
 std::string tileDiagnostic;
 assert(!editor_record_published_tilemap(e.projectRoot,"Assets/Maps/Other.epochmap",tileDiagnostic));
 assert(read(freshManifest)==afterMapSave);
 // An externally admitted project is cached ahead of scanned profiles.
 privateData.clear();editor_refresh_project_profiles();e.tileMapWorkspace.reset();
 const auto externalMap=fixtureRoot/"Elsewhere/ExternalMap/project.epoch.json";
 write(externalMap,manifest("external-map","External Map"));
 assert(editor_admit_project_manifest(externalMap.generic_string()).succeeded);
 assert(editor_find_project_profile("external-map")->tilemap_path.empty());
 e.projectId="external-map";e.projectRoot=externalMap.parent_path().generic_string();
 auto* externalWorkspace=tilemap_workspace_controller(e);
 assert(externalWorkspace&&publish_editor_tilemap(e,*externalWorkspace));
 assert(editor_find_project_profile("external-map")->tilemap_path==externalWorkspace->logical);
 assert(nlohmann::ordered_json::parse(read(externalMap))["tilemap"]==externalWorkspace->logical);
 e.tileMapWorkspace.reset();
 const auto software=fixtureRoot/"Projects/ToolFixture/project.epoch.json";
 auto toolManifest=nlohmann::ordered_json::parse(manifest("tool-fixture","Tool Fixture"));
 toolManifest["kind"]="tool";
 const auto toolText=toolManifest.dump();
 write(software,toolText);editor_refresh_project_profiles();
 e.projectId="tool-fixture";e.projectRoot=software.parent_path().generic_string();
 auto* toolWorkspace=tilemap_workspace_controller(e);
 assert(toolWorkspace&&publish_editor_tilemap(e,*toolWorkspace));
 assert(read(software)==toolText&&editor_find_project_profile("tool-fixture")->tilemap_path.empty());
 std::ofstream(fixtureRoot/"result.json")<<R"({"discovery_two_saved_projects":true,"selection_never_opens":true,"explicit_refresh_discovers_new":true,"file_menu_uses_same_picker":true,"cancel_preserves_session":true,"busy_and_bad_manifest_refused":true,"manual_path_optional":true,"candidate_root_isolated":true,"project_bytes_unchanged":true,"one_new_project_claim":true,"manifest_published_without_replacement":true,"discovery_requires_publication":true,"tilemap_workspace_uses_admitted_profile":true,"unconfigured_workspace_keeps_default_path":true,"successful_tilemap_publication_only":true,"declared_tilemap_preserved":true,"external_registry_updated":true,"software_manifest_unchanged":true})";
}
'''
        body = body.replace("GUI_TYPES", gui_types).replace("TYPES", types).replace("REGISTRY", registry_code).replace("APP", app_code).replace("FILE_MENU", file_menu)
        body = body.replace("NATIVE_PUBLICATION", native_publication).replace("SHELL_CLAIM", shell_claim)
        source = report / "contract.cpp"
        source.write_text(body, encoding="utf-8")
        kits = windows_sdk_includes(Path("C:/Program Files (x86)/Windows Kits/10"))
        includes = [dependency_include(ROOT, "nlohmann/json.hpp")]
        if kits:
            includes.extend(kits[-1] / folder for folder in ("um", "shared"))
        records = compile_and_run(self, source, report, includes=includes, args=lambda config: [report / "data" / config])
        (report / "records.json").write_text(json.dumps(records, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
