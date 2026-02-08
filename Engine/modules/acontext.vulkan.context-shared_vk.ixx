// ============================================================================
// modules/acontext.vulkan.context-shared_vk.ixx
// Partition: acontext.vulkan.context:shared_vk
// Vulkan-facing shared types + Application declaration.
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>

#if defined(ALMOND_VULKAN_STANDALONE)
#   ifndef GLFW_INCLUDE_VULKAN
#       define GLFW_INCLUDE_VULKAN
#   endif
#   include <GLFW/glfw3.h>
#endif

// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

export module acontext.vulkan.context:shared_vk;

#if !defined(ALMOND_VULKAN_STANDALONE)
struct GLFWwindow; // engine-owned window integration: don't drag GLFW into the BMI
#endif

import :shared_context;

import <array>;
import <cstddef>;
import <cstdint>;
import <functional>;
import <optional>;
import <span>;
import <string>;
import <unordered_map>;
import <utility>;
import <vector>;

import aengine.context.commandqueue;
import aengine.core.context;
import aengine.input;
import acontext.vulkan.camera;
import aatlas.texture;
import aspritehandle;

namespace almondnamespace::vulkancontext
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

        bool process(std::shared_ptr<almondnamespace::core::Context> ctx,
            almondnamespace::core::CommandQueue& queue);

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

        void set_context(std::shared_ptr<almondnamespace::core::Context> ctx, void* nativeWindow);
        void set_active_context(const almondnamespace::core::Context* ctx);
        void cleanup_gui_context(const almondnamespace::core::Context* ctx);

        vk::CommandBuffer getCurrentCommandBuffer() const
        {
            return *commandBuffers[currentFrame];
        }

        void enqueue_gui_draw(
            const almondnamespace::core::Context* ctx,
            const almondnamespace::SpriteHandle& sprite,
            std::span<const almondnamespace::TextureAtlas* const> atlases,
            float x,
            float y,
            float w,
            float h);
        void ensure_gui_atlas(const almondnamespace::TextureAtlas& atlas);

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

    private:
        std::weak_ptr<almondnamespace::core::Context> context;

        int framebufferWidth = 800;
        int framebufferHeight = 600;

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
        bool framebufferResized = false;

        vk::UniqueImage depthImage;
        vk::UniqueDeviceMemory depthImageMemory;
        vk::UniqueImageView depthImageView;

        vk::UniqueImage textureImage;
        vk::UniqueDeviceMemory textureImageMemory;
        vk::UniqueImageView textureImageView;
        vk::UniqueSampler textureSampler;

        bool validationLayersEnabled = false;

        inline static almondnamespace::vulkancamera::State cam =
            almondnamespace::vulkancamera::create(
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

        std::uint32_t findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        vk::UniqueCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(vk::UniqueCommandBuffer& commandBuffer);

        void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

        void createUniformBuffers();
        void updateUniformBuffer(std::uint32_t currentImage,
            const almondnamespace::vulkancamera::State& camera);
        GuiContextState& gui_state_for_context(const almondnamespace::core::Context* ctx);
        GuiContextState* find_gui_state(const almondnamespace::core::Context* ctx) noexcept;
        void reset_gui_swapchain_state(GuiContextState& guiState);

        void createDescriptorPool();
        void createDescriptorSets();

        void createCommandBuffers();
        void createSyncObjects();

        void recreateSwapChain();
        void cleanupSwapChain();

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

        std::unordered_map<const almondnamespace::core::Context*, GuiContextState> guiContexts{};
        const almondnamespace::core::Context* activeGuiContext = nullptr;
    };

    export std::span<const Application::Vertex> cube_vertices() noexcept;
    export std::span<const std::uint16_t>       cube_indices()  noexcept;

    export Application& vulkan_app();
}
