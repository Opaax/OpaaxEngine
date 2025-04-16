#include "Renderer/Vulkan/OpaaxVulkanInstance.h"
#include "OpaaxTypes.h"
#include "Core/OPLogMacro.h"
#include "Renderer/OpaaxWindow.h"
#include "Renderer/Vulkan/OpaaxVulkanHelper.h"

OPAAX::VULKAN::OpaaxVulkanInstance::OpaaxVulkanInstance()
{
    OPAAX_VERBOSE("============== [OpaaxVulkanInstance]: Init Vulkan Instance... ==============")
    
    CreateInstance();
    SetupDebugMessenger();

    bIsInitialized = true;
    
    OPAAX_VERBOSE("============== [OpaaxVulkanInstance]: End Init Vulkan Instance ==============")
}

OPAAX::VULKAN::OpaaxVulkanInstance::~OpaaxVulkanInstance()
{
    if(bIsInitialized)
    {
        OPAAX_WARNING("%1% Destroying but not clean. this may result to undef behavior.", %typeid(*this).name())
    }
}

void OPAAX::VULKAN::OpaaxVulkanInstance::CreateInstance()
{
    OPAAX_LOG("[OpaaxVulkanInstance]: Creating Vulkan Instance...")

    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport())
    {
        OPAAX_ERROR("[OpaaxVulkanInstance] Validation layers requested, but not available!")
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo lAppInfo{};
    lAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    lAppInfo.pApplicationName = OPAAX::CONST::ENGINE_NAME.c_str();
    lAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    lAppInfo.pEngineName = OPAAX::CONST::ENGINE_NAME.c_str();
    lAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    lAppInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo lCreateInfo{};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    lCreateInfo.pApplicationInfo = &lAppInfo;

    auto lExtensions = GetRequiredExtensions();
    lCreateInfo.enabledExtensionCount = static_cast<UInt32>(lExtensions.size());
    lCreateInfo.ppEnabledExtensionNames = lExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT lDebugCreateInfo{};
    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        lCreateInfo.enabledLayerCount = static_cast<UInt32>(VULKAN_CONST::G_VALIDATION_LAYERS.size());
        lCreateInfo.ppEnabledLayerNames = VULKAN_CONST::G_VALIDATION_LAYERS.data();

        PopulateDebugMessengerCreateInfo(lDebugCreateInfo);
        lCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &lDebugCreateInfo;
    } else
    {
        lCreateInfo.enabledLayerCount = 0;
        lCreateInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&lCreateInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanInstance]: Failed to create Vulkan Instance!")
        throw std::runtime_error("Failed to create instance!");
    }

    OPAAX_LOG("[OpaaxVulkanInstance]: Vulkan Instance created!")
}

void OPAAX::VULKAN::OpaaxVulkanInstance::SetupDebugMessenger()
{
    if (!VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT lCreateInfo;
    PopulateDebugMessengerCreateInfo(lCreateInfo);

    if (CreateDebugUtilsMessengerEXT(m_vkInstance, &lCreateInfo, nullptr, &m_vkDebugMessenger) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanInstance]: Failed to create debug messenger!")
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

bool OPAAX::VULKAN::OpaaxVulkanInstance::CheckValidationLayerSupport()
{
    UInt32 lLayerCount;
    vkEnumerateInstanceLayerProperties(&lLayerCount, nullptr);

    std::vector<VkLayerProperties> lAvailableLayers(lLayerCount);
    vkEnumerateInstanceLayerProperties(&lLayerCount, lAvailableLayers.data());

    for (const char* lLayerName : VULKAN_CONST::G_VALIDATION_LAYERS)
    {
        bool lLayerFound = false;

        for (const auto& lLayerProperties : lAvailableLayers)
        {
            if (strcmp(lLayerName, lLayerProperties.layerName) == 0)
            {
                lLayerFound = true;
                break;
            }
        }

        if (!lLayerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char*> OPAAX::VULKAN::OpaaxVulkanInstance::GetRequiredExtensions()
{
    UInt32 lGlfwExtensionCount = 0;
    const char** lGlfwExtensions;
    lGlfwExtensions = glfwGetRequiredInstanceExtensions(&lGlfwExtensionCount);

    std::vector<const char*> lExtensions(lGlfwExtensions, lGlfwExtensions + lGlfwExtensionCount);

    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        lExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return lExtensions;
}

void OPAAX::VULKAN::OpaaxVulkanInstance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& CreateInfo)
{
    OPAAX_LOG("[OpaaxVulkanInstance]: Creating Vulkan Debug Messenger...")
    
    CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    CreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    CreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    CreateInfo.pfnUserCallback = DebugCallback;

    OPAAX_LOG("[OpaaxVulkanInstance]: Vulkan Debug Messenger Created!")
}

VkBool32 OPAAX::VULKAN::OpaaxVulkanInstance::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity, VkDebugUtilsMessageTypeFlagsEXT MessageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    OPAAX_DEBUG("Validation Layer %1%", %pCallbackData->pMessage)
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void OPAAX::VULKAN::OpaaxVulkanInstance::Cleanup()
{
    OPAAX_VERBOSE("============== [OpaaxVulkanInstance]: Start Cleanup ==============")
    
    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugMessenger, nullptr);
    }
    
    vkDestroyInstance(m_vkInstance, nullptr);

    bIsInitialized = false;

    OPAAX_VERBOSE("============== [OpaaxVulkanInstance]: End Cleanup ==============")
}
