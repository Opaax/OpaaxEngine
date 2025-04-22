#pragma once
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            class OpaaxVKPhysicalDevice
            {
                //-----------------------------------------------------------------
                // MEMBERS
                //-----------------------------------------------------------------
                /*---------------------------- PRIVATE ----------------------------*/
            private:
                VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;

                bool bIsInitialized = false;

                //-----------------------------------------------------------------
                // CTOR/DTOR
                //-----------------------------------------------------------------
                /*---------------------------- PRIVATE ----------------------------*/
            public:
                OpaaxVKPhysicalDevice();
                ~OpaaxVKPhysicalDevice();

                //-----------------------------------------------------------------
                // Function
                //-----------------------------------------------------------------
                /*---------------------------- PRIVATE ----------------------------*/
            private:
                void PickPhysicalDevice(VkInstance Instance, VkSurfaceKHR Surface);

                /*---------------------------- PUBLIC ----------------------------*/
            public:
                void Init(VkInstance VKInstance, VkSurfaceKHR Surface);
                void Cleanup();
                
                /*---------------------------- Getter - Setter ----------------------------*/
                FORCEINLINE VkPhysicalDevice GetPhysicalDevice() const { return m_vkPhysicalDevice; }
            };
        }
    }
}
