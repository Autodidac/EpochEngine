// ============================================================================
// modules/acontext.vulkan.context-instance.ixx
// Partition: acontext.vulkan.context:instance
// Vulkan instance + surface creation.
// Fix: avoid calling an invalid global vk* entrypoint by dynamically resolving
// vkEnumerateInstanceLayerProperties via the Vulkan loader at runtime.
// This works whether you're linked to vulkan-1.lib or using VK_NO_PROTOTYPES/volk.
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
#   include <windows.h> // for LoadLibrary/GetProcAddress
#   include <include/aframework.hpp>
#else
#   include <dlfcn.h> // dlopen/dlsym
#endif

// Include Vulkan-Hpp after config.
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
                // Most common loader name on Linux
                so = ::dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
                if (!so)
                {
                    // Fallback
                    so = ::dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
                }
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
        // Important: this now uses the Vulkan loader safely (no invalid vk* call)
        validationLayersEnabled = almondnamespace::vulkanrenderer::vulkan_config.enable_validation_layers;
        if (validationLayersEnabled && !checkValidationLayerSupport())
        {
            std::cerr << "[Vulkan] Validation layers requested but not available; "
                         "continuing with validation disabled.\n";
            validationLayersEnabled = false;
        }

        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName = "AlmondEngine Vulkan";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "AlmondEngine";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_0;

        const auto extensions = getRequiredExtensions();

        vk::InstanceCreateInfo createInfo{};
        createInfo.flags = {};
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

        auto [r, inst] = vk::createInstanceUnique(createInfo);
        if (r != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create Vulkan instance.");

        instance = std::move(inst);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());
    }

    void Application::createSurface()
    {
#if defined(ALMOND_VULKAN_STANDALONE)
        VkSurfaceKHR rawSurface{};
        if (glfwCreateWindowSurface(instance.get(), window, nullptr, &rawSurface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create GLFW window surface.");

        surface = vk::UniqueSurfaceKHR(
            vk::SurfaceKHR(rawSurface),
            vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(
                instance.get(), nullptr, VULKAN_HPP_DEFAULT_DISPATCHER)
        );
#elif defined(_WIN32)
        if (!nativeWindowHandle)
            throw std::runtime_error("No native window handle available for Vulkan surface.");

        vk::Win32SurfaceCreateInfoKHR sci{};
        sci.flags = {};
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
