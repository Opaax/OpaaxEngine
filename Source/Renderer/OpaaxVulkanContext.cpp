#include <set>
#include <Renderer/OpaaxVulkanContext.h>

#include <stdexcept>
#include "Core/OPLogMacro.h"

OpaaxVulkanContext::OpaaxVulkanContext(GLFWwindow* Window)
    : m_window(Window), m_vkInstance(VK_NULL_HANDLE), m_vkDebugMessenger(VK_NULL_HANDLE) {}

OpaaxVulkanContext::~OpaaxVulkanContext()
{
    Shutdown();
}

void OpaaxVulkanContext::CreateInstance()
{
    // Check if validation layers are enabled and supported, throw error if unsupported
    if (gEnableValidationLayers && !CheckValidationLayerSupport())
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Validation layers requested but not available!")
        throw std::runtime_error("Validation layers requested but not available!");
    }

    // Create and initialize Vulkan application info structure
    VkApplicationInfo lAppInfo{};
    lAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // Struct type
    lAppInfo.pApplicationName = "OpaaxGameEngine"; // Application name
    lAppInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1); // Application version
    lAppInfo.pEngineName = "OpaaxEngine"; // Engine name
    lAppInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1); // Engine version
    lAppInfo.apiVersion = VK_API_VERSION_1_2; // Vulkan API version

    // Get required extensions from GLFW
    UInt32 lGlfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&lGlfwExtCount);

    // Collect required extensions into a vector
    std::vector<const char*> lExtensions(glfwExtensions, glfwExtensions + lGlfwExtCount);

    // Add debug extension if validation layers are enabled
    if (gEnableValidationLayers)
    {
        lExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Create and initialize Vulkan instance creation info structure
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // Struct type
    createInfo.pApplicationInfo = &lAppInfo; // Pointer to application info struct
    createInfo.enabledExtensionCount = static_cast<uint32_t>(lExtensions.size()); // Number of extensions
    createInfo.ppEnabledExtensionNames = lExtensions.data(); // List of extensions

    // Handle validation layers
    if (gEnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size()); // Number of validation layers
        createInfo.ppEnabledLayerNames = gValidationLayers.data(); // List of validation layers
    }
    else
    {
        createInfo.enabledLayerCount = 0; // No validation layers
    }

    // Attempt to create Vulkan instance and throw error if it fails
    if (vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create Vulkan instance.")
        throw std::runtime_error("Failed to create Vulkan instance.");
    }
}

