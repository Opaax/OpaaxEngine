#pragma once
#include "OpaaxVKAllocatedImage.h"
#include "OpaaxVKGlobal.h"
#include "OpaaxVKTypes.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/Renderer/IOpaaxRendererContext.h"
#include "OpaaxVKFrameData.h"

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
                VkQueue                     m_vkGraphicsQueue   = VK_NULL_HANDLE;
                
                VkFormat                    m_vkSwapchainImageFormat;

                VecVkImg                    m_vkSwapchainImages;
                VecVkImgView                m_vkSwapchainImageViews;
                VkExtent2D                  m_vkSwapchainExtent;

                UInt32                      m_graphicsQueueFamily{0};

                OpaaxVKFrameData            m_framesData[VULKAN_CONST::MAX_FRAMES_IN_FLIGHT];
                
                Int32                       m_frameNumber{0};

                OpaaxVKDeletionQueue        m_mainDeletionQueue;

                VmaAllocator                m_vmaAllocator;

                //draw resources
                OpaaxVKAllocatedImage       m_drawImage;
                VkExtent2D                  m_drawExtent;
                
            public:
                explicit OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window);

                ~OpaaxVulkanRenderer() override;

            private:
                /**
                 * Initializes the Vulkan context by creating the Vulkan instance, validation layers (if enabled),
                 * Vulkan surface, and Vulkan device. This method leverages vk-bootstrap to streamline the setup process.
                 *
                 * The initialization includes:
                 *   - Building the Vulkan instance with the application name and enabling debug callbacks.
                 *   - Setting up validation layers and debug messenger (if validation layers are enabled).
                 *   - Retrieving and enabling the required Vulkan extensions.
                 *   - Creating the Vulkan surface required for rendering.
                 *   - Selecting a suitable physical GPU that supports Vulkan 1.3 with required features.
                 *   - Retrieving and setting Vulkan 1.3 and Vulkan 1.2 features on the physical device.
                 *   - Creating the logical Vulkan device for GPU operations.
                 *
                 * Logs details about Vulkan setup, including GPU selection, for debugging and tracing purposes.
                 *
                 * This method must be called before using any Vulkan-dependent functions in the renderer.
                 *
                 * @throws std::runtime_error If Vulkan initialization fails at any stage, including instance creation,
                 *         surface creation, GPU selection, or logical device creation.
                 */
                void InitVulkanBootStrap();
                void InitVMAAllocator();
                void CreateVulkanSurface();
                void InitVulkanSwapchain();
                void InitVulkanCommands();
                void InitVulkanSyncs();
                void CreateSwapchain(UInt32 Width, UInt32 Height);

                void DrawBackground(VkCommandBuffer CommandBuffer);
                
                void DestroySwapchain();
            
            public:
                bool Initialize()   override;
                void Resize()       override;
                void RenderFrame()  override;
                void Shutdown()     override;
                
                FORCEINLINE SDL_WindowFlags GetWindowFlags() override { return (SDL_WindowFlags)(SDL_WINDOW_VULKAN); }
                FORCEINLINE OpaaxVKFrameData& GetCurrentFrameData() { return m_framesData[m_frameNumber % VULKAN_CONST::MAX_FRAMES_IN_FLIGHT]; }
            };
        }
    }
}
