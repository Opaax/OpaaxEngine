#include "Renderer/Vulkan/OpaaxVulkanPhysicalDevice.h"

#include <stdexcept>

#include "OpaaxTypes.h"
#include "Core/OPLogMacro.h"
#include "Renderer/Vulkan/OpaaxVulkanHelper.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanPhysicalDevice::OpaaxVulkanPhysicalDevice(VkInstance VKInstance, VkSurfaceKHR Surface)
{
    OPAAX_VERBOSE("============== [OpaaxVulkanPhysicalDevice]: Init Vulkan Picking GPU ==============")
    PickPhysicalDevice(VKInstance, Surface);
    OPAAX_VERBOSE("============== [OpaaxVulkanPhysicalDevice]: End Init Vulkan Picking GPU! ==============")
}

OpaaxVulkanPhysicalDevice::~OpaaxVulkanPhysicalDevice()
{
    
}

void OpaaxVulkanPhysicalDevice::PickPhysicalDevice(VkInstance Instance, VkSurfaceKHR Surface)
{
    OPAAX_LOG("[OpaaxVulkanPhysicalDevice]: Start Picking GPU...")
    
    UInt32 lDeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &lDeviceCount, nullptr);

    if (lDeviceCount == 0)
    {
        OPAAX_ERROR("[OpaaxVulkanPhysicalDevice] Failed to find GPUs with Vulkan support!")
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> lDevices(lDeviceCount);
    vkEnumeratePhysicalDevices(Instance, &lDeviceCount, lDevices.data());

    for (const auto& lDevice : lDevices)
    {
        if (VULKAN_HELPER::IsPhysicalDeviceSuitable(lDevice, Surface))
        {
            m_vkPhysicalDevice = lDevice;
            break;
        }
    }

    if (m_vkPhysicalDevice == VK_NULL_HANDLE)
    {
        OPAAX_ERROR("[OpaaxVulkanPhysicalDevice] Failed to find a suitable GPU!")
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties lProps;
    vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &lProps);

    OPAAX_LOG("[OpaaxVulkanPhysicalDevice] Picked GPU: %1% !!", %lProps.deviceName)
    OPAAX_LOG("[OpaaxVulkanPhysicalDevice]: End Start Picking GPU!")
}
