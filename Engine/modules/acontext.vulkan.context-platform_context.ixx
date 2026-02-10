//// acontext.vulkan.context-platform_context.ixx
//
// Module partition for `acontext.vulkan.context` that defines
// `almondnamespace::vulkancontext::Application` methods.
//
// This TU provides the single STB_IMAGE_IMPLEMENTATION definition.
// Vulkan-Hpp dynamic dispatch storage lives in :dispatch_storage.
//
// -----------------------------------------------------------------------------

module;

// ---- One-TU-only implementations / storage ---------------------------------
#ifndef STB_IMAGE_IMPLEMENTATION
#   define STB_IMAGE_IMPLEMENTATION
#endif

// Vulkan-Hpp config (static dispatch)
#include <include/acontext.vulkan.hpp>

#include "../include/epoch.config.hpp"

#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR 1
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#if defined(ALMOND_USING_OPENGL)
#   include <glad/glad.h>
#endif

#if defined(ALMOND_VULKAN_STANDALONE)
#   ifndef GLFW_INCLUDE_VULKAN
#       define GLFW_INCLUDE_VULKAN
#   endif
#   include <GLFW/glfw3.h>
#endif

#include <vulkan/vulkan.hpp>

// STB must be included after its implementation define:
#include "src/stb/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// Now enter the named module (partition)
// -----------------------------------------------------------------------------
export module acontext.vulkan.context:platform_context;

import :window;
import :instance;
import :device;
import :swapchain;
import :shader_pipeline;
import :depth;
import :memory;
import :texture;
import :descriptor;
import :commands;

import aengine.context.commandqueue;
import aengine.core.context;
import aengine.input;
import :shared_vk;

#if defined(ALMOND_USING_OPENGL)
import acontext.opengl.platform;
#endif

// -----------------------------------------------------------------------------
// Correct namespace for the exported type declared in the interface:
// export namespace almondnamespace::vulkancontext { export class Application ... }
// -----------------------------------------------------------------------------
namespace almondnamespace::vulkancontext
{
    void Application::run()
    {
        almondnamespace::core::CommandQueue queue;
        initWindow();
        initVulkan();

#ifdef _DEBUG
        std::cout << "Vertex struct size: " << sizeof(Vertex) << "\n";
        std::cout << "Position offset: " << offsetof(Vertex, pos) << "\n";
        std::cout << "Normal offset: " << offsetof(Vertex, normal) << "\n";
        std::cout << "TexCoord offset: " << offsetof(Vertex, texCoord) << "\n";
#endif

        while (process(nullptr, queue)) {}
        cleanup();
    }

    void Application::initVulkan()
    {
#ifdef _DEBUG
        std::cout << "initVulkan() called\n";
#endif
        createInstance();
        createSurface();

        if (validationLayersEnabled)
        {
            // setupDebugMessenger(); // if you implement it
        }

        physicalDevice = pickPhysicalDevice();
        if (!physicalDevice)
            throw std::runtime_error("Failed to pick a physical device!");

        queueFamilyIndices = findQueueFamilies(physicalDevice);

        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createGuiPipeline();
        createCommandPool();
        createDepthResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createGuiUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void Application::initWindow()
    {
#if defined(ALMOND_VULKAN_STANDALONE)
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(framebufferWidth, framebufferHeight, "Vulkan Cube", nullptr, nullptr);
        if (!window)
            throw std::runtime_error("Failed to create GLFW window");

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetCursorPosCallback(window, mouseCallback);

        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#else
        window = nullptr;
        if (!nativeWindowHandle)
            throw std::runtime_error("Vulkan requires a native window handle.");
#endif

        // IMPORTANT: with VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE defined
        // in THIS TU, this init call is valid and links cleanly.

#ifdef _DEBUG
        std::cout << "Window configured\n";
#endif
    }

    void Application::framebufferResizeCallback(GLFWwindow* window_, int width, int height)
    {
#if defined(ALMOND_VULKAN_STANDALONE)
        auto* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window_));
        if (!app)
            return;

        int fbWidth = width;
        int fbHeight = height;
        if (fbWidth == 0 || fbHeight == 0)
            glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);

        app->set_framebuffer_size(fbWidth, fbHeight);
#else
        (void)window_;
        (void)width;
        (void)height;
#endif
    }

    bool Application::process(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!device)
            return false;

        set_active_context(ctx.get());

        const auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = 0.0f;
        if (lastFrameTime)
            deltaTime = std::chrono::duration<float>(currentTime - *lastFrameTime).count();
        lastFrameTime = currentTime;

#if defined(ALMOND_VULKAN_STANDALONE)
        if (window)
        {
            glfwPollEvents();
            if (glfwWindowShouldClose(window))
                return false;
        }
#else
        if (ctx && ctx->get_mouse_position)
        {
            int mouseX = 0;
            int mouseY = 0;
            ctx->get_mouse_position(mouseX, mouseY);
            processMouseInput(static_cast<double>(mouseX), static_cast<double>(mouseY));
        }
#endif

        bool framebufferMinimized = false;
        if (ctx)
        {
            const int fbWidth = (std::max)(1, ctx->framebufferWidth);
            const int fbHeight = (std::max)(1, ctx->framebufferHeight);
            if (fbWidth != framebufferWidth || fbHeight != framebufferHeight)
                set_framebuffer_size(fbWidth, fbHeight);

            framebufferMinimized = ctx->framebufferWidth == 0 || ctx->framebufferHeight == 0;
        }
