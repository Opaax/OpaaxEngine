#pragma once

#include "RHI/Buffer.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanVertexBuffer / VulkanIndexBuffer  (Phase 2 stubs)
    // =============================================================================
    // No-op shells so Renderer2D::Init builds without a Vulkan device-memory path. Phase 3
    // backs these with VMA allocations + real upload; the interface stays identical.

    class VulkanVertexBuffer final : public IVertexBuffer
    {
    public:
        explicit VulkanVertexBuffer(Uint32 /*InSize*/) {}
        VulkanVertexBuffer(const float* /*InVertices*/, Uint32 /*InSize*/) {}

        void Bind()   const override {}
        void Unbind() const override {}

        void SetData(const void* /*InData*/, Uint32 /*InSize*/) override {}
        void SetLayout(const BufferLayout& InLayout)            override { m_Layout = InLayout; }

        const BufferLayout& GetLayout() const override { return m_Layout; }

    private:
        BufferLayout m_Layout;
    };

    class VulkanIndexBuffer final : public IIndexBuffer
    {
    public:
        VulkanIndexBuffer(const Uint32* /*InIndices*/, Uint32 InCount) : m_Count(InCount) {}

        void Bind()   const override {}
        void Unbind() const override {}

        Uint32 GetCount() const override { return m_Count; }

    private:
        Uint32 m_Count = 0;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
