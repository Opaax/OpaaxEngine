#pragma once
#include "OpaaxEngineMacros.h"
#include "vulkan/vulkan_core.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanPhysicalDevice
        {
            //-----------------------------------------------------------------
            // MEMBERS
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        public:
            OpaaxVulkanPhysicalDevice() = default;
            OpaaxVulkanPhysicalDevice(VkInstance VKInstance, VkSurfaceKHR Surface);
            ~OpaaxVulkanPhysicalDevice();

            //-----------------------------------------------------------------
            // Function
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            void PickPhysicalDevice(VkInstance Instance, VkSurfaceKHR Surface);

            /*---------------------------- PUBLIC ----------------------------*/
        public:
            /*---------------------------- Getter - Setter ----------------------------*/
            FORCEINLINE VkPhysicalDevice GetPhysicalDevice() const { return m_vkPhysicalDevice; }
        };
    }
}
