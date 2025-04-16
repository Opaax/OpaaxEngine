#pragma once
#include "OpaaxEngineMacros.h"
#include "OpaaxVulkanTypes.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanFrameBuffers
        {
            VecVkFrameBuffer m_vkSwapChainFrameBuffers;

        public:
            OpaaxVulkanFrameBuffers() = default;
            OpaaxVulkanFrameBuffers(const VecVkImgView& SwapChainImageViews,
                VkExtent2D SwapChainExtent, VkRenderPass RenderPass, VkDevice LogicalDevice);
            ~OpaaxVulkanFrameBuffers();
            
        private:
            void CreateFrameBuffers(const VecVkImgView& SwapChainImageViews,
                VkExtent2D SwapChainExtent, VkRenderPass RenderPass, VkDevice LogicalDevice);

        public:
            void Cleanup(VkDevice LogicalDevice);
            void CleanupForRecreate(VkDevice LogicalDevice);
            void Recreate(const VecVkImgView& SwapChainImageViews,
                VkExtent2D SwapChainExtent, VkRenderPass RenderPass, VkDevice LogicalDevice);

            FORCEINLINE const VecVkFrameBuffer& GetSwapChainFrameBuffers() { return m_vkSwapChainFrameBuffers; }
        };
    }
}
