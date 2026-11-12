"""Actual collision controller methods over real tile document/compiler/history."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/editor.tilemap_workspace.cpp"
API = ROOT / "Engine/modules/editor/editor.tilemap_workspace.ixx"


def fixture():
    owner, api = (p.read_text(encoding="utf-8") for p in (OWNER, API))
    types = "\n".join(definition(api, marker, True) for marker in (
        "    enum class ControllerCode", "    struct ControllerResult"))
    types += definition(api, "    [[nodiscard]] constexpr std::string_view controller_code_name(")
    helpers = "\n".join(definition(owner, marker) for marker in (
        "        [[nodiscard]] constexpr bool valid_coordinate(",
        "        [[nodiscard]] constexpr authoring::tilemap::UInt2 tile_coordinate(",
        "        [[nodiscard]] constexpr bool cell_action_available(",
        "        [[nodiscard]] constexpr std::string_view collision_guidance("))
    methods = []
    for name, result in (
        ("apply_cell", "ControllerResult"), ("select_palette", "ControllerResult"),
        ("select_tool", "ControllerResult"), ("accept", "ControllerResult"), ("reject", "ControllerResult"),
        ("stage_selected_palette_collision", "ControllerResult"),
        ("apply_selected_palette_collision", "ControllerResult"),
        ("selected_palette", "std::optional<authoring::tilemap::PaletteEntryHandle>"),
        ("selected_layer", "std::optional<authoring::tilemap::LayerHandle>")):
        methods.append(result + " " + definition(owner, "TileMapWorkspaceController::" + name + "("))
    return r'''
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
import authoring.tilemap;
import epoch.gui.tile_workspace;
namespace authoring=epochengine::authoring;
namespace asset=epochengine::asset;
namespace gui_lib=epochengine::gui_lib;
namespace project_tilemap_sources{enum class SourceCode{ready,invalid_store};constexpr std::string_view source_code_name(SourceCode){return "source";}}
namespace project_tilemaps{enum class PipelineCode{ready,invalid_pipeline};constexpr std::string_view pipeline_code_name(PipelineCode){return "pipeline";}}
void check(bool value,int line){if(!value){std::fprintf(stderr,"Collision contract failed line %d\n",line);std::exit(82);}}
#define CHECK(value) check(static_cast<bool>(value),__LINE__)
namespace control {
using Tool=gui_lib::tile_workspace::Tool;
using CellCoordinate=gui_lib::tile_workspace::CellCoordinate;
''' + types + helpers + r'''
struct TileMapWorkspaceController{
 std::unique_ptr<authoring::tilemap::Document> document_;
 gui_lib::tile_workspace::WorkspaceState ui_state_;
 struct{std::uint64_t commands{},mutations{},painted_cells{},rejected_commands{};}metrics_;
 std::string status_;
 std::optional<authoring::tilemap::LayerHandle> selected_layer_handle_;
 std::optional<authoring::tilemap::PaletteEntryHandle> selected_palette_handle_;
 std::optional<authoring::tilemap::CollisionShape> selected_palette_collision_draft_;
 bool selected_palette_collision_draft_dirty_{};
 std::optional<CellCoordinate> selected_cell_;
 std::optional<authoring::tilemap::MapObjectHandle> selected_object_;
 std::optional<authoring::tilemap::MapObjectDescriptor> selected_object_draft_;
 bool selected_object_draft_dirty_{};
 bool has_document()const{return document_&&document_->valid();}
 auto snapshot()const{return document_->snapshot();}
 // Non-collision branches link through ports and are never exercised here.
 void clear_object_selection(){selected_object_.reset();selected_object_draft_.reset();}
 ControllerResult fill_cell(authoring::tilemap::LayerHandle,authoring::tilemap::UInt2,authoring::tilemap::CellValue){CHECK(false);return {};}
 ControllerResult apply_cell(CellCoordinate)noexcept;
 ControllerResult select_palette(std::uint32_t)noexcept;
 ControllerResult select_tool(Tool)noexcept;
 ControllerResult accept(authoring::tilemap::MutationResult)noexcept;
 ControllerResult reject(ControllerCode,authoring::tilemap::ResultCode=authoring::tilemap::ResultCode::invalid_document,
  project_tilemap_sources::SourceCode=project_tilemap_sources::SourceCode::invalid_store,
  project_tilemaps::PipelineCode=project_tilemaps::PipelineCode::invalid_pipeline)noexcept;
 ControllerResult stage_selected_palette_collision(authoring::tilemap::CollisionShape)noexcept;
 ControllerResult apply_selected_palette_collision()noexcept;
 std::optional<authoring::tilemap::PaletteEntryHandle> selected_palette()const noexcept;
 std::optional<authoring::tilemap::LayerHandle> selected_layer()const noexcept;
};
''' + "\n".join(methods) + r'''
}
int main(){
 using namespace control;using namespace authoring::tilemap;
 auto made=Document::create({1,1},{.name="Collision workflow",.extent_tiles={8,8},.default_chunk_extent={4,4}});
 CHECK(made);TileMapWorkspaceController controller;controller.document_=std::move(made.document);
 auto& doc=*controller.document_;const auto layer=doc.create_layer({.name="Ground",.extent_tiles={8,8},.chunk_extent={4,4},.collision_source=true});
 CHECK(layer);controller.selected_layer_handle_=layer.layer;
 const auto initial=doc.serialize();const auto initialRevision=doc.revision();const auto initialOperations=doc.metrics().operation_count;
 CHECK(controller.select_tool(Tool::collision));
 CHECK(controller.status_==collision_guidance(false,false));
 CHECK(!cell_action_available(Tool::collision,false,true));
 const auto fresh=controller.apply_cell({0,0});CHECK(fresh.code==ControllerCode::palette_required);
 CHECK(doc.revision()==initialRevision&&doc.metrics().operation_count==initialOperations&&doc.serialize().bytes==initial.bytes);
 CHECK(!controller.stage_selected_palette_collision({.kind=asset::tilemap::CollisionKind::full_cell}));
 CHECK(!controller.apply_selected_palette_collision());
 CHECK(doc.serialize().bytes==initial.bytes);
 // Real admitted texture identity; no fake texture-free palette fallback.
 TileSetDescriptor tiles;tiles.name="Terrain";tiles.texture={.logical_path="Assets/terrain.png",.artifact_key={1,2,3,4},
  .project_key=7,.asset_key=8,.artifact_revision=1,.stable_material_key=9,.texture_extent={32,16}};
 tiles.tile_extent={16,16};tiles.grid={2,1};tiles.tile_count=2;
 const auto tileSet=doc.create_tile_set(tiles);CHECK(tileSet);
 const auto a=doc.create_palette_entry({.name="Ground",.tile_set=tileSet.tile_set,.local_tile=0});CHECK(a);
 const auto b=doc.create_palette_entry({.name="Backdrop",.tile_set=tileSet.tile_set,.local_tile=1});CHECK(b);
 CHECK(doc.paint_cells(layer.layer,std::array{CellEdit{{1,1},{.palette=a.palette_entry}},CellEdit{{2,1},{.palette=a.palette_entry}},
  CellEdit{{3,1},{.palette=b.palette_entry}}}));
 CHECK(controller.select_palette(1));const auto before=doc.serialize();const auto revision=doc.revision();const auto operations=doc.metrics().operation_count;
 CHECK(controller.apply_cell({1,1}));
 CHECK((controller.selected_palette()==a.palette_entry&&controller.selected_cell_==std::optional{CellCoordinate{1,1}}));
 CHECK(doc.serialize().bytes==before.bytes&&doc.revision()==revision&&doc.metrics().operation_count==operations);
 CHECK(controller.status_==collision_guidance(true,true));
 const auto empty=controller.apply_cell({0,0});CHECK(empty.code==ControllerCode::tool_not_applicable);
 CHECK(doc.serialize().bytes==before.bytes&&controller.selected_palette()==a.palette_entry);
 CHECK(!cell_action_available(Tool::collision,false,true)&&cell_action_available(Tool::collision,true,true));
 CHECK(!cell_action_available(Tool::collision,true,false)&&cell_action_available(Tool::pencil,false,true));
 CollisionShape solid;solid.kind=asset::tilemap::CollisionKind::full_cell;
 CHECK(controller.stage_selected_palette_collision(solid));
 CHECK(controller.selected_palette_collision_draft_dirty_&&doc.serialize().bytes==before.bytes);
 CHECK(controller.apply_selected_palette_collision());CHECK(!controller.selected_palette_collision_draft_dirty_);
 CHECK(doc.metrics().operation_count==operations+1&&doc.palette_entry(a.palette_entry)->collision==solid);
 CHECK(doc.palette_entry(b.palette_entry)->collision.kind==asset::tilemap::CollisionKind::none);
 auto compiled=doc.compile();CHECK(compiled&&compiled.artifact.collision.size()==2);
 for(const auto& collision:compiled.artifact.collision)CHECK(collision.cell_coordinate.y==1&&collision.cell_coordinate.x>=1&&collision.cell_coordinate.x<=2);
 const auto after=doc.serialize();CHECK(after);
 CHECK(doc.undo());CHECK(doc.compile().artifact.collision.empty());
 CHECK(doc.redo());CHECK(doc.compile().artifact.collision.size()==2);
 auto restored=Document::deserialize(after.bytes);CHECK(restored);
 CHECK(restored.document->compile().artifact.collision==compiled.artifact.collision);
 const auto afterRevision=doc.revision();CHECK(controller.apply_selected_palette_collision().code==ControllerCode::unchanged);
 CHECK(doc.revision()==afterRevision);
 // Selecting another real painted tile never toggles either tile globally.
 CHECK(controller.apply_cell({3,1}));CHECK(controller.selected_palette()==b.palette_entry);
 CHECK(doc.palette_entry(a.palette_entry)->collision==solid&&doc.compile().artifact.collision.size()==2);
}
'''


class TileMapCollisionWorkflow(unittest.TestCase):
    def test_actual_controller_document_compiler_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 CPU module consumer")
        proof = ROOT / "Artifacts/Reports/tilemap-collision-workflow-oct28"
        build = ROOT / "Artifacts/Intermediate/tests/tilemap-collision-workflow"
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.tilemap_collision_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [engine_module_source(ROOT / "Engine/modules", name) for name in (
            "authoring.document.ixx", "authoring.tilemap.ixx", "asset.tilemap_artifact.ixx")]
        modules += [portable / "modules" / name for name in ("epoch.gui.ixx", "epoch.gui.tile_workspace.ixx")]
        implementations = [ROOT / path for path in (
            "Engine/src/authoring/tilemap/authoring.tilemap_document.cpp", "Engine/src/assets/tilemap/asset.tilemap_artifact.cpp")]
        implementations += [portable / "src/epochgui/tile_workspace.cpp"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(TileMapCollisionWorkflow LANGUAGES CXX)
add_executable(tilemap_collision "{source.as_posix()}" {" ".join('"'+p.as_posix()+'"' for p in implementations)})
target_compile_features(tilemap_collision PRIVATE cxx_std_23)
target_compile_options(tilemap_collision PRIVATE /W3 /WX /utf-8)
target_sources(tilemap_collision PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_tilemap_collision COMMAND tilemap_collision)
set_tests_properties(actual_tilemap_collision PROPERTIES TIMEOUT 10)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        receipts = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            receipts.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-7000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": receipts,
            "ownerSha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(),
            "scope": "Actual apply/select/stage/apply/guard/accept methods with real tile document/compiler/history/serialization; unrelated project pipeline/controller construction are ports.",
            "cases": ["fresh palette rejection preserves exact source/history", "empty-cell collision cannot change selected tile",
                      "painted cell selects its actual tile without document mutation", "explicit stage/apply only affects selected tile placements",
                      "no-op apply", "Undo/Redo", "serialize/reopen compiled collision", "collision action availability"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
