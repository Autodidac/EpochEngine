"""Actual portable env owner and runtime readers; native CPU/files only."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from Tests.platform.test_child_close_router import definition

ROOT = Path(__file__).resolve().parents[2]


class RuntimeProjectEnvironment(unittest.TestCase):
    def test_native_binding_is_visible_after_crt_environment_was_cached(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC/Windows native environment consumer")
        tools = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))
        sdk = Path("C:/Program Files (x86)/Windows Kits/10")
        kits = sorted((sdk / "Include").glob("*"))
        if not tools or not kits:
            self.skipTest("MSVC/SDK unavailable")
        tool, kit = tools[-1], kits[-1]
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix="runtime-env-", dir=parent))
        try:
            registry = (ROOT / "Engine/src/editor/implementation/editor.project_registry.inl").read_text(encoding="utf-8")
            legacy = (ROOT / "Engine/src/epoch/epoch.engine_legacy.cpp").read_text(encoding="utf-8")
            readers = definition(registry, "    [[nodiscard]] static std::string read_env_var(")
            readers += definition(legacy, "    [[nodiscard]] inline std::string read_environment_string(")
            factory = definition(legacy, "        [[nodiscard]] std::unique_ptr<epochengine::scene::Scene> make_scene_from_id(std::string_view scene_id)\n        {")
            project_factory = factory.split('            if (scene_id == "snake")', 1)[0] + "return {};\n}\n"
            # Native renderer construction is substituted. The actual payload
            # parser and project-factory branch decide whether construction is
            # permitted; no GPU or window is needed to test a rejected ID.
            selection = r'''
namespace epochengine {
namespace previewgrid {enum class CameraMode{Editor,FPS,Canvas2D};}
struct EditorProjectProfile {std::string_view id;};
const EditorProjectProfile* editor_find_project_profile(std::string_view id){
    static const EditorProjectProfile bound{"bound"};return id=="bound"?&bound:nullptr;
}
namespace scene {struct Scene {virtual ~Scene()=default;};}
namespace logger {
enum class LogLevel {Error};inline unsigned errors{};
struct Log {void log(LogLevel,std::string,std::source_location){++errors;}};
Log& get(std::string_view){static Log value;return value;}
}
}
namespace input {enum class ProfilePreset{EditorDefault};
ProfilePreset profile_preset_from_id(std::string_view){return ProfilePreset::EditorDefault;}}
namespace logger=epochengine::logger;
constexpr std::string_view kEngineLog="contract";
struct ProjectPlayScene : epochengine::scene::Scene {
    static inline unsigned created{};static inline std::string payload{};
    explicit ProjectPlayScene(std::string_view value){++created;payload=value;}
};
'''
            selection += definition(legacy, "        struct ProjectSceneLaunchOptions", True)
            selection += definition(legacy, "        [[nodiscard]] epochengine::previewgrid::CameraMode camera_mode_from_argument(")
            selection += definition(legacy, "        [[nodiscard]] ProjectSceneLaunchOptions parse_project_scene_launch(")
            selection += project_factory
            source = scratch / "contract.cpp"
            source.write_text('''#include <cassert>
#include <cstdlib>
#include <string>
#include <string_view>
#include <memory>
#include <source_location>
import core.env;
''' + readers + selection + r'''
int main(){
    constexpr const char* key="EPOCH_RUNTIME_BINDING_NATIVE_ENV_CONTRACT_532874";
    const std::string_view name{key};
    assert(epochengine::core::env::unset({name.data(),name.size()}));
    char* cached{};std::size_t cachedSize{};
    assert(_dupenv_s(&cached,&cachedSize,key)==0);std::free(cached);
    for(const auto value : {std::string{"first project"},std::string{"copied_\xce\xa9_project"}}){
        assert(epochengine::core::env::set({name.data(),name.size()},{value.data(),value.size()}));
        // Windows' native update deliberately does not rewrite the CRT copy.
        cached=nullptr;cachedSize=0;
        assert(_dupenv_s(&cached,&cachedSize,key)==0);
        assert(cached==nullptr);std::free(cached);
        assert(read_env_var(name)==value);
        assert(read_environment_string(key)==value);
    }
    assert(epochengine::core::env::unset({name.data(),name.size()}));
    assert(read_env_var(name).empty());assert(read_environment_string(key).empty());
    assert(read_environment_string(nullptr).empty());assert(read_environment_string("").empty());
    assert(!make_scene_from_id("project:unadmitted|camera=editor"));
    assert(ProjectPlayScene::created==0u&&logger::errors==1u);
    assert(make_scene_from_id("project:bound|camera=fps"));
    assert(ProjectPlayScene::created==1u&&ProjectPlayScene::payload=="bound|camera=fps");
    assert(make_scene_from_id("project:")); // Established empty/default route remains available.
    assert(ProjectPlayScene::created==2u&&logger::errors==1u);
}
''', encoding="utf-8")
            compiler = tool / "bin/Hostx64/x64/cl.exe"
            common = [str(compiler), "/nologo", "/std:c++23preview", "/EHsc", "/utf-8", "/DNOMINMAX", "/DWIN32_LEAN_AND_MEAN",
                "/I" + str(tool / "include"), "/I" + str(kit / "ucrt"), "/I" + str(kit / "shared"), "/I" + str(kit / "um")]
            component = ROOT / "Engine/dep/EpochPlatformEngine"
            interface, implementation = scratch / "core.env.ifc", scratch / "core.env.obj"
            commands = [common + ["/c", "/interface", "/ifcOutput", str(interface), "/Fo" + str(scratch / "interface.obj"), str(component / "modules/core.env.ixx")],
                common + ["/c", "/reference", "core.env=" + str(interface), "/Fo" + str(implementation), str(component / "src/core/core.env.cpp")],
                common + ["/reference", "core.env=" + str(interface), "/Fo" + str(scratch / "contract.obj"), "/Fe" + str(scratch / "contract.exe"), str(source),
                    str(implementation), str(scratch / "interface.obj"), "/link", "/LIBPATH:" + str(tool / "lib/x64"),
                    "/LIBPATH:" + str(sdk / "Lib" / kit.name / "um/x64"), "/LIBPATH:" + str(sdk / "Lib" / kit.name / "ucrt/x64")]]
            environment = os.environ.copy()
            environment["TEMP"] = environment["TMP"] = str(scratch)
            for command in commands:
                result = subprocess.run(command, cwd=scratch, env=environment, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-5000:] + result.stderr)
            result = subprocess.run([str(scratch / "contract.exe")], cwd=scratch, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent.resolve()))
            shutil.rmtree(scratch)
