#pragma once
#include "OpaaxVulkanInclude.h"
#include "Opaax/Renderer/IOpaaxRendererContext.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKInstance.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKPhysicalDevice.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            class OPAAX_API OpaaxVulkanRenderer final : public OPAAX::IOpaaxRendererContext
            {
                TUniquePtr<RENDERER::VULKAN::OpaaxVKInstance> m_opaaxVKInstance = nullptr;
                VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;
                TUniquePtr<RENDERER::VULKAN::OpaaxVKPhysicalDevice> m_opaaxVKPhysicalDevice = nullptr;

                void CreateVulkanSurface();
            
            public:
                explicit OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window)
                    : IOpaaxRendererContext(Window) {}

                ~OpaaxVulkanRenderer() override;
            
                bool Initialize() override;
                void Resize() override;
                void RenderFrame() override;
                void Shutdown() override;
                FORCEINLINE SDL_WindowFlags GetWindowFlags() override { return (SDL_WindowFlags)(SDL_WINDOW_VULKAN); }
            };
        }
    }
}