void OpaaxVulkanContext::SetupDebugMessenger()
{
    if (!gEnableValidationLayers)
    {
        // If validation layers are not enabled, exit the function
        return;
    }

    // Initialize the Vulkan debug messenger create info structure
    VkDebugUtilsMessengerCreateInfoEXT lCreateInfo{};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // Structure type
    lCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | // Enable warning messages
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // Enable error messages

    lCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | // General messages
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | // Validation messages
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Performance messages

    // Callback function to handle validation layer messages
    lCreateInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT,
                                     VkDebugUtilsMessageTypeFlagsEXT,
                                     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                     void*) -> VkBool32
    {
        // Log the validation layer message
        OPAAX_ERROR("Validation Layer: %1%", %pCallbackData->pMessage)
        return VK_FALSE; // Continue execution after the message
    };

    // Attempt to create the Vulkan debug messenger
    if (CreateDebugUtilsMessengerEXT(m_vkInstance, &lCreateInfo, nullptr, &m_vkDebugMessenger) != VK_SUCCESS)
    {
        // Throw an error if the debug messenger creation fails
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to set up debug messenger!")
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

bool OpaaxVulkanContext::CheckValidationLayerSupport()
{
    UInt32 lLayerCount;
    // Query the number of available Vulkan instance layers
    vkEnumerateInstanceLayerProperties(&lLayerCount, nullptr);

    // Retrieve the layer properties of all available Vulkan instance layers
    std::vector<VkLayerProperties> lAvailableLayers(lLayerCount);
    vkEnumerateInstanceLayerProperties(&lLayerCount, lAvailableLayers.data());

    // Loop through the validation layers we want to use (gValidationLayers)
    for (const char* lLayerName : gValidationLayers)
    {
        bool lbFound = false;
        // Check if the current validation layer is supported by the available layers
        for (const auto& layerProp : lAvailableLayers)
        {
            if (strcmp(lLayerName, layerProp.layerName) == 0) // Compare layer names
            {
                lbFound = true; // Validation layer is found
                break;
            }
        }
        // If any layer from gValidationLayers is not found, return false
        if (!lbFound)
        {
            return false;
        }
    }

    // All required validation layers are supported
    return true;
}

void OpaaxVulkanContext::CreateSurface()
{
    if (glfwCreateWindowSurface(m_vkInstance, m_window, nullptr, &m_vkSurface) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create window surface!")
        throw std::runtime_error("Failed to create window surface!");
    }
}

void OpaaxVulkanContext::PickPhysicalDevice()
{
    // Query the number of available physical devices
    UInt32 lDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_vkInstance, &lDeviceCount, nullptr);

    // Check if there are any physical devices available
    if (lDeviceCount == 0)
    {
        // Log an error if no Vulkan-supported GPUs are found
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to find GPUs with Vulkan support!")
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    // Retrieve the list of physical devices
    std::vector<VkPhysicalDevice> lDevices(lDeviceCount);
    vkEnumeratePhysicalDevices(m_vkInstance, &lDeviceCount, lDevices.data());

    // Iterate over the list of physical devices to find a suitable device
    for (const auto& lDevice : lDevices)
    {
        // Check if the current device meets the requirements
        if (IsDeviceSuitable(lDevice))
        {
            // Store the suitable device and stop searching
            m_vkPhysicalDevice = lDevice;
            break;
        }
    }

    // Verify if a suitable GPU was found
    if (m_vkPhysicalDevice == VK_NULL_HANDLE)
    {
        // Log an error if no suitable GPU was found
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to find a suitable GPU!")
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &props);
   OPAAX_VERBOSE("[OpaaxVulkanContext] Picked GPU: %1%", %props.deviceName)
}

bool OpaaxVulkanContext::IsDeviceSuitable(VkPhysicalDevice Device)
{
    VkPhysicalDeviceProperties lDeviceProperties;
    vkGetPhysicalDeviceProperties(Device, &lDeviceProperties);

    VkPhysicalDeviceFeatures lDeviceFeatures;
    vkGetPhysicalDeviceFeatures(Device, &lDeviceFeatures);
    
    return lDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        || lDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

QueueFamilyIndices OpaaxVulkanContext::FindQueueFamilies(VkPhysicalDevice Device)
{
    QueueFamilyIndices lIndices;

    UInt32 lQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(lQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, queueFamilies.data());

    // Evaluate each queue family
    for (UInt32 i = 0; i < lQueueFamilyCount; ++i)
    {
        const auto& queueFamily = queueFamilies[i];

        // Check for graphics support
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            lIndices.GraphicsFamily = i;
        }

        // Check for presentation support
        VkBool32 lPresentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, m_vkSurface, &lPresentSupport);

        if (lPresentSupport)
        {
            lIndices.PresentFamily = i;
        }

        if (lIndices.IsComplete())
        {
            break;
        }
    }

    return lIndices;
}

void OpaaxVulkanContext::CreateLogicalDevice()
{
    m_queueIndices = FindQueueFamilies(m_vkPhysicalDevice);

    if (!m_queueIndices.IsComplete())
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to find suitable queue families.")
        throw std::runtime_error("Queue families incomplete");
    }

    std::vector<VkDeviceQueueCreateInfo> lQueueCreateInfos;
    std::set<UInt32> uniqueFamilies = {
        m_queueIndices.GraphicsFamily.value(),
        m_queueIndices.PresentFamily.value()
    };

    float lQueuePriority = 1.0f;
    for (uint32_t lQueueFamily : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo lQueueCreateInfo{};
        
        lQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        lQueueCreateInfo.queueFamilyIndex = lQueueFamily;
        lQueueCreateInfo.queueCount = 1;
        lQueueCreateInfo.pQueuePriorities = &lQueuePriority;
        lQueueCreateInfos.push_back(lQueueCreateInfo);
    }

    VkPhysicalDeviceFeatures lDeviceFeatures{}; // Fill later

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(lQueueCreateInfos.size());
    createInfo.pQueueCreateInfos = lQueueCreateInfos.data();
    createInfo.pEnabledFeatures = &lDeviceFeatures;

    // Validation layers for logical device (legacy)
    if (gEnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<UInt32>(gValidationLayers.size());
        createInfo.ppEnabledLayerNames = gValidationLayers.data();
    } else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(m_vkPhysicalDevice, &createInfo, nullptr, &m_vkDevice) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create logical device.")
        throw std::runtime_error("Failed to create logical device.");
    }

    // Retrieve queue handles
    vkGetDeviceQueue(m_vkDevice, m_queueIndices.GraphicsFamily.value(), 0, &m_vkGraphicsQueue);
    vkGetDeviceQueue(m_vkDevice, m_queueIndices.PresentFamily.value(), 0, &m_vkPresentQueue);

    OPAAX_VERBOSE("[OpaaxVulkanContext] Logical device and queues successfully created.")
}

