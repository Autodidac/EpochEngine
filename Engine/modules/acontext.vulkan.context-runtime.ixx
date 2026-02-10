module;

#include <include/acontext.vulkan.hpp>

#if defined(ALMOND_VULKAN_STANDALONE)
#   include <GLFW/glfw3.h>
#endif
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

export module acontext.vulkan.context:runtime;

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

import <algorithm>;
import <chrono>;
import <stdexcept>;
import <vector>;

export namespace almondnamespace::vulkancontext
{
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // SINGLE definition lives here.
    // (Declaration is exported from :shared_context.)
    Application& vulkan_app()
    {
        static Application app{};
        return app;
    }

    // ---- Application method definitions ----
//
//    void Application::initWindow()
//    {
//#if defined(ALMOND_VULKAN_STANDALONE)
//        if (!glfwInit())
//            throw std::runtime_error("[Vulkan] Failed to initialize GLFW.");
//
//        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
//
//        window = glfwCreateWindow(framebufferWidth, framebufferHeight, "Almond Vulkan", nullptr, nullptr);
//        if (!window)
//            throw std::runtime_error("[Vulkan] Failed to create GLFW window.");
//
//        glfwSetWindowUserPointer(window, this);
//        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
//        glfwSetCursorPosCallback(window, mouseCallback);
//        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
//#else
//        window = nullptr;
//        if (!nativeWindowHandle)
//            throw std::runtime_error("[Vulkan] Requires a native window handle.");
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
//    bool Application::process(std::shared_ptr<almondnamespace::core::Context> ctx,
//        almondnamespace::core::CommandQueue& queue)
//    {
//        if (!device)
//            return false;
//
//        static auto lastTime = std::chrono::high_resolution_clock::now();
//        const auto currentTime = std::chrono::high_resolution_clock::now();
//        const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
//        lastTime = currentTime;
//
//#if defined(ALMOND_VULKAN_STANDALONE)
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
//#if defined(ALMOND_VULKAN_STANDALONE)
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
//    void Application::set_context(std::shared_ptr<almondnamespace::core::Context> ctx, void* nativeWindow)
//    {
//        context = ctx;
//        nativeWindowHandle = nativeWindow;
//    }
}
