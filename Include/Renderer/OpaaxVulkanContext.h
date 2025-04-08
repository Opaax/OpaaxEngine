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

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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

    std::vector<VkImageView> m_vkSwapchainImageViews;

    VkRenderPass m_vkRenderPass = VK_NULL_HANDLE;
    
    std::vector<VkFramebuffer> m_vkSwapchainFrameBuffers;

    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_vkCommandBuffers;

    std::vector<VkSemaphore> m_vkImageAvailableSemaphores;
    std::vector<VkSemaphore> m_vkRenderFinishedSemaphores;
    std::vector<VkFence>     m_vkInFlightFences;

    size_t m_currentFrame = 0;
    
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

    void CreateImageViews();
    
    void CreateRenderPass();
    
    void CreateFrameBuffers();

    void CreateCommandPool();
    void CreateCommandBuffers();
    void RecordCommandBuffers();

    void CreateSyncObjects();
    void DrawFrame();
    
    /*---------------------------- PUBLIC ----------------------------*/
    void Init();
    void Shutdown();
};
