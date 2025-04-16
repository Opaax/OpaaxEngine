#include "Renderer/Vulkan/OpaaxVulkanLogicalDevice.h"

#include <set>

#include "Renderer/Vulkan/OpaaxVulkanHelper.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanLogicalDevice::OpaaxVulkanLogicalDevice(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
{
    OPAAX_VERBOSE("============== [OpaaxVulkanInstance]: Init Vulkan Logical Device... ==============")
    
    CreateLogicalDevice(PhysicalDevice, Surface);
    bIsInitialized = true;
    
    OPAAX_VERBOSE("============== [OpaaxVulkanInstance]: End Init Vulkan Logical Device... ==============")
}

OpaaxVulkanLogicalDevice::~OpaaxVulkanLogicalDevice()
{
    if(bIsInitialized)
    {
        OPAAX_WARNING("%1% Destroying but not clean. this may result to undef behavior.", %typeid(*this).name())
    }
}

void OpaaxVulkanLogicalDevice::CreateLogicalDevice(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
{
    OPAAX_LOG("[OpaaxVulkanInstance]: Creating Vulkan Logical Device...")
    
    OpaaxQueueFamilyIndices lIndices = VULKAN_HELPER::FindQueueFamilies(PhysicalDevice, Surface);

    std::vector<VkDeviceQueueCreateInfo> lQueueCreateInfos;
    std::set<UInt32> lUniqueQueueFamilies = {lIndices.GraphicsFamily.value(), lIndices.PresentFamily.value()};

    float lQueuePriority = 1.0f;
    for (UInt32 lQueueFamily : lUniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo lQueueCreateInfo{};
        lQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        lQueueCreateInfo.queueFamilyIndex = lQueueFamily;
        lQueueCreateInfo.queueCount = 1;
        lQueueCreateInfo.pQueuePriorities = &lQueuePriority;
        lQueueCreateInfos.push_back(lQueueCreateInfo);
    }

    VkPhysicalDeviceFeatures lDeviceFeatures{};
    lDeviceFeatures.samplerAnisotropy = VK_TRUE;
    lDeviceFeatures.sampleRateShading = VK_TRUE; // enable sample shading feature for the device

    VkDeviceCreateInfo lCreateInfo{};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    lCreateInfo.queueCreateInfoCount = static_cast<UInt32>(lQueueCreateInfos.size());
    lCreateInfo.pQueueCreateInfos = lQueueCreateInfos.data();

    lCreateInfo.pEnabledFeatures = &lDeviceFeatures;

    lCreateInfo.enabledExtensionCount = static_cast<UInt32>(VULKAN_CONST::G_DEVICES_EXTENSIONS.size());
    lCreateInfo.ppEnabledExtensionNames = VULKAN_CONST::G_DEVICES_EXTENSIONS.data();

    if (vkCreateDevice(PhysicalDevice, &lCreateInfo, nullptr, &m_vkDevice) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanLogicalDevice] Failed to create logical device!")
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(m_vkDevice, lIndices.GraphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_vkDevice, lIndices.PresentFamily.value(), 0, &m_presentQueue);

    OPAAX_LOG("[OpaaxVulkanInstance]: Vulkan Logical Device Created!")
}

void OpaaxVulkanLogicalDevice::Cleanup()
{
    vkDestroyDevice(m_vkDevice, nullptr);
    
    bIsInitialized = false;
}
