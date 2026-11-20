"""Exercise the production immutable terrain and body receipt with a saved FPS edit."""

from pathlib import Path
import time
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run, dependency_include


ROOT = Path(__file__).resolve().parents[2]


class PlanetaryGenerationReceipt(unittest.TestCase):
    def test_run_fps_preference_keeps_current_receipt_and_legacy_remains_exact(self):
        owner = ROOT / "Engine/src/project/project.planetary.cpp"
        source = owner.read_text(encoding="utf-8-sig")
        receipt = source[source.index("        enum class MeshGenerationReceiptVersion"):
                         source.index("        std::optional<fs::path> generation_build_directory")]
        checks = source[source.index("    bool source_generation_is_current("):
                        source.index("    Result generate(")]
        assets = (ROOT / "Engine/src/assets/texture/asset.texture_artifact.cpp").read_text()
        stable_hash = assets[assets.index("        constexpr std::uint64_t kFnvPrime"):
                             assets.index("        [[nodiscard]] constexpr bool checked_add(")]
        hash_api = "\n".join(definition(assets, name) for name in (
            "    ContentHash deterministic_content_hash(", "    std::string content_hash_hex("))
        code = r'''
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <nlohmann/json.hpp>
namespace fs = std::filesystem;
namespace asset {
namespace mesh {
struct ContentHash {
    std::array<std::uint64_t,4> words{};
    bool empty() const { return words == std::array<std::uint64_t,4>{}; }
};
struct Artifact {};
}
namespace texture {
using ContentHash = mesh::ContentHash;
STABLE_HASH
HASH_API
}
}
struct SourceGeneration { fs::path project_root; std::string project_manifest, content_hash; };
bool source_generation_is_current(const fs::path&, const SourceGeneration&, std::stop_token = {});
struct Settings { int value{1}; bool operator==(const Settings&) const = default; };
struct MeshFrame { double meters_per_unit{637100}; std::array<double,3> source_origin_meters{}; };
struct MeshGenerationIdentity {
    std::array<std::uint64_t,4> artifact_key{{1,2,3,4}};
    unsigned vertex_count{5828}, index_count{18048};
    MeshFrame coordinates;
};
struct GeneratedBody { unsigned id{}; };
struct Limits { unsigned maximum_vertices{65536}, maximum_indices{65536}, maximum_bytes{4000000}; };
Limits artifact_limits(const Settings&) { return {}; }
std::string encode_settings(Settings settings) {
    return "epoch.planet.recipe.test\n" + std::to_string(settings.value) + "\n";
}
std::optional<Settings> decode_settings(std::string_view bytes) {
    if (bytes == encode_settings({1})) return Settings{1};
    if (bytes == encode_settings({2})) return Settings{2};
    return {};
}
std::optional<Settings> load_settings(const fs::path&) { return Settings{1}; }
std::string real_text(double value) { return std::to_string(value); }
std::optional<fs::path> project_directory(const fs::path& root, fs::path relative, bool create) {
    if (create) fs::create_directories(root / relative);
    if (!fs::is_directory(root / relative)) return {};
    return relative.empty() ? root : root / relative;
}
namespace platform::filesystem {
bool exclusive_create_and_write(const fs::path& file, std::span<const std::byte> bytes, std::error_code&) {
    if (fs::exists(file)) return false;
    std::ofstream out(file,std::ios::binary);
    return bool(out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
}
}
std::string attached;
namespace package_source {
struct Attachment { std::string content_hash; bool source_attached() const { return !content_hash.empty(); } };
Attachment load_project_attachment(const fs::path&, std::string_view, std::stop_token) { return {attached}; }
}
std::vector<GeneratedBody> admit_generated_body_inventory(
    std::string_view bytes, const Settings&, const MeshGenerationIdentity&, const asset::mesh::Artifact&) {
    return bytes == "admitted body" ? std::vector<GeneratedBody>{{42}} : std::vector<GeneratedBody>{};
}
READ_SMALL
RECEIPT_OWNER
ADMISSION_OWNER
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); return __LINE__; } } while(false)
void save_manifest(const SourceGeneration& source) {
    std::ofstream out(source.project_root / "project.epoch.json",std::ios::binary | std::ios::trunc);
    out << source.project_manifest;
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    const fs::path root{argv[1]};
    const auto directory = root / "Assets/Mesh";
    fs::create_directories(directory);
    attached = "sha256:" + std::string(64,'a');
    const SourceGeneration old{root,R"({"id":"Verdant","run_frame_limit_fps":60.0,"scene":"worlds/main.epoch"})",attached};
    const SourceGeneration uncapped{root,R"({"id":"Verdant","run_frame_limit_fps":0.0,"scene":"worlds/main.epoch"})",attached};
    const MeshGenerationIdentity mesh{};
    const Settings active{1}, differentRecipe{2};
    save_manifest(old);
    const auto legacy = mesh_generation_record(old,active,mesh,MeshGenerationReceiptVersion::exact_manifest);
    CHECK(legacy && legacy->bytes.starts_with("epoch.planet.mesh-generation.v1\n"));
    std::ofstream(directory / legacy->filename,std::ios::binary) << legacy->bytes;
    auto oldBody = fs::path{legacy->filename};
    oldBody.replace_extension(".epoch_pbod");
    std::ofstream(directory / oldBody,std::ios::binary) << "admitted body";
    CHECK(mesh_generation_is_current(root,old,active,mesh));
    CHECK(mesh_generation_bodies(root,old,mesh,{}).size() == 1);
    save_manifest(uncapped);
    CHECK(!source_generation_is_current(root,old,{}));
    CHECK(!mesh_generation_settings(root,uncapped,mesh));
    CHECK(mesh_generation_bodies(root,uncapped,mesh,{}).empty());
    CHECK(write_mesh_generation_record(root,old,active,mesh));
    CHECK(write_mesh_generation_bodies(root,old,active,mesh,"admitted body"));
    const auto v2 = mesh_generation_record(uncapped,active,mesh);
    CHECK(v2 && v2->bytes.starts_with("epoch.planet.mesh-generation.v2\n") && v2->filename != legacy->filename);
    CHECK(mesh_generation_is_current(root,uncapped,active,mesh));
    CHECK(mesh_generation_settings(root,uncapped,mesh) == active);
    auto bodies = mesh_generation_bodies(root,uncapped,mesh,{});
    CHECK(bodies.size() == 1 && bodies.front().id == 42);
    CHECK(!mesh_generation_is_current(root,old,active,mesh));  // Existing in-flight raw manifest CAS.
    auto wrongArtifact = mesh;
    ++wrongArtifact.artifact_key[0];
    CHECK(!mesh_generation_settings(root,uncapped,wrongArtifact));
    wrongArtifact = mesh;
    ++wrongArtifact.coordinates.source_origin_meters[0];
    CHECK(!mesh_generation_settings(root,uncapped,wrongArtifact));
    CHECK(write_mesh_generation_record(root,old,differentRecipe,wrongArtifact));
    CHECK(write_mesh_generation_bodies(root,old,differentRecipe,wrongArtifact,"admitted body"));
    CHECK(mesh_generation_settings(root,uncapped,wrongArtifact) == differentRecipe);
    CHECK(mesh_generation_bodies(root,uncapped,wrongArtifact,{}).size() == 1);
    auto edited = uncapped;
    edited.project_manifest = R"({"id":"Verdant","run_frame_limit_fps":0.0,"scene":"worlds/swapped.epoch"})";
    save_manifest(edited);
    CHECK(!mesh_generation_settings(root,edited,mesh));
    edited.project_manifest = R"({"id":"Verdant","run_frame_limit_fps":0.0,"run_frame_limit_fps":60.0})";
    CHECK(!mesh_generation_record(edited,active,mesh));
    edited.project_manifest = R"({"id":"Verdant","run_frame_limit_fps":"unlimited"})";
    CHECK(!mesh_generation_record(edited,active,mesh));
    save_manifest(uncapped);
    attached = "sha256:" + std::string(64,'b');
    CHECK(!mesh_generation_settings(root,uncapped,mesh));
    edited = uncapped;
    edited.content_hash = attached;
    CHECK(!mesh_generation_settings(root,edited,mesh));
}
'''
        code = (code.replace("STABLE_HASH", stable_hash).replace("HASH_API", hash_api)
                .replace("READ_SMALL", definition(source, "        std::string read_small("))
                .replace("RECEIPT_OWNER", receipt).replace("ADMISSION_OWNER", checks))
        report = ROOT / "Artifacts/Reports/planetary-fps-generation-receipt" / str(time.time_ns())
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / "consumer.cpp"
        fixture.write_text(code, encoding="utf-8")
        compile_and_run(self, fixture, report, includes=[dependency_include(ROOT)],
                        args=lambda configuration: (report / ("project-" + configuration),))


if __name__ == "__main__":
    unittest.main()