void OpaaxVulkanContext::CreateSwapchain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupportForVK(m_vkPhysicalDevice);

    VkSurfaceFormatKHR lSurfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(swapChainSupport.presentModes);
    VkExtent2D lExtent = ChooseSwapExtent(swapChainSupport.capabilities);

    uint32_t lImageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && lImageCount > swapChainSupport.capabilities.maxImageCount)
    {
        lImageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_vkSurface;

    createInfo.minImageCount = lImageCount;
    createInfo.imageFormat = lSurfaceFormat.format;
    createInfo.imageColorSpace = lSurfaceFormat.colorSpace;
    createInfo.imageExtent = lExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamiliesForVK(m_vkPhysicalDevice);
    uint32_t queueFamilyIndices[] = {indices.GraphicsFamily.value(), indices.PresentFamily.value()};

    if (indices.GraphicsFamily != indices.PresentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_vkDevice, &createInfo, nullptr, &m_vkSwapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &lImageCount, nullptr);
    m_vkSwapchainImages.resize(lImageCount);
    vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &lImageCount, m_vkSwapchainImages.data());

    m_vkSwapchainImageFormat = lSurfaceFormat.format;
    m_vkSwapchainExtent = lExtent;
}

SwapChainSupportDetails OpaaxVulkanContext::QuerySwapChainSupportForVK(VkPhysicalDevice Device)
{
    SwapChainSupportDetails lDetails;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, m_vkSurface, &lDetails.capabilities);

    uint32_t lFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Device, m_vkSurface, &lFormatCount, nullptr);

    if (lFormatCount != 0)
    {
        lDetails.formats.resize(lFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(Device, m_vkSurface, &lFormatCount, lDetails.formats.data());
    }

    uint32_t lPresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(Device, m_vkSurface, &lPresentModeCount, nullptr);

    if (lPresentModeCount != 0)
    {
        lDetails.presentModes.resize(lPresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(Device, m_vkSurface, &lPresentModeCount, lDetails.presentModes.data());
    }

    return lDetails;
}

VkSurfaceFormatKHR OpaaxVulkanContext::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats)
{
    for (const auto& lFormat : Formats)
    {
        if (lFormat.format == VK_FORMAT_B8G8R8A8_SRGB && lFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return lFormat;
        }
    }
    
    return Formats[0];
}

VkPresentModeKHR OpaaxVulkanContext::ChoosePresentMode(const std::vector<VkPresentModeKHR>& PresentModes)
{
    for (const auto& lMode : PresentModes)
    {
        if (lMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return lMode; // Triple buffering
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D OpaaxVulkanContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities)
{
    //TODO Use Custom math
    if (Capabilities.currentExtent.width != UINT32_MAX)
    {
        return Capabilities.currentExtent;
    }
    else
    {
        int lWidth, lHeight;
        glfwGetFramebufferSize(m_window, &lWidth, &lHeight);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(lWidth),
            static_cast<uint32_t>(lHeight)
        };

        //TODO Use Custom math
        actualExtent.width = std::max(Capabilities.minImageExtent.width,
                                      std::min(Capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(Capabilities.minImageExtent.height,
                                       std::min(Capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

QueueFamilyIndices OpaaxVulkanContext::FindQueueFamiliesForVK(VkPhysicalDevice Device)
{
    QueueFamilyIndices lIndices;

    uint32_t lQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> lQueueFamilies(lQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, lQueueFamilies.data());

    int i = 0;
    for (const auto& lQueueFamily : lQueueFamilies)
    {
        if (lQueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, m_vkSurface, &presentSupport);

            if (presentSupport)
            {
                lIndices.PresentFamily = i;
            }

            lIndices.GraphicsFamily = i;
        }

        if (lIndices.IsComplete())
        {
            break;
        }

        i++;
    }

    return lIndices;
}

void OpaaxVulkanContext::Init()
{
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
}

void OpaaxVulkanContext::Shutdown()
{
    if (gEnableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugMessenger, nullptr);
    }

    if (m_vkSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_vkDevice, m_vkSwapchain, nullptr);
    }

    if (m_vkDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_vkDevice, nullptr);
    }

    if (gEnableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugMessenger, nullptr);
    }

    if (m_vkSurface)
    {
        vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, nullptr);
    }
    
    vkDestroyInstance(m_vkInstance, nullptr);
}
