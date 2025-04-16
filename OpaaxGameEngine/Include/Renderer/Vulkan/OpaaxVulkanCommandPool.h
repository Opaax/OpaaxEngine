#pragma once
#include "OpaaxEngineMacros.h"
#include "vulkan/vulkan_core.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanCommandPool
        {
            VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;

        public:
            OpaaxVulkanCommandPool() = default;
            OpaaxVulkanCommandPool(VkPhysicalDevice PhysicalDevice,VkSurfaceKHR Surface, VkDevice LogicalDevice);
            ~OpaaxVulkanCommandPool();
        private:
            void CreateCommandPool(VkPhysicalDevice PhysicalDevice,VkSurfaceKHR Surface, VkDevice LogicalDevice);
            
        public:
            void Cleanup(VkDevice LogicalDevice);

            FORCEINLINE VkCommandPool GetCommandPool() const { return m_vkCommandPool; }
        };
    }
}
