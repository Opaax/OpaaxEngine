#pragma once
#include <vulkan/vulkan_core.h>

#include "OpaaxVulkanGraphicsPipeline.h"
#include "OpaaxVulkanLogicalDevice.h"
#include "OpaaxVulkanPhysicalDevice.h"
#include "OpaaxVulkanSwapChain.h"
#include "OpaaxVulkanFrameBuffers.h"
#include "OpaaxVulkanCommandPool.h"
#include "Renderer/IOpaaxRenderer.h"
#include "GLFW/glfw3.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanInstance;
        class OpaaxVulkanPhysicalDevice;
        class OpaaxVulkanLogicalDevice;
        class OpaaxVulkanSwapChain;
        class OpaaxVulkanGraphicsPipeline;
        class OpaaxVulkanFrameBuffers;
        class OpaaxVulkanCommandPool;
        class OpaaxVulkanCommandBuffers;
        class OpaaxVulkanSyncObjects;
        
        class OpaaxVulkanRenderer final : public OPAAX::IOpaaxRenderer
        {
            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            /*---------------------------- PUBLIC ----------------------------*/
            TUniquePtr<OpaaxVulkanInstance>         m_opaaxVkInstance;
            TUniquePtr<OpaaxVulkanPhysicalDevice>   m_opaaxVkPhysicalDevice;
            TUniquePtr<OpaaxVulkanLogicalDevice>    m_opaaxVkLogicalDevice;
            TUniquePtr<OpaaxVulkanSwapChain>        m_opaaxVkSwapChain;
            TUniquePtr<OpaaxVulkanGraphicsPipeline> m_opaaxGraphicPipeline;
            TUniquePtr<OpaaxVulkanFrameBuffers>     m_opaaxFrameBuffers;
            TUniquePtr<OpaaxVulkanCommandPool>      m_opaaxCommandPool;
            TUniquePtr<OpaaxVulkanCommandBuffers>   m_opaaxCommandBuffers;
            TUniquePtr<OpaaxVulkanSyncObjects>      m_opaaxSyncObjects;
            
            VkSurfaceKHR m_vkSurface    = VK_NULL_HANDLE;
            VkRenderPass m_vkRenderPass = VK_NULL_HANDLE;

            UInt32  m_currentFrame = 0;
            bool    bFramebufferResized = false;

            //TODO
            //@https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/00_Vertex_input_description.html
            
            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            /*---------------------------- PUBLIC ----------------------------*/
        public:
            explicit OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window);
            ~OpaaxVulkanRenderer() override;

            //----------------------------------------------------------------- 
            // Functions                                                        
            //----------------------------------------------------------------- 
            /*---------------------------- Private ----------------------------*/
        private:
            void CreateSurface(const VkInstance Instance, GLFWwindow* const Window);
            void CreateRenderPass();
            void RecordCommandBuffer(VkCommandBuffer CommandBuffer, UInt32 ImageIndex);
            void RecreateSwapChain();

            //-----------------------------------------------------------------
            // Overrides
            //-----------------------------------------------------------------
            /*---------------------------- PUBLIC ----------------------------*/
        public:
            bool Initialize() override;
            void Resize() override;
            void RenderFrame() override;
            void Shutdown() override;
        };
    }
}
