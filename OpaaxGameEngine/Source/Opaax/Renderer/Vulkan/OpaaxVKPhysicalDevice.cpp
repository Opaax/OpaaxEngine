#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKPhysicalDevice.h"

#include "Opaax/Log/OPLogMacro.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKHelper.h"

using namespace OPAAX::RENDERER::VULKAN;

OpaaxVKPhysicalDevice::OpaaxVKPhysicalDevice()
{
    
}

OpaaxVKPhysicalDevice::~OpaaxVKPhysicalDevice()
{
    if (bIsInitialized)
    {
        OPAAX_WARNING("%1% Destroying but not clean. this may result to undef behavior.", %typeid(*this).name())
    }
}

void OpaaxVKPhysicalDevice::PickPhysicalDevice(VkInstance Instance, VkSurfaceKHR Surface)
{
    OPAAX_LOG("[OpaaxVKPhysicalDevice]: Start Picking GPU...")
    
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

    bIsInitialized = true;
    
    VkPhysicalDeviceProperties lProps;
    vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &lProps);

    OPAAX_LOG("[OpaaxVKPhysicalDevice] Picked GPU: %1% !!", %lProps.deviceName)
    OPAAX_LOG("[OpaaxVKPhysicalDevice]: End Start Picking GPU!")
}

void OpaaxVKPhysicalDevice::Init(VkInstance VKInstance, VkSurfaceKHR Surface)
{
    OPAAX_VERBOSE("============== [OpaaxVKPhysicalDevice]: Init Vulkan Picking GPU ==============")
    PickPhysicalDevice(VKInstance, Surface);
    OPAAX_VERBOSE("============== [OpaaxVKPhysicalDevice]: End Init Vulkan Picking GPU! ==============")
}

void OpaaxVKPhysicalDevice::Cleanup()
{
    OPAAX_VERBOSE("============== [OpaaxVKPhysicalDevice]: Start Cleanup ==============")

    //Not necessary? 
    bIsInitialized = false;

    OPAAX_VERBOSE("============== [OpaaxVKPhysicalDevice]: End Cleanup ==============")
}
