#pragma once

#define GLFW_INCLUDE_VULKAN
#include <vector>
#include <GLFW/glfw3.h>

const std::vector<const char*> gValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> gDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool gEnableValidationLayers = false;
#else
const bool gEnableValidationLayers = true;
#endif

inline VkResult CreateDebugUtilsMessengerEXT(VkInstance Instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator,
                                             VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(Instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

inline void DestroyDebugUtilsMessengerEXT(VkInstance Instance, VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(Instance, debugMessenger, pAllocator);
    }
}

class OpaaxVulkanContext
{
    //-----------------------------------------------------------------
    // Members
    //-----------------------------------------------------------------
    /*---------------------------- PRIVATE ----------------------------*/
    
    GLFWwindow*                 m_window;
    VkInstance                  m_vkInstance;
    VkDebugUtilsMessengerEXT    m_vkDebugMessenger;
    
    //-----------------------------------------------------------------
    // CTOR DTOR
    //-----------------------------------------------------------------
public:
    OpaaxVulkanContext(GLFWwindow* Window);
    ~OpaaxVulkanContext();
    
    //-----------------------------------------------------------------
    // Functions
    //-----------------------------------------------------------------
    /*---------------------------- PRIVATE ----------------------------*/
    void CreateInstance();
    void SetupDebugMessenger();
    bool CheckValidationLayerSupport();
    
    /*---------------------------- PUBLIC ----------------------------*/
    void Init();
    void Shutdown();
};
