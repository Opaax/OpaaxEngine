#pragma once

#include "RHI/BindGroup.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanBindGroup  (Phase 2 stub)
    // =============================================================================
    // No descriptor set yet. Phase 3 allocates a VkDescriptorSet (UBO at binding 0 + the
    // 16-sampler array) and updates it each flush.
    class VulkanBindGroup final : public IBindGroup
    {
    public:
        explicit VulkanBindGroup(const BindGroupLayout& /*InLayout*/) {}

        void SetUniformBuffer(IUniformBuffer& /*InUniformBuffer*/) override {}
        void SetTexture(Uint32 /*InSlot*/, ITexture2D& /*InTexture*/) override {}
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
