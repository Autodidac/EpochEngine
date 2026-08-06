module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <vector>

export module render.canvas2d.cpu;

import render.canvas2d;
import render.device;

export namespace epochengine::canvas2d::cpu
{
    enum class RasterCode : std::uint8_t
    {
        success,
        partial,
        invalid_frame,
        invalid_limits,
        invalid_resource,
        invalid_clip,
        unsupported_format,
        unsupported_material,
        capacity_exceeded,
        arithmetic_overflow,
        allocation_failure
    };

    enum class CpuContractFailure : std::uint8_t
    {
        none,
        identity_registry,
        frame_compile,
        canvas_image_invalid,
        presentation_image_invalid,
        unexpected_result_code,
        raster_partial,
        raster_invalid_frame,
        raster_invalid_limits,
        raster_invalid_resource,
        raster_invalid_clip,
        raster_unsupported_format,
        raster_unsupported_material,
        raster_capacity_exceeded,
        raster_arithmetic_overflow,
        raster_allocation_failure,
        canvas_hash_mismatch,
        presentation_hash_mismatch,
        canvas_extent_mismatch,
        presentation_extent_mismatch,
        solid_raster,
        texture_raster,
        missing_resource,
        capacity_limit
    };

    [[nodiscard]] constexpr const char* cpu_contract_failure_name(
        CpuContractFailure failure) noexcept
    {
        switch (failure)
        {
        case CpuContractFailure::none:
            return "pass";
        case CpuContractFailure::identity_registry:
            return "identity_registry";
        case CpuContractFailure::frame_compile:
            return "frame_compile";
        case CpuContractFailure::canvas_image_invalid:
            return "canvas_image_invalid";
        case CpuContractFailure::presentation_image_invalid:
            return "presentation_image_invalid";
        case CpuContractFailure::unexpected_result_code:
            return "unexpected_result_code";
        case CpuContractFailure::raster_partial:
            return "raster_partial";
        case CpuContractFailure::raster_invalid_frame:
            return "raster_invalid_frame";
        case CpuContractFailure::raster_invalid_limits:
            return "raster_invalid_limits";
        case CpuContractFailure::raster_invalid_resource:
            return "raster_invalid_resource";
        case CpuContractFailure::raster_invalid_clip:
            return "raster_invalid_clip";
        case CpuContractFailure::raster_unsupported_format:
            return "raster_unsupported_format";
        case CpuContractFailure::raster_unsupported_material:
            return "raster_unsupported_material";
        case CpuContractFailure::raster_capacity_exceeded:
            return "raster_capacity_exceeded";
        case CpuContractFailure::raster_arithmetic_overflow:
            return "raster_arithmetic_overflow";
        case CpuContractFailure::raster_allocation_failure:
            return "raster_allocation_failure";
        case CpuContractFailure::canvas_hash_mismatch:
            return "canvas_hash_mismatch";
        case CpuContractFailure::presentation_hash_mismatch:
            return "presentation_hash_mismatch";
        case CpuContractFailure::canvas_extent_mismatch:
            return "canvas_extent_mismatch";
        case CpuContractFailure::presentation_extent_mismatch:
            return "presentation_extent_mismatch";
        case CpuContractFailure::solid_raster:
            return "solid_raster";
        case CpuContractFailure::texture_raster:
            return "texture_raster";
        case CpuContractFailure::missing_resource:
            return "missing_resource";
        case CpuContractFailure::capacity_limit:
            return "capacity_limit";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr CpuContractFailure cpu_contract_failure_from_raster_code(
        RasterCode code) noexcept
    {
        switch (code)
        {
        case RasterCode::success:
            return CpuContractFailure::none;
        case RasterCode::partial:
            return CpuContractFailure::raster_partial;
        case RasterCode::invalid_frame:
            return CpuContractFailure::raster_invalid_frame;
        case RasterCode::invalid_limits:
            return CpuContractFailure::raster_invalid_limits;
        case RasterCode::invalid_resource:
            return CpuContractFailure::raster_invalid_resource;
        case RasterCode::invalid_clip:
            return CpuContractFailure::raster_invalid_clip;
        case RasterCode::unsupported_format:
            return CpuContractFailure::raster_unsupported_format;
        case RasterCode::unsupported_material:
            return CpuContractFailure::raster_unsupported_material;
        case RasterCode::capacity_exceeded:
            return CpuContractFailure::raster_capacity_exceeded;
        case RasterCode::arithmetic_overflow:
            return CpuContractFailure::raster_arithmetic_overflow;
        case RasterCode::allocation_failure:
            return CpuContractFailure::raster_allocation_failure;
        }
        return CpuContractFailure::unexpected_result_code;
    }

    [[nodiscard]] constexpr bool succeeded(RasterCode code) noexcept
    {
        return code == RasterCode::success || code == RasterCode::partial;
    }

    enum class AlphaEncoding : std::uint8_t
    {
        straight,
        premultiplied
    };

    struct Rgba8 final
    {
        std::uint8_t r{};
        std::uint8_t g{};
        std::uint8_t b{};
        std::uint8_t a{255};

        friend constexpr bool operator==(Rgba8, Rgba8) noexcept = default;
    };

    static_assert(sizeof(Rgba8) == 4, "Canvas2D RGBA8 upload layout must remain tightly packed.");

    struct Image final
    {
        CanvasExtent extent{};
        TextureFormat format{TextureFormat::rgba8_unorm};
        std::vector<Rgba8> pixels{};

        [[nodiscard]] bool valid() const noexcept
        {
            if (extent.empty())
                return false;
            const std::uint64_t expected = static_cast<std::uint64_t>(extent.width)
                * static_cast<std::uint64_t>(extent.height);
            return expected == pixels.size()
                && format == TextureFormat::rgba8_unorm;
        }

        [[nodiscard]] const Rgba8* row(std::uint32_t y) const noexcept
        {
            if (!valid() || y >= extent.height)
                return nullptr;
            return pixels.data() + static_cast<std::size_t>(y) * extent.width;
        }

        [[nodiscard]] Rgba8* row(std::uint32_t y) noexcept
        {
            if (!valid() || y >= extent.height)
                return nullptr;
            return pixels.data() + static_cast<std::size_t>(y) * extent.width;
        }
    };

    struct TextureView final
    {
        LogicalTextureReference logical{};
        TextureHandle physical{};
        CanvasExtent extent{};
        std::uint32_t row_stride_pixels{};
        std::span<const Rgba8> pixels{};
        SpriteColorSpace color_space{SpriteColorSpace::srgb};
        AlphaEncoding alpha_encoding{AlphaEncoding::straight};

        [[nodiscard]] constexpr bool has_identity() const noexcept
        {
            return static_cast<bool>(logical) || static_cast<bool>(physical);
        }
    };

    struct ClipRect final
    {
        std::uint32_t key{};
        RectI rectangle{};
    };

    struct ResourceBindings final
    {
        std::span<const TextureView> textures{};
        std::span<const ClipRect> clips{};
    };

    struct RasterLimits final
    {
        std::uint64_t maximum_canvas_pixels{16'777'216};
        std::uint64_t maximum_presentation_pixels{33'554'432};
        std::uint64_t maximum_triangles{262'144};
        std::uint64_t maximum_fragments{268'435'456};
        std::uint32_t maximum_textures{4'096};
        std::uint32_t maximum_clips{4'096};
    };

    struct RasterPolicy final
    {
        bool allow_partial_frame{};
        bool allow_unresolved_tile_layers{};
    };

    [[nodiscard]] constexpr bool valid(const RasterLimits& limits) noexcept
    {
        return limits.maximum_canvas_pixels != 0
            && limits.maximum_presentation_pixels != 0
            && limits.maximum_triangles != 0
            && limits.maximum_fragments != 0
            && limits.maximum_textures != 0
            && limits.maximum_clips != 0;
    }

    struct RasterMetrics final
    {
        std::uint64_t submitted_batches{};
        std::uint64_t accepted_batches{};
        std::uint64_t skipped_batches{};
        std::uint64_t submitted_triangles{};
        std::uint64_t rasterized_triangles{};
        std::uint64_t degenerate_triangles{};
        std::uint64_t clipped_triangles{};
        std::uint64_t fragments_tested{};
        std::uint64_t fragments_covered{};
        std::uint64_t fragments_shaded{};
        std::uint64_t fragments_discarded{};
        std::uint64_t fragments_blended{};
        std::uint64_t texture_samples{};
        std::uint64_t missing_texture_bindings{};
        std::uint64_t missing_clip_bindings{};
        std::uint64_t invalid_resource_bindings{};
        std::uint64_t unresolved_tile_layers{};
        std::uint64_t canvas_bytes{};
        std::uint64_t presentation_bytes{};
    };

    struct RasterResult final
    {
        RasterCode code{RasterCode::invalid_frame};
        Image canvas{};
        Image presentation{};
        RasterMetrics metrics{};
        std::uint64_t canvas_hash{};
        std::uint64_t presentation_hash{};
        std::uint64_t frame_sequence{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == RasterCode::success && canvas.valid() && presentation.valid();
        }
    };

    namespace detail
    {
        inline constexpr std::int64_t subpixel_scale = 256;
        inline constexpr std::int64_t subpixel_half = subpixel_scale / 2;
        inline constexpr std::int64_t maximum_fixed_coordinate = 1ll << 29;

        struct LinearSample final
        {
            double r{};
            double g{};
            double b{};
            double a{1.0};
        };

        struct RasterVertex final
        {
            std::int64_t x{};
            std::int64_t y{};
            double u{};
            double v{};
            LinearSample color{};
        };

        [[nodiscard]] constexpr bool checked_product(
            std::uint64_t lhs,
            std::uint64_t rhs,
            std::uint64_t& output) noexcept
        {
            if (lhs != 0 && rhs > (std::numeric_limits<std::uint64_t>::max)() / lhs)
                return false;
            output = lhs * rhs;
            return true;
        }

        [[nodiscard]] constexpr double clamp_unit(double value) noexcept
        {
            return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
        }

        inline constexpr std::array<std::uint16_t, 256> srgb8_to_linear16{{
            0, 20, 40, 60, 80, 99, 119, 139, 159, 179, 199, 219, 241, 264, 288, 313,
            340, 367, 396, 427, 458, 491, 526, 562, 599, 637, 677, 718, 761, 805, 851, 898,
            947, 997, 1048, 1101, 1156, 1212, 1270, 1330, 1391, 1453, 1517, 1583, 1651, 1720, 1790, 1863,
            1937, 2013, 2090, 2170, 2250, 2333, 2418, 2504, 2592, 2681, 2773, 2866, 2961, 3058, 3157, 3258,
            3360, 3464, 3570, 3678, 3788, 3900, 4014, 4129, 4247, 4366, 4488, 4611, 4736, 4864, 4993, 5124,
            5257, 5392, 5530, 5669, 5810, 5953, 6099, 6246, 6395, 6547, 6700, 6856, 7014, 7174, 7335, 7500,
            7666, 7834, 8004, 8177, 8352, 8528, 8708, 8889, 9072, 9258, 9445, 9635, 9828, 10022, 10219, 10417,
            10619, 10822, 11028, 11235, 11446, 11658, 11873, 12090, 12309, 12530, 12754, 12980, 13209, 13440, 13673, 13909,
            14146, 14387, 14629, 14874, 15122, 15371, 15623, 15878, 16135, 16394, 16656, 16920, 17187, 17456, 17727, 18001,
            18277, 18556, 18837, 19121, 19407, 19696, 19987, 20281, 20577, 20876, 21177, 21481, 21787, 22096, 22407, 22721,
            23038, 23357, 23678, 24002, 24329, 24658, 24990, 25325, 25662, 26001, 26344, 26688, 27036, 27386, 27739, 28094,
            28452, 28813, 29176, 29542, 29911, 30282, 30656, 31033, 31412, 31794, 32179, 32567, 32957, 33350, 33745, 34143,
            34544, 34948, 35355, 35764, 36176, 36591, 37008, 37429, 37852, 38278, 38706, 39138, 39572, 40009, 40449, 40891,
            41337, 41785, 42236, 42690, 43147, 43606, 44069, 44534, 45002, 45473, 45947, 46423, 46903, 47385, 47871, 48359,
            48850, 49344, 49841, 50341, 50844, 51349, 51858, 52369, 52884, 53401, 53921, 54445, 54971, 55500, 56032, 56567,
            57105, 57646, 58190, 58737, 59287, 59840, 60396, 60955, 61517, 62082, 62650, 63221, 63795, 64372, 64952, 65535
        }};

        [[nodiscard]] inline double linear_to_srgb(double value) noexcept
        {
            const auto linear = static_cast<std::uint16_t>(
                std::lround(clamp_unit(value) * 65535.0));
            const auto upper = std::lower_bound(
                srgb8_to_linear16.begin(), srgb8_to_linear16.end(), linear);
            if (upper == srgb8_to_linear16.begin())
                return 0.0;
            if (upper == srgb8_to_linear16.end())
                return 1.0;
            const auto lower = upper - 1;
            const auto lower_distance = static_cast<std::uint32_t>(linear - *lower);
            const auto upper_distance = static_cast<std::uint32_t>(*upper - linear);
            const auto index = upper_distance < lower_distance
                ? static_cast<std::uint32_t>(upper - srgb8_to_linear16.begin())
                : static_cast<std::uint32_t>(lower - srgb8_to_linear16.begin());
            return static_cast<double>(index) / 255.0;
        }

        [[nodiscard]] inline std::uint8_t quantize(double value) noexcept
        {
            return static_cast<std::uint8_t>(std::lround(clamp_unit(value) * 255.0));
        }

        [[nodiscard]] inline LinearSample decode(
            Rgba8 pixel,
            bool srgb) noexcept
        {
            LinearSample value{
                static_cast<double>(pixel.r) / 255.0,
                static_cast<double>(pixel.g) / 255.0,
                static_cast<double>(pixel.b) / 255.0,
                static_cast<double>(pixel.a) / 255.0
            };
            if (srgb)
            {
                value.r = static_cast<double>(srgb8_to_linear16[pixel.r]) / 65535.0;
                value.g = static_cast<double>(srgb8_to_linear16[pixel.g]) / 65535.0;
                value.b = static_cast<double>(srgb8_to_linear16[pixel.b]) / 65535.0;
            }
            return value;
        }

        [[nodiscard]] inline Rgba8 encode(
            LinearSample value,
            bool srgb) noexcept
        {
            if (srgb)
            {
                value.r = linear_to_srgb(value.r);
                value.g = linear_to_srgb(value.g);
                value.b = linear_to_srgb(value.b);
            }
            return {quantize(value.r), quantize(value.g), quantize(value.b), quantize(value.a)};
        }

        [[nodiscard]] inline bool valid_texture(const TextureView& texture) noexcept
        {
            if (!texture.has_identity() || texture.extent.empty()
                || texture.row_stride_pixels < texture.extent.width)
            {
                return false;
            }
            if (texture.color_space != SpriteColorSpace::linear
                && texture.color_space != SpriteColorSpace::srgb)
            {
                return false;
            }
            std::uint64_t required{};
            if (!checked_product(texture.row_stride_pixels, texture.extent.height, required))
                return false;
            return required <= texture.pixels.size();
        }

        [[nodiscard]] constexpr bool valid_clip(const ClipRect& clip) noexcept
        {
            return clip.key != 0 && !clip.rectangle.empty();
        }

        [[nodiscard]] inline const TextureView* find_texture(
            const ResourceBindings& resources,
            const SpriteMaterialDeclaration& material) noexcept
        {
            const auto found = std::find_if(
                resources.textures.begin(),
                resources.textures.end(),
                [&](const TextureView& texture)
                {
                    if (material.source == SpriteSourceKind::texture)
                        return texture.logical == material.logical_texture;
                    if (material.source == SpriteSourceKind::render_surface)
                        return texture.physical == material.texture;
                    return false;
                });
            return found == resources.textures.end() ? nullptr : &*found;
        }

        [[nodiscard]] inline const ClipRect* find_clip(
            const ResourceBindings& resources,
            std::uint32_t key) noexcept
        {
            if (key == 0)
                return nullptr;
            const auto found = std::find_if(
                resources.clips.begin(),
                resources.clips.end(),
                [&](const ClipRect& clip) { return clip.key == key; });
            return found == resources.clips.end() ? nullptr : &*found;
        }

        [[nodiscard]] inline double address(double coordinate, AddressMode mode) noexcept
        {
            if (mode == AddressMode::clamp_to_edge)
                return clamp_unit(coordinate);
            if (mode == AddressMode::repeat)
            {
                const double wrapped = coordinate - std::floor(coordinate);
                return wrapped < 0.0 ? wrapped + 1.0 : wrapped;
            }
            const double period = coordinate - std::floor(coordinate / 2.0) * 2.0;
            const double positive = period < 0.0 ? period + 2.0 : period;
            return positive <= 1.0 ? positive : 2.0 - positive;
        }

        [[nodiscard]] inline Rgba8 texel(
            const TextureView& texture,
            std::uint32_t x,
            std::uint32_t y) noexcept
        {
            const std::size_t index = static_cast<std::size_t>(y)
                * texture.row_stride_pixels + x;
            return texture.pixels[index];
        }

        [[nodiscard]] inline LinearSample sample_texture(
            const TextureView& texture,
            const SpriteMaterialDeclaration& material,
            double u,
            double v,
            RasterMetrics& metrics) noexcept
        {
            ++metrics.texture_samples;
            u = address(u, material.sampler.address_u);
            v = address(v, material.sampler.address_v);
            const bool srgb = texture.color_space == SpriteColorSpace::srgb;
            const bool linear_filter = material.sampler.min_filter == FilterMode::linear
                || material.sampler.mag_filter == FilterMode::linear;
            if (!linear_filter || texture.extent.width == 1 || texture.extent.height == 1)
            {
                const auto x = static_cast<std::uint32_t>(std::lround(
                    u * static_cast<double>(texture.extent.width - 1)));
                const auto y = static_cast<std::uint32_t>(std::lround(
                    v * static_cast<double>(texture.extent.height - 1)));
                return decode(texel(texture, x, y), srgb);
            }

            const double fx = u * static_cast<double>(texture.extent.width - 1);
            const double fy = v * static_cast<double>(texture.extent.height - 1);
            const auto x0 = static_cast<std::uint32_t>(std::floor(fx));
            const auto y0 = static_cast<std::uint32_t>(std::floor(fy));
            const auto x1 = (std::min)(x0 + 1, texture.extent.width - 1);
            const auto y1 = (std::min)(y0 + 1, texture.extent.height - 1);
            const double tx = fx - static_cast<double>(x0);
            const double ty = fy - static_cast<double>(y0);
            const LinearSample p00 = decode(texel(texture, x0, y0), srgb);
            const LinearSample p10 = decode(texel(texture, x1, y0), srgb);
            const LinearSample p01 = decode(texel(texture, x0, y1), srgb);
            const LinearSample p11 = decode(texel(texture, x1, y1), srgb);
            const auto mix = [](double a, double b, double amount) noexcept
            {
                return a + (b - a) * amount;
            };
            return {
                mix(mix(p00.r, p10.r, tx), mix(p01.r, p11.r, tx), ty),
                mix(mix(p00.g, p10.g, tx), mix(p01.g, p11.g, tx), ty),
                mix(mix(p00.b, p10.b, tx), mix(p01.b, p11.b, tx), ty),
                mix(mix(p00.a, p10.a, tx), mix(p01.a, p11.a, tx), ty)
            };
        }

        [[nodiscard]] inline bool fixed_coordinate(float value, std::int64_t& output) noexcept
        {
            if (!std::isfinite(value))
                return false;
            const double scaled = static_cast<double>(value) * subpixel_scale;
            if (scaled < -static_cast<double>(maximum_fixed_coordinate)
                || scaled > static_cast<double>(maximum_fixed_coordinate))
            {
                return false;
            }
            output = static_cast<std::int64_t>(std::llround(scaled));
            return true;
        }

        [[nodiscard]] inline bool make_raster_vertex(
            const SpriteVertex& vertex,
            const CameraPlan& camera,
            RasterVertex& output) noexcept
        {
            const Float2 canvas = world_to_canvas(vertex.position, camera);
            if (!fixed_coordinate(canvas.x, output.x)
                || !fixed_coordinate(canvas.y, output.y))
            {
                return false;
            }
            output.u = vertex.uv.x;
            output.v = vertex.uv.y;
            output.color = {
                vertex.color.r,
                vertex.color.g,
                vertex.color.b,
                vertex.color.a
            };
            return true;
        }

        [[nodiscard]] constexpr std::int64_t edge(
            const RasterVertex& a,
            const RasterVertex& b,
            std::int64_t x,
            std::int64_t y) noexcept
        {
            return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
        }

        [[nodiscard]] constexpr bool top_left(
            const RasterVertex& a,
            const RasterVertex& b) noexcept
        {
            const std::int64_t dy = b.y - a.y;
            const std::int64_t dx = b.x - a.x;
            return dy < 0 || (dy == 0 && dx > 0);
        }

        [[nodiscard]] constexpr bool covered(std::int64_t edge_value, bool inclusive) noexcept
        {
            return edge_value > 0 || (edge_value == 0 && inclusive);
        }

        [[nodiscard]] inline RectI full_clip(CanvasExtent extent) noexcept
        {
            return {0, 0, extent.width, extent.height};
        }

        [[nodiscard]] inline RectI intersect_clip(RectI lhs, CanvasExtent extent) noexcept
        {
            const std::int64_t left = (std::max)(std::int64_t{0}, static_cast<std::int64_t>(lhs.x));
            const std::int64_t top = (std::max)(std::int64_t{0}, static_cast<std::int64_t>(lhs.y));
            const std::int64_t right = (std::min)(
                static_cast<std::int64_t>(extent.width),
                static_cast<std::int64_t>(lhs.x) + lhs.width);
            const std::int64_t bottom = (std::min)(
                static_cast<std::int64_t>(extent.height),
                static_cast<std::int64_t>(lhs.y) + lhs.height);
            if (right <= left || bottom <= top)
                return {};
            return {
                static_cast<std::int32_t>(left),
                static_cast<std::int32_t>(top),
                static_cast<std::uint32_t>(right - left),
                static_cast<std::uint32_t>(bottom - top)
            };
        }

        [[nodiscard]] inline LinearSample shade(
            LinearSample source,
            LinearSample tint,
            const SpriteMaterialDeclaration& material,
            bool& discard) noexcept
        {
            source.r *= tint.r;
            source.g *= tint.g;
            source.b *= tint.b;
            source.a *= tint.a;
            source.r = clamp_unit(source.r);
            source.g = clamp_unit(source.g);
            source.b = clamp_unit(source.b);
            source.a = clamp_unit(source.a);
            if (material.alpha == SpriteAlphaMode::opaque)
                source.a = 1.0;
            if (material.alpha == SpriteAlphaMode::mask
                && source.a < material.alpha_cutoff)
            {
                discard = true;
            }
            return source;
        }

        [[nodiscard]] inline LinearSample blend(
            LinearSample source,
            LinearSample destination,
            SpriteAlphaMode mode) noexcept
        {
            if (mode == SpriteAlphaMode::opaque || mode == SpriteAlphaMode::mask)
                return source;
            if (mode == SpriteAlphaMode::additive)
            {
                return {
                    clamp_unit(destination.r + source.r * source.a),
                    clamp_unit(destination.g + source.g * source.a),
                    clamp_unit(destination.b + source.b * source.a),
                    clamp_unit(destination.a + source.a)
                };
            }

            const double inverse_alpha = 1.0 - source.a;
            double source_r = source.r;
            double source_g = source.g;
            double source_b = source.b;
            if (mode == SpriteAlphaMode::straight)
            {
                source_r *= source.a;
                source_g *= source.a;
                source_b *= source.a;
            }
            return {
                clamp_unit(source_r + destination.r * inverse_alpha),
                clamp_unit(source_g + destination.g * inverse_alpha),
                clamp_unit(source_b + destination.b * inverse_alpha),
                clamp_unit(source.a + destination.a * inverse_alpha)
            };
        }

        [[nodiscard]] inline RasterCode validate_resources(
            const ResourceBindings& resources,
            const RasterLimits& limits,
            RasterMetrics& metrics) noexcept
        {
            if (resources.textures.size() > limits.maximum_textures
                || resources.clips.size() > limits.maximum_clips)
            {
                return RasterCode::capacity_exceeded;
            }
            for (std::size_t index = 0; index < resources.textures.size(); ++index)
            {
                const TextureView& texture = resources.textures[index];
                if (!valid_texture(texture))
                {
                    ++metrics.invalid_resource_bindings;
                    return RasterCode::invalid_resource;
                }
                for (std::size_t other = index + 1; other < resources.textures.size(); ++other)
                {
                    const TextureView& candidate = resources.textures[other];
                    const bool duplicate_logical = texture.logical && candidate.logical
                        && texture.logical == candidate.logical;
                    const bool duplicate_physical = texture.physical && candidate.physical
                        && texture.physical == candidate.physical;
                    if (duplicate_logical || duplicate_physical)
                    {
                        ++metrics.invalid_resource_bindings;
                        return RasterCode::invalid_resource;
                    }
                }
            }
            for (std::size_t index = 0; index < resources.clips.size(); ++index)
            {
                if (!valid_clip(resources.clips[index]))
                    return RasterCode::invalid_clip;
                for (std::size_t other = index + 1; other < resources.clips.size(); ++other)
                {
                    if (resources.clips[index].key == resources.clips[other].key)
                        return RasterCode::invalid_clip;
                }
            }
            return RasterCode::success;
        }

        [[nodiscard]] inline bool allocate_image(
            Image& image,
            CanvasExtent extent,
            TextureFormat format,
            Rgba8 clear,
            std::uint64_t maximum_pixels) noexcept
        {
            std::uint64_t pixel_count{};
            if (!checked_product(extent.width, extent.height, pixel_count)
                || pixel_count == 0 || pixel_count > maximum_pixels
                || pixel_count > (std::numeric_limits<std::size_t>::max)())
            {
                return false;
            }
            try
            {
                image.extent = extent;
                image.format = format;
                image.pixels.assign(static_cast<std::size_t>(pixel_count), clear);
                return true;
            }
            catch (const std::bad_alloc&)
            {
                image = {};
                return false;
            }
        }

        [[nodiscard]] inline std::uint64_t image_hash(const Image& image) noexcept
        {
            if (!image.valid())
                return 0;
            std::uint64_t value = 1469598103934665603ull;
            const auto mix = [&](std::uint8_t byte) noexcept
            {
                value ^= byte;
                value *= 1099511628211ull;
            };
            mix(1);
            for (unsigned shift = 0; shift != 32; shift += 8)
                mix(static_cast<std::uint8_t>(image.extent.width >> shift));
            for (unsigned shift = 0; shift != 32; shift += 8)
                mix(static_cast<std::uint8_t>(image.extent.height >> shift));
            const std::uint64_t packed_row_bytes =
                static_cast<std::uint64_t>(image.extent.width) * sizeof(Rgba8);
            for (unsigned shift = 0; shift != 64; shift += 8)
                mix(static_cast<std::uint8_t>(packed_row_bytes >> shift));
            mix(static_cast<std::uint8_t>(image.format));
            for (Rgba8 pixel : image.pixels)
            {
                mix(pixel.r);
                mix(pixel.g);
                mix(pixel.b);
                mix(pixel.a);
            }
            return value == 0 ? 1 : value;
        }

        [[nodiscard]] inline LinearSample sample_image(
            const Image& image,
            double x,
            double y,
            FilterMode filter) noexcept
        {
            constexpr bool srgb = false;
            if (filter == FilterMode::nearest)
            {
                const auto px = static_cast<std::uint32_t>((std::clamp)(
                    std::llround(x),
                    0ll,
                    static_cast<long long>(image.extent.width - 1)));
                const auto py = static_cast<std::uint32_t>((std::clamp)(
                    std::llround(y),
                    0ll,
                    static_cast<long long>(image.extent.height - 1)));
                return decode(image.pixels[static_cast<std::size_t>(py) * image.extent.width + px], srgb);
            }

            x = (std::clamp)(x, 0.0, static_cast<double>(image.extent.width - 1));
            y = (std::clamp)(y, 0.0, static_cast<double>(image.extent.height - 1));
            const auto x0 = static_cast<std::uint32_t>(std::floor(x));
            const auto y0 = static_cast<std::uint32_t>(std::floor(y));
            const auto x1 = (std::min)(x0 + 1, image.extent.width - 1);
            const auto y1 = (std::min)(y0 + 1, image.extent.height - 1);
            const double tx = x - x0;
            const double ty = y - y0;
            const auto at = [&](std::uint32_t px, std::uint32_t py)
            {
                return decode(image.pixels[static_cast<std::size_t>(py) * image.extent.width + px], srgb);
            };
            const LinearSample p00 = at(x0, y0);
            const LinearSample p10 = at(x1, y0);
            const LinearSample p01 = at(x0, y1);
            const LinearSample p11 = at(x1, y1);
            const auto mix = [](double a, double b, double amount) noexcept
            {
                return a + (b - a) * amount;
            };
            return {
                mix(mix(p00.r, p10.r, tx), mix(p01.r, p11.r, tx), ty),
                mix(mix(p00.g, p10.g, tx), mix(p01.g, p11.g, tx), ty),
                mix(mix(p00.b, p10.b, tx), mix(p01.b, p11.b, tx), ty),
                mix(mix(p00.a, p10.a, tx), mix(p01.a, p11.a, tx), ty)
            };
        }

        [[nodiscard]] inline RasterCode compose_presentation(
            const FinalComposePlan& compose,
            const Image& canvas,
            const RasterLimits& limits,
            Image& presentation,
            RasterMetrics& metrics) noexcept
        {
            constexpr bool target_srgb = false;
            const Rgba8 letterbox = encode({
                compose.letterbox_color.r,
                compose.letterbox_color.g,
                compose.letterbox_color.b,
                compose.letterbox_color.a
            }, target_srgb);
            if (!allocate_image(
                    presentation,
                    compose.viewport.output_surface,
                    canvas.format,
                    letterbox,
                    limits.maximum_presentation_pixels))
            {
                return RasterCode::capacity_exceeded;
            }

            const RectI destination = compose.viewport.clipped_destination;
            for (std::uint32_t local_y = 0; local_y < destination.height; ++local_y)
            {
                const std::int64_t output_y = static_cast<std::int64_t>(destination.y) + local_y;
                if (output_y < 0 || output_y >= presentation.extent.height)
                    continue;
                for (std::uint32_t local_x = 0; local_x < destination.width; ++local_x)
                {
                    const std::int64_t output_x = static_cast<std::int64_t>(destination.x) + local_x;
                    if (output_x < 0 || output_x >= presentation.extent.width)
                        continue;
                    const double canvas_x = compose.viewport.visible_canvas.x
                        + (static_cast<double>(local_x) + 0.5)
                            / compose.viewport.scale_x - 0.5;
                    const double canvas_y = compose.viewport.visible_canvas.y
                        + (static_cast<double>(local_y) + 0.5)
                            / compose.viewport.scale_y - 0.5;
                    const LinearSample sample = sample_image(
                        canvas,
                        canvas_x,
                        canvas_y,
                        compose.presentation_filter);
                    presentation.pixels[static_cast<std::size_t>(output_y)
                        * presentation.extent.width + static_cast<std::size_t>(output_x)] =
                        encode(sample, target_srgb);
                }
            }
            metrics.presentation_bytes = presentation.pixels.size() * sizeof(Rgba8);
            return RasterCode::success;
        }
    }

    [[nodiscard]] RasterResult rasterize(
        const Canvas2DFramePlan& frame,
        const ResourceBindings& resources = {},
        const RasterLimits& limits = {},
        const RasterPolicy& policy = {})
    {
        RasterResult output{};
        output.frame_sequence = frame.frame_sequence;
        output.metrics.submitted_batches = frame.sprites.batches.size();
        const bool accepted_frame = frame.code == ResultCode::success
            || (policy.allow_partial_frame && frame.code == ResultCode::partial);
        if (!valid(limits) || !accepted_frame || !frame.compose
            || !frame.compose.requires_offscreen_canvas)
        {
            output.code = !valid(limits)
                ? RasterCode::invalid_limits
                : RasterCode::invalid_frame;
            return output;
        }
        if (frame.compose.canvas_target.render_target.color_format
                != TextureFormat::rgba8_unorm)
        {
            output.code = RasterCode::unsupported_format;
            return output;
        }

        output.code = detail::validate_resources(resources, limits, output.metrics);
        if (output.code != RasterCode::success)
            return output;

        constexpr bool target_srgb = false;
        const Rgba8 clear = detail::encode({
            frame.compose.clear_color.r,
            frame.compose.clear_color.g,
            frame.compose.clear_color.b,
            frame.compose.clear_color.a
        }, target_srgb);
        if (!detail::allocate_image(
                output.canvas,
                frame.compose.viewport.render_extent,
                frame.compose.canvas_target.render_target.color_format,
                clear,
                limits.maximum_canvas_pixels))
        {
            output.code = RasterCode::capacity_exceeded;
            return output;
        }
        output.metrics.canvas_bytes = output.canvas.pixels.size() * sizeof(Rgba8);

        bool partial{};
        if (frame.compose.tile_layer_count != 0)
        {
            if (!policy.allow_unresolved_tile_layers)
            {
                output.code = RasterCode::unsupported_material;
                return output;
            }
            output.metrics.unresolved_tile_layers = frame.compose.tile_layer_count;
            partial = true;
        }

        try
        {
            for (const CompiledSpriteBatch& batch : frame.sprites.batches)
            {
                if (batch.first_index > frame.sprites.indices.size()
                    || batch.index_count > frame.sprites.indices.size() - batch.first_index
                    || batch.index_count % 3 != 0
                    || batch.first_vertex > frame.sprites.vertices.size()
                    || batch.vertex_count > frame.sprites.vertices.size() - batch.first_vertex)
                {
                    output.code = RasterCode::invalid_frame;
                    return output;
                }
                if (batch.key.material.sampler.mipmapped
                    || batch.key.material.sampler.maximum_anisotropy != 1.0f)
                {
                    output.code = RasterCode::unsupported_material;
                    return output;
                }
                if (!batch.key.material.writes_color)
                {
                    ++output.metrics.skipped_batches;
                    continue;
                }

                const TextureView* texture{};
                if (batch.key.material.source != SpriteSourceKind::solid_color)
                {
                    texture = detail::find_texture(resources, batch.key.material);
                    if (!texture)
                    {
                        ++output.metrics.missing_texture_bindings;
                        ++output.metrics.skipped_batches;
                        partial = true;
                        continue;
                    }
                    const bool source_is_premultiplied =
                        texture->alpha_encoding == AlphaEncoding::premultiplied;
                    const bool material_is_premultiplied =
                        batch.key.material.alpha == SpriteAlphaMode::premultiplied;
                    if (texture->color_space != batch.key.material.color_space
                        || source_is_premultiplied != material_is_premultiplied)
                    {
                        output.code = RasterCode::unsupported_material;
                        return output;
                    }
                }
                const ClipRect* clip_binding = detail::find_clip(resources, batch.key.clip_key);
                if (batch.key.clip_key != 0 && !clip_binding)
                {
                    ++output.metrics.missing_clip_bindings;
                    ++output.metrics.skipped_batches;
                    partial = true;
                    continue;
                }
                const RectI clip = clip_binding
                    ? detail::intersect_clip(clip_binding->rectangle, output.canvas.extent)
                    : detail::full_clip(output.canvas.extent);
                if (clip.empty())
                {
                    ++output.metrics.skipped_batches;
                    continue;
                }

                ++output.metrics.accepted_batches;
                for (std::uint32_t offset = 0; offset < batch.index_count; offset += 3)
                {
                    if (++output.metrics.submitted_triangles > limits.maximum_triangles)
                    {
                        output.code = RasterCode::capacity_exceeded;
                        return output;
                    }
                    const std::uint32_t first = frame.sprites.indices[batch.first_index + offset];
                    const std::uint32_t second = frame.sprites.indices[batch.first_index + offset + 1];
                    const std::uint32_t third = frame.sprites.indices[batch.first_index + offset + 2];
                    if (first >= frame.sprites.vertices.size()
                        || second >= frame.sprites.vertices.size()
                        || third >= frame.sprites.vertices.size())
                    {
                        output.code = RasterCode::invalid_frame;
                        return output;
                    }

                    std::array<detail::RasterVertex, 3> triangle{};
                    if (!detail::make_raster_vertex(
                            frame.sprites.vertices[first], frame.compose.camera, triangle[0])
                        || !detail::make_raster_vertex(
                            frame.sprites.vertices[second], frame.compose.camera, triangle[1])
                        || !detail::make_raster_vertex(
                            frame.sprites.vertices[third], frame.compose.camera, triangle[2]))
                    {
                        output.code = RasterCode::invalid_frame;
                        return output;
                    }

                    std::int64_t area = detail::edge(
                        triangle[0], triangle[1], triangle[2].x, triangle[2].y);
                    if (area == 0)
                    {
                        ++output.metrics.degenerate_triangles;
                        continue;
                    }
                    if (area < 0)
                    {
                        std::swap(triangle[1], triangle[2]);
                        area = -area;
                    }

                    const std::int64_t minimum_x = (std::min)({
                        triangle[0].x, triangle[1].x, triangle[2].x});
                    const std::int64_t minimum_y = (std::min)({
                        triangle[0].y, triangle[1].y, triangle[2].y});
                    const std::int64_t maximum_x = (std::max)({
                        triangle[0].x, triangle[1].x, triangle[2].x});
                    const std::int64_t maximum_y = (std::max)({
                        triangle[0].y, triangle[1].y, triangle[2].y});
                    const std::int64_t clip_right = static_cast<std::int64_t>(clip.x) + clip.width;
                    const std::int64_t clip_bottom = static_cast<std::int64_t>(clip.y) + clip.height;
                    const std::int64_t start_x = (std::max)(
                        static_cast<std::int64_t>(clip.x),
                        static_cast<std::int64_t>(std::floor(
                            static_cast<double>(minimum_x) / detail::subpixel_scale)));
                    const std::int64_t start_y = (std::max)(
                        static_cast<std::int64_t>(clip.y),
                        static_cast<std::int64_t>(std::floor(
                            static_cast<double>(minimum_y) / detail::subpixel_scale)));
                    const std::int64_t end_x = (std::min)(
                        clip_right,
                        static_cast<std::int64_t>(std::ceil(
                            static_cast<double>(maximum_x) / detail::subpixel_scale)));
                    const std::int64_t end_y = (std::min)(
                        clip_bottom,
                        static_cast<std::int64_t>(std::ceil(
                            static_cast<double>(maximum_y) / detail::subpixel_scale)));
                    if (end_x <= start_x || end_y <= start_y)
                    {
                        ++output.metrics.clipped_triangles;
                        continue;
                    }

                    const bool edge0_inclusive = detail::top_left(triangle[1], triangle[2]);
                    const bool edge1_inclusive = detail::top_left(triangle[2], triangle[0]);
                    const bool edge2_inclusive = detail::top_left(triangle[0], triangle[1]);
                    ++output.metrics.rasterized_triangles;
                    for (std::int64_t y = start_y; y < end_y; ++y)
                    {
                        for (std::int64_t x = start_x; x < end_x; ++x)
                        {
                            if (++output.metrics.fragments_tested > limits.maximum_fragments)
                            {
                                output.code = RasterCode::capacity_exceeded;
                                return output;
                            }
                            const std::int64_t sample_x = x * detail::subpixel_scale
                                + detail::subpixel_half;
                            const std::int64_t sample_y = y * detail::subpixel_scale
                                + detail::subpixel_half;
                            const std::int64_t w0 = detail::edge(
                                triangle[1], triangle[2], sample_x, sample_y);
                            const std::int64_t w1 = detail::edge(
                                triangle[2], triangle[0], sample_x, sample_y);
                            const std::int64_t w2 = detail::edge(
                                triangle[0], triangle[1], sample_x, sample_y);
                            if (!detail::covered(w0, edge0_inclusive)
                                || !detail::covered(w1, edge1_inclusive)
                                || !detail::covered(w2, edge2_inclusive))
                            {
                                continue;
                            }
                            ++output.metrics.fragments_covered;
                            const double inverse_area = 1.0 / static_cast<double>(area);
                            const double a = static_cast<double>(w0) * inverse_area;
                            const double b = static_cast<double>(w1) * inverse_area;
                            const double c = static_cast<double>(w2) * inverse_area;
                            const double u = triangle[0].u * a
                                + triangle[1].u * b + triangle[2].u * c;
                            const double v = triangle[0].v * a
                                + triangle[1].v * b + triangle[2].v * c;
                            detail::LinearSample source = texture
                                ? detail::sample_texture(
                                    *texture, batch.key.material, u, v, output.metrics)
                                : detail::LinearSample{1.0, 1.0, 1.0, 1.0};
                            const detail::LinearSample tint{
                                triangle[0].color.r * a + triangle[1].color.r * b
                                    + triangle[2].color.r * c,
                                triangle[0].color.g * a + triangle[1].color.g * b
                                    + triangle[2].color.g * c,
                                triangle[0].color.b * a + triangle[1].color.b * b
                                    + triangle[2].color.b * c,
                                triangle[0].color.a * a + triangle[1].color.a * b
                                    + triangle[2].color.a * c
                            };
                            bool discard{};
                            source = detail::shade(source, tint, batch.key.material, discard);
                            if (discard)
                            {
                                ++output.metrics.fragments_discarded;
                                continue;
                            }
                            const std::size_t pixel_index = static_cast<std::size_t>(y)
                                * output.canvas.extent.width + static_cast<std::size_t>(x);
                            const detail::LinearSample destination = detail::decode(
                                output.canvas.pixels[pixel_index], target_srgb);
                            const detail::LinearSample blended = detail::blend(
                                source, destination, batch.key.material.alpha);
                            output.canvas.pixels[pixel_index] = detail::encode(
                                blended, target_srgb);
                            ++output.metrics.fragments_shaded;
                            if (batch.key.material.alpha != SpriteAlphaMode::opaque
                                && batch.key.material.alpha != SpriteAlphaMode::mask)
                            {
                                ++output.metrics.fragments_blended;
                            }
                        }
                    }
                }
            }
        }
        catch (const std::bad_alloc&)
        {
            output = {};
            output.frame_sequence = frame.frame_sequence;
            output.code = RasterCode::allocation_failure;
            return output;
        }

        output.canvas_hash = detail::image_hash(output.canvas);
        const RasterCode compose_code = detail::compose_presentation(
            frame.compose,
            output.canvas,
            limits,
            output.presentation,
            output.metrics);
        if (compose_code != RasterCode::success)
        {
            output.code = compose_code;
            return output;
        }
        output.code = partial ? RasterCode::partial : RasterCode::success;
        if (output.code == RasterCode::success)
        {
            output.canvas_hash = detail::image_hash(output.canvas);
            output.presentation_hash = detail::image_hash(output.presentation);
        }
        else
            output.canvas_hash = output.presentation_hash = 0;
        return output;
    }

    [[nodiscard]] CpuContractFailure canvas2d_cpu_runtime_contract_failure()
    {
        SpriteIdentityRegistry identities{8};
        const auto sprite = identities.create();
        if (!sprite)
            return CpuContractFailure::identity_registry;

        ProjectSettings project{};
        project.logical_canvas = {8, 8};
        project.pixels_per_world_unit = 1.0f;
        project.viewport_policy = ViewportPolicy::integer_scale;
        project.presentation_filter = FilterMode::nearest;
        project.pixel_snap = PixelSnapMode::camera_and_sprites;

        SpriteMaterialDeclaration solid{};
        solid.stable_key = 1;
        solid.source = SpriteSourceKind::solid_color;
        solid.alpha = SpriteAlphaMode::opaque;
        const std::array<SpriteSubmission, 1> sprites{{
            {*sprite, solid, {{0.0f, 0.0f}, {4.0f, 4.0f}}, {0.0f, 0.0f, 1.0f, 1.0f},
                {1.0f, 0.0f, 0.0f, 1.0f}, SpritePhase::world, 0, 0, 0, 1}
        }};
        Canvas2DSubmission submission{};
        submission.project = project;
        submission.camera = {};
        submission.sprites = sprites;
        const Canvas2DFramePlan frame = compile_canvas2d_submission(
            submission,
            {16, 16});
        if (!frame)
            return CpuContractFailure::frame_compile;

        const RasterResult first = rasterize(frame);
        const RasterResult second = rasterize(frame);
        if (first.code != RasterCode::success)
            return cpu_contract_failure_from_raster_code(first.code);
        if (second.code != RasterCode::success)
            return cpu_contract_failure_from_raster_code(second.code);
        if (!first.canvas.valid() || !second.canvas.valid())
            return CpuContractFailure::canvas_image_invalid;
        if (!first.presentation.valid() || !second.presentation.valid())
            return CpuContractFailure::presentation_image_invalid;
        if (first.canvas_hash == 0 || first.canvas_hash != second.canvas_hash)
            return CpuContractFailure::canvas_hash_mismatch;
        if (first.presentation_hash == 0
            || first.presentation_hash != second.presentation_hash)
        {
            return CpuContractFailure::presentation_hash_mismatch;
        }
        if (first.canvas.extent != CanvasExtent{8, 8})
            return CpuContractFailure::canvas_extent_mismatch;
        if (first.presentation.extent != CanvasExtent{16, 16})
            return CpuContractFailure::presentation_extent_mismatch;
        const Rgba8 center = first.canvas.pixels[4 * first.canvas.extent.width + 4];
        const Rgba8 corner = first.canvas.pixels[0];
        if (center.r != 255 || center.g != 0 || center.b != 0 || center.a != 255
            || corner == center || first.metrics.rasterized_triangles != 2
            || first.metrics.fragments_shaded != 16)
        {
            return CpuContractFailure::solid_raster;
        }

        std::array<Rgba8, 4> texture_pixels{{
            {255, 255, 255, 255}, {0, 0, 0, 255},
            {0, 0, 0, 255}, {255, 255, 255, 255}
        }};
        const LogicalTextureReference logical{7, 1};
        const TextureView texture{logical, {}, {2, 2}, 2, texture_pixels,
            SpriteColorSpace::srgb, AlphaEncoding::straight};
        SpriteMaterialDeclaration textured = solid;
        textured.stable_key = 2;
        textured.source = SpriteSourceKind::texture;
        textured.logical_texture = logical;
        textured.sampler.min_filter = FilterMode::linear;
        textured.sampler.mag_filter = FilterMode::linear;
        textured.alpha = SpriteAlphaMode::straight;
        const std::array<SpriteSubmission, 1> textured_sprites{{
            {*sprite, textured, {{0.0f, 0.0f}, {4.0f, 4.0f}}, {0.0f, 0.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 0.5f}, SpritePhase::world, 0, 0, 0, 2}
        }};
        submission.sprites = textured_sprites;
        const Canvas2DFramePlan textured_frame = compile_canvas2d_submission(
            submission,
            {16, 16});
        const std::array<TextureView, 1> textures{texture};
        const RasterResult textured_result = rasterize(
            textured_frame,
            {textures, {}});
        if (!textured_result || textured_result.metrics.texture_samples != 16
            || textured_result.metrics.fragments_blended != 16)
        {
            return CpuContractFailure::texture_raster;
        }

        const RasterResult missing = rasterize(textured_frame);
        if (missing.code != RasterCode::partial
            || missing.metrics.missing_texture_bindings != 1
            || !missing.canvas.valid() || !missing.presentation.valid()
            || missing.canvas_hash != 0 || missing.presentation_hash != 0)
        {
            return CpuContractFailure::missing_resource;
        }

        RasterLimits tiny{};
        tiny.maximum_canvas_pixels = 4;
        if (rasterize(frame, {}, tiny).code != RasterCode::capacity_exceeded)
            return CpuContractFailure::capacity_limit;

        return CpuContractFailure::none;
    }

    [[nodiscard]] bool canvas2d_cpu_runtime_contract()
    {
        return canvas2d_cpu_runtime_contract_failure() == CpuContractFailure::none;
    }
}