#if defined(ALMOND_VULKAN_STANDALONE)
        else if (window)
        {
            int fbWidth = 0;
            int fbHeight = 0;
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            framebufferMinimized = fbWidth == 0 || fbHeight == 0;
        }
#endif

        updateCamera(deltaTime);

        if (framebufferMinimized)
        {
            if (auto* guiState = find_gui_state(context_id_from_ptr(ctx)))
                guiState->guiDraws.clear();
            queue.drain();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            return true;
        }
        if (auto* guiState = find_gui_state(context_id_from_ptr(ctx)))
            guiState->guiDraws.clear();
        queue.drain();
        drawFrame();
        return true;
    }

    void Application::set_context(std::shared_ptr<almondnamespace::core::Context> ctx, void* nativeWindow)
    {
        context = std::move(ctx);
        nativeWindowHandle = nativeWindow;
        const auto locked = context.lock();
        activeGuiContextId = context_id_from_ptr(locked);
        if (activeGuiContextId != 0u && locked)
            guiContexts[activeGuiContextId].context = locked;
    }

    void Application::set_active_context(const almondnamespace::core::Context* ctx)
    {
        activeGuiContextId = context_id_from_ptr(ctx);
        if (activeGuiContextId != 0u)
        {
            const auto locked = context.lock();
            if (locked && locked.get() == ctx)
                guiContexts[activeGuiContextId].context = locked;
        }
        prune_gui_contexts();
    }

    void Application::cleanup_gui_context(const almondnamespace::core::Context* ctx)
    {
        if (!ctx)
            return;

        const ContextId ctxId = context_id_from_ptr(ctx);
        guiContexts.erase(ctxId);
        if (activeGuiContextId == ctxId)
            activeGuiContextId = 0u;
        prune_gui_contexts();
    }

    Application::GuiContextState& Application::gui_state_for_context(
        ContextId ctxId,
        std::weak_ptr<almondnamespace::core::Context> ctxRef)
    {
        if (ctxId == 0u)
        {
            static GuiContextState emptyState{};
            return emptyState;
        }

        auto& entry = guiContexts[ctxId];
        if (entry.context.expired() && !ctxRef.expired())
            entry.context = std::move(ctxRef);
        return entry.state;
    }

    Application::GuiContextState* Application::find_gui_state(
        ContextId ctxId) noexcept
    {
        if (ctxId == 0u)
            return nullptr;
        auto it = guiContexts.find(ctxId);
        if (it == guiContexts.end())
            return nullptr;
        return &it->second.state;
    }

    void Application::prune_gui_contexts()
    {
        for (auto it = guiContexts.begin(); it != guiContexts.end(); )
        {
            if (it->first == 0u || it->second.context.expired())
                it = guiContexts.erase(it);
            else
                ++it;
        }

        if (activeGuiContextId != 0u && guiContexts.find(activeGuiContextId) == guiContexts.end())
            activeGuiContextId = 0u;
    }

    void Application::reset_gui_swapchain_state(GuiContextState& guiState)
    {
        guiState.guiPipeline.reset();
        guiState.guiUniformBuffers.clear();
        guiState.guiUniformBuffersMemory.clear();
        guiState.guiUniformBuffersMapped.clear();
        guiState.guiAtlases.clear();
    }

    void Application::set_framebuffer_size(int width, int height)
    {
        framebufferWidth = (std::max)(1, width);
        framebufferHeight = (std::max)(1, height);
        framebufferResized = true;
    }

    int Application::get_framebuffer_width() const noexcept
    {
        return (std::max)(1, framebufferWidth);
    }

    int Application::get_framebuffer_height() const noexcept
    {
        return (std::max)(1, framebufferHeight);
    }

    void Application::cleanup()
    {
        if (device)
            (void)device->waitIdle();

        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        inFlightFences.clear();

        if (!commandBuffers.empty() && commandPool && device)
        {
            device->resetCommandPool(*commandPool);
            commandBuffers.clear();
        }

        cleanupSwapChain();

        framebuffers.clear();

        descriptorPool.reset();

        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBuffersMapped.clear();

        indexBuffer.reset();
        indexBufferMemory.reset();
        vertexBuffer.reset();
        vertexBufferMemory.reset();
        guiContexts.clear();
        activeGuiContextId = 0u;

        textureSampler.reset();
        textureImageView.reset();
        textureImage.reset();
        textureImageMemory.reset();

        pipelineLayout.reset();
        graphicsPipeline.reset();
        descriptorSetLayout.reset();
        renderPass.reset();

        depthImage.reset();
        depthImageMemory.reset();
        depthImageView.reset();

        commandPool.reset();

        device.reset();

        // surface is a UniqueSurfaceKHR; do not manually destroy it.
        surface.reset();

        if (validationLayersEnabled && debugMessenger && instance)
        {
            instance->destroyDebugUtilsMessengerEXT(debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }

        instance.reset();

#if defined(ALMOND_VULKAN_STANDALONE)
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
#endif
    }

} // namespace almondnamespace::vulkancontext
