#include "Renderer/Vulkan/OpaaxVulkanCommandPool.h"

#include "Renderer/Vulkan/OpaaxQueueFamilyIndices.h"
#include "Renderer/Vulkan/OpaaxVulkanHelper.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanCommandPool::OpaaxVulkanCommandPool(VkPhysicalDevice PhysicalDevice,VkSurfaceKHR Surface, VkDevice LogicalDevice)
{
    CreateCommandPool(PhysicalDevice, Surface, LogicalDevice);
}

OpaaxVulkanCommandPool::~OpaaxVulkanCommandPool()
{
    
}

void OpaaxVulkanCommandPool::CreateCommandPool(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkDevice LogicalDevice)
{
    OpaaxQueueFamilyIndices lQueueFamilyIndices = VULKAN_HELPER::FindQueueFamilies(PhysicalDevice, Surface);

    VkCommandPoolCreateInfo lPoolInfo{};
    lPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    lPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    lPoolInfo.queueFamilyIndex = lQueueFamilyIndices.GraphicsFamily.value();

    if (vkCreateCommandPool(LogicalDevice, &lPoolInfo, nullptr, &m_vkCommandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}

void OpaaxVulkanCommandPool::Cleanup(VkDevice LogicalDevice)
{
    vkDestroyCommandPool(LogicalDevice, m_vkCommandPool, nullptr);
}

