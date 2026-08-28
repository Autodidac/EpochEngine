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

#include "core.format_text.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/renderers/vulkan/vulkan.context_shared.hpp>
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

export module vulkan.context:texture;

import :shared_vk;
import core.context;
import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_limits;
import render.canvas2d_runtime;
import render.device;
import core.logger;
import core.path;
import epoch.cli;
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

            log_error(epochengine::format_text("Failed to load texture image. Tried paths:{}", tried), loc);
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
                log_error(epochengine::format_text("a_loadImage failed for path='{}': {}", texturePathUtf8, ex.what()), loc);
                throw std::runtime_error("Failed to load texture image '" + texturePathUtf8 + "': " + ex.what());
            }
        }();

        ensure_rgba8(texture);

        const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texture.pixels.size());

        log_info(epochengine::format_text("path='{}'", texturePathUtf8), loc);
        log_info(epochengine::format_text("size={}x{} bytes={}",
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

        log_info(epochengine::format_text("staging-copy bytes={}", static_cast<std::uint64_t>(imageSize)), loc);

        auto [mapRes, mapped] = device->mapMemory(*stagingMemory, 0, imageSize);
        if (mapRes != vk::Result::eSuccess)
        {
            log_error(epochengine::format_text("mapMemory failed: {}", vk_result_to_string(mapRes)), loc);
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
            log_error(epochengine::format_text("createImageUnique failed: {}", vk_result_to_string(imgRes)), loc);
            throw std::runtime_error("Failed to create Vulkan texture image (" + vk_result_to_string(imgRes) + ").");
        }

        textureImage = std::move(img);

        const vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*textureImage);

        const std::uint32_t memType =
            findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (memType == UINT32_MAX)
        {
            log_error(epochengine::format_text("findMemoryType failed for DeviceLocal. bits=0x{:08X}",
                static_cast<std::uint32_t>(memReq.memoryTypeBits)), loc);
            throw std::runtime_error("No suitable memory type for texture image (DeviceLocal).");
        }

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memType;

        auto [memRes, mem] = device->allocateMemoryUnique(allocInfo);
        if (memRes != vk::Result::eSuccess)
        {
            log_error(epochengine::format_text("allocateMemoryUnique failed: {}", vk_result_to_string(memRes)), loc);
            throw std::runtime_error("Failed to allocate texture image memory (" + vk_result_to_string(memRes) + ").");
        }

        textureImageMemory = std::move(mem);

        const vk::Result bindRes = device->bindImageMemory(*textureImage, *textureImageMemory, 0);
        if (bindRes != vk::Result::eSuccess)
        {
            log_error(epochengine::format_text("bindImageMemory failed: {}", vk_result_to_string(bindRes)), loc);
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
            epochengine::format_text(
                "uploaded gui atlas '{}' ({}x{}, version={})",
                atlas.name,
                entry.width,
                entry.height,
                entry.version),
            std::source_location::current());
#endif
    }

    void Application::resetCanvas2DState(Canvas2DContextState& state) noexcept
    {
        state.ready = false;
        state.pipeline.reset();
        state.nearestDescriptorSets.clear();
        state.linearDescriptorSets.clear();
        state.descriptorPool.reset();
        state.nearestSampler.reset();
        state.linearSampler.reset();
        state.imageView.reset();
        state.image.reset();
        state.imageMemory.reset();
        state.indexBuffer.reset();
        state.indexBufferMemory.reset();
        state.vertexBuffer.reset();
        state.vertexBufferMemory.reset();
        state.session.reset();
        state.surface = {};
        state.destination = {};
        state.visibleCanvas = {};
        state.letterboxColor = {};
        state.filter = FilterMode::nearest;
        state.imageExtent = {};
        state.contentHash = 0u;
        state.canvasHash = 0u;
        state.frameSequence = 0u;
        state.clearLetterbox = false;
        state.refusalLogged = false;
    }

    bool Application::prepareCanvas2D() noexcept {
      const auto *ctx = bound_context();
      if (!ctx || !device || !physicalDevice || !renderPass || !pipelineLayout)
        return false;

      Canvas2DContextState *statePtr = nullptr;

      try {
        auto &guiState = gui_state_for_context(ctx);
        auto &state = guiState.canvas2d;
        statePtr = &state;
        state.ready = false;

        const auto retireInactiveState = [&]() {
          if (state.image || state.vertexBuffer || state.indexBuffer) {
            if (graphicsQueue.waitIdle() != vk::Result::eSuccess)
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D retirement synchronization failed.");
            resetCanvas2DState(state);
          } else {
            state.session.reset();
            state.refusalLogged = false;
          }
        };

        const auto sceneViewport = ctx->scene_viewport();
        if (ctx->scene_preview_mode() != core::ScenePreviewMode::Editor ||
            !sceneViewport.valid() || sceneViewport.x < 0 ||
            sceneViewport.y < 0) {
          retireInactiveState();
          return false;
        }

        const std::uint64_t viewportRight =
            static_cast<std::uint64_t>(sceneViewport.x) +
            static_cast<std::uint32_t>(sceneViewport.width);
        const std::uint64_t viewportBottom =
            static_cast<std::uint64_t>(sceneViewport.y) +
            static_cast<std::uint32_t>(sceneViewport.height);
        if (viewportRight > swapChainExtent.width ||
            viewportBottom > swapChainExtent.height) {
          return false;
        }

        const canvas2d::limits::NativeExecutionLimits executionLimits =
            canvas2d::limits::for_backend(RendererBackendKind::vulkan);
        if (!executionLimits.valid())
          return false;
        const std::uint64_t maximumUploadBytes =
            executionLimits.maximum_native_canvas_bytes;
        canvas2d::cpu::RasterLimits rasterLimits = executionLimits.raster;
        canvas2d::CanvasLimits canvasLimits = executionLimits.canvas;
        const std::uint32_t deviceDimension =
            physicalDevice.getProperties().limits.maxImageDimension2D;
        const std::uint32_t boundedDimension =
            (std::min)(std::uint32_t{16'384}, deviceDimension);
        if (boundedDimension == 0u)
          return false;
        canvasLimits.maximum_canvas_dimension =
            (std::min)(canvasLimits.maximum_canvas_dimension, boundedDimension);
        canvasLimits.maximum_surface_dimension =
            (std::min)(canvasLimits.maximum_surface_dimension, boundedDimension);
        canvasLimits.maximum_canvas_pixels = rasterLimits.maximum_canvas_pixels;
        canvasLimits.maximum_surface_pixels =
            rasterLimits.maximum_presentation_pixels;

        const canvas2d::runtime::PreparedSceneView prepared =
            state.session.prepare(
                ctx,
                {static_cast<std::uint32_t>(sceneViewport.width),
                 static_cast<std::uint32_t>(sceneViewport.height)},
                canvasLimits, rasterLimits);
        if (prepared.code == canvas2d::runtime::PrepareCode::missing_scene) {
          retireInactiveState();
          return false;
        }
        if (!prepared || !prepared.frame || !prepared.raster) {
          if (!state.refusalLogged) {
            log_error(epochengine::format_text(
                "Canvas2D prepare refused: {}",
                canvas2d::runtime::prepare_code_name(prepared.code)));
            state.refusalLogged = true;
          }
          return false;
        }

        const canvas2d::cpu::Image &image = prepared.raster->canvas;
        const canvas2d::FinalComposePlan &compose = prepared.frame->compose;
        const std::uint64_t uploadBytes =
            static_cast<std::uint64_t>(image.pixels.size()) *
            sizeof(canvas2d::cpu::Rgba8);
        if (!image.valid() || prepared.raster->canvas_hash == 0u ||
            uploadBytes == 0u || uploadBytes > maximumUploadBytes ||
            image.extent.width > boundedDimension ||
            image.extent.height > boundedDimension ||
            compose.viewport.output_surface.width !=
                static_cast<std::uint32_t>(sceneViewport.width) ||
            compose.viewport.output_surface.height !=
                static_cast<std::uint32_t>(sceneViewport.height) ||
            compose.viewport.clipped_destination.empty()) {
          if (!state.refusalLogged) {
            log_error(
                "Canvas2D prepare refused an invalid or oversized upload.");
            state.refusalLogged = true;
          }
          return false;
        }

        const canvas2d::CanvasExtent previousImageExtent = state.imageExtent;
        bool synchronized = false;
        const bool nativeResourcesMissing =
            state.imageExtent != image.extent || !state.image ||
            !state.imageView ||
            state.nearestDescriptorSets.size() != swapChainImages.size() ||
            state.linearDescriptorSets.size() != swapChainImages.size();
        const bool imageReplacement = nativeResourcesMissing ||
            state.canvasHash != prepared.raster->canvas_hash;
        const bool uploadChanged = imageReplacement;
        if (uploadChanged) {
          vk::UniqueBuffer stagingBuffer{};
          vk::UniqueDeviceMemory stagingMemory{};
          std::tie(stagingBuffer, stagingMemory) =
              createBuffer(static_cast<vk::DeviceSize>(uploadBytes),
                           vk::BufferUsageFlagBits::eTransferSrc,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent);
          auto [mapResult, mapped] = device->mapMemory(
              *stagingMemory, 0u, static_cast<vk::DeviceSize>(uploadBytes));
          if (mapResult != vk::Result::eSuccess || !mapped)
            throw std::runtime_error(
                "[ Vulkan ] - Canvas2D staging map failed.");
          std::memcpy(mapped, image.pixels.data(),
                      static_cast<std::size_t>(uploadBytes));
          device->unmapMemory(*stagingMemory);

          vk::UniqueImage nextImage{};
          vk::UniqueDeviceMemory nextMemory{};
          vk::Image uploadImage{};
          if (imageReplacement) {
            vk::ImageCreateInfo imageInfo{};
            imageInfo.imageType = vk::ImageType::e2D;
            imageInfo.format = vk::Format::eR8G8B8A8Unorm;
            imageInfo.extent =
                vk::Extent3D{image.extent.width, image.extent.height, 1u};
            imageInfo.mipLevels = 1u;
            imageInfo.arrayLayers = 1u;
            imageInfo.samples = vk::SampleCountFlagBits::e1;
            imageInfo.tiling = vk::ImageTiling::eOptimal;
            imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst |
                              vk::ImageUsageFlagBits::eSampled;
            imageInfo.sharingMode = vk::SharingMode::eExclusive;
            imageInfo.initialLayout = vk::ImageLayout::eUndefined;
            auto imageResult = device->createImageUnique(imageInfo);
            if (imageResult.result != vk::Result::eSuccess ||
                !imageResult.value)
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D image creation failed.");
            nextImage = std::move(imageResult.value);

            const vk::MemoryRequirements memoryRequirements =
                device->getImageMemoryRequirements(*nextImage);
            vk::MemoryAllocateInfo allocationInfo{};
            allocationInfo.allocationSize = memoryRequirements.size;
            allocationInfo.memoryTypeIndex =
                findMemoryType(memoryRequirements.memoryTypeBits,
                               vk::MemoryPropertyFlagBits::eDeviceLocal);
            auto memoryResult = device->allocateMemoryUnique(allocationInfo);
            if (memoryResult.result != vk::Result::eSuccess ||
                !memoryResult.value)
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D image allocation failed.");
            nextMemory = std::move(memoryResult.value);
            if (device->bindImageMemory(*nextImage, *nextMemory, 0u) !=
                vk::Result::eSuccess) {
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D image binding failed.");
            }
            uploadImage = *nextImage;
          } else {
            uploadImage = *state.image;
          }

          vk::UniqueCommandBuffer upload = beginSingleTimeCommands();
          vk::ImageMemoryBarrier toTransfer{};
          toTransfer.oldLayout = imageReplacement
                                     ? vk::ImageLayout::eUndefined
                                     : vk::ImageLayout::eShaderReadOnlyOptimal;
          toTransfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
          toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          toTransfer.image = uploadImage;
          toTransfer.subresourceRange = vk::ImageSubresourceRange{
              vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};
          toTransfer.srcAccessMask = imageReplacement
                                         ? vk::AccessFlags{}
                                         : vk::AccessFlagBits::eShaderRead;
          toTransfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
          upload->pipelineBarrier(
              imageReplacement ? vk::PipelineStageFlagBits::eTopOfPipe
                               : vk::PipelineStageFlagBits::eFragmentShader,
              vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr,
              toTransfer);

          vk::BufferImageCopy copy{};
          copy.imageSubresource = vk::ImageSubresourceLayers{
              vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u};
          copy.imageExtent =
              vk::Extent3D{image.extent.width, image.extent.height, 1u};
          upload->copyBufferToImage(*stagingBuffer, uploadImage,
                                    vk::ImageLayout::eTransferDstOptimal, copy);

          vk::ImageMemoryBarrier toSample{};
          toSample.oldLayout = vk::ImageLayout::eTransferDstOptimal;
          toSample.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
          toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          toSample.image = uploadImage;
          toSample.subresourceRange = toTransfer.subresourceRange;
          toSample.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
          toSample.dstAccessMask = vk::AccessFlagBits::eShaderRead;
          upload->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eFragmentShader,
                                  {}, nullptr, nullptr, toSample);
          endSingleTimeCommands(upload);
          synchronized = true;

          if (imageReplacement) {
            vk::UniqueImageView nextView =
                createImageViewUnique(*nextImage, vk::Format::eR8G8B8A8Unorm,
                                      vk::ImageAspectFlagBits::eColor);
            const auto createSampler = [&](vk::Filter filter) {
              vk::SamplerCreateInfo samplerInfo{};
              samplerInfo.magFilter = filter;
              samplerInfo.minFilter = filter;
              samplerInfo.mipmapMode = filter == vk::Filter::eNearest
                                           ? vk::SamplerMipmapMode::eNearest
                                           : vk::SamplerMipmapMode::eLinear;
              samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
              samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
              samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
              samplerInfo.maxAnisotropy = 1.0f;
              samplerInfo.compareOp = vk::CompareOp::eAlways;
              samplerInfo.maxLod = 0.0f;
              samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
              auto result = device->createSamplerUnique(samplerInfo);
              if (result.result != vk::Result::eSuccess || !result.value)
                throw std::runtime_error(
                    "[ Vulkan ] - Canvas2D sampler creation failed.");
              return std::move(result.value);
            };
            vk::UniqueSampler nextNearest = createSampler(vk::Filter::eNearest);
            vk::UniqueSampler nextLinear = createSampler(vk::Filter::eLinear);

            const std::uint32_t descriptorCount =
                static_cast<std::uint32_t>(swapChainImages.size());
            if (descriptorCount == 0u ||
                guiState.guiUniformBuffers.size() != descriptorCount) {
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D descriptors require GUI uniforms.");
            }
            std::array<vk::DescriptorPoolSize, 2> poolSizes{};
            poolSizes[0] = {vk::DescriptorType::eUniformBuffer,
                            descriptorCount * 2u};
            poolSizes[1] = {vk::DescriptorType::eCombinedImageSampler,
                            descriptorCount * 2u};
            vk::DescriptorPoolCreateInfo poolInfo{};
            poolInfo.flags =
                vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
            poolInfo.maxSets = descriptorCount * 2u;
            poolInfo.poolSizeCount =
                static_cast<std::uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            auto poolResult = device->createDescriptorPoolUnique(poolInfo);
            if (poolResult.result != vk::Result::eSuccess || !poolResult.value)
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D descriptor pool creation failed.");
            vk::UniqueDescriptorPool nextPool = std::move(poolResult.value);

            std::vector<vk::DescriptorSetLayout> layouts(descriptorCount,
                                                         *descriptorSetLayout);
            const auto allocateSets = [&]() {
              vk::DescriptorSetAllocateInfo descriptorInfo{};
              descriptorInfo.descriptorPool = *nextPool;
              descriptorInfo.descriptorSetCount = descriptorCount;
              descriptorInfo.pSetLayouts = layouts.data();
              auto result =
                  device->allocateDescriptorSetsUnique(descriptorInfo);
              if (result.result != vk::Result::eSuccess)
                throw std::runtime_error(
                    "[ Vulkan ] - Canvas2D descriptor allocation failed.");
              return std::move(result.value);
            };
            auto nextNearestSets = allocateSets();
            auto nextLinearSets = allocateSets();

            const auto writeSets =
                [&](std::vector<vk::UniqueDescriptorSet> &sets,
                    vk::Sampler sampler) {
                  for (std::uint32_t index = 0u; index < descriptorCount;
                       ++index) {
                    vk::DescriptorBufferInfo bufferInfo{
                        *guiState.guiUniformBuffers[index], 0u,
                        sizeof(UniformBufferObject)};
                    vk::DescriptorImageInfo sampledImage{
                        sampler, *nextView,
                        vk::ImageLayout::eShaderReadOnlyOptimal};
                    std::array<vk::WriteDescriptorSet, 2> writes{};
                    writes[0].dstSet = *sets[index];
                    writes[0].dstBinding = 0u;
                    writes[0].descriptorCount = 1u;
                    writes[0].descriptorType =
                        vk::DescriptorType::eUniformBuffer;
                    writes[0].pBufferInfo = &bufferInfo;
                    writes[1].dstSet = *sets[index];
                    writes[1].dstBinding = 1u;
                    writes[1].descriptorCount = 1u;
                    writes[1].descriptorType =
                        vk::DescriptorType::eCombinedImageSampler;
                    writes[1].pImageInfo = &sampledImage;
                    device->updateDescriptorSets(writes, {});
                  }
                };
            writeSets(nextNearestSets, *nextNearest);
            writeSets(nextLinearSets, *nextLinear);

            state.nearestDescriptorSets.clear();
            state.linearDescriptorSets.clear();
            state.descriptorPool.reset();
            state.nearestSampler.reset();
            state.linearSampler.reset();
            state.imageView.reset();
            state.image.reset();
            state.imageMemory.reset();

            state.image = std::move(nextImage);
            state.imageMemory = std::move(nextMemory);
            state.imageView = std::move(nextView);
            state.nearestSampler = std::move(nextNearest);
            state.linearSampler = std::move(nextLinear);
            state.descriptorPool = std::move(nextPool);
            state.nearestDescriptorSets = std::move(nextNearestSets);
            state.linearDescriptorSets = std::move(nextLinearSets);
          }
          state.imageExtent = image.extent;
          state.canvasHash = prepared.raster->canvas_hash;
        }

        createCanvas2DPipeline(state);

        const canvas2d::RectI nextSurface{
            sceneViewport.x, sceneViewport.y,
            static_cast<std::uint32_t>(sceneViewport.width),
            static_cast<std::uint32_t>(sceneViewport.height)};
        const canvas2d::RectI nextDestination{
            nextSurface.x + compose.viewport.clipped_destination.x,
            nextSurface.y + compose.viewport.clipped_destination.y,
            compose.viewport.clipped_destination.width,
            compose.viewport.clipped_destination.height};
        const bool geometryChanged =
            !state.vertexBuffer || !state.indexBuffer ||
            state.surface != nextSurface ||
            state.destination != nextDestination ||
            state.visibleCanvas != compose.viewport.visible_canvas ||
            previousImageExtent != image.extent;
        if (geometryChanged) {
          if (!synchronized && (state.vertexBuffer || state.image)) {
            if (graphicsQueue.waitIdle() != vk::Result::eSuccess)
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D geometry synchronization failed.");
            synchronized = true;
          }
          if (!state.indexBuffer) {
            auto [nextIndexBuffer, nextIndexMemory] =
                createBuffer(sizeof(std::uint32_t) * 6u,
                             vk::BufferUsageFlagBits::eIndexBuffer,
                             vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent);
            constexpr std::array<std::uint32_t, 6> indices{0u, 1u, 2u,
                                                           2u, 3u, 0u};
            auto [indexMapResult, indexMap] =
                device->mapMemory(*nextIndexMemory, 0u, sizeof(indices));
            if (indexMapResult != vk::Result::eSuccess || !indexMap)
              throw std::runtime_error(
                  "[ Vulkan ] - Canvas2D index map failed.");
            std::memcpy(indexMap, indices.data(), sizeof(indices));
            device->unmapMemory(*nextIndexMemory);
            state.indexBuffer = std::move(nextIndexBuffer);
            state.indexBufferMemory = std::move(nextIndexMemory);
          }

          const float x0 = static_cast<float>(nextDestination.x);
          const float y0 = static_cast<float>(nextDestination.y);
          const float x1 = x0 + static_cast<float>(nextDestination.width);
          const float y1 = y0 + static_cast<float>(nextDestination.height);
          const float u0 = compose.viewport.visible_canvas.x /
                           static_cast<float>(image.extent.width);
          const float v0 = compose.viewport.visible_canvas.y /
                           static_cast<float>(image.extent.height);
          const float u1 = (compose.viewport.visible_canvas.x +
                            compose.viewport.visible_canvas.width) /
                           static_cast<float>(image.extent.width);
          const float v1 = (compose.viewport.visible_canvas.y +
                            compose.viewport.visible_canvas.height) /
                           static_cast<float>(image.extent.height);
          const std::array<Vertex, 4> vertices{
              {{{x0, y0, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v0}},
               {{x1, y0, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v0}},
               {{x1, y1, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v1}},
               {{x0, y1, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v1}}}};
          vk::UniqueBuffer nextVertexBuffer{};
          vk::UniqueDeviceMemory nextVertexMemory{};
          vk::DeviceMemory vertexMemory{};
          if (!state.vertexBuffer) {
            std::tie(nextVertexBuffer, nextVertexMemory) = createBuffer(
                sizeof(vertices), vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible |
                    vk::MemoryPropertyFlagBits::eHostCoherent);
            vertexMemory = *nextVertexMemory;
          } else {
            vertexMemory = *state.vertexBufferMemory;
          }
          auto [vertexMapResult, vertexMap] =
              device->mapMemory(vertexMemory, 0u, sizeof(vertices));
          if (vertexMapResult != vk::Result::eSuccess || !vertexMap)
            throw std::runtime_error(
                "[ Vulkan ] - Canvas2D vertex map failed.");
          std::memcpy(vertexMap, vertices.data(), sizeof(vertices));
          device->unmapMemory(vertexMemory);
          if (nextVertexBuffer) {
            state.vertexBuffer = std::move(nextVertexBuffer);
            state.vertexBufferMemory = std::move(nextVertexMemory);
          }
        }

        state.surface = nextSurface;
        state.destination = nextDestination;
        state.visibleCanvas = compose.viewport.visible_canvas;
        state.letterboxColor = compose.letterbox_color;
        state.filter = compose.presentation_filter;
        state.contentHash = prepared.content_hash;
        state.frameSequence = prepared.frame->frame_sequence;
        state.clearLetterbox = compose.clear_letterbox;
        state.ready = state.pipeline && state.vertexBuffer &&
                      state.indexBuffer && state.imageView &&
                      state.frameSequence != 0u && state.contentHash != 0u;
        state.refusalLogged = false;
        return state.ready;
      } catch (const std::exception &error) {
        if (!statePtr || !statePtr->refusalLogged) {
          log_error(epochengine::format_text(
              "Canvas2D Vulkan prepare failed: {}", error.what()));
          if (statePtr)
            statePtr->refusalLogged = true;
        }
        if (statePtr)
          statePtr->ready = false;
        return false;
      } catch (...) {
        if (!statePtr || !statePtr->refusalLogged) {
          log_error("Canvas2D Vulkan prepare failed with an unknown error.");
          if (statePtr)
            statePtr->refusalLogged = true;
        }
        if (statePtr)
          statePtr->ready = false;
        return false;
      }
    }
    } // namespace epochengine::vulkancontext

    namespace epochengine::vulkantextures {
    void ensure_uploaded(const epochengine::TextureAtlas& atlas)
    {
        if (!epochengine::vulkancontext::has_vulkan_apps())
            return;

        (void)atlas;
    }
    } // namespace epochengine::vulkantextures
