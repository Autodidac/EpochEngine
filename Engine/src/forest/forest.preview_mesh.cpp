// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <utility>
#include <vector>
#include <filesystem>
#include <mutex>
module forest.preview_mesh;
import asset.texture_import;
import core.path;
import core.logger;
namespace epochengine::forest::preview_mesh
{
    [[nodiscard]] const asset::texture::CompiledTextureArtifact& fallback_material_atlas()
    {
        // A bounded, deterministic authored procedural texture, compiled once.
        // No model, source image download or project-side generator is needed.
        static const auto atlas = [] {
            asset::texture::CompiledTextureArtifact result;
            asset::texture::CompiledTextureMip mip{.width=256u,.height=128u,.row_pitch_bytes=1024u};
            mip.texels.resize(256u * 128u * 4u);
            const auto noise=[](std::uint32_t x,std::uint32_t y) {
                auto h=x*374761393u+y*668265263u+0x46524231u;
                h=(h^(h>>13u))*1274126177u;return h^(h>>16u);
            };
            for(std::uint32_t y=0;y<128u;++y)
                for(std::uint32_t x=0;x<256u;++x)
                {
                    const auto u=(std::clamp)(x%128u,4u,123u)-4u;
                    const auto v=(std::clamp)(y,4u,123u)-4u;
                    const auto grain=noise(u,v)%7u;
                    std::array<std::uint32_t,3> color;
                    if(x<128u)
                    {
                        // Continuous periodic furrows: no blockwise Y noise or
                        // vertical padding bands at the repeat boundary.
                        constexpr double tau=6.283185307179586;
                        const double phase=tau*double(y)/128.0;
                        double shade=222.0;
                        for(std::uint32_t line=0;line<8u;++line)
                        {
                            const double offset=double(noise(line,0)%100u)/100.0;
                            const double center=double(line)*15.0+offset*8.0
                                +1.3*std::sin(phase+offset*tau)
                                +0.45*std::sin(2.0*phase+offset*3.0);
                            const double distance=std::remainder(double(u)-center,120.0);
                            const double width=0.9+double(noise(line,1)%100u)/110.0;
                            shade-=143.0*std::exp(-distance*distance/(width*width));
                        }
                        const auto value=static_cast<std::uint32_t>((std::clamp)(shade+double(grain),55.0,235.0));
                        color={value,value,value};
                    }
                    else
                    {
                        const auto midrib=u>57u&&u<62u;
                        const auto veins=((u+v/3u)%23u)<2u;
                        const auto shade=midrib?23u:veins?12u:0u;
                        color={200u+grain+shade,200u+grain+shade,200u+grain+shade};
                    }
                    const auto offset=(std::size_t{y}*256u+x)*4u;
                    for(std::size_t channel=0;channel<3u;++channel)
                        mip.texels[offset+channel]=static_cast<std::byte>(color[channel]);
                    // The existing opaque bark occupies the left half. Foliage
                    // uses an explicit cutout material; never infer transparency
                    // from a checkerboard painted into an RGB reference image.
                    bool covered=true;
                    if(x>=128u)
                    {
                        const double along=double(v)/119.0;
                        const double across=std::abs(double(u)/119.0-0.5);
                        const double halfWidth=0.48*std::sin(3.141592653589793*along)
                            *(0.96+0.04*std::cos(18.0*3.141592653589793*along));
                        // Leaf UVs attach at v=119. Keep an opaque petiole
                        // there even when the artwork is unavailable.
                        const bool stem = v >= 102u && u >= 58u && u <= 61u;
                        covered=stem || across<=halfWidth;
                    }
                    mip.texels[offset+3u]=covered?std::byte{255u}:std::byte{0u};
                }
            mip.content=asset::texture::compiled_texture_mip_content(mip);
            result.identity=asset::texture::build_compiled_texture_artifact_identity(
                {asset::texture::deterministic_content_hash("forest.bark_foliage_atlas.v2",mip.texels),1u},
                {.format=asset::texture::ArtifactFormat::rgba8_unorm,
                 .color_space=asset::texture::ColorSpace::linear},256u,128u,1u);
            result.mips.push_back(std::move(mip));
            result.identity.compilation_required=false;
            result.payload_content=asset::texture::compiled_texture_payload_content(result.identity,result.mips);
            result.status=asset::texture::ArtifactCompilationStatus::ready;
            if(!asset::texture::validate_compiled_artifact(result))return asset::texture::CompiledTextureArtifact{};
            return result;
        }();
        return atlas;
    }

