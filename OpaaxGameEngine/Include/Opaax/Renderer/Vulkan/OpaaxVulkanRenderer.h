#pragma once
#include "OpaaxVKTypes.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/Renderer/IOpaaxRendererContext.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            class OPAAX_API OpaaxVulkanRenderer final : public OPAAX::IOpaaxRendererContext
            {
                VkExtent2D                  m_windowExtent{ 1280 , 720 };
                
                VkSurfaceKHR                m_vkSurface         = VK_NULL_HANDLE;
                VkInstance                  m_vkInstance        = VK_NULL_HANDLE;
                VkDebugUtilsMessengerEXT    m_vkDebugMessenger  = VK_NULL_HANDLE;
                VkPhysicalDevice            m_vkPhysicalDevice  = VK_NULL_HANDLE;
                VkDevice                    m_vkDevice          = VK_NULL_HANDLE;
                VkSwapchainKHR              m_vkSwapchain       = VK_NULL_HANDLE;
                
                VkFormat                    m_vkSwapchainImageFormat;

                VecVkImg                    m_vkSwapchainImages;
                VecVkImgView                m_vkSwapchainImageViews;
                VkExtent2D                  m_vkSwapchainExtent;
                
            public:
                explicit OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window);

                ~OpaaxVulkanRenderer() override;

            private:
                void InitVulkanBootStrap();
                void CreateVulkanSurface();
                void InitVulkanSwapchain();

                void CreateSwapchain(UInt32 Width, UInt32 Height);
                void DestroySwapchain();
            public:
            
                bool Initialize() override;
                void Resize() override;
                void RenderFrame() override;
                void Shutdown() override;
                FORCEINLINE SDL_WindowFlags GetWindowFlags() override { return (SDL_WindowFlags)(SDL_WINDOW_VULKAN); }
            };
        }
    }
}
