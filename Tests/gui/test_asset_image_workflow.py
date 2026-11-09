"""Real project-file admission, PNG importer and Windows JPEG preview; no window."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/application/editor.application_asset_browsing.inl"


def fixture():
    owner = OWNER.read_text(encoding="utf-8")
    api = (ROOT / "Engine/modules/editor.project_textures.ixx").read_text(encoding="utf-8")
    types = "\n".join(definition(api, "    struct " + name + " final", True) for name in (
        "TexturePreview", "TextureThumbnailPolicy", "TextureThumbnail"))
    thumbnail = definition((ROOT / "Engine/src/editor/editor.project_textures.cpp").read_text(encoding="utf-8"),
                           "    std::optional<TextureThumbnail> make_texture_thumbnail(")
    helpers = "\n".join(definition(owner, marker, marker.endswith("Result") or marker.endswith("Entry")) for marker in (
        "        struct EditorBrowserEntry", "        [[nodiscard]] static bool should_skip_browser_directory(",
        "        [[nodiscard]] static std::string relative_browser_path(",
        "        [[nodiscard]] static std::string browser_kind_for_path(",
        "        [[nodiscard]] static EditorBrowserEntry make_browser_entry(",
        "        [[nodiscard]] static bool project_texture_source_path(",
        "        [[nodiscard]] static bool project_image_preview_path(",
        "        struct ProjectImageSourceResult", "        [[nodiscard]] ProjectImageSourceResult add_project_image_source(",
        "        [[nodiscard]] static bool asset_category_visible(",
        "        [[nodiscard]] static std::vector<EditorBrowserEntry>\n            collect_asset_manager_entries(",
        "        [[nodiscard]] std::filesystem::path normalized_project_asset_path(",
        "        [[nodiscard]] std::uint64_t texture_source_surface_key(",
        "        [[nodiscard]] const ProjectTextureThumbnailState*\n            project_source_texture_thumbnail("))
    helpers = helpers.replace("asset::texture::import_texture_file(", "asset::texture::counted_import_texture_file(")
    state = definition((ROOT / "Engine/src/editor/application/editor.application_state.inl").read_text(encoding="utf-8"),
                       "        struct ProjectTextureThumbnailState", True)
    display = definition((ROOT / "Engine/src/editor/application/editor.application_code_workspaces.inl").read_text(encoding="utf-8"),
                         "        [[nodiscard]] std::string display_project_path(")
    controller = (ROOT / "Engine/src/editor/editor.project_textures.cpp").read_text(encoding="utf-8")
    paths = "\n".join(definition(controller, marker) for marker in (
        "        [[nodiscard]] bool path_component_equal(",
        "        [[nodiscard]] bool path_is_within(",
        "        [[nodiscard]] std::optional<std::string> logical_source_path("))
    view = (ROOT / "Engine/src/editor/application/editor.frame_viewport.inl").read_text(encoding="utf-8")
    grid_kind = definition(view, "                const auto gridKind =", True)
    presentation = definition(view, "                struct AssetGridPresentation", True)
    grid_loop = view[view.index("                std::vector<AssetGridPresentation> presentations"):
                     view.index("                std::vector<gui::AssetGridItem> gridItems")]
    png_owner = (ROOT / "Engine/src/assets/texture/asset.texture_import.cpp").read_text(encoding="utf-8")
    png = re.search(r"constexpr std::array<unsigned char, 80> pngSource\{(.*?)\};", png_owner, re.S).group(1)
    import_action = definition(owner, "        void import_selected_project_texture(")
    # This port supplies the active renderer profile only; the source capture,
    # admission, immutable artifacts, catalog and selection are real owners.
    import_action = import_action.replace("core::Context", "ContextPort").replace("editor_project_textures::", "epochengine::editor_project_textures::")
    import_action = re.sub(r"            const RendererBackendKind backend = renderer_backend_kind\(ctx\);\s*const capability::SubsystemProfile renderer =\s*capability::renderer_subsystem_profile_for\(backend\);",
                           "            (void)ctx; const capability::SubsystemProfile renderer = testRenderer;", import_action)
    renderer = definition(controller, "        [[nodiscard]] constexpr capability::SubsystemProfile\n            contract_renderer()")
    renderer = renderer.replace("contract_renderer()", "test_renderer()")
    return r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>
import asset.texture_artifact;
import asset.texture_import;
import core.sha256;
import editor.project_textures;
import capability.profile;
import platform.budgets;
import project.texture_library;
import project.texture_admission;
import project.texture_pipeline;
import project.texture_source;
import platform.filesystem;
namespace fs=std::filesystem;namespace asset=epochengine::asset;namespace core=epochengine::core;
namespace capability=epochengine::capability;namespace project_textures=epochengine::project_textures;
namespace project_texture_sources=epochengine::project_texture_sources;
using epochengine::Budgets;
namespace platform{namespace filesystem{
using epochengine::platform::filesystem::exclusive_create_and_write;
using epochengine::platform::filesystem::atomic_publish_new_same_filesystem;
unsigned jpegDecodes{};
auto image_thumbnail(std::string_view bytes){++jpegDecodes;return epochengine::platform::filesystem::image_thumbnail(bytes);}
}}
namespace epochengine::asset::texture{unsigned pngDecodes{};
auto counted_import_texture_file(const TextureImportRequest& request){++pngDecodes;return import_texture_file(request);}}
struct SpriteHandle{int value{};bool is_valid()const{return value!=0;}};
namespace gui{
struct Vec2{float x{},y{};};
enum class AssetGridItemKind{Generic,Folder,Image,Texture,Model,Audio,Scene,Document};
struct RuntimeSurfaceDescriptor{std::string_view id;std::span<const std::uint8_t> rgba_pixels;std::uint32_t width{},height{};};
struct RuntimeSurfaceAtlasResult{std::vector<SpriteHandle> handles;explicit operator bool()const{return !handles.empty();}};
std::vector<std::uint8_t> lastPixels;unsigned uploads{};
RuntimeSurfaceAtlasResult register_runtime_surface_atlas(std::span<const RuntimeSurfaceDescriptor> sources){
 ++uploads;std::vector<SpriteHandle> handles;
 for(const auto& source:sources){lastPixels.assign(source.rgba_pixels.begin(),source.rgba_pixels.end());handles.push_back({int(uploads)});}
 return {std::move(handles)};
}}
namespace editor_project_textures{
''' + types + "\n" + thumbnail.replace("TextureThumbnailPolicy policy)", "TextureThumbnailPolicy policy = {})") + r'''
}
''' + state + r'''
struct EditorState{std::string projectRoot,projectId{"test"};std::size_t assetCategory=1;std::unordered_map<std::string,ProjectTextureThumbnailState>projectSourceTextureThumbnails;
 std::string projectTextureStatus,selectedProjectFile,selectedAssetPath;
 std::unique_ptr<epochengine::editor_project_textures::ProjectTextureController>projectTextures;};
fs::path resolve_editor_path(const fs::path& value){return fs::absolute(value).lexically_normal();}
''' + display + r'''
namespace epochengine{template<class...Args>std::string format_text(std::format_string<Args...>format,Args&&...args){return std::format(format,std::forward<Args>(args)...);}}
void check(bool good,int line){if(!good){std::fprintf(stderr,"Image workflow failed line %d\n",line);std::exit(86);}}
#define CHECK(value) check(static_cast<bool>(value),__LINE__)
''' + helpers + "\nnamespace controller_paths {\n" + paths + "\n}\n" + renderer + r'''
auto testRenderer=test_renderer();struct ContextPort{};
auto* project_texture_controller(EditorState& editor){return editor.projectTextures.get();}
void push_editor_log(EditorState&,const std::string&){}
''' + import_action + r'''
struct TexturePort{std::string logical_path;};struct ControllerPort{};
const ProjectTextureThumbnailState* project_texture_thumbnail_state(EditorState&,ControllerPort&,const TexturePort&){CHECK(false);return nullptr;}
std::uint64_t asset_graph_id(std::string_view value){return std::hash<std::string_view>{}(value);}
auto present(EditorState& editor,const std::vector<EditorBrowserEntry>& assetEntries){
 ControllerPort* textureController=nullptr;
 const auto textureForEntry=[](const EditorBrowserEntry&)->const TexturePort*{return nullptr;};
''' + grid_kind + presentation + grid_loop + r'''
 return presentations;
}
void write(const fs::path& path,std::string_view bytes){fs::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file.write(bytes.data(),std::streamsize(bytes.size()));CHECK(bool(file));}
std::string read(const fs::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};}
int main(int argc,char** argv){
 CHECK(argc==2);const fs::path base=fs::path{argv[1]},project=base/fs::path{u8"project_树"},external=base/"external";
 const auto stage=[&](std::string_view value){std::ofstream file(base/"stage.txt",std::ios::app);file<<value<<'\n';file.flush();};stage("start");
 try {
 fs::create_directories(project);fs::create_directories(external);
 constexpr std::array<unsigned char,80> png{''' + png + r'''};
 const std::string bytes(reinterpret_cast<const char*>(png.data()),png.size());
 const auto original=external/"reference.PNG";write(original,bytes);
 CHECK(project_texture_source_path(display_project_path(original))&&project_image_preview_path(display_project_path(original)));
 CHECK(!project_texture_source_path("photo.jpg")&&project_image_preview_path("photo.JPEG"));
 CHECK(!project_image_preview_path("document.epoch_texture"));
 stage("add PNG");const auto added=add_project_image_source(project,original);CHECK(added&&added.texture_source);stage("PNG added");
 CHECK(added.path==fs::canonical(project)/"Assets/Images/reference.PNG"&&read(added.path)==bytes&&read(original)==bytes);
 CHECK(asset::texture::import_texture_file({.source_path=added.path}));
 CHECK(!add_project_image_source(project,original)&&read(added.path)==bytes);
 write(external/"reference.PNG","bad new data");CHECK(!add_project_image_source(project,external/"reference.PNG")&&read(added.path)==bytes);
 stage("add JPEG");const auto jpeg=add_project_image_source(project,external/"photo.jpg");CHECK(jpeg&&!jpeg.texture_source);stage("JPEG added");
 CHECK(read(jpeg.path)==read(external/"photo.jpg"));
 CHECK(!asset::texture::import_texture_file({.source_path=jpeg.path}));
 write(external/"broken.png","not an image");CHECK(!add_project_image_source(project,external/"broken.png")&&!fs::exists(project/"Assets/Images/broken.png"));
 write(external/"broken.jpg","not an image");CHECK(!add_project_image_source(project,external/"broken.jpg"));
 write(external/"notes.txt","not an image");CHECK(!add_project_image_source(project,external/"notes.txt"));
 CHECK(!add_project_image_source(base/"missing",original));
 CHECK(!add_project_image_source(project,external));
 // The fixture's junction points outside this project; do not follow it.
 CHECK(!add_project_image_source(base/"linked-project",external/"photo.jpg"));
 CHECK(!fs::exists(base/"outside/Images/photo.jpg"));
 const auto unicode=external/fs::path{u8"café_树.png"};write(unicode,bytes);
 stage("add Unicode");const auto unicodeAdded=add_project_image_source(project,unicode);CHECK(unicodeAdded&&read(unicodeAdded.path)==bytes);stage("Unicode added");
 CHECK(controller_paths::logical_source_path(unicodeAdded.path,project/"Assets")==std::optional<std::string>{"Assets/Images/café_树.png"});
 // Real scanner and category filter see PNG and JPEG in the chosen folder,
 // reject an outside folder and omit the current generated-output root.
 fs::create_directories(project/"Artifacts/Runtime");write(project/"Artifacts/Runtime/junk.png",bytes);
 write(project/fs::path{u8"Assets/Images/metadata.树"},"ordinary unknown file type");
 stage("scan");auto entries=collect_asset_manager_entries(display_project_path(project),"Assets/Images",512);stage("scanned");
 unsigned imageCount{};for(const auto& entry:entries)if(entry.kind=="IMAGE"){CHECK(asset_category_visible(1,entry.kind));++imageCount;}
 CHECK(imageCount==3&&collect_asset_manager_entries(display_project_path(project),"../../external").empty());
 for(const auto& entry:collect_asset_manager_entries(display_project_path(project),""))CHECK(fs::path(entry.path).filename()!="Artifacts");
 EditorState editor{display_project_path(project)};
 stage("preview PNG");const auto preview=project_source_texture_thumbnail(editor,display_project_path(added.path));stage("PNG previewed");
 CHECK(preview&&preview->sprite.is_valid()&&preview->source_width==2&&preview->source_height==2);
 CHECK(gui::lastPixels.size()==16&&gui::lastPixels[0]==255&&gui::lastPixels[1]==0);
 const auto pngCalls=asset::texture::pngDecodes,uploads=gui::uploads;
 CHECK(project_source_texture_thumbnail(editor,display_project_path(added.path))&&asset::texture::pngDecodes==pngCalls&&gui::uploads==uploads);
 stage("preview JPEG");const auto photo=project_source_texture_thumbnail(editor,display_project_path(jpeg.path));stage("JPEG previewed");
 CHECK(photo&&photo->source_width==400&&photo->source_height==200&&photo->width==96&&photo->height==48);
 CHECK(gui::lastPixels.size()==96*48*4&&gui::lastPixels[0]>200&&gui::lastPixels[1]<40);
 const auto jpegCalls=platform::filesystem::jpegDecodes;
 CHECK(project_source_texture_thumbnail(editor,display_project_path(jpeg.path))&&platform::filesystem::jpegDecodes==jpegCalls);
 // A failed decode is cached, but changed source metadata enables a retry.
 const auto invalid=project/"Assets/Images/bad.jpg";write(invalid,"bad");
 CHECK(!project_source_texture_thumbnail(editor,display_project_path(invalid)));
 const auto failures=platform::filesystem::jpegDecodes;
 for(int n=0;n<5;++n)CHECK(!project_source_texture_thumbnail(editor,display_project_path(invalid)));
 CHECK(platform::filesystem::jpegDecodes==failures);
 write(invalid,read(jpeg.path));CHECK(project_source_texture_thumbnail(editor,display_project_path(invalid)));
 CHECK(platform::filesystem::jpegDecodes==failures+1);
 const auto badPng=project/"Assets/Images/bad.png";write(badPng,"bad");
 CHECK(!project_source_texture_thumbnail(editor,display_project_path(badPng)));const auto badCalls=asset::texture::pngDecodes;
 CHECK(!project_source_texture_thumbnail(editor,display_project_path(badPng))&&asset::texture::pngDecodes==badCalls);
 stage("preview Unicode");CHECK(project_source_texture_thumbnail(editor,display_project_path(unicodeAdded.path)));stage("Unicode previewed");
 stage("actual grid presentation");entries=collect_asset_manager_entries(display_project_path(project),"Assets/Images",512);
 const auto presentations=present(editor,entries);unsigned previewCount{};
 for(const auto& p:presentations)if(p.kind==gui::AssetGridItemKind::Image){if(p.image.is_valid())++previewCount;}
 CHECK(previewCount>=4);stage("grid presented");
 for(const auto& entry:fs::directory_iterator(project/"Assets/Images"))CHECK(!entry.path().filename().generic_u8string().starts_with(u8".epoch-image-import-"));
 // Repeated native jobs have the same output filename. The actual import
 // helper uses their verified bytes, never that shared filename, as identity.
 stage("generated identity import");const auto generatedProject=base/"generated-project";
 fs::create_directories(generatedProject);EditorState generated{display_project_path(generatedProject)};
 generated.projectTextures=std::make_unique<epochengine::editor_project_textures::ProjectTextureController>(generated.projectId,generatedProject);
 CHECK(generated.projectTextures->valid());
 const auto first=external/"job-one/generated.png",second=external/"job-two/generated.png";
 write(first,bytes);const auto firstSha=core::sha256::hex(core::sha256::hash(std::string_view{bytes}));
 const std::string different=read(external/"generated-two.png");write(second,different);
 const auto secondSha=core::sha256::hex(core::sha256::hash(std::string_view{different}));
 const auto firstAdded=add_project_image_source(generatedProject,first,std::string_view{firstSha});
 const auto secondAdded=add_project_image_source(generatedProject,second,std::string_view{secondSha});
 stage(firstAdded.diagnostic);stage(secondAdded.diagnostic);
 CHECK(firstAdded&&secondAdded&&firstAdded.path!=secondAdded.path&&!firstAdded.generated_texture_key.empty()&&!secondAdded.generated_texture_key.empty());
 CHECK(firstAdded.path.filename()=="generated-"+firstSha+".png"&&secondAdded.path.filename()=="generated-"+secondSha+".png");
 CHECK(read(firstAdded.path)==bytes&&read(secondAdded.path)==different&&!fs::exists(generatedProject/"Assets/Images/generated.png"));
 generated.selectedProjectFile=display_project_path(firstAdded.path);import_selected_project_texture(generated,{},firstAdded.generated_texture_key);
 stage(generated.projectTextureStatus);
 CHECK(generated.projectTextures->catalog().size()==1u&&generated.projectTextures->selected());
 const auto originalEntry=*generated.projectTextures->selected();
 generated.selectedProjectFile=display_project_path(secondAdded.path);import_selected_project_texture(generated,{},secondAdded.generated_texture_key);
 CHECK(generated.projectTextures->catalog().size()==2u&&generated.projectTextures->selected()->artifact_key!=originalEntry.artifact_key);
 const auto repeated=add_project_image_source(generatedProject,first,std::string_view{firstSha});
 CHECK(repeated&&repeated.path==firstAdded.path&&repeated.generated_texture_key==originalEntry.artifact_key);
 const auto uppercase=external/"chosen-reference.PNG";write(uppercase,bytes);
 CHECK(add_project_image_source(generatedProject,uppercase,std::string_view{firstSha}).path==repeated.path);
 generated.selectedProjectFile=display_project_path(repeated.path);import_selected_project_texture(generated,{},repeated.generated_texture_key);
 CHECK(generated.projectTextureStatus.starts_with("Reused the verified")&&generated.projectTextures->catalog().size()==2u);
 CHECK(generated.projectTextures->selected()->source_sequence==originalEntry.source_sequence&&generated.projectTextures->selected()->artifact_key==originalEntry.artifact_key);
 const auto retainedBytes=read(firstAdded.path);write(firstAdded.path,different);
 CHECK(!add_project_image_source(generatedProject,first,std::string_view{firstSha})&&read(firstAdded.path)==different);
 write(firstAdded.path,retainedBytes);
 // Missing/invalid/stale receipts and other extensions cannot enter the
 // generated path, even when they are otherwise valid ordinary imports.
 CHECK(!add_project_image_source(generatedProject,first,std::string_view{})&&read(firstAdded.path)==bytes);
 CHECK(!add_project_image_source(generatedProject,first,std::string_view{secondSha}));
 CHECK(!add_project_image_source(generatedProject,first,std::string_view{std::string(64,'g')}));
 CHECK(!add_project_image_source(generatedProject,external/"photo.jpg",std::string_view{firstSha}));
 const auto invalidGenerated=external/"bad-job/generated.png";write(invalidGenerated,"invalid PNG");
 const auto invalidSha=core::sha256::hex(core::sha256::hash(std::string_view{"invalid PNG"}));
 CHECK(!add_project_image_source(generatedProject,invalidGenerated,std::string_view{invalidSha}));
 CHECK(!fs::exists(generatedProject/"Assets/Images"/("generated-"+invalidSha+".png")));
 // Reopen restores the existing immutable entries; repeating the same import
 // selects the exact artifact without rewriting its revision or source file.
 generated.projectTextures.reset();generated.projectTextures=std::make_unique<epochengine::editor_project_textures::ProjectTextureController>(generated.projectId,generatedProject);
 CHECK(generated.projectTextures->catalog().size()==2u);
 import_selected_project_texture(generated,{},repeated.generated_texture_key);
 CHECK(generated.projectTextureStatus.starts_with("Reused the verified")&&generated.projectTextures->selected()->source_sequence==originalEntry.source_sequence);
 // Reuse still runs real current-renderer admission and refuses an inadequate
 // texture budget without silently selecting a different artifact.
 testRenderer.capability.recommended_budgets.vram_budget_bytes=1u;
 import_selected_project_texture(generated,{},repeated.generated_texture_key);
 CHECK(generated.projectTextureStatus.find("could not be restored or admitted")!=std::string::npos);
 for(const auto& entry:fs::directory_iterator(generatedProject/"Assets/Images"))CHECK(!entry.path().filename().generic_u8string().starts_with(u8".epoch-image-import-"));
 stage("complete");
 } catch(const std::exception& error) {stage(error.what());std::_Exit(87);}
}
'''


class AssetImageWorkflow(unittest.TestCase):
    def test_real_image_admission_preview_and_scan_debug_release(self):
        if os.name != "nt":
            self.skipTest("Windows WIC JPEG preview and MSVC CPU lane")
        proof = ROOT / "Artifacts/Reports/asset-image-workflow-oct28" / str(time.time_ns())
        build = ROOT / "Artifacts/Intermediate/tests/asset-image-workflow" / proof.name
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "editor.asset_image_contract.cpp"
        source.write_text(fixture(), encoding="utf-8")
        implementations = [ROOT / path for path in (
            "Engine/src/assets/texture/asset.texture_artifact.cpp", "Engine/src/assets/texture/asset.texture_import.cpp", "Engine/src/assets/image.stb.cpp",
            "Engine/src/editor/editor.project_textures.cpp", "Engine/src/project/project.texture_library.cpp",
            "Engine/src/project/project.texture_pipeline.cpp", "Engine/src/project/project.texture_resources.cpp",
            "Engine/src/project/project.texture_source.cpp", "Engine/src/project/project.asset_registry.cpp",
            "Engine/src/authoring/texture/authoring.texture_document.cpp", "Engine/src/authoring/texture/authoring.texture_residency.cpp",
            "Engine/src/renderers/canvas2d/render.canvas2d_scene.cpp", "Engine/dep/EpochPlatformEngine/src/core/core.env.cpp",
            "Engine/src/platform/core.path.cpp", "Engine/dep/EpochPlatformEngine/src/core/core.log.cpp",
            "Engine/dep/EpochPlatformEngine/src/core/core.time.cpp")]
        roots = [ROOT / "Engine/modules", ROOT / "Engine/dep/EpochPlatformEngine/modules"]
        available = {}
        for folder in roots:
            for path in folder.glob("*.ixx"):
                match = re.search(r"^export module\s+([\w.:]+);", path.read_text(encoding="utf-8-sig"), re.M)
                if match:
                    available[match.group(1)] = path
        pending = re.findall(r"^import\s+([\w.]+);", source.read_text(encoding="utf-8"), re.M)
        for path in implementations:
            pending += re.findall(r"^(?:export )?(?:import|module)\s+([\w.]+);", path.read_text(encoding="utf-8-sig"), re.M)
        modules = {}
        while pending:
            name = pending.pop()
            if name in modules or name in ("std", "std.compat"):
                continue
            self.assertIn(name, available)
            modules[name] = available[name]
            pending += re.findall(r"^(?:export )?import\s+([\w.]+);", available[name].read_text(encoding="utf-8-sig"), re.M)
        cmake = f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(AssetImageWorkflow LANGUAGES CXX)
add_executable(asset_image "{source.as_posix()}" {" ".join('"'+p.as_posix()+'"' for p in implementations)})
target_compile_features(asset_image PRIVATE cxx_std_23)
target_compile_options(asset_image PRIVATE /W3 /WX /utf-8)
target_include_directories(asset_image PRIVATE "{(ROOT/'Engine/include').as_posix()}" "{(ROOT/'Engine/dep/EpochPlatformEngine/include').as_posix()}")
target_link_libraries(asset_image PRIVATE shell32)
target_sources(asset_image PRIVATE FILE_SET CXX_MODULES BASE_DIRS {" ".join('"'+p.as_posix()+'"' for p in roots)} FILES {" ".join('"'+p.as_posix()+'"' for p in modules.values())})
'''
        (proof / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
        receipts = []
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            data = ROOT / "Artifacts/Runtime/TestData" / ("img" + f"{time.time_ns():x}"[-8:]) / config
            external, outside, linked = data / "external", data / "outside", data / "linked-project"
            external.mkdir(parents=True); outside.mkdir(); linked.mkdir()
            path = str(external / "photo.jpg").replace("'", "''")
            png_path = str(external / "generated-two.png").replace("'", "''")
            junction = str(linked / "Assets").replace("'", "''")
            target = str(outside).replace("'", "''")
            setup = subprocess.run(["powershell", "-NoProfile", "-Command",
                f"Add-Type -AssemblyName System.Drawing; $image=New-Object System.Drawing.Bitmap 400,200; $paint=[System.Drawing.Graphics]::FromImage($image); $paint.Clear([System.Drawing.Color]::Red); $image.Save('{path}',[System.Drawing.Imaging.ImageFormat]::Jpeg); $image.Save('{png_path}',[System.Drawing.Imaging.ImageFormat]::Png); $paint.Dispose(); $image.Dispose(); New-Item -ItemType Junction -Path '{junction}' -Target '{target}' | Out-Null"],
                cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(setup.returncode, 0, setup.stdout + setup.stderr)
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                         [str(build / config / "asset_image.exe"), str(data)]]
        temporary = proof / "tmp"
        temporary.mkdir()
        env = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), EPOCH_ASSET_ROOT=str(ROOT / "Engine/resource"))
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=20 if command[0].endswith(".exe") else 150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            receipts.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-7000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "passed": True, "nativeAcceptance": False, "commands": receipts,
            "ownerSha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(),
            "scope": "Actual source add/scan/filter/preview/import helpers and ProjectTextureController admission/catalog/immutable artifact/restore owners. Generated PNG receipt hashes, unique content names, exact repeat reuse, differing-file refusal, invalid receipts, reopen and current budget refusal are exercised. GUI atlas and active-renderer profile are recording ports; no GPU/window/model.",
            "jpegPortability": "Windows WIC proven only; non-Windows image_thumbnail returns unavailable. No JPEG full texture import.",
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
