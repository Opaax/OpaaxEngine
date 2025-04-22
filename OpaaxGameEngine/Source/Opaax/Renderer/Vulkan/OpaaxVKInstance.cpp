#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKInstance.h"

#include "Opaax/Log/OPLogMacro.h"

#include "Opaax/Renderer/Vulkan/OpaaxVKGlobal.h"
#include <SDL3/SDL_vulkan.h>

using namespace OPAAX::RENDERER::VULKAN;

OpaaxVKInstance::OpaaxVKInstance() {}

OpaaxVKInstance::~OpaaxVKInstance()
{
    if (bIsInitialized)
    {
        OPAAX_WARNING("%1% Destroying but not clean. this may result to undef behavior.", %typeid(*this).name())
    }
}

void OpaaxVKInstance::CreateInstance()
{
    OPAAX_LOG("[OpaaxVulkanInstance]: Building VK Instance and Validation Layer (if enable)...")

    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport())
    {
        OPAAX_ERROR("[OpaaxVulkanInstance] Validation layers requested, but not available!")
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo lAppInfo{};
    lAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    lAppInfo.pApplicationName = OPAAX_CONST::ENGINE_NAME.c_str();
    lAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    lAppInfo.pEngineName = OPAAX_CONST::ENGINE_NAME.c_str();
    lAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    lAppInfo.apiVersion = VK_API_VERSION_1_3;

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
        lCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&lDebugCreateInfo;
    }
    else
    {
        lCreateInfo.enabledLayerCount = 0;
        lCreateInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&lCreateInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanInstance]: Failed to create Vulkan Instance!")
        throw std::runtime_error("Failed to create instance!");
    }

    OPAAX_LOG("[OpaaxVulkanInstance]: VK Instance and Validation Layer Builded!")
}

void OpaaxVKInstance::SetupDebugMessenger()
{
    if (!VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT lCreateInfo;
    PopulateDebugMessengerCreateInfo(lCreateInfo);

    if (CreateDebugUtilsMessengerEXT(m_vkInstance, &lCreateInfo, nullptr, &m_vkDebugMessenger) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVKInstance]: Failed to create debug messenger!")
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

bool OpaaxVKInstance::CheckValidationLayerSupport()
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

std::vector<const char*> OpaaxVKInstance::GetRequiredExtensions()
{
    UInt32 lExtensionCount = 0;
    const char* const* lFetchedExtensions = SDL_Vulkan_GetInstanceExtensions(&lExtensionCount);

    if (lFetchedExtensions == NULL)
    {
        OPAAX_ERROR("[OpaaxVKInstance]: Failed to extension for VK instance!")
        throw std::runtime_error("Failed to extension for VK instance!");
    }

    std::vector<const char*> lExtensions(lFetchedExtensions, lFetchedExtensions + lExtensionCount);

    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        lExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return lExtensions;
}

void OpaaxVKInstance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& CreateInfo)
{
    OPAAX_LOG("[OpaaxVKInstance]: Creating Vulkan Debug Messenger...")

    CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    CreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    CreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    CreateInfo.pfnUserCallback = DebugCallback;

    OPAAX_LOG("[OpaaxVKInstance]: Vulkan Debug Messenger Created!")
}

VkBool32 OpaaxVKInstance::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                        VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                        void* pUserData)
{
    OPAAX_DEBUG("Validation Layer %1%", %pCallbackData->pMessage)
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void OpaaxVKInstance::Init()
{
    OPAAX_VERBOSE("============== [OpaaxVKInstance]: Init Vulkan Instance... ==============")

    CreateInstance();

    bIsInitialized = true;

    OPAAX_VERBOSE("============== [OpaaxVKInstance]: End Init Vulkan Instance ==============")
}

void OpaaxVKInstance::Cleanup()
{
    OPAAX_VERBOSE("============== [OpaaxVKInstance]: Start Cleanup ==============")

    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugMessenger, nullptr);
    }

    vkDestroyInstance(m_vkInstance, nullptr);

    bIsInitialized = false;

    OPAAX_VERBOSE("============== [OpaaxVKInstance]: End Cleanup ==============")
}
