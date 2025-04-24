#pragma once
#include <vulkan/vulkan_core.h>

#include "OpaaxImguiBase.h"
#include "Opaax/OpaaxDeletionQueue.h"

namespace OPAAX
{
    namespace IMGUI
    {
        class OPAAX_API OpaaxImguiVulkan : public OpaaxImguiBase
        {
            VkDevice            m_vkDevice = VK_NULL_HANDLE;
            VkDescriptorPool    m_vkPool   = VK_NULL_HANDLE;
            
            OpaaxDeletionQueue        m_mainDeletionQueue;
        public:
            void Initialize(SDL_Window* SLDWindow, VkInstance Instance, VkPhysicalDevice PhysicalDevice, VkDevice Device, VkQueue GraphicsQueue, VkFormat Format);
            void Shutdown() override;
        };
    }
}
