"""Compiled motion owner + real inert DLLs and actual versioned host/registration code."""
from pathlib import Path
import hashlib,json,os,subprocess,time,unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import msvc_2022_toolsets, windows_sdk_includes
ROOT=Path(__file__).resolve().parents[2]

class ProjectMotionScripts(unittest.TestCase):
 def test_compiled_tracks_script_abi_and_project_copy_debug_release(self):
  if os.name != "nt": self.skipTest("Win32 project DLL load/unload ABI consumer")
  loader=(ROOT/'Engine/modules/scripting.system.ixx').read_text(encoding='utf-8')
  adapter=(ROOT/'Engine/src/editor/application/editor.application_scripting_renderers.inl').read_text(encoding='utf-8')
  selection=(ROOT/'Engine/src/editor/application/editor.application_scene_selection.inl').read_text(encoding='utf-8')
  workspaces=(ROOT/'Engine/src/editor/application/editor.application_code_workspaces.inl').read_text(encoding='utf-8')
  context=(ROOT/'Engine/src/editor/application/editor.application_context_snapshot.inl').read_text(encoding='utf-8')
  inspector=(ROOT/'Engine/src/editor/application/editor.frame_inspector.inl').read_text(encoding='utf-8')
  guard=definition(context,'                if (!editor_scene_script_mutation_allowed(it->second))')
  self.assertLess(context.index(guard),context.index('bool ok = scripting::load_or_reload_script(',context.index('    bool editor_run_script(')))
  actions_begin=inspector.index('                const bool projectScriptReady=')
  actions=inspector[actions_begin:inspector.index('                if (const auto action = gui::inline_button_row(',actions_begin)]
  document=(ROOT/'Engine/src/editor/application/editor.application_document_editing.inl').read_text(encoding='utf-8')
  bindings=definition(document,'        [[nodiscard]] bool editor_project_viewport_input_reserved(')+'\n'+definition(adapter,'        struct ScriptTranslationRegistration')+';\n'+''.join(definition(adapter,s) for s in (
   '        [[nodiscard]] bool editor_scene_script_mutation_allowed(',
   '        [[nodiscard]] project_motion_track::Binding script_translation_binding(',
   '        int script_copy_selected_translation_callback(', '        [[nodiscard]] bool commit_script_translation_registration('))
  code=r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "scripting.epoch_api.h"
