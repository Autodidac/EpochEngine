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
// modules/vulkan.context-shared_vk.ixx
// Partition: vulkan.context:shared_vk
// Vulkan-facing shared types + Application declaration.
// ============================================================================

module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/renderers/vulkan/vulkan.context_shared.hpp>

#if defined(EPOCH_VULKAN_STANDALONE)
#   ifndef GLFW_INCLUDE_VULKAN
#       define GLFW_INCLUDE_VULKAN
#   endif
#   include <GLFW/glfw3.h>
#endif

// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

export module vulkan.context:shared_vk;

import :shared_context;

import context.commandqueue;
import core.context;
import input.engine;
import vulkan.camera;
import atlas.texture;
import sprite.handle;

#if !defined(EPOCH_VULKAN_STANDALONE)
struct GLFWwindow; // engine-owned window integration: don't drag GLFW into the BMI
#endif

namespace epochengine::vulkancontext
{
    // Debug callback for validation layers
    inline VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
        const VkDebugUtilsMessengerCallbackDataEXT* /*pCallbackData*/,
        void* /*pUserData*/)
    {
        (void)messageSeverity;
        return VK_FALSE;
    }

    export struct SwapChainSupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities{};
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    export class Application
    {
    public:
        struct GuiContextState;

        void run();

        void initWindow();
        void initVulkan();

        bool process(std::shared_ptr<epochengine::core::Context> ctx,
            epochengine::core::CommandQueue& queue);

        void cleanup();

        void createVertexBuffer();
        void createIndexBuffer();
        void createGuiUniformBuffers();
        void updateGuiUniformBuffer(std::uint32_t currentImage);
        void createGuiPipeline();
        void recordCommandBuffer(std::uint32_t imageIndex);
        void recordGuiCommands(vk::CommandBuffer cmd, std::uint32_t imageIndex, struct GuiContextState& guiState);

        void drawFrame();

        void set_framebuffer_size(int width, int height);
        int get_framebuffer_width() const noexcept;
        int get_framebuffer_height() const noexcept;

        void set_context(std::shared_ptr<epochengine::core::Context> ctx, void* nativeWindow);
        void set_active_context(const epochengine::core::Context* ctx);
        void cleanup_gui_context(const epochengine::core::Context* ctx);
        bool should_stop_rendering() noexcept;

        vk::CommandBuffer getCurrentCommandBuffer() const
        {
            return *commandBuffers[currentFrame];
        }

        void enqueue_gui_draw(
            const epochengine::core::Context* ctx,
            const epochengine::SpriteHandle& sprite,
            std::span<const epochengine::TextureAtlas* const> atlases,
            float x,
            float y,
            float w,
            float h);
        void ensure_gui_atlas(const epochengine::TextureAtlas& atlas);

        std::vector<vk::Image> swapChainImages;

        // Window
        GLFWwindow* window = nullptr;
        void* nativeWindowHandle = nullptr;

        // Device
        vk::UniqueDevice device;
        vk::PhysicalDevice physicalDevice;

        // Surface/commands
        void createSurface();
        vk::UniqueCommandPool commandPool;
        std::vector<vk::UniqueCommandBuffer> commandBuffers;

        bool checkValidationLayerSupport();

        std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
            createBuffer(vk::DeviceSize size,
                vk::BufferUsageFlags usage,
                vk::MemoryPropertyFlags properties);

        std::uint32_t indexCount = 0;
        std::uint32_t solidIndexCount = 0;
        std::uint32_t arcadeScreenIndexOffset = 0;
        std::uint32_t arcadeScreenIndexCount = 0;
        std::uint32_t lineIndexCount = 0;

    private:
        std::weak_ptr<epochengine::core::Context> context;
        const epochengine::core::Context* activeGuiContext = nullptr;

        mutable std::mutex framebufferStateMutex;
        int framebufferWidth = 800;
        int framebufferHeight = 600;
        bool framebufferResized = false;
        bool stopRenderingRequested = false;

        std::thread::id renderThreadId{};
        std::uint64_t previewGeometryRevision = 0;

        vk::UniqueInstance instance;
        vk::DebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        vk::UniqueSurfaceKHR surface;

        vk::Queue graphicsQueue;
        vk::Queue presentQueue;
        QueueFamilyIndices queueFamilyIndices;

        vk::UniqueSwapchainKHR swapChain;
        vk::Format swapChainImageFormat{};
        vk::Extent2D swapChainExtent{};
        std::vector<vk::UniqueImageView> swapChainImageViews;

        vk::UniqueRenderPass renderPass;
        vk::UniqueDescriptorSetLayout descriptorSetLayout;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniquePipeline graphicsPipeline;
        vk::UniquePipeline solidGraphicsPipeline;
        vk::UniquePipeline arcadeScreenPipeline;

        std::vector<vk::UniqueFramebuffer> framebuffers;

        vk::UniqueBuffer vertexBuffer;
        vk::UniqueDeviceMemory vertexBufferMemory;

        vk::UniqueBuffer indexBuffer;
        vk::UniqueDeviceMemory indexBufferMemory;

        std::vector<vk::UniqueBuffer> uniformBuffers;
        std::vector<vk::UniqueDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        vk::UniqueDescriptorPool descriptorPool;
        std::vector<vk::UniqueDescriptorSet> descriptorSets;

        std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
        std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
        std::vector<vk::UniqueFence> inFlightFences;

        std::size_t currentFrame = 0;

        vk::UniqueImage depthImage;
        vk::UniqueDeviceMemory depthImageMemory;
        vk::UniqueImageView depthImageView;

        vk::UniqueImage textureImage;
        vk::UniqueDeviceMemory textureImageMemory;
        vk::UniqueImageView textureImageView;
        vk::UniqueSampler textureSampler;

        vk::Extent2D arcadeRenderExtent{};
        vk::UniqueRenderPass arcadeRenderPass;
        vk::UniqueImage arcadeRenderImage;
        vk::UniqueDeviceMemory arcadeRenderImageMemory;
        vk::UniqueImageView arcadeRenderImageView;
        vk::UniqueSampler arcadeRenderSampler;
        vk::UniqueFramebuffer arcadeRenderFramebuffer;
        vk::UniqueDescriptorPool arcadeDescriptorPool;
        std::vector<vk::UniqueDescriptorSet> arcadeDescriptorSets;
        std::uint64_t arcadePreviewFrame = 0;

        bool validationLayersEnabled = false;

        inline static epochengine::vulkancamera::State cam =
            epochengine::vulkancamera::create(
                glm::vec3(0.0f, 0.0f, 5.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                -90.0f, 0.0f);

        float lastX = 400.0f;
        float lastY = 300.0f;
        bool firstMouse = true;

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
        static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

        void processMouseInput(double xpos, double ypos);
        void updateCamera(float deltaTime);

        void createInstance();
        std::vector<const char*> getRequiredExtensions();

        vk::PhysicalDevice pickPhysicalDevice();
        bool isDeviceSuitable(vk::PhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
        bool checkDeviceExtensionSupport(vk::PhysicalDevice device);

        void createLogicalDevice();
        void createCommandPool();

        SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
        vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes);
        vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

        void createSwapChain();
        vk::UniqueImageView createImageViewUnique(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags);
        void createImageViews();

        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();

        void createDepthResources();
        bool hasStencilComponent(vk::Format format) noexcept;
        vk::Format findDepthFormat();
        vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
            vk::ImageTiling tiling,
            vk::FormatFeatureFlags features);

        void createFramebuffers();

        void createTextureImage();
        void transitionImageLayout(vk::Image image, vk::Format format,
            vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
        void copyBufferToImage(vk::Buffer buffer, vk::Image image,
            std::uint32_t width, std::uint32_t height);
        void createTextureImageView();
        void createTextureSampler();
        void createArcadeRenderTarget();
        void destroyArcadeRenderTarget() noexcept;
        void createArcadeDescriptorSets();
        void recordArcadeRenderTexturePass(vk::CommandBuffer commandBuffer);

        std::uint32_t findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        vk::UniqueCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(vk::UniqueCommandBuffer& commandBuffer);

        void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

        void createUniformBuffers();
        void updateUniformBuffer(std::uint32_t currentImage,
            const epochengine::vulkancamera::State& camera);
        GuiContextState& gui_state_for_context(const epochengine::core::Context* ctx);
        GuiContextState* find_gui_state(const epochengine::core::Context* ctx) noexcept;
        const epochengine::core::Context* bound_context() const noexcept;
        void reset_gui_swapchain_state(GuiContextState& guiState);

        void createDescriptorPool();
        void createDescriptorSets();

        void createCommandBuffers();
        void createSyncObjects();

        void recreateSwapChain();
        void cleanupSwapChain();
        bool consume_framebuffer_resize_intent() noexcept;
        void set_framebuffer_resize_intent(bool resized) noexcept;
        bool consume_render_stop_intent() noexcept;
        void request_render_stop() noexcept;
        void bind_render_thread() noexcept;
        void assert_thread_affinity() const noexcept;

        static std::vector<char> readFile(const std::string& filename);

    public:
        struct Vertex
        {
            glm::vec3 pos{};
            glm::vec3 normal{};
            glm::vec2 texCoord{};

            static vk::VertexInputBindingDescription getBindingDescription()
            {
                return { 0u, static_cast<std::uint32_t>(sizeof(Vertex)), vk::VertexInputRate::eVertex };
            }

            static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
            {
                return { {
                    { 0u, 0u, vk::Format::eR32G32B32Sfloat, static_cast<std::uint32_t>(offsetof(Vertex, pos)) },
                    { 1u, 0u, vk::Format::eR32G32B32Sfloat, static_cast<std::uint32_t>(offsetof(Vertex, normal)) },
                    { 2u, 0u, vk::Format::eR32G32Sfloat,    static_cast<std::uint32_t>(offsetof(Vertex, texCoord)) },
                } };
            }
        };

        struct GuiDrawCommand
        {
            const TextureAtlas* atlas{};
            std::uint32_t localIndex{};
            float x{};
            float y{};
            float w{};
            float h{};
        };

        struct GuiAtlasResources
        {
            vk::UniqueImage image{};
            vk::UniqueDeviceMemory memory{};
            vk::UniqueImageView view{};
            vk::UniqueSampler sampler{};
            vk::UniqueDescriptorPool descriptorPool{};
            std::vector<vk::UniqueDescriptorSet> descriptorSets{};
            std::uint64_t version{ 0 };
            std::uint32_t width{ 0 };
            std::uint32_t height{ 0 };
        };

        struct GuiContextState
        {
            std::unordered_map<const TextureAtlas*, GuiAtlasResources> guiAtlases{};

            vk::UniqueBuffer guiVertexBuffer{};
            vk::UniqueDeviceMemory guiVertexBufferMemory{};
            vk::UniqueBuffer guiIndexBuffer{};
            vk::UniqueDeviceMemory guiIndexBufferMemory{};
            std::size_t guiVertexCapacity = 0;
            std::size_t guiIndexCapacity = 0;

            std::vector<vk::UniqueBuffer> guiUniformBuffers;
            std::vector<vk::UniqueDeviceMemory> guiUniformBuffersMemory;
            std::vector<void*> guiUniformBuffersMapped;
            vk::UniquePipeline guiPipeline;
            std::vector<GuiDrawCommand> guiDraws{};
        };

        std::unordered_map<const epochengine::core::Context*, GuiContextState> guiContexts{};
    };

    export std::vector<Application::Vertex> preview_vertices_for(const epochengine::core::Context* ctx);
    export std::vector<std::uint16_t>       preview_indices_for(const epochengine::core::Context* ctx);
    export std::uint32_t                    preview_solid_index_count_for(const epochengine::core::Context* ctx);
    export std::uint32_t                    preview_arcade_screen_index_count_for(const epochengine::core::Context* ctx);

    export Application& bind_vulkan_app(const std::shared_ptr<epochengine::core::Context>& ctx);
    export std::shared_ptr<Application> try_get_vulkan_app(const epochengine::core::Context* ctx) noexcept;
    export std::shared_ptr<Application> take_vulkan_app(const epochengine::core::Context* ctx) noexcept;
    export bool has_vulkan_apps() noexcept;
}
