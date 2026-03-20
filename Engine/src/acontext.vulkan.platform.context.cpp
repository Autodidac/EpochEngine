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
 //// acontext.vulkan.platform.context.cpp
//
// This file MUST be a module implementation unit for `acontext.vulkan.context`
// because it defines `epochnamespace::vulkancontext::Application` methods.
//
// It also MUST be the single TU that provides:
//  - STB_IMAGE_IMPLEMENTATION
//  - Vulkan-Hpp dynamic dispatch storage (see acontext.vulkan.dispatch_storage.cpp)
//
// If you keep vulkan.hpp in the module interface BMI, the safest way to guarantee
// the dispatch storage exists is to include vulkan.hpp textually here (with the
// same config macros) before `module acontext.vulkan.context;`.
//
// -----------------------------------------------------------------------------
//
// NOTE: No `import acontext.vulkan.context;` here. This file *is* that module.
//

module;

// ---- One-TU-only implementations / storage ---------------------------------
#ifndef STB_IMAGE_IMPLEMENTATION
#   define STB_IMAGE_IMPLEMENTATION
#endif

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <include/acontext.vulkan.hpp>

#if defined(_WIN32)
#   ifndef VK_USE_PLATFORM_WIN32_KHR
#       define VK_USE_PLATFORM_WIN32_KHR
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

#if defined(EPOCH_VULKAN_STANDALONE)
#   ifndef GLFW_INCLUDE_VULKAN
#       define GLFW_INCLUDE_VULKAN
#   endif
#   include <GLFW/glfw3.h>
#endif

#include <vulkan/vulkan.hpp>

// STB must be included after its implementation define:
#include "src/stb/stb_image.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// Now enter the named module (implementation unit)
// -----------------------------------------------------------------------------
module acontext.vulkan.context;

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
import aengine.core.logger;
import aengine.input;
import :shared_vk;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import acontext.opengl.platform;
#endif

// -----------------------------------------------------------------------------
// Correct namespace for the exported type declared in the interface:
// export namespace epochnamespace::vulkancontext { export class Application ... }
// -----------------------------------------------------------------------------
namespace epochnamespace::vulkancontext
{
    constexpr std::string_view kPlatformLogSys = "Context.Vulkan.Platform";

    void Application::run()
    {
        epochnamespace::core::CommandQueue queue;
        initWindow();
        initVulkan();

#ifdef _DEBUG
        logger::get(kPlatformLogSys).logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "Vertex layout size={} pos={} normal={} uv={}",
            sizeof(Vertex),
            offsetof(Vertex, pos),
            offsetof(Vertex, normal),
            offsetof(Vertex, texCoord));
#endif

        while (process(nullptr, queue)) {}
        cleanup();
    }

    void Application::initVulkan()
    {
#if defined(_DEBUG) && EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_VULKAN_CONFIRMATION_LOGS
            logger::get(kPlatformLogSys).log(
            logger::LogLevel::INFO,
            "initVulkan() called",
            std::source_location::current());
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
        bind_render_thread();
        assert_thread_affinity();
#if defined(EPOCH_VULKAN_STANDALONE)
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(get_framebuffer_width(), get_framebuffer_height(), "Vulkan Cube", nullptr, nullptr);
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
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

#if defined(_DEBUG) && EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_VULKAN_CONFIRMATION_LOGS
        logger::get(kPlatformLogSys).log(
            logger::LogLevel::INFO,
            "Window configured",
            std::source_location::current());
#endif
    }

    void Application::framebufferResizeCallback(GLFWwindow* window_, int width, int height)
    {
#if defined(EPOCH_VULKAN_STANDALONE)
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

    bool Application::process(std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!device)
            return false;

        bind_render_thread();
        assert_thread_affinity();

        set_active_context(ctx.get());

        static auto lastTime = std::chrono::high_resolution_clock::now();
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

#if defined(EPOCH_VULKAN_STANDALONE)
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
            if (fbWidth != get_framebuffer_width() || fbHeight != get_framebuffer_height())
                set_framebuffer_size(fbWidth, fbHeight);

            framebufferMinimized = ctx->framebufferWidth == 0 || ctx->framebufferHeight == 0;
        }
#if defined(EPOCH_VULKAN_STANDALONE)
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
            if (auto* guiState = find_gui_state(ctx.get()))
            {
                guiState->guiDraws.clear();
                guiState->lastGuiDraws.clear();
            }
            queue.drain();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            return true;
        }
        if (auto* guiState = find_gui_state(ctx.get()))
            guiState->guiDraws.clear();
        // Vulkan should mirror the other backends here without starving frame
        // updates. A small second pass helps absorb same-frame follow-up work
        // without reintroducing the old multi-pass churn.
        constexpr int kMaxDrainPasses = 2;
        int drainPasses = 0;
        do
        {
            const bool drained = queue.drain();
            ++drainPasses;
            if (!drained || queue.depth() == 0 || drainPasses >= kMaxDrainPasses)
                break;
        } while (true);
        const bool queueSettled = queue.depth() == 0;

        if (auto* guiState = find_gui_state(ctx.get()))
        {
            const bool hasLastFrame = !guiState->lastGuiDraws.empty();
            const bool missingCurrentFrame = guiState->guiDraws.empty();
            const bool suspiciouslyPartial =
                hasLastFrame
                && !missingCurrentFrame
                && (guiState->guiDraws.size() * 3u) < (guiState->lastGuiDraws.size() * 2u);

            if ((missingCurrentFrame && !queueSettled) || suspiciouslyPartial)
            {
                if (hasLastFrame)
                    guiState->guiDraws = guiState->lastGuiDraws;
            }
            else
            {
                guiState->lastGuiDraws = guiState->guiDraws;
            }
        }

