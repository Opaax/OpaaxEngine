#pragma once

#include "OpaaxEngineMacros.h"
#include "OpaaxVulkanTypes.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanCommandBuffers
        {
            VecVkCommandBuffers m_vkCommandBuffers;
            
        public:
            OpaaxVulkanCommandBuffers() =default;
            OpaaxVulkanCommandBuffers(VkCommandPool CommandPool, VkDevice Device);
            ~OpaaxVulkanCommandBuffers();

            void CreateCommandBuffers(VkCommandPool CommandPool, VkDevice Device);
            
            FORCEINLINE const VecVkCommandBuffers& GetCommandBuffers() const { return m_vkCommandBuffers; }
        };
    }
}
