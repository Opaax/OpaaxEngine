#pragma once
#include "OpaaxEngineMacros.h"
#include "vulkan/vulkan_core.h"

namespace OPAAX
{
    namespace VULKAN
    {
        class OpaaxVulkanGraphicsPipeline
        {
            VkPipelineLayout    m_vkPipelineLayout;
            VkPipeline          m_vkGraphicsPipeline;

            void CreateGraphicsPipeline(VkDevice LogicalDevice, VkRenderPass RenderPass);
        public:
            OpaaxVulkanGraphicsPipeline() = default;
            OpaaxVulkanGraphicsPipeline(VkDevice LogicalDevice, VkRenderPass RenderPass);
            ~OpaaxVulkanGraphicsPipeline();
        public:
            void Cleanup(VkDevice LogicalDevice);

            FORCEINLINE VkPipeline GetGraphicsPipeline() const { return m_vkGraphicsPipeline; }
        };
    }
}