    namespace
    {
        struct FoliagePlacement final
        {
            double attachmentX{};
            double attachmentY{};
            double sourcePixelsPerTexel{};
            bool valid{};
        };

        template<class AlphaAt>
        [[nodiscard]] FoliagePlacement foliage_placement(
            std::uint32_t width, std::uint32_t height, AlphaAt alphaAt)
        {
            std::uint32_t minimumX = width, minimumY = height, maximumX{}, maximumY{};
            bool covered{};
            for (std::uint32_t y = 0u; y < height; ++y)
                for (std::uint32_t x = 0u; x < width; ++x)
                {
                    if (alphaAt(x, y) < 128u) continue;
                    minimumX = (std::min)(minimumX, x);
                    minimumY = (std::min)(minimumY, y);
                    maximumX = (std::max)(maximumX, x);
                    maximumY = y;
                    covered = true;
                }
            if (!covered) return {};
            std::uint32_t stemLeft{}, stemWidth{};
            double stemDistance = double(width);
            for (std::uint32_t x = 0u; x < width;)
            {
                if (alphaAt(x, maximumY) < 128u) { ++x; continue; }
                const auto left = x;
                while (x < width && alphaAt(x, maximumY) >= 128u) ++x;
                const auto runWidth = x - left;
                const double distance = std::abs((double(left) + double(x - 1u)) * 0.5
                    - double(width - 1u) * 0.5);
                if (runWidth > stemWidth || (runWidth == stemWidth && distance < stemDistance))
                { stemLeft = left; stemWidth = runWidth; stemDistance = distance; }
            }
            // A foliage cutout's stem faces the bottom. Anchor its lowest opaque
            // row, not the transparent image rectangle, to the card's petiole.
            // Fit about that attachment without stretching or clipping the blade.
            // Separate stems must not place the attachment in their transparent gap.
            // Prefer the widest connected run, then the run closest to image centre.
            const double attachmentX = double(stemLeft) + double(stemWidth - 1u) * 0.5;
            const double halfWidth = (std::max)(attachmentX - minimumX + 0.5,
                maximumX - attachmentX + 0.5);
            const double heightAboveStem = maximumY - minimumY + 0.5;
            return {attachmentX, double(maximumY),
                (std::max)(halfWidth / 238.0, heightAboveStem / 476.0), true};
        }

        [[nodiscard]] bool foliage_sample(const FoliagePlacement& placement,
            std::uint32_t width, std::uint32_t height, std::uint32_t localX,
            std::uint32_t targetY, std::uint32_t& sourceX,
            std::uint32_t& sourceY) noexcept
        {
            if (!placement.valid) return false;
            const auto x = std::lround(placement.attachmentX
                + (double(localX) - 238.0) * placement.sourcePixelsPerTexel);
            const auto y = std::lround(placement.attachmentY
                + (double((std::clamp)(targetY, 18u, 494u)) - 494.0)
                    * placement.sourcePixelsPerTexel);
            if (x < 0 || y < 0 || std::uint64_t(x) >= width || std::uint64_t(y) >= height)
                return false;
            sourceX = static_cast<std::uint32_t>(x);
            sourceY = static_cast<std::uint32_t>(y);
            return true;
        }

        struct MaterialResource final
        {
            asset::texture::CompiledTextureArtifact artifact;
            bool referenceArtwork{};
        };

