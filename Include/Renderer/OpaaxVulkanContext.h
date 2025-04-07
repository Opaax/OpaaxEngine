#pragma once

#define GLFW_INCLUDE_VULKAN
#include <optional>
#include <vector>
#include <GLFW/glfw3.h>

#include "OpaaxTypes.h"

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

// Queue family indices
struct QueueFamilyIndices
{
    std::optional<UInt32> GraphicsFamily;
    std::optional<UInt32> PresentFamily;

    bool IsComplete() const
    {
        return GraphicsFamily.has_value() && PresentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class OpaaxVulkanContext
{
    //-----------------------------------------------------------------
    // Members
    //-----------------------------------------------------------------
    /*---------------------------- PRIVATE ----------------------------*/
    
    GLFWwindow*                 m_window;
    VkInstance                  m_vkInstance;
    VkDebugUtilsMessengerEXT    m_vkDebugMessenger;

    VkSurfaceKHR m_vkSurface;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    
    QueueFamilyIndices m_queueIndices;

    // Logical device and queues
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkQueue m_vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_vkPresentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR m_vkSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_vkSwapchainImages;
    VkFormat m_vkSwapchainImageFormat;
    VkExtent2D m_vkSwapchainExtent;
    
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

    void CreateSurface();
    void PickPhysicalDevice();
    bool IsDeviceSuitable(VkPhysicalDevice Device);
    
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device);
    void CreateLogicalDevice();

    void CreateSwapchain();
    SwapChainSupportDetails QuerySwapChainSupportForVK(VkPhysicalDevice Device);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& PresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities);

    QueueFamilyIndices FindQueueFamiliesForVK(VkPhysicalDevice Device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats);
    
    /*---------------------------- PUBLIC ----------------------------*/
    void Init();
    void Shutdown();
};
