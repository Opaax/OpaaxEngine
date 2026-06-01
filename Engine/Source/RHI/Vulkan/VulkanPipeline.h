#pragma once

#include "RHI/Pipeline.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanPipeline  (Phase 2 stub)
    // =============================================================================
    // No VkPipeline yet. Phase 3 bakes one from the SPIR-V shader + vertex layout + blend,
    // compatible with the swapchain's dynamic-rendering color format.
    class VulkanPipeline final : public IPipeline
    {
    public:
        explicit VulkanPipeline(const PipelineDesc& /*InDesc*/) {}
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
