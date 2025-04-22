#pragma once
#include <set>

#include "OpaaxQueueFamilyIndices.h"
#include "OpaaxSwapChainSupportDetails.h"
#include "OpaaxVKGlobal.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxStdTypes.h"
#include "Opaax/Log/OPLogMacro.h"

namespace OPAAX
{
    namespace RENDERER
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

            static VULKAN::OpaaxQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR Surface) {
                VULKAN::OpaaxQueueFamilyIndices lIndices;

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
                VULKAN::OpaaxQueueFamilyIndices lIndices = FindQueueFamilies(Device, Surface);
                bool lExtensionsSupported = CheckDeviceExtensionSupport(Device);

                return lIndices.IsComplete() && lExtensionsSupported;
            }

            static void CreateBuffer(VkPhysicalDevice PhysicalDevice, VkDevice LogicalDevice, VkDeviceSize Size, VkBufferUsageFlags Usage,
                          VkMemoryPropertyFlags Properties, VkBuffer& Buffer, VkDeviceMemory& BufferMemory)
            {
                VkBufferCreateInfo lBufferInfo{};
                lBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                lBufferInfo.size = Size;
                lBufferInfo.usage = Usage;
                lBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                if (vkCreateBuffer(LogicalDevice, &lBufferInfo, nullptr, &Buffer) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to create buffer!");
                }

                VkMemoryRequirements lMemRequirements;
                vkGetBufferMemoryRequirements(LogicalDevice, Buffer, &lMemRequirements);

                VkMemoryAllocateInfo lAllocInfo{};
                lAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                lAllocInfo.allocationSize = lMemRequirements.size;
                lAllocInfo.memoryTypeIndex = FindMemoryType(PhysicalDevice, lMemRequirements.memoryTypeBits, Properties);

                if (vkAllocateMemory(LogicalDevice, &lAllocInfo, nullptr, &BufferMemory) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to allocate buffer memory!");
                }

                vkBindBufferMemory(LogicalDevice, Buffer, BufferMemory, 0);
            }

            static void CopyBuffer(VkDevice Device,VkCommandPool CommandPool, VkQueue GraphicsQueue, VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size)
            {
                VkCommandBufferAllocateInfo lAllocInfo{};
                lAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                lAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                lAllocInfo.commandPool = CommandPool;
                lAllocInfo.commandBufferCount = 1;

                VkCommandBuffer lCommandBuffer;
                vkAllocateCommandBuffers(Device, &lAllocInfo, &lCommandBuffer);

                VkCommandBufferBeginInfo lBeginInfo{};
                lBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

                vkBeginCommandBuffer(lCommandBuffer, &lBeginInfo);

                VkBufferCopy lCopyRegion{};
                lCopyRegion.size = Size;
                vkCmdCopyBuffer(lCommandBuffer, SrcBuffer, DstBuffer, 1, &lCopyRegion);

                vkEndCommandBuffer(lCommandBuffer);

                VkSubmitInfo lSubmitInfo{};
                lSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                lSubmitInfo.commandBufferCount = 1;
                lSubmitInfo.pCommandBuffers = &lCommandBuffer;

                vkQueueSubmit(GraphicsQueue, 1, &lSubmitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(GraphicsQueue);

                vkFreeCommandBuffers(Device, CommandPool, 1, &lCommandBuffer);
            }
        }
    }
}
