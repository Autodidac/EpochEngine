/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/context.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

export module vulkan.context:texture;

import :shared_vk;
import core.logger;
import core.path;
import engine.cli;
import utility.string_converter;
import image.loader;
import atlas.texture;
import render.arcade;

export namespace epochengine::vulkantextures
{
    void ensure_uploaded(const epochengine::TextureAtlas& atlas);
}

namespace epochengine::vulkancontext
{
    namespace
    {
        constexpr std::string_view kLogSys = "Vulkan.Texture";

        inline void log_info(std::string_view msg,
            const std::source_location& loc = std::source_location::current())
        {
#if EPOCH_ENABLE_BACKEND_UPLOAD_CONFIRMATION_LOGS && EPOCH_ENABLE_VULKAN_CONFIRMATION_LOGS
            epochengine::logger::get(kLogSys).log(
                epochengine::logger::LogLevel::INFO, msg, loc);
#else
            (void)msg;
            (void)loc;
#endif
        }

        inline void log_error(std::string_view msg,
            const std::source_location& loc = std::source_location::current())
        {
            epochengine::logger::get(kLogSys).log(
                epochengine::logger::LogLevel::Error, msg, loc);
        }

        std::filesystem::path resolve_texture_path(const std::source_location& loc)
        {
            namespace fs = std::filesystem;

            const fs::path target = "texture.ppm";
            const fs::path exeDir = epochengine::core::cli::exe_path.empty()
                ? fs::path{}
                : fs::absolute(epochengine::core::cli::exe_path).parent_path();
            const fs::path runtimeRoot = epochengine::core::path::runtime_root_dir();
            const fs::path engineAssets = epochengine::core::path::engine_asset_dir();
            const fs::path exampleAssets = epochengine::core::path::example_asset_dir();
            const std::array<fs::path, 13> candidates = {
                exeDir / target,
                exeDir / "assets" / "vulkan" / target,
                target,
                fs::path("assets") / "vulkan" / target,
                runtimeRoot / "assets" / "vulkan" / target,
                engineAssets / "vulkan" / target,
                engineAssets / target,
                exampleAssets / "vulkan" / target,
                exampleAssets / target,
                fs::path("epochengine") / "assets" / "vulkan" / target,
                fs::path("..") / "epochengine" / "assets" / "vulkan" / target,
                fs::path("..") / ".." / ".." / "x64" / "Debug" / target,
                fs::path("..") / ".." / ".." / "x64" / "Debug" / "assets" / "vulkan" / target,
            };

            for (const auto& path : candidates)
            {
                fs::path abs = fs::absolute(path).lexically_normal();
                if (fs::exists(abs) && fs::is_regular_file(abs))
                    return abs;
            }

            std::string tried;
            tried.reserve(256);
            for (const auto& p : candidates)
            {
                tried += "\n  - ";
                tried += epochengine::text::path_to_utf8(fs::absolute(p).lexically_normal());
            }

            log_error(std::format("Failed to load texture image. Tried paths:{}", tried), loc);
            throw std::runtime_error("Failed to resolve Vulkan texture path.");
        }

        void ensure_rgba8(ImageData& img)
        {
            const std::size_t w = static_cast<std::size_t>(img.width);
            const std::size_t h = static_cast<std::size_t>(img.height);

            if (w == 0 || h == 0)
                throw std::runtime_error("Texture has invalid dimensions (0).");

            const std::size_t rgbBytes = w * h * 3u;
            const std::size_t rgbaBytes = w * h * 4u;

            if (img.pixels.size() == rgbaBytes)
                return;

            if (img.pixels.size() != rgbBytes)
            {
                throw std::runtime_error(
                    "Unexpected texture pixel size: got " +
                    std::to_string(img.pixels.size()) +
                    " expected " + std::to_string(rgbBytes) +
                    " or " + std::to_string(rgbaBytes));
            }

            std::vector<std::uint8_t> rgba(rgbaBytes);

            const std::uint8_t* src = img.pixels.data();
            std::uint8_t* dst = rgba.data();

            for (std::size_t i = 0; i < w * h; ++i)
            {
                dst[i * 4 + 0] = src[i * 3 + 0];
                dst[i * 4 + 1] = src[i * 3 + 1];
                dst[i * 4 + 2] = src[i * 3 + 2];
                dst[i * 4 + 3] = 255;
            }

            img.pixels = std::move(rgba);
        }

