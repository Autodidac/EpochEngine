// ============================================================================
// modules/acontext.vulkan.context-depth.ixx
// Partition implementation: acontext.vulkan.context:depth
// Depth buffer + framebuffers.
// Compatible with Vulkan-Hpp both with and without VULKAN_HPP_NO_EXCEPTIONS.
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

export module acontext.vulkan.context:depth;

import :shared_vk;

namespace almondnamespace::vulkancontext
{
    // Not exported: internal helpers are fine here (including anonymous namespace).
    namespace
    {
        [[nodiscard]] inline bool has_stencil(vk::Format fmt) noexcept
        {
            return fmt == vk::Format::eD32SfloatS8Uint || fmt == vk::Format::eD24UnormS8Uint;
        }

#if defined(VULKAN_HPP_NO_EXCEPTIONS)
        template <class ResultValueT>
        [[nodiscard]] inline auto unwrap_or_throw(ResultValueT&& rv, const char* msg)
        {
            if (rv.result != vk::Result::eSuccess)
                throw std::runtime_error(msg);
            return std::move(rv.value);
        }
#endif
    } // namespace

    // Export each definition explicitly (no exported namespace => no anonymous export issue).
    export vk::Format Application::findSupportedFormat(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::Flags<vk::FormatFeatureFlagBits> requiredFeatures)
    {
        for (vk::Format format : candidates)
        {
            const vk::FormatProperties props = physicalDevice.getFormatProperties(format);

            if (tiling == vk::ImageTiling::eLinear)
            {
                if ((props.linearTilingFeatures & requiredFeatures) == requiredFeatures)
                    return format;
            }
            else
            {
                if ((props.optimalTilingFeatures & requiredFeatures) == requiredFeatures)
                    return format;
            }
        }
        throw std::runtime_error("Failed to find supported format!");
    }

    export vk::Format Application::findDepthFormat()
    {
        return findSupportedFormat(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
    }

    export void Application::createDepthResources()
    {
        const vk::Format depthFormat = findDepthFormat();

        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = depthFormat;
        imageInfo.extent = vk::Extent3D{ swapChainExtent.width, swapChainExtent.height, 1u };
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;

#if defined(VULKAN_HPP_NO_EXCEPTIONS)
        depthImage = unwrap_or_throw(device->createImageUnique(imageInfo), "Failed to create depth image!");
#else
        depthImage = device->createImageUnique(imageInfo);
#endif

        const vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*depthImage);
        const std::uint32_t memTypeIndex =
            findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

        if (memTypeIndex == UINT32_MAX)
            throw std::runtime_error("Failed to find suitable memory type for depth image!");

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memTypeIndex;

#if defined(VULKAN_HPP_NO_EXCEPTIONS)
        depthImageMemory = unwrap_or_throw(device->allocateMemoryUnique(allocInfo), "Failed to allocate depth image memory!");
#else
        depthImageMemory = device->allocateMemoryUnique(allocInfo);
#endif

        const vk::Result bindRes = device->bindImageMemory(*depthImage, *depthImageMemory, 0);
        if (bindRes != vk::Result::eSuccess)
            throw std::runtime_error("Failed to bind depth image memory!");

        vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eDepth;
        if (has_stencil(depthFormat))
            aspect |= vk::ImageAspectFlagBits::eStencil;

        depthImageView = createImageViewUnique(*depthImage, depthFormat, aspect);
    }

    export void Application::createFramebuffers()
    {
        framebuffers.resize(swapChainImageViews.size());

        for (std::size_t i = 0; i < swapChainImageViews.size(); ++i)
        {
            const std::array<vk::ImageView, 2> attachments = {
                *swapChainImageViews[i],
                *depthImageView
            };

            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = *renderPass;
            framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1u;

#if defined(VULKAN_HPP_NO_EXCEPTIONS)
            framebuffers[i] = unwrap_or_throw(device->createFramebufferUnique(framebufferInfo), "Failed to create framebuffer!");
#else
            framebuffers[i] = device->createFramebufferUnique(framebufferInfo);
#endif
        }
    }
} // namespace almondnamespace::vulkancontext
