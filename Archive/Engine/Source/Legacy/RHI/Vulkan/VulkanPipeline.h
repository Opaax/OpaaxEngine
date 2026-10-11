#pragma once

#include "RHI/Pipeline.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

namespace Opaax
{
    // =============================================================================
    // VulkanPipeline
    // =============================================================================
    /**
     * @class VulkanPipeline
     *
     * IPipeline for Vulkan: bakes an immutable VkPipeline from the SPIR-V shader modules
     * (VulkanShader), the vertex input layout (PipelineDesc::VertexLayout), and fixed-function
     * state (alpha blend, triangle list, dynamic viewport+scissor, no depth). Uses VK 1.3 dynamic
     * rendering — no VkRenderPass; the color attachment format comes from the swapchain
     * (VulkanFrameContext::ColorFormat). Owns its descriptor-set layout + pipeline layout.
     */
    class VulkanPipeline final : public IPipeline
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanPipeline(const PipelineDesc& InDesc);
        ~VulkanPipeline() override;

        // =============================================================================
        // Get — consumed by VulkanCommandBuffer
        // =============================================================================
    public:
        VkPipeline       GetPipeline()       const noexcept { return m_Pipeline; }
        VkPipelineLayout GetPipelineLayout() const noexcept { return m_PipelineLayout; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VkDevice              m_Device         = VK_NULL_HANDLE;   // borrowed
        VkDescriptorSetLayout m_SetLayout      = VK_NULL_HANDLE;
        VkPipelineLayout      m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline            m_Pipeline       = VK_NULL_HANDLE;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