        std::string vk_result_to_string(vk::Result r)
        {
            try { return vk::to_string(r); }
            catch (...) { return std::to_string(static_cast<int>(r)); }
        }
    } // namespace

    void Application::transitionImageLayout(vk::Image image, vk::Format /*format*/,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
    {
        vk::UniqueCommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vk::PipelineStageFlags sourceStage{};
        vk::PipelineStageFlags destinationStage{};

        if (oldLayout == vk::ImageLayout::eUndefined &&
            newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
            newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
            throw std::runtime_error("Unsupported layout transition!");
        }

        commandBuffer->pipelineBarrier(
            sourceStage,
            destinationStage,
            {},
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void Application::copyBufferToImage(vk::Buffer buffer, vk::Image image,
        std::uint32_t width, std::uint32_t height)
    {
        vk::UniqueCommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{ 0, 0, 0 };
        region.imageExtent = vk::Extent3D{ width, height, 1u };

        commandBuffer->copyBufferToImage(
            buffer,
            image,
            vk::ImageLayout::eTransferDstOptimal,
            1,
            &region
        );

        endSingleTimeCommands(commandBuffer);
    }

    void Application::createTextureImageView()
    {
        textureImageView = createImageViewUnique(
            *textureImage,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageAspectFlagBits::eColor
        );
    }

    void Application::createTextureSampler()
    {
        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.flags = {};
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = vk::CompareOp::eAlways;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        auto [r, samp] = device->createSamplerUnique(samplerInfo);
        if (r != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create texture sampler!");

        textureSampler = std::move(samp);
    }

    void Application::createTextureImage()
    {
        const auto loc = std::source_location::current();

        const std::filesystem::path texturePath = resolve_texture_path(loc);
        const std::string texturePathUtf8 = epochengine::text::path_to_utf8(texturePath);

        ImageData texture = [&]() -> ImageData {
            try
            {
                return a_loadImage(texturePath, false);
            }
            catch (const std::exception& ex)
            {
                log_error(std::format("a_loadImage failed for path='{}': {}", texturePathUtf8, ex.what()), loc);
                throw std::runtime_error("Failed to load texture image '" + texturePathUtf8 + "': " + ex.what());
            }
        }();

        ensure_rgba8(texture);

        const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texture.pixels.size());

        log_info(std::format("path='{}'", texturePathUtf8), loc);
        log_info(std::format("size={}x{} bytes={}",
            static_cast<std::uint32_t>(texture.width),
            static_cast<std::uint32_t>(texture.height),
            static_cast<std::uint64_t>(imageSize)), loc);

        vk::UniqueBuffer stagingBuffer;
        vk::UniqueDeviceMemory stagingMemory;

        std::tie(stagingBuffer, stagingMemory) = createBuffer(
            imageSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

        log_info(std::format("staging-copy bytes={}", static_cast<std::uint64_t>(imageSize)), loc);

        auto [mapRes, mapped] = device->mapMemory(*stagingMemory, 0, imageSize);
        if (mapRes != vk::Result::eSuccess)
        {
            log_error(std::format("mapMemory failed: {}", vk_result_to_string(mapRes)), loc);
            throw std::runtime_error(
                "Failed to map texture staging buffer (mapMemory returned " +
                vk_result_to_string(mapRes) + ").");
        }

        std::memcpy(mapped, texture.pixels.data(), static_cast<std::size_t>(imageSize));
        device->unmapMemory(*stagingMemory);

        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = vk::Format::eR8G8B8A8Srgb;
        imageInfo.extent = vk::Extent3D{
            static_cast<std::uint32_t>(texture.width),
            static_cast<std::uint32_t>(texture.height),
            1u
        };
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;

        auto [imgRes, img] = device->createImageUnique(imageInfo);
        if (imgRes != vk::Result::eSuccess)
        {
            log_error(std::format("createImageUnique failed: {}", vk_result_to_string(imgRes)), loc);
            throw std::runtime_error("Failed to create Vulkan texture image (" + vk_result_to_string(imgRes) + ").");
        }

        textureImage = std::move(img);

        const vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*textureImage);

        const std::uint32_t memType =
            findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (memType == UINT32_MAX)
        {
            log_error(std::format("findMemoryType failed for DeviceLocal. bits=0x{:08X}",
                static_cast<std::uint32_t>(memReq.memoryTypeBits)), loc);
            throw std::runtime_error("No suitable memory type for texture image (DeviceLocal).");
        }

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memType;

        auto [memRes, mem] = device->allocateMemoryUnique(allocInfo);
        if (memRes != vk::Result::eSuccess)
        {
            log_error(std::format("allocateMemoryUnique failed: {}", vk_result_to_string(memRes)), loc);
            throw std::runtime_error("Failed to allocate texture image memory (" + vk_result_to_string(memRes) + ").");
        }

        textureImageMemory = std::move(mem);

        const vk::Result bindRes = device->bindImageMemory(*textureImage, *textureImageMemory, 0);
        if (bindRes != vk::Result::eSuccess)
        {
            log_error(std::format("bindImageMemory failed: {}", vk_result_to_string(bindRes)), loc);
            throw std::runtime_error("Failed to bind texture image memory (" + vk_result_to_string(bindRes) + ").");
        }

        transitionImageLayout(
            *textureImage,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal);

        copyBufferToImage(
            *stagingBuffer,
            *textureImage,
            static_cast<std::uint32_t>(texture.width),
            static_cast<std::uint32_t>(texture.height));

        transitionImageLayout(
            *textureImage,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        log_info("upload complete", loc);
    }

    void Application::destroyArcadeRenderTarget() noexcept
    {
        arcadeDescriptorSets.clear();
        arcadeDescriptorPool.reset();
        arcadeRenderFramebuffer.reset();
        arcadeRenderSampler.reset();
        arcadeRenderImageView.reset();
        arcadeRenderImage.reset();
        arcadeRenderImageMemory.reset();
        arcadeRenderPass.reset();
        arcadeRenderExtent = vk::Extent2D{};
        arcadePreviewFrame = 0u;
    }

    void Application::createArcadeRenderTarget()
    {
        destroyArcadeRenderTarget();

        const auto desc = epochengine::render_arcade::make_screen_render_texture_desc();
        constexpr std::uint32_t kMaximumArcadeTargetAxis = 2048u;
        if (desc.width == 0u || desc.height == 0u
            || desc.width > kMaximumArcadeTargetAxis
            || desc.height > kMaximumArcadeTargetAxis)
        {
            throw std::runtime_error("[ Vulkan ] - Invalid bounded Engine Arcade render-target extent.");
        }

        arcadeRenderExtent = vk::Extent2D{ desc.width, desc.height };
        constexpr vk::Format kArcadeFormat = vk::Format::eR8G8B8A8Unorm;

        try
        {
            vk::AttachmentDescription colorAttachment{};
            colorAttachment.format = kArcadeFormat;
            colorAttachment.samples = vk::SampleCountFlagBits::e1;
            colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
            colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
            colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
            colorAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentReference colorReference{};
            colorReference.attachment = 0u;
            colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::SubpassDescription subpass{};
            subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass.colorAttachmentCount = 1u;
            subpass.pColorAttachments = &colorReference;

            std::array<vk::SubpassDependency, 2> dependencies{};
            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0u;
            dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
            dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependencies[0].srcAccessMask = {};
            dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
            dependencies[1].srcSubpass = 0u;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
            dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
            dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

            vk::RenderPassCreateInfo renderPassInfo{};
            renderPassInfo.attachmentCount = 1u;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1u;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = dependencies.data();

            auto [renderPassResult, renderPass] = device->createRenderPassUnique(renderPassInfo);
            if (renderPassResult != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - Failed to create Engine Arcade render pass.");
            arcadeRenderPass = std::move(renderPass);

            vk::ImageCreateInfo imageInfo{};
            imageInfo.imageType = vk::ImageType::e2D;
            imageInfo.format = kArcadeFormat;
            imageInfo.extent = vk::Extent3D{ desc.width, desc.height, 1u };
            imageInfo.mipLevels = 1u;
            imageInfo.arrayLayers = 1u;
            imageInfo.samples = vk::SampleCountFlagBits::e1;
            imageInfo.tiling = vk::ImageTiling::eOptimal;
            imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment
                | vk::ImageUsageFlagBits::eSampled;
            imageInfo.sharingMode = vk::SharingMode::eExclusive;
            imageInfo.initialLayout = vk::ImageLayout::eUndefined;

            auto [imageResult, image] = device->createImageUnique(imageInfo);
            if (imageResult != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - Failed to create Engine Arcade color image.");
            arcadeRenderImage = std::move(image);

            const vk::MemoryRequirements requirements =
                device->getImageMemoryRequirements(*arcadeRenderImage);
            vk::MemoryAllocateInfo allocationInfo{};
            allocationInfo.allocationSize = requirements.size;
            allocationInfo.memoryTypeIndex = findMemoryType(
                requirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

            auto [memoryResult, memory] = device->allocateMemoryUnique(allocationInfo);
            if (memoryResult != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - Failed to allocate Engine Arcade image memory.");
            arcadeRenderImageMemory = std::move(memory);

            if (device->bindImageMemory(
                    *arcadeRenderImage,
                    *arcadeRenderImageMemory,
                    0u) != vk::Result::eSuccess)
            {
                throw std::runtime_error("[ Vulkan ] - Failed to bind Engine Arcade image memory.");
            }

            arcadeRenderImageView = createImageViewUnique(
                *arcadeRenderImage,
                kArcadeFormat,
                vk::ImageAspectFlagBits::eColor);

            vk::SamplerCreateInfo samplerInfo{};
            samplerInfo.magFilter = vk::Filter::eNearest;
            samplerInfo.minFilter = vk::Filter::eNearest;
            samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
            samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;
            samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            auto [samplerResult, sampler] = device->createSamplerUnique(samplerInfo);
            if (samplerResult != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - Failed to create Engine Arcade sampler.");
            arcadeRenderSampler = std::move(sampler);

            const vk::ImageView attachment = *arcadeRenderImageView;
            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = *arcadeRenderPass;
            framebufferInfo.attachmentCount = 1u;
            framebufferInfo.pAttachments = &attachment;
            framebufferInfo.width = arcadeRenderExtent.width;
            framebufferInfo.height = arcadeRenderExtent.height;
            framebufferInfo.layers = 1u;

            auto [framebufferResult, framebuffer] =
                device->createFramebufferUnique(framebufferInfo);
            if (framebufferResult != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - Failed to create Engine Arcade framebuffer.");
            arcadeRenderFramebuffer = std::move(framebuffer);
        }
        catch (...)
        {
            destroyArcadeRenderTarget();
            throw;
        }
    }

    void Application::ensure_gui_atlas(const TextureAtlas& atlas)
    {
        if (!device)
            return;

        if (!activeGuiContext)
            activeGuiContext = bound_context();
        if (!activeGuiContext)
            return;

        auto& guiState = gui_state_for_context(activeGuiContext);
        auto& entry = guiState.guiAtlases[&atlas];
        if (entry.version == atlas.version && entry.image)
            return;

        if (atlas.width == 0 || atlas.height == 0 || atlas.pixel_data.empty())
            return;

        entry.image.reset();
        entry.memory.reset();
        entry.view.reset();
        entry.sampler.reset();
        entry.descriptorPool.reset();
        entry.descriptorSets.clear();

        const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(atlas.pixel_data.size());

        vk::UniqueBuffer stagingBuffer;
        vk::UniqueDeviceMemory stagingMemory;
        std::tie(stagingBuffer, stagingMemory) = createBuffer(
            imageSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

        auto [mapRes, mapped] = device->mapMemory(*stagingMemory, 0, imageSize);
        if (mapRes != vk::Result::eSuccess || !mapped)
            throw std::runtime_error("[ Vulkan ] - Failed to map GUI atlas staging buffer.");

        std::memcpy(mapped, atlas.pixel_data.data(), static_cast<std::size_t>(imageSize));
        device->unmapMemory(*stagingMemory);

        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = vk::Format::eR8G8B8A8Srgb;
        imageInfo.extent = vk::Extent3D{ atlas.width, atlas.height, 1u };
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;

        auto [imgRes, img] = device->createImageUnique(imageInfo);
        if (imgRes != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - Failed to create GUI atlas image.");

        entry.image = std::move(img);

        const vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*entry.image);
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto [memRes, mem] = device->allocateMemoryUnique(allocInfo);
        if (memRes != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - Failed to allocate GUI atlas memory.");

        entry.memory = std::move(mem);

        const vk::Result bindRes = device->bindImageMemory(*entry.image, *entry.memory, 0);
        if (bindRes != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - Failed to bind GUI atlas memory.");

        transitionImageLayout(
            *entry.image,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal);

        copyBufferToImage(
            *stagingBuffer,
            *entry.image,
            atlas.width,
            atlas.height);

        transitionImageLayout(
            *entry.image,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        entry.view = createImageViewUnique(
            *entry.image,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageAspectFlagBits::eColor);

        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.flags = {};
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = vk::CompareOp::eAlways;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        auto [sRes, sampler] = device->createSamplerUnique(samplerInfo);
        if (sRes != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - Failed to create GUI atlas sampler.");

        entry.sampler = std::move(sampler);

        if (guiState.guiUniformBuffers.empty())
            createGuiUniformBuffers();

        const std::uint32_t count = static_cast<std::uint32_t>(swapChainImages.size());

        std::array<vk::DescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
        poolSizes[0].descriptorCount = count;
        poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[1].descriptorCount = count;

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolInfo.maxSets = count;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        auto [poolRes, pool] = device->createDescriptorPoolUnique(poolInfo);
        if (poolRes != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - Failed to create GUI atlas descriptor pool.");

        entry.descriptorPool = std::move(pool);

        std::vector<vk::DescriptorSetLayout> layouts(count, *descriptorSetLayout);

        vk::DescriptorSetAllocateInfo descriptorAllocInfo{};
        descriptorAllocInfo.descriptorPool = *entry.descriptorPool;
        descriptorAllocInfo.descriptorSetCount = count;
        descriptorAllocInfo.pSetLayouts = layouts.data();

        auto [setRes, sets] = device->allocateDescriptorSetsUnique(descriptorAllocInfo);
        if (setRes != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - Failed to allocate GUI atlas descriptor sets.");

        entry.descriptorSets = std::move(sets);

        for (std::size_t i = 0; i < count; ++i)
        {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = *guiState.guiUniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            vk::DescriptorImageInfo atlasImageInfo{};
            atlasImageInfo.sampler = *entry.sampler;
            atlasImageInfo.imageView = *entry.view;
            atlasImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            std::array<vk::WriteDescriptorSet, 2> writes{};
            writes[0].dstSet = *entry.descriptorSets[i];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            writes[0].pBufferInfo = &bufferInfo;

            writes[1].dstSet = *entry.descriptorSets[i];
            writes[1].dstBinding = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            writes[1].pImageInfo = &atlasImageInfo;

            device->updateDescriptorSets(writes, {});
        }

        entry.version = atlas.version;
        entry.width = atlas.width;
        entry.height = atlas.height;

#if EPOCH_ENABLE_BACKEND_UPLOAD_CONFIRMATION_LOGS && EPOCH_ENABLE_VULKAN_CONFIRMATION_LOGS
        log_info(
            std::format(
                "uploaded gui atlas '{}' ({}x{}, version={})",
                atlas.name,
                entry.width,
                entry.height,
                entry.version),
            std::source_location::current());
#endif
    }
} // namespace epochengine::vulkancontext

namespace epochengine::vulkantextures
{
    void ensure_uploaded(const epochengine::TextureAtlas& atlas)
    {
        if (!epochengine::vulkancontext::has_vulkan_apps())
            return;

        (void)atlas;
    }
}
