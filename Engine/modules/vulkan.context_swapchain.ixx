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
 // modules/vulkan.context-swapchain.ixx
// Partition implementation: vulkan.context:swapchain
// Swapchain + image views implementation.
// ============================================================================

module;

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/renderers/vulkan/vulkan.context_shared.hpp>
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

export module vulkan.context:swapchain;

import :shared_vk;
import core.logger;


namespace epochengine::vulkancontext
{
    namespace
    {
        class RecoverableSwapChainError final : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };
    }

    vulkancontext::SwapChainSupportDetails Application::querySwapChainSupport(vk::PhysicalDevice dev)
    {
        auto capabilitiesResult = dev.getSurfaceCapabilitiesKHR(*surface);
        if (capabilitiesResult.result != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - getSurfaceCapabilitiesKHR failed.");
        vk::SurfaceCapabilitiesKHR capabilities = capabilitiesResult.value;

        auto formatsResult = dev.getSurfaceFormatsKHR(*surface);
        if (formatsResult.result != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - getSurfaceFormatsKHR failed.");
        auto formats = std::move(formatsResult.value);

        auto presentModesResult = dev.getSurfacePresentModesKHR(*surface);
        if (presentModesResult.result != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - getSurfacePresentModesKHR failed.");
        auto presentModes = std::move(presentModesResult.value);

        return SwapChainSupportDetails{
            .capabilities = capabilities,
            .formats = std::move(formats),
            .presentModes = std::move(presentModes),
        };
    }

    vk::SurfaceFormatKHR Application::chooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (const auto& fmt : availableFormats)
        {
            if (fmt.format == vk::Format::eB8G8R8A8Srgb &&
                fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return fmt;
        }
        return availableFormats.front();
    }

    vk::PresentModeKHR Application::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& modes)
    {
        for (const auto& mode : modes)
        {
            if (mode == vk::PresentModeKHR::eImmediate)
                return mode;
        }
        for (const auto& mode : modes)
        {
            if (mode == vk::PresentModeKHR::eMailbox)
                return mode;
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Application::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != (std::numeric_limits<std::uint32_t>::max)())
            return capabilities.currentExtent;

        const int width = get_framebuffer_width();
        const int height = get_framebuffer_height();

        vk::Extent2D actual{};
        actual.width = std::clamp(
            static_cast<std::uint32_t>(width),
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
        );
        actual.height = std::clamp(
            static_cast<std::uint32_t>(height),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        );
        return actual;
    }

    void Application::createSwapChain()
    {
        if (!device)
            throw std::runtime_error("[ Vulkan ] - createSwapChain called without a logical device.");
        if (!surface)
            throw std::runtime_error("[ Vulkan ] - createSwapChain called without a surface.");
        if (!physicalDevice)
            throw std::runtime_error("[ Vulkan ] - createSwapChain called without a physical device.");
        if (!queueFamilyIndices.isComplete())
            throw std::runtime_error("[ Vulkan ] - createSwapChain called with incomplete queue family indices.");

        SwapChainSupportDetails details = querySwapChainSupport(physicalDevice);

        if (details.formats.empty())
            throw RecoverableSwapChainError(
                "[ Vulkan ] - Swapchain surface formats unavailable; will retry swapchain creation later.");
        if (details.presentModes.empty())
            throw RecoverableSwapChainError(
                "[ Vulkan ] - Swapchain present modes unavailable; will retry swapchain creation later.");

        const vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
        const vk::PresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
        const vk::Extent2D extent = chooseSwapExtent(details.capabilities);

        logger::get("Epoch.Vulkan").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "Swapchain present mode: {}",
            presentMode == vk::PresentModeKHR::eImmediate
                ? "Immediate"
                : (presentMode == vk::PresentModeKHR::eMailbox ? "Mailbox" : "Fifo"));

        if (extent.width == 0 || extent.height == 0)
            throw RecoverableSwapChainError("[ Vulkan ] - Swapchain extent is zero; waiting for a valid framebuffer size.");

        std::uint32_t imageCount = details.capabilities.minImageCount + 1;
        if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
            imageCount = details.capabilities.maxImageCount;

        vk::SwapchainCreateInfoKHR createInfo{};
        createInfo.surface = *surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

        const std::uint32_t familyIndices[] = {
            queueFamilyIndices.graphicsFamily.value(),
            queueFamilyIndices.presentFamily.value()
        };

        if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily)
        {
            createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = familyIndices;
        }
        else
        {
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        }

        createInfo.preTransform = details.capabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = vk::SwapchainKHR{};

        vk::SwapchainKHR rawSwapchain{};
        const vk::Result scResult = device->createSwapchainKHR(
            &createInfo,
            nullptr,
            &rawSwapchain);
        if (scResult == vk::Result::eErrorOutOfDateKHR ||
            scResult == vk::Result::eSuboptimalKHR ||
            scResult == vk::Result::eErrorSurfaceLostKHR ||
            scResult == vk::Result::eErrorInitializationFailed)
        {
            throw RecoverableSwapChainError("[ Vulkan ] - Swapchain creation returned a recoverable result; will retry.");
        }

        if (scResult != vk::Result::eSuccess || !rawSwapchain)
            throw std::runtime_error("[ Vulkan ] - createSwapchainKHR failed. VkResult=" + std::to_string(static_cast<int>(scResult)));

        swapChain = vk::UniqueSwapchainKHR(
            rawSwapchain,
            vk::detail::ObjectDestroy<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(
                *device,
                nullptr,
                VULKAN_HPP_DEFAULT_DISPATCHER));

        std::uint32_t imageCountOut = 0;
        vk::Result imagesResult = device->getSwapchainImagesKHR(*swapChain, &imageCountOut, nullptr);
        if (imagesResult != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - getSwapchainImagesKHR(count) failed. VkResult=" + std::to_string(static_cast<int>(imagesResult)));

        std::vector<vk::Image> images(imageCountOut);
        imagesResult = device->getSwapchainImagesKHR(*swapChain, &imageCountOut, images.data());
        if (imagesResult != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - getSwapchainImagesKHR(data) failed. VkResult=" + std::to_string(static_cast<int>(imagesResult)));

        images.resize(imageCountOut);
        swapChainImages = std::move(images);

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    vk::UniqueImageView Application::createImageViewUnique(
        vk::Image image,
        vk::Format format,
        vk::ImageAspectFlags aspectFlags)
    {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = vk::ImageSubresourceRange(aspectFlags, 0, 1, 0, 1);

        auto ivResult = device->createImageViewUnique(viewInfo);
        if (ivResult.result != vk::Result::eSuccess || !ivResult.value)
            throw std::runtime_error("[ Vulkan ] - createImageViewUnique failed.");
        return std::move(ivResult.value);
    }

    void Application::createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());
        for (std::size_t i = 0; i < swapChainImages.size(); ++i)
        {
            swapChainImageViews[i] = createImageViewUnique(
                swapChainImages[i],
                swapChainImageFormat,
                vk::ImageAspectFlagBits::eColor
            );
        }
    }

    void Application::cleanupSwapChain()
    {
        if (commandPool && device)
        {
            device->resetCommandPool(*commandPool);
            commandBuffers.clear();
        }

        destroyArcadeRenderTarget();

        framebuffers.clear();

        depthImageView.reset();
        depthImage.reset();
        depthImageMemory.reset();

        arcadeScreenPipeline.reset();
        solidGraphicsPipeline.reset();
        graphicsPipeline.reset();
        pipelineLayout.reset();
        renderPass.reset();

        swapChainImageViews.clear();
        swapChain.reset();

        descriptorPool.reset();
        descriptorSets.clear();

        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBuffersMapped.clear();
        if (auto* guiState = find_gui_state(bound_context()))
            reset_gui_swapchain_state(*guiState);
    }

    void Application::recreateSwapChain()
    {
        assert_thread_affinity();
        if (!device)
            throw std::runtime_error("[ Vulkan ] - recreateSwapChain called without a device.");

        const int framebufferWidth = get_framebuffer_width();
        const int framebufferHeight = get_framebuffer_height();
        if (framebufferWidth <= 0 || framebufferHeight <= 0)
        {
            set_framebuffer_resize_intent(true);
            return;
        }

        const SwapChainSupportDetails details = querySwapChainSupport(physicalDevice);
        if (details.formats.empty() || details.presentModes.empty())
        {
            set_framebuffer_resize_intent(true);
            return;
        }

        (void)device->waitIdle();

        cleanupSwapChain();

        try
        {
            createSwapChain();
            createImageViews();
            createRenderPass();
            createGraphicsPipeline();
            createGuiPipeline();
            createDepthResources();
            createFramebuffers();
            createArcadeRenderTarget();
            createUniformBuffers();
            createGuiUniformBuffers();
            createDescriptorPool();
            createDescriptorSets();
            createArcadeDescriptorSets();
            createCommandBuffers();
            set_framebuffer_resize_intent(false);
        }
        catch (const RecoverableSwapChainError&)
        {
            set_framebuffer_resize_intent(true);
        }
    }

} // namespace epochengine::vulkancontext
