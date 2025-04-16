#pragma once
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "OpaaxEngineMacros.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanLogicalDevice
        {
            //-----------------------------------------------------------------
            // MEMBERS
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            VkDevice    m_vkDevice      = VK_NULL_HANDLE;
            VkQueue     m_graphicsQueue = VK_NULL_HANDLE;
            VkQueue     m_presentQueue  = VK_NULL_HANDLE;

            bool        bIsInitialized  = false;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            /*---------------------------- PUBLIC ----------------------------*/
        public:
            OpaaxVulkanLogicalDevice(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);
            ~OpaaxVulkanLogicalDevice();

            //-----------------------------------------------------------------
            // Function
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            void CreateLogicalDevice(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);

            /*---------------------------- PUBLIC ----------------------------*/
        public:
            void Cleanup();

            /*---------------------------- Getter - Setter ----------------------------*/
            FORCEINLINE VkDevice    GetDevice()         const { return m_vkDevice; }
            FORCEINLINE VkQueue     GetGraphicsQueue()  const { return m_graphicsQueue; }
            FORCEINLINE VkQueue     GetPresentQueue()   const { return m_presentQueue; }
        };
    }
}
