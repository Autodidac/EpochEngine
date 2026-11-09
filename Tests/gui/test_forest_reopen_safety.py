"""Compile the actual Plant Lab reopen/ensure/publish helpers with real Forest owners."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = next(p for p in Path(__file__).resolve().parents if (p/"Engine/modules/project.forest_library.ixx").is_file())
sys.path.insert(0,str(ROOT))
from Tests.build.test_opengl_sprite_stream import definition
from Tests.packages.test_forest_materialized_codec import MODULES

class ForestReopenSafety(unittest.TestCase):
    def test_reopen_errors_never_seed_or_publish_defaults(self):
        if os.name!="nt":self.skipTest("Installed MSVC actual-helper CPU lane")
        cmake=shutil.which("cmake")
        if not cmake:self.skipTest("Installed CMake required; never download")
        owner=ROOT/"Engine/src/editor/application/editor.application_scene_preview.inl"
        source=owner.read_text(encoding="utf-8")
        markers=("reopen_plant_lab_project_asset","ensure_plant_lab_compiled_asset","publish_plant_lab_project_asset")
        bodies=[definition(source,"[[nodiscard]] bool "+name+"(") for name in markers]
        packages=(ROOT/"Engine/src/editor/application/editor.application_packages_creation.inl").read_text(encoding="utf-8")
        package_bodies="\n".join(definition(packages,mark) for mark in (
            "        [[nodiscard]] std::string editor_json_escape(",
            "        [[nodiscard]] std::string safe_package_artifact_id(",
            "        [[nodiscard]] bool stage_forest_factory_package_opt_in(",
            "        [[nodiscard]] bool builtin_project_package_ready(",
            "        void add_builtin_project_package("))

        registry=(ROOT/"Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        scene=(ROOT/"Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        parser="\n".join(definition(registry,mark,semi) for mark,semi in (
            ("    enum class JsonStringFieldState",True),
            ("    struct JsonStringFieldResult",True),
            ("    [[nodiscard]] static JsonStringFieldResult inspect_json_string_field(",False),
            ("    [[nodiscard]] static std::optional<std::string> extract_json_string_field(",False),
            ("    [[nodiscard]] static std::string default_world_name(",False),
            ("    [[nodiscard]] static std::optional<EditorProjectKind> parse_project_manifest_kind(",False),
            ("    [[nodiscard]] static std::string parse_world_name(",False),
            ("    [[nodiscard]] static std::optional<OwnedProjectProfile>\n        parse_manifest_project_profile(",False)))
        parser+="\n"+definition(scene,"    EditorProjectAdmissionResult editor_inspect_project_manifest_identity(")
        code=r'''
#include "core.format_text.hpp"
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <optional>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <span>
#include <cstddef>
#include <vector>
import forest.factory;
import project.forest_library;
import package.registry;
import core.sha256;
namespace epochengine {
namespace fs=std::filesystem;
fs::path evidence;
void require(bool value,int line){if(value)return;std::ofstream(evidence/"failure.txt")<<line;std::exit(91);}
#define REQUIRE(condition) require(bool(condition),__LINE__)
std::vector<char> read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>{in},{}};}
void write(const fs::path& p,const std::vector<char>& b){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(b.data(),std::streamsize(b.size()));}
void text(const fs::path& p,std::string_view s){write(p,{s.begin(),s.end()});}
enum class EditorProjectKind {Game,Tool,Editor,EngineDevelopment};
struct CapabilityPolicy {};
namespace updater::distribution {constexpr std::string_view edition_id="epoch.development.site.v1";}
namespace core::path {
fs::path candidateRoot;
fs::path candidate_data_root(){return candidateRoot;}
fs::path engine_asset_dir(){return fs::current_path()/"immutable-engine-assets";}
}
std::optional<CapabilityPolicy> editor_project_capability_policy(std::string_view s){return s=="portable"?std::optional{CapabilityPolicy{}}:std::nullopt;}
namespace project_input {constexpr std::string_view canonical_source_path="Assets/Config/input_profile.epochinput";}
namespace project_sprite_animation {constexpr std::string_view canonical_source_path="Assets/Animations/sprite_animations.epochanim";}
namespace project_audio {constexpr std::string_view canonical_source_path="Assets/Audio/project_audio.epochaudio";}
namespace project_gui {constexpr std::string_view canonical_source_path="Assets/Gui/main.epochgui";}
struct EditorProjectAdmissionResult {bool succeeded{};std::string project_id,manifest_path,summary;};
struct OwnedProjectProfile {
EditorProjectKind kind{};
std::string id,display_name,root_path,scene_path,tilemap_path,input_profile_path,
sprite_animation_path,audio_profile_path,gui_path,world_name,runtime_scene_id,
manifest_path,template_family,default_script,engine_integration_mode,
public_include_root,demo_model_asset,description;
CapabilityPolicy renderer_capability{};
};
constexpr std::string_view kProjectManifestFormat="epoch-project-v1",kStaticRuntimeBuildProfile="epoch-runtime-static";
std::string read_text_file(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>{f},{}};}
bool project_path_below(const fs::path&p,const fs::path&r){auto rel=p.lexically_relative(r);return !rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..";}
fs::path candidate_project_path(const fs::path&r,const fs::path&p){return project_path_below(p,r)?p:fs::path{};}
fs::path resolve_repo_relative_path(const fs::path&p,const fs::path&r){return p.is_absolute()?p:r/p;}
PARSER
enum class EditorMainSurface {Scene,Project,ForestFactory};
namespace core {struct Context{};}
enum class EditorProjectInputProvision {project_default};
struct EditorState {
 std::string projectId,projectRoot,plantLabStatus,plantLabAssetStatus,packageInstallStatus,projectStatus;
 float packageInstallProgress{};bool showPackageManagerModal{true},workspaceCompanionsPending{};
 EditorMainSurface mainSurface{EditorMainSurface::Scene};
 forest::ForestAssetDocument plantLabDocument;
 forest::CompiledForestAsset plantLabCompiled;
 project_forests::ForestArtifactLocator plantLabPublishedArtifact;
 bool plantLabDocumentInitialized{};
 std::uint64_t plantLabCompiledRevision{999},plantLabCompiledContentHash{888};
 std::vector<int> entities{17,23};std::vector<std::string> logs;
 std::uint64_t beforeRevision{},beforeContent{};std::size_t beforeCursor{},beforeJournalSize{};
 struct {struct {bool project_selected{};} value;const auto& evidence()const{return value;}} projectLifecycle;
};
void push_editor_log(EditorState& s,std::string_view v){s.logs.emplace_back(v);}
fs::path resolve_editor_path(const fs::path& p){return fs::absolute(p).lexically_normal();}
std::string display_project_path(const fs::path& p){return p.generic_string();}
constexpr std::string_view kDefaultPlantLabLogicalPath="Assets/Forest/default.epoch_forest";
FUNCTIONS
void append_project_note(EditorState&,std::string_view,std::string_view,std::string_view){}
bool package_source_task_busy(const EditorState&){return false;}
bool planetary_project_busy(const EditorState&){return false;}
struct CreationResult{bool succeeded{};std::string summary,project_id,root_path,manifest_path;};
int createCalls{};
CreationResult editor_create_project_shell(EditorProjectKind,EditorProjectInputProvision){++createCalls;return {false,"Creation refused"};}
EditorProjectAdmissionResult editor_admit_project_manifest(std::string_view){return {};}
bool editor_open_project(const std::shared_ptr<core::Context>&,std::string_view){return false;}
CreationResult editor_stage_engine_arcade_project(std::string_view){return {false,"Arcade staging is a separate actual-owner contract"};}
PACKAGE_BODIES
std::unique_ptr<EditorState> project(const fs::path& base,std::string_view name){
 auto s=std::make_unique<EditorState>();const auto dir=base/name;fs::create_directories(dir);
 s->projectId="forest-reopen-safety";s->projectRoot=dir.generic_string();s->projectLifecycle.value.project_selected=true;
 text(dir/"project.epoch.json","{\"id\":\"forest-reopen-safety\",\"display_name\":\"Forest\",\"kind\":\"game\",\"scene\":\"worlds/main.epoch\"}\n");
 s->plantLabDocument=forest::make_default_plant_lab_document();
 REQUIRE(forest::apply_forest_profile_edit(s->plantLabDocument,forest::ForestProfileProperty::TargetHeightMeters,7.25f));
 s->beforeRevision=s->plantLabDocument.revision;s->beforeContent=s->plantLabDocument.contentHash;
 s->beforeCursor=s->plantLabDocument.historyCursor;s->beforeJournalSize=s->plantLabDocument.journal.size();
 return s;
}
project_forests::PublishedForestAsset publish_source(EditorState& s){
 auto document=forest::make_default_plant_lab_document();
 REQUIRE(forest::apply_forest_profile_edit(document,forest::ForestProfileProperty::TargetHeightMeters,11.5f));
 project_forests::ForestAssetLibrary library{s.projectId,fs::path{s.projectRoot}};
 return library.publish(kDefaultPlantLabLogicalPath,document);
}
void retained(EditorState& s){
 REQUIRE(!s.plantLabDocumentInitialized);
 REQUIRE(s.plantLabDocument.profile.config.targetHeightMeters==7.25f);
 REQUIRE(s.plantLabDocument.revision==s.beforeRevision&&s.plantLabDocument.contentHash==s.beforeContent&&s.plantLabDocument.historyCursor==s.beforeCursor&&s.plantLabDocument.journal.size()==s.beforeJournalSize);
 REQUIRE(s.plantLabCompiledRevision==999&&s.plantLabCompiledContentHash==888);
 REQUIRE(s.entities==std::vector<int>({17,23}));
 REQUIRE(!s.plantLabAssetStatus.empty()&&s.plantLabStatus==s.plantLabAssetStatus);
}
void manifest_identity(const fs::path& base){
 for(const auto& [name,payload]:std::vector<std::pair<std::string,std::string>>{
  {"corrupt-manifest","not json"},
  {"foreign-manifest","{\"id\":\"foreign\",\"display_name\":\"Foreign\",\"kind\":\"game\",\"scene\":\"worlds/main.epoch\"}"}}){
  auto s=project(base,name);text(fs::path{s->projectRoot}/"project.epoch.json",payload);
  REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);REQUIRE(!publish_plant_lab_project_asset(*s));retained(*s);
  REQUIRE(!fs::exists(fs::path{s->projectRoot}/kDefaultPlantLabLogicalPath));
 }
 auto s=project(base,"legacy-manifest");const auto path=fs::path{s->projectRoot}/"project.epoch.json";
 REQUIRE(editor_inspect_project_manifest_identity(path.generic_string(),s->projectId,s->projectRoot).succeeded);
 REQUIRE(!editor_inspect_project_manifest_identity(path.generic_string(),s->projectId,base.generic_string()).succeeded);
 REQUIRE(!editor_inspect_project_manifest_identity(path.generic_string(),"foreign",s->projectRoot).succeeded);
 core::path::candidateRoot=fs::path{s->projectRoot};
 REQUIRE(editor_inspect_project_manifest_identity(path.generic_string(),s->projectId,s->projectRoot).succeeded);
 core::path::candidateRoot=base/"foreign-private-data";
 REQUIRE(!editor_inspect_project_manifest_identity(path.generic_string(),s->projectId,s->projectRoot).succeeded);
 core::path::candidateRoot.clear();
 REQUIRE(ensure_plant_lab_compiled_asset(*s));
}
void stale(const fs::path& base){
 auto s=project(base,"stale-artifact");const auto old=publish_source(*s);REQUIRE(old);
 project_forests::ForestAssetLibrary library{s->projectId,fs::path{s->projectRoot}};
 auto authored=library.load_source(kDefaultPlantLabLogicalPath);REQUIRE(authored);
 REQUIRE(forest::apply_forest_profile_edit(authored.document,forest::ForestProfileProperty::TargetHeightMeters,14.0f));
 const auto latest=library.publish(kDefaultPlantLabLogicalPath,authored.document);REQUIRE(latest);
 const auto source=read(latest.source.storage_path),artifact=read(old.artifact.storage_path);
 REQUIRE(fs::remove(latest.artifact.storage_path));
 REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);
 REQUIRE(s->plantLabAssetStatus.find("revisions do not match")!=std::string::npos);
 REQUIRE(!publish_plant_lab_project_asset(*s));retained(*s);
 REQUIRE(read(latest.source.storage_path)==source&&read(old.artifact.storage_path)==artifact);
}
void reseal(std::vector<char>& bytes){
 REQUIRE(bytes.size()>32);const auto digest=core::sha256::hash(std::string_view{bytes.data(),bytes.size()-32});
 for(std::size_t i=0;i<32;++i)bytes[bytes.size()-32+i]=static_cast<char>(digest.bytes[i]);
}
std::string artifact_name(const std::vector<char>& bytes){
 constexpr char digits[]="0123456789abcdef";std::string name;
 for(std::size_t i=bytes.size()-32;i<bytes.size();++i){const auto value=static_cast<unsigned char>(bytes[i]);name+=digits[value>>4];name+=digits[value&15];}
 return name+".epoch_forest_artifact";
}
void compiler_upgrade(const fs::path& base){
 auto s=project(base,"compiler-upgrade");
 project_forests::ForestAssetLibrary library{s->projectId,fs::path{s->projectRoot}};
 auto doc=forest::make_default_plant_lab_document();const auto old=library.publish(kDefaultPlantLabLogicalPath,doc);REQUIRE(old);
 REQUIRE(forest::apply_forest_profile_edit(doc,forest::ForestProfileProperty::LeafSizeMeters,0.59f));
 const auto newer=library.publish(kDefaultPlantLabLogicalPath,doc);REQUIRE(newer);
 const auto source=read(newer.source.storage_path);auto bytes=read(newer.artifact.storage_path);
 REQUIRE(bytes[8]==3&&bytes[9]==0);bytes[10]=0;bytes[11]=0;bytes[12]=0;bytes[13]=0;reseal(bytes);
 const auto obsolete=newer.artifact.storage_path.parent_path()/artifact_name(bytes);write(obsolete,bytes);
 REQUIRE(fs::remove(newer.artifact.storage_path));
 REQUIRE(library.load_latest(kDefaultPlantLabLogicalPath).code==project_forests::ForestLibraryCode::rebuild_required);
 REQUIRE(ensure_plant_lab_compiled_asset(*s));
 REQUIRE(s->plantLabDocument.contentHash==doc.contentHash&&s->plantLabDocument.revision==doc.revision);
 REQUIRE(read(newer.source.storage_path)==source&&read(obsolete)==bytes);
 REQUIRE(library.load_latest(kDefaultPlantLabLogicalPath));
 const auto current=read(s->plantLabPublishedArtifact.storage_path);
 REQUIRE(current[8]==3&&current[10]==1);
 // A rehashed current-compiler descriptor is still rejected, never recompiled
 // as though it came from an older compiler.
 auto tampered=current;tampered[tampered.size()-33]^=1;reseal(tampered);
 const auto damaged=s->plantLabPublishedArtifact.storage_path.parent_path()/artifact_name(tampered);write(damaged,tampered);
 REQUIRE(library.load_latest(kDefaultPlantLabLogicalPath).code==project_forests::ForestLibraryCode::integrity_failure);
 REQUIRE(read(newer.source.storage_path)==source&&read(obsolete)==bytes);
}
void saved_project_upgrade(const fs::path& base,const fs::path& fixture){
 auto s=project(base,"saved-project-upgrade");
 const auto root=fs::path{s->projectRoot};
 const auto manifest=read(fixture/"project.epoch.json");write(root/"project.epoch.json",manifest);
 s->projectId=*extract_json_string_field(std::string{manifest.begin(),manifest.end()},"id");
 fs::copy(fixture/"Assets",root/"Assets",fs::copy_options::recursive);
 fs::copy(fixture/"Library",root/"Library",fs::copy_options::recursive);
 project_forests::ForestAssetLibrary library{s->projectId,root};
 const auto source=library.load_source(kDefaultPlantLabLogicalPath);REQUIRE(source);
 std::vector<std::pair<fs::path,std::vector<char>>> preserved;
 for(const auto& entry:fs::recursive_directory_iterator(root/"Library"))if(entry.is_regular_file())preserved.emplace_back(entry.path(),read(entry.path()));
 const auto sourceBytes=read(root/kDefaultPlantLabLogicalPath);
 REQUIRE(library.load_latest(kDefaultPlantLabLogicalPath).code==project_forests::ForestLibraryCode::rebuild_required);
 REQUIRE(ensure_plant_lab_compiled_asset(*s));
 REQUIRE(s->plantLabDocument.contentHash==source.document.contentHash&&s->plantLabDocument.revision==source.document.revision);
 REQUIRE(s->plantLabDocument.historyCursor==source.document.historyCursor&&s->plantLabDocument.journal.size()==source.document.journal.size());
 REQUIRE(read(root/kDefaultPlantLabLogicalPath)==sourceBytes);
 for(const auto& [path,bytes]:preserved)REQUIRE(read(path)==bytes);
 REQUIRE(library.load_latest(kDefaultPlantLabLogicalPath));
 text(base/"saved-project-result.txt","PASS: copied real saved source and all old compiled artifacts reopened through actual helper; source/journal bytes and old artifacts preserved.\n");
}
void errors(const fs::path& base){
 {
  auto s=project(base,"corrupt-artifact");const auto saved=publish_source(*s);REQUIRE(saved);
  const auto source=read(saved.source.storage_path);auto artifact=read(saved.artifact.storage_path);
  REQUIRE(!artifact.empty());artifact.back()^=1;write(saved.artifact.storage_path,artifact);
  REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);
  REQUIRE(s->plantLabAssetStatus.find("integrity_failure")!=std::string::npos);
  REQUIRE(!publish_plant_lab_project_asset(*s));retained(*s);
  REQUIRE(read(saved.source.storage_path)==source&&read(saved.artifact.storage_path)==artifact);
 }
 {
  auto s=project(base,"corrupt-source");const auto saved=publish_source(*s);REQUIRE(saved);
  const auto artifact=read(saved.artifact.storage_path);const std::vector<char> bad{'b','a','d'};write(saved.source.storage_path,bad);
  REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);REQUIRE(!publish_plant_lab_project_asset(*s));retained(*s);
  REQUIRE(read(saved.source.storage_path)==bad&&read(saved.artifact.storage_path)==artifact);
 }
 {
  auto s=project(base,"missing-artifact");const auto saved=publish_source(*s);REQUIRE(saved);const auto source=read(saved.source.storage_path);
  REQUIRE(fs::remove(saved.artifact.storage_path));REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);
  REQUIRE(!publish_plant_lab_project_asset(*s));retained(*s);REQUIRE(read(saved.source.storage_path)==source);
 }
 {auto s=project(base,"invalid-id");s->projectId="\n";REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);}
 {auto s=project(base,"unselected");s->projectLifecycle.value.project_selected=false;REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);}
 {auto s=project(base,"partial-id");s->projectId.clear();REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);}
 {auto s=project(base,"missing-root");s->projectRoot=(base/"not-a-project").generic_string();REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);}
 {
  auto s=project(base,"dangling-source");const auto path=fs::path{s->projectRoot}/kDefaultPlantLabLogicalPath;
  fs::create_directories(path.parent_path());fs::create_symlink(fs::path{s->projectRoot}/"missing-target",path);
  REQUIRE(!ensure_plant_lab_compiled_asset(*s));retained(*s);REQUIRE(fs::is_symlink(fs::symlink_status(path)));
 }
}
void builtin_actions(const fs::path& base){
 const auto* package=package_registry::find(package_registry::kEngineForestFactoryPackageId);REQUIRE(package);
 auto s=project(base,"forest-package-add");
 const auto context=std::make_shared<core::Context>();
 add_builtin_project_package(*s,context,*package);
 REQUIRE(createCalls==0&&!s->showPackageManagerModal&&s->mainSurface==EditorMainSurface::ForestFactory);
 REQUIRE(s->workspaceCompanionsPending&&s->packageInstallProgress==1.0f);
 const auto root=fs::path{s->projectRoot};
 REQUIRE(fs::is_regular_file(root/"assets/packages/engine_forest_factory.package.json"));
 REQUIRE(fs::is_regular_file(root/"assets/packages/engine_forest_factory/default.forest.json"));
 project_forests::ForestAssetLibrary library{s->projectId,root};
 REQUIRE(library.load_source(kDefaultPlantLabLogicalPath)&&library.load_latest(kDefaultPlantLabLogicalPath));
 REQUIRE(s->entities==std::vector<int>({17,23}));
 auto corrupt=project(base,"forest-package-corrupt");const auto saved=publish_source(*corrupt);REQUIRE(saved);
 const auto source=read(saved.source.storage_path);auto bytes=read(saved.artifact.storage_path);bytes.back()^=1;write(saved.artifact.storage_path,bytes);
 add_builtin_project_package(*corrupt,context,*package);
 REQUIRE(corrupt->showPackageManagerModal&&!corrupt->plantLabDocumentInitialized);
 REQUIRE(!fs::exists(fs::path{corrupt->projectRoot}/"assets/packages/engine_forest_factory.package.json"));
 REQUIRE(read(saved.source.storage_path)==source&&read(saved.artifact.storage_path)==bytes);
 auto blank=std::make_unique<EditorState>();
 add_builtin_project_package(*blank,context,*package);
 REQUIRE(createCalls==1&&blank->projectRoot.empty()&&blank->packageInstallStatus=="Creation refused");
 REQUIRE(blank->showPackageManagerModal&&!blank->plantLabDocumentInitialized);
}
void allowed(const fs::path& base){
 {auto s=project(base,"missing-source");REQUIRE(ensure_plant_lab_compiled_asset(*s));REQUIRE(s->plantLabDocumentInitialized&&s->plantLabCompiled.valid);REQUIRE(!fs::exists(fs::path{s->projectRoot}/kDefaultPlantLabLogicalPath));}
 {auto s=std::make_unique<EditorState>();REQUIRE(ensure_plant_lab_compiled_asset(*s));REQUIRE(s->plantLabDocumentInitialized&&s->plantLabCompiled.valid);}
 {auto s=project(base,"initialized");s->plantLabDocumentInitialized=true;s->projectLifecycle.value.project_selected=false;REQUIRE(ensure_plant_lab_compiled_asset(*s));REQUIRE(s->plantLabDocument.profile.config.targetHeightMeters==7.25f);REQUIRE(!fs::exists(fs::path{s->projectRoot}/kDefaultPlantLabLogicalPath));}
 {
  auto s=project(base,"valid-reopen");const auto saved=publish_source(*s);REQUIRE(saved);
  const auto source=read(saved.source.storage_path),artifact=read(saved.artifact.storage_path);
  REQUIRE(ensure_plant_lab_compiled_asset(*s));REQUIRE(s->plantLabDocumentInitialized&&s->plantLabDocument.profile.config.targetHeightMeters==11.5f);
  REQUIRE(s->plantLabPublishedArtifact.artifact_key.words==saved.artifact.artifact_key.words);
  REQUIRE(read(saved.source.storage_path)==source&&read(saved.artifact.storage_path)==artifact);
 }
}
int run(const fs::path& base,const fs::path& fixture={}){
 evidence=base;if(fs::exists(base))return 2;fs::create_directories(base);
 try{errors(base);stale(base);manifest_identity(base);allowed(base);builtin_actions(base);compiler_upgrade(base);if(!fixture.empty())saved_project_upgrade(base,fixture);text(base/"result.txt","PASS: real helper/library reject corrupt or missing artifacts, invalid source/identity and dangling source without default seeding or source/scene writes; genuine absence, scratch, initialized, old-compiler rebuild and valid reopen succeed.\n");return 0;}
 catch(...){text(base/"exception.txt","Forest reopen safety exception.\n");return 255;}
}
}
int main(int argc,char**argv){return argc==2||argc==3?epochengine::run(std::filesystem::absolute(argv[1]),argc==3?std::filesystem::absolute(argv[2]):std::filesystem::path{}):1;}
'''.replace("FUNCTIONS","\n".join(bodies)).replace("PARSER",parser).replace("PACKAGE_BODIES",package_bodies)
        parent=ROOT/"build/contract-tests";parent.mkdir(parents=True,exist_ok=True)
        self.assertTrue(parent.resolve().is_relative_to(ROOT.resolve()))
        scratch=Path(tempfile.mkdtemp(prefix="forest-reopen-",dir=parent))
        src=scratch/"source";src.mkdir();build=scratch/"msvc"
        (src/"consumer.cpp").write_text(code,encoding="utf-8")
        env=os.environ.copy();tmp=scratch/"process-temp";tmp.mkdir()
        for key in ("TEMP","TMP","TMPDIR"):env[key]=str(tmp)
        engine=ROOT/"Engine"
        saved_input = None
        saved_project = ROOT / "Projects/GameProject01"
        saved_source = saved_project / "Assets/Forest/default.epoch_forest"
        if saved_source.is_file():
            saved_input = scratch / "saved-input"
            (saved_input / "Assets/Forest").mkdir(parents=True)
            shutil.copyfile(saved_project / "project.epoch.json", saved_input / "project.epoch.json")
            shutil.copyfile(saved_source, saved_input / "Assets/Forest/default.epoch_forest")
            for artifact in (saved_project / "Library/Forest").glob("*/*.epoch_forest_artifact"):
                target = saved_input / artifact.relative_to(saved_project)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(artifact, target)
        modules="\n".join('    "'+(engine/"modules"/(name+".ixx")).as_posix()+'"' for name in MODULES)
        (src/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
project(EpochForestReopenSafety LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(reopen_owners STATIC "{(engine/'src/project/project.forest_library.cpp').as_posix()}")
target_sources(reopen_owners PUBLIC FILE_SET modules TYPE CXX_MODULES BASE_DIRS "{(engine/'modules').as_posix()}" FILES
{modules})
target_include_directories(reopen_owners PUBLIC "{(engine/'include').as_posix()}" "{(engine/'dep/EpochPlatformEngine/include').as_posix()}")
target_compile_options(reopen_owners PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8)
add_executable(forest_reopen_safety consumer.cpp "{(engine/'resource/epoch.manifest').as_posix()}")
target_link_libraries(forest_reopen_safety PRIVATE reopen_owners)
''',encoding="utf-8")
        results={};saved_results={};success=False
        def checked(name,args,timeout=120):
            p=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,timeout=timeout,env=env)
            (scratch/(name+".log")).write_text(p.stdout+p.stderr,encoding="utf-8")
            self.assertEqual(p.returncode,0,f"{name}: {scratch}\n{p.stdout[-5000:]}{p.stderr[-5000:]}")
        try:
            checked("configure",[cmake,"-S",str(src),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64","-DCMAKE_TOOLCHAIN_FILE=","-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false"])
            for config in ("Debug","Release"):
                checked("build-"+config,[cmake,"--build",str(build),"--config",config,"--target","forest_reopen_safety","--parallel","1"])
                case=scratch/("case-"+config.lower())
                checked("run-"+config,[str(build/config/"forest_reopen_safety.exe"),str(case)]+([str(saved_input)] if saved_input else []),30)
                results[config]=(case/"result.txt").read_text(encoding="utf-8").strip()
                if saved_input:
                    saved_results[config]=(case/"saved-project-result.txt").read_text(encoding="utf-8").strip()
            record={"scope":"actual3Editorhelpers+existingProjectManifestParser/identityInspector+actualForestLibrary/filesystem; no graphics/models","configurations":results,"owner":str(owner.relative_to(ROOT)),"helper_sha256":{name:hashlib.sha256(body.encode()).hexdigest()for name,body in zip(markers,bodies)},"source_sha256":{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in [owner,engine/"src/editor/application/editor.application_packages_creation.inl",engine/"src/editor/editor.scene.cpp",engine/"modules/editor.core.ixx",engine/"src/editor/implementation/editor.project_registry.inl",engine/"src/project/project.forest_library.cpp",*[engine/"modules"/(name+".ixx")for name in MODULES]]}}
            record["saved_project_compiler_upgrade"]={"configurations":saved_results,"input_sha256":{str(p.relative_to(saved_input)):hashlib.sha256(p.read_bytes()).hexdigest()for p in saved_input.rglob("*")if p.is_file()}if saved_input else {}}
            (parent/"forest-reopen-safety-last.json").write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
            success=True
        finally:
            if success:
                resolved=scratch.resolve();self.assertEqual(resolved.parent,parent.resolve());self.assertTrue(resolved.is_relative_to(ROOT.resolve()));shutil.rmtree(resolved)
            else:print("Forest reopen safety failure evidence retained:",scratch)

if __name__=="__main__":unittest.main()

