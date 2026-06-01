#pragma once

#include "RHI/UniformBuffer.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanUniformBuffer  (Phase 2 stub)
    // =============================================================================
    // No device buffer yet. Phase 3 backs it with a per-frame-in-flight VMA buffer (avoids
    // overwriting a UBO still read by an in-flight frame) bound via a descriptor set.
    class VulkanUniformBuffer final : public IUniformBuffer
    {
    public:
        VulkanUniformBuffer(Uint32 /*InSize*/, Uint32 /*InBinding*/) {}

        void SetData(const void* /*InData*/, Uint32 /*InSize*/, Uint32 /*InOffset*/ = 0) override {}
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
