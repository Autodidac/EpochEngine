// ============================================================================
// src/acontext.vulkan.context-texture.cpp
// acontext.vulkan.context:texture
// Vulkan texture upload (PPM -> RGBA8 -> staging buffer -> vkImage)
// Uses aengine.core.logger (per-system + console) via logger::get(...).log(...)
// (Does not require logger::infof/errorf exports.)
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>
#include <vulkan/vulkan.hpp>

module acontext.vulkan.context:texture;

import <array>;
import <cstdint>;
import <cstring>;
import <filesystem>;
import <format>;
import <source_location>;
import <sstream>;
import <stdexcept>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import aengine.core.logger;
import autility.string.converter;
import aimage.loader;
import aatlas.texture;
import :shared_vk;

namespace almondnamespace::vulkancontext
{
    namespace
    {
        constexpr std::string_view kLogSys = "Vulkan.Texture";

        inline void log_info(std::string_view msg,
            const std::source_location& loc = std::source_location::current())
        {
            almondnamespace::logger::get(kLogSys).log(
                almondnamespace::logger::LogLevel::INFO, msg, loc);
        }

        inline void log_warn(std::string_view msg,
            const std::source_location& loc = std::source_location::current())
        {
            almondnamespace::logger::get(kLogSys).log(
                almondnamespace::logger::LogLevel::WARN, msg, loc);
        }

        inline void log_error(std::string_view msg,
            const std::source_location& loc = std::source_location::current())
        {
            almondnamespace::logger::get(kLogSys).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR, msg, loc);
        }

        std::filesystem::path resolve_texture_path(const std::source_location& loc)
        {
            namespace fs = std::filesystem;

            const fs::path target = "texture.ppm";
            const std::array<fs::path, 4> candidates = {
                target,
                fs::path("assets") / "vulkan" / target,
                fs::path("AlmondShell") / "assets" / "vulkan" / target,
                fs::path("..") / "AlmondShell" / "assets" / "vulkan" / target,
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
                tried += almondnamespace::text::path_to_utf8(fs::absolute(p).lexically_normal());
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

    void Application::createTextureImage()
    {
        const auto loc = std::source_location::current();

        const std::filesystem::path texturePath = resolve_texture_path(loc);
        const std::string texturePathUtf8 = almondnamespace::text::path_to_utf8(texturePath);

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

        // Staging buffer
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

        // GPU image
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

    void Application::ensure_gui_atlas(const TextureAtlas& atlas)
    {
        if (!device)
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
            throw std::runtime_error("[Vulkan] Failed to map GUI atlas staging buffer.");

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
            throw std::runtime_error("[Vulkan] Failed to create GUI atlas image.");

        entry.image = std::move(img);

        const vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*entry.image);
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto [memRes, mem] = device->allocateMemoryUnique(allocInfo);
        if (memRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to allocate GUI atlas memory.");

        entry.memory = std::move(mem);

        const vk::Result bindRes = device->bindImageMemory(*entry.image, *entry.memory, 0);
        if (bindRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to bind GUI atlas memory.");

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
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
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
            throw std::runtime_error("[Vulkan] Failed to create GUI atlas sampler.");

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
            throw std::runtime_error("[Vulkan] Failed to create GUI atlas descriptor pool.");

        entry.descriptorPool = std::move(pool);

        std::vector<vk::DescriptorSetLayout> layouts(count, *descriptorSetLayout);

        vk::DescriptorSetAllocateInfo descriptorAllocInfo{};
        descriptorAllocInfo.descriptorPool = *entry.descriptorPool;
        descriptorAllocInfo.descriptorSetCount = count;
        descriptorAllocInfo.pSetLayouts = layouts.data();

        auto [setRes, sets] = device->allocateDescriptorSetsUnique(descriptorAllocInfo);
        if (setRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to allocate GUI atlas descriptor sets.");

        entry.descriptorSets = std::move(sets);

        for (std::size_t i = 0; i < count; ++i)
        {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = *guiState.guiUniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            vk::DescriptorImageInfo imageInfo{};
            imageInfo.sampler = *entry.sampler;
            imageInfo.imageView = *entry.view;
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

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
            writes[1].pImageInfo = &imageInfo;

            device->updateDescriptorSets(writes, {});
        }

        entry.version = atlas.version;
        entry.width = atlas.width;
        entry.height = atlas.height;
    }
} // namespace almondnamespace::vulkancontext

namespace almondnamespace::vulkantextures
{
    void ensure_uploaded(const almondnamespace::TextureAtlas& atlas)
    {
        almondnamespace::vulkancontext::vulkan_app().ensure_gui_atlas(atlas);
    }
}
