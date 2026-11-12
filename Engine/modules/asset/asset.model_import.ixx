// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
export module asset.model_import;
import asset.mesh_artifact;
import asset.texture_artifact;

export namespace epochengine::asset::model_import
{
    enum class Code : std::uint8_t
    {
        ready, invalid_format, unsupported_feature, limit_exceeded,
        invalid_geometry, external_reference, read_failed, allocation_failure
    };
    struct Limits final
    {
        std::uint64_t maximum_file_bytes{64ull * 1024ull * 1024ull};
        std::uint32_t maximum_json_bytes{4u * 1024u * 1024u};
        std::uint32_t maximum_json_values{131'072u};
        std::uint32_t maximum_nodes{4096u};
        std::uint32_t maximum_primitives{1024u};
        std::uint32_t maximum_images{64u};
        std::uint64_t maximum_image_bytes{32ull * 1024ull * 1024ull};
        // Applies to the whole selected scene as well as each artifact.
        mesh::Limits geometry{};
    };
    struct Sampler final
    {
        std::uint32_t wrap_s{10497u}, wrap_t{10497u};
        std::optional<std::uint32_t> min_filter{}, mag_filter{};
    };
    struct TextureBinding final
    {
        std::uint32_t image{};
        Sampler sampler{};
        // Only TEXCOORD_0 is admitted by this bounded static reader.
        std::uint32_t texcoord{};
    };
    enum class AlphaMode : std::uint8_t { opaque, mask, blend };
    struct Material final
    {
        std::string name{};
        std::array<float, 4> base_color{1, 1, 1, 1};
        std::array<float, 3> emissive{};
        float metallic{1}, roughness{1}, alpha_cutoff{0.5f};
        float normal_scale{1}, occlusion_strength{1};
        bool double_sided{};
        AlphaMode alpha_mode{AlphaMode::opaque};
        std::optional<TextureBinding> base_color_texture{}, metallic_roughness_texture{};
        std::optional<TextureBinding> normal_texture{}, occlusion_texture{}, emissive_texture{};
    };
    struct EmbeddedImage final
    {
        std::string mime_type{};
        std::vector<std::byte> encoded{};
        texture::ContentHash content{};
        // Encoded transport only. Texture decoding/compilation must still pass
        // the texture importer before project publication or runtime binding.
    };
    struct Primitive final
    {
        std::shared_ptr<const mesh::Artifact> geometry{};
        std::optional<std::uint32_t> material{};
        std::uint32_t node{}, mesh{}, primitive{};
        bool normals_generated{}, texcoords_present{}, reflected_winding{};
    };
    struct Result final
    {
        Code code{Code::invalid_format};
        std::string diagnostic{};
        texture::ContentHash source_content{};
        std::vector<Primitive> primitives{};
        std::vector<Material> materials{};
        std::vector<EmbeddedImage> images{};
        std::uint32_t selected_scene{};
        // glTF's right-handed, Y-up metre basis is retained. Node transforms
        // are baked once; this is not a scene/material import or approval.
        double meters_per_unit{1.0};
        [[nodiscard]] explicit operator bool() const noexcept { return code == Code::ready; }
    };
    [[nodiscard]] Result read_glb(std::span<const std::byte> bytes, Limits limits = {}) noexcept;
    [[nodiscard]] Result read_glb_file(const std::filesystem::path& path, Limits limits = {}) noexcept;

    struct CompiledBaseColorTexture final
    {
        std::uint32_t source_image{};
        texture::CompiledTextureArtifact artifact{};
        std::uint64_t decoded_bytes{};
    };
    struct BaseColorCompilation final
    {
        Code code{Code::invalid_format};
        std::string diagnostic{};
        Result model{};
        std::vector<CompiledBaseColorTexture> textures{};
        // Index into textures for every primitive in model, in the same order.
        std::vector<std::uint32_t> primitive_textures{};
        [[nodiscard]] explicit operator bool() const noexcept
        { return code == Code::ready && static_cast<bool>(model)
            && !textures.empty() && primitive_textures.size() == model.primitives.size(); }
    };
    // Pure same-byte GLB -> mesh/texture artifact compilation. This narrow
    // project bridge admits opaque, single-sided, unit-factor base color only;
    // it never silently drops unsupported PBR/alpha channels. No approval,
    // filesystem publication, scene mutation or renderer is created here.
    [[nodiscard]] BaseColorCompilation compile_base_color_glb(
        std::span<const std::byte> bytes, Limits limits = {},
        std::uint32_t maximum_texture_dimension = 4096u,
        std::uint64_t maximum_texture_bytes = 64ull * 1024ull * 1024ull) noexcept;
}
