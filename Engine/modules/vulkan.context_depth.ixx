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
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/renderers/vulkan/vulkan.context_shared.hpp>
#include <compare>
#include <vulkan/vulkan.hpp>
#include <stdexcept>

export module vulkan.context:depth;

import :shared_vk;

namespace epochengine::vulkancontext {

    // NOTE: your Vulkan-Hpp config apparently does NOT have vk::FormatFeatureFlags.
    // Use vk::Flags<vk::FormatFeatureFlagBits> instead.
    vk::Format Application::findSupportedFormat(
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

    vk::Format Application::findDepthFormat()
    {
        return findSupportedFormat(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
    }

    void Application::createDepthResources()
    {
        const vk::Format depthFormat = findDepthFormat();

        vk::ImageCreateInfo imageInfo{};
        imageInfo.flags = {};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = depthFormat;
        imageInfo.extent = vk::Extent3D{ swapChainExtent.width, swapChainExtent.height, 1u };
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;
        imageInfo.queueFamilyIndexCount = 0u;
        imageInfo.pQueueFamilyIndices = nullptr;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;

        auto [imgRes, img] = device->createImageUnique(imageInfo);
        if (imgRes != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create depth image!");
        depthImage = std::move(img);

        const vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*depthImage);
        const std::uint32_t memTypeIndex =
            findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (memTypeIndex == UINT32_MAX)
            throw std::runtime_error("Failed to find suitable memory type for depth image!");

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memTypeIndex;

        auto [memRes, mem] = device->allocateMemoryUnique(allocInfo);
        if (memRes != vk::Result::eSuccess)
            throw std::runtime_error("Failed to allocate depth image memory!");
        depthImageMemory = std::move(mem);

        (void)device->bindImageMemory(*depthImage, *depthImageMemory, 0);

        depthImageView = createImageViewUnique(*depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
    }

    void Application::createFramebuffers()
    {
        framebuffers.resize(swapChainImageViews.size());

        for (std::size_t i = 0; i < swapChainImageViews.size(); ++i)
        {
            const std::array<vk::ImageView, 2> attachments = {
                *swapChainImageViews[i],
                *depthImageView
            };

            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.flags = {};
            framebufferInfo.renderPass = *renderPass;
            framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1u;

            auto [fbRes, fb] = device->createFramebufferUnique(framebufferInfo);
            if (fbRes != vk::Result::eSuccess)
                throw std::runtime_error("Failed to create framebuffer!");

            framebuffers[i] = std::move(fb);
        }
    }

} // namespace epochengine::vulkancontext