        const MaterialResource& material_resource(std::uint32_t barkTileIndex = 0u)
        {
            // Bounded lazy cache: immutable artifacts remain safe for concurrent
            // preview/publication, and selection never rereads a loaded tile.
            static std::array<MaterialResource, kBarkTileCount> resources;
            static std::array<std::once_flag, kBarkTileCount> initialized;
            const auto index = barkTileIndex < kBarkTileCount ? barkTileIndex : 0u;
            std::call_once(initialized[index], [index] {
            resources[index] = [index] {
                MaterialResource result;
                try
                {
                const auto root = core::path::engine_asset_dir() / "forest";
                static const auto bark = asset::texture::import_texture_file({.source_path=root / "bark_texture_preview.png"});
                static const auto leaf = asset::texture::import_texture_file({.source_path=root / "foliage_reference_board.png"});
                if (!bark || !leaf || bark.artifact.mips.empty() || leaf.artifact.mips.empty())
                {
                    logger::warn("forest", "Reference bark/foliage artwork unavailable; using procedural fallback.");
                    return result;
                }
                const auto& b = bark.artifact.mips.front();
                const auto& l = leaf.artifact.mips.front();
                if (b.width != 1024u || b.height != 1024u || l.width != 1254u || l.height != 1254u)
                {
                    logger::warn("forest", "Reference bark/foliage dimensions differ from the admitted source; using procedural fallback.");
                    return result;
                }
                // Only the oak cell is admitted. Its RGB checkerboard is baked
                // into the source. Remove neutral background connected to the
                // crop border, never interior pale veins or the brown stem.
                constexpr std::uint32_t leafOrigin=4u, leafSize=306u;
                std::vector<std::uint8_t> background(leafSize*leafSize,0u);
                std::vector<std::uint32_t> frontier;
                frontier.reserve(background.size());
                const auto visit=[&](std::uint32_t x,std::uint32_t y) {
                    const auto index=y*leafSize+x;
                    if(background[index])return;
                    const auto pixel=std::size_t{y+leafOrigin}*l.row_pitch_bytes+(x+leafOrigin)*4u;
                    const auto r=std::to_integer<unsigned>(l.texels[pixel]);
                    const auto g=std::to_integer<unsigned>(l.texels[pixel+1u]);
                    const auto bvalue=std::to_integer<unsigned>(l.texels[pixel+2u]);
                    const auto low=(std::min)({r,g,bvalue});
                    const auto high=(std::max)({r,g,bvalue});
                    if(low<150u || high-low>24u)return;
                    background[index]=1u;
                    frontier.push_back(index);
                };
                for(std::uint32_t edge=0;edge<leafSize;++edge) {
                    visit(edge,0);visit(edge,leafSize-1u);
                    visit(0,edge);visit(leafSize-1u,edge);
                }
                for(std::size_t next=0;next<frontier.size();++next) {
                    const auto x=frontier[next]%leafSize,y=frontier[next]/leafSize;
                    if(x)visit(x-1u,y);
                    if(y)visit(x,y-1u);
                    if(x+1u<leafSize)visit(x+1u,y);
                    if(y+1u<leafSize)visit(x,y+1u);
                }
                const auto foliagePlacement = foliage_placement(leafSize, leafSize,
                    [&](std::uint32_t x, std::uint32_t y) {
                        return background[y * leafSize + x] ? 0u : 255u;
                    });
                // The source is a 4x4 bark board. Select one bark cell,
                // excluding its divider; never repeat the entire contact sheet.
                // UV proportions match the existing padded two-slot material.
                asset::texture::CompiledTextureMip mip{.width=1024u,.height=512u,.row_pitch_bytes=4096u};
                mip.texels.resize(1024u*512u*4u);
                for (std::uint32_t y=0; y<512u; ++y)
                    for (std::uint32_t x=0; x<1024u; ++x)
                    {
                        const bool foliage=x>=512u;
                        const auto local=(std::clamp)(x%512u,18u,494u)-18u;
                        std::uint32_t leafX{}, leafY{};
                        const bool leafCovered = !foliage || foliage_sample(foliagePlacement,
                            leafSize, leafSize, local, y, leafX, leafY);
                        const auto sy=foliage ? leafOrigin+leafY
                            : (index/4u)*256u+7u+y*241u/511u;
                        const auto sx=foliage ? leafOrigin+leafX
                            : (index%4u)*256u+7u+local*241u/476u;
                        const auto& source=foliage?l:b;
                        const auto from=std::size_t{sy}*source.row_pitch_bytes+sx*4u;
                        const auto to=(std::size_t{y}*1024u+x)*4u;
                        for (std::size_t c=0;c<4u;++c) mip.texels[to+c]=source.texels[from+c];
                        if(foliage) mip.texels[to+3u]=!leafCovered
                            || background[leafY*leafSize+leafX] ? std::byte{0u}:std::byte{255u};
                    }
                mip.content=asset::texture::compiled_texture_mip_content(mip);
                auto& atlas=result.artifact;
                atlas.identity=asset::texture::build_compiled_texture_artifact_identity(
                    {asset::texture::deterministic_content_hash("forest.supplied_bark_photo_oak.v2",mip.texels),1u},
                    {.format=asset::texture::ArtifactFormat::rgba8_srgb,
                     .color_space=asset::texture::ColorSpace::srgb},1024u,512u,1u);
                atlas.mips.push_back(std::move(mip));
                atlas.identity.compilation_required=false;
                atlas.payload_content=asset::texture::compiled_texture_payload_content(atlas.identity,atlas.mips);
                atlas.status=asset::texture::ArtifactCompilationStatus::ready;
                result.referenceArtwork=asset::texture::validate_compiled_artifact(atlas);
                if (result.referenceArtwork)
                    logger::info("forest", "Loaded supplied bark and alpha foliage artwork into the forest material.");
                else logger::warn("forest", "Reference forest atlas validation failed; using procedural fallback.");
                return result;
                }
                catch (...)
                {
                    logger::warn("forest", "Reference forest material preparation failed; using procedural fallback.");
                    return MaterialResource{};
                }
            }();
            });
            return resources[index];
        }
    }

