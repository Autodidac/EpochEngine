// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>
import asset.texture_import;
import asset.texture_artifact;

namespace texture = epochengine::asset::texture;
namespace fs = std::filesystem;
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
void put(const fs::path& path, std::span<const std::byte> bytes)
{
    std::ofstream output{path, std::ios::binary};
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
bool same(const texture::TextureImportResult& a, const texture::TextureImportResult& b)
{
    if (a.status != b.status || a.width != b.width || a.height != b.height
        || a.source_bytes != b.source_bytes || a.decoded_bytes != b.decoded_bytes)
        return false;
    if (!a) return !b;
    const auto first = texture::serialize_compiled_artifact(a.artifact);
    const auto second = texture::serialize_compiled_artifact(b.artifact);
    return first && second && first.bytes == second.bytes;
}
int main(int argc, char** argv)
{
    CHECK(argc == 2);
    const auto root = fs::absolute(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(root);
    constexpr std::array<unsigned char, 80> png{
        137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,23,73,68,65,84,120,156,99,248,207,192,240,159,225,63,67,3,3,152,254,255,223,1,0,63,24,7,186,222,66,110,250,0,0,0,0,73,69,78,68,174,66,96,130};
    const auto bytes = std::as_bytes(std::span{png});
    const auto path = root / "alpha.PNG";
    put(path, bytes);
    texture::TextureMemoryImportRequest memory{.source=bytes, .extension=".png", .source_sequence=7u};
    texture::TextureImportRequest file{.source_path=path, .source_sequence=7u};
    const auto imported = texture::import_texture_memory(memory);
    CHECK(imported && imported.width == 2u && imported.height == 2u && imported.decoded_bytes == 16u);
    CHECK(same(imported, texture::import_texture_file(file)));
    CHECK(imported.artifact.mips.front().texels == std::vector<std::byte>({
        std::byte{255},std::byte{0},std::byte{0},std::byte{255},
        std::byte{0},std::byte{255},std::byte{0},std::byte{128},
        std::byte{0},std::byte{0},std::byte{255},std::byte{0},
        std::byte{255},std::byte{255},std::byte{255},std::byte{64}}));
    // Replacing a path cannot change this admitted byte snapshot or its identity.
    put(path, std::as_bytes(std::span{"changed", 7u}));
    CHECK(!texture::import_texture_file(file));
    CHECK(same(imported, texture::import_texture_memory(memory)));
    put(path, bytes);
    memory.profile.format = texture::ArtifactFormat::rgba8_unorm;
    memory.profile.color_space = texture::ColorSpace::linear;
    file.profile = memory.profile;
    const auto linear = texture::import_texture_memory(memory);
    CHECK(linear && same(linear, texture::import_texture_file(file)));
    CHECK(linear.artifact.identity.key != imported.artifact.identity.key);
    CHECK(linear.artifact.mips.front().texels == imported.artifact.mips.front().texels);
    memory.profile.color_space = texture::ColorSpace::srgb; file.profile=memory.profile;
    CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    CHECK(texture::import_texture_memory(memory).status == texture::TextureImportStatus::invalid_request);
    memory.profile={}; file.profile={};
    memory.source_sequence=0u; file.source_sequence=0u;
    CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    memory.source_sequence=1u; file.source_sequence=1u;
    for (const auto limit : {0u, 1u, 79u})
    {
        memory.limits.maximum_source_bytes=limit; file.limits=memory.limits;
        CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
        CHECK(!texture::import_texture_memory(memory));
    }
    memory.limits={}; memory.limits.maximum_dimension=1u; file.limits=memory.limits;
    CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    CHECK(texture::import_texture_memory(memory).status == texture::TextureImportStatus::dimensions_exceeded);
    memory.limits={}; memory.limits.maximum_decoded_bytes=15u; file.limits=memory.limits;
    CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    CHECK(!texture::import_texture_memory(memory)); memory.limits={}; file.limits={};
    for (std::size_t end{}; end < bytes.size(); ++end)
    {
        memory.source=bytes.first(end); put(path,memory.source);
        CHECK(!texture::import_texture_memory(memory));
        CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    }
    auto corrupt=png; corrupt[29u]^=1u;
    memory.source=std::as_bytes(std::span{corrupt}); put(path,memory.source);
    CHECK(texture::import_texture_memory(memory).status == texture::TextureImportStatus::malformed_source);
    CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    memory.source=bytes; memory.extension=".PNG";
    CHECK(texture::import_texture_memory(memory).status == texture::TextureImportStatus::unsupported_format);
    memory.extension=".jpg"; CHECK(!texture::import_texture_memory(memory));
    constexpr std::array<unsigned char,17> ppm{'P','6','\n','2',' ','1','\n','2','5','5','\n',255,0,0,0,255,0};
    memory.source=std::as_bytes(std::span{ppm}); memory.extension=".ppm";
    file.source_path=root/"color.ppm"; put(file.source_path,memory.source);
    CHECK(texture::import_texture_memory(memory));
    CHECK(same(texture::import_texture_memory(memory), texture::import_texture_file(file)));
    // Existing file-only malformed/PNG budget/alpha contract remains authoritative.
    CHECK(texture::texture_import_contract_failure() == texture::TextureImportContractFailure::none);
    return 0;
}
