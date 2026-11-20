"""Real Tile Map geometry, queued input and object journal with recording paint ports."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.gui.test_gui_manual_press_origin import fixture as manual_fixture
from Tests.support.cpp import engine_module_source

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/editor.tilemap_workspace.cpp"
GUI = ROOT / "Engine/src/epochgui/gui.engine.cpp"
API = ROOT / "Engine/modules/gui/gui.engine.ixx"
BUTTONS = ROOT / "Engine/src/epochgui/controls/gui.buttons_images.inl"
WINDOWS = ROOT / "Engine/src/epochgui/controls/gui.window_controls.inl"
WORKSPACE_API = ROOT / "Engine/modules/editor/editor.tilemap_workspace.ixx"


def fixture():
    owner, gui, api = (p.read_text(encoding="utf-8") for p in (OWNER, GUI, API))
    base = manual_fixture()
    base = base[:base.index("int main(int argc,char** argv)")]
    base = base.replace("import epoch.gui.input;",
                        "import epoch.gui.input;\nimport epoch.gui.tile_workspace;\nimport authoring.tilemap;")
    clip = definition(api, "    export class ContentClipScope", True).replace("export ", "")
    clip += "\n" + definition(gui, "    ContentClipScope::ContentClipScope(")
    clip += "\n" + definition(gui, "    ContentClipScope::~ContentClipScope(")
    base = base.replace("struct ContentClipScope{ContentClipScope(Vec2,Vec2){}};", clip)
    base = base.replace("float base_line_height(float){return 12;}",
                        "float fontHeight=12;float base_line_height(float){return fontHeight;}")
    base = base.replace("void draw_sprite(int,float,float,float,float){}", r'''
struct Paint{int sprite;gui_lib::Rect requested,visible,clip;};std::vector<Paint> paints;
gui_lib::Rect intersection(gui_lib::Rect a,gui_lib::Rect b){
 const float x=std::max(a.position.x,b.position.x),y=std::max(a.position.y,b.position.y);
 const float r=std::min(a.position.x+a.size.x,b.position.x+b.size.x);
 const float d=std::min(a.position.y+a.size.y,b.position.y+b.size.y);
 return {{x,y},{std::max(0.f,r-x),std::max(0.f,d-y)}};
}
void draw_sprite(int sprite,float x,float y,float w,float h){
 const gui_lib::Rect requested{{x,y},{w,h}},clip{to_lib(g_frame.contentMin),
  {g_frame.contentMax.x-g_frame.contentMin.x,g_frame.contentMax.y-g_frame.contentMin.y}};
 paints.push_back({sprite,requested,intersection(requested,clip),clip});
}''')
    base = base.replace("void draw_text_line(std::string_view,float,float,float){}", r'''
std::vector<std::string> captions;
void draw_text_line(std::string_view text,float,float,float){captions.emplace_back(text);}
''')
    panel = definition(WINDOWS.read_text(encoding="utf-8"), "    void panel_rect(")
    base = base.replace("}\nusing namespace gui;", panel + "\n}\nusing namespace gui;", 1)
    helpers = "\n".join(definition(owner, marker) for marker in (
        "        [[nodiscard]] gui::Vec2 gui_point(", "        [[nodiscard]] gui::Vec2 gui_size(",
        "        [[nodiscard]] bool canvas_item(",
        "        [[nodiscard]] constexpr bool cell_action_available("))
    workspace_api = WORKSPACE_API.read_text(encoding="utf-8")
    workspace_types = "\n".join(definition(workspace_api, marker, True) for marker in (
        "    enum class ControllerCode", "    struct ControllerMetrics", "    struct ControllerResult",
        "    struct CanvasRenderResult"))
    workspace_types += definition(workspace_api, "    [[nodiscard]] constexpr std::string_view controller_code_name(")
    object_geometry = "\n".join(definition(owner, marker) for marker in (
        "        [[nodiscard]] gui_lib::Vec2 gui_lib_point(",
        "        [[nodiscard]] bool point_in_rect(",
        "        [[nodiscard]] gui_lib::Rect object_canvas_rect(",
        "        [[nodiscard]] gui_lib::Rect clipped_rect(",
        "        [[nodiscard]] gui_lib::Rect canvas_object_bounds(",
        "        [[nodiscard]] const authoring::tilemap::MapObjectSnapshot* canvas_object_at("))
    object_control = "\n".join(definition(owner, marker) for marker in (
        "    ControllerResult TileMapWorkspaceController::begin_object_drag(",
        "    ControllerResult TileMapWorkspaceController::update_object_drag(",
        "    ControllerResult TileMapWorkspaceController::end_object_drag(",
        "    ControllerResult TileMapWorkspaceController::apply_selected_object(",
        "    void TileMapWorkspaceController::clear_object_selection(",
        "    bool TileMapWorkspaceController::route_canvas_object_drag(",
        "    ControllerResult TileMapWorkspaceController::accept(",
        "    ControllerResult TileMapWorkspaceController::reject("))
    # Execute the actual workspace admission/layout/backing prefix, before its
    # toolbar and document editing. All geometry remains the real portable owner.
    render = definition(owner, "    CanvasRenderResult render_canvas_workspace(")
    prefix = render[render.index("            const auto current ="):render.index("            const gui::Vec2 pointer =")]
    return base + "\nnamespace tilemap {\nusing Tool=gui_lib::tile_workspace::Tool;\n" + helpers + r'''
namespace authoring=epochengine::authoring;
namespace project_tilemap_sources {
enum class SourceCode {ready, invalid_store};
constexpr std::string_view source_code_name(SourceCode) {return "source";}
}
namespace project_tilemaps {
enum class PipelineCode {ready, invalid_pipeline};
constexpr std::string_view pipeline_code_name(PipelineCode) {return "pipeline";}
}
''' + workspace_types + r'''
struct Options{gui::WidgetBounds viewport;bool show_inspector=true;};
struct ControllerPort{
 struct Snapshot{std::array<int,1> palette,layers;};gui_lib::tile_workspace::WorkspaceState state;
 Snapshot snapshot(){return {};}auto& ui_state(){return state;}
 bool object_drag_active()const{return false;}
 void end_object_drag(bool){CHECK(false);}
};
CanvasRenderResult surface(ControllerPort& controller,const Options& options){CanvasRenderResult result;
''' + prefix + r'''
 return result;
}
// Only project construction is a port. These are the exact drag/mutation
// methods over the same document journal used by the regular editor.
struct TileMapWorkspaceController {
    std::unique_ptr<authoring::tilemap::Document> document_;
    ControllerMetrics metrics_;
    std::string status_;
    std::optional<gui_lib::tile_workspace::CellCoordinate> selected_cell_;
    std::optional<authoring::tilemap::MapObjectHandle> selected_object_;
    std::optional<authoring::tilemap::MapObjectDescriptor> selected_object_draft_;
    bool selected_object_draft_dirty_{};
    bool object_drag_active_{};
    authoring::tilemap::Float2 object_drag_offset_{};
    gui::DragSurfaceState object_drag_input_{};

    bool has_document() const {return document_ && document_->valid();}
    auto selected_object() const {return selected_object_;}
    auto selected_object_draft() const {
        return selected_object_draft_ ? std::addressof(*selected_object_draft_) : nullptr;
    }

    ControllerResult accept(authoring::tilemap::MutationResult) noexcept;
    ControllerResult reject(ControllerCode,
        authoring::tilemap::ResultCode = authoring::tilemap::ResultCode::invalid_document,
        project_tilemap_sources::SourceCode = project_tilemap_sources::SourceCode::invalid_store,
        project_tilemaps::PipelineCode = project_tilemaps::PipelineCode::invalid_pipeline) noexcept;
    ControllerResult begin_object_drag(authoring::tilemap::MapObjectHandle, authoring::tilemap::Float2) noexcept;
    ControllerResult update_object_drag(authoring::tilemap::Float2) noexcept;
    ControllerResult end_object_drag(bool) noexcept;
    ControllerResult apply_selected_object() noexcept;
    void clear_object_selection() noexcept;
    bool route_canvas_object_drag(const authoring::tilemap::DocumentSnapshot&,
        const gui_lib::tile_workspace::TileCanvasLayout&, bool, CanvasRenderResult&) noexcept;
};
''' + object_geometry + object_control + r'''
}
bool same(gui_lib::Rect a,gui_lib::Rect b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.size.x==b.size.x&&a.size.y==b.size.y;}
gui_lib::Rect clip_rect(){return {to_lib(g_frame.contentMin),{g_frame.contentMax.x-g_frame.contentMin.x,g_frame.contentMax.y-g_frame.contentMin.y}};}
void object_drag_contract(const std::shared_ptr<core::Context>& context) {
    using namespace tilemap::authoring::tilemap;
    namespace tw = gui_lib::tile_workspace;
    tilemap::TileMapWorkspaceController controller;
    auto created = Document::create({1,1}, {.name="Object gestures", .extent_tiles={16,16},
        .default_chunk_extent={4,4}});
    CHECK(created);
    controller.document_ = std::move(created.document);
    auto& document = *controller.document_;
    const auto source = document.create_object({.name="Player", .type="spawn", .position={2,3}});
    const auto other = document.create_object({.name="Beacon", .position={5,3}});
    CHECK(source && other);
    const auto sourceDescriptor = document.object(source.object);
    const auto neighborDescriptor = document.object(other.object);
    CHECK(sourceDescriptor && neighborDescriptor);
    const auto& original = *sourceDescriptor;
    const auto& neighbor = *neighborDescriptor;
    const auto operations = document.metrics().operation_count;
    const auto canvas = tw::controller().make_canvas({.viewport={{100,100},{460,460}},
        .map_width=16,.map_height=16,.base_cell_pixels=28,.zoom=1});
    CHECK(canvas.valid);
    const auto screen = [&](float x, float y) -> Vec2 {
        return {canvas.origin.x + x * canvas.cell_pixels, canvas.origin.y + y * canvas.cell_pixels};
    };
    const auto at = [&](EventType type, float x, float y) {
        const auto point = screen(x,y);
        return event(type,point.x,point.y);
    };
    const auto route = [&](std::initializer_list<InputEvent> events, bool held = false,
        bool interactive = true, bool clipOrigin = false) {
        tick(context, events, held);
        g_frame.contentMin={100,100};g_frame.contentMax={560,560};
        if (clipOrigin) g_frame.contentMin.x=screen(3,0).x;
        tilemap::CanvasRenderResult result;
        const bool captured = controller.route_canvas_object_drag(document.snapshot(),canvas,interactive,result);
        CHECK(captured==result.input_captured);
        return captured;
    };

    // The release is over a different object and a later move lands elsewhere.
    // Only the object beneath the original press moves, using the release.
    fresh(context);
    CHECK(route({at(EventType::MouseDown,2.25f,3),at(EventType::MouseMove,5,3),
        at(EventType::MouseUp,5.25f,3),at(EventType::MouseMove,6,4)}));
    auto moved = original;
    moved.position = {5,3};
    CHECK(controller.selected_object()==source.object && !controller.object_drag_active_);
    CHECK(document.object(source.object)==moved && document.object(other.object)==neighbor);
    CHECK(document.metrics().operation_count==operations+1 && g_frame.mouseReleaseConsumed);
    CHECK(document.undo() && document.object(source.object)==original);
    CHECK(document.redo() && document.object(source.object)==moved);
    const auto reopened = Document::deserialize(document.serialize().bytes);
    CHECK(reopened && reopened.document->object(source.object)==moved);
    CHECK(document.undo());
    controller.clear_object_selection();

    fresh(context);
    CHECK(route({at(EventType::MouseDown,2,3)},true));
    CHECK(route({at(EventType::MouseMove,3,3)},true) && controller.selected_object_draft_dirty_);
    CHECK(document.object(source.object)==original);
    CHECK(route({at(EventType::MouseUp,4,5)}));
    moved.position = {4,5};
    CHECK(document.object(source.object)==moved && !controller.object_drag_active_);
    CHECK(document.undo() && document.object(source.object)==original);
    controller.clear_object_selection();
    const auto cancellationRevision = document.revision();

    fresh(context);
    CHECK(route({at(EventType::MouseDown,2,3),at(EventType::MouseMove,3,3)},true));
    CHECK(route({{.type=EventType::FocusLost},at(EventType::MouseUp,4,5)}));
    CHECK(!controller.object_drag_active_ && !controller.selected_object_draft_dirty_);
    CHECK(document.revision()==cancellationRevision && document.object(source.object)==original);

    fresh(context);
    CHECK(route({at(EventType::MouseDown,2,3),at(EventType::MouseMove,3,3)},true));
    CHECK(!route({at(EventType::MouseUp,4,5)},false,false));
    CHECK(!controller.object_drag_active_ && document.revision()==cancellationRevision);

    fresh(context);
    CHECK(!route({at(EventType::MouseDown,2,3),at(EventType::MouseUp,5,3)},false,true,true));
    CHECK(!route({at(EventType::MouseDown,8,8),at(EventType::MouseUp,5,3)}));
    CHECK(route({at(EventType::MouseDown,2,3),at(EventType::MouseUp,2,3)}));
    CHECK(controller.selected_object()==source.object && !controller.object_drag_active_);
    CHECK(document.revision()==cancellationRevision && document.object(other.object)==neighbor);
}
int main(){
 auto ctx=std::make_shared<core::Context>();fresh(ctx);
 object_drag_contract(ctx);fresh(ctx);
 const auto frame=[&](std::initializer_list<InputEvent> events={}){
  tick(ctx,events);g_frame.contentMin={0,0};g_frame.contentMax={1000,800};paints.clear();captions.clear();};
 namespace tw=gui_lib::tile_workspace;
 const auto& workspace=tw::controller();
 const tw::WorkspaceLimits limits{.minimum_zoom=.75f,.maximum_zoom=8};
 const gui_lib::Rect viewport{{100,100},{301,181}};
 for(float zoom:{.75f,1.f,8.f})for(float textHeight:{12.f,26.f,42.f}){
  fontHeight=textHeight;
  const auto canvas=workspace.make_canvas({.viewport=viewport,.map_width=64,.map_height=64,
   .base_cell_pixels=28,.zoom=zoom,.pan={-7,-9},.overscroll_cells=1,.limits=limits});
  CHECK(canvas.valid&&canvas.zoom==zoom);
  const tw::CellCoordinate coordinate{canvas.visible_cells.first.x+1,canvas.visible_cells.first.y+1};
  const auto bounds=workspace.cell_rect(canvas,coordinate,1);
  CHECK(bounds.size.x==28*zoom-2&&bounds.size.y==28*zoom-2);
  frame();{
   ContentClipScope clip{from_lib(viewport.position),{401,281}};
   CHECK(!tilemap::canvas_item("tilemap.cell",bounds,"BOX",true,true));
  }
  CHECK(!paints.empty()&&same(paints[0].requested,bounds));
  const auto expected=intersection(bounds,viewport);
  for(const auto& p:paints)CHECK(same(p.visible,intersection(p.requested,expected)));
  CHECK(!captions.empty()&&captions[0]=="BOX");
  CHECK(g_frame.contentMin.x==0&&g_frame.contentMax.x==1000);
  // The old text button really expands at these font/zoom combinations.
  frame();g_frame.cursor=from_lib(bounds.position);
  (void)button_with_state("BOX",from_lib(bounds.size),true);
  const bool oldInflated=g_frame.lastButtonBounds->size.y>bounds.size.y;
  CHECK(oldInflated==(textHeight+10>bounds.size.y));
  const auto neighbor=workspace.cell_rect(canvas,{coordinate.x+1,coordinate.y},1);
  const Vec2 target{bounds.position.x+bounds.size.x/2,bounds.position.y+bounds.size.y/2};
  frame({event(EventType::MouseDown,target.x,target.y),event(EventType::MouseUp,target.x,target.y)});
  CHECK(!tilemap::canvas_item("tilemap.cell",neighbor,{},false,true));
  CHECK(tilemap::canvas_item("tilemap.cell",bounds,{},false,true));
  CHECK(g_frame.mousePressConsumed&&g_frame.mouseReleaseConsumed);
  // Grid gaps cannot be claimed by either adjacent area.
  const Vec2 gap{bounds.position.x+bounds.size.x+.5f,target.y};
  frame({event(EventType::MouseDown,gap.x,gap.y),event(EventType::MouseUp,gap.x,gap.y)});
  CHECK(!tilemap::canvas_item("tilemap.cell",bounds,{},false,true));
  CHECK(!tilemap::canvas_item("tilemap.cell",neighbor,{},false,true));
  CHECK(!g_frame.mousePressConsumed);
 }
 fontHeight=26;
 // The visible edge has one clip for drawing and queued-press admission.
 const gui_lib::Rect edge{{90,110},{20,20}};
 frame({event(EventType::MouseDown,95,115),event(EventType::MouseUp,95,115)});
 {ContentClipScope clip{{100,100},{401,281}};
  CHECK(!tilemap::canvas_item("tilemap.cell",edge,"BOX",true,true));
  CHECK(same(paints[0].visible,gui_lib::Rect{{100,110},{10,20}}));}
 CHECK(!g_frame.mousePressConsumed);
 frame({event(EventType::MouseDown,105,115),event(EventType::MouseUp,105,115)});
 {ContentClipScope clip{{100,100},{401,281}};CHECK(tilemap::canvas_item("tilemap.cell",edge,{},false,true));}
 // Noninteractive cells leave the real pointer owner untouched.
 frame({event(EventType::MouseDown,105,115),event(EventType::MouseUp,105,115)});
 CHECK(!tilemap::canvas_item("tilemap.cell",edge,{},false,
  tilemap::cell_action_available(gui_lib::tile_workspace::Tool::collision,false,true))&&!g_frame.mousePressConsumed);
 CHECK(tilemap::canvas_item("tilemap.object",edge,"Spawn",true,true));
 // Even a subpixel clipped item cannot draw or hit outside its supplied area.
 frame();const gui_lib::Rect tiny{{100,100},{.5f,.5f}};
 CHECK(!tilemap::canvas_item("tiny",tiny,"BOX",true,true));
 for(const auto& p:paints)CHECK(same(p.visible,intersection(p.requested,tiny)));
 const auto painted=paints.size();
 CHECK(!tilemap::canvas_item("invalid",{{0,0},{NAN,1}},{},false,true));
 CHECK(!tilemap::canvas_item("invalid",{{INFINITY,0},{1,1}},{},false,true));
 CHECK(!tilemap::canvas_item("invalid",{{0,0},{0,1}},{},false,true));
 CHECK(paints.size()==painted);
 // Palette cells share the same exact-size control, including clipped scroll.
 const auto palette=workspace.make_palette_grid({.viewport={{20,30},{150,101}},.item_count=80,
  .preferred_cell_size=32,.minimum_cell_size=32,.maximum_cell_size=72,.gap=4,.padding=4,.scroll_y=19});
 CHECK(palette.valid);
 for(auto i=palette.first_visible_row*palette.columns;i<std::min(palette.item_count,palette.past_last_visible_row*palette.columns);++i){
  const auto cell=workspace.make_palette_cell(palette,i,{},false);if(!cell.visible)continue;
  frame();ContentClipScope clip{{20,30},{170,131}};
  CHECK(!tilemap::canvas_item("tilemap.palette",cell.content,"12",false,true));
  CHECK(same(paints[0].requested,cell.content));
 }
 // The real accepted workspace paints its whole owned surface, including gaps,
 // while an inherited narrower window clip remains authoritative.
 for(float width:{320.f,460.f,900.f}){
  frame();g_frame.contentMin={80,70};g_frame.contentMax={900,700};
  tilemap::ControllerPort controller;
  const gui::WidgetBounds area{.position={50,50},.size={width,500}};
  const auto result=tilemap::surface(controller,{area,true});
  CHECK(result.visible&&paints.size()==1&&same(paints[0].requested,{to_lib(area.position),to_lib(area.size)}));
  CHECK(same(paints[0].visible,intersection(paints[0].requested,{{80,70},{820,630}})));
  CHECK(g_frame.contentMin.x==80&&g_frame.contentMin.y==70&&g_frame.contentMax.x==900&&g_frame.contentMax.y==700);
 }
 frame();tilemap::ControllerPort narrow;
 CHECK(!tilemap::surface(narrow,{{.position={50,50},.size={220,500}},true}).visible&&paints.empty());
}
'''


class TileMapCanvasRectangles(unittest.TestCase):
    def test_actual_area_controls_geometry_and_input_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        owner = OWNER.read_text(encoding="utf-8")
        render = definition(owner, "    CanvasRenderResult render_canvas_workspace(")
        for identity in ("tilemap.palette", "tilemap.cell", "tilemap.object"):
            self.assertIn('"' + identity + '"', render)
        self.assertIn("controller.route_canvas_object_drag(", render)
        generated = fixture()
        identity = hashlib.sha256(generated.encode() + Path(__file__).read_bytes()).hexdigest()[:16]
        proof = ROOT / "Artifacts/Reports/tilemap-canvas-object-drag-20261116" / identity
        build = ROOT / "Artifacts/Intermediate/tests/tilemap-canvas-object-drag" / identity
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.tilemap_canvas_contract.cpp"
        source.write_text(generated, encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [engine_module_source(ROOT / "Engine/modules", name) for name in (
            "authoring.document.ixx", "authoring.tilemap.ixx", "asset.tilemap_artifact.ixx")]
        implementations = [ROOT / path for path in (
            "Engine/src/authoring/tilemap/authoring.tilemap_document.cpp",
            "Engine/src/assets/tilemap/asset.tilemap_artifact.cpp")]
        modules += [portable / "modules" / name for name in (
            "epoch.gui.ixx", "epoch.gui.input.ixx", "epoch.gui.tile_workspace.ixx")]
        implementations += [portable / "src/epochgui" / name for name in (
            "input.cpp", "tile_workspace.cpp")]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(TileMapCanvasRectangles LANGUAGES CXX)
add_executable(tilemap_canvas "{source.as_posix()}" {" ".join('"'+p.as_posix()+'"' for p in implementations)})
target_compile_features(tilemap_canvas PRIVATE cxx_std_23)
target_compile_options(tilemap_canvas PRIVATE /W3 /WX /utf-8)
target_sources(tilemap_canvas PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_tilemap_canvas COMMAND tilemap_canvas)
set_tests_properties(actual_tilemap_canvas PROPERTIES TIMEOUT 10)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        temporary = proof / "tmp"
        temporary.mkdir(exist_ok=True)
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary))
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-8000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": results,
            "sourceIdentity": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in (OWNER, WORKSPACE_API, GUI, API, BUTTONS, WINDOWS, *modules, *implementations)},
            "scope": "Actual Tile Map area, queued object drag and commit owners with the real input queue, tile map document/history and serialization. Recording paint and project-construction ports.",
            "cases": ["min/default/max zoom and varied font metrics", "old actual button inflates small cells",
                      "exact cell draw bounds", "queued adjacent cell/gap clicks", "viewport clip draw/hit",
                      "noninteractive cell preserves object click", "subpixel/nonfinite bounds", "scrolled palette",
                      "opaque owned workspace gaps and inherited clip restoration",
                      "queued originating object and exact release", "held preview and one semantic commit",
                      "Undo/Redo and serialized reopen", "focus and noninteractive cancel",
                      "blocked origins and stationary selection"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
