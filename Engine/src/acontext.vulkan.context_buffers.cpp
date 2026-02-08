// ============================================================================
// modules/acontext.vulkan.context-buffers.cpp   (or .ixx)
// Partition implementation unit
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>

// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <cstring>
#include <stdexcept>

module acontext.vulkan.context:buffers;

import :shared_vk;
import :meshcube;

namespace almondnamespace::vulkancontext
{
    void Application::createVertexBuffer()
    {
        const auto vertices = cube_vertices();
        if (vertices.empty())
            throw std::runtime_error("[Vulkan] No vertex data available.");

        const vk::DeviceSize bufferSize =
            static_cast<vk::DeviceSize>(sizeof(vertices[0]) * vertices.size());

        const auto stagingUsage = vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc };
        const auto stagingProps =
            vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostVisible } |
            vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostCoherent };

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, stagingUsage, stagingProps);

        auto [mapRes, mapped] = device->mapMemory(*stagingBufferMemory, 0, bufferSize);
        if (mapRes != vk::Result::eSuccess)
            throw std::runtime_error("Failed to map vertex staging buffer memory.");
        std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(bufferSize));
        device->unmapMemory(*stagingBufferMemory);

        const auto vertexUsage =
            vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst } |
            vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eVertexBuffer };
        const auto deviceLocal = vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eDeviceLocal };

        auto [vb, vbMem] = createBuffer(bufferSize, vertexUsage, deviceLocal);
        copyBuffer(*stagingBuffer, *vb, bufferSize);

        stagingBuffer.reset();
        stagingBufferMemory.reset();

        vertexBuffer = std::move(vb);
        vertexBufferMemory = std::move(vbMem);
    }

    void Application::createIndexBuffer()
    {
        const auto indices = cube_indices();
        if (indices.empty())
            throw std::runtime_error("[Vulkan] No index data available.");

        const vk::DeviceSize bufferSize =
            static_cast<vk::DeviceSize>(sizeof(indices[0]) * indices.size());

        const auto stagingUsage = vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc };
        const auto stagingProps =
            vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostVisible } |
            vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostCoherent };

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, stagingUsage, stagingProps);

        auto [mapRes, mapped] = device->mapMemory(*stagingBufferMemory, 0, bufferSize);
        if (mapRes != vk::Result::eSuccess)
            throw std::runtime_error("Failed to map index staging buffer memory.");
        std::memcpy(mapped, indices.data(), static_cast<std::size_t>(bufferSize));
        device->unmapMemory(*stagingBufferMemory);

        const auto indexUsage =
            vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst } |
            vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eIndexBuffer };
        const auto deviceLocal = vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eDeviceLocal };

        auto [ib, ibMem] = createBuffer(bufferSize, indexUsage, deviceLocal);
        copyBuffer(*stagingBuffer, *ib, bufferSize);

        stagingBuffer.reset();
        stagingBufferMemory.reset();

        indexBuffer = std::move(ib);
        indexBufferMemory = std::move(ibMem);
        indexCount = static_cast<std::uint32_t>(indices.size());
    }
}
