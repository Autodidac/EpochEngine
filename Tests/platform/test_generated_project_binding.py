"""Exercise actual generated-project binding/parser functions on ordinary files.

Only native executable location and process environment are substituted. No
Editor, renderer, model, source build, network or project self-test is launched.
"""
from pathlib import Path
import os
import json
import shutil
import subprocess
import tempfile
import unittest
from Tools.source.owned_source_text import read_owned_source

from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class GeneratedProjectBinding(unittest.TestCase):
    def test_actual_default_entry_archive_accepts_a_consumer_main(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU consumer")
        tools = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = sorted((sdk / "Include").glob("*"))
        if not tools or not kits:
            self.skipTest("MSVC/SDK unavailable")
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix="project-entry-", dir=parent))
        try:
            tool, kit = tools[-1], kits[-1]
            compiler = tool / "bin/Hostx64/x64/cl.exe"
            include = ["/I" + str(tool / "include"), "/I" + str(kit / "ucrt"),
                "/I" + str(ROOT / "Engine/src/epoch")]
            libraries = ["/LIBPATH:" + str(tool / "lib/x64"),
                "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"),
                "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")]
            (scratch / "dispatch.cpp").write_text('#include "epoch.main.hpp"\n'
                'int epochengine::core::run_application_main(int,char**){return 17;}\n')
            result = subprocess.run([str(compiler), "/nologo", "/std:c++latest", "/c", *include,
                "/Fo" + str(scratch) + "\\", str(ROOT / "Engine/src/epoch/epoch.main.cpp"), str(scratch / "dispatch.cpp")],
                cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            archive = scratch / "runtime.lib"
            result = subprocess.run([str(tool / "bin/Hostx64/x64/lib.exe"), "/nologo", "/OUT:" + str(archive),
                str(scratch / "epoch.main.obj"), str(scratch / "dispatch.obj")], cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for name, contents in (("default", "int ordinary_consumer_symbol(){return 0;}\n"),
                    ("generated", '#include "epoch.main.hpp"\nint main(int a,char**v){return epochengine::core::run_application_main(a,v);}\n')):
                source, binary = scratch / (name + ".cpp"), scratch / (name + ".exe")
                source.write_text(contents)
                result = subprocess.run([str(compiler), "/nologo", "/std:c++latest", *include,
                    "/Fe" + str(binary), "/Fo" + str(scratch / (name + ".obj")), str(source), str(archive),
                    "/link", "/SUBSYSTEM:CONSOLE", "/ENTRY:mainCRTStartup", *libraries],
                    cwd=scratch, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True)
                self.assertEqual(result.returncode, 17, result.stdout + result.stderr)
            msbuild = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe")
            for configuration in ("Debug", "Release", "ReleaseWithDebugInfo"):
                result = subprocess.run([str(msbuild), str(ROOT / "Engine/examples/EpochEngine/EpochEngine.vcxproj"),
                    "/nologo", "/nr:false", "/p:Platform=x64", f"/p:Configuration={configuration}",
                    "/p:VcpkgManifestInstall=false", "/getItem:ClCompile"], cwd=scratch, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-3000:] + result.stderr)
                items = json.loads(result.stdout)["Items"]["ClCompile"]
                entry = [item for item in items if Path(item["Identity"]).name == "epoch.main.cpp"]
                self.assertEqual(len(entry), 1)
                self.assertNotEqual(entry[0].get("ExcludedFromBuild", "").lower(), "true")
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)

    def test_actual_binding_uses_executable_project_and_preserves_admission(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC CPU consumer")
        tools = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = sorted((sdk / "Include").glob("*"))
        if not tools or not kits:
            self.skipTest("MSVC/SDK unavailable")
        registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
        scene = (ROOT / "Engine/src/editor/editor.scene.cpp").read_text(encoding="utf-8")
        legacy = (ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp").read_text(encoding="utf-8")
        header = (ROOT / "Engine/include/epoch.engine.hpp").read_text(encoding="utf-8")
        shell = (ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl").read_text(encoding="utf-8")
        selected = "\n".join(definition(registry, marker, semicolon) for marker, semicolon in (
            ("    enum class JsonStringFieldState", True),
            ("    struct JsonStringFieldResult", True),
            ("    [[nodiscard]] static JsonStringFieldResult inspect_json_string_field(", False),
            ("    [[nodiscard]] static std::optional<std::string> extract_json_string_field(", False),
            ("    [[nodiscard]] static std::string default_world_name(", False),
            ("    [[nodiscard]] static std::optional<EditorProjectKind> parse_project_manifest_kind(", False),
            ("    [[nodiscard]] static std::string parse_world_name(", False),
            ("    [[nodiscard]] static std::optional<OwnedProjectProfile>\n        parse_manifest_project_profile(", False),
        ))
        inspected = definition(scene, "    EditorProjectAdmissionResult editor_inspect_runtime_project_manifest(")
        bound = definition(legacy, "    RuntimeProjectBinding BindRuntimeProject(")
        binding_type = definition(header, "    struct RuntimeProjectBinding", True)
        prefix = r'''
#include <array>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace fs=std::filesystem;
enum class EditorProjectKind {Game,Tool,Editor,EngineDevelopment};
struct CapabilityPolicy {};
namespace epochengine {
using string_view=std::string_view;
namespace updater::distribution {constexpr std::string_view edition_id="epoch.development.site.v1";constexpr bool public_edition=PUBLIC_MARKER;}
namespace core::path {
fs::path executableDirectory, candidateRoot;
fs::path executable_dir(){return executableDirectory;}
fs::path candidate_data_root(){return candidateRoot;}
fs::path engine_asset_dir(){return fs::current_path()/"immutable-engine-assets";}
}
namespace core::env {
std::map<std::string,std::string> values;
std::string failName;
std::optional<std::string> get(std::string_view key){auto i=values.find(std::string(key));return i==values.end()?std::nullopt:std::optional{i->second};}
bool set(std::string_view key,std::string_view value){if(key==failName)return false;values[std::string(key)]=value;return true;}
bool unset(std::string_view key){values.erase(std::string(key));return true;}
}
std::optional<CapabilityPolicy> editor_project_capability_policy(std::string_view value){return value=="portable"?std::optional{CapabilityPolicy{}}:std::nullopt;}
namespace project_input {constexpr std::string_view canonical_source_path="Assets/Config/input_profile.epochinput";}
namespace project_sprite_animation {constexpr std::string_view canonical_source_path="Assets/Animations/sprite_animations.epochanim";}
namespace project_audio {constexpr std::string_view canonical_source_path="Assets/Audio/project_audio.epochaudio";}
namespace project_gui {constexpr std::string_view canonical_source_path="Assets/Gui/main.epochgui";}
struct EditorProjectAdmissionResult {bool succeeded{};std::string project_id,manifest_path,summary;};
}
using epochengine::EditorProjectAdmissionResult;
struct OwnedProjectProfile {
EditorProjectKind kind{};
std::string id,display_name,root_path,scene_path,tilemap_path,input_profile_path,
sprite_animation_path,audio_profile_path,gui_path,world_name,runtime_scene_id,
manifest_path,template_family,default_script,engine_integration_mode,
public_include_root,demo_model_asset,description;
CapabilityPolicy renderer_capability{};
};
constexpr std::string_view kProjectManifestFormat="epoch-project-v1",kStaticRuntimeBuildProfile="epoch-runtime-static";
std::string read_text_file(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>{f},{}};}
void write(const fs::path&p,std::string_view text){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<text;assert(f);}
bool project_path_below(const fs::path& p,const fs::path& r){auto rel=p.lexically_relative(r);return !rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..";}
fs::path candidate_project_path(const fs::path& r,const fs::path& p){return project_path_below(p,r)?p:fs::path{};}
fs::path resolve_repo_relative_path(const fs::path&p,const fs::path&r){return p.is_absolute()?p:r/p;}
'''
        public = "#define EPOCH_SOURCE_EDITION_PUBLIC 1" in (ROOT / "Engine/include/epoch.edition.hpp").read_text()
        prefix = prefix.replace("PUBLIC_MARKER", str(public).lower())
        body = prefix + selected + "\nnamespace epochengine {\n" + inspected + "\nnamespace project {\n" + binding_type + bound + "\n}}\n"
        expression = shell.split("        const std::string entrySourceText =", 1)[1].split(
            "        const std::string sourceAdmissionCmake =", 1)[0]
        body += definition(registry, "    [[nodiscard]] static std::string cxx_escape(")
        body += definition(shell, "    [[nodiscard]] static std::string make_project_entry_run_text(")
        body += definition(shell, "    [[nodiscard]] static std::string generated_child_project_library_properties(")
        owned_shell = read_owned_source(ROOT / "Engine/src/editor/implementation/editor.project_shell_generation.inl")
        body += "\n#define EPOCH_EDITION_PUBLIC " + str(int(public)) + "\n"
        body += definition(owned_shell, "    [[nodiscard]] static std::string generated_child_project_link_dependencies(")
        body += definition(owned_shell, "    static void repair_generated_project_link_dependencies(")
        body += '''
std::string generated_entry(){
struct Spec{std::string project_id="copied",project_name="Copied",tilemap_path,
input_profile_path="Assets/Config/input_profile.epochinput",sprite_animation_path,
audio_profile_path,gui_path;EditorProjectKind kind=EditorProjectKind::Game;}spec;
const std::string selfTestTitle="Copied",versionTitle="Copied: ",integrationMode="static runtime";
const std::string entrySourceText=''' + expression + "\nreturn entrySourceText;}\n"
        body += r'''
std::string manifest(const fs::path& oldRoot,std::string_view edition="epoch.development.site.v1",std::string_view id="copied"){
return "{\"project_format\":\"epoch-project-v1\",\"build_profile\":\"epoch-runtime-static\",\"id\":\""+std::string(id)+"\",\"display_name\":\"Copied\",\"kind\":\"game\",\"engine_edition\":\""+std::string(edition)+"\",\"scene\":\""+(oldRoot/"worlds/main.epoch").generic_string()+"\",\"entry_source\":\""+(oldRoot/"source/epoch.main.cpp").generic_string()+"\"}";
}
int main(){
for (const bool debug : {false, true}) {
    const auto complete=generated_child_project_link_dependencies(debug);
    for (const auto* name : {"EpochPlatformEngine.lib;", "EpochAudioEngine.lib;", "EpochMediaEngine.lib;"})
        assert(complete.find(name)!=std::string::npos);
    assert((complete.find("EpochSelfCoding.lib;")!=std::string::npos)==!EPOCH_EDITION_PUBLIC);
    auto prior=complete;
    for (const auto* name : {"EpochPlatformEngine.lib;", "EpochAudioEngine.lib;", "EpochMediaEngine.lib;", "EpochSelfCoding.lib;"})
        if (const auto at=prior.find(name);at!=std::string::npos) prior.erase(at,std::string_view{name}.size());
    const auto wrap=[debug](const std::string& list){return std::string{"<ItemDefinitionGroup Condition=\""}+(debug?"Debug|x64":"Release|x64")+"\"><Link><AdditionalDependencies>"+list+"</AdditionalDependencies></Link></ItemDefinitionGroup>";};
    auto managed=wrap(prior);repair_generated_project_link_dependencies(managed);assert(managed==wrap(complete));
    auto custom=wrap("consumer.lib;"+prior);const auto before=custom;repair_generated_project_link_dependencies(custom);assert(custom==before);
}
const auto base=fs::current_path()/"files";const auto old=base/"original";const auto copied=base/"copied";
write(old/"project.epoch.json",manifest(old));write(old/"worlds/main.epoch","scene \"Copied\"\n");
fs::rename(old,copied);assert(!fs::exists(old));
auto& executable=epochengine::core::path::executableDirectory;
executable=copied/"build/bin/windows/Debug/x64";fs::create_directories(executable);
auto& environment=epochengine::core::env::values;
environment["EPOCH_EDITOR_PROJECT_ROOT"]=old.generic_string();
auto binding=epochengine::project::BindRuntimeProject("copied");
assert(binding.succeeded&&fs::path(binding.project_root)==fs::canonical(copied));
assert(fs::path(binding.scene_path)==fs::canonical(copied/"worlds/main.epoch"));
assert(environment["EPOCH_EDITOR_PROJECT_ROOT"]==copied.generic_string());
// Wrong identity/edition cannot bind, and failed startup leaves ambient state unchanged.
const auto validEnvironment=environment;
assert(!epochengine::project::BindRuntimeProject("wrong").succeeded);assert(environment==validEnvironment);
write(copied/"project.epoch.json",manifest(old,"epoch.public.github.v1"));
assert(!epochengine::project::BindRuntimeProject("copied").succeeded);assert(environment==validEnvironment);
// Legacy Windows metadata also rebases after transfer to another host path.
write(copied/"project.epoch.json",manifest(fs::path{"C:/old-windows-project"}));
assert(epochengine::project::BindRuntimeProject("copied").succeeded);
// A scene outside the declared project owner cannot be smuggled in.
write(base/"outside/worlds/main.epoch","outside\n");
auto external=manifest(base/"outside");auto entry=std::string{"\"entry_source\":\""}+(base/"outside/source/epoch.main.cpp").generic_string()+"\"";
external.replace(external.find(entry),entry.size(),"\"entry_source\":\"source/epoch.main.cpp\"");
write(copied/"project.epoch.json",external);
assert(!epochengine::project::BindRuntimeProject("copied").succeeded);
write(copied/"project.epoch.json",manifest(old));
environment["EPOCH_EDITOR_PROJECT_ID"]="previous-id";
environment["EPOCH_EDITOR_PROJECT_ROOT"]="previous-root";
const auto beforeFailedSet=environment;
epochengine::core::env::failName="EPOCH_EDITOR_PROJECT_MANIFEST";
assert(!epochengine::project::BindRuntimeProject("copied").succeeded);assert(environment==beforeFailedSet);
epochengine::core::env::failName.clear();
// A closer foreign project is rejected; binding never skips it for an outer match.
write(executable/"project.epoch.json",manifest(old,"epoch.development.site.v1","foreign"));
assert(!epochengine::project::BindRuntimeProject("copied").succeeded);
fs::remove(executable/"project.epoch.json");
fs::create_symlink(copied/"project.epoch.json",executable/"project.epoch.json");
assert(!epochengine::project::BindRuntimeProject("copied").succeeded);
write(fs::current_path()/"generated.cpp",generated_entry());
for (const auto edition : {"development", "public"}) {
    const auto defaultPath=std::string{"$(EpochRepoRoot)build\\bin\\windows\\"}+edition+"\\$(Platform)\\$(Configuration)\\";
    write(fs::current_path()/(std::string{"library-"}+edition+".xml"),
        generated_child_project_library_properties(defaultPath,edition,true));
}
write(fs::current_path()/"library-legacy.xml",
    generated_child_project_library_properties("$(EpochRepoRoot)x64\\$(Configuration)\\","development",false));
}
'''
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix="project-binding-", dir=parent))
        try:
            tool, kit = tools[-1], kits[-1]
            cpp, binary = scratch / "contract.cpp", scratch / "contract.exe"
            cpp.write_text(body, encoding="utf-8")
            result = subprocess.run([str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/EHsc",
                "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/Fo" + str(scratch / "contract.obj"),
                "/Fe" + str(binary), str(cpp), "/link", "/LIBPATH:" + str(tool / "lib/x64"),
                "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")],
                cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-5000:] + result.stderr)
            result = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            generated = scratch / "generated.cpp"
            self.assertIn('BindRuntimeProject("copied")', generated.read_text())
            result = subprocess.run([str(tool / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/EHsc", "/c",
                "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/I" + str(kit / "um"),
                "/I" + str(kit / "shared"), "/I" + str(ROOT / "Engine/include"),
                "/Fo" + str(scratch / "generated.obj"), str(generated)],
                cwd=scratch, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-5000:] + result.stderr)
            msbuild = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe")
            for edition in ("development", "public", "legacy"):
                properties = (scratch / f"library-{edition}.xml").read_text()
                project = scratch / f"library-{edition}.proj"
                project.write_text('<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><PropertyGroup>'
                    f'<EpochRepoRoot>{scratch}\\</EpochRepoRoot><Platform>x64</Platform>'
                    + properties + '</PropertyGroup></Project>', encoding="utf-8")
                for configuration in ("Debug", "Release"):
                    for override in (False, True):
                        command = [str(msbuild), str(project), "/nologo", "/nr:false",
                            f"/p:Configuration={configuration}", "/getProperty:EpochEngineLibraryDirectory"]
                        if override:
                            command.append(f"/p:EpochBuildRoot={scratch / 'accepted-engine'}")
                        result = subprocess.run(command, cwd=scratch, capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        expected = (scratch / "x64" / configuration if edition == "legacy" else
                            (scratch / ("accepted-engine" if override else "build") / "bin/windows" / edition / "x64" / configuration))
                        self.assertEqual(Path(result.stdout.strip()).resolve(), expected.resolve())
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
