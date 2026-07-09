/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

#include <algorithm>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/context.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module vulkan.context:descriptor;

import :shared_context;
import :shared_vk;
import vulkan.camera;
import render.preview_grid;

namespace epochnamespace::vulkancontext
{
    namespace
    {
        [[nodiscard]] inline glm::mat4 previewgrid_to_glm(
            const epochnamespace::previewgrid::Mat4& source) noexcept
        {
            glm::mat4 out{ 1.0f };
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                    out[column][row] = source[static_cast<std::size_t>(column * 4 + row)];
            }
            return out;
        }
    }

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
            throw std::runtime_error("[ Vulkan ] - Failed to create descriptor pool.");

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
            throw std::runtime_error("[ Vulkan ] - Failed to allocate descriptor sets.");

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
                throw std::runtime_error("[ Vulkan ] - Failed to map uniform buffer memory.");

            uniformBuffersMapped[i] = ptr;
        }
    }

    void Application::createGuiUniformBuffers()
    {
        auto& guiState = gui_state_for_context(bound_context());
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
                throw std::runtime_error("[ Vulkan ] - Failed to map GUI uniform buffer memory.");

            guiState.guiUniformBuffersMapped[i] = ptr;
        }
    }

    void Application::updateUniformBuffer(std::uint32_t currentImage, const vulkancamera::State& camera)
    {
        [[maybe_unused]] static auto startTime = std::chrono::high_resolution_clock::now();

        [[maybe_unused]] const auto now = std::chrono::high_resolution_clock::now();
        [[maybe_unused]] const float time = std::chrono::duration<float>(now - startTime).count();

        // No mystery member like `cubeRotation` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â just rotate at a constant rate.
        UniformBufferObject ubo{};
        const auto* ctx = bound_context();
        const bool editorPreview =
            ctx
            && ctx->scene_preview_mode() == epochnamespace::core::ScenePreviewMode::Editor
            && ctx->scene_viewport().valid();
        const auto previewCamera = epochnamespace::previewgrid::camera_for(ctx);

        ubo.model = glm::mat4(1.0f);
        if (editorPreview)
        {
            ubo.view = glm::lookAt(
                glm::vec3(previewCamera.eye.x, previewCamera.eye.y, previewCamera.eye.z),
                glm::vec3(previewCamera.target.x, previewCamera.target.y, previewCamera.target.z),
                glm::vec3(previewCamera.up.x, previewCamera.up.y, previewCamera.up.z));
        }
        else
        {
            ubo.view = vulkancamera::getViewMatrix(camera);
        }

        std::uint32_t sceneWidth = swapChainExtent.width;
        std::uint32_t sceneHeight = swapChainExtent.height;
        if (ctx)
        {
            const auto sceneViewport = ctx->scene_viewport();
            if (sceneViewport.valid())
            {
                sceneWidth = static_cast<std::uint32_t>((std::max)(1, sceneViewport.width));
                sceneHeight = static_cast<std::uint32_t>((std::max)(1, sceneViewport.height));
            }
        }

        const float aspect = sceneHeight
            ? (sceneWidth / static_cast<float>(sceneHeight))
            : 1.0f;

        if (editorPreview)
        {
            const auto previewProj = epochnamespace::previewgrid::projection_for(
                ctx,
                aspect,
                previewCamera);
            const auto previewView = epochnamespace::previewgrid::look_at(
                previewCamera.eye,
                previewCamera.target,
                previewCamera.up);

            ubo.view = previewgrid_to_glm(previewView);
            ubo.proj = previewgrid_to_glm(previewProj);
            ubo.proj[1][1] *= -1.0f; // Vulkan clip space
        }
        else
        {
            glm::mat4 proj = glm::perspective(
                glm::radians(45.0f),
                aspect,
                0.1f,
                10.0f);
            proj[1][1] *= -1.0f; // Vulkan clip space
            ubo.proj = proj;
        }

        std::memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void Application::updateGuiUniformBuffer(std::uint32_t currentImage)
    {
        auto* guiState = find_gui_state(bound_context());
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
} // namespace epochnamespace::vulkancontext