import project.motion_track;
import platform.filesystem;
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
namespace fs=std::filesystem;using namespace epochengine;
namespace core{struct Context{};namespace path{fs::path candidate_data_root(){return {};}}}
namespace scene{using SceneObjectId=std::uint64_t;constexpr auto kInvalidSceneObjectId=SceneObjectId{};}
struct EditorEntity{scene::SceneObjectId sceneObjectId{};bool editorOnly{};std::string type{"StaticMesh"};};
struct Id{std::uint64_t value{17};};struct Identity{Id scene;};
struct SceneDocument{Identity identity()const{return {};}};
struct Lifecycle{struct Evidence{struct Generations{struct Selected{std::uint64_t generation{3};}selected_project;}generations;};Evidence evidence()const{return {};}};
struct Oscillator{scene::SceneObjectId entity_id{};};
struct EditorState{
 std::string projectRoot,projectId{"project-1"};Lifecycle projectLifecycle;SceneDocument sceneDocument;
 scene::SceneObjectId selectedEntityId{7};std::vector<EditorEntity>entities{{7,false},{9,false}};
 std::unordered_map<scene::SceneObjectId,project_motion_track::Track>scriptTranslationTracks;
 std::unordered_map<scene::SceneObjectId,Oscillator>scriptCubeOscillators;Oscillator scriptOscillator;bool sceneInteractionDirty{};
 bool scriptEditorDirty{},scriptBuildPending{};std::string scriptBuildStatus,activeScript,selectedProjectFile,selectedAssetPath,scriptEditorStatus;
 struct RuntimeHandle{bool live{};bool valid()const{return live;}}projectRuntimeProcess;
 bool projectRuntimeInEditor{};struct{bool playing{};}timelineState;
 std::optional<int>projectBuildPending;bool projectBuildRunAfterBuild{},projectBuildRunInEditor{};
};
struct Storage{std::mutex mutex;std::map<const core::Context*,EditorState>states;};Storage store;
Storage&editor_storage(){return store;}
ENTITY_INDEX
BINDINGS
unsigned loaderCalls{},refusalLogs{};
void push_editor_log(EditorState&,std::string_view){++refusalLogs;}
bool actual_preload_guard(const core::Context*ctx){const auto it=store.states.find(ctx);if(it==store.states.end())return false;PRELOAD_GUARD ++loaderCalls;return true;}
namespace gui{struct InlineButtonSpec{std::string_view label;float width{};bool enabled{true};};}
std::array<bool,3>actual_movement_actions(const EditorState&editor,bool movable=true,bool motionAttached=true){MOVEMENT_ACTIONS return {movementActions[0].enabled,movementActions[1].enabled,movementActions[2].enabled};}
using run_script_fn=void(*)(EpochScriptHost*);using run_script_v2_fn=int(*)(const EpochScriptHostV2*);
INVOKE
fs::path templateSource;
fs::path resolve_editor_path(const fs::path&p){return p;}
fs::path editor_resolve_candidate_project_path(const fs::path&p){return p;}
std::string editor_resolve_script_source_path(std::string_view){return templateSource.string();}
DISPLAY_PATH
bool load_script_source_editor(EditorState&,const fs::path&p,bool){return fs::is_regular_file(p);}
PREPARE_TEMPLATE
std::string contents(const fs::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
int legacyCalls{};void legacy_log(void*,const char*){++legacyCalls;}
int main(int argc,char**argv){
 CHECK(argc==3);const auto root=fs::absolute(argv[1]);templateSource=fs::absolute(argv[2]);
 const project_motion_track::Binding binding{3,17,7,"project-1"};
 const std::array<project_motion_track::Keyframe,3>keys{{{0,{0,0,0}},{2,{0,2,0}},{4,{4,2,0}}}};
 auto built=project_motion_track::make_track(binding,keys);CHECK(built);
 auto track=built.track;CHECK(project_motion_track::sample_translation(track,binding,1).offset[1]==1);
 CHECK(project_motion_track::sample_translation(track,binding,2).offset[1]==2);
 CHECK(project_motion_track::sample_translation(track,binding,-100).offset[1]==0);
 CHECK(project_motion_track::sample_translation(track,binding,500).offset[0]==4);
 CHECK(project_motion_track::sample_translation(track,binding,3).offset[0]==2);
 CHECK(project_motion_track::sample_translation(track,binding,1).offset[1]==1); // same sample after reverse/seek history.
 auto stale=binding;++stale.project_generation;CHECK(project_motion_track::sample_translation(track,stale,1).code==project_motion_track::Code::stale_binding);
 stale=binding;++stale.scene_id;CHECK(!project_motion_track::sample_translation(track,stale,1));
 stale=binding;++stale.entity_id;CHECK(!project_motion_track::sample_translation(track,stale,1));
 stale=binding;stale.project_id="foreign";CHECK(!project_motion_track::sample_translation(track,stale,1));
 const auto nan=(std::numeric_limits<double>::quiet_NaN)();CHECK(!project_motion_track::sample_translation(track,binding,nan));
 CHECK(!project_motion_track::make_track(binding,{}));auto invalid=keys;invalid[1].seconds=invalid[0].seconds;CHECK(!project_motion_track::make_track(binding,invalid));
 invalid=keys;invalid[1].seconds=-1;CHECK(!project_motion_track::make_track(binding,invalid));
 invalid=keys;invalid[2].seconds=86401;CHECK(!project_motion_track::make_track(binding,invalid));
 invalid=keys;invalid[1].offset[0]=nan;CHECK(!project_motion_track::make_track(binding,invalid));
 std::vector<project_motion_track::Keyframe>many(257);for(std::size_t i{};i<many.size();++i)many[i].seconds=double(i);
 CHECK(!project_motion_track::make_track(binding,many));many.resize(256);CHECK(project_motion_track::make_track(binding,many));
 EpochScriptHost legacy{};legacy.log=&legacy_log;
 HMODULE old=LoadLibraryW((root/"legacy.dll").c_str());CHECK(old);auto legacyEntry=reinterpret_cast<run_script_fn>(GetProcAddress(old,"run_script"));
 CHECK(invoke_script_entry(legacyEntry,nullptr,&legacy,nullptr)&&legacyCalls==1);CHECK(FreeLibrary(old));
 HMODULE lib=LoadLibraryW((root/"motion.dll").c_str());CHECK(lib);
 CHECK(!GetProcAddress(lib,"run_script")); // Old engines safely reject this v2-only script.
 auto entry=reinterpret_cast<run_script_v2_fn>(GetProcAddress(lib,"run_script_v2"));CHECK(entry);
 CHECK(!invoke_script_entry(nullptr,entry,&legacy,nullptr));
 core::Context context;auto&state=store.states[&context];ScriptTranslationRegistration request{.context=&context,.binding=binding};
 EpochScriptHostV2 host{EPOCH_SCRIPT_HOST_ABI_V2,sizeof(EpochScriptHostV2),&legacy,&request,&script_copy_selected_translation_callback};
 auto oldSize=host;oldSize.struct_size=8;CHECK(!invoke_script_entry(nullptr,entry,&legacy,&oldSize)&&!request.pending);
 auto wrongVersion=host;wrongVersion.abi_version=1;CHECK(!invoke_script_entry(nullptr,entry,&legacy,&wrongVersion)&&!request.pending);
 CHECK(invoke_script_entry(nullptr,entry,&legacy,&host)&&request.pending&&state.scriptTranslationTracks.empty());
 CHECK(FreeLibrary(lib));CHECK(commit_script_translation_registration(state,request));
 const auto copied=state.scriptTranslationTracks.at(7);CHECK(project_motion_track::sample_translation(copied,binding,2).offset[1]==1); // after DLL unload.
 // A runtime can acquire the project viewport after the DLL copied its data.
 // Such Editor-only motion must not commit behind the running child.
 auto runtimeRace=request;const std::array<project_motion_track::Keyframe,1>replacement{{{0,{0,9,0}}}};
 runtimeRace.pending=project_motion_track::make_track(binding,replacement).track;
 state.projectRuntimeInEditor=true;state.projectRuntimeProcess.live=true;
 state.projectRoot="owned-project";
 CHECK(!actual_preload_guard(&context)&&loaderCalls==0&&refusalLogs==1);
 CHECK((actual_movement_actions(state)==std::array<bool,3>{true,false,false}));
 CHECK(!commit_script_translation_registration(state,runtimeRace));
 CHECK(project_motion_track::sample_translation(state.scriptTranslationTracks.at(7),binding,2).offset[1]==1);
 request.pending.reset();const EpochScriptTranslationKey runtimeKey{0,{0,3,0}};
 CHECK(script_copy_selected_translation_callback(&request,&runtimeKey,1)!=0&&!request.pending);
 state.projectRuntimeInEditor=false;CHECK(!commit_script_translation_registration(state,runtimeRace));
 CHECK(!actual_preload_guard(&context)&&loaderCalls==0&&refusalLogs==2);
 state.projectRuntimeProcess.live=false;CHECK(commit_script_translation_registration(state,runtimeRace)); // retirement releases authoring
 CHECK(actual_preload_guard(&context)&&loaderCalls==1);
 CHECK((actual_movement_actions(state)==std::array<bool,3>{true,true,true}));
 state.projectRuntimeInEditor=true;state.projectRuntimeProcess.live=false;state.timelineState.playing=true;
 CHECK(commit_script_translation_registration(state,runtimeRace)); // sampled timeline alone remains authorable
 CHECK(actual_preload_guard(&context)&&loaderCalls==2);
 CHECK((actual_movement_actions(state)==std::array<bool,3>{true,true,true}));
 state.timelineState.playing=false;state.projectRuntimeInEditor=false;request.pending=copied;CHECK(commit_script_translation_registration(state,request));
 auto refused=request;refused.binding.project_id="old-project";CHECK(!commit_script_translation_registration(state,refused));
 CHECK(project_motion_track::sample_translation(state.scriptTranslationTracks.at(7),binding,2).offset[1]==1);
 request.pending.reset();state.selectedEntityId=9;const EpochScriptTranslationKey one{0,{0,0,0}};
 CHECK(script_copy_selected_translation_callback(&request,&one,1)!=0&&!request.pending);state.selectedEntityId=7;
 CHECK(script_copy_selected_translation_callback(&request,&one,0)!=0&&!request.pending);
 CHECK(script_copy_selected_translation_callback(&request,nullptr,1)!=0&&!request.pending);
 const EpochScriptTranslationKey bad[]{ {2,{0,0,0}},{1,{1,0,0}} };
 CHECK(script_copy_selected_translation_callback(&request,bad,2)!=0&&!request.pending);
 CHECK(script_copy_selected_translation_callback(&request,&one,1)==0&&request.pending);
 state.entities[0].editorOnly=true;CHECK(!commit_script_translation_registration(state,request));state.entities[0].editorOnly=false;
 state.projectRoot=(root/"project").string();fs::create_directories(root/"project");state.projectRuntimeInEditor=true;state.projectRuntimeProcess.live=true;
 CHECK(prepare_project_movement_script(state));state.projectRuntimeInEditor=false;state.projectRuntimeProcess.live=false; // source access remains available
 const auto owned=root/"project/scripts/timeline_translation.ascript.cpp";CHECK(contents(owned)==contents(templateSource));
 {std::ofstream out(owned,std::ios::binary);out<<"// user edit retained\n";}CHECK(prepare_project_movement_script(state));CHECK(contents(owned)=="// user edit retained\n");
 state.scriptEditorDirty=true;CHECK(!prepare_project_movement_script(state));state.scriptEditorDirty=false;
 fs::create_directories(root/"redirect-project");fs::create_directories(root/"outside");std::error_code ec;
 fs::create_directory_symlink(root/"outside",root/"redirect-project/scripts",ec);
 if(!ec){state.projectRoot=(root/"redirect-project").string();CHECK(!prepare_project_movement_script(state));CHECK(!fs::exists(root/"outside/timeline_translation.ascript.cpp"));}
}
'''.replace('ENTITY_INDEX',definition(selection,'        [[nodiscard]] std::optional<std::size_t> editor_entity_index(')).replace('BINDINGS',bindings).replace('INVOKE',definition(loader,'    [[nodiscard]] inline bool invoke_script_entry(')).replace('DISPLAY_PATH',definition(workspaces,'        [[nodiscard]] std::string display_project_path(')).replace('PREPARE_TEMPLATE',definition(workspaces,'        [[nodiscard]] bool prepare_project_movement_script('))
  code=code.replace('PRELOAD_GUARD',guard).replace('MOVEMENT_ACTIONS',actions)
  proof=ROOT/'Artifacts/Reports/project-motion-scripts-oct29'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  (proof/'consumer.cpp').write_text(code,encoding='utf-8')
  (proof/'legacy.cpp').write_text('#include "scripting.epoch_api.h"\nEPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost*h){if(h&&h->log)h->log(h->user_data,"legacy");}\n',encoding='utf-8')
  vc=msvc_2022_toolsets()[-1]
  sdk=Path('C:/Program Files (x86)/Windows Kits/10');kit=windows_sdk_includes(sdk)[-1];compiler=vc/'bin/Hostx64/x64/cl.exe';records=[]
  env=dict(os.environ);env['INCLUDE']=';'.join(str(p) for p in [vc/'include',kit/'ucrt',kit/'um',kit/'shared',kit/'winrt'])
  for config,flags in [('Debug',['/Od','/MDd','/D_DEBUG']),('Release',['/O2','/MD'])]:
   out=proof/config;out.mkdir(exist_ok=True);base=[str(compiler),'/nologo','/std:c++latest','/utf-8','/EHsc','/Gy','/Zc:inline',*flags,'/I'+str(ROOT/'Engine/include')]
   refs=[];objects=[]
   def run(cmd,label,timeout=60):
    started=time.monotonic();r=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True,timeout=timeout)
    (out/(label+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8');self.assertEqual(r.returncode,0,label+': '+r.stdout+r.stderr)
    records.append({'config':config,'step':label,'exit':r.returncode,'seconds':time.monotonic()-started})
   for name,path in [('temporal.request','Engine/modules/temporal.request.ixx'),('platform.filesystem','Engine/modules/platform.filesystem.ixx'),('project.motion_track','Engine/modules/project.motion_track.ixx')]:
    obj=out/(name+'.obj');ifc=out/(name+'.ifc');run(base+refs+['/c','/interface','/ifcOutput',str(ifc),'/Fo'+str(obj),str(ROOT/path)],name)
    refs+=['/reference',name+'='+str(ifc)];objects.append(str(obj))
   obj=out/'motion_impl.obj';run(base+refs+['/c','/Fo'+str(obj),str(ROOT/'Engine/src/project/project.motion_track.cpp')],'motion_impl');objects.append(str(obj))
   links=['/link','/LIBPATH:'+str(vc/'lib/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'um/x64'),'/LIBPATH:'+str(sdk/'Lib'/kit.name/'ucrt/x64'),'comdlg32.lib','shell32.lib','ole32.lib','windowscodecs.lib','gdi32.lib','user32.lib']
   for label,path in [('motion',ROOT/'Engine/src/scripts/script.timeline_translation.cpp'),('legacy',proof/'legacy.cpp')]:
    run(base+['/LD','/Fo'+str(out/(label+'.obj')),'/Fe'+str(out/(label+'.dll')),str(path)]+links,label+'_dll')
   exe=out/'consumer.exe';run(base+refs+['/Fo'+str(out/'consumer.obj'),'/Fe'+str(exe),str(proof/'consumer.cpp'),*objects]+links,'consumer')
   run([str(exe),str(out),str(ROOT/'Engine/src/scripts/script.timeline_translation.cpp')],'actual_contract',15)
  (proof/'result.json').write_text(json.dumps({'passed':True,'records':records,'scope':'Actual compiled C++23 temporal+motion+filesystem owners; real v1/v2 DLL load/unload; actual invoke/Editor staged registration and project template copy. Explicit minimal Editor state/source-editor ports. No GUI/model/full Engine or ProjectPlay script-host claim.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
