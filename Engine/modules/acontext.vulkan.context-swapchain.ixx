// ============================================================================
// modules/acontext.vulkan.context-swapchain.ixx
// Partition implementation: acontext.vulkan.context:swapchain
// Swapchain + image views implementation.
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

export module acontext.vulkan.context:swapchain;

import :shared_vk;

import <algorithm>;
import <cstdint>;
import <limits>;
import <stdexcept>;
import <vector>;

namespace almondnamespace::vulkancontext
{
    SwapChainSupportDetails Application::querySwapChainSupport(vk::PhysicalDevice dev)
    {
        auto capabilitiesResult = dev.getSurfaceCapabilitiesKHR(*surface);
        if (capabilitiesResult.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] getSurfaceCapabilitiesKHR failed.");
        vk::SurfaceCapabilitiesKHR capabilities = capabilitiesResult.value;

        auto formatsResult = dev.getSurfaceFormatsKHR(*surface);
        if (formatsResult.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] getSurfaceFormatsKHR failed.");
        auto formats = std::move(formatsResult.value);

        auto presentModesResult = dev.getSurfacePresentModesKHR(*surface);
        if (presentModesResult.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] getSurfacePresentModesKHR failed.");
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
        SwapChainSupportDetails details = querySwapChainSupport(physicalDevice);

        const vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
        const vk::PresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
        const vk::Extent2D extent = chooseSwapExtent(details.capabilities);

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
        createInfo.oldSwapchain = vk::SwapchainKHR{}; // no VK_NULL_HANDLE macro

        // ---- create swapchain (Unique + ResultValue) ----
        auto [scRes, sc] = device->createSwapchainKHRUnique(createInfo);
        if (scRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createSwapchainKHRUnique failed.");
        swapChain = std::move(sc);

        // ---- images ----
        auto [imgsRes, imgs] = device->getSwapchainImagesKHR(*swapChain);
        if (imgsRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] getSwapchainImagesKHR failed.");
        swapChainImages = std::move(imgs);

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

        auto [ivRes, iv] = device->createImageViewUnique(viewInfo);
        if (ivRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createImageViewUnique failed.");
        return std::move(iv);
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

        framebuffers.clear();

        depthImageView.reset();
        depthImage.reset();
        depthImageMemory.reset();

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
        if (auto* guiState = find_gui_state(activeGuiContext))
            reset_gui_swapchain_state(*guiState);
    }

    void Application::recreateSwapChain()
    {
        if (!device)
            throw std::runtime_error("[Vulkan] recreateSwapChain called without a device.");

        (void)device->waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createGuiPipeline();
        createDepthResources();
        createFramebuffers();
        createUniformBuffers();
        createGuiUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
    }

} // namespace almondnamespace::vulkancontext
