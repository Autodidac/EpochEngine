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
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <include/acontext.vulkan.hpp>

#if defined(EPOCH_VULKAN_STANDALONE)
#   include <GLFW/glfw3.h>
#endif
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

export module vulkan.context:runtime;

import :shared_context;
import :shared_vk;
import :commands;
import :buffers;
import :depth;
import :descriptor;
import :device;
import :instance;
import :shader_pipeline;
import :swapchain;
import :texture;
import :window;

export namespace epochnamespace::vulkancontext
{
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    namespace appreg
    {
        class ApplicationRegistry
        {
        public:
            Application& bind(const std::shared_ptr<core::Context>& ctx)
            {
                if (!ctx)
                    throw std::runtime_error("[ Vulkan ] - ApplicationRegistry::bind requires non-null Context");

                std::unique_lock lock{ mutex_ };
                auto& slot = apps_[ctx.get()];
                if (!slot)
                    slot = std::make_unique<Application>();
                return *slot;
            }

            Application* get(const core::Context* ctx) noexcept
            {
                if (!ctx)
                    return nullptr;

                std::shared_lock lock{ mutex_ };
                auto it = apps_.find(ctx);
                return (it != apps_.end() && it->second) ? it->second.get() : nullptr;
            }

            bool release(const core::Context* ctx) noexcept
            {
                if (!ctx)
                    return false;

                std::unique_lock lock{ mutex_ };
                return apps_.erase(ctx) > 0;
            }

            bool any() const noexcept
            {
                std::shared_lock lock{ mutex_ };
                return !apps_.empty();
            }

        private:
            mutable std::shared_mutex mutex_{};
            std::unordered_map<const core::Context*, std::unique_ptr<Application>> apps_{};
        };

        ApplicationRegistry& application_registry() noexcept
        {
            static ApplicationRegistry registry{};
            return registry;
        }
    }

    Application& bind_vulkan_app(const std::shared_ptr<core::Context>& ctx)
    {
        return appreg::application_registry().bind(ctx);
    }

    Application* try_get_vulkan_app(const core::Context* ctx) noexcept
    {
        return appreg::application_registry().get(ctx);
    }

    bool release_vulkan_app(const core::Context* ctx) noexcept
    {
        return appreg::application_registry().release(ctx);
    }

    bool has_vulkan_apps() noexcept
    {
        return appreg::application_registry().any();
    }

