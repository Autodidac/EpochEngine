"""Run the real Planetary settings form against recording UI and job boundaries."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets

ROOT=Path(__file__).resolve().parents[2]


def fixture():
    view=(ROOT/'Engine/src/editor/application/editor.frame_viewport.inl').read_text(encoding='utf-8')
    module=(ROOT/'Engine/modules/project.planetary.ixx').read_text(encoding='utf-8')
    mesh=(ROOT/'Engine/modules/asset.mesh_artifact.ixx').read_text(encoding='utf-8')
    snapshot=(ROOT/'Engine/modules/project.planetary_snapshot.ixx').read_text(encoding='utf-8')
    snapshot_types='\n'.join(definition(snapshot,'    struct '+name+' final',True)
                             for name in ('TileKey','Patch','Limits','DrawPatch'))
    generation=(ROOT/'Engine/src/editor/application/editor.application_packages_creation.inl').read_text(encoding='utf-8')
    blocker=definition(generation,'        [[nodiscard]] std::string planetary_generation_block_reason(')
    start=definition(generation[generation.rfind('        void start_admitted_planetary_generation('):],
                     '        void start_admitted_planetary_generation(')
    preflight=start[start.index('{')+1:start.index('            std::uint32_t seed{};')]
    # Execute the actual settings/budget/preset owner. No provider, renderer,
    # scheduler, process or file write is launched by this fixture.
    begin=module.index('export namespace epochengine::project_planetary')
    end=module.index('    struct SceneMeshBudget',begin)
    settings=module[begin:end].replace('export namespace','namespace',1)+'}\n'
    body=definition(view,'                    if (planetAttached)')
    options=definition(view,'                    const gui::InlineButtonRowOptions planetButtons',True)
    limits=definition(mesh,'    struct Limits final',True)
    return r'''
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>
namespace epochengine::asset::mesh {class Artifact;LIMITS}
namespace epochengine::project_planetary_snapshot {struct Snapshot;SNAPSHOT_TYPES}
namespace epochengine {template<class...Args>std::string format_text(std::string_view value,Args&&...){return std::string(value);}}
SETTINGS
namespace project_planetary=epochengine::project_planetary;
namespace epochengine::project_planetary {
inline unsigned saves{};inline Settings saved;
bool save_settings(const std::filesystem::path&,const Settings& s){saved=s;++saves;return true;}
}
namespace gui {
struct Vec2 {float x{},y{};};
struct InlineButtonSpec {std::string_view label{};float width{};bool enabled=true;};
enum class InlineButtonSizing {Fill,FitContent};enum class InlineButtonAlignment {Start,Center};
struct InlineButtonRowOptions {InlineButtonSizing sizing{};InlineButtonAlignment alignment{};bool wrap{};float available_width{};};
struct SelectBoxOptions {std::string_view id{},placeholder{},selected{};std::span<const std::string_view> options;Vec2 size;};
struct SelectBoxResult {bool changed{};std::optional<std::size_t> selected_index;};
struct SliderOptions {std::string_view id{};float minimum{},maximum{},value{},step{};Vec2 size;};
struct SliderResult {float value{};bool changed{};};
float y{},firstActionY=-1,lastControlY{},measuredWidth=640,cellsMaximum{};unsigned presetClicks{};
std::string requested;std::vector<std::string> messages;
std::unordered_map<std::string,float> edits;
void label(std::string_view text){y+=22;messages.emplace_back(text);}
void wrapped_label(std::string_view text,float width){y+=22*std::max(1.f,std::ceil(static_cast<float>(text.size())*8/width));messages.emplace_back(text);}
void section_heading(std::string_view text){label(text);y+=10;}
struct EditResult {bool changed{};};
EditResult edit_box(std::string&,Vec2 size,unsigned,bool){y+=size.y+4;return {};}
bool toggle_switch(std::string_view label,bool& value,Vec2 size){y+=size.y+4;if(requested==label){value=!value;return true;}return false;}
SelectBoxResult select_box(SelectBoxOptions o){y+=o.size.y+4;return {};}
SliderResult slider(SliderOptions o){
 y+=o.size.y+4;lastControlY=y;if(o.id=="planet-cells")cellsMaximum=o.maximum;
 const auto at=edits.find(std::string(o.id));
 const float value=at==edits.end()?o.value:std::clamp(at->second,o.minimum,o.maximum);
 return {value,value!=o.value};
}
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> items,float height,float gap,InlineButtonRowOptions){
 if(items.size()==3&&items[0].label=="Build & Generate")firstActionY=y;
 y+=height+gap;
 for(std::size_t i=0;i<items.size();++i)if(items[i].label==requested&&items[i].enabled){
  if(requested=="Compact"||requested=="Fast"||requested=="Balanced"||requested=="Fine")++presetClicks;
  return i;
 }
 return {};
}
void fresh(std::string_view action){y=0;firstActionY=-1;lastControlY=0;cellsMaximum=0;presetClicks=0;requested=action;messages.clear();edits.clear();}
}
struct EditorState {
 struct MeshPort{bool coordinates{};};struct EntityPort{std::string category;std::optional<MeshPort> meshAsset;};
 std::vector<EntityPort> entities;
 project_planetary::Settings planetarySettings;
 std::string planetarySeedDraft="1337",planetaryStatus,projectRoot="fixture";
 std::string planetaryRadiusDraft="6371000";
 double planetaryRadiusSynced=6371000;
 std::uint32_t planetaryRadiusReferenceSynced{};
 bool projectBuildPending{},scriptBuildPending{},planetaryStreamFailed{},planetaryStreamContinue{};
 bool terrainWorkBusy{};
 struct Attachment {bool ready=true;bool source_attached()const{return ready;}} planetaryAttachment;
 std::string projectBuildConfiguration="Debug",projectBuildRequestedConfiguration;
 std::string planetaryStreamStatus;
 unsigned starts{},cancels{},refreshes{};project_planetary::Settings submitted;
};
bool planetary_project_busy(const EditorState& s){return s.terrainWorkBusy;}
BLOCK_REASON
void start_planetary_project_generation(EditorState& editor){
GENERATION_PREFLIGHT
++editor.starts;editor.submitted=editor.planetarySettings;}
void cancel_planetary_project_task(EditorState& s){++s.cancels;}
void refresh_editor_planetary_mesh_policy(EditorState& s){++s.refreshes;}
std::filesystem::path resolve_editor_path(const std::filesystem::path& p){return p;}
void request_planetary_feature_action(EditorState& s,bool generate){
 // Package prerequisite review is exercised with the real installer in the
 // Water admission contract. This form fixture records that action boundary.
 if(generate)start_planetary_project_generation(s);
 else {project_planetary::save_settings(s.projectRoot,s.planetarySettings);refresh_editor_planetary_mesh_policy(s);
 s.planetaryStatus="Requested settings saved; displayed mesh may be from the previous generation.";}
}
void draw(EditorState& editor,bool planetBusy,float centerWidth){
 editor.terrainWorkBusy=planetBusy;
 const bool planetAttached=true;
 OPTIONS
 BODY
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 for(float width:{220.f,520.f,960.f}){
  EditorState s;const auto original=s.planetarySettings;
  CHECK(project_planetary::valid_settings(original));
  gui::fresh("");draw(s,false,width);
  CHECK(gui::firstActionY>100&&gui::firstActionY>=gui::lastControlY&&s.planetarySettings==original&&s.starts==0&&gui::presetClicks==0);
  gui::fresh("Build & Generate");draw(s,false,width);
  CHECK(s.starts==1&&s.submitted==original&&gui::firstActionY>=gui::lastControlY&&gui::presetClicks==0);
 }
 EditorState custom;custom.planetarySeedDraft="987";
 gui::fresh("Build & Generate");gui::edits={{"planet-cells",48.f},{"planet-snapshot-memory",64.f},
  {"planet-lod-pixels",170.f},{"planet-lod-stability",20.f}};
 draw(custom,false,520);
 CHECK(gui::cellsMaximum==64&&gui::presetClicks==0&&custom.starts==1);
 CHECK(custom.submitted.cells_per_tile==48&&custom.submitted.snapshot_megabytes==64&&custom.submitted.seed==987);
 CHECK(custom.submitted.patch_pixels==170&&std::abs(custom.submitted.lod_hysteresis-.2)<1e-6);
 EditorState tiled;tiled.planetarySettings.mode="tiled";tiled.planetarySettings.tiles_per_axis=8;
 gui::fresh("Build & Generate");gui::edits={{"planet-cells",32.f},{"planet-snapshot-memory",16.f}};
 draw(tiled,false,520);
 CHECK(tiled.starts==1&&tiled.submitted.mode=="tiled"&&tiled.submitted.cells_per_tile==32&&tiled.submitted.snapshot_megabytes==16);
 EditorState refused;gui::fresh("Build & Generate");gui::edits={{"planet-cells",64.f},{"planet-snapshot-memory",1.f}};
 draw(refused,false,520);CHECK(refused.starts==0);
 EditorState recovered;recovered.planetarySettings.cells_per_tile=64;recovered.planetarySettings.snapshot_megabytes=1;
 CHECK(!project_planetary::valid_settings(recovered.planetarySettings));
 gui::fresh("Build & Generate");gui::edits={{"planet-snapshot-memory",64.f}};
 draw(recovered,false,520);CHECK(recovered.starts==1&&recovered.submitted.snapshot_megabytes==64);
 EditorState seed;seed.planetarySeedDraft="4294967296";gui::fresh("Build & Generate");draw(seed,false,520);
 CHECK(seed.starts==0&&std::ranges::any_of(gui::messages,[](const auto& s){return s.find("whole-number seed")!=std::string::npos;}));
 EditorState busy;gui::fresh("Cancel");draw(busy,true,520);CHECK(busy.cancels==1&&busy.starts==0);
 EditorState save;save.projectBuildPending=true;gui::fresh("Save Settings");draw(save,false,520);
 CHECK(project_planetary::saves==1&&save.refreshes==1&&save.starts==0);
 EditorState blocked;blocked.projectBuildPending=true;gui::fresh("Build & Generate");draw(blocked,false,520);CHECK(blocked.starts==0);
 CHECK(std::ranges::any_of(gui::messages,[](const auto& s){return s.find("Debug project build")!=std::string::npos;}));
 blocked.planetarySettings.mode="universe";blocked.planetarySettings.reference_radius_meters=1737000.0;
 const auto requested=blocked.planetarySettings;blocked.projectBuildRequestedConfiguration="Release";
 start_planetary_project_generation(blocked);CHECK(blocked.starts==0&&blocked.planetarySettings==requested&&blocked.planetaryStatus.find("Release project build")!=std::string::npos);
 blocked.projectBuildPending=false;blocked.scriptBuildPending=true;
 start_planetary_project_generation(blocked);CHECK(blocked.starts==0&&blocked.planetarySettings==requested&&blocked.planetaryStatus.find("script build")!=std::string::npos);
 blocked.scriptBuildPending=false;blocked.terrainWorkBusy=true;
 start_planetary_project_generation(blocked);CHECK(blocked.starts==0&&blocked.planetarySettings==requested&&blocked.planetaryStatus.find("Terrain work")!=std::string::npos);
 blocked.terrainWorkBusy=false;blocked.planetaryAttachment.ready=false;
 start_planetary_project_generation(blocked);CHECK(blocked.starts==0&&blocked.planetarySettings==requested&&blocked.planetaryStatus.find("Package Manager")!=std::string::npos);
 CHECK(save.planetaryStatus.find("previous generation")!=std::string::npos);
 EditorState preset;gui::fresh("Balanced");draw(preset,false,520);
 CHECK(gui::presetClicks==1&&preset.starts==0&&project_planetary::valid_settings(preset.planetarySettings));
 CHECK(preset.planetarySettings.maximum_level==20&&preset.planetarySettings.maximum_leaves==384&&preset.planetarySettings.patch_pixels==210);
 gui::fresh("Fine");draw(preset,false,520);
 CHECK(gui::presetClicks==1&&preset.starts==0&&project_planetary::valid_settings(preset.planetarySettings));
 CHECK(preset.planetarySettings.maximum_level==20&&preset.planetarySettings.maximum_leaves==576&&preset.planetarySettings.patch_pixels==170);
 EditorState solar;gui::fresh("Original planetary sunlight");draw(solar,false,520);
 CHECK(solar.planetarySettings.original_sunlight&&solar.starts==0&&solar.planetaryStatus.find("Save Settings")!=std::string::npos);
 gui::fresh("Save Settings");draw(solar,false,520);CHECK(project_planetary::saved.original_sunlight);
 EditorState solarBusy;gui::fresh("Original planetary sunlight");draw(solarBusy,true,520);CHECK(!solarBusy.planetarySettings.original_sunlight);
}
'''.replace('LIMITS',limits).replace('SNAPSHOT_TYPES',snapshot_types).replace('SETTINGS',settings).replace('OPTIONS',options).replace('BODY',body).replace('BLOCK_REASON',blocker).replace('GENERATION_PREFLIGHT',preflight)


class PlanetarySettingsUi(unittest.TestCase):
    def test_first_frame_actions_custom_values_and_guards(self):
        parent=ROOT/'Artifacts/Reports/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        scratch=Path(tempfile.mkdtemp(prefix='planetary-settings-ui-',dir=parent))
        try:
            cpp=scratch/'contract.cpp';cpp.write_text(fixture(),encoding='utf-8')
            if os.name=='nt':
                tools=msvc_2022_toolsets()
                sdk=Path('C:/Program Files (x86)/Windows Kits/10');kits=sorted((sdk/'Include').glob('*'))
                if not tools or not kits:self.skipTest('MSVC/SDK required')
                tool,kit=tools[-1],kits[-1]
                base=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++latest','/EHsc','/W4','/WX','/I'+str(tool/'include'),'/I'+str(kit/'ucrt')]
                link=['/link','/LIBPATH:'+str(tool/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64')]
                modes=[('Debug',['/Od','/MDd','/D_DEBUG']),('Release',['/O2','/MD','/DNDEBUG'])]
            else:
                compiler=shutil.which('clang++')or shutil.which('g++')
                if not compiler:self.skipTest('C++23 compiler required')
                base=[compiler,'-std=c++23'];link=[];modes=[('Debug',['-O0']),('Release',['-O2','-DNDEBUG'])]
            for mode,flags in modes:
                with self.subTest(mode=mode):
                    binary=scratch/(mode+('.exe'if os.name=='nt'else''))
                    output=['/Fo'+str(scratch/(mode+'.obj')),'/Fe'+str(binary)]if os.name=='nt'else['-o',str(binary)]
                    built=subprocess.run(base+flags+output+[str(cpp)]+link,cwd=scratch,capture_output=True,text=True,timeout=60)
                    self.assertEqual(built.returncode,0,built.stdout[-9000:]+built.stderr)
                    ran=subprocess.run([str(binary)],cwd=scratch,capture_output=True,text=True,timeout=10)
                    self.assertEqual(ran.returncode,0,f'Actual Planetary UI/settings check failed at C++ line {ran.returncode}')
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()));shutil.rmtree(scratch)


if __name__=='__main__':unittest.main()
