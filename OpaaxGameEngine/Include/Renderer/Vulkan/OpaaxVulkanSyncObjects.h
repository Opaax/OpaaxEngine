#pragma once

#include "OpaaxEngineMacros.h"
#include "OpaaxVulkanTypes.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanSyncObjects
        {           
            VkSemaphore m_vkImageAvailableSemaphore = VK_NULL_HANDLE;
            VkSemaphore m_vkRenderFinishedSemaphore = VK_NULL_HANDLE;
            VkFence     m_vkInFlightFence           = VK_NULL_HANDLE;

            VecVkSemaphore  m_vkImageAvailableSemaphores;
            VecVkSemaphore  m_vkRenderFinishedSemaphores;
            VecVkFence    m_vkInFlightFences;

            
        public:
            OpaaxVulkanSyncObjects() = default;
            OpaaxVulkanSyncObjects(VkDevice LogicalDevice);
            ~OpaaxVulkanSyncObjects();
            void CreateSyncObjects(VkDevice LogicalDevice);
            void Cleanup(VkDevice LogicalDevice);

            FORCEINLINE VkSemaphore GetImageAvailableSemaphore()    const { return m_vkImageAvailableSemaphore; }
            FORCEINLINE VkSemaphore GetRenderFinishedSemaphore()    const { return m_vkRenderFinishedSemaphore; }
            FORCEINLINE VkFence     GetInFlightFence()              const { return m_vkInFlightFence; }
            
            FORCEINLINE const VecVkSemaphore&   GetImageAvailableSemaphores()   const { return m_vkImageAvailableSemaphores; }
            FORCEINLINE const VecVkSemaphore&   GetRenderFinishedSemaphores()   const { return m_vkRenderFinishedSemaphores; }
            FORCEINLINE const VecVkFence&       GetInFlightFences()             const { return m_vkInFlightFences; }
        };
    }
}
