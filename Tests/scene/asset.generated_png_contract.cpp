// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>
import asset.model_import;
import asset.texture_import;
import asset.texture_artifact;
namespace model=epochengine::asset::model_import;
namespace texture=epochengine::asset::texture;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr,"Generated PNG contract line %d\n",__LINE__);return 86;} } while(false)
int main(int argc,char** argv)
{
    CHECK(argc==3);
    const auto original=model::read_glb_file(std::filesystem::path{argv[1]});
    const auto repaired=model::read_glb_file(std::filesystem::path{argv[2]});
    CHECK(original&&repaired&&original.images.size()==1u&&repaired.images.size()==1u);
    CHECK(original.primitives.size()==repaired.primitives.size());
    for(std::size_t i=0;i<original.primitives.size();++i)
        CHECK(original.primitives[i].geometry->key()==repaired.primitives[i].geometry->key());
    texture::TextureCompileProfile profile{};
    profile.format=texture::ArtifactFormat::rgba8_srgb;profile.color_space=texture::ColorSpace::srgb;
    const texture::TextureImportLimits limits{32u*1024u*1024u,64u*1024u*1024u,512u};
    const auto bad=texture::import_texture_memory({.source=original.images[0].encoded,.extension=".png",.profile=profile,.limits=limits});
    CHECK(!bad&&bad.status==texture::TextureImportStatus::malformed_source);
    const auto good=texture::import_texture_memory({.source=repaired.images[0].encoded,.extension=".png",.profile=profile,.limits=limits});
    CHECK(good&&good.width==512u&&good.height==512u&&good.decoded_bytes==512u*512u*4u);
    CHECK(good.artifact.identity.profile.color_space==texture::ColorSpace::srgb);
    CHECK(original.images[0].encoded.size()==repaired.images[0].encoded.size()+3u);
    for(const auto badDimension:{0u,511u})
    {
        auto bounded=limits;bounded.maximum_dimension=badDimension;
        CHECK(!texture::import_texture_memory({.source=repaired.images[0].encoded,.extension=".png",.profile=profile,.limits=bounded}));
    }
    auto corrupt=repaired.images[0].encoded;corrupt[29]^=std::byte{1};
    CHECK(!texture::import_texture_memory({.source=corrupt,.extension=".png",.profile=profile,.limits=limits}));
    std::ifstream input{std::filesystem::path{argv[2]},std::ios::binary|std::ios::ate};CHECK(input);
    std::vector<std::byte> bytes(static_cast<std::size_t>(input.tellg()));input.seekg(0);
    CHECK(input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())));
    const auto compiled=model::compile_base_color_glb(bytes,{},512u,64u*1024u*1024u);
    CHECK(compiled&&compiled.textures.size()==1u&&compiled.primitive_textures.size()==repaired.primitives.size());
    CHECK(compiled.textures[0].artifact.identity.key==good.artifact.identity.key);
    return 0;
}
