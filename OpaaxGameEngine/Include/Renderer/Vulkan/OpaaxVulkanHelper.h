#pragma once
#include <set>
#include <vulkan/vulkan_core.h>

#include "OpaaxQueueFamilyIndices.h"
#include "OpaaxSwapChainSupportDetails.h"
#include "OpaaxTypes.h"
#include "OpaaxVulkanGlobal.h"
#include "Core/OPLogMacro.h"

namespace OPAAX
{
    namespace VULKAN_HELPER
    {        
        static UInt32 FindMemoryType(VkPhysicalDevice PhysicalDevice, UInt32 TypeFilter,
                                     VkMemoryPropertyFlags Properties)
        {
            VkPhysicalDeviceMemoryProperties lMemProperties;

            vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &lMemProperties);

            for (UInt32 i = 0; i < lMemProperties.memoryTypeCount; i++)
            {
                if ((TypeFilter & (1 << i)) && (lMemProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
                {
                    return i;
                }
            }

            OPAAX_ERROR("[OpaaxVulkanHelper] Failed to find suitable memory type!")
            throw std::runtime_error("Failed to find suitable memory type!");
        }

        static VULKAN::OpaaxSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice Device, VkSurfaceKHR Surface)
        {
            VULKAN::OpaaxSwapChainSupportDetails lDetails{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, Surface, &lDetails.Capabilities);

            UInt32 lFormatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &lFormatCount, nullptr);

            if (lFormatCount != 0)
            {
                lDetails.Formats.resize(lFormatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &lFormatCount, lDetails.Formats.data());
            }

            UInt32 lPresentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &lPresentModeCount, nullptr);

            if (lPresentModeCount != 0)
            {
                lDetails.PresentModes.resize(lPresentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &lPresentModeCount, lDetails.PresentModes.data());
            }
            
            return lDetails;
        }

        static OpaaxQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR Surface) {
            OpaaxQueueFamilyIndices lIndices;

            UInt32 lQueueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> lQueueFamilies(lQueueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, lQueueFamilies.data());

            int i = 0;
            for (const auto& lQueueFamily : lQueueFamilies)
            {
                if (lQueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 lPresentSupport = false;
                    vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, Surface, &lPresentSupport);

                    if (lPresentSupport)
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

        static bool CheckDeviceExtensionSupport(VkPhysicalDevice device) {
            UInt32 lExtensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &lExtensionCount, nullptr);

            std::vector<VkExtensionProperties> lAvailableExtensions(lExtensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &lExtensionCount, lAvailableExtensions.data());

            std::set<std::string> lRequiredExtensions(VULKAN_CONST::G_DEVICES_EXTENSIONS.begin(), VULKAN_CONST::G_DEVICES_EXTENSIONS.end());

            for (const auto& extension : lAvailableExtensions) {
                lRequiredExtensions.erase(extension.extensionName);
            }

            return lRequiredExtensions.empty();
        }
        
        static bool IsPhysicalDeviceSuitable(VkPhysicalDevice Device, VkSurfaceKHR Surface)
        {
            OpaaxQueueFamilyIndices lIndices = FindQueueFamilies(Device, Surface);
            bool lExtensionsSupported = CheckDeviceExtensionSupport(Device);

            return lIndices.IsComplete() && lExtensionsSupported;
        }
        
    }
}