    const core::Context* Application::bound_context() const noexcept
    {
        if (activeGuiContext)
            return activeGuiContext;

        if (const auto ctx = context.lock())
            return ctx.get();
        return nullptr;
    }
    // ---- Application method definitions ----
//
//    void Application::initWindow()
//    {
//#if defined(EPOCH_VULKAN_STANDALONE)
//        if (!glfwInit())
//            throw std::runtime_error("[ Vulkan ] - Failed to initialize GLFW.");
//
//        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
//
//        window = glfwCreateWindow(framebufferWidth, framebufferHeight, "Epoch Vulkan", nullptr, nullptr);
//        if (!window)
//            throw std::runtime_error("[ Vulkan ] - Failed to create GLFW window.");
//
//        glfwSetWindowUserPointer(window, this);
//        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
//        glfwSetCursorPosCallback(window, mouseCallback);
//        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
//#else
//        window = nullptr;
//        if (!nativeWindowHandle)
//            throw std::runtime_error("[ Vulkan ] - Requires a native window handle.");
//#endif
//    }
//
//    void Application::initVulkan()
//    {
//        createInstance();
//        createSurface();
//
//        if (enableValidationLayers)
//        {
//            // setupDebugMessenger() can be added here if implemented
//        }
//
//        createLogicalDevice();
//        createSwapChain();
//        createImageViews();
//        createRenderPass();
//        createDescriptorSetLayout();
//        createGraphicsPipeline();
//        createCommandPool();
//        createDepthResources();
//        createFramebuffers();
//        createTextureImage();
//        createTextureImageView();
//        createTextureSampler();
//        createVertexBuffer();
//        createIndexBuffer();
//        createUniformBuffers();
//        createDescriptorPool();
//        createDescriptorSets();
//        createCommandBuffers();
//        createSyncObjects();
//    }
//
//    bool Application::process(std::shared_ptr<epochnamespace::core::Context> ctx,
//        epochnamespace::core::CommandQueue& queue)
//    {
//        if (!device)
//            return false;
//
//        static auto lastTime = std::chrono::high_resolution_clock::now();
//        const auto currentTime = std::chrono::high_resolution_clock::now();
//        const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
//        lastTime = currentTime;
//
//#if defined(EPOCH_VULKAN_STANDALONE)
//        if (window)
//        {
//            glfwPollEvents();
//            if (glfwWindowShouldClose(window))
//                return false;
//        }
//#else
//        if (ctx && ctx->get_mouse_position)
//        {
//            int mouseX = 0;
//            int mouseY = 0;
//            ctx->get_mouse_position(mouseX, mouseY);
//            processMouseInput(static_cast<double>(mouseX), static_cast<double>(mouseY));
//        }
//#endif
//
//        if (ctx)
//        {
//            const int fbWidth = (std::max)(1, ctx->framebufferWidth);
//            const int fbHeight = (std::max)(1, ctx->framebufferHeight);
//            if (fbWidth != framebufferWidth || fbHeight != framebufferHeight)
//                set_framebuffer_size(fbWidth, fbHeight);
//        }
//
//        updateCamera(deltaTime);
//
//        queue.drain();
//        drawFrame();
//        return true;
//    }
//
//    void Application::cleanup()
//    {
//        if (device)
//            (void)device->waitIdle();
//
//        imageAvailableSemaphores.clear();
//        renderFinishedSemaphores.clear();
//        inFlightFences.clear();
//
//        if (!commandBuffers.empty() && commandPool && device)
//        {
//            device->resetCommandPool(*commandPool);
//            commandBuffers.clear();
//        }
//
//        framebuffers.clear();
//        swapChainImageViews.clear();
//        swapChain.reset();
//
//        descriptorPool.reset();
//        descriptorSets.clear();
//
//        uniformBuffers.clear();
//        uniformBuffersMemory.clear();
//        uniformBuffersMapped.clear();
//
//        indexBuffer.reset();
//        indexBufferMemory.reset();
//        vertexBuffer.reset();
//        vertexBufferMemory.reset();
//
//        textureSampler.reset();
//        textureImageView.reset();
//        textureImage.reset();
//        textureImageMemory.reset();
//
//        pipelineLayout.reset();
//        graphicsPipeline.reset();
//        descriptorSetLayout.reset();
//        renderPass.reset();
//
//        depthImageView.reset();
//        depthImage.reset();
//        depthImageMemory.reset();
//
//        commandPool.reset();
//        device.reset();
//        surface.reset();
//
//        if (enableValidationLayers && debugMessenger && instance)
//        {
//            instance->destroyDebugUtilsMessengerEXT(debugMessenger, nullptr);
//            debugMessenger = VK_NULL_HANDLE;
//        }
//
//        instance.reset();
//
//#if defined(EPOCH_VULKAN_STANDALONE)
//        if (window)
//        {
//            glfwDestroyWindow(window);
//            window = nullptr;
//        }
//        glfwTerminate();
//#endif
//
//        nativeWindowHandle = nullptr;
//    }
//
//    void Application::set_framebuffer_size(int width, int height)
//    {
//        framebufferWidth = width;
//        framebufferHeight = height;
//        framebufferResized = true;
//    }
//
//    int Application::get_framebuffer_width() const noexcept { return framebufferWidth; }
//    int Application::get_framebuffer_height() const noexcept { return framebufferHeight; }
//
//    void Application::set_context(std::shared_ptr<epochnamespace::core::Context> ctx, void* nativeWindow)
//    {
//        context = ctx;
//        nativeWindowHandle = nativeWindow;
//    }
}