#if EPOCH_VULKAN_RUNTIME_DIAGNOSTICS
        {
            std::ofstream diag("vulkan_runtime_diag.txt", std::ios::app);
            const auto* guiState = find_gui_state(ctx.get());
            diag << "[ Vulkan ] - process hwnd=" << static_cast<void*>(ctx && ctx->windowData ? ctx->windowData->hwnd : nullptr)
                 << " fb=" << get_framebuffer_width() << "x" << get_framebuffer_height()
                 << " queuedGui=" << (guiState ? guiState->guiDraws.size() : 0)
                 << " queueDepth=" << queue.depth()
                 << " drainPasses=" << drainPasses
                 << " queueSettled=" << (queueSettled ? 1 : 0)
                 << "\n";
        }
#endif

        try
        {
            drawFrame();
        }
        catch (const std::runtime_error& ex)
        {
            const std::string_view message{ ex.what() ? ex.what() : "" };
            const bool windowClosing = ctx
                && ctx->windowData
                && ctx->windowData->get_should_close();
            const bool surfaceGone =
                message.find("getSurfaceCapabilitiesKHR failed") != std::string_view::npos
                || message.find("getSurfaceFormatsKHR failed") != std::string_view::npos
                || message.find("getSurfacePresentModesKHR failed") != std::string_view::npos
                || message.find("presentKHR failed") != std::string_view::npos;
            if (windowClosing || surfaceGone)
            {
                request_render_stop();
                return false;
            }
            throw;
        }
        return true;
    }

    void Application::set_context(std::shared_ptr<epochnamespace::core::Context> ctx, void* nativeWindow)
    {
        context = std::move(ctx);
        nativeWindowHandle = nativeWindow;
        activeGuiContext = context.lock().get();
    }

    void Application::set_active_context(const epochnamespace::core::Context* ctx)
    {
        activeGuiContext = ctx;
    }

    void Application::cleanup_gui_context(const epochnamespace::core::Context* ctx)
    {
        if (!ctx)
            return;

        guiContexts.erase(ctx);
        if (activeGuiContext == ctx)
            activeGuiContext = nullptr;
    }

    Application::GuiContextState& Application::gui_state_for_context(
        const epochnamespace::core::Context* ctx)
    {
        return guiContexts[ctx];
    }

    Application::GuiContextState* Application::find_gui_state(
        const epochnamespace::core::Context* ctx) noexcept
    {
        auto it = guiContexts.find(ctx);
        if (it == guiContexts.end())
            return nullptr;
        return &it->second;
    }

    void Application::reset_gui_swapchain_state(GuiContextState& guiState)
    {
        guiState.guiPipeline.reset();
        guiState.guiUniformBuffers.clear();
        guiState.guiUniformBuffersMemory.clear();
        guiState.guiUniformBuffersMapped.clear();
        guiState.guiAtlases.clear();
        guiState.guiDraws.clear();
        guiState.lastGuiDraws.clear();
    }

    void Application::set_framebuffer_size(int width, int height)
    {
        std::scoped_lock lock(framebufferStateMutex);
        framebufferWidth = (std::max)(1, width);
        framebufferHeight = (std::max)(1, height);
        framebufferResized = true;
    }

    int Application::get_framebuffer_width() const noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        return (std::max)(1, framebufferWidth);
    }

    int Application::get_framebuffer_height() const noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        return (std::max)(1, framebufferHeight);
    }

    bool Application::should_stop_rendering() noexcept
    {
        return consume_render_stop_intent();
    }

    bool Application::consume_framebuffer_resize_intent() noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        const bool resized = framebufferResized;
        framebufferResized = false;
        return resized;
    }

    void Application::set_framebuffer_resize_intent(bool resized) noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        framebufferResized = resized;
    }

    bool Application::consume_render_stop_intent() noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        const bool stopRequested = stopRenderingRequested;
        stopRenderingRequested = false;
        return stopRequested;
    }

    void Application::request_render_stop() noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        stopRenderingRequested = true;
    }

    void Application::bind_render_thread() noexcept
    {
        std::scoped_lock lock(framebufferStateMutex);
        if (renderThreadId == std::thread::id{})
            renderThreadId = std::this_thread::get_id();
    }

    void Application::assert_thread_affinity() const noexcept
    {
#ifndef NDEBUG
        std::scoped_lock lock(framebufferStateMutex);
        const std::thread::id expected = renderThreadId;
        if (expected != std::thread::id{})
            assert(expected == std::this_thread::get_id());
#endif
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

#if defined(EPOCH_VULKAN_STANDALONE)
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
#endif
    }

} // namespace epochnamespace::vulkancontext
