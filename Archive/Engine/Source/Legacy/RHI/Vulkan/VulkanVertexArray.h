#pragma once

#include "RHI/Buffer.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanVertexArray  (Phase 2 stub)
    // =============================================================================
    // Owns the buffers handed to it (so they outlive Renderer2D's setup) but issues no GPU
    // work. Phase 3 records vertex/index binding into the command buffer instead.
    class VulkanVertexArray final : public IVertexArray
    {
    public:
        void Bind()   const override {}
        void Unbind() const override {}

        void AddVertexBuffer(UniquePtr<IVertexBuffer> InVBO) override { m_VertexBuffers.push_back(Move(InVBO)); }
        void SetIndexBuffer(UniquePtr<IIndexBuffer>   InIBO) override { m_IndexBuffer = Move(InIBO); }

        const TDynArray<UniquePtr<IVertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
        const IIndexBuffer*                        GetIndexBuffer()   const override { return m_IndexBuffer.get(); }

    private:
        TDynArray<UniquePtr<IVertexBuffer>> m_VertexBuffers;
        UniquePtr<IIndexBuffer>             m_IndexBuffer;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
