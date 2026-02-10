// ============================================================================
// modules/acontext.vulkan.context-instance.ixx
// Partition: acontext.vulkan.context:instance
// Vulkan instance + surface creation.
// Fixes:
//   - Force Vulkan-Hpp dynamic dispatcher in this partition (so .init exists)
//   - Correct debug callback signature (VKAPI_ATTR/VKAPI_CALL)
//   - Keep your loader-based layer enumeration
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>

#if defined(ALMOND_VULKAN_STANDALONE)
#   include <GLFW/glfw3.h>
#endif

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   include <include/aframework.hpp>
#else
#   include <dlfcn.h>
#endif

// ---------------------------------------------------------------------------
// Vulkan-Hpp config MUST be set BEFORE including vulkan.hpp.
// This partition currently ends up with DispatchLoaderStatic otherwise.
// ---------------------------------------------------------------------------
#include <vulkan/vulkan.hpp>

export module acontext.vulkan.context:instance;

import :shared_context;
import :shared_vk;
import :renderer;

import <algorithm>;
import <cstdint>;
import <cstring>;
import <iostream>;
import <stdexcept>;
import <vector>;

namespace almondnamespace::vulkancontext::detail
{
    using PFN_EnumLayers = VkResult(VKAPI_PTR*)(uint32_t*, VkLayerProperties*);
    using PFN_GetInstanceProcAddr = PFN_vkGetInstanceProcAddr;

    struct Loader
    {
#if defined(_WIN32)
        HMODULE dll{};
#else
        void* so{};
#endif
        PFN_GetInstanceProcAddr getInstanceProcAddr{};
        PFN_EnumLayers enumLayers{};

        static Loader& instance() noexcept
        {
            static Loader g{};
            return g;
        }

        bool ensure_loaded() noexcept
        {
            if (enumLayers) return true;

#if defined(_WIN32)
            if (!dll)
                dll = ::LoadLibraryW(L"vulkan-1.dll");
            if (!dll)
                return false;

            auto gp = reinterpret_cast<PFN_GetInstanceProcAddr>(::GetProcAddress(dll, "vkGetInstanceProcAddr"));
            if (!gp)
                return false;

            getInstanceProcAddr = gp;

            enumLayers = reinterpret_cast<PFN_EnumLayers>(
                getInstanceProcAddr(VkInstance(VK_NULL_HANDLE), "vkEnumerateInstanceLayerProperties"));

            return enumLayers != nullptr;
#else
            if (!so)
            {
                so = ::dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
                if (!so)
                    so = ::dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
            }
            if (!so)
                return false;

            auto gp = reinterpret_cast<PFN_GetInstanceProcAddr>(::dlsym(so, "vkGetInstanceProcAddr"));
            if (!gp)
                return false;

            getInstanceProcAddr = gp;

            enumLayers = reinterpret_cast<PFN_EnumLayers>(
                getInstanceProcAddr(VkInstance(VK_NULL_HANDLE), "vkEnumerateInstanceLayerProperties"));

            return enumLayers != nullptr;
#endif
        }

        bool enumerate_layers(std::vector<VkLayerProperties>& out) noexcept
        {
            if (!ensure_loaded())
                return false;

            uint32_t count = 0;
            VkResult vr = enumLayers(&count, nullptr);
            if (vr != VK_SUCCESS || count == 0)
                return false;

            out.resize(static_cast<std::size_t>(count));
            vr = enumLayers(&count, out.data());
            if (vr != VK_SUCCESS)
                return false;

            out.resize(static_cast<std::size_t>(count));
            return true;
        }
    };
}

namespace almondnamespace::vulkancontext
{
    // -----------------------------------------------------------------------
    // Correct callback signature for Vulkan (fixes PFN mismatch on MSVC).
    // -----------------------------------------------------------------------
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT /*severity*/,
        VkDebugUtilsMessageTypeFlagsEXT /*type*/,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* /*userData*/)
    {
        if (callbackData && callbackData->pMessage)
            std::cerr << "[Vulkan] " << callbackData->pMessage << "\n";
        return VK_FALSE;
    }
}

export namespace almondnamespace::vulkancontext
{
    bool Application::checkValidationLayerSupport()
    {
        std::vector<VkLayerProperties> props;
        if (!detail::Loader::instance().enumerate_layers(props))
            return false;

        for (const char* layerName : validationLayers)
        {
            const bool found = std::any_of(props.begin(), props.end(),
                [&](const VkLayerProperties& p)
                {
                    return std::strcmp(p.layerName, layerName) == 0;
                });

            if (!found)
                return false;
        }
        return true;
    }

    std::vector<const char*> Application::getRequiredExtensions()
    {
        std::vector<const char*> extensions;

#if defined(ALMOND_VULKAN_STANDALONE)
        std::uint32_t glfwCount = 0;
        const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
        if (!glfwExt)
            throw std::runtime_error("glfwGetRequiredInstanceExtensions returned null.");
        extensions.assign(glfwExt, glfwExt + glfwCount);
#else
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#   if defined(_WIN32)
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#   endif
#endif

        if (validationLayersEnabled)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    void Application::createInstance()
    {
        validationLayersEnabled = almondnamespace::vulkanrenderer::vulkan_config.enable_validation_layers;
        if (validationLayersEnabled && !checkValidationLayerSupport())
        {
            std::cerr << "[Vulkan] Validation layers requested but not available; "
                "continuing with validation disabled.\n";
            validationLayersEnabled = false;
        }

        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName = "Epoch Engine Vulkan Backend";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "Epoch Engine";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_0;

        const auto extensions = getRequiredExtensions();

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (validationLayersEnabled)
        {
            createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

#if defined(VULKAN_HPP_NO_EXCEPTIONS) && (VULKAN_HPP_NO_EXCEPTIONS == 1)
        vk::ResultValue<vk::UniqueInstance> rv = vk::createInstanceUnique(createInfo);
        if (rv.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create Vulkan instance.");
        instance = std::move(rv.value);
#else
        instance = vk::createInstanceUnique(createInfo);
#endif

        if (validationLayersEnabled)
            setupDebugMessenger();
    }

    void Application::setupDebugMessenger()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        createInfo.messageType =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        // Use the correctly-typed callback above.
        createInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debugCallback);

        createInfo.pUserData = nullptr;

        auto [mr, messenger] = instance->createDebugUtilsMessengerEXT(createInfo);
        if (mr != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create Vulkan debug messenger.");

        debugMessenger = messenger;
    }

    void Application::createSurface()
    {
#if defined(ALMOND_VULKAN_STANDALONE)
        VkSurfaceKHR rawSurface{};
        if (glfwCreateWindowSurface(instance.get(), window, nullptr, &rawSurface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create GLFW window surface.");

        surface = vk::UniqueSurfaceKHR(
            vk::SurfaceKHR(rawSurface),
            vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic>(
            );
#elif defined(_WIN32)
        if (!nativeWindowHandle)
            throw std::runtime_error("No native window handle available for Vulkan surface.");

        vk::Win32SurfaceCreateInfoKHR sci{};
        sci.hinstance = ::GetModuleHandleW(nullptr);
        sci.hwnd = static_cast<HWND>(nativeWindowHandle);

        auto [sr, s] = instance->createWin32SurfaceKHRUnique(sci);
        if (sr != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create Win32 Vulkan surface.");

        surface = std::move(s);
#else
        throw std::runtime_error("Vulkan surface creation not implemented for this platform.");
#endif
    }
} // namespace almondnamespace::vulkancontext
