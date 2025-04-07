#include <Renderer/OpaaxVulkanContext.h>

#include <stdexcept>

#include "OpaaxTypes.h"
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

void OpaaxVulkanContext::Init()
{
    CreateInstance();
    SetupDebugMessenger();
}

void OpaaxVulkanContext::Shutdown()
{
    if (gEnableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugMessenger, nullptr);
    }

    vkDestroyInstance(m_vkInstance, nullptr);
}
