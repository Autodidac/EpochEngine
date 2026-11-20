"""Real calculator asset generation preserves edited GUI source on regeneration."""
from pathlib import Path
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.gui_contract import run_gui_contract

ROOT = Path(__file__).resolve().parents[2]


class CalculatorRegeneration(unittest.TestCase):
    def test_edited_source_survives_regeneration_and_reopen(self):
        shell = ROOT / 'Engine/src/editor/implementation/editor.project_shell_generation.inl'
        text = shell.read_text(encoding='utf-8')
        saved_gui = ROOT / 'Engine/examples/BasicSoftwareExampleCalculator/Assets/Gui/main.epochgui'
        owners = definition(text, '    [[nodiscard]] static bool ensure_project_gui_assets(')
        code = r'''
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
import authoring.gui_document;
import authoring.gui_compiler;
import asset.gui_artifact;
import project.gui_runtime;
import project.gui_library;
import platform.filesystem;
namespace fs=std::filesystem;
OWNERS
#define CHECK(value) do {if(!(value)){std::ofstream("failure.txt")<<__LINE__<<" "<<diagnostic;return __LINE__;}}while(false)
int main(){
 namespace gui=epochengine::authoring::gui;
 namespace library=epochengine::project_gui;
 const auto root=fs::current_path()/("calculator-project-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::string diagnostic;
 const auto path=root/fs::path{library::canonical_source_path};
 fs::create_directories(path.parent_path());fs::copy_file("GUI_SOURCE",path);
 std::ifstream source(path,std::ios::binary);
 const std::vector<char> seedBytes{std::istreambuf_iterator<char>{source},{}};
 const auto seed=gui::deserialize_gui_document(std::as_bytes(std::span{seedBytes}));CHECK(seed);
 const auto seedArtifact=gui::compile_gui_document(*seed.snapshot);CHECK(seedArtifact);
 library::ArtifactLibrary firstLibrary{"calculator-regeneration",root.generic_string()};
 const auto location=firstLibrary.identify(library::canonical_source_path,seedArtifact.artifact);CHECK(location);
 const auto directory=fs::path{location.storage_path}.parent_path();fs::create_directories(directory);
 const auto collision=directory/".pending.0000000000000001";
 {std::ofstream out(collision);out<<"foreign staging file";}
 CHECK(!fs::exists(location.storage_path));
 const auto cold=firstLibrary.load_or_compile(library::canonical_source_path);
 CHECK(cold&&cold.artifact.identity==seedArtifact.artifact.identity);
 {std::ifstream in(collision);const std::string kept{std::istreambuf_iterator<char>{in},{}};CHECK(kept=="foreign staging file");}
 CHECK(fs::exists(location.storage_path));
 CHECK(ensure_project_gui_assets("calculator-regeneration",root,library::canonical_source_path,diagnostic));
 const auto read=[&](){std::ifstream in(path,std::ios::binary);std::vector<char> raw{std::istreambuf_iterator<char>{in},{}};
  std::vector<std::byte> out;for(char c:raw)out.push_back(static_cast<std::byte>(c));return out;};
 const auto original=read();const auto decoded=gui::deserialize_gui_document(original);CHECK(decoded);
 gui::GuiDocument edited{*decoded.snapshot};CHECK(edited.valid());bool changed{};
 bool titleChanged{};
 float titleHeight{};
 for(const auto& widget:edited.widgets())if(widget.descriptor.name=="CalculatorTitle"){
  const auto* text=std::get_if<gui::TextContent>(&widget.descriptor.content);
  CHECK(text&&text->text=="Calculator"&&text->wrap);
  titleHeight=widget.descriptor.layout.height;
  CHECK(titleHeight>0.0f);
  CHECK(edited.update_content(widget.handle,gui::TextContent{"My personal calculator",true}));titleChanged=true;
 }
 CHECK(titleChanged);
 for(const auto& widget:edited.widgets())if(widget.descriptor.interaction.action=="calculator.clear"){
  CHECK(edited.update_content(widget.handle,gui::ButtonContent{"Reset calculator"}));changed=true;
 }
 CHECK(changed);const auto encoded=gui::serialize_gui_document(edited.snapshot());CHECK(encoded);
 {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());CHECK(out.good());}
 CHECK(ensure_project_gui_assets("calculator-regeneration",root,library::canonical_source_path,diagnostic));
 CHECK(read()==encoded.bytes);
 const auto reopened=gui::deserialize_gui_document(read());CHECK(reopened);
 gui::GuiDocument restoredDocument{*reopened.snapshot};
 bool matchingTitle{};
 for(const auto& widget:restoredDocument.widgets())if(widget.descriptor.name=="CalculatorTitle")
  matchingTitle=widget.descriptor.layout.height==titleHeight;
 CHECK(matchingTitle);
 const auto compiled=gui::compile_gui_document(*reopened.snapshot);CHECK(compiled);
 library::ArtifactLibrary restored{"calculator-regeneration",root.generic_string()};
 auto loaded=restored.load_exact(library::canonical_source_path,compiled.artifact.identity.key);CHECK(loaded);
 epochengine::project_gui_runtime::RuntimeSession runtime{std::move(loaded.artifact)};
 const auto frame=runtime.build_frame({520,440});CHECK(frame);
 CHECK(std::ranges::any_of(frame.widgets,[](const auto& w){return w.primary_text=="Reset calculator";}));
 CHECK(std::ranges::any_of(frame.widgets,[](const auto& w){return w.primary_text=="My personal calculator";}));
 namespace rt=epochengine::project_gui_runtime;
 const auto portrait=runtime.build_frame({1080,2400});CHECK(portrait);
 const auto find=[](const auto& current,std::string_view name){
  return std::ranges::find_if(current.widgets,[&](const auto& widget){return widget.name==name;});};
 const auto title=find(portrait,"CalculatorTitle");
 const auto expression=find(portrait,"CalculatorExpression");
 CHECK(title!=portrait.widgets.end()&&title->visible);
 CHECK(expression!=portrait.widgets.end()&&expression->visible);
 CHECK(portrait.scale>1080.0f/seedArtifact.artifact.virtual_width);
 CHECK(title->bounds.y<expression->bounds.y && expression->bounds.x<portrait.viewport.x/3.0f);
 const auto bounds=expression->bounds;
 CHECK(runtime.pointer_press({bounds.x+bounds.width/2,bounds.y+bounds.height/2})==rt::RuntimeCode::ready);
 CHECK(runtime.focused_widget()==expression->id);
 CHECK(runtime.set_text(expression->id,"9/4")==rt::RuntimeCode::ready);
 const auto safePortrait=runtime.build_frame({1080,2200},{0,80});CHECK(safePortrait);
 const auto safeExpression=find(safePortrait,"CalculatorExpression");
 CHECK(safeExpression!=safePortrait.widgets.end()&&safeExpression->focused);
 CHECK(safeExpression->clip.y>=80 && !runtime.pointer_captured({540,20}));
 const auto safeBounds=safeExpression->bounds;
 CHECK(runtime.pointer_captured({safeBounds.x+safeBounds.width/2,safeBounds.y+safeBounds.height/2}));
 const auto landscape=runtime.build_frame({2400,1080});CHECK(landscape);
 const auto wideExpression=find(landscape,"CalculatorExpression");
 CHECK(wideExpression!=landscape.widgets.end()&&wideExpression->bounds.x>1200.0f);
 CHECK(wideExpression->id==expression->id&&wideExpression->focused&&wideExpression->primary_text=="9/4");
 CHECK(landscape.scale==1080.0f/seedArtifact.artifact.virtual_height);
 // Invalid authored bytes must fail instead of being replaced by a new default.
 {std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<"invalid authored bytes";}
 const auto invalid=read();CHECK(!ensure_project_gui_assets("calculator-regeneration",root,library::canonical_source_path,diagnostic));
 CHECK(read()==invalid);
 CHECK(restored.load_or_compile(library::canonical_source_path));
 fs::remove(fs::path{loaded.locator.storage_path});fs::remove(location.storage_path);
 CHECK(restored.load_or_compile(library::canonical_source_path).code==library::LibraryCode::invalid_artifact);
 CHECK(!fs::exists(location.storage_path)&&read()==invalid);
}
'''.replace('GUI_SOURCE', saved_gui.as_posix()).replace('OWNERS', owners)
        run_gui_contract(self, 'calculator-regeneration', code, [shell, Path(__file__), saved_gui],
                         'Calculator materializer and runtime. Source edits survive regeneration, invalid source is preserved, and authored portrait layout retains text focus across rotation.',
                         extra_sources=('Engine/src/project/project.gui_library.cpp',),
                         extra_modules=('project.gui_library.ixx', 'project.asset_registry.ixx', 'platform.filesystem.ixx'),
                         libraries=('comdlg32', 'shell32', 'ole32'))


if __name__ == '__main__':
    unittest.main()
