module;

// Keep this in the global module fragment so it doesn't leak macros into importers.
#ifndef _CRT_SECURE_NO_WARNINGS
#   define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdint>
#include <iostream>
#include <stdexcept>

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

export module acontext.vulkan.context:memory;

import :shared_vk;

namespace almondnamespace::vulkancontext
{
    std::uint32_t Application::findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        const vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            const bool typeOk = (typeFilter & (1u << i)) != 0u;
            const bool propsOk = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeOk && propsOk)
                return i;
        }

        throw std::runtime_error("[Vulkan] Failed to find suitable memory type.");
    }

    std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory> Application::createBuffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties)
    {
        // BufferCreateInfo: avoid positional ctors (your build log shows they don't match your Vulkan-Hpp config)
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.flags = {};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        bufferInfo.queueFamilyIndexCount = 0u;
        bufferInfo.pQueueFamilyIndices = nullptr;

        auto [bRes, b] = device->createBufferUnique(bufferInfo);
        if (bRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createBufferUnique failed.");

        // Memory
        const vk::MemoryRequirements memReq = device->getBufferMemoryRequirements(*b);

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);

        auto [mRes, m] = device->allocateMemoryUnique(allocInfo);
        if (mRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] allocateMemoryUnique failed for buffer.");

        (void)device->bindBufferMemory(*b, *m, 0);

        return { std::move(b), std::move(m) };
    }

    vk::UniqueCommandBuffer Application::beginSingleTimeCommands()
    {
        if (!commandPool)
            throw std::runtime_error("[Vulkan] beginSingleTimeCommands: commandPool is null.");

        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = *commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1u;

        auto [aRes, bufs] = device->allocateCommandBuffersUnique(allocInfo);
        if (aRes != vk::Result::eSuccess || bufs.empty())
            throw std::runtime_error("[Vulkan] allocateCommandBuffersUnique failed.");

        vk::UniqueCommandBuffer cmd = std::move(bufs[0]);

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        beginInfo.pInheritanceInfo = nullptr;

        (void)cmd->begin(beginInfo);
        return cmd;
    }

    void Application::endSingleTimeCommands(vk::UniqueCommandBuffer& commandBuffer)
    {
        (void)commandBuffer->end();

        vk::SubmitInfo submitInfo{};
        submitInfo.waitSemaphoreCount = 0u;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 1u;

        vk::CommandBuffer rawCmd = *commandBuffer;
        submitInfo.pCommandBuffers = &rawCmd;

        submitInfo.signalSemaphoreCount = 0u;
        submitInfo.pSignalSemaphores = nullptr;

        (void)graphicsQueue.submit(1u, &submitInfo, vk::Fence{});
        (void)graphicsQueue.waitIdle();
        // UniqueCommandBuffer auto-frees on destruction.
    }

    void Application::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
    {
        std::cout << "[Vulkan] Copying " << size << " bytes.";

        vk::UniqueCommandBuffer cmd = beginSingleTimeCommands();

        vk::BufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;

        cmd->copyBuffer(srcBuffer, dstBuffer, 1u, &copyRegion);
        endSingleTimeCommands(cmd);
    }
} // namespace almondnamespace::vulkancontext
