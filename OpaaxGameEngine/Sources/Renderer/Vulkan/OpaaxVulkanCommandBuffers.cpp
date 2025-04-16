#include "Renderer/Vulkan/OpaaxVulkanCommandBuffers.h"

#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "OpaaxTypes.h"
#include "Renderer/Vulkan/OpaaxVulkanGlobal.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanCommandBuffers::OpaaxVulkanCommandBuffers(VkCommandPool CommandPool, VkDevice Device)
{
    CreateCommandBuffers(CommandPool, Device);
}

OpaaxVulkanCommandBuffers::~OpaaxVulkanCommandBuffers()
{
    
}

void OpaaxVulkanCommandBuffers::CreateCommandBuffers(VkCommandPool CommandPool, VkDevice Device)
{
    m_vkCommandBuffers.resize(VULKAN_CONST::MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo lAllocInfo{};
    lAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    lAllocInfo.commandPool = CommandPool;
    lAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    lAllocInfo.commandBufferCount = static_cast<UInt32>(m_vkCommandBuffers.size());

    if (vkAllocateCommandBuffers(Device, &lAllocInfo, m_vkCommandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}
