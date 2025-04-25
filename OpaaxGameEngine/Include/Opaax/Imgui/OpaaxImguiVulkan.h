#pragma once
#include <vulkan/vulkan_core.h>

#include "OpaaxImguiBase.h"
#include "Opaax/OpaaxDeletionQueue.h"

namespace OPAAX
{
    namespace IMGUI
    {
        /**
         * @class OpaaxImguiVulkan
         * @brief A Vulkan-specific implementation of the ImGui integration for the Opaax engine.
         *
         * This class provides Vulkan-specific functionality for initializing and shutting down the ImGui library.
         * It is designed to integrate with the Vulkan rendering backend of the Opaax engine
         * to manage ImGui rendering components and resources.
         *
         * Currently, the life circle is a bit weird... Own by engine but manage by renderer context imgui is quit link to it.
         *
         * @details
         * The `OpaaxImguiVulkan` class is derived from `OpaaxImguiBase` and includes Vulkan-specific
         * resource initialization, such as descriptor pools and Vulkan pipelines used by ImGui.
         * It leverages the SDL3 window system for compatibility with Vulkan.
         */
        class OPAAX_API OpaaxImguiVulkan : public OpaaxImguiBase
        {
            VkDevice            m_vkDevice = VK_NULL_HANDLE;
            VkDescriptorPool    m_vkPool   = VK_NULL_HANDLE;
            
            OpaaxDeletionQueue  m_mainDeletionQueue;
        public:
            void Initialize(SDL_Window* SLDWindow, VkInstance Instance, VkPhysicalDevice PhysicalDevice, VkDevice Device, VkQueue GraphicsQueue, VkFormat Format);
            void Draw(VkCommandBuffer CommandBuffer);

            void PollEvents(SDL_Event& Event) override;
            void PreUpdate() override;
            void PostUpdate() override;
            void Shutdown() override;
        };
    }
}