    [[nodiscard]] bool reference_material_available()
    {
        return material_resource().referenceArtwork;
    }

    [[nodiscard]] const asset::texture::CompiledTextureArtifact& material_atlas(std::uint32_t barkTileIndex)
    {
        const auto& resource=material_resource(barkTileIndex);
        return resource.referenceArtwork ? resource.artifact : fallback_material_atlas();
    }

    [[nodiscard]] MaterialAtlasResult compose_material_atlas(
        const asset::texture::CompiledTextureArtifact* bark,
        const asset::texture::CompiledTextureArtifact* foliage,
        std::uint32_t barkTileIndex, std::array<float, 3u> woodTint,
        std::array<float, 3u> foliageTint)
    {
        MaterialAtlasResult result;
        const auto fail = [&](std::string message) { result.artifact = {}; result.diagnostic = std::move(message); return std::move(result); };
        const auto supported = [](const auto& image) {
            if (image.mips.empty()) return false;
            const auto& mip = image.mips.front();
            const auto& profile = image.identity.profile;
            std::uint64_t bytes{};
            for (const auto& level:image.mips)
            {
                if (level.texels.size() > 64u * 1024u * 1024u - bytes) return false;
                bytes += level.texels.size();
            }
            return mip.width <= 8192u && mip.height <= 8192u
                && ((profile.format == asset::texture::ArtifactFormat::rgba8_srgb && profile.color_space == asset::texture::ColorSpace::srgb)
                    || (profile.format == asset::texture::ArtifactFormat::rgba8_unorm && profile.color_space == asset::texture::ColorSpace::linear))
                && asset::texture::validate_compiled_artifact(image);
        };
        if ((!bark && !foliage) || barkTileIndex >= kBarkTileCount)
            return fail("Choose an imported bark or foliage texture.");
        if ((bark && !supported(*bark)) || (foliage && !supported(*foliage)))
            return fail("Forest materials require a verified RGBA8 texture up to 8192 pixels and 64 MiB.");
        if (foliage)
        {
            bool transparent{}, opaque{};
            const auto& mip = foliage->mips.front();
            for (std::uint32_t y = 0; y < mip.height; ++y)
                for (std::uint32_t x = 0; x < mip.width; ++x)
                {
                    const auto alpha = std::to_integer<unsigned>(mip.texels[std::size_t{y} * mip.row_pitch_bytes + x * 4u + 3u]);
                    transparent |= alpha < 128u; opaque |= alpha >= 128u;
                }
            if (!transparent || !opaque) return fail("Foliage needs a transparent background and a visible leaf. Import an RGBA cutout; a painted white or checkerboard background is not transparency.");
        }
        for (const auto& tint : {woodTint, foliageTint}) for (const auto channel : tint)
            if (!std::isfinite(channel) || channel < 0.0f || channel > 1.0f) return fail("The fallback forest colors are invalid.");
        try
        {
            const auto& base = material_atlas(barkTileIndex);
            if (!supported(base)) return fail("The packaged forest material could not be verified.");
            const bool tintBase = !reference_material_available();
            std::uint32_t barkLeft{}, barkWidth{};
            if (bark)
            {
                const auto& mip = bark->mips.front();
                barkWidth = mip.width;
                // Generated trunk photographs may include background on both
                // sides. Crop only paired, low-detail margins with a clear
                // transition to the bark within the outer eighth of the image.
                if (mip.width >= 64u && mip.height >= 32u)
                {
                    const auto margin = [&](bool fromRight) {
                        const auto limit = mip.width / 8u;
                        for (std::uint32_t column = 0u; column < limit; ++column)
                        {
                            const auto x = fromRight ? mip.width - 1u - column : column;
                            std::array<unsigned, 64u> samples{};
                            unsigned total{};
                            for (std::size_t row = 0u; row < samples.size(); ++row)
                            {
                                const auto y = (2u * row + 1u) * mip.height / (2u * samples.size());
                                const auto offset = y * mip.row_pitch_bytes + x * 4u;
                                samples[row] = (std::to_integer<unsigned>(mip.texels[offset])
                                    + 2u * std::to_integer<unsigned>(mip.texels[offset + 1u])
                                    + std::to_integer<unsigned>(mip.texels[offset + 2u])) / 4u;
                                total += samples[row];
                            }
                            const auto mean = static_cast<int>(total / samples.size());
                            unsigned deviation{};
                            for (auto sample : samples)
                                deviation += static_cast<unsigned>(std::abs(static_cast<int>(sample) - mean));
                            if (deviation > 10u * samples.size()) return column;
                        }
                        return 0u;
                    };
                    const auto left = margin(false);
                    const auto right = margin(true);
                    if (left >= mip.width / 64u && right >= mip.width / 64u)
                    {
                        const auto inset = mip.width / 64u;
                        barkLeft = left + inset;
                        barkWidth -= barkLeft + right + inset;
                    }
                }
            }
            FoliagePlacement foliagePlacement;
            if (foliage)
            {
                const auto& mip = foliage->mips.front();
                foliagePlacement = foliage_placement(mip.width, mip.height,
                    [&](std::uint32_t x, std::uint32_t y) {
                        return std::to_integer<unsigned>(mip.texels[
                            std::size_t{y} * mip.row_pitch_bytes + x * 4u + 3u]);
                    });
            }
            asset::texture::CompiledTextureMip target{.width=1024u, .height=512u, .row_pitch_bytes=4096u};
            target.texels.resize(1024u * 512u * 4u);
            const auto encode = [](double value) {
                return static_cast<std::byte>(static_cast<unsigned>(std::lround(255.0 * (value <= 0.0031308
                    ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055))));
            };
            for (std::uint32_t y = 0u; y < 512u; ++y)
                for (std::uint32_t x = 0u; x < 1024u; ++x)
                {
                    const bool leaf = x >= 512u;
                    const auto* custom = leaf ? foliage : bark;
                    const auto& source = custom ? *custom : base;
                    const auto& mip = source.mips.front();
                    std::uint32_t sx{}, sy{};
                    bool outside{};
                    const auto local = (std::clamp)(x % 512u, 18u, 494u) - 18u;
                    if (!custom)
                    {
                        sx = static_cast<std::uint32_t>((leaf ? 0.5 : 0.0) * mip.width
                            + double(x % 512u) * mip.width / 1024.0);
                        sy = y * mip.height / 512u;
                    }
                    else if (!leaf)
                    {
                        // Preserve the source pixel aspect and vertical grain;
                        // repeat tall/short bark rather than stretching a square.
                        sx = barkLeft + local * (barkWidth - 1u) / 476u;
                        sy = static_cast<std::uint32_t>(std::uint64_t{y} * barkWidth / 476u) % mip.height;
                    }
                    else
                    {
                        outside = !foliage_sample(foliagePlacement, mip.width, mip.height,
                            local, y, sx, sy);
                    }
                    const auto from = std::size_t{sy} * mip.row_pitch_bytes + sx * 4u;
                    const auto to = std::size_t{y} * target.row_pitch_bytes + x * 4u;
                    const auto seamDistance = (std::min)(local, 476u - local);
                    constexpr auto seamWidth = 16u;
                    for (std::size_t c = 0u; c < 3u; ++c)
                    {
                        auto byte = mip.texels[from+c];
                        if (!leaf && barkLeft && seamDistance < seamWidth)
                        {
                            const auto opposite = barkLeft + (476u - local) * (barkWidth - 1u) / 476u;
                            const auto other = mip.texels[std::size_t{sy} * mip.row_pitch_bytes + opposite * 4u + c];
                            const auto weight = seamWidth - seamDistance;
                            byte = static_cast<std::byte>((std::to_integer<unsigned>(byte) * (2u * seamWidth - weight)
                                + std::to_integer<unsigned>(other) * weight) / (2u * seamWidth));
                        }
                        double value = std::to_integer<unsigned>(byte) / 255.0;
                        if (!custom && tintBase) value *= (leaf ? foliageTint : woodTint)[c];
                        target.texels[to+c] = source.identity.profile.color_space == asset::texture::ColorSpace::linear
                            ? encode(value) : byte;
                    }
                    target.texels[to+3u] = !leaf ? std::byte{255u} : outside ? std::byte{0u} : mip.texels[from+3u];
                }
            target.content = asset::texture::compiled_texture_mip_content(target);
            auto& atlas = result.artifact;
            atlas.identity = asset::texture::build_compiled_texture_artifact_identity(
                {asset::texture::deterministic_content_hash("forest.project_bark_foliage.v1", target.texels),1u},
                {.format=asset::texture::ArtifactFormat::rgba8_srgb,.color_space=asset::texture::ColorSpace::srgb},1024u,512u,1u);
            atlas.mips.push_back(std::move(target));
            atlas.identity.compilation_required = false;
            atlas.payload_content = asset::texture::compiled_texture_payload_content(atlas.identity, atlas.mips);
            atlas.status = asset::texture::ArtifactCompilationStatus::ready;
            if (!asset::texture::validate_compiled_artifact(atlas)) return fail("The composed forest material failed validation.");
            return result;
        }
        catch (...) { return fail("The bounded forest material could not be allocated."); }
    }

