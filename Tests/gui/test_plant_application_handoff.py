"""Real application profiles/forest modules plus production handoff/router CPU proof.

Only native context initialization and renderer presentation are substituted.
Native windows and the full Editor integration require their separate acceptance.
"""
from pathlib import Path
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


class PlantApplicationHandoff(unittest.TestCase):
    def test_real_profiles_compiled_asset_and_application_route(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC module consumer lane')
        snapshot = (ROOT/'Engine/src/editor/application/editor.application_context_snapshot.inl').read_text(encoding='utf-8')
        runtime = (ROOT/'Engine/src/epoch/implementation/epoch.session_runtime.inl').read_text(encoding='utf-8')
        payload = definition(snapshot, 'struct EditorPlantLabPreview final', True)
        handoff = definition(snapshot, 'bool editor_restore_plant_lab_preview(')
        capture = snapshot[snapshot.index('snapshot.valid = true;'):snapshot.index('snapshot.project_id =')]
        begin = runtime.index('const bool plantLabApplication = guiRoute == "application.plant_lab";')
        end = runtime.index('if (!restored)', begin)
        route = runtime[begin:end]
        clear_begin = runtime.index('if (pendingRestoreStatus == PendingEditorRestoreStatus::restored\n')
        clear_end = runtime.index('win->guiRoute.clear();', clear_begin)+len('win->guiRoute.clear();')
        clear_route = runtime[clear_begin:clear_end]
        proof = ROOT/'build/validation/plant-application-handoff'
        proof.mkdir(parents=True, exist_ok=True)
        source = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
import editor.application;
import forest.factory;
namespace epochengine {
namespace core { struct Context { unsigned identity{}; }; }
PAYLOAD
struct EditorContextSnapshot { bool valid{};std::shared_ptr<const EditorPlantLabPreview> plant_lab_preview; };
struct State {
 EditorApplicationKind applicationKind{EditorApplicationKind::Standard};
 forest::ForestAssetDocument plantLabDocument;
 forest::CompiledForestAsset plantLabCompiled;
 std::uint64_t plantLabCompiledRevision{},plantLabCompiledContentHash{};
 bool plantLabDocumentInitialized{};
 std::string project;
};
std::map<unsigned,State> states;
unsigned applicationLoads{},projections{},genericRestores{},handoffRestores{};
EditorContextSnapshot capture(const State& editor) {
 EditorContextSnapshot snapshot;
 CAPTURE
 return snapshot;
}
State& editor_state_for(const std::shared_ptr<core::Context>& ctx) { return states[ctx->identity]; }
void editor_load_application(const std::shared_ptr<core::Context>& ctx,EditorApplicationKind kind) {
 ++applicationLoads;auto& state=editor_state_for(ctx);state.applicationKind=kind;state.project.clear();
 assert(editor_application_supports_surface(editor_application_profile(kind),EditorApplicationSurface::PlantLab));
}
void ensure_plant_lab_preview_entities(State& state) {
 ++projections;assert(state.plantLabCompiled.valid&&state.plantLabCompiled.preview.segmentCount>0);
}
void push_editor_log(State&,const char*) {}
HANDOFF
bool editor_restore_context_snapshot(core::Context*,const EditorContextSnapshot&) {++genericRestores;return true;}
bool restore(const std::shared_ptr<core::Context>& targetCtx,const EditorContextSnapshot& editorSnapshot,std::string_view guiRoute) {
 ROUTE
 return restored;
}
}
int main() {
 using namespace epochengine;
 assert(!editor_application_supports_surface(standard_editor_application(),EditorApplicationSurface::PlantLab));
 assert(editor_application_supports_surface(plant_lab_editor_application(),EditorApplicationSurface::PlantLab));
 auto original=std::make_shared<core::Context>(core::Context{1});
 auto target=std::make_shared<core::Context>(core::Context{2});
 states[1].project="original-project-must-remain-open";
 auto document=forest::make_default_plant_lab_document();
 auto compiled=forest::compile_forest_asset(document);
 assert(compiled.valid&&compiled.sourceRevision==document.revision&&compiled.sourceContentHash==document.contentHash);
 states[1].plantLabDocument=document;states[1].plantLabCompiled=compiled;states[1].plantLabDocumentInitialized=true;
 const auto transferred=capture(states[1]);const auto immutable=transferred.plant_lab_preview;
 assert(immutable);
 // Later caller changes cannot replace the captured immutable handoff.
 document.name="later source edit";
 assert(restore(target,transferred,"application.plant_lab"));
 assert(applicationLoads==1&&projections==1&&genericRestores==0);
 assert(states[2].applicationKind==EditorApplicationKind::PlantLab&&states[2].project.empty());
 assert(states[2].plantLabDocument.name==immutable->document.name);
 assert(states[2].plantLabCompiled.sourceContentHash==immutable->compiled.sourceContentHash);
 assert(states[2].plantLabCompiledRevision==immutable->document.revision);
 assert(states[1].applicationKind==EditorApplicationKind::Standard&&states[1].project=="original-project-must-remain-open");
 auto invalid=std::make_shared<EditorPlantLabPreview>(*immutable);invalid->compiled.sourceRevision++;
 assert(!restore(target,EditorContextSnapshot{true,invalid},"application.plant_lab"));
 assert(applicationLoads==1&&genericRestores==0);
 assert(!restore(target,{},"application.plant_lab"));
 assert(restore(target,transferred,"pane.properties")&&genericRestores==1);
 enum class PendingEditorRestoreStatus { none,restored,failed };
 struct Window {std::string guiRoute;};
 const auto finishRoute=[&](std::string route,PendingEditorRestoreStatus pendingRestoreStatus) {
  auto win=std::make_unique<Window>(Window{std::move(route)});
  CLEAR_ROUTE
  return win->guiRoute;
 };
 assert(finishRoute("application.plant_lab",PendingEditorRestoreStatus::restored).empty());
 assert(finishRoute("application.plant_lab",PendingEditorRestoreStatus::failed)=="application.plant_lab");
 assert(finishRoute("pane.properties",PendingEditorRestoreStatus::restored)=="pane.properties");
}
'''.replace('PAYLOAD', payload).replace('HANDOFF', handoff).replace('CAPTURE', capture).replace('CLEAR_ROUTE', clear_route).replace('ROUTE', route)
        (proof/'consumer.cpp').write_text(source,encoding='utf-8')
        engine=(ROOT/'Engine').as_posix()
        modules=['editor.application','forest.factory','authoring.morphology','authoring.document','voxel.field','package.registry']
        module_sources='\n'.join(f'"{engine}/modules/{name}.ixx"' for name in modules)
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EpochPlantHandoffProof LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(plant_handoff consumer.cpp
 "{engine}/src/editor/editor.standard.cpp"
 "{engine}/src/editor/editor.plant_lab.cpp"
 "{engine}/src/editor/editor.gui.cpp")
target_sources(plant_handoff PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{engine}/modules" FILES
{module_sources})
target_include_directories(plant_handoff PRIVATE "{engine}/include")
target_compile_options(plant_handoff PRIVATE /W3 /WX /utf-8)
enable_testing()
add_test(NAME actual_plant_application_handoff COMMAND plant_handoff)
''',encoding='utf-8')
        commands=[['cmake','-S',str(proof),'-B',str(proof/'msvc'),'-G','Visual Studio 17 2022','-A','x64'],
                  ['cmake','--build',str(proof/'msvc'),'--config','Debug','--parallel','1'],
                  ['ctest','--test-dir',str(proof/'msvc'),'-C','Debug','--output-on-failure']]
        for index,command in enumerate(commands):
            completed=subprocess.run(command,cwd=proof,capture_output=True,text=True,timeout=120)
            (proof/f'lane-{index}.log').write_text(completed.stdout+completed.stderr,encoding='utf-8')
            self.assertEqual(completed.returncode,0,completed.stdout[-7000:]+completed.stderr)


if __name__ == '__main__': unittest.main()
