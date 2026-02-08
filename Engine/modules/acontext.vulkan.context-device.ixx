module;

#include <stdexcept>
#include <set>
#include <string>
#include <vector>

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

export module acontext.vulkan.context:device;

import :shared_context;
import :shared_vk;
import :swapchain;

namespace almondnamespace::vulkancontext {

    inline vk::PhysicalDevice Application::pickPhysicalDevice()
    {
        // Works whether enumeratePhysicalDevices returns ResultValue or vector in your build.
        auto rv = instance->enumeratePhysicalDevices();
        const auto& devices = rv.value;

        if (rv.result != vk::Result::eSuccess || devices.empty())
            throw std::runtime_error("[Vulkan] Failed to find GPUs with Vulkan support.");

        for (const auto& dev : devices)
        {
            if (isDeviceSuitable(dev))
                return dev;
        }

        throw std::runtime_error("[Vulkan] Failed to find a suitable GPU.");
    }

    inline bool Application::checkDeviceExtensionSupport(vk::PhysicalDevice device)
    {
        auto availableExts = device.enumerateDeviceExtensionProperties();
        if (availableExts.result != vk::Result::eSuccess)
            return false;

        std::set<std::string> requiredExts(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& ext : availableExts.value)
            requiredExts.erase(ext.extensionName);

        return requiredExts.empty();
    }

    inline QueueFamilyIndices Application::findQueueFamilies(vk::PhysicalDevice device)
    {
        QueueFamilyIndices indices{};

        const auto queueProps = device.getQueueFamilyProperties();
        uint32_t i = 0;

        for (const auto& qf : queueProps)
        {
            if (qf.queueFlags & vk::QueueFlagBits::eGraphics)
                indices.graphicsFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            (void)device.getSurfaceSupportKHR(i, *surface, &presentSupport);
            if (presentSupport)
                indices.presentFamily = i;

            if (indices.isComplete())
                break;

            ++i;
        }

        return indices;
    }

    inline bool Application::isDeviceSuitable(vk::PhysicalDevice device)
    {
        const QueueFamilyIndices indices = findQueueFamilies(device);
        const bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            const SwapChainSupportDetails swapDetails = Application::querySwapChainSupport(device);
            swapChainAdequate = !swapDetails.formats.empty() && !swapDetails.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    void Application::createLogicalDevice()
    {
        physicalDevice = pickPhysicalDevice();
        queueFamilyIndices = findQueueFamilies(physicalDevice);

        std::set<uint32_t> uniqueFamilies = {
            queueFamilyIndices.graphicsFamily.value(),
            queueFamilyIndices.presentFamily.value()
        };

        float queuePriority = 1.0f;

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueFamilies.size());

        for (uint32_t family : uniqueFamilies)
        {
            vk::DeviceQueueCreateInfo qci{};
            qci.sType = vk::StructureType::eDeviceQueueCreateInfo;
            qci.pNext = nullptr;
            qci.flags = {};
            qci.queueFamilyIndex = family;
            qci.queueCount = 1;
            qci.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(qci);
        }

        vk::PhysicalDeviceFeatures deviceFeatures{};

        vk::DeviceCreateInfo createInfo{};
        createInfo.sType = vk::StructureType::eDeviceCreateInfo;
        createInfo.pNext = nullptr;
        createInfo.flags = {};

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        if (validationLayersEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        auto [dRes, d] = physicalDevice.createDeviceUnique(createInfo);
        if (dRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to create logical device.");

        device = std::move(d);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());

        graphicsQueue = device->getQueue(queueFamilyIndices.graphicsFamily.value(), 0);
        presentQueue = device->getQueue(queueFamilyIndices.presentFamily.value(), 0);
    }

    void Application::createCommandPool()
    {
        if (!queueFamilyIndices.graphicsFamily.has_value())
            throw std::runtime_error("[Vulkan] Graphics family index not set.");

        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
        poolInfo.pNext = nullptr;
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        auto [res, pool] = device->createCommandPoolUnique(poolInfo);
        if (res != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to create command pool.");

        commandPool = std::move(pool);
    }

} // namespace almondnamespace::vulkancontext