    [[nodiscard]] Geometry extract(
        const epochengine::authoring::morphology::Graph& graph,
        const epochengine::authoring::morphology::Sample& sample) noexcept
    {
        Geometry geometry{};
        geometry.segmentCount = (std::min)(sample.segments.size(), kMaximumSegments);
        for (std::size_t index = 0u; index < geometry.segmentCount; ++index)
        {
            const auto& source = sample.segments[index];
            geometry.segments[index] = Segment{
                .start = source.start,
                .end = source.end,
                .radiusStart = source.radius_start_meters,
                .radiusEnd = source.radius_end_meters,
                .depth = source.depth,
                .sourceSegment = source.source.index};
        }

        for (const auto& source : sample.terminals)
        {
            if (source.kind != epochengine::authoring::morphology::TerminalKind::foliage)
                continue;
            if (geometry.leafCount == kMaximumLeaves)
                break;
            epochengine::voxel::Float3 outward{1.0f, 0.0f, 0.0f};
            epochengine::voxel::Float3 up{0.0f, 1.0f, 0.0f};
            std::uint32_t variant{};
            std::uint32_t sourceSegment{};
            if (source.source.index < graph.terminals.size())
            {
                const auto& terminal = graph.terminals[source.source.index];
                outward = terminal.outward;
                up = terminal.up;
                variant = terminal.variant;
                for (const auto& segment : graph.segments)
                {
                    if (segment.child_node == terminal.node)
                    {
                        sourceSegment = segment.id.index;
                        break;
                    }
                }
            }
            geometry.leaves[geometry.leafCount++] = Leaf{
                .position = source.position,
                .outward = outward,
                .up = up,
                .size = source.scale_meters,
                .sourceSegment = sourceSegment,
                .variant = variant};
        }
        return geometry;
    }
}
