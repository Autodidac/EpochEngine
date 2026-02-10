module;

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module acontext.vulkan.context:descriptor;

import :shared_context;
import :shared_vk;
import acontext.vulkan.camera;

namespace almondnamespace::vulkancontext
{
    void Application::createDescriptorPool()
    {
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

        auto [r, pool] = device->createDescriptorPoolUnique(poolInfo);
        if (r != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to create descriptor pool.");

        descriptorPool = std::move(pool);
    }

    void Application::createDescriptorSets()
    {
        const std::size_t n = swapChainImages.size();

        std::vector<vk::DescriptorSetLayout> layouts(n, *descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = *descriptorPool;
        allocInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        auto [r, sets] = device->allocateDescriptorSetsUnique(allocInfo);
        if (r != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] Failed to allocate descriptor sets.");

        descriptorSets = std::move(sets);

        for (std::size_t i = 0; i < n; ++i)
        {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = *uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            vk::DescriptorImageInfo imageInfo{};
            imageInfo.sampler = *textureSampler;
            imageInfo.imageView = *textureImageView;
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            std::array<vk::WriteDescriptorSet, 2> writes{};

            // Binding 0: UBO
            writes[0].dstSet = *descriptorSets[i];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            writes[0].pImageInfo = nullptr;
            writes[0].pBufferInfo = &bufferInfo;
            writes[0].pTexelBufferView = nullptr;

            // Binding 1: combined sampler
            writes[1].dstSet = *descriptorSets[i];
            writes[1].dstBinding = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            writes[1].pImageInfo = &imageInfo;
            writes[1].pBufferInfo = nullptr;
            writes[1].pTexelBufferView = nullptr;

            device->updateDescriptorSets(writes, {});
        }
    }

    void Application::createUniformBuffers()
    {
        const std::size_t n = swapChainImages.size();

        uniformBuffers.resize(n);
        uniformBuffersMemory.resize(n);
        uniformBuffersMapped.resize(n);

        const vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        for (std::size_t i = 0; i < n; ++i)
        {
            auto [buf, mem] = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );

            uniformBuffers[i] = std::move(buf);
            uniformBuffersMemory[i] = std::move(mem);

            auto [mapRes, ptr] = device->mapMemory(*uniformBuffersMemory[i], 0, bufferSize);
            if (mapRes != vk::Result::eSuccess || ptr == nullptr)
                throw std::runtime_error("[Vulkan] Failed to map uniform buffer memory.");

            uniformBuffersMapped[i] = ptr;
        }
    }

    void Application::createGuiUniformBuffers()
    {
        auto& guiState = gui_state_for_context(activeGuiContext);
        const std::size_t n = swapChainImages.size();

        guiState.guiUniformBuffers.resize(n);
        guiState.guiUniformBuffersMemory.resize(n);
        guiState.guiUniformBuffersMapped.resize(n);

        const vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        for (std::size_t i = 0; i < n; ++i)
        {
            auto [buf, mem] = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );

            guiState.guiUniformBuffers[i] = std::move(buf);
            guiState.guiUniformBuffersMemory[i] = std::move(mem);

            auto [mapRes, ptr] = device->mapMemory(*guiState.guiUniformBuffersMemory[i], 0, bufferSize);
            if (mapRes != vk::Result::eSuccess || ptr == nullptr)
                throw std::runtime_error("[Vulkan] Failed to map GUI uniform buffer memory.");

            guiState.guiUniformBuffersMapped[i] = ptr;
        }
    }

    void Application::updateUniformBuffer(std::uint32_t currentImage, const vulkancamera::State& camera)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        const auto now = std::chrono::high_resolution_clock::now();
        const float time = std::chrono::duration<float>(now - startTime).count();

        // No mystery member like `cubeRotation` — just rotate at a constant rate.
        constexpr float kRadPerSec = 0.7f;

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), kRadPerSec * time, glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = vulkancamera::getViewMatrix(camera);

        const float aspect = swapChainExtent.height
            ? (swapChainExtent.width / static_cast<float>(swapChainExtent.height))
            : 1.0f;

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
        proj[1][1] *= -1.0f; // Vulkan clip space

        ubo.proj = proj;

        std::memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void Application::updateGuiUniformBuffer(std::uint32_t currentImage)
    {
        auto* guiState = find_gui_state(activeGuiContext);
        if (!guiState)
            return;
        if (guiState->guiUniformBuffersMapped.empty()
            || currentImage >= guiState->guiUniformBuffersMapped.size())
            return;

        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view = glm::mat4(1.0f);

        const float width = swapChainExtent.width ? static_cast<float>(swapChainExtent.width) : 1.0f;
        const float height = swapChainExtent.height ? static_cast<float>(swapChainExtent.height) : 1.0f;

        ubo.proj = glm::ortho(0.0f, width, height, 0.0f);

        std::memcpy(guiState->guiUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }
} // namespace almondnamespace::vulkancontext
